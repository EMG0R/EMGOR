# neuralGrid — Design Spec

**Date:** 2026-07-06
**Author:** emgor
**Status:** Design locked, ready for implementation planning
**Runs on:** DEMIURGE_OS (Raspberry Pi 5, 16GB)

---

## 1. One-line thesis

A Monome-style grid instrument that **rewrites its own interface on the beat**: a real-time live layer plays with zero detectable latency while tiny local neural models watch how you play, author a new interface configuration, and swap it in — quantized to the global clock so model latency is never felt.

For NIME, two defensible novel contributions:
1. **Beat-quantized interface reconfiguration** — the interface morphs on the downbeat, making model latency musically invisible.
2. **Tuning-agnostic melodic modulation** — the melody model works in scale-degree space, so it modulates the same way under any harmonic system, including uneven/microtonal tunings.

---

## 2. Core architecture — two tiers, latency-isolated

Two hard-separated tiers, the central design commitment of the project:

- **LIVE LAYER** (real-time, sacred core): reads Pico sensor events → emits MIDI/OSC → drives grid LEDs → plays *now*. Deterministic, never blocks on a model. Pinned to the isolated CPU core (DEMIURGE's `isolcpus=3` / `CPUAffinity=3` pattern).
- **MODEL LAYER** (async, other cores): watches the interaction stream, authors a new **config**, stages it. The live layer **atomically swaps it in on the downbeat** via `demiurge-clock`.

The boundary is a **schema-versioned config** + a **clock-gated atomic swap**. The model can be arbitrarily slow; worst case the interface simply doesn't morph this beat. This is DEMIURGE's `live.conf` hot-reload discipline applied to the instrument's own brain.

```
Pico sensors ──serial──> LIVE LAYER (core 3, real-time) ──MIDI/OSC──> DEMIURGE bus ──> RAVE / synths
                              │  ▲                                          
                     interaction │  │ config swapped on downbeat            
                          stream │  │ (clock-gated)                         
                              ▼  │                                          
                          MODEL LAYER (cores 0–2, async)                    
                          writes next config + latent OSC                   
```

---

## 3. Hardware / construction

- **Raspberry Pi 5 16GB**, stock. **No AI HAT** — models are tiny; RAM use ~1–2GB of 16.
- **Raspberry Pi Pico (RP2040)** — single MCU, USB-serial (CDC) to the Pi. No WiFi radio to steal cycles from sensor timing.
  - **I2C bus 0:** 4× Adafruit NeoTrellis 4×4 (addresses 0x2E–0x31 via jumpers) → 8×8 = **64 RGB pads**.
  - **I2C bus 1:** 4× VL53L1X time-of-flight lidar (corners) — unique addresses assigned via **XSHUT boot sequencing**.
  - **1 ADC pin ← 74HC4067 16-ch analog mux:** 4× ribbon (SoftPot membrane) + 4× FSR (velocity) + piezo(s), with spare channels to grow.
  - **Core 0:** I2C (grid key events + LED writes + lidar reads). **Core 1:** analog mux scan. Framed binary events over USB serial.
- **Power:** NeoTrellis LEDs at full white pull real current — **separate 5V rail for the grid**, common ground, do not back-power the Pico.

### Hardware risks
- **I2C bandwidth:** 64-pad LED refresh + 4 lidar across two buses. If it can't keep both snappy, move LED driving to the RP2040 **PIO** or a second Pico. (Cheap fallback, no architecture change.)
- **VL53L1X addressing:** all default to 0x29 — careful XSHUT sequencing at boot is required.

---

## 4. Firmware (Pico)

C/Arduino or CircuitPython. **Dumb and fast — no musical logic on the Pico.**
- **Emits** a compact framed event stream: pad on/off + xy, ribbon position/velocity, lidar height ×4, FSR pressure, piezo peak.
- **Accepts** LED-state frames from the live layer.
- Flashed via the Pico's native **UF2** path (DEMIURGE's Teensy baud-134 flasher does not apply; UF2 is trivial).

---

## 5. The four-mode stack

The instrument is a stack. Each layer is gig-ready on its own; **A–C work with the model switched off.**

| Mode | What it is | Nature |
|---|---|---|
| **A — Instrument** | grid + sensors as a plain playable controller / remote instrument | hand-coded foundation |
| **B — Sequencer** | normal Monome-style step sequencer | hand-coded foundation |
| **C — MIDI effects** | pre-coded arps / echo / quantize / etc. | hand-coded foundation |
| **D — The brain** (cherry) | Interface-VAE reconfigures A/B/C live + Melody/Rhythm VQ-VAE mimics & modulates the player | the research contribution |

Mode D's Interface-VAE reconfigures **which of A/B/C behaviors are live on which controls and how** — so the four modes are not separate apps, they are the **vocabulary the brain reconfigures over**.

---

## 6. The live layer

A DEMIURGE `chain=` / `midi=` stage (Python or C++). Owns: pad→note/trigger, sensor→CC/effect, sequencer, MIDI effects, LED rendering. Reads the **active config** to decide what each control does *right now*. On each downbeat it checks for a staged config and swaps atomically. Talks MIDI on `Midi Through` and tempo-locks to `demiurge-clock` — so it, NEPTR, and RAVE share clock/transport for free.

**Priority output:** MIDI/OSC note + CC. Works standalone; needs nothing from the model or RAVE.

---

## 7. The model layer — three tiny local models

Everything is **latent-space navigation, quantized to the beat.** All sub-1M params, trained by emgor, run local on the Pi off the isolated core. Rhymes conceptually with the collaborator's RAVE (latent audio ↔ latent interface/melody).

### 7.1 Interface-VAE (continuous VAE, sequence-aware encoder)
- **Input:** rolling interaction-feature window + a derived **interaction-mode estimate** (direct-trigger ↔ expressive continuum, read from *which* sensors are active — hammering pads vs. sliding ribbons / hovering lidar).
- **Output:** the next **config vector** — layout, scale/tuning selection, densities, velocity curves, effect sends, **LED language**.
- **Character:** supportive, anticipatory **mirror** — low-temperature sampling keeps configs close to the player's habits. It *supports* the mode you're in, does not fight it.
- **Boundary:** writes config → live layer swaps on the downbeat.

### 7.2 Melody/Rhythm model (VQ-VAE, factorized)
Discrete-latent (VQ) — discrete codes suit symbolic rhythm/pitch, give clean "swap one code = one variation" control, and rhyme harder with modern neural audio (RAVE/encodec).
- **Phrase source:** **live capture** — grabs phrases the player performs on the instrument and modulates those. It mimics *you*.
- **Factorized latents:**
  - **Rhythm latent** — the groove/skeleton. **Preserved** ("same rhythm"). Trained **first**, on rhythm characteristics.
  - **Pitch/harmony latent** — modulated on top, **conditioned on incoming harmony** (from what you play, a chord track, or the collaborator's side).
- **Tuning-agnostic:** the model works in **scale-degree / contour space** ("up 2 steps, down 1"), never absolute pitch. A separate **tuning table** renders degrees → frequencies. Because unevenness lives only in the render table, the model modulates identically under 12-EDO, 7-note uneven scales, 19-EDO, or arbitrary microtonal/user scales.

### 7.3 Style conditioning (conditional VAE / FiLM)
A dial on *how* 7.2 modulates (transpose / densify / swing / reharmonize / ornament). **Emergent style first** (v1: latent directions learned from a melody corpus), **explicit style vector second** (v2, if emergent feels too vague). Trained after rhythm + harmony work.

### Training data (all sources, phased)
- **Synthetic bootstrap + hand-authored presets** → models are valid **day one**, before any real data exists.
- **Logged live sessions** (interaction → config-you-landed-on, and captured phrases) → learns *your* style.
- **Public MIDI corpus** → general musicality for the melody/style latents.
- **Schema-versioned config:** fixed-length parameter vector *for now*, schema grows as the format develops; retrain on schema bump; old logs stay usable via version tags.

### Training order
Rhythm characteristics → harmonic modulation → style conditioning.

---

## 8. Integration — DEMIURGE + RAVE

Everything meets on the DEMIURGE bus, all tempo-locked to `demiurge-clock`. RAVE is "just another peer."

- **Priority path (standalone):** neuralGrid emits MIDI/CC → RAVE or any synth as a downstream local engine (`chain=` stage). Works with nothing else present.
- **Bonus path (optional, additive):** a parallel **OSC latent stream** — `/neuralgrid/latent/style`, `/neuralgrid/latent/rhythm`, `/neuralgrid/latent/interface` — that the collaborator's RAVE can subscribe to and map into *its* conditioning. **Latent-to-latent**, two neural instruments steering each other ("shared latent performance"). Loosely coupled: if RAVE isn't there, nothing breaks.

---

## 9. Compute budget (stock Pi 5 16GB)

- **RAM — non-issue.** VAEs are a few MB each; RAVE model ~hundreds of MB; audio buffers tiny. **~1–2GB of 16 used.**
- **CPU — the constraint, and only RAVE matters.** 4 cores:
  - **Core 3 isolated** → live layer + audio (deterministic).
  - **Cores 0–2** → RAVE + VAEs + OS.
  - **VAEs are effectively free** — microsecond inference, fire once per beat, not per sample.
  - **RAVE is the only heavy tenant** — a small RAVE model runs near-realtime on Pi 5 CPU (~1–2 cores). The one thing to benchmark.

---

## 10. Build order (adaptive loop ASAP, de-risked)

1. **Pico firmware + one NeoTrellis + mux** → clean event stream over serial.
2. **Live layer skeleton (Modes A–C)** → pads/sensors → MIDI into DEMIURGE, LED feedback, clock-locked. *A real, gig-ready instrument exists here.*
3. **Config boundary** → live layer reads a config vector; manual beat-quantized swaps prove the mechanism.
4. **Interface-VAE (v1)** → synthetic/authored-trained, emitting configs live. **Adaptive loop closed. Headline demo/paper achievable here.**
5. **Melody/Rhythm VQ-VAE** → live capture, rhythm-preserving modulation, tuning-agnostic. Style conditioning. **The ambitious second movement.**
6. **Logging + retrain** on real sessions; **RAVE** integration (MIDI path, then optional latent-OSC); scale to full 8×8 + 4 lidar + all analog.

---

## 11. Open risks

- **I2C bandwidth** — LED + lidar load; PIO or second Pico is the fallback.
- **VL53L1X XSHUT** boot addressing sequence.
- **Config schema drift** — every schema bump = retrain; versioned so logs survive.
- **RAVE realtime headroom** — the one real compute unknown; benchmark in Phase 6.
- **VQ-VAE training** — discrete-latent models can be finicky (codebook collapse); budget iteration.

---

## 12. Scope notes

- This is **one architecture spec.** Each build phase (Section 10) gets its own implementation plan.
- Modes A–C are a complete instrument independent of the model — the safety floor.
- The melody/style brain (Phase 5) is additive and does not block the headline (Phase 4).

---

## 13. Revision 2026-07-17 — MCU switch, interaction model, serial contract

This revision supersedes earlier MCU/firmware decisions. Everything above about the two-tier
architecture, modes A–D, config boundary, and models is **unchanged**. What changed is the
microcontroller and the concrete interaction model.

### 13.1 MCU: Pico → ESP32 (Pico scrubbed)

**The Raspberry Pi Pico (RP2040) is dropped entirely.** The single MCU is now an **ESP32 dev
board**, already wired and working:
- **8×8 NeoTrellis grid** on I2C (SDA = GPIO21, SCL = GPIO22), 2×2 tiling, addresses `0x2E 0x2F
  0x30 0x31`.
- **4× SoftPot ribbons** on direct ADC1 pins (GPIO32/33/34/35) — no mux needed at 4 channels.
  (The 74HC4067 mux was a Pico-era plan; revisit only when FSR/piezo channels are added.)
- WiFi/Bluetooth left **off**; ADC1 only.

Wherever this spec says "Pico," read "ESP32." The firmware philosophy is **unchanged and
reinforced**: dumb and fast, no musical logic, never rewritten by the agent.

### 13.2 The hard firmware/Pi split

- **ESP32 = dumb I/O bridge.** Sends raw sensor/button data up; renders full LED/UI
  instructions down in real time. Nothing else. Its firmware is frozen — when the model
  "rewrites the interface," it rewrites **Pi-side config**, never ESP32 code.
- **Pi = the entire brain.** Touch detection, modes, sequencer, MIDI-fx, MIDI out, clock
  lock, and later the neural models — all on the Pi.

### 13.3 Serial contract

Defined in **`docs/PROTOCOL.md`** (COBS-framed binary, bidirectional, CRC-checked). This is
the locked contract between firmware and the Pi live layer. Sensor/key events flow up; LED
frames (full + partial), brightness, clear flow down. Raw ADC only — **the ESP32 does no
noise filtering or touch detection**; that lives on the Pi so it can be tuned/rewritten
without reflashing.

### 13.4 Interaction model — softpot-per-menu, "touched-last selects"

The 4-mode stack (§5) is surfaced as **one menu per SoftPot**:

| SoftPot | Menu |
|---|---|
| 1 (N) | **Mode A — Instrument** (MIDI player) |
| 2 (S) | **Mode B — Sequencer** |
| 3 (E) | **Mode C — MIDI effects** |
| 4 (W) | **Mode D — brain** (reserved; a global param until D exists) |

**Switching:** touching and scrolling a SoftPot makes its menu the active one; within a menu
that SoftPot is the menu's live control. "Whichever I definitely touched last" wins — so
touch detection must be unambiguous.

**Noise vs. touch (the crux):** untouched SoftPots float and read wideband noise; a real
touch is a stable, in-range reading. Handled on the Pi with a per-channel touch state machine
(variance/stability gate + debounced onset/release). Hardware aid: ~100 kΩ pulldown wiper→GND
(required on input-only GPIO34/35), strongly recommended.

### 13.5 Live layer language

**Python** for the Phase 2 skeleton (pyserial + `python-rtmidi`/`mido`): it emits MIDI at
gesture rate, not per-sample audio, so timing is adequate. If jitter appears, port the hot
path to C++ without changing the serial contract. Runs as a DEMIURGE stage; MIDI on `Midi
Through` (14:0); tempo-locked to `demiurge-clock`.
