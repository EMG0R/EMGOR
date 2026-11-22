# Sensor system — what to ship to the Pi (2026-08-14)

Modular sensor→parameter mapping for NEPTR. The expression pedal stops being
hardcoded and becomes one source among many; LiDAR / gyro / FSR / joystick can
drive any of its 35 destinations. Encoders and toggles are untouched.

**Nothing here has run on hardware yet.** Everything below is verified by static
proof, unit tests and localhost round-trips only.

## Two repos, and only part of one is this work

`NEPTR phase4` has **53 files of uncommitted work that is NOT part of this
change** — `effects/*.orc` edits, `nam/src` C++, `state_persist.orc`, and a pile
of untracked effect files. Do not sweep those in by accident. This change is
exactly these 24:

    M  csound/handler/handler_menus.orc      2521-line monolith -> 62-line manifest
    M  csound/midi_handler.orc               OSC input + drain loops + /p
    M  csound/neptrPhase4.csd                one #include for the registry
    +  csound/handler/dispatch/*.orc         14 files, the split dispatch chain
    +  csound/handler/dispatch/gen/*.orc      4 files, GENERATED P-EXPR branches
    +  csound/handler/expr_targets.src.orc   the destination table (source of truth)
    +  csound/handler/expr_registry.orc      GENERATED ExprSet UDO
    +  csound/handler/expr_targets.json      GENERATED, consumed by the web UI
    +  csound/tools/gen_expr_registry.py     generator + --check
    +  csound/tools/gen_expr_encoders.py     generator + --check/--report

`DEMIURGE_OS`:

    +  src/demiurge-io/                      the control plane (stdlib only)
    +  config/demiurge-io.service
    +  setup-script/deploy-io.sh
    M  web/demiurge_web.py                   sensor API, SSE, sim endpoints
    M  web/static/{app.js,style.css,index.html}
    M  web/tests/{test_server.py,test_parsers.py}
    +  web/tests/fixtures/expr_targets.json

Also not mine, already in the DEMIURGE_OS tree: `src/demiurge-clock.cpp`,
`src/demiurge-clock-pll.h`, the Rust launcher sources, `clockrole.rs`.

## Order of operations

1. Sync both repos to the Pi.
2. `sudo bash setup-script/deploy-io.sh`
   Installs the package + unit, seeds `~/demiurge/sensors.json` (12 sensors
   declared, **0 maps** — nothing moves until you map it), and disables
   `neptr-bridge`.
3. Confirm the Teensy's direct MIDI feed survived the bridge going away:
       aconnect -l | grep -i teensy
       aconnect "Teensy MIDI":0 14:0     # if the teardown missed it
   This is what carries encoders and toggles. Without it you lose the controls.
4. Restart the engine. Regenerate-check first:
       cd ~/NEPTR_phase4/csound
       python3 tools/gen_expr_registry.py --check
       python3 tools/gen_expr_encoders.py --check
5. `sudo systemctl start demiurge-io` (or, with no Teensy attached, run it
   headless: `python3 -m demiurge_io --no-teensy` from `/opt/demiurge/src/demiurge-io`).

## First smoke test — no hardware needed

    oscsend 127.0.0.1 7770 /cc iii 1 70 127     # menu should step
    oscsend 127.0.0.1 7770 /p   if 31 0.8       # overdrive drive to 0.8

Then the web UI: `/neptr/panel` is a virtual instrument (controls out as `/cc`,
sensors out as raw UDP into demiurge-io), and `/neptr/sensors` is the mapping
editor with live meters.

**The one thing the simulator cannot tell you**: whether encoders and the pedal
still FEEL right through the new path. That needs fingers on knobs.

## Port map — collisions here fail silently

    7770  control INTO csound (/cc, /p)
    8000  engine state OUT (UIs listen)
    8001  wavetable OSC — neptrPhase4_UI.py owns it, do NOT bind
    8002  sensor telemetry OUT (web UI meters)
    8003  pushed sensor values IN (virtual panel, phone)

## Rollback

Engine: `git checkout csound/handler/handler_menus.orc csound/midi_handler.orc
csound/neptrPhase4.csd` and delete `handler/dispatch/`, `handler/expr_*`. The
split was proven byte-identical, so reverting is exact.

Control plane: `sudo systemctl disable --now demiurge-io` and re-enable
`neptr-bridge`. The engine keeps working either way — `-Ma` is still live and
MIDI remains the fallback path by design.

## Known, deliberate gaps

- `sinks/video.py` and `sinks/game.py` are not built. The interface is defined.
- Phone gyro: `sources/web.py` accepts pushed values on 8003, but nothing in the
  browser sends `DeviceOrientation` yet.
- **FREEZE (toggle 72) has three dead encoder branches.** It engages by gesture
  and sets only `gkLastToggle29`, never `gkLastToggle`, so no rotation branch for
  it can fire. Pre-existing, unrelated to this change. One-line fix, not applied
  because it would start stealing encoder focus on a played effect.
