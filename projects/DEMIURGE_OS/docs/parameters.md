# DEMIURGE parameters — the pool (bathtub §3)

Status: DESIGN. Nothing here is deployed, wired, or run. Staged for review.

## 1. The contract

1. **Every parameter is a float in 0..1.** Rainbow convention. No exceptions on the pool wire. Native ranges (Hz, dB, ms, MIDI 0..127) are the *engine adapter's* job at the last inch, never the pool's.
2. **A parameter has a path**: `/<stage>/<group>/<name>`, lowercase, e.g. `/csound/gkMasterVol`, `/sc/pad/cutoff`, `/rnbo/0/cutoff`, `/global/bpm`. Hardware inputs are *sources*, not parameters: `/hw/<device>/<name>`, e.g. `/hw/teensy/tof1`.
3. **Engines never touch hardware.** An engine's only inputs are its parameters. Encoders, buttons, LiDAR, pedal, phone gyro, web faders are all sources that reach engines only through a mapping.
4. **A mapping is `source -> parameter`**, saved per patch (section 5). The web UI may also write a parameter directly (no source involved).
5. **Discrete events** (button press, "trigger", menu step) are still 0..1: a gate is 0/1, a trigger is a 0->1 edge. The existing router modes `toggle | gate | trigger | step` already express this; no second event type is added.

Not parameters (stay as they are): the clock (`0xF8`, ch16 CC118/119, `clock.md`), transport, patch loads, audio graph wiring. The pool carries *values*, not coordination.

## 2. What already exists (so this is an extension)

Read from the live repo, 2026-10-05:

**`demiurge-io`** (`src/demiurge-io/demiurge_io/`, unit `config/demiurge-io.service`, config `~/demiurge/sensors.json`). It already IS most of the pool's control plane:
- `sources/teensy.py`: reads the Teensy NEPTR2 100 Hz serial frame at full resolution; `sources/web.py`: UDP `<source_id> <raw>\n` on 8003 (phone gyro, browser faders, the web UI's sensor sim).
- `conditioning.py`: raw -> `norm` 0..1 (+ valid), calibration/deadzone/smoothing/dropout from `sources[]`. **0..1 is already the internal currency.**
- `router.py`: the mapping matrix. `maps[]` = source -> target with zone, invert, curve, out range, `mode` (continuous/toggle/gate/trigger/step), `combine` (replace/add/max), slew. Hot-reloaded from `sensors.json` by `config.ConfigWatcher` (mtime poll, bad JSON keeps last good).
- `sinks/csound.py`: UDP OSC to the engine on 7770: `/p <id:int> <val:float>` (absolute write into the generated `ExprSet` UDO, 35 targets from `handler/expr_targets.json`) and `/cc <ch> <cc> <val>`. Sink interface is one method, `send(target, value)`, explicitly designed so another sink "drops in without touching router.py or daemon.py's dispatch loop".
- `telemetry.py`: the **normalized input bus**: `/sensor <id> <norm>` (20 Hz decimated) and `/cc` to an explicit destination list (default 8002; the web UI meters and the video engine on 8004). Its header already states the contract this doc generalizes: consumers never re-scale or re-calibrate.
- `osc.py`: 20-line stdlib encoder (send only; **no OSC receive/parse yet**).
- Deliberately not duplicated today: encoders/buttons ride Teensy USB-MIDI straight into csound (`-Ma`); `--forward-control` is off so they don't register twice.

**`demiurge-monitor`** (`neptr/scripts/demiurge-monitor` + `.conf` + `.service`): a mido ALSA-seq tap that records the latest value of each CC and atomically publishes a one-line snapshot (`label:value ...`) to `/tmp/neptr_companion_telemetry` for the `demiurge` terminal companion. Config `monitor.conf` holds the CC->label table (`[map]`). It is read-only and independent of everything by design.

**Elsewhere**:
- `demiurge_web.py`: `GET/POST sensors config` (`read/write_sensors_config`, `validate_sensors_config`), `/api/neptr/sensors` (+SSE), `/api/neptr/expr-targets`, `/api/neptr/sm/param`, direct `/nv/param` to the video engine (8004), direct `/cc` to csound 7770. Several ad-hoc direct paths.
- Per-engine OSC conventions in `osc.md`: `DEMIURGE_OSC_BASE=9000`, stage N listens on BASE+N; RNBO runner on 1234.
- Patches: `~/demiurge/patches/<name>.conf`; `demiurge-patch <name>` copies it over `live.conf` (which carries a `# set: <name>` header) and restarts demiurge.
- Csound reads OSC on 7770 in `midi_handler.orc` (`/cc`, `/p`, `/st`, plus `/hg/*` for hyperGuitar on the same port).

Gaps against §3: no per-patch mappings (one global `sensors.json`); targets are csound-only (`sink: csound`, int ids); no registry of what parameters exist; encoders/buttons bypass the plane; MIDI is read twice (monitor tap and teensy source); the web UI has several private routes to engines; non-Csound languages expose nothing.

## 3. Design: grow `demiurge-io` into the pool. No new daemon.

The pool **is** `demiurge-io`, with the same unit name, user, CPU budget (`CPUAffinity=0-2`, `Nice=5`; the isolated audio core 3 is never touched), and the same hot-reload philosophy. What grows:

| Piece | Stays | Grows |
|---|---|---|
| `conditioning.py`, `router.py`, `model.py` | unchanged logic | none (target is just a dict) |
| `sources/` | `teensy`, `web` | + `serial_auto` (generic microcontroller), + `midi` (ALSA seq CC in; absorbs monitor's tap), + `osc_in` (UDP OSC ingress, see below) |
| `sinks/` | `csound` (`/p`, `/cc`) | + `osc` (generic stage sink: host/port/address), + `midi` (CC out to Midi Through) |
| `config.py` | `sensors.json` hot reload | + per-patch map file, + param registry loader |
| `telemetry.py` | `/sensor`, `/cc` fan-out | + `/param <path> <val>` (parameter changes, for UI echo) |
| `osc.py` | encoder | + a ~30-line decoder (the pool must now receive) |
| NEW `pool.py` | n/a | the in-process API: registry, publish, subscribe, map, save/load. The daemon's dispatch loop calls it. |

### 3.1 The pool bus
- **OSC ingress**: one UDP socket, `DEMIURGE_POOL_PORT` (proposed **9102**, outside the 9000-9099 stage range and clear of 7770/8000-8004/1234; unverified free). Message: `/p <path:string> <val:float>`. Anyone (web UI, phone, a script, an engine reporting its own value) can publish a parameter. The pool is the single writer toward engines.
- **MIDI side**: the existing `Midi Through` (14:0) stays the MIDI pool. The `midi` source subscribes to it (CC -> source `/hw/midi/<ch>/<cc>`, value/127); the `midi` sink writes CC there. Clock CCs (ch16 CC104-119) and ch1 CC29 (the DUODECIMUS pedal) are **excluded from sink output** by an allowlist; reserved CCs are never auto-assigned.
- **Egress to engines**: `/p <id> <val>` to Csound 7770 (as today); per-stage sockets at `BASE+N` for others (section 6). The pool resolves `path -> (sink, address)` through the registry.
- Pool-wide limits: per-parameter rate cap (default 100 Hz, coalesce to latest), loopback-only destinations by default, remote hosts need an explicit allowlist. Nothing is ever sent toward Machine Lab robots by default.

### 3.2 Serial auto-conversion
`sources/serial_auto.py` generalizes `teensy.py`. Order of preference for naming:
1. **Self-describing**: a device announces itself once, `@hello <device> <name>:<kind>:<lo>:<hi> ...`, then streams `<name> <raw>` lines. Pool derives `/hw/<device>/<name>`, calibration seed from `lo:hi`, and kind -> router mode (`knob`->continuous, `button`->gate, `enc`->step/delta).
2. **Known frames** (NEPTR2): existing `teensy.py` descriptors, now named `/hw/teensy/<id>`.
3. **Unknown**: `/hw/<device>/ch<N>` with the observed min/max auto-ranged; flagged `uncalibrated` in the UI until someone confirms.
Frames without USB-MIDI semantics are never converted to 7-bit CC internally; MIDI is an *output* format only.

### 3.3 `demiurge-monitor` consolidation
Two readers of one reality today. Plan, in order (each step reversible):
1. **A**: monitor unchanged; the pool ships the `midi` source.
2. **B**: pool writes the *same* one-line snapshot (`label:value`, same file, same atomic replace, cadence from `monitor.conf`) from its own state, with labels from `monitor.conf [map]` as aliases. Companion verified byte-compatible.
3. **C**: `demiurge-monitor.service` retired (file moved to `_attic/`, never deleted). `monitor.conf` stays as the alias table.
Until C, never run both with different labels for the same thing without noting it. Monitor's independence property (works with nothing else running) is lost; accepted because the pool's control-rate daemon is already `Restart=always` and holds no state outside JSON.

### 3.4 Encoders/buttons
Today these bypass the plane (USB-MIDI -> csound `-Ma`). §3 says engines never touch hardware. Final state: Teensy MIDI no longer connected to csound; pool `--forward-control` on; encoders/buttons are sources like any other, and menu navigation is a mapping to `/csound/menu/...` params. This is a **behavioural change of a live rig** and is last in the migration (section 8), done only by Emory at the bench; flipping it early double-triggers (see the comment in `demiurge-io.service`).

## 4. The parameter registry
Every engine contributes a manifest; the pool merges them. Directory: `~/demiurge/params/<stage>.json`.
```json
{ "stage": "sc-pad", "port": 9002, "version": 1,
  "params": [
    {"path": "/sc-pad/cutoff", "label": "Cutoff", "default": 0.5,
     "native": {"min": 40, "max": 12000, "curve": "exp", "unit": "Hz"},
     "address": "/cutoff"} ] }
```
- `address` is what the *engine* hears; `native` is only used if the engine wants real units (the adapter denormalizes). Engines that natively take 0..1 omit `native`.
- **Csound**: no new file authored by hand. The pool reads the already-generated `neptr/csound/handler/expr_targets.json` (`id`, `label`, `writes`) and synthesizes `/csound/<writes-slug>` -> `{sink: csound, kind: expr, id}`. One source of truth, no second list to drift.
- Registry change = file mtime, hot-reloaded like `sensors.json`. The web UI lists parameters from it (`GET /api/pool/params`), generalizing `/api/neptr/expr-targets`.

## 5. Per-patch mapping store
- Identity: the active patch name = the `# set: <name>` header in `~/demiurge/live.conf` (already written by the patch copy, and visible in the current live.conf). The pool watches `live.conf` mtime next to `sensors.json`. **No change to `demiurge-patch`** is needed to switch mappings; they follow the patch automatically.
- File: `~/demiurge/patches/<name>.map.json`, beside `<name>.conf`:
```json
{ "version": 1, "patch": "neptr_gorcore",
  "maps": [ { "id": "tof1-wah", "source": "/hw/teensy/tof1",
              "target": {"param": "/csound/gkWAH_freq"},
              "zone_lo": 0.0, "zone_hi": 1.0, "invert": false,
              "curve": "lin", "mode": "continuous", "combine": "replace" } ],
  "values": { "/csound/gkMasterVol": 0.8 } }
```
- `maps[]` is **the existing `sensors.json` map schema** unchanged, except `target` may be `{"param": path}` (new) or the old `{"sink","kind","id"}` (still valid, so nothing breaks). `values` (optional) are param baselines applied on patch load.
- **Split of responsibility**: `sensors.json` keeps *hardware truth* (`sources[]`: calibration/deadzone/smoothing: a property of the physical sensor, not the song) and, for backward compat, a global `maps[]` used when a patch has no `.map.json` (so today's behavior is the fallback, not a migration). Mappings move to the patch file.
- Merge order: global maps first, then patch maps override by `id` (patch wins). Atomic write (tmp + `os.replace`), same as monitor's publish. A bad file keeps the last good one (existing watcher behavior).
- Because the reload is mtime-based and in-process, switching patches does not need the pool restarted.

## 6. How each language exposes parameters
The pool owns the wire; the stage owns a small, native receiver. All stages publish a manifest (section 4) and listen on `DEMIURGE_OSC_BASE + stage index` (osc.md's convention, kept).

| Language | Receive (pool -> stage) | Report/publish (stage -> pool) | Notes |
|---|---|---|---|
| Csound | existing `OSClisten giCtlOSC "/p" "if"` -> `ExprSet` on 7770 | `OSCsend` `/p <path> <val>` to 9102 | already done; manifest generated from `expr_targets.json` |
| SuperCollider | `OSCdef(\p, {...}, '/cutoff')` on BASE+N | `NetAddr("127.0.0.1",9102).sendMsg("/p", path, v)` | |
| Pd | `[netreceive -u BASE+N]` -> `[oscparse]` -> `[route /cutoff]` | `[oscformat]`/`[netsend -u]` | |
| ChucK | `OscIn` + `OscMsg` | `OscOut` | |
| Faust | built-in OSC (`faust2jackconsole -osc`) exposes `/<name>/<param>`; pool `osc` sink maps to those addresses | n/a | native ranges: adapter denormalizes. Unverified on this rig. |
| RNBO | runner OSCQuery on 1234, `/rnbo/inst/0/params/<name>` (0..1 via the `normalized` form, to confirm) | OSCQuery subscribe | manifest can be generated from the runner's OSCQuery tree. Unverified. |
| Strudel / C++ | node `osc`/liblo receiver on BASE+N | same | |

Rule: a stage never reads a hardware device and never invents a second control path. If it has a knob, it is a manifest entry.

## 7. Web UI
Today's UI already has the pieces (`sensors` config read/write + SSE meters, expr-target list). Consolidation, not addition:
- **List**: `GET /api/pool/params` (registry) and `GET /api/pool/sources` (calibrated sources + live norm, i.e. today's `/api/neptr/sensors` + SSE).
- **Drive**: `POST /api/pool/set {path, value}` -> UDP `/p` to 9102. Replaces the private routes (`/nv/param`, direct `/cc`, `sm/param`) one at a time; each is removed only when its client is migrated (one mechanism per job; no period where both are advertised as current).
- **Map**: `GET/POST /api/pool/map` read/write `<patch>.map.json` through the existing `validate_*` + atomic-write pattern; hot reload applies it live.
- **Learn** ("hold this button + do this -> controls this"):
  1. UI selects (focuses) a parameter, or the player picks it with a hardware **hold button** (a source mapped as `gate` to `/global/learn_hold`).
  2. While `learn_hold` is high (or UI "Learn" armed for N seconds), the pool watches the telemetry-normalized sources and picks the one with the largest recent excursion above a threshold (default 0.15 of range, ignoring the hold button itself and uncalibrated sources).
  3. It writes a `maps[]` entry (`zone 0..1`, `lin`, `continuous`, or `gate` for 2-state sources) to the patch map file; the hot reload makes it live; UI shows it and the user can refine zone/curve/invert (existing fields).
  4. Release/timeout ends learn. Escape cancels without writing.
  Learn state lives in the pool process only; it holds nothing durable except the map file it writes.

## 8. osc.md reconciliation
`demiurge/docs/osc.md` (not edited by this work) says DEMIURGE "runs **no OSC router daemon** by design" and lists "No central OSC router/dispatcher" under "deliberately does not do". **The plan supersedes this**, with the following precise reconciliation, to be applied when osc.md is updated (by Emory/the owner, not here):

- osc.md's *reasons*: (a) OSC wants addressing, not broadcast; (b) avoid another daemon on a lean Pi; (c) "a problem we don't have". The pool respects (a): it is an **addressed router**, not a broadcast pool. It sends each parameter to exactly one stage socket at `BASE+N`; the point-to-point model and port range are unchanged. (b): it is not a new daemon; it is `demiurge-io`, already running, control-rate, off the audio core. (c): the problem now exists: per-patch mappable hardware, and every language exposed uniformly.
- Rewrite targets in osc.md: the intro bullet "no OSC router daemon", the last section "What DEMIURGE deliberately does not do", and the clause "no pool required" in the RNBO section (RNBO then becomes one more registry entry; direct `live.conf`-driven RNBO values remain valid until migrated).
- Unchanged: MIDI is the shared coordination bus (clock); OSC is point-to-point per stage; the 9000-9099 range; the clock-republish-bridge note.
- New rows to add: pool ingress port 9102, the `/p <path> <val>` verb, the manifest directory.

## 9. Migration (each step independently shippable and reversible)
1. Registry loader + manifests (read-only, nothing sent). 2. `osc.py` decoder + OSC ingress + `osc` sink, loopback only. 3. `{"param"}` targets + per-patch map file + live.conf watcher (fallback to current `sensors.json` behavior). 4. Web `/api/pool/*`; migrate private routes one by one. 5. Learn. 6. `midi` source/sink; monitor steps A-C. 7. `serial_auto`. 8. Per-language manifests (SC, Pd, ChucK, Faust, RNBO, Strudel). 9. Encoders/buttons through the pool (rig-affecting; bench, Emory present). Every step that restarts `demiurge-io` is cheap; none requires touching `demiurge.service`/csound, but wiring the Csound manifest or step 9 touches the audio side and needs direct permission each time.

## 10. Assumptions and unknowns
- Port 9102 free; `# set:` header is present in every patch conf (true for current live.conf; confirm for `patches/*.conf`).
- `~/demiurge/patches/` exists on the rig (from `demiurge-patch`; not checked on the Pi's own disk, only the repo).
- Faust `-osc` addressing and RNBO `normalized` endpoint semantics: from memory, not verified here.
- The `@hello` serial handshake needs firmware support; only the Teensy NEPTR2 frame is known to exist.
- `demiurge-io.service` hardcodes `User=gorpi7` and `/home/gorpi7/...`, but the rig's current user is `gorpi7v2` (the 2026-10-02 rebuild incident); the pool unit must use `%h`/`$HOME` the same way.
- No timing/CPU numbers measured; nothing was run (live engine).

## 11. Hard/soft mapping + two-way
Status: implemented in `demiurge-io` (`pool.py`, `sources/osc_in.py`, `daemon.py`; 4 new tests in `tests/test_pool.py`) and in `resources/` (per-language hooks, `testpool.py`). Not deployed.

### 11.0 Hook-engine params: `"proto": "p"`
A manifest param with `"proto": "p"` (and no `address`) belongs to an engine using the `resources/params/<lang>/` hooks. The pool's `osc` sink sends it `/p <path:s> <val:f>` to the manifest `port` with the raw 0..1 value (never denormalized; `native` is display metadata only). Params with `address` keep the old mode: `<address> <native float>` (Faust's own OSC, etc.). A stage name may be `<anything>-<lang>` (e.g. `nld-csound`); the launcher matches it to the lang's program when the lang is unique, and builds Faust with `-osc` only when such a manifest exists.

### 11.1 Hard vs soft
Each manifest param gains an optional `"map": "hard" | "soft"` (default `"soft"`).
- **SOFT**: remappable live (web UI, learn, hand-edited patch). Its mappings live in the patch's `<name>.map.json` (§5), exactly as before.
- **HARD**: the mapping is defined by the *instrument/system*, not the player. The manifest carries a top-level `hard_maps` array using **the existing map schema** unchanged (§5; `target` is `{"param": path}`):
```json
{ "stage": "sc-pad", "port": 9002, "version": 1,
  "params": [ {"path": "/sc-pad/cutoff", "address": "/cutoff", "map": "hard"} ],
  "hard_maps": [ {"id": "pad-cutoff-tof1", "source": "/hw/teensy/tof1",
                  "target": {"param": "/sc-pad/cutoff"}, "mode": "continuous"} ] }
```
The web UI shows a hard param and its mapping but cannot change them. Enforcement is in the pool, not the UI:
1. `ParamRegistry.enforce_hard(maps)` runs on every map reload (sensors.json + patch maps + registry change). Any global/patch map whose `target.param` is hard is **dropped and logged** (`[demiurge-io] map 'x' rejected: /p/a is hard-mapped`); a user map reusing a hard map's `id` is dropped too. The manifests' `hard_maps` are then appended, so they always win.
2. Direct writes (`/p` from the UI sliders, scripts) to a hard param are still allowed: hard fixes the *mapping*, not the value. (If a stage wants a read-only param, it simply isn't a param; use `/pout` reporting, §11.2.)
3. `Pool.can_remap(path)` is the guard `POST /api/pool/map` must call before writing a map for that param (reject with 403 and the same log line). `ParamRegistry.is_hard(path)` / `hard_maps()` feed the UI's lock icon.
Legacy `{sink,kind,id}` targets are never judged hard.

### 11.2 Two-way (engine -> pool -> UI)
- Pool -> engine: unchanged, `/p <path:s> <val:f>` to the manifest `port`.
- **Engine -> pool: `/pout <path:s> <val:f>`** to `DEMIURGE_POOL_HOST:DEMIURGE_POOL_PORT` (default `127.0.0.1:9102`, the same ingress socket as `/p`). `/pout` is "my value is now this": the pool clamps it to 0..1, stores it, and **echoes it to subscribers (the telemetry `/param`, i.e. the UI) but never to a sink**, so an engine report can never loop back into the engine. A report for an unregistered path is dropped. (A plain `/p` from an engine would be re-sent to the engine; that is why a distinct verb exists.) Implementation: `OscInSource` hands engine-origin items to the daemon queue flagged `engine=True` -> `Pool.publish_out`.
- Engine listens on its manifest `port`; env **`DEMIURGE_PARAM_PORT`** overrides it for that engine. Pool host/port for `/pout`: env `DEMIURGE_POOL_HOST` / `DEMIURGE_POOL_PORT` (default 127.0.0.1 / 9102). The pool's own ingress port is the same `DEMIURGE_POOL_PORT` (§3.1).
- Reporting etiquette: send `/pout` on change only (the hooks do), never in a loop that re-reports a value just received (the pool does not echo it back, but it would still cost a packet per tick).

### 11.3 Env contract for engines (set by Demiurge's launcher)
`DEMIURGE_PARAM_PORT`, `DEMIURGE_POOL_HOST`, `DEMIURGE_POOL_PORT`, `DEMIURGE_STAGE`, `DEMIURGE_RESOURCES` (= the `resources/` directory; per-language hooks at `$DEMIURGE_RESOURCES/params/<lang>/`). Csound has no `getenv`, so the launcher passes the first three as `--omacro:NAME=value`; Pd gets `pd -send "demiurge-param-port N; demiurge-pool-port M"`. Full table and hooks: `resources/README.md`. Self-tests and a stock-Pi checker: `resources/params/*/selftest.sh`, `testing/stock-check/run.sh`.

### 11.4 §6 table corrections found while building the hooks
- The §6 "Report/publish" column should read `/pout`, not `/p`, for every language (see 11.2).
- Csound: the *new-stage* hook is `resources/params/csound/demiurge_params.udo` (string paths on `DEMIURGE_PARAM_PORT`); the NEPTR rig's int-id `/p` on 7770 is untouched.
- Pd: `[oscparse]` strips the leading `/` of the OSC *address* (`/p` -> `p`) but not of string arguments.
- Faust: verified on this rig's Faust 2.79 that `[osc:/addr 0 1]` aliases accept 0..1 and map **linearly** onto the slider; exponential ranges must use the default address plus manifest `native {curve:"exp"}` so the pool's osc sink denormalizes (`resources/params/faust/faust_manifest.py` generates both forms).

## 12. NEPTR on the pool + the USS slot (implemented 2026-10-07, not deployed)
Runbook: `docs/NEPTR_POOL_BENCH.md`. Tests: `src/demiurge-io/tests/test_neptr.py`.
- **Port**: the pool ingress is **UDP 9102**. 9100 is demiurge-clock's `/transport/tap` input (bound live); the pool and the clock cannot share it. §3.1/§6/§11 above say 9102 now.
- **Sources** (`sources/teensy.py`, `daemon.source_aliases`): every Teensy sensor is also `/hw/teensy/<id>` (maps may use the bare id or the pool name); body buttons `b1..b9` and joystick click `jb` are new edge-only sources `/hw/teensy/<id>` (0/1, a button held at connect is blocked until released). Encoders and encoder buttons are never sources: they keep their fixed menu roles on MIDI.
- **Pedal**: `/hw/midi/1/29` = the DUODECIMUS treadle. `demiurge-io --midi --midi-neptr` applies midi_handler.orc's two rewrites at the source (body ch1 CC29 -> parked CC30; ch7 CC14/15 -> ch1 CC29) and publishes only that. Registered pool parameter `/csound/neptr/expr` = `{"sink":"csound","kind":"cc","ch":1,"cc":29,"norm":true}` (the sink scales 0..1 to the engine's 0..127 `/cc`, OSC-origin so exempt from the engine's demotion). `--omacro:PEDAL_VIA_POOL=1` switches the engine's own funnel off: one mechanism.
- **Arcade** (`arcade.py`): `/global/tap` -> `TransportSink` -> OSC `/transport/tap ,if (1, age_ms)` to the clock (age = serial_bridge.py's formula). Maps may carry `"arcade": "tap"|"trig"`; only the live role's maps run. A USS manifest's b1 map is `"arcade":"trig"`; a long hold (2 s default, `arcade_hold_s` per map, `--arcade-hold`) flips the role while a USS is active.
- **Default patch map**: `neptr/pool/neptr_default.map.json` (pedal + arcade tap), installed as `patches/<patch>.map.json` for every NEPTR patch. Sensors stay unmapped, exactly as today.
- **USS slot**: `demiurge-uss` edits a marked second `chain =` block of live.conf; the launcher's reconcile keeps NEPTR's stage. `demiurge-run-chuck/-csound` run `uss-*` stages on cores 0-2 (csound stays on the isolated core 3, FIFO 74; guest Csound FIFO 40 with its own JACK client name, which the launcher resolves by id).
