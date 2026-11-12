# neuralGrid — Dimensional Map v0.1 (working draft for lock)

**Purpose:** enumerate every graph (space) in the system, its dimensions, each axis's
definition, range, source/anchor, and how it clamps to OSC (NEPTR) vs MIDI (Machine
Orchestra). This is the document that gets *locked* to answer the open item "lock in the
5–8 core perceptual axes for v1."

Framing (from the 2026-07 project briefing): the Neural Grid is a **dimensional
translator** — sensor data mapped across the dimensions of human interaction is
arbitrarily interfaced *through the onboard model* onto a **pre-defined multidimensional
DSP graph** anchored to **perceptual outcomes**, not raw signal parameters. Tuning /
temperament is a runtime render parameter, **never a trained dimension**.

Three graphs, one mapping chain:

```
GRAPH A (interaction space) ──features──> GRAPH C (model spaces) ──decode──> GRAPH B (perceptual DSP graph)
   what the human does              rhythm system + traversal system        what the sound does
                                                                 └──> clamp per target: OSC (NEPTR) / MIDI (Machine Lab)
```

---

## GRAPH A — Interaction space (input)

The dimensions of human action the sensors can actually observe. Raw axes first, then
derived axes computed Pi-side (never on the ESP32 — dumb bridge, per PROTOCOL.md).

### A-raw: sensed axes

| # | Axis | Range (raw → norm) | Sensor | Rate | Notes |
|---|------|--------------------|--------|------|-------|
| A1 | grid x | 0..7 | NeoTrellis 8×8 | event | discrete spatial |
| A2 | grid y | 0..7 | NeoTrellis 8×8 | event | discrete spatial |
| A3 | pad gate | 0/1 per pad | NeoTrellis | event | 64 binary channels, the trigger layer |
| A4 | slider position ×4 | 0..4095 → 0..1 | SoftPot N/S/E/W | ~50 Hz | valid **only while touched** (stability detector, touch.py) |
| A5 | slider touch ×4 | bool | derived from A4 | ~50 Hz | onset/release edges are events in their own right |
| A6 | quadrant pressure ×4 | 0..1 | FSR under grid | ~50 Hz | per-quadrant velocity/aftertouch **(planned — not yet wired)** |
| A7 | hand height ×4 | mm → 0..1 over playable column | LiDAR N/S/E/W via PCA9548A | ~30–50 Hz | the 3D layer; also yields presence bool per side |
| A8 | case excitation | envelope 0..1 | piezo contact mic(s) | ~50 Hz env | resonance/tap/handling energy **(planned)** |
| A9 | time | clock phase | demiurge_clock 24 PPQN | 0xF8 | pulse / 16th step / beat / bar — the 4th dimension, shared with every peer |

Full raw dimensionality: 64 gates + 4 sliders + 4 touch + 4 pressure + 4 height + 1 piezo
+ clock phase ≈ **81 live channels**, but the *interaction space* the model reasons over
is the derived layer:

### A-derived: interaction features (model input, extends `models/features.py`)

| # | Axis | Meaning | Currently implemented? |
|---|------|---------|------------------------|
| D1 | hit rate | pad onsets/sec | yes |
| D2 | spatial centroid x,y | where on the grid activity centers | yes |
| D3 | spatial spread | how scattered activity is | yes |
| D4 | velocity proxy | inter-onset interval → urgency (FSR replaces this when wired) | yes |
| D5 | slider activity / entropy / slide speed | expressive-continuum evidence | yes |
| D6 | interaction mode | direct-trigger ↔ expressive continuum | yes |
| D7 | hand altitude ×4 + approach velocity | LiDAR: height and d(height)/dt per side | no (LiDAR pending) |
| D8 | bilateral symmetry | are opposite sides doing the same thing? (4-sided tabletop is symmetric — symmetry is signal) | no |
| D9 | case energy | piezo envelope + spectral tilt | no (piezo pending) |
| D10 | phase alignment | how event timing sits against the clock grid (early/late/swung) | partial (rhythm system owns this) |

### A-reserve: probability branching (5th dimension)

Held in reserve, **not in the v1 map**: the branching axis treats the performance as a
tree of possible continuations; the player's position on this axis = how far from the
trunk the system is allowed to fork (breadth/depth of the possibility tree per beat).
Reserved hooks: one slot in the config vector + one OSC address (`/ng/axis/branch`),
so adding it later is additive, not schema-breaking.

---

## GRAPH B — Perceptual DSP graph (output)

Axes anchored to **perceptual outcomes** (per the Drake paper on parameters of human
sound perception — to be formally integrated; placeholders marked ⚠ where his taxonomy
must confirm/rename an axis). Every axis is normalized 0..1 in one shared coordinate
space; the **target config** clamps each axis to OSC-continuous or MIDI-quantized.

### B-core: proposed v1 lock — 7 axes

| # | Axis | Perceptual anchor | DSP correlates (NEPTR side) | Machine-Lab clamp (MIDI side) |
|---|------|-------------------|------------------------------|-------------------------------|
| P1 | **Brightness** | dark ↔ bright | spectral centroid, filter cutoff, waveshape | CC → mallet/beater choice, register shift |
| P2 | **Complexity** | pure ↔ dense/rich | partial count, mod depth, layer count, noise ratio ⚠ | CC → voice count, ornament density |
| P3 | **Responsiveness** | diffuse ↔ immediate | attack time, transient sharpness, env slopes | note-velocity curve, roll vs single-hit |
| P4 | **Pitch density** | sparse ↔ saturated register spread | voice count × register span (degrees, tuning-agnostic) | notes-per-beat cap, chord spread |
| P5 | **Rhythmic feel** | grid-locked ↔ elastic | quantize strength, swing, subdivision density, humanize | quantize/swing on generated MIDI (rhythm system owns this axis) |
| P6 | **Resonance / body** | dry ↔ resonant | reverb/body models, decay time, feedback, case-piezo coupling | sustain pedal analog, damped vs ringing instruments |
| P7 | **Spatialization** | point ↔ enveloping (azimuth, spread, distance) | multichannel panning — **first-class axis, not a post FX** | instrument *choice/position* in the physical robot array |

Sub-dimensions: P7 is internally (azimuth θ, spread, distance) — 3 components published
under one perceptual axis. Total v1 output dimensionality: **7 axes / 9 components**.

### B-reserve (v2+, named now so the namespace is stable)

- `branch` (probability branching, ← A-reserve)
- `roughness` ⚠ (sensory dissonance — likely in Drake's taxonomy; promote if his paper ranks it core)
- `motion` (internal rate-of-change/LFO-ness, distinct from rhythmic feel)

### Explicit non-axes

- **Tuning/temperament** (12-EDO, JI, microtonal, Machine Orchestra tunings): runtime
  render table at output, per target. Never trained, never an axis (already the
  convention in `config.py`'s scale/tuning tables).
- **Loudness**: master gain is a performance/safety control, not a model dimension
  (velocity/pressure live in A6/P3 instead).

---

## GRAPH C — Model spaces (the translator between A and B)

Dual system per the briefing:

| Space | Dims | Owner | Role |
|-------|------|-------|------|
| C1 feature window | 12 now → **~20** with D7–D10 | shared frontend | Graph A summary, rolling window |
| C2 rhythm latent | small, discrete-leaning | **rhythm system** | phase alignment, subdivision, groove skeleton — priority 1: rhythmic accuracy + low latency; writes **P5 only** |
| C3 traversal latent | 12 now (VAE) | **dimensional-traversal system** | continuous navigation of P1–P4, P6–P7 |
| C4 config/output vector | 89 now (interface config) → grows a **9-component perceptual block** + 1 reserve slot | boundary object | staged → swapped on the downbeat (Phase 3 mechanism, already built + verified) |

Self-supervised stance: the model defines points in Graph B *itself* from manual-control
experience — the existing manual-first flow (`--brain off/manual/model`) is exactly the
bootstrap for this; no pretraining on pitch/tuning.

---

## Clamping — one coordinate space, two targets

Per-target config file (the existing schema-versioned config discipline) declares, per
axis: `mode: osc | midi`, plus the quantization when `midi`.

**OSC (NEPTR), continuous/semantic** — namespace proposal (open item "OSC namespace"):

```
/ng/axis/brightness      f 0..1        /ng/axis/space/azimuth   f 0..1 (θ/2π)
/ng/axis/complexity      f 0..1        /ng/axis/space/spread    f 0..1
/ng/axis/response        f 0..1        /ng/axis/space/distance  f 0..1
/ng/axis/pitchdensity    f 0..1        /ng/axis/branch          f 0..1  (reserved)
/ng/axis/feel            f 0..1
/ng/meta/config          blob (schema_version, target, axis-clamp map)
/ng/raw/...              optional debug echo of Graph A
```
Rate: axes ≤ ~100 Hz each, downbeat-stamped; raw echoes at sensor rate.

**MIDI (Machine Lab), quantized** — each clamped axis gets a CC (proposal: CC20–CC28 in
axis order) at 0..127, and P4/P5 additionally shape **generated note streams**
(notes-per-beat, quantize/swing) rather than only CCs. Channel/note maps live in the
target config, one file per robot ensemble.

Swapping the target config re-aims the whole instrument without touching inference —
same rule as the Phase-3 downbeat swap, applied at the output stage.

---

## Latency budget (open item — proposed targets to lock)

| Segment | Budget |
|---------|--------|
| Sensor → ESP32 frame | ≤ 2 ms (scan+frame) |
| ESP32 → Pi (USB, COBS protocol now; Ilai custom protocol later) | ≤ 2 ms |
| Pi feature update + **rhythm path** decision | ≤ 3 ms (hot path, no model blocking — two-tier rule) |
| OSC/MIDI out → receiving Pi (NEPTR) / robot interface | ≤ 3 ms LAN/USB |
| **End-to-end, trigger→sound-onset command** | **≤ 10 ms** hard target, 15 ms ceiling |
| Traversal-system config authoring | unbounded (swaps on downbeat — latency musically invisible by design) |

---

## Deltas vs. current repo state (so nobody is surprised)

- Built + verified today: single ESP32 WROOM (grid + 4 softpots direct ADC), COBS
  protocol, Pi live layer w/ menus, config downbeat-swap, bootstrap VAE. That stack
  **is** the A1–A5/A9 + C4 subset of this map — nothing is thrown away.
- Briefing adds (not yet in hardware/code): FSRs (A6), 4× LiDAR via PCA9548A (A7),
  piezo (A8), second Pi (NEPTR as separate box), custom USB protocol (supersedes COBS
  later — keep the *message semantics*, swap the transport), dual rhythm/traversal
  split (C2/C3 — current single VAE becomes C3).
- ⚠ items pend Matthew Drake's paper: axis names/ranking must be reconciled with his
  taxonomy before the lock is final.

## Decisions needed to lock v1

1. Approve the **7 core axes** (P1–P7) — or demote one to reserve to land at 6.
2. Approve OSC namespace `/ng/axis/*` + CC20–28 mapping.
3. Approve ≤10 ms end-to-end budget.
4. Confirm the config vector grows by the 9+1 perceptual block (schema bump v2).
