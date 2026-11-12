# Stacked pitch+formant crackle — confirmed root causes & fixes (2026-08-23)

All work host-side (hosttest/bench), nothing deployed to the Bela.
Detector: `disc.py` (d2 hits above 25x local scale) on 7 s of realistic
inharmonic material, scenes in `bench_main.cpp`.

## Root causes (all confirmed by probe, not inferred)

1. **Up-shift stale-ring click (pitch up alone).** In DRIFT the tap leg
   drifts `tapDelay += 1-rho`; for rho>1 it runs INTO the write head. With
   `lo = 8` the splice fired with `tapOld ~ 7`, which kept drifting at
   -1/sample during the 0.5*Tref crossfade, crossed zero ~7 samples in, and
   `bufAtM` wrapped past the write head into buffer-old ring data at ~full
   outgoing gain. Probe: 720 low-bound splices -> 245 click clusters, every
   cluster 7-9 samples after its splice (random-position null: thousands).
   Down-shift drifts away from the head — that is why only up crackled.
   *Fix:* rho-aware low bound `lo += (rho-1)*0.55*Tref` + freeze-at-3
   underrun guards for both fade legs; splice crossfade re-shaped to
   equal-power on a smoothstep phase (plain sqrt(t) has infinite slope at
   t=0).

2. **Stale `transAge` on re-pick.** `shifterOnset` never reset `transAge`,
   so a fast re-pick inherited `transAge > 25` from the previous pick and
   fell TRANS->DRIFT one hop later with the tap parked at 4 — instant
   stale-ring click at rho>1 (probe showed tapDelay -3/-7 at the splice).
   *Fix:* `transAge = 0` in `shifterOnset` + tapDelay drift clamp while a
   fade blocks the splice.

3. **The recorded step (stacked scenes' remaining clicks).** The mark
   picker re-acquires within ~2T of an onset, so the up leg is launching
   again DURING TRANS; the TRANS->GRAINS `legReset()` instant-killed its
   mid-window grains, stepping the pitch-stage output `p` to ~0 in one
   sample. `grainMix ~ 0` masked the step at the output, but `p` is written
   into `fxbuf`, and the formant leg read it back dc (~150-650) samples
   later at full gain — a delayed, un-crossfaded click with no event
   anywhere near it. Probe: the biggest step in `p` sits exactly ON the
   `M 1->0` transition and every worst click follows it by exactly that
   string's formant read delay (234/643/355/149 samples on the four worst).
   *Fix:* `legReset` re-anchors launch state only; in-flight grains finish
   their Hann windows (gone within 2S) and overlap-add smoothly with the
   new stream.

## Measurements (d2 hits / worst, L channel, before -> after)

| scene              | before          | after        |
|--------------------|-----------------|--------------|
| 0 all off (floor)  | 5 / 34x         | 5 / 34x      |
| 1 formant only     | 20 / 51x        | 23 / 51x     |
| 2 down12+formant   | 37 / 96x        | 37 / 95x     |
| 3 formant LPC      | 20 / 51x        | 20 / 51x     |
| 4 up +7            | 205 / 169x      | 23 / 51x     |
| 5 up +12           | 467 / 276x      | 28 / 55x     |
| 6 up12 + formant   | 456 / 173x      | 28 / 55x     |
| 7 up7 + formant.8  | 303 / 228x      | 25 / 51x     |

Pitch-up scenes now sit at/below the formant-only baseline the previous
session shipped; down-shift and formant-only paths are unchanged.

## Not done / open

- **stab=1 on the up leg** (the 5.8% fractional-splice roughness): NOT
  applied. Analysis while preparing the A/B: at beta=2 the up leg's
  designed alternating `dj ~ +T` lattice skips are period-aligned
  identities; the stab one-pole would smear each into a ~0.75T
  NON-period-aligned splice — likely strictly worse. The A/B run itself
  was not executed (session interrupted); if revisited, measure scenes
  4/5/6 with the up-leg `legRun(..., stab)` flag flipped and check the
  up-leg dj distribution on bends before judging.
- HG_PROBE / HG_TAP instrumentation (host-only, `#ifdef`-guarded) left in
  `shifter.cpp` for future correlation work; new bench scenes 4-7 left in
  `bench_main.cpp`.
- On-device (Bela) listening verification still pending.
