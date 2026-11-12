# NEPTR Phase 4 — Looper Pedal Design

**Date:** 2026-06-22
**Status:** Approved design, pre-implementation
**Author:** brainstormed with Emory

---

## 1. Overview

Add a **5-channel looper** to NEPTR Phase 4, exposed as a new `LOOPER` menu in the
pygame UI and a new looper engine in Csound. The control surface is the existing
**5 push-encoders** (push + twist each), arranged left→right (slot 4 leftmost =
OVERDRIVE preset position, slot 0 rightmost = REVERB position). Each encoder owns
**one independent loop channel**: push for transport, twist for that channel's
speed/reverse.

Design priorities, in order: **fast/tight timing**, **clean audio (pure varispeed)**,
**loops that phase-lock to each other in clean fractions without ever shifting the
audio you played**.

### Goals (v1)
- 5 independent loop channels, one per push-encoder.
- Per-channel record / play / stop / overdub (infinite layering) / clear.
- The first loop recorded is the **master** and defines the grid; channels 2–5
  round their length to clean power-of-two fractions/multiples of the master.
- Relative phase-sync: starting a channel off the beat snaps the loop *boundary*
  to the nearest master subdivision **without time-shifting the recorded audio**.
- Per-channel varispeed speed + reverse on the encoder twist, symmetric about a
  center median, pure varispeed (no pitch-preserving/time-stretch processing).
- Loops keep running while you navigate to other menus to tweak effects.

### Explicitly out of scope (future, hooks reserved)
- **Native A/B / song-section snapshots** — done manually with the 5 channels for now.
- **External BPM / clock sync** — `gkGLITCH_bpm` (already synced from DEMIURGE) is the
  reserved hook; v1's grid is the master loop itself, no external clock required.
- **NeoPixel ring playhead** — optional later nicety.

---

## 2. Control Scheme (per channel)

All five channels are identical and independent. Three gestures on each encoder's
push switch (**short tap**, **hold**, **double tap**) plus the **twist**.

Every gesture **acts instantly** — no waiting to disambiguate — so timing stays tight.
Double tap is a distinct, deliberate gesture for the one destructive action (clear).

### State machine

| State | Short tap | Hold | Double tap | Twist |
|---|---|---|---|---|
| **Empty** | Start **Recording** | — | — | (no effect) |
| **Recording** (first pass) | End recording → set length → **Playing** | **Cancel/wipe** → Empty | — | (no effect) |
| **Playing** | **Stop** → Stopped | Start **Overdub** | **Clear** → Empty | Set speed/reverse |
| **Overdubbing** | Commit → **Playing** | **Cancel this overdub session** → Playing | **Clear** → Empty | Set speed/reverse |
| **Stopped** | **Play** (re-aligned) → Playing | Start **Overdub** → Overdubbing | **Clear** → Empty | Set speed/reverse |

Notes:
- **Recording → End (short tap)** is single-only and acts immediately — this is the
  timing-critical moment that defines the master grid, so it must never wait.
- **Hold during Recording = cancel/wipe** the in-progress take (works for the master
  and for channels 2–5's initial take).
- **Overdub is infinite layering:** while overdubbing, input is summed into the loop
  every pass, accumulating layers across loop boundaries until a short tap commits or
  a hold cancels the whole overdub session.
- **Clear** is destructive → UI flashes a confirm cue when it fires.

### Hold / double-tap detection
Reuse the proven GLITCH press/release pattern (currently a 0.30 s hold threshold in
`globals.orc`). Add a double-tap window (~250–300 ms). **All three classifications
(tap / hold / double) live in one detector block** so the logic is in one place — this
is the "same area as the hold-vs-press logic" requirement. The block emits a single
classified event (`TAP` / `HOLD` / `DOUBLE`) per gesture to the channel state machine.

---

## 3. Speed / Reverse (per channel twist)

Single bipolar varispeed control, **symmetric about a center median**, with soft
detents at each named ratio:

```
right (CW)  +2×   +1×   +½×   +¼×  ‖  −¼×   −½×   −1×   −2×   left (CCW)
            fast        slow   slow    slow         fast
            fwd         fwd   fwd  median  rev          rev
```

- Right of median = forward, slowing from 2× down to ¼×.
- Crossing the median flips to reverse at ¼×, ramping up to reverse 2×.
- **Pure varispeed**: playback is interpolated table read with read-increment =
  signed speed. Pitch tracks speed naturally (octave up at 2×, down at ½×, etc.).
  **No time-stretch, no pitch-shift, no formant tricks — clean resampled audio only.**
- Negative speed = read pointer runs backward (reverse).
- Use cubic/`table3`-grade interpolation to keep varispeed clean at fractional rates.

---

## 4. Quantization & Sync Model

Two separable behaviors, both relative to the **master loop** (the first channel
recorded). No external clock in v1 — the master loop *is* the grid.

### 4.1 Length quantization (channels 2–5)
When a channel ends recording, its length `Lk` snaps to the nearest **power-of-two
ratio** of the master length `L0`: … ¼·L0, ½·L0, 1·L0, 2·L0, 4·L0 …
- End early (cut partway) → rounds down (½, ¼).
- Keep recording past the master → rounds up (×2, ×4).
This is the EDP/Pigtronix "master cycle, integer relations" model.

### 4.2 Relative phase-sync (the key feel feature)
Starting a channel slightly before/after the "1" (or the ½ / ¼ point) must **not
shift the audio**. Instead:
- The recorded audio is stored starting at buffer sample 0 (where you actually began).
- The channel's loop **restart point is phase-aligned** to the nearest master
  subdivision, so once `Lk` is a clean ratio of `L0`, the channel stays locked in
  phase with every other channel — in clean fractions — while the audio keeps its
  natural feel and timing.
- Implementation intent: maintain a master phase counter `0…L0`; on a channel's
  play/commit, set its read offset so buffer-sample-0 lines up with the nearest master
  grid fraction. Never resample or slide recorded content to "correct" timing.

### 4.3 Future clock hook
When clock sync is added later, the master loop locks to `gkGLITCH_bpm` (DEMIURGE
master tempo over MIDI ch16 CC119) and grid fractions become true bars/beats. Design
the grid math so swapping "master-loop-defines-grid" for "BPM-defines-grid" is a
localized change.

---

## 5. Csound Audio Engine

New file: `csound/effects/looper.orc` (new instrument number, e.g. 85). Follows the
existing effect conventions (global send/return buses, bypass macro, OSC feedback).

### Buffers
- One mono recording buffer (ftgen table) per channel × 5.
- Recommended size **2^21 samples (~44 s @ 48 kHz)** per channel as the starting point
  (tunable). Mono keeps memory sane on the Pi; playback fans mono → both outputs.
- **Overdub undo:** keep one pre-overdub snapshot buffer per channel so "cancel
  overdub session" restores cleanly. This doubles looper buffer memory — **verify
  memory + CPU headroom on the Pi** and reduce buffer size (2^20 ≈ 22 s) if needed.

### Transport / heads
- Record: write head advances at 1× while in Recording/Overdubbing.
- Play: read pointer advances by signed `speed` per sample, wraps at `Lk`,
  phase-locked per §4.2.
- Overdub: `buf[pos] += input` each pass (infinite layering).

### Insert point in the signal chain
Insert the looper **late in `signal_chain.orc`**, capturing the fully processed
playing signal so loops play back with your tone, and summing loop output into the
final mix. Exact placement (pre- vs post-REVERB) to be finalized in the plan; default
is **post everything, last stage before output**, so recorded loops include the full
processed sound. Loops persist regardless of which menu/effects are active.

### Channels & OSC
Per channel `n` (1–5): state global `gkLoopState_n`, length `giLoopLen_n`, speed
`gkLoopSpeed_n`, plus playhead phase for UI. Emit OSC to the UI:
`/loop_ch{n}_state`, `/loop_ch{n}_pos`, `/loop_ch{n}_speed`, `/loop_ch{n}_ratio`.

---

## 6. UI (pygame)

New menu appended to `MENUS` in `neptrPhase4_UI.py` (becomes the 10th menu, index 9):
`{"name": "LOOPER", ...}`. Reached via the existing prev/next menu navigation.

### Rendering (custom-drawn, minimal CPU — per project UI taste)
Five channel strips, reusing the existing Max/MSP fill-slider aesthetic. Each strip shows:
- **State** by color (Empty / Recording / Playing / Overdubbing / Stopped).
- **Playhead**: a moving progress indicator around the loop (the looper's LED-ring
  equivalent — the single most important "fast & musical" feedback).
- **Speed/reverse**: numeric/iconic (e.g. `2×`, `½×`, `◄1×` for reverse).
- **Length ratio** vs master (`×1`, `½`, `×2`, …).
- A brief **Clear** confirm flash.

No widget toolkit; pure pygame drawing driven by the OSC messages above. While in any
other menu, the 5 encoders return to their normal effect-knob roles; loops keep playing.

---

## 7. Integration Points (the add-effect checklist)

Per the established "5 Csound files + UI" pattern an effect touches:

1. **`csound/globals.orc`** — looper globals (state, lengths, speeds, thresholds),
   ftgen buffer tables, and the unified **tap/hold/double** detector macro.
2. **`csound/signal_chain.orc`** — insert looper instrument at the chosen late stage;
   wire send/return and sum loop output into the mix.
3. **`csound/midi_handler.orc`** — in the LOOPER menu, route each encoder's push
   (→ gesture detector → channel state machine) and twist (→ channel speed).
4. **`csound/neptrPhase4.csd`** — include/define the looper instrument(s).
5. **`csound/effects/looper.orc`** (new) — the looper engine (record/play/overdub/
   varispeed/quantize per channel).
6. **`neptrPhase4_UI.py`** — add the `LOOPER` menu to `MENUS`, the channel-strip
   renderer, and OSC handlers for state/playhead/speed/ratio.

---

## 8. Testing & Verification Strategy

Per the directive: *everything working perfectly, logic adversarially double-checked,
tests run.*

- **Reference state machine in Python, TDD-first.** Implement the per-channel state
  machine (§2) as a pure, isolated Python model with unit tests covering every
  transition and edge case: double-tap-during-recording, hold-cancel of master vs
  overdub, infinite overdub across boundaries, clear from each state, tap immediately
  after record-end. The Csound implementation must match this model's behavior.
- **Quantization math tests.** Unit-test §4.1 length rounding (power-of-two nearest)
  and §4.2 phase-alignment offset across a spread of start offsets, asserting "loop
  boundary snaps, audio sample-0 never moves."
- **Adversarial logic review.** Independent agents review the state machine and
  quantization logic to refute correctness before porting/committing.
- **Hardware verification on the Pi.** Audio cleanliness (pure varispeed at ¼×/2×,
  forward & reverse, no zipper/aliasing nasties), memory + CPU headroom with all 5
  channels + overdub snapshots active, and timing/phase-lock across channels.

---

## 9. Open Implementation Details (resolved in the plan, not blocking design)
- Exact looper insert point (pre/post REVERB).
- Final buffer size (2^20 vs 2^21) after Pi memory check.
- Interpolation order for varispeed read (`tablei` vs `table3`).
- Double-tap window length tuning (~250–300 ms) and hold threshold reuse (0.30 s).
- OSC message rate for the playhead (smooth but cheap).
