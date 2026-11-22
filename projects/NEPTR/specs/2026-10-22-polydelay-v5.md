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

## Pitch shifter (UDO `PdlyShift`, one instance per tap, max 2 x 8 = 16)
- Phase vocoder, H90 lineage (effects/h90.orc): FFT `$PDLY_FFT` (default 2048),
  hop FFT/4, latency = FFT samples = 42.7 ms @ 48 k. Down branch: pvsanal →
  pvslock → pvscale → pvsynth. Up branch: 4x butterlp anti-alias tracking
  sr/(2R) − 500 → pvsanal → pvscale (no pvslock) → pvsynth → tanh → pareq
  −3 dB @ 6 k. Branch gating with the ±1 semi hysteresis/warm-up band exactly
  as h90.orc. Small-interval blend to dry for |semi| < 1 as h90.orc. NO
  transient sidechain (repeats are not the dry signal).
- **Latency compensation:** every tap reads at `k*T − FFT/sr`; the shifting
  path adds FFT back; the non-shifting path (Interval*depth == 0 or shifter
  inactive) passes through `delay aSig, FFT/sr` so both paths land exactly on
  the grid. Minimum T is clamped to FFT/sr (never muted).
- CPU gate: shifter k only runs when `k <= Repeats` and the stack's shift is
  non-zero. Unroll the 16 calls (Csound opcode instances cannot live in a
  while loop).

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
