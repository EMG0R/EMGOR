# neuralGrid — AI Handoff

Continuation doc for the next AI/session. Read this, then the spec, then continue at the
current build phase. Author: emgor (maintainer@example.invalid).

## What this is (one line)

A Monome-style grid instrument on a **Raspberry Pi 5 16GB** (running DEMIURGE_OS) that
**rewrites its own interface on the beat**: a zero-latency live layer plays now, tiny local
neural models watch how the player plays and author a new interface config, swapped in
quantized to the global clock so model latency is never felt. Target: working instrument +
a NIME paper. A collaborator is building a RAVE-adjacent neural-audio program that will
integrate locally over the shared bus.

## Read these first (in order)

1. **`docs/superpowers/specs/2026-07-06-neuralgrid-design.md`** — the full locked design spec.
   Hardware/BOM, firmware, the 4-mode stack, the three models, integration, compute budget,
   phased build order, risks. THIS IS THE SOURCE OF TRUTH for intent.
2. **`/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/README.md`** — the OS this
   runs on. Provides the MIDI bus (`Midi Through`), global clock (`demiurge-clock`, 24 PPQN,
   ch16 CC119 bpm / CC118 retune / CC117 transport), hot-reloadable `live.conf`, PipeWire
   audio abstraction, core isolation (`isolcpus=3`). We build ON this, not from scratch.
3. **`/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/PHASE4IMPLEMENTATION.md`**
   — emgor's existing live instrument (NEPTR) on the same platform; reference for MIDI/OSC
   conventions, Csound/ChucK stages, sync workflow.

## Locked decisions (do not relitigate without emgor)

- **MCU:** Raspberry Pi Pico (RP2040), USB-serial to the Pi. WiFi-free = clean timing.
- **Grid:** 4× Adafruit NeoTrellis 4×4 tiled 2×2 = 8×8 / 64 RGB pads, one shared I2C bus,
  addresses `0x2E 0x2F 0x30 0x31`.
- **Sensors:** 4× VL53L1X lidar (corners, 2nd I2C bus, XSHUT addressing), 4× ribbon (SoftPot),
  4× FSR (velocity), piezo(s) — all analog via a **74HC4067 16-ch mux** into one Pico ADC pin.
- **Two-tier latency isolation:** LIVE LAYER (real-time, pinned to isolated core) never blocks
  on the model. MODEL LAYER (async) writes a config; live layer swaps it **on the downbeat**.
- **Config boundary:** schema-versioned parameter vector, hot-swapped like DEMIURGE's live.conf.
- **Models — three tiny local ones (all sub-1M params, trained by emgor, run off the isolated core):**
  - **Interface-VAE** (continuous, sequence-aware encoder) → reconfigures the interface; supportive mirror.
  - **Melody/Rhythm model = VQ-VAE**, factorized (rhythm latent PRESERVED, pitch latent MODULATED,
    conditioned on incoming harmony). Works in **scale-degree/contour space** → tuning-agnostic
    (same behavior under 12-EDO, uneven, microtonal). Phrase source = **live capture**.
  - **Style conditioning** (conditional VAE) → dial on how the melody modulates. Emergent first, explicit second.
  - Train order: rhythm → harmony → style.
- **4-mode stack:** A instrument · B sequencer · C MIDI effects (all hand-coded, gig-ready) ·
  D = the neural brain (the research cherry, reconfigures over A/B/C).
- **Integration:** priority output = MIDI/CC on the DEMIURGE bus (standalone). Optional bonus =
  latent-OSC stream (`/neuralgrid/latent/style|rhythm|interface`) for the collaborator's RAVE
  to consume (latent-to-latent). Additive; nothing breaks if RAVE absent.
- **Compute:** stock Pi 5 16GB, NO AI HAT. RAM ~1–2GB. Only RAVE is CPU-heavy (benchmark it).

## Status — where we are (updated 2026-07-17)

**MCU CHANGED: Pico is scrubbed → classic ESP32 WROOM.** See spec §13 and `docs/PROTOCOL.md`.
Firmware is a dumb bridge (`firmware/esp32/grid_link/`); all logic is Pi-side under `pi/`.
Flash the WROOM **from the Pi** (`demiurge@neuralgrid.local`, `/dev/ttyUSB0`), never the Mac.

- [x] Design spec written & locked (+ §13 revision: ESP32, interaction model, serial contract).
- [x] **Phase 1 DONE (verified on hardware):** COBS-framed binary protocol both ways
      (`docs/PROTOCOL.md`). ESP32 sends raw KEY + ANALOG; Pi sends LED frames. HELLO handshake +
      LED render confirmed on the real WROOM. Pi codec: `pi/neuralgrid/{codec,link,touch}.py`.
- [x] **Phase 2 DONE (self-tests green on Pi):** live layer — `pi/neuralgrid/live/` engine +
      Instrument menu (press→note+light) + Sequencer demo (clock-locked playhead) + softpot
      menu-switching + MIDI out to `Midi Through` + clock-lock to `demiurge_clock`.
      Run: `python3 pi/run_live.py --port /dev/ttyUSB0`. *Interactive feel still needs emgor's hands.*
- [x] **Phase 3 DONE (self-tests green on Pi):** config boundary — `pi/neuralgrid/config.py`
      (schema v1, `VECTOR_LEN=89`), staged→active atomic swap on the downbeat, manual stager
      (`--stage-demo`). Requires numpy (installed on Pi).
- [x] **Phase 4 DONE (self-tests green on Pi, ran clean on hardware):** Interface-VAE — pure-numpy,
      4,561 params, trained on synthetic bootstrap (`pi/neuralgrid/models/`, weights
      `interface_vae.npz`). Wired as `--brain {off,model,manual}` (default off = safety floor).
      `python3 pi/run_live.py --port /dev/ttyUSB0 --brain model` closes the adaptive loop. HEADLINE.
      *Musical/visual behavior of the morphing still needs emgor's hands + ears.*
- [ ] Phase 5: Melody/Rhythm VQ-VAE (live capture, tuning-agnostic) + style conditioning.
- [ ] Phase 6: session logging + retrain on real data; RAVE integration; scale to full sensors.

## Files that exist now

```
neuralGrid/
├── HANDOFF.md                                   ← this file
├── docs/superpowers/specs/
│   └── 2026-07-06-neuralgrid-design.md          ← THE SPEC (read first)
└── firmware/pico/grid_test/
    ├── grid_test.ino                            ← WORKING 8×8 rainbow + white-on-hold
    ├── README.md                                ← tiling, addresses, power, level-shift, flashing
    └── build/grid_test.ino.uf2                  ← compiled firmware (drag to RPI-RP2)
```

## Hardware wiring (as built)

- Grid → Pico: **SDA = pin 6 (GP4)**, **SCL = pin 7 (GP5)**, **GND = pin 8**.
- NeoTrellis VIN: external power (5V + level shifter on SDA/SCL, OR external 3.3V no-shifter).
  Do NOT run 64 pixels off the Pico 3V3 pin. See `firmware/pico/grid_test/README.md`.
- Not yet wired: lidar (I2C bus 1), analog mux + ribbon/FSR/piezo.

## Flashing workflow (works headless from the Mac)

`arduino-cli` is installed. Core: **`arduino:mbed_rp2040`** (official, installed). Libs installed:
Adafruit seesaw + Adafruit BusIO.

```sh
cd neuralGrid/firmware/pico/grid_test
arduino-cli compile --fqbn arduino:mbed_rp2040:pico --output-dir ./build .
# Put Pico in BOOTSEL (hold button, plug USB into Mac) → mounts /Volumes/RPI-RP2
cp build/grid_test.ino.uf2 /Volumes/RPI-RP2/     # auto-reboots into new firmware
```

Gotcha fixed during Phase 1: `MultiTrellis.activateKey(x,y,edge)` is ambiguous — pass the
explicit 4th arg `true` → `activateKey(x, y, EDGE, true)`.

## Process note

This project was scoped via the superpowers brainstorming flow. Each build phase gets its own
implementation plan (writing-plans skill) before coding. Modes A–C are the gig-ready safety
floor; Mode D is the paper. Keep the live layer sacred — the model must never sit in the hot path.

## Immediate next step

Extend `grid_test` (or fork to `firmware/pico/grid_io/`) into the full **Phase 1 event stream**:
framed binary USB-serial protocol emitting pad on/off+xy, then add the analog mux scan and lidar.
Define the serial framing first (it's the contract between firmware and the live layer). Then
start Phase 2 (live layer) as a DEMIURGE `midi=` stage. Confirm intent with emgor before big moves.
