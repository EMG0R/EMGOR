# Deploying the parameter-pool extension of demiurge-io

STAGED, NOT DEPLOYED, NOT RUN against the live engine. This tree is the existing
`src/demiurge-io/` plus the pool (design: `docs/parameters.md`). It is the same
daemon, same unit name, same `python3 -m demiurge_io`. Deploy and restart are
Emory's call at the bench; nothing here does either.

Restarting `demiurge-io` does NOT touch `demiurge.service`, csound or neptr-ui.
Sensors are additive: encoders/buttons stay on the Teensy's USB-MIDI, so if the
daemon is down for a few seconds the instrument plays as before (the expression
pedal's sensor lane and any sensor maps pause).

## 0. Before you start
- Tests (offline, no live sockets): `cd <this dir> && python3 -W ignore -m unittest discover -s tests -p test_pool.py` -> 48 OK.
- Check 9102 is free: `ss -ulpn | grep 9102` (must print nothing).
- Back up what you will replace (rollback depends on it):
  ```
  sudo cp -a /opt/demiurge/src/demiurge-io /opt/demiurge/src/demiurge-io.bak-$(date +%F)
  sudo cp -a /etc/systemd/system/demiurge-io.service /etc/systemd/system/demiurge-io.service.bak-$(date +%F)
  ```

## 1. Deploy (files only, still no restart)
```
sudo rsync -a --exclude __pycache__ ./demiurge_io/ /opt/demiurge/src/demiurge-io/demiurge_io/
sudo rsync -a ./tests/ /opt/demiurge/src/demiurge-io/tests/
# unit: render the user (the live unit already runs as the rig user)
sed "s/@RIG_USER@/$(id -un)/" demiurge-io.service | sudo tee /etc/systemd/system/demiurge-io.service >/dev/null
sudo systemctl daemon-reload        # reads the unit; does not restart anything
```
Do not install `demiurge-io.service` without the `sed`: the raw file has
`User=@RIG_USER@` and will fail to start (by design, loudly).

Smoke test BEFORE restarting, using only non-live ports and no hardware:
```
cd /opt/demiurge/src/demiurge-io
python3 -m demiurge_io --list-params          # prints the merged registry, opens nothing
python3 -W ignore -m unittest discover -s tests -p test_pool.py
```

## 2. Restart (bench, Emory's call)
```
sudo systemctl restart demiurge-io
journalctl -u demiurge-io -n 30 --no-pager
```
Expect: `registry: N parameters`, `patch '<name>': 0 patch maps`, `running (live); sources=['teensy','web','osc_in']`.
Then: `echo` a test write only if you want to prove ingress. It is a real write to a
live engine parameter, so pick a harmless one:
`python3 - <<'P'` ... send `/p /csound/<param> 0.5` to 127.0.0.1:9102 with `demiurge_io.osc.encode_osc`.

## 3. What changes behaviourally
With the unit as staged, almost nothing, on purpose:
- NEW: UDP listener on 127.0.0.1:9102 (`/p <path:string> <val:float>`). Writes a registered parameter through the existing sinks. If 9102 is taken the osc_in thread logs and dies; the rest of the daemon runs.
- NEW: reads `~/demiurge/params/*.json` (stage manifests) and `handler/expr_targets.json` (found at `~/NEPTR_phase4/csound/handler/` then `~/NEPTR/...`, like the web UI) into a registry. Read-only.
- NEW: follows the `# set: <name>` header in `~/demiurge/live.conf` and loads `~/demiurge/patches/<name>.map.json` if it exists, layered over `sensors.json` maps (patch wins by `id`). **No map file = identical behaviour to today.** `~/demiurge/patches/` need not exist.
- Map `target` may now be `{"param": "/csound/<writes-name>"}` as well as the old `{"sink","kind","id"}` (still valid, unchanged path to csound 7770).
- NEW: a source id starting `/hw/` with no `sensors.json` entry is now conditioned (identity 0..1 calibration) and can be mapped. Any other unknown id behaves exactly as before (telemetry only).
- Per-patch baseline `values` in a map file are written once when that patch becomes active (and at daemon start).
- Fixed: unit no longer hardcodes `gorpi7`/`/home/gorpi7`; `--config` dropped (default is `$HOME/demiurge/sensors.json`).

Opt-in extras, none enabled by the staged unit (add to `ExecStart`, restart):
- `--midi` : ALSA-seq CC source `/hw/midi/<ch>/<cc>` (clock CCs skipped). Adds one ALSA subscriber on the monitor's preferred port.
- `--midi --midi-snapshot [FILE]` : also writes demiurge-monitor's exact one-line file (default `/tmp/neptr_companion_telemetry`, labels from `~/demiurge/monitor.conf`). **Stop `demiurge-monitor.service` first (or give a different FILE): two writers on one file.** Compare outputs, then retire the monitor (move to `_attic/`).
- `--telemetry-params` : also emits `/param <path> <val>` on the telemetry bus (8002/8004 consumers have never seen this verb; verify the web UI and video engine ignore it before enabling).
- `--serial-auto /dev/serial/by-id/...` : SCAFFOLD for a future `@hello` microcontroller. Explicit port only. Never point it at the NEPTR body, DUODECIMUS or Bela.
- `--osc-allow-host HOST` : allow the generic osc sink to send off-box. Default is loopback-only. Never list Machine Lab hosts.
- The osc/midi SINKS are always constructed but send only if a map targets a registered osc/midi parameter (none exist until a stage writes a manifest). The midi sink never emits ch16 CC104-119 (clock) or ch1 CC29 (DUODECIMUS pedal).

Not done (deliberately, later steps in parameters.md §9): encoders/buttons through the pool (`--forward-control` stays off), `/api/pool/*` web routes, Learn, per-language stage manifests, renaming Teensy sensors to `/hw/teensy/<id>` (would break existing `sensors.json` maps).

## 4. Bench verification (nothing below has been run live)
1. `journalctl -u demiurge-io` clean after restart; `ss -ulpn | grep 9102` shows python.
2. Existing sensor maps still move their csound targets exactly as before (tof/accel/FSR/pedal). This is the regression that matters.
3. Encoders/buttons still single-step (no double triggers; `forward_control` unchanged).
4. Create `~/demiurge/patches/<active>.map.json` with one `{"param":"/csound/..."}` map; confirm it takes effect within ~0.1 s with no restart, and that switching `demiurge-patch` swaps mappings (watch for the `patch '<name>'` log line).
5. `/p` write to 9102 for a `/csound/...` path moves the parameter.
6. Idle CPU of the python process unchanged (the loop adds only stats and a flush per tick).

## 5. Rollback
Fastest, no file changes: remove `--osc-in` from `ExecStart` (and any opt-ins), `daemon-reload`, `restart`. That disables every new listener; the patch-map/registry code stays inert without map files.

Full rollback to the previous build:
```
sudo systemctl stop demiurge-io
sudo rm -rf /opt/demiurge/src/demiurge-io && sudo mv /opt/demiurge/src/demiurge-io.bak-<date> /opt/demiurge/src/demiurge-io
sudo mv /etc/systemd/system/demiurge-io.service.bak-<date> /etc/systemd/system/demiurge-io.service
sudo systemctl daemon-reload && sudo systemctl start demiurge-io
```
(Stopping/starting `demiurge-io` only; never `demiurge.service`.) Any `~/demiurge/params/`, `patches/*.map.json` files are inert to the old build and can stay.

## NEPTR integration addendum (2026-10-07)
For the NEPTR controls and the USS slot, do NOT follow this file's steps by hand: use `setup-script/deploy-neptr-pool.sh` and
`docs/NEPTR_POOL_BENCH.md` (backup-first, dry-run default, one-command rollback). Differences from the text above: the pool port is
9102 (9100 is demiurge-clock's), the unit starts with `--osc-in --midi --midi-port Through --midi-neptr` and has no
`ConditionPathExists=/dev/ttyACM0`, and the 55 -> 77 tests include `tests/test_neptr.py` (offline, ephemeral ports).
