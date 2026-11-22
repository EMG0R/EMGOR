# TAPE — Generation Loss-style tape machine (replaces DROPOUT)

Date: 2026-06-11
Status: approved design, pre-implementation
Primary reference: `docs/reference/GenerationLoss_MKII_Manual_Chase_Bliss.pdf`
(complete official web guide; signal flow on p. 44 is the ground truth for stage
ordering). When this spec and the manual disagree about *behavior*, the manual
wins; when they disagree about *scope*, this spec wins (we deliberately macro-ize
and extend — the expression-pedal transport is ours, not the pedal's).

---

## 1. Goal

Replace DROPOUT (instr 500, toggle 21, MENU 1 VOLUME s4, CC72) entirely with
**TAPE**: a Chase Bliss Generation Loss MKII-inspired tape machine with

1. dropouts/failure as an active musical part (drops + crinkles/pops, the old
   dropout.orc's job done properly),
2. an expression-pedal **transport** — instant tape slowdown/speedup gestures
   with no functional latency while playing,
3. moderately aggressive magnetic-hysteresis **saturation**,
4. a **generative** system: stochastic pitch spikes/warps plus a slow
   self-evolving drift layer, all macro'd onto one AMOUNT knob.

DROPOUT is removed from the project: `dropout.orc` deleted, `gaDrop*` buses
removed, globals renamed, menu label changed. Toggle ID **21** is inherited.

---

## 2. How the real Gen Loss MKII works (condensed from the manual)

Signal flow (manual p. 44, DRY TYPE: STANDARD):

```
INPUT ─► SATURATION ─► WOW & FLUTTER ─► MODEL ─► DROPS ─►(+ DRY)─► OUTPUT
```

- **SATURATE**: "magnetic hysteresis loop" — the magnetic structure of the tape
  shifts, altering future recordings; i.e. distortion with *memory*, not a static
  waveshaper. Subtle for most of the sweep, "mis-biased and gnarly" at the top.
  SATURATE and MODEL are interactive: models mold and filter the distortion.
- **WOW**: slow, smooth, random pitch modulation — "off-kilter pitch bends,
  rubbery." Sweet spot ~10:00; high settings = struggling machinery/melted tape.
- **FLUTTER**: fast, twitchy, random modulation of **both amplitude and pitch** —
  "quivering, trembling tape." More texture than modulation; the secret
  ingredient that reads as tape rather than vibrato.
- **MODEL**: 12 EQ profiles frequency-sampled from physical hardware: CPR-3300
  VHS gens 1/2/3 (a copy of a copy of a copy), Portamax 4-tracks, camcorder,
  dictaphones, Fishy-60 toy recorder, Walkman, AMU-2 "broken/deteriorated", and
  ¼" reel-to-reel.
- **FAILURE**: multi-effect of small malfunctions — **drops** (brief silences,
  playhead losing contact), **snags** (abrupt pitch spikes, tape briefly stuck),
  **crinkles & pops** (micro-disturbances: an audible rustle *and* a crumbling
  AM effect on the signal). Failure manifesting differently L/R is how the pedal
  makes its moving stereo image (SPREAD).
- **NOISE**: hiss + mechanical/motor noise, independently mixable; audible even
  with no input.
- **AUX: STOP**: tape-stop that "slows your wet signal to a rubbery halt,"
  onset time adjustable. Binary footswitch only — our expression transport is a
  continuous generalization of this.
- **Ramping/bounce**: any knob internally modulated by triangle or **random**
  motion ("tape hopper" = roving MODEL, "maintenance phase" = roving FAILURE).
  Precedent for our generative drift layer.

---

## 3. Control surface (user decisions)

| Control | Name | Function |
|---|---|---|
| CC72 toggle (MENU 1 s4, toggle 21) | — | TAPE on/off (`$FX_TOGGLE(gkTapeBypass'21)`) |
| Slot encoder (logical rot CC22) | **AMOUNT** | macro: saturation drive + generative spikes/warps + drift, 0..1 |
| CC20 | **FAIL** | failure density: drops + crinkles/pops, 0..1 (the old dropout-amount home) |
| CC23 | **MODEL** | morphable voicing, 0 = reel-to-reel → 0.5 = cassette/Walkman → 1 = broken VHS/dictaphone |
| CC29 expression | **TRANSPORT** | performance tape slowdown/speedup gesture (§5) |

- **100% wet** when active (hard assignment in the chain, like DROPOUT was).
  No mix knob; it's a channel vibe, not a parallel effect.
- Toggle-on defaults ("nice and wistful"): AMOUNT 0.4, FAIL 0.25, MODEL 0.35.
  Expression rest = full speed.
- Voicing is a continuous **morph**, not stepped models. User: "morphable…
  may change w/ other profiles later" — anchor profiles must be easy to retune.

---

## 4. Architecture: inline single-transport engine

**Inline opcode pattern** (like the DRIVE pedals, NOT a bus instrument): a
`TapeMachine` UDO defined in `tape.orc`, called conditionally inside instr 1 at
DROPOUT's old chain position. The bus pattern's one-ksmps-block latency
(128/48k = 2.67 ms) is exactly what the user can hear and rejected for drives;
same rule applies here. instr 500 and the `gaDrop*` buses go away.

**One transport.** A ~4 s circular stereo buffer is the tape: saturation writes
into it; a single varispeed interpolated read head plays it back. Wow, flutter,
generative snags, and the expression transport are all *speed modulations of the
same read head*, so they compound like a physical machine (a snag during a pedal
dive deepens the dive).

```
in ─► input trim
   ─► SATURATE  pre-emphasis (HF shelf up) ─► hysteresis nonlinearity ─►
                de-emphasis (mirror shelf down).
                Hysteresis: feedback-biased anti-aliased tanh —
                bias = lowpassed previous output, so the operating point
                depends on signal history (program-dependent asymmetry).
                Reuses the shared OS4Tanh UDO (globals.orc). Drive ← AMOUNT.
   ─► TRANSPORT write head always advances; read head speed =
                kPedal × kWow × kFlutter × kSnag  (§5, §6).
                Flutter also applies its small AM twitch post-read.
   ─► MODEL     voicing filter, coefficients interpolated across 3 anchors:
                  0.0  reel-to-reel: full bandwidth, hint of head bump, flat
                  0.5  cassette: 60–100 Hz head bump, ~8 kHz rolloff, slight
                       mid presence
                  1.0  broken VHS/dictaphone: ~150 Hz–3.5 kHz band, resonant
                       peak, unstable character
                Implementation: a small fixed topology (low shelf + peak +
                resonant lowpass + highpass) whose parameters are tabled per
                anchor and interpolated — retuning anchors later = editing a
                table, per user request.
   ─► FAILURE   drops: stochastic gain dips with shaped envelopes (fast fall,
                lumpy recovery) — the evolved dropout.orc. Crinkles & pops:
                dust/crackle noise that is both audible AND briefly
                amplitude-crumbles the signal. All events Poisson-triggered,
                density ← FAIL, decorrelated L/R (the SPREAD stereo trick).
   ─► NOISE     hiss level/color follows MODEL (more + darker toward broken
                end) and scales with FAIL so it can disappear; faint motor
                flavor only near MODEL = 1.
   ─► dcblock2 + limit ±0.99
```

House style: header comment with chain position / reads / writes / controls,
`port`-smoothed k-controls — **except** any control feeding the transport's
quantize/crossfade-like decisions (Habit lesson: never portk a control that
feeds a quantize-then-crossfade stage).

---

## 5. Transport & the latency story

- **Rest state**: read head trails write head by the wow/flutter headroom —
  that is the *entire* added latency (no bus block). **Amended 2026-06-12
  (implemented + bench-verified): the headroom is adaptive, 32 samples
  (0.67 ms) at AMOUNT=0 scaling to ~152 samples (3.2 ms) at AMOUNT=1**, since
  headroom is only needed in proportion to the wobble dialed in. Base slews
  over ~4 s (sub-cent); a weak spring (max −20 cents, under the wow's own
  motion) keeps the gap from random-walking into the floor. Measured total
  rest group delay at clean settings: ~60 samples = 1.25 ms.
- **Pedal toe-ward**: target speed eases from 1.0 toward 0 with a rubbery slew
  (the Gen Loss STOP "rubbery halt" feel). Depth tracks pedal position —
  partial press = partial sag/wobble, full press = dive to a complete stop
  (silence, with hiss/noise still breathing).
- **Pedal release**: speed eases *above* 1.0 and winds up until the read head
  catches the write head — this IS the tape-start/speedup sound, and it heals
  the accumulated gap back to the 2–3 ms baseline automatically. Catch-up is
  gap-proportional and completes in ~0.5–1.5 s no matter how long the stop was
  held (longer hold ⇒ faster, higher wind-up — like a real machine spinning up
  under load). If the gap exceeds the 4 s buffer during a long hold, the stale
  tail is abandoned and wind-up starts from the oldest valid audio (inaudible
  in practice: speed there is ~0).
- **At full speed untouched**: timing is bit-faithful; the only pitch motion is
  the wow/flutter/snags you dialed in.
- Read interpolation must be high quality (cubic/4-point minimum) — varispeed
  through zero exposes cheap interpolation as zipper/aliasing.

---

## 6. AMOUNT macro & the generative system

One knob, three layers, all 0 at AMOUNT = 0 (stable, clean-glue tape):

1. **Saturation drive** — felt-not-heard warmth by ~0.4, moderately aggressive
   compression/grit by ~0.7, mis-biased gnarl at 1.0.
2. **Spikes/warps** — Poisson-triggered transport excursions: brief pitch snags
   up *and* down plus slower "stretch" glides. Rate ∝ AMOUNT, depth ∝ AMOUNT²
   (the top of the knob gets dramatic fast).
3. **Drift** — three independent ultra-slow random walks (~0.005–0.05 Hz,
   `jspline`-class) roving **wow depth**, **MODEL morph**, and **failure
   density** *around* the user's set values (orbit, never replace). Range
   ∝ AMOUNT². This is Gen Loss's RANDOM bounce ramping ("tape hopper" /
   "maintenance phase") as a zero-extra-controls macro.

Baseline wow/flutter (always on while active, pre-drift): wow ≈ ±0.2 % at
0.5–2 Hz, flutter ≈ ±0.05–0.1 % pitch + ~0.5 dB AM at 4–12 Hz, both scaled up
modestly by AMOUNT. Sweet-spot tuning per manual: wow noticeable-but-musical,
flutter "turn it down to just where you stop noticing it."

**Volume law**: unity (±0 dB target, drive-pedal tooling) at any AMOUNT,
enforced by a bench-fitted gain-compensation curve; recalibrate with the
/tmp bench pattern whenever saturation DSP or drive curves change.

---

## 7. Wiring changes (full DROPOUT replacement)

1. `csound/effects/dropout.orc` **deleted**; new `csound/effects/tape.orc` holds
   the `TapeMachine` UDO (+ helper UDOs). No instr number.
2. `globals.orc`: `gkDropout*` → removed; add `gkTapeBypass init 1` (toggle 21),
   `gkTapeAmount`, `gkTapeFail`, `gkTapeModel`, `gkTapeExpr`, step sizes,
   tape buffer ftables. `gaDrop*` bus inits removed.
3. `signal_chain.orc`: DROPOUT block (pos 12) replaced by conditional inline
   call: `if gkTapeBypass < 0.5 then aL, aR TapeMachine aPrevL, aPrevR ...`
   (hard assignment, 100% wet). `$FX_BYPASS` line removed.
4. `midi_handler.orc`: CC72/slot-4 toggle branch → TAPE printks; slot rotation
   → AMOUNT via `$ENC_ROT`; CC20 branch (gkLastToggle == 21) → FAIL; CC23 →
   MODEL; CC29 (gkLastToggle29 == 21) → TRANSPORT. Old depth/smoothing branches
   removed. OSC feedback: `/tape_state`, `/tape_amt`, `/tape_fail`,
   `/tape_model` (old `/drp_*` lines removed).
5. `neptrPhase4.csd`: include swapped to `effects/tape.orc`.
6. UI (`neptrPhase4_UI.py`, `neptrPhase4_UI_pi.py`): MENU 1 s4 label DROPOUT →
   TAPE; param displays mirror the new OSC addresses.
7. Memory/checklist note: toggle 21 now = TAPE.

---

## 8. Testing

Bench harness `scripts/bench/tape_bench.csd` + `analyze_tape.py` (Habit
pattern; run locally, not by subagents — sandbox can't exec csound). Numeric
scenarios:

1. **Volume law**: RMS in vs out across AMOUNT ∈ {0, .25, .5, .75, 1} with
   MODEL 0 / FAIL 0 → within ±0.5 dB of unity (MODEL ≠ 0 is allowed to color).
2. **Rest latency**: impulse through engaged effect → group delay ≤ 3 ms.
3. **Transport continuity**: scripted CC29 dive-hold-release → no sample
   discontinuities (max inter-sample step bounded), monotonic pitch fall on
   dive, wind-up completes ≤ 1.5 s, gap returns to baseline.
4. **Generative statistics**: spike event rate and drift excursion ranges at
   AMOUNT {0, 0.5, 1} match design (0 ⇒ literally zero events/drift).
5. **CPU**: headroom on the Pi alongside the existing chain.

Listening pass on hardware after numerics: expression feel (rubbery, no zipper),
flutter AM authenticity, failure stereo movement, hiss taste.

Bench gotchas inherited from Habit: driver instrs numbered below the effect
must not zero send buses (moot for inline, but the harness still drives instr 1);
no portk into quantize/crossfade decisions (tap flapping = pitch smear).

---

## 9. Future extension (logged, not built now)

**Deep granular layer** feeding off the same 4 s tape buffer: grains sampled
from the transport's past as a separate, combinable element ("granular from a
deep level w/ this process"). The buffer + read-head architecture is the reason
the buffer is 4 s rather than the minimum: a future granular engine can read the
same tape without rework. Would also enable *sustained* pitch-up states the
varispeed head can't physically do (it would collide with the write head).
