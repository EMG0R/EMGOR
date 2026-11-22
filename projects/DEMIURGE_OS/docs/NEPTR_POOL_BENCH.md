# NEPTR on the parameter pool + the USS slot: bench runbook

For Emory (or main with Emory at the rig). Nothing here has run on the live rig. Everything below was built and
tested OFFLINE (`src/demiurge-io/tests/test_neptr.py`, 22 tests; cargo; `uss/check.sh`; stock-check).
Design: `docs/parameters.md`, `docs/neptr-param-migration.md`. Deploy: `setup-script/deploy-neptr-pool.sh`.
Undo: `setup-script/rollback-neptr-pool.sh`.

## What changes, in four lines
1. **Pedal** (DUODECIMUS treadle, wire ch7 CC14/15) and the **arcade tap tempo** now go through the pool. Same writes, proven equal
   by test (`/cc 1 29 <v>` for every value; tap age maths identical to `serial_bridge.py`).
2. Joystick x/y/click, LiDAR 1/2, FSR 1/2, accel, mics, buttons b2-b9 become pool sources (`/hw/teensy/<id>`) and stay UNMAPPED in
   NEPTR (exactly as today, where nothing in Csound reads them). They are what a USS instrument's hard maps use.
3. **Encoders and encoder buttons are untouched**: still direct USB-MIDI into csound with their fixed menu roles.
4. **USS slot**: `demiurge-uss <name>` runs one USS instrument (ChucK or Csound) as its own launcher stage next to NEPTR's csound;
   swapping or removing it never restarts csound. While one is active the arcade is its trigger; **hold the arcade 2 s to get tap
   tempo back** (hold again to return to the trigger).

## Things you must know first
- **Pool port is 9102, not 9100.** `demiurge-clock` already owns 9100 (its `/transport/tap` input; verified bound on twink). The staged
  design had the pool on 9100, which would have lost the bind. All defaults moved to 9102 (daemon, web, launcher, every language hook).
- **The live launcher predates the pool launch contract** (`strings demiurge-launcher | grep DEMIURGE_PARAM_PORT` is empty), so a USS
  stage cannot get its hook/port until the new launcher is installed. That install restarts `demiurge.service` = **csound restarts
  once** (phase `engine`). After that, no instrument change ever restarts csound.
- `demiurge-io` is currently **inactive** on twink (its unit has `ConditionPathExists=/dev/ttyACM0`, and the body is not attached),
  and `neptr-bridge` is not installed: today the arcade tap is not running on this unit at all. The new unit has no such condition.
- Tap tempo and every `/hw/teensy/*` source need the **NEPTR body (Teensy) attached**; the pedal and USS slot do not.
- Not mid-performance. Phases `io` and `engine` are control/audio restarts.

## 0. Before you start (2 min)
```
cd ~/_______DEMIURGE            # the repo checkout holding this change (git pull first; do not deploy a dirty tree)
bash setup-script/deploy-neptr-pool.sh plan          # read-only report: services, ports, serial, launcher, patch
ls ~/demiurge/backups                                 # your known-good tree backup exists (BENCH_DEPLOY.md step 0)
```
Expect: `UDP 9100: demiurge-clock`, `UDP 9102: free`, `launcher: OLD`.
Build the new launcher OFF-PEAK now (CPU-heavy; this is the only slow step; confined to cores 0-2, nice 19):
```
cd src/demiurge-launcher-rs && PATH=$HOME/.cargo/bin:$PATH taskset -c 0-2 nice -n 19 ionice -c3 cargo build --release
```

## 1. Stage the files (no restart, safe any time)
```
bash setup-script/deploy-neptr-pool.sh stage                 # DRY RUN: read what it will do
APPLY=yes bash setup-script/deploy-neptr-pool.sh stage       # do it (backs everything up first)
```
Installs: demiurge-io (pool + arcade + pedal source), `resources/` hooks and `neptr_default.map.json`, the new unit (daemon-reload
only), USS-aware `demiurge-run-chuck`/`-csound` (only act on stages named `uss-*`; NEPTR's own respawns are unchanged),
`demiurge-uss`, the instruments in `~/demiurge/uss/`, a copy of the default map as `~/demiurge/patches/<patch>.map.json` for every
patch that chains `neptrPhase4.csd`, and the 6-line `PEDAL_VIA_POOL` guard in `~/NEPTR_phase4/csound/midi_handler.orc` (a no-op
unless the macro is set; it refuses to touch the file if it is not byte-identical to the version it was written for).
Check: `demiurge-uss status` (slot empty, launcher contract: NO), `ls ~/demiurge/patches/*.map.json`.

## 2. Control plane on: restart demiurge-io only  (CONTROL restart, csound untouched)
```
APPLY=yes CONFIRM_LIVE_RESTART=yes bash setup-script/deploy-neptr-pool.sh io
```
It prints the journal. Expect `registry: N parameters`, `patch 'neptr_gorcore': 2 patch maps`, `midi source reading: Midi Through...`,
`running (live); sources=['teensy','web','osc_in','midi']`, and `OK: pool ingress bound on 9102`.

Listen / check (the engine funnel is still on, so the pedal arrives twice with the same value: that is harmless and is the "A/B" state):
- **Encoders**: turn each one a few clicks: one click = one step (NOT two). If any doubles, stop and roll back.
- **Pedal**: move the treadle; the focused effect's expression parameter follows as before, soft takeover behaves as before.
- **Tap tempo** (body attached): tap the arcade button 4x at a steady tempo. `demiurge-clock` follows it; NEPTR's BPM readout changes.
  The 5th press after a pause is a new window. A single press does nothing.
- **Probes** (all harmless):
  ```
  python3 -m demiurge_io --list-params | grep -E 'neptr/expr|global/tap'      # both registered
  aseqdump -p 14:0 | grep -E 'controller +29'                                   # (optional) the pedal on the wire
  ```
  Hold the arcade 3 s with no USS active: still just a long tap (no mode change in the journal).
- Journal must stay quiet: `journalctl -u demiurge-io -f` (no tracebacks, no `source ... died`).

## 3. Engine funnel off + new launcher  (**AUDIO restart: csound restarts, a few seconds**)
```
APPLY=yes CONFIRM_LIVE_RESTART=yes bash setup-script/deploy-neptr-pool.sh engine
```
It refuses unless demiurge-io is active (the pedal would go dead), the launcher build exists and has the contract, and the guard is in
the `.orc`. It installs the launcher, adds `--omacro:PEDAL_VIA_POOL=1` to the `csound_extra` line (the rest of live.conf untouched),
restarts `demiurge.service`, waits for csound and prints its links.
Listen / check:
- Sound returns; preset bank/state as before (`PRESET_BANK=2` still in `csound_extra`).
- **Pedal still works, now through the pool only**: `ps -o args= -C csound | grep -c PEDAL_VIA_POOL` prints 1, and moving the treadle
  still moves the focused effect. (If the pedal is dead: `systemctl status demiurge-io`, then roll back `engine`.)
- Encoders single-step, touch UI EXPR strip still works (it writes `/cc 1 29` directly, same as before).
- `pw-link -l | grep -A1 csound6:output` shows csound6 -> the card, as before.

## 4. USS slot: play an instrument next to NEPTR
```
pgrep -x csound                         # note the PID
demiurge-uss status                     # launcher contract: yes
demiurge-uss supersaw                   # ChucK instrument into the slot (or: lockin, nonlinear_daylight)
```
About 2 s later the stage is up (`tail ~/.demiurge/logs/uss-supersaw.log`, `journalctl -u demiurge -n 20` shows `reconcile:` keeping
`neptrPhase4`). **`pgrep -x csound` must print the SAME PID.** It plays by itself (generative). Now play it:

| control | does |
|---|---|
| joystick X / Y | pitch of the next note / tone |
| LiDAR 1 / 2 | space / length (holds the last value on dropout) |
| FSR 1 | density (rest = original density, press = busier) |
| DUODECIMUS treadle | intensity (heel = quiet) AND still NEPTR's focused effect |
| arcade | trig (note now). **Hold 2 s = tap tempo again**, hold 2 s again = trig. (lockin: hold 8 s; its own 5 s hold toggles its sequencer) |
| encoders | unchanged NEPTR menu roles (never mapped) |

Listen for / measure (this is the "csound stays on top" proof):
```
pw-top                                                    # ERR column for csound6 must stay 0 and not climb when the stage starts
ps -eLo pid,cls,rtprio,psr,comm | grep -E 'csound|ChucK|chuck'    # csound: FF 74 on core 3; ChucK/guest threads on cores 0-2, never 3
journalctl -u demiurge -f | grep -iE 'xrun|reconcile'
```
- NEPTR's sound must not change when the instrument starts or stops (no click, no level jump, no added latency). The two sum at the output.
- Switch: `demiurge-uss lockin` (supersaw stops, lockin starts; csound PID unchanged). Empty it: `demiurge-uss off`.
- **Abort rule**: any xrun on csound6 or any audible glitch in NEPTR: `demiurge-uss off` at once (stage stops in about a second; csound is
  never touched). Report which instrument and what you were doing (the Csound instrument is the heaviest).
- A patch switch (`demiurge-patch`) restarts the engine and drops the slot (live.conf is replaced); run `demiurge-uss <name>` again.

## 5. Roll back (ONE command)
```
APPLY=yes CONFIRM_LIVE_RESTART=yes bash setup-script/rollback-neptr-pool.sh      # undo everything, newest first
bash setup-script/rollback-neptr-pool.sh                                         # (no env) dry run: prints the plan
APPLY=yes CONFIRM_LIVE_RESTART=yes bash setup-script/rollback-neptr-pool.sh engine   # just undo phase 3 (csound restarts)
APPLY=yes CONFIRM_LIVE_RESTART=yes bash setup-script/rollback-neptr-pool.sh io       # just demiurge-io
APPLY=yes bash setup-script/rollback-neptr-pool.sh stage                             # just the files
```
It reads the backup `~/demiurge/backups/neptr-pool/LATEST/MANIFEST`: puts the old launcher, demiurge-io, unit, wrappers, resources,
`midi_handler.orc` back; removes only what the deploy created; empties the USS slot; removes `--omacro:PEDAL_VIA_POOL=1` from live.conf
without overwriting your other edits; restarts demiurge.service only if phase 3 was applied. After rollback the engine funnel is live
again and the pedal/encoders behave exactly as before the migration. Verified in a sandbox: rollback leaves the tree byte-identical.
Fastest manual "stop the experiment" with nothing else changed: `demiurge-uss off`.

## Risks for the live rig (read before phase 3)
- **Phase 3 restarts csound** (new launcher binary, `csound_extra` change). The new launcher is built from today's repo HEAD and carries
  every launcher change since the Oct 3 binary was built (clockrole, HDMI fallback...), not only the USS bits. 64 launcher tests pass; it
  has not run on the rig.
- **PipeWire graph coupling**: the USS stage is a node in the same graph. A heavy instrument that misses its deadline can delay the
  graph cycle and xrun NEPTR's output even though it runs on other cores. Mitigation is the abort rule; watch `pw-top` the first time per
  instrument. `nonlinear_daylight` (Csound, 40-voice FM) is the heaviest. Not measured live.
- **A second Csound** under the launcher is the least-proven path (the guest gets its own JACK client name, FIFO 40, cores 0-2; the launcher
  resolves it by id). Start with a ChucK instrument; do the Csound one last.
- ChucK USS stages inherit the default hardware-input link (the interface's capture feeds the stage's input). The instruments do not read
  `adc`, so it is silent plumbing; `input = off` in live.conf would remove it for every stage (not recommended: NEPTR needs its input).
- The pedal drives both NEPTR's focused effect and the USS `intensity` while an instrument is active. Intended; remap in the patch map if not.
- One stray tap: holding the arcade 2 s in TAP mode (to get back to trig) registers one tap at the press; a single tap never changes tempo.
- The lockin arcade hold is 8 s (not 2 s) so its own 5 s sequencer gesture still works.
- Port 9102 is new; if anything else on the rig uses it, `ss -ulnp | grep 9102` before phase 2 (the `plan` report checks).

## Not done / next
- Encoders through the pool (parameters.md step 9) is deliberately not part of this: they keep their fixed menu roles.
- Mapping the unused body sensors to NEPTR effects is a patch-map edit (learn/web UI), not a migration.
- The hyperGuitar Bela link and the web `/api/pool/*` routes are separate.
