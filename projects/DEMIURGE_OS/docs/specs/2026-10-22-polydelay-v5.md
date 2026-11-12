# POLY DLY v5 — clean clock-locked pitch-cascade delay (2026-10-22)

Replaces the v3/v4 16-mode POLY DELAY engine (movedelay.orc + movedelay_modes.orc,
instr 191, toggle 69, DELAY menu slot 2, UI code `mvd`). EMGOR's spec, verbatim
intent: "maintain the interpolated delay vibe (Doppler glide when time moves) but
define exactly how many equal-volume delays as a number, not feedback; two numbers
make an a/b delay; no mix — 100 % dry + 100 % wet, super clean, no filtering; every
repeat pitch-shifts another interval, highest quality; attack/decay pitch envelope
relative to delay time; time locked to clock subdivisions; two delays hard L and R
with different subdivisions but shared attack/decay." Max tempo 300 BPM is fine.

## Signal
- Input: mono sum of gaMvdInL/R (guitar). Dry passes 100 % on both channels.
- Two independent tap stacks: **L stack panned hard left, R stack hard right**.
- Repeat k (k = 1..Repeats) of a stack plays at `k * T` where
  `T = Num/Den * beat`, `beat = 60 / gkChaseBlissBPM` (limit 40..300; NO 120
  fallback — gkChaseBlissBPM is valid in both loop and no-loop states).
- Every repeat is **unity gain**. No normalisation (user's choice, flagged).
- Repeat k is pitch-shifted by `k * Interval * depth * env(t)` semitones
  (cumulative). `depth = 1 - gkMvdExpr` (EXPR heel/0 = full interval; toe pulls
  the shift toward unison).
- Pitch envelope, shared by every repeat and both stacks, defined per stack
  window of length T, phase-locked to the clock: attack ramps 0→1 over
  `Attack * T` from window start; decay ramps 1→0 over `Decay * T` ending at
  window end; hold at 1 between. If Attack + Decay > 1 the ramps meet at a
  peak (both scaled by 1/(A+D)). Attack = Decay = 0 → static harmonised repeats.
  Phase source: the master loop playhead if a position global exists (the
  engine echoes /lp*_pos — find its source); when no loop, a phasor at 1/T.
- Doppler: T changes glide through `portk` with a 0.2 s half-time so moving a
  Num/Den encoder bends the repeats (the "interpolated delay vibe"). deltap3
  reads, as before.
- Repeat count changes: each tap has a gain mask portk'd over 10 ms (no click).
- Delay lines: 16 s per stack. A tap whose k*T exceeds the line is muted, not
  clamped (clamping would break the rhythm).

## Pitch shifter (v5.1: Csound `mincer`, one per tap, max 2 x 8 = 16)
**v5.1 (2026-10-22, later the same night).** The v5 shifter (`PdlyShift`:
pvsanal/pvscale/pvsynth) measured NOT equal-volume: -2.3 dB at +14 semitones,
-14.5 dB at +21, about -11 dB at +28, plus frame-rate sidebands, because pvscale
re-bins. v5.1 replaces it with `mincer` (phase-locked, resampling phase vocoder
reading a ring table). `PdlyShift`, the pvs chain, the `delay aSig, LAT/sr` path
and `PDLY_HOP/GAIN/LAT` are removed. Everything else (clock, Num/Den/Reps/Intv/
Att/Dec/Expr, envelope, filter, bypass, 16 s lines, gain mask, I/O) is unchanged.

- Per tap (K = 1..8, stack L/R): a 32768-sample ring table `giPdlyRing<S><K>`
  written every cycle from a pre-read `deltap3`; shared write index
  `aIdx = upsamp(kIdx0) + phasor-ramp*ksmps`, `kIdx0` advanced at the end of the
  instrument (ring length is a multiple of ksmps and of the hop). `mincer` reads
  the ring with pointer `ptr = W - LAT - N*(pitch-1)/2` (the output at pointer p
  is centred on source sample p + N*(pitch-1)/2); the window end needs
  `LAT >= N*(pitch+1)/2`, so `LAT = min(FFT*(Pmax+1)/2, K*T*sr - MARGIN)`,
  Pmax = min(2^K, 8) (3072, 5120, 9216 samples at taps 1, 2, 3+).
- Macros: `PDLY_FFT` 2048, `PDLY_DECIM` **4** (hop 512), `PDLY_RING` 32768,
  `PDLY_MARGIN` 2*ksmps + hop (640 at decim 4), `PDLY_MAXDN` -36, `PDLY_MAXUP` +36, `PDLY_MAXREPS` 8.
- **Hop residual:** mincer truncates its read start to the hop, so content is
  late by `r = ptr mod hop`. Frame instants sit on ring indices = 0 mod hop, so
  r is a constant for a static pitch and is absorbed in the pre-read delay
  (computed from the static peak at depth 1, so it does not wobble with the pedal
  or the envelope). Activation and run are latched only on hop-aligned cycles
  so mincer starts/pauses on frame boundaries.
- **Anti-alias:** up-shifts go through 4x `butterlp` at `0.9*sr/(2*P)` before the
  ring (without it a +19 shift of a sawtooth had off-harmonic energy only 17.6 dB
  below the harmonics at 6-12 kHz; with it 49.7 dB).
- **Caps:** per-tap up-shift cap from available time `12*log2(2*LAT/N - 1)`
  (1/8 and slower are uncapped to +12 per repeat at every tempo <= 300; a 1/16
  repeat 1 is uncapped to about 185 BPM, +11 at 200 BPM, unshifted at the T
  floor; repeats 2+ uncapped); shift clamped to [-36, +36]. -36 is below the
  FFT's resolution at 220 Hz (purity 5 dB), which is why MAXDN is -36.
- **T floor:** `T >= (FFT + MARGIN)/sr` = 56 ms at decim 4 (1/16 exact up to
  268 BPM; above that T clamps to the floor, never mutes), 50.7 ms at decim 8.
  The hop inside MARGIN keeps the pre-read above one k-cycle after the hop
  residual correction, so timing is exact at every T above the floor.
- Small-interval blend (`|semi| < 1`) crossfades to the exact tap; no transient
  sidechain; shifter k only runs when `k <= Repeats` and the static shift is
  non-zero; 16 calls unrolled via `PDLY_TAP`.
- **Measured (offline, Csound 6.18):** unity level at +7 +12 +14 +21 +24 +28
  +36 -12 -24 (0.00 dB); pitch error < 0.01 %; pure-tone spurs below -58 dBc.
  Repo bench (decim 4, 1 s burst): plateau level 0.00 dB at +7/+14/+21/+28,
  pitch error <= 0.02 %.
- **Timing:** exact to the sample while pitch sits at its static peak; up to one
  hop late (10.7 ms at decim 4, 5.3 ms at decim 8) while the envelope is between
  0 and peak or the EXPR pedal is partially pressed (inherent to the
  hop-quantised pointer; the exact tap and the fully pressed pedal are exact).
  The pitch envelope is re-read once per hop. Phase-vocoder pre-echo grows with
  ratio: the 10 %-of-peak onset leads the grid by about 9 ms at +7, 15 ms at +12,
  43 ms at +24 while the energy centroid of a 0.2 s burst stays within +-2.6 ms
  (inherent: the analysis window spans N*(pitch+1)/2 either side).
- **CPU / decim:** decim 4 shipped for headroom (the rig engine already sits at
  60-88 % of the audio period and the HAT wedges on a late cycle). Isolated
  prototype, 16 shifters, 6 s on a Pi 4: decim 4 = 2.4 s, decim 8 = 4.4 s (v5:
  6.1 s); purity equal on pure tones. Full bench render: decim 4 = 3.6 s,
  decim 8 = 5.4 s, no shifters = 1.4 s. `PDLY_DECIM 8` is the quality setting
  once the rig shows headroom; `PDLY_MAXREPS` (default 8) clamps Repeats as a
  CPU knob for a tight rig.

## Hidden filter (defaults = bypassed, not in UI, not persisted)
- `gkMvdFiltDepth init 0`, `gkMvdFiltQ init 2`, `gkMvdFiltBase init 400` (Hz).
  When depth > 0: per stack output, resonant 2-pole LP (`zdf_2pole` or
  `statevar`), cutoff = base * 2^(depth*6*env) riding the SAME pitch envelope.
  Depth 0 skips the filter entirely.

## Controls — toggle 69, 6 encoders + 2 touch-only absolute CCs + EXPR
| param | global | range/step | encoder CC (relative, $ENC_ROT) | OSC echo |
|---|---|---|---|---|
| bypass | gkMvdBypass | 0/1 | toggle 69 | /mvd_state |
| Num L | gkMvdNumL | 1..16 int | CC20 (P2) | /mvd_numl |
| Den L | gkMvdDenL | 1..16 int | CC23 (P3) | /mvd_denl |
| Num R | gkMvdNumR | 1..16 int | CC24 (slot0) | /mvd_numr |
| Den R | gkMvdDenR | 1..16 int | CC26 (slot2) | /mvd_denr |
| Attack | gkMvdAtt | 0..1, step 0.02 | CC21 (slot3) | /mvd_att |
| Decay | gkMvdDec | 0..1, step 0.02 | CC22 (slot4) | /mvd_dec |
| Repeats | gkMvdReps | 1..8 int | **ch1 CC110 absolute**: reps = 1 + round(val/127*7) | /mvd_reps |
| Interval | gkMvdIntv | −12..+12 semi int | **ch1 CC111 absolute**: semi = round(val/127*24) − 12 | /mvd_intv |
| expr | gkMvdExpr | 0..1 | CC25 (generated, unchanged) | — |
CC25 stays the EXPR row (gen/expr_rot_25.orc) — do not reassign.
Defaults: NumL 1 DenL 2, NumR 1 DenR 3, Attack 0, Decay 0, Repeats 4,
Interval +7 (so the thing audibly works the first time it is switched on).

## State table (giNeptrState) — reuse the 9 old mvd slots
145 bypass · 146 NumL · 147 DenL · 148 NumR · 149 DenR · 150 Interval ·
151 Attack · 307 Decay · 308 Repeats. Unpack MUST clamp every value to its
range (old presets hold v4 values in these slots: e.g. 146 was mix 0..1).

## Removed
movedelay_modes.orc (and its #include in neptrPhase4.csd), MvdGrainPS, the 16
modes, presets, decay modes, /mvd_mix /mvd_div /mvd_regen /mvd_moda /mvd_modb
/mvd_mode /mvd_preset /mvd_decay /mvd_topn /mvd_botn, UI MVD_DIVS / MVD_MODES /
MVD_POLY_PRESETS / MVD_DECAY_* tables, the preset picker modal and decay chips.

## UI (neptrPhase4_UI.py, `mvd`)
- Sliders (6, left→right): NUM L, DEN L, NUM R, DEN R, ATTACK, DECAY. Num/Den
  display as integers. Attack/Decay 0..1.
- Top strip (where preset/decay chips were): a REPEATS row of 8 chips (1..8,
  selected lit, tap → CC110 absolute) and an INTERVAL row `[−]  +7  [+]`
  (tap → CC111 absolute; display signed semitones; 0 shows "0"). Highlights
  follow /mvd_reps and /mvd_intv echoes only — never a local write (house rule).
- Live readout line under the strip: `L 1/2   R 1/3   T 250 / 167 ms` — ms
  computed UI-side from the master BPM the UI already knows (if it does; else omit ms).

## Chain
signal_chain.orc 20.2 unchanged in shape: gaMvdWetL/R already carries dry+wet.
