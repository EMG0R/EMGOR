# DEMIURGE NAM Integration — Design

## Summary

NAM (Neural Amp Modeler) runs as a first-class DEMIURGE language. Drop a `.nam`
profile into `live.conf`'s `chain =` exactly like any other stage:

```ini
chain =
  in1 ->
  ~/profiles/jcm800_crunch.nam
  ~/fx/reverb.csd
```

The launcher sees `.nam` → dispatches `demiurge-run-nam` → which calls
`demiurge-run-csound` with a fixed CSD template (`nam_stage.csd`). CSound loads
`csound-nam.so` at startup (auto-discovered from `/usr/local/lib/csound/plugins64/`),
which provides the `NAMProcess` opcode. Audio I/O, JACK wiring, RT scheduling,
and MIDI are handled identically to any other CSound stage — no new infrastructure.

---

## Architecture

```
live.conf (.nam entry)
    ↓
demiurge-launcher  [lang = "nam"]
    ↓
demiurge-run-nam
    ├── resolves profile dir + starting index
    ├── exports NAM_PROFILE_DIR, NAM_PROFILE_IDX
    └── exec demiurge-run-csound nam_stage.csd
                ↓
            CSound (JACK client: csound6)
                ├── auto-loads csound-nam.so
                └── NAMProcess opcode
                        ↓
                    NeuralAmpModelerCore
                    (NamLoader: background thread, hot-swap)
```

---

## Components

### `src/wrappers/demiurge-run-nam`
Thin bash wrapper. Resolves the `.nam` argument to a (directory, starting-index)
pair, exports env vars, and execs `demiurge-run-csound`. All JACK/ALSA/RT logic
lives in `demiurge-run-csound` — this wrapper adds nothing extra.

### `nam/templates/nam_stage.csd`
The CSound orchestra. Reads `NAM_PROFILE_DIR` and `NAM_PROFILE_IDX` from the
environment, opens an OSC socket on `DEMIURGE_OSC_BASE`, and runs `NAMProcess`
in a single always-on instrument (`i1 0 86400`). Profile switching and bypass
are live-controllable via OSC without stopping the process.

### `neptr/csound/nam/src/csound_nam_opcode.cpp` + `nam_loader.*` (moved 2026-10-03; `nam/src` retired to `nam/_attic_src_single_instance/`)
The `NAMProcess` and `NAMCount` CSound opcodes. `NamLoader` runs a background
thread that watches for index changes, calls `nam::get_dsp()` off the audio
thread, and hot-swaps the `shared_ptr<nam::DSP>` under a mutex. The audio thread
only acquires the mutex to copy the `shared_ptr` — no blocking I/O on the RT path.

### `neptr/csound/nam/CMakeLists.txt`
Builds `csound-nam.so`. Compiles NAMCore sources directly into the plugin
(same as NEPTR's version). C++20 required for `atomic<shared_ptr<T>>` in NAMCore.

### `neptr/csound/nam/build-on-pi.sh`
The one build script (replaces nam/setup/build-csound-nam.sh). Uses the vendored
NAMCore (pinned clone only as fallback), verifies C++20 support, builds with -j2,
installs the plugin into csound's system plugin dir.

---

## OSC Control

Port: `DEMIURGE_OSC_BASE` (default 9000).

| Address      | Type | Meaning                              |
|--------------|------|--------------------------------------|
| `/nam/idx`   | `i`  | 0-based profile index (hot-swap)     |
| `/nam/bypass`| `i`  | 1 = passthrough, 0 = NAM active      |

From the Mac:
```sh
oscsend demiurge.local 9000 /nam/idx i 2
oscsend demiurge.local 9000 /nam/bypass i 1
```

---

## Launcher changes (Rust)

`live.rs` — `resolve_lang_from_ext`: `"nam"` → `"nam"`
`supervisor.rs` — `resolve_lang`: pass-through; `candidate_client_names`:
`"nam"` returns `["csound6", "csound", "Csound"]` (same as `"csound"` — because
NAM runs inside CSound, that IS the JACK client).

---

## Profile management

Profiles live in any directory the user chooses. Recommended:
```
~/profiles/
├── clean/
│   └── fender_clean.nam
├── crunch/
│   └── jcm800_crunch.nam
└── high-gain/
    └── 5150_high.nam
```

Profile sources: [ToneHunt](https://tonehunt.org), [tone3000](https://tone3000.com).
File naming convention: `{amp}_{gain_stage}.nam`, lowercase, underscores, no spaces.

---

## Relation to NEPTR

NEPTR phase4 has its own NAM integration: `csound-nam.so` is the same plugin, but
NAMProcess is embedded inside the full NEPTR CSound orchestra (alongside all the
other NEPTR effects). Users of NEPTR do not use `demiurge-run-nam` — NAM is just
one opcode among many in their CSD.

`demiurge-run-nam` is for DEMIURGE users who want NAM as a standalone chain
stage, without NEPTR.

Both use the same `csound-nam.so` binary and `NeuralAmpModelerCore` library.
