# Resampler-up / re-anchored-down shifter (host findings, 2026-10-01)

Host-only. Nothing deployed, nothing committed. Baseline = lock-in commit e4b7844
(rebuilt in /tmp with the same stub fixes). Files touched: `shifter.cpp`,
`dsp_core.cpp` (AUTO EQ tilt now after `lpcProcess`; host-only `HG_OPCOUNT`
counters), `hyperguitar.h` (Shifter struct only), `hosttest/stub/Bela.h`,
`hosttest/bench/{stub/Bela.h,stub/hg_misc_stub.cpp,Makefile,bench_main.cpp}`.

## What changed
1. **One resampler, both directions** (`resRun`, replaces `soladRun` + the up
   grain leg). Up: delay falls (rho-1)/sample, at floor `sdLo = 5 + (rho-1)*L`
   jumps back `n <= N` whole periods, offset refined +-4 by the shared-read
   correlation search (`corrRefine`), equal-power smoothstep crossfade of
   `L = SUP_XF*T/rho` (SUP_XF=1.0). `N = clamp(round(350/T),1,4)`. Reads `abuf`;
   AA cutoff `0.45*sr/max(rho,phi)`. Never jumps before the onset stamp (takes
   fewer periods or holds at the floor); hand-off from TRANS re-seats the read
   inside the note. `mkIn` picker and the up `GrainLeg` are gone (no per-sample
   biquad). Formant leg untouched.
2. **Down attack**: pitch down re-anchors the read to delay 4 through a smoothstep
   overlay (`sdOnsLive`, a THIRD leg so a half-finished splice blend keeps
   running; carrying one leg forward gave a 716x click). Length
   `clamp(0.20*T, 32, 80)` samples. Chain stays live, no dry-tap detour. Until
   the tracker locks the note the delay may run 1.5T past the ceiling; if it
   still has not locked, the splice is by correlation on periodGood/p0.
3. **Modes**: RUN and TRANS only. TRANS is entered at an onset for pitch UP or a
   live formant leg (down alone never). Exit: 3 relock hops, or formant-only
   fxAlive, or 25 hops. No noLockHops/dwell/flap logic. Formant gate kept
   (`fxGate`, 6 fresh/8 stale hops, only when pitch idle + formant leg on).
   `lpcAnalyze` every hop when `gFormantActive && gFormantMode==1 && dbgGate!=2`.
4. Tap leg is now a fixed 4-sample dry read (no teleports, no tap fade), and the
   tap/chain blend is equal-power on a smoothstep phase (plain sqrt had infinite
   slope at the pick; TRANS entry fade 32 samples). Idle resampler no longer
   does a dummy read.
5. Removed the bit-exactness guard in the correlation search: the up leg's base
   is nearly constant per string, so a string with base in 508..512 took the
   2700-read slow path on every splice (worst slot 2707 bufAtM every ~750 samples).

## Beading (pitchbench steady, PB_ORACLE=1, 25c flat, partial-mod %, s0..s5)
| shift | before | after |
|---|---|---|
| +12 | 8.5 9.0 16.1 17.2 30.3 24.7 | 1.8 2.6 1.3 2.9 3.5 7.1 |
| +7 | 4.4 5.0 6.4 8.2 25.4 20.7 | 2.8 3.1 2.5 3.3 3.9 6.7 |
| -12 | 14.9 7.1 5.4 4.5 5.5 5.9 | 15.0 7.1 5.8 4.2 5.5 6.3 |
| formant 1.3 | 2.6 2.7 1.3 2.2 6.3 4.0 | identical |
Dry reference 2.3 3.0 1.5 2.9 3.1 5.2. Tracker period (no oracle) gives the same
picture (+12 s4 30.3 -> 3.5, s5 24.8 -> 6.4). Crossfade length is capped at `SUP_XF_MAX`=240 samples (low-string latency; uncapped/160 cost nothing measurable in beading). s0 at -12 (15%) is unchanged: 32 Hz
partials are not resolved by the 50 ms frames, a metric artefact on both builds.
`SUP_XF=1.5` takes s5 +12 7.1 -> 5.8 (+7 6.7 -> 5.8); `SUP_XF=2` breaks +12 on
s1-s3 (fade as long as the splice interval at N=1), keep <= 1.5.

## Clicks (disc.py d2 hits L / worst, 7 s, bench scenes; before -> after)
Isolated build (my 3 files on the committed lpc.cpp/header, apples to apples):
| scene | before | after |
|---|---|---|
| 0 off | 6 / 33x | 6 / 33x |
| 1 formant | 45 / 70x | 13 / 44x |
| 2 several fx (-12 + formant, events) | 71 / 107x | 82 / 90x |
| 3 formant LPC | 9 / 35x | 6 / 31x |
| 4 up +7 | 53 / 64x | 17 / 42x |
| 5 up +12 | 52 / 63x | 19 / 45x |
| 6 up12+formant | 52 / 63x | 19 / 45x |
| 7 up7+formant.8 | 55 / 64x | 18 / 42x |
| 20 down -12 (new) | 193 / 233x | 125 / 90x |
| 21 down -5 (new) | 61 / 96x | 48 / 62x |
| 22 -12 + formant (new) | 60 / 99x | 41 / 59x |
| 23 -5 + formant (new) | 42 / 69x | 14 / 36x |
New scenes 20-23 (steady down, no UI events) were added to bench_main.cpp.
Up scenes are now 17-19, formant-only baseline 13: at/near baseline, 3x below before.
Caveat on scene 2/20/22: ~25-70 of those hits are ONE cluster in the first 100 ms
(cold-start pick noise out of digital silence, samples 7800-8300); outside it,
scene 2 is 9 (was 5), scene 20 is 60 (was 166), 21 is 23 (was 32), 22 is 36 (was 33).
The cluster grows with the onset fade length (scene 20: 39 at 32 samples, 65 at 80)
but the fade also cuts the loud re-pick step; 0.20*T/80 max is a listening knob.
In the working tree (with the other agent's WIP lpc.cpp) scene 3 reads 34 and
`run_tests` formant_1.4_lpc FAILS (centroid 979 -> 1072, bar is higher); with the
committed lpc.cpp all 10 tests pass, before and after. That failure is not from
this change; on the final working-tree run (lpc.cpp moved on meanwhile) `run_tests`
is ALL PASS (10).
Device level (HG_BENCH_PICKUP=0.1, NOISE 3e-4): 0 hits, worst <= 8x all scenes.

## Attack probe (scenes 20 = -12, 21 = -5, 42 picks each, 6 strings)
Metric: max |d2| in [onset-4, onset+48] over the local median |d2| (disc.py style);
dry-input control median 12x (the pick itself is >10x on its own).
- onsets where out > max(10x, 1.5*control): before 10 / 9, after 4 / 5 (worst 95x / 89x
  vs control worst 88x; before 385x / 370x). The remaining ones are loud re-picks
  over a ringing bass note (old read +0.27 -> new -0.45, a 0.7-amplitude phase step
  smoothed over 80 samples); the output is smooth, the ratio is large only because the
  local median is tiny.
- first splice after onset: median 750 samples (-12), 1359 (-5); tracker lock median
  665 samples after the pick. First splice before lock: 3 of 42 at -12, 0 of 42 at -5
  (those 3 take the correlation path).
- up hand-off: scene 5, 42 picks, 42 TRANS->RUN, 0 needed the onset clamp re-seat.

## CPU (bufAtM calls per sample, 6 strings, mean / worst slot)
| scene | before | after |
|---|---|---|
| 0 off | 12.0 / 14 | 0.0 / 0 |
| 1 formant | 23.3 / 27 | 11.3 / 15 |
| 4 up +7 | 23.5 / 484 | 10.5 / 901 |
| 5 up +12 | 23.7 / 2725 | 14.2 / 924 |
| 6 up12+formant | 34.9 / 2737 | 25.5 / 936 |
| 20 down -12 | 13.6 / 473 | 7.5 / 467 |
| 21 down -5 | 12.8 / 472 | 6.8 / 10 |
Up leg alone at +12 ~ (14.2-0)/6 = 2.4 reads/string/sample incl. amortised search,
vs a 6-grain stream (2-3 active, 6 capacity) plus its tap splicer. Worst slot is two
strings searching in one sample (~460 reads each); the old up path had 2725.
Host-only op counts; no ARM timing taken.

## Output level
+1.0 to +1.3 dB louder on pure up scenes (4, 5, 7); within 0.2 dB elsewhere.

## Not verified
- Anything audible. Beading/click numbers are synthetic inharmonic material.
- Real-hardware tracker lock timing (bench lock median 665 samples).
- Down + formant still routes the pick through the dry tap (formant leg has no marks
  for ~2 periods); the pick lands unshifted there.
- ARM/Bela CPU; the 2 x ~460-read burst when two strings splice together.
- Up latency, by construction only: mean delay ~ sdLo + N*T/2. 169-sample string at
  +12: ~89..427 (mean ~260 samples, 5.9 ms). Low E (T=683, N=1) at +12: floor 245
  (fade capped at 240), saw 245..928, mean ~590 samples = 13 ms - roughly 2x the old
  grain leg's ~300 on the lowest strings. Lower N*T is not available without
  splicing less than a period; the cap trades fade softness for ~100 samples.
- Rapid beta glides (slot changes) while resampler direction flips through unity.
