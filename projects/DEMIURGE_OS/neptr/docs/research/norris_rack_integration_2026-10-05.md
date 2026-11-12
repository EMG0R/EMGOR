# NORRIS spectral rack: integration bench, 2026-10-05

Bench: `scripts/bench/sm_rack_integration.csd` (run from `neptr/`, offline `-o` file, `taskset -c 0-2 nice -n 19`). Real rack, same include order as `neptrPhase4.csd` (params, core, 19 kernels, rack, persist), signal_chain SPECTRAL RACK block copied. Input 10 s: 220 Hz tremolo sine + gated noise + 880 Hz blips. ksmps 64, sr 48000, Csound 6.18, Pi 5. Macros: `MASK` (bit per fx), `FFT`, `MODE` (1 = mixed 512/2048/8192), `OFFAT`, `GAIN`, `DUR`.
CPU = Csound "CPU" at end of performance minus 0.23 s baseline (compile + rack off). Another live `csound` (the running neptr engine) was on the box during the runs, so treat as +-10 %.

## Results
1. Each effect alone, defaults, 2048 and 8192: no NaN/Inf, bounded, group instr (241+idx) starts, every effect toggles on and off through `giSMon` (after off: output bit-exact dry, `gkSMActive`=0, group instr gone). Exceptions: PARTGLD (bug 1), and near-silent at defaults: GLIDEFLT (rms -20 dB at 2048, -35 dB at 8192), EMERGE (rms -21 dB at 2048, near zero at 8192 in the later window).
2. All 19 on at 2048: no NaN, no blow-up. Output decays to digital silence (rms <1e-4): the cascade of lossy effects removes all energy (cumulative stack 0..4 already -30 dB, 0..16 about -75 dB). CPU 18.1 s per 10 s (1.8x real time on one core). Limit +-0.99 holds in every run with input scaled x6 (peak exactly 0.9900 for 17 of 18 working effects).
3. Mixed groups: 11 effects (5@512, 3@2048, 3@8192): 11.6 s/10 s (sum of the singles 14.6, so one analysis/synth per group saves about 20 %); groups 241/243/245 all start. 18 effects (no PARTGLD) at 512 / 2048 / 8192: 19.9 / 17.3 / 17.0 s per 10 s.
4. CPU per effect, s per 10 s audio (1 core = 10 s), baseline removed:

| rank | effect | 2048 | 8192 |
|---|---|---|---|
| 1 | GRANUL | 2.33 | 2.49 |
| 2 | SHIMMER | 2.33 | 2.32 |
| 3 | DRONE | 1.93 | 2.01 |
| 4 | GATEHOLD | 1.90 | 2.02 |
| 5 | PITCH | 1.86 | 1.93 |
| 6 | WEAVE | 1.80 | 1.93 |
| 7 | HARMONIZ | 1.75 | 1.86 |
| 8 | EMERGE | 1.66 | 1.80 |
| 9 | STRETCH | 1.57 | 1.67 |
| 10 | BINSHIFT | 1.36 | 1.53 |
| 11 | BLUR | 1.18 | 1.36 |
| 12 | FREEZE | 1.13 | 1.39 |
| 13 | AVERAGE | 0.99 | 1.26 |
| 14 | PULSING | 0.87 | 1.09 |
| 15 | SHUFFLE | 0.87 | 1.08 |
| 16 | TRACING | 0.83 | 1.03 |
| 17 | GLIDEFLT | 0.82 | 0.95 |
| 18 | FILTBANK | 0.68 | 0.89 |
| - | PARTGLD | 0.06 (dead, bug 1) | 0.24 (dead) |

Observations: cost is almost independent of FFT size (512 vs 8192 within ~15 %), so it is per-k-cycle UDO/array overhead, not FFT work. Floor is about 0.7 s per 10 s per active group. Average (not worst-case) numbers: a 65536-point frame still lands in one k-cycle, so peak-per-period spikes at big FFT are NOT visible here and need a per-k-cycle probe before trusting 128-256 sample periods. Budget: any single effect is 7-25 % of a core; about 4 heavy effects fill one core, all 19 is not real time.

## Bugs (rack level)
1. **PARTGLD kills its whole group.** `csound/spectral/sm_partialglide.orc` line 70 (`elseif kp < iNb - 1 && kAge[kp+1] > 0`) and line 106 (`if kt >= 0 && kt < iNb && kMag[kk]*kGn > kOM[kt]`): Csound `&&` does not short-circuit, so the array read happens with the out-of-range index anyway (kp = iNb-1, kt = iNb). PERF ERROR "Array index 1025 out of range" turns the group instrument off (reproduced at 2048 and 8192, both lines). Line 68 (`kp > 0 && kAge[kp-1]`) has the same hazard at kp = 0. Consequences: PARTGLD itself never works, every other effect at the same FFT size stops with it, `kWant`/`kPrv` in instr 249 still say running so it never restarts, and the chain bus is left stale dry and unlimited (peak 2.47 at x6 input). Fix: nest the ifs, or read through a clamped index, e.g. `kAge[limit(kp+1, 0, iNb-1)]`, `kAge[limit(kp-1, 0, iNb-1)]`, `kOM[limit(kt, 0, iNb-1)]`. Same pattern is safe in sm_pitchshift.orc 83 and sm_core.orc 99 (bounded loops), but worth a grep when adding kernels.
2. Rack hardening (suggestion, sm_rack.orc): a group that dies on a perf error is never restarted and leaves a stale unlimited bus. Instr 249 could compare `active:k(241+kg)` with `kWant` and re-fire, and the bus limiter could sit in instr 1 instead of the group.
3. No bug: the chain never went NaN in any run; toggles all work.
