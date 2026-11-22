# LPC formant mode v2 - findings (2026-10-01, host only)

Files touched: `lpc.cpp` (rewritten), `hyperguitar.h` (LPC block only, see "Header"),
this folder (`lpcbench.cpp`, `Makefile`, `sweep.sh`, `lpc_old.cpp.txt` = e4b7844 lpc.cpp frozen,
`results_before.txt`, `results_after.txt`). Nothing deployed, nothing committed.
`hosttest/run_tests`: ALL PASS (10), `lpc.cpp` is `-std=c++11 -Wall -Wextra` clean.

## Verdict

* The warp bug is fixed (whitening target is now E_in(w/rho), recolour target E_in(w/phi)).
* Envelope error vs the true target, steady state, dB RMS over the output partials in 100 Hz-8 kHz,
  mean over rho {0.5,0.75,1,1.5,2} x phi {0.75,1,1.3}: **old 4.37 / 4.33 (string 4, 2 notes), 4.58 / 4.50
  (string 0)  ->  new 2.58 / 2.54 and 2.77 / 2.59**. "Bypass" (do nothing) is 4.32-4.52, so the old code
  was no better than not running LPC at all (that is the bug); the new one removes ~40 %.
* Steady-state gain error: old 0.3-0.4 dB mean (up to 1.6 dB), new 0.00-0.01 dB (slow level trim).
* Attack smear: no measurable smear in either; mean 10-90 % rise delta vs the plain resample
  old +0.30 / -0.24 ms (with a +2.0 ms outlier at rho 1.5, phi 0.75/1.0), new -0.04 / -0.02 ms (worst case
  -0.48 ms, the 1 ms RMS window on a 2 ms rise is the noise floor of this metric). Peak-level wobble
  across the attack (mean |dB| of max|out|/max|y|): 0.33 / 0.35 old, 0.43 / 0.41 new (not a clear change;
  see "Onset" for the cleaner view).
* rho == phi (including 1 == 1): output is **bit-identical to the input** (max |out-y| = 0, tested at
  rho=phi = 1, 1.5, 0.75). The old code was 0.25 / 0.56 / 0.28 of peak off at the same points (start-up
  transient of lattices slewing in from zero).
* Cost went up. See "CPU". The default order/window are a compromise; every knob is a `-D`.

The absolute numbers depend on my synthetic envelope (`-5 dB/oct` tilt + three Lorentzian resonances,
450 Hz BW 200, 1.5 kHz BW 500, 3.3 kHz BW 900, +16/+14/+12 dB, stiff-string stack B=1e-4..5e-4).
Treat them as a ranking of designs, not as what the guitar will measure.

## What changed in lpc.cpp

1. **Warp fix.** Analyse the input ring; evaluate the envelope on a grid; build E_w(w)=E(w/rho) (what the
   shifted signal carries) and E_r(w)=E(w/phi) (what the user wants, phi = `v->formant` relative to the
   ORIGINAL formants). rho = `v->beta` clamped 0.25..4, phi clamped 0.4..2.4. Both warps are
   interpolated in log-power, held flat above the grid, and renormalised to equal total power.
2. **The filter is the ratio R = E_r/E_w, not two refits.** My first cut did "whiten with a refit of E_w,
   recolour with a refit of E_r" (the task's literal design). Measured with a perfect (oracle) envelope
   the two independent all-pole refits cost 1.1-2.0 dB each (an all-pole model matches peaks, not the dips
   between them), and they are an error source that does NOT cancel. R is smooth with no tilt, so two small
   all-pole fits reproduce it: `R ~ (E1/E2) |B|^2/|A1|^2` with A1 = all-pole fit of R, B = all-pole fit of
   1/(R |A1|^2/E1). Runtime structure is unchanged (FIR lattice then IIR lattice, same `lpcK`/`lpcKr`
   arrays), but the FIR lattice is now B and the IIR one 1/A1. Analysis error common to both warps cancels
   in the ratio. No pre-/de-emphasis is needed in the audio path any more (it cancels in R).
3. **Level.** (a) model gain made power-preserving on the realised filter; (b) a slow (~23 ms) in/out power
   trim (`LPC_AGC`) because a sparse line spectrum samples the smooth envelope at few points, and the
   realised level can sit 1-3.5 dB off at pitch +12 (measured -3.5 dB at rho 2 before the trim). The trim
   is measured pre-trim (no loop) and slow enough not to touch attacks.
4. **Exact bypass** when |phi - rho| < 0.2 % of rho (includes rho = phi = 1): `lpcValid = 0`, returns
   before any math, output = input exactly. Free CPU for the default (formant 1.0, pitch off) case.
5. **Trig tables.** `cosT/sinT` serve the envelope AND all fits unconditionally. `HG_LPC_REFIT_TABLE` is
   retired; the ULP note (3 of 140,573 rw[] values one ULP off, tracker bit-identical, -132 dBFS) is kept
   in the header comment. Old: 561 full-argument-reduction `cosf` per hop per string. New: zero trig in
   the fits (only ~650 `expf/logf` per hop for log-domain warping and the R^a mask, plus 25 `expf` for
   the lag window).
6. **Decimate-by-2 analysis (the big win).** 15-tap half-band + decimate before autocorrelation: the model
   covers 0-11 kHz at half rate, held flat above 8.3 kHz. At 44.1 kHz the poles were being spent on
   10-22 kHz partials (python proxy, same material: old p16/W512 2.90 dB fit error at the partials,
   2.46 on the grid; p28/W1024 decimated 1.15 / 1.19; C++ bench steady-state final: 1.3 mean).
7. **Pre-emphasis (analysis only, mu 0.94).** Divided back out on the envelope grid. Measured:
   neutral-to-slightly-negative on my mild -5 dB/oct tilt (mu 0: 2.47 vs 2.57 dB steady), and decisive on
   steep tilts (python proxy, fit error at partials: tilt -9 dB/oct 5.8 dB -> 1.5 dB, tilt -13 dB/oct
   11.7 -> 5.8 dB). A real pickup is steeper than my test, so it stays. It is a safety margin, not a
   measured win on this material.
8. **Order / window.** 28 poles + 1024-sample window on strings 0-1 (512 decimated samples = ~2.7 periods
   of the low C instead of 0.76), 24 poles on strings 2-5. `LPC_ORDER` (array max) is 32 in the header.
9. **Robustness on sparse harmonics.** Lag-window bandwidth now follows the note: max(60 Hz,
   0.25 x f0) capped at 250 Hz, f0 from `sh->Tsm`. Measured on f0 = 440/660/880/1100 Hz (string 5):
   at-partial error 2.65 -> 0.88 dB and whole-grid (between harmonics) 5.47 -> 3.89 dB, no cost on low
   notes. Noise loading is irrelevant in this bench (-30..-60 dB: no change, so the old -30 dB was not
   doing harm or good); -45 dB kept on the analysis fit. A mask pulls R toward 1 where the signal is
   > 40 dB below its peak (see "Gotcha").
10. **Coefficient slew and onset.** Slew 0.002 -> 0.005/sample, applied every 4th sample at 4x rate (same
    time constant, a quarter of the MACs). Onset snap: when `gOnsetStamp[s]` changes the output is slammed
    to dry (no 11 ms of the previous note's lattices on the pick), and after an analysis made from a window
    that is >= 6/8 post-onset (`LPC_HOLD_Q`, 17 ms on W=1024) the lattices are SNAPPED to it and primed by
    running the last 96 input samples through them, then the mix fades in (0.01/sample). The same snap
    happens on every invalid -> valid edge. State is a 128-sample input history in the Shifter.

## Numbers (before = e4b7844 lpc.cpp, after = this lpc.cpp; run `make && make old`)

Steady state, envelope error dB RMS (lower is better), gain = 10log10(P_out/P_y) (ideal 0):

| case | bypass | before | after | gain before | gain after |
|---|---|---|---|---|---|
| string 4, f0 196 | 4.32 | 4.37 | 2.58 | 0.32 | 0.00 |
| string 4, f0 392 | 4.33 | 4.33 | 2.54 | 0.40 | 0.00 |
| string 0, f0 65 | 4.52 | 4.58 | 2.77 | 0.34 | 0.01 |
| string 0, f0 131 | 4.42 | 4.50 | 2.59 | 0.33 | 0.01 |

(rho = phi cells are 0.00 for both: the bench's "bypass" there equals the target; the old code made them
0.0-0.1 by luck of identical lattices, the new one is an exact bypass.) Full per-cell tables in
`results_before.txt` / `results_after.txt`.

Attack (note B after a different-envelope note A, string 4 / string 0, 10-90 % rise of a 1 ms RMS
envelope, output time, mean delta vs plain resample over the 12 non-bypass cells):
old +0.304 / -0.242 ms, new -0.044 / -0.021 ms. |peak dB| old 0.33 / 0.35, new 0.43 / 0.41.
Level 20-30 ms after the onset (mean |dB| vs the plain resample): old 0.40 / 0.42, new 0.23 / 0.57
(the string-0 figure is the worst-case row of rho 2; the 23 ms slow trim is still converging at 20-30 ms
for an octave-up low string. Not a steady-state error.)

Convergence of the new note's spectrum after a re-pick (string 4, env error in 25 ms windows, mean of
0-80 ms over four rho/phi cases): old 4.95, new 4.39; at +250 ms old 4.0-6.4, new 2.6-3.7. For the first
~17 ms the new code is dry by design, so the early windows sit at the bypass level (4.3-5.7).

Envelope-fit fidelity (analysis stage only, true-domain grid vs E_in, 6 strings x 2 notes):
at partials 1.3 dB, grid 1.3 dB. Sparse high notes (440-1100 Hz): 0.98 / 3.75.

Identity: rho = phi = 1 / 1.5 / 0.75: new 0 (bit-identical), old 2.5e-1 / 5.6e-1 / 2.8e-1 of peak.
Invalid analyses (Levinson |k| > 0.998) while loud, non-bypass cases: 0 of 29,180 hops.

### Tuning sweeps (measured during development on intermediate builds; same ranking, absolute values
### shifted slightly by later changes - re-run `./sweep.sh "-D..."` for the final code)

* Order, string-4 steady error (two notes) before the BW/mask/trim work: 24/20 3.15 / 3.25, 28/24
  2.76 / 2.66, 32/28 2.57 / 2.47, 32/32 2.29 / 2.10, 40/40 1.96 / 1.89. Monotone: the all-pole ratio fit
  is order-limited at the low-frequency resonance. Shipped 28/24 (final build: 2.58 / 2.54).
* Window: with decimation 512 vs 1024 vs 2048 input samples at p20: 1.49 / 1.39 / 1.36 dB fit error
  (python proxy). 1024 on all strings; 2048 not worth it.
* Pre-emphasis mu 0 / 0.9 / 0.94 / 0.97: steady 2.47 / 2.57 / 2.57 / 2.57 (mild tilt); see item 7.
* Grid points 65 / 97 / 129 / 257: steady 2.82 / 2.64 / 2.58 / 2.54. 129 shipped.
* HF hold index 40 / 44 / 48 / 54 (of 128): 2.29 / 2.33 / 2.38 / 2.52 (lower is slightly better, the
  decimator droop poisons the top of the model); 48 shipped (8.3 kHz, above the 8 kHz band you care about).
* Slew 0.001 / 0.002 / 0.005 / 0.02 / 0.1: steady identical, convergence 4.45 / 4.31 / 4.19 / 4.10 / 4.08,
  peak wobble flat until 0.1. 0.005 shipped; faster only buys a little and risks zipper.
* Hold after onset (fraction of window): 0 / 2/8 / 4/8 / 8/8: peak wobble 0.41 / 0.49 / 0.54 / 0.19 dB,
  and without the snap at all 1.07 dB. Shipped 6/8. Honest caveat: the rise-time metric cannot separate
  these (all within +-0.1 ms); only the level/peak wobble moves.
* Noise loading -30/-40/-50/-60 dB: no change (analysis and ratio fits). Lag-window BW fixed 20..250 Hz:
  low notes best at <= 60 Hz, high notes best at >= 150 Hz, hence the f0 rule.
* Decimation off (`-DLPC_DEC=1`): fit 2.06 vs 1.31, steady 2.69/2.93 vs 2.57/2.51, attack rise delta
  +1.3 ms (noise), 2x the autocorrelation MACs.

### Gotcha found by the existing test

`formant_1.4_lpc` (a saw band-limited to 3.3 kHz) failed at centroid +9.5 % (needs > 12 %) with
pre-emphasis on: the empty band above 3.3 kHz made R = E(w/1.4)/E(w) a +50 dB hump there, eating the
fit's poles. The fix is the R^a mask (a = Pw/(Pw + 1e-4 Pmax), i.e. pull R to 1 where the signal is >
40 dB down); the test now reads 979 -> 1147 Hz and the bench improved too (steady 2.44 -> 2.36 at -40).
`-DLPC_MASK_DB=-60` brings the failure back, so keep it.

## CPU

Analytic (multiply-accumulates, per string per hop; hop = 128 samples, six strings staggered):

| part | old (16/512/33) | new strings 0-1 (28/1024/129) | new strings 2-5 (24) |
|---|---|---|---|
| pre-emph copy + half-band FIR | 0 | 1.0k + 4.1k | 1.0k + 4.1k |
| autocorrelation | 8.6k | 14.4k | 12.5k |
| envelope eval (cos/sin tables) | 1.1k | 7.2k | 6.2k |
| fits (R, 1/R2) + Levinson x3 | 0.56k + 0.6k | 9.0k | 7.6k |
| |A1|^2 eval, gain pass | 0 | 14.4k | 12.4k |
| **total** | **~11k + 561 inline cosf** | **~51k + ~675 expf/logf** | **~45k + ~675 expf/logf** |

Per sample, all six strings in LPC: old ~1.1k MAC (hop part 0.5k + lattice 0.6k) plus 26 big-argument
cosf; new ~2.9k MAC (hop part 2.2k + lattice 0.7k). About 2.6x the old average, but the old hop
spike also carried the 561 cosf. Per-sample lattice per string: old 16 sections, ~100 MAC; new 28/24
sections, 135 / 116 MAC (the slew is every 4th sample, the trim is ~6 MAC).

Host measurement (Apple M4 Max, -O2, only a ratio is meaningful): analyse 5.0 us/hop old -> 16.3 us
(string 0) / 13.7 us (string 4); process 19.2 ns/sample old -> 45.2 / 32.6 ns. So ~3x per hop, ~2x per
sample. On the Cortex-A53 this is unmeasured.

Cheaper settings if the device balks (all `-D` in lpc.cpp, no code change): `-DLPC_NG=65` (about -35 %
hop cost, +0.25 dB steady error), `-DLPC_ORD_LO=24 -DLPC_ORD_HI=20` (3.15 vs 2.58 dB), `-DLPC_WIN_LO=512`.

## Header (hyperguitar.h) - minimal, LPC block only

* `LPC_ORDER` 16 -> 32 and `LPC_WIN` 512 -> 1024, now `#ifndef`-guarded (they are array maxima; lpc.cpp
  picks 28/24 and 1024). `LPC_EPTS` is untouched and unused by the new code (the new grid is `LPC_NG` in
  lpc.cpp, 129).
* Appended after `lpcValid`, under a `LPC v2` comment: `lpcOnsetSeen, lpcFreshStamp, lpcSnap, lpcHistPos,
  lpcHist[128], lpcPin, lpcPout, lpcCorr`. Nothing reordered. Shifter grows by ~0.7 KB per voice (the
  lattice arrays now hold 32 instead of 16).

## What the integration must provide (shifter.cpp/dsp_core.cpp are the other agent's)

* `lpcAnalyze(v)` every hop on every non-silent string whenever LPC is active, NOT only in
  `SH_MODE_GRAINS` (the onset snap depends on analyses continuing through the attack; it returns early
  and cheaply on silence and on the rho~=phi bypass).
* `v->beta` must be the glided pitch ratio the pitch stage is actually realising and `v->formant` the
  glided user formant, both read at hop time. If the pitch stage lags `beta` (e.g. a SOLAD crossfade),
  the whitening target is momentarily wrong; the ratio filter degrades gracefully (it just under/over
  shifts), it does not blow up.
* `sh->Tsm` should be a valid tracker period in samples (> 8) for the f0-dependent lag window; if it is
  0 the code falls back to the 60 Hz floor.
* `gOnsetStamp[s]`/`gSampleStamp` as today. The tilt EQ after LPC (other agent) does not interact:
  LPC no longer applies its own pre/de-emphasis in the audio path.
* Presets: formant 1.0 with pitch 0 is now a free exact bypass; formant 1.0 with pitch -12 is the
  formant-corrected shift; formant = beta is the plain resample (also free).

## Not verified / limits

* Everything above is host-only on synthetic additive stiff-string stacks and an offline sinc
  resampler. Real pickups, real SOLAD/resampler artefacts, noise, bleed from neighbouring strings: untested.
* CPU on the Cortex-A53 / Bela: not measured. The hop spike is ~3x the old one in MACs; if the build is
  not vectorising the autocorrelation/FIR loops it could matter at block 64. Check with the existing
  `bench`/`pitchbench` style timing on the device before relying on it.
* The shipped orders (28/24) leave ~2.6 dB of steady error in my bench because an all-pole lattice at
  these orders cannot follow a narrow (Q ~ 2) resonance at 450 Hz when it moves to 585 Hz. Higher order
  keeps helping (40/40 -> 1.9 dB). A frequency-warped (Laguerre) lattice would spend its poles where
  the formants are (low, and log-spaced) and is the real next step; not attempted (delay-free loops in
  the warped IIR lattice).
* Attack smear: the 10-90 % rise metric is quantised/noisy at the +-0.1-0.5 ms level, so "no smear" means
  "none above that"; the 6/8-window dry hold means the first ~17 ms of a pick comes out with the plain
  resample's formants (by design, it is the clean transient) - that is a timbral choice you should hear
  on the rig. `LPC_HOLD_Q` (0..8) is the dial.
* The slow level trim means a sudden formant change keeps the old level for up to ~25 ms of convergence
  (it only ever corrects what the model got wrong, and is bounded to x0.4..2.5).
* `lpc_stability` (noise) passes; no sustained-instability case was found, but there is no on-device
  long-run soak.
* Beyond the listed `-D`s, `LPC_BENCH` in lpc.cpp exposes debug hooks (envelope dump, oracle, ratio fit
  error) used only by lpcbench; they compile out of the device build.

## Reproduce

```
cd hosttest/lpcbench
make lb_new old                 # ./lb_new, ./lb_old
./lb_new all 4                  # steady, attack, converge, ident, cpu (string 4)
./lb_new fit ; ./lb_new fithi   # envelope-fit fidelity, sparse high notes
./sweep.sh "-DLPC_NG=65"        # one-line summary for any -D override
```
