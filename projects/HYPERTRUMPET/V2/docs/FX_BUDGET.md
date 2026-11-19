> **Status as of bringup-1.12:** the device-measured tables near the end of this file (and the combo and runtime-guard sections) are the truth; earlier sections are host estimates kept for the optimisation history. Admission limit 65 %, runtime shed at 8 of 32 blocks over 75 % or one over 90 %.

# Effect CPU budget (host bench, no hardware)

Bench: `bringup/fw/tests/fx_budget.cpp` (build line in its header: `-O2 -fno-vectorize`, the M7 has no NEON). Each effect with the firmware's defaults, MIX 1, continuous input (noise + 440 Hz), 48-sample blocks. Average = ns per sample over 30,000 blocks; worst block = the largest single block over 5 identical passes with the per-block minimum taken (removes OS jitter; the timer step is about 40 ns, so the worst-block column is good to about +-0.4 %).
Seed estimate = host ns/sample x F / 20,833 ns, with **F = 82.1**, derived from the one measured pair: the ORIGINAL reverb, 74.1 ns/sample on this host = 29.2 % on the Seed (cpu4[1], fader 1 at 95 %). F is fixed from that original number so the "after" column is on the same scale.
Caveat: F comes from a memory-bound effect (the reverb walks 500 KB of SDRAM). Compute-bound code (FFT, filters) may cost less on the Seed than this scale says, SDRAM-heavy code (grain, delay, looper) may cost more. Treat the numbers as a ranking, and replace them with `fx[i]` telemetry (`cyc` / `pk`) from the Seed.

Targets: average < 5 %, worst block < 8 %, reverb < 8 %.

| Effect | Before avg / worst | After avg / worst | Met | What changed |
|---|---|---|---|---|
| reverb | 29.2 % / 30.5 % | 7.0 % / 7.2 % (7.3 / 9.2 on a noisy run) | yes (worst borderline) | 4-line FDN (`HX_REVERB_LITE`, set in `fx_glue_common.h`); per-line modulation, Hermite weights and warp slew once per 8 samples instead of per sample (no `floorf`, no per-sample LFO); no division / `flush` per state (anti-denormal +-1e-18 on the input); cubic output limiter without division; dry/wet blend inside the loop |
| drone (other owner) | 22.3 % / 87 % | 12.2 % / 60 % (their file as of 01:28) | NO | not touched; the drone agent's own bench says 26 ns/sample, worst 2.3 us, mine measures 30.8 ns, worst 7.3 us (frame boundary block); on this scale it is over 6 % / 10 % |
| freeze | 28.3 % / 47.6 % | 12.2 % / 25.3 % | no | hop N/2 (half the frames/s), sliced windowed load (`slicedLoad`), the region-peak hill climb / normalisation only for bins that can take a new peak (bit-identical result) |
| blur | 18.3 % / 26.0 % | 10.3 % / 17.4 % | no | hop N/2, sliced load, one sqrt + one division per bin instead of two |
| grain | 21.9 % / 26.0 % | 6.0 % / 8.2 % | almost (avg 6.0 vs 5) | 16 voices max, density cap 0.7 x voices (about 11 active at the v1 preset); voices render at half rate with linear rebuild; one mono grain buffer; four voices per loop; voice-major; spawn math hoisted (`powf` to `exp2f`, transposition ratio once per block); level equal to the original (rms 0.197 vs 0.191), 1.8 dB less above ~12 kHz |
| roar | 13.1 % / 13.3 % | 4.3 % / 4.4 % | yes | half-band FIR exploited (symmetry + zero taps: 4 + 5 mults instead of 8 + 15), tilt filters skipped at TONE 0 (identity), one `tanf` set per block only when TONE is used; output equal to the original within 1e-6 |
| chorus | 10.4 % / 10.6 % | 3.8 % / 3.8 % | yes | voice delays and Hermite weights once per 8 samples, denormal guard once per block |
| delay | 6.2 % / 6.5 % | 4.5 % / 4.8 % | yes | Hermite weights cached while the delay time is steady (recomputed per sample only while it glides), gate smoother skipped when settled |
| flange | 4.2 % / 6.8 % | 4.3 % / 6.8 % | yes | already inside the budget |
| harm | 3.9 % / 4.1 % | 2.3 % / 2.4 % | yes | (run-to-run) |
| mood | 3.6 % / 4.8 % | 3.9 % / 4.8 % | yes | |
| ring | 2.3 % / 2.4 % | 2.3 % / 2.4 % | yes | |
| wah | 2.4 % / 2.4 % | 2.3 % / 2.4 % | yes | |
| looper | 1.2 % / 1.4 % | 1.2 % / 1.4 % | yes | playing one layer; each extra layer adds one buffer read per sample. The new trim scan runs only at the first close (16 x block samples per block for about 20 ms) |

## Why freeze, blur and the drone cannot reach 5 % on this scale
The STFT plumbing alone (window load, forward FFT, inverse FFT, overlap-add, trivial bins) costs 19.5 ns/sample at N = 1024, hop N/2 (hop N/4: 37 ns), i.e. 7.7 % on the Seed scale before any effect maths. Freeze adds about 11 ns/sample, blur about 6.6. Reaching 5 % needs a faster FFT in `fx_fft.h` (radix-4 or radix-2^2 butterflies: fewer loads/stores and 25 % fewer multiplications, about 1.4x), which is the drone agent's file; I did not touch it. A smaller `FRAME` does not help (N = 512: 18.6 ns/sample).
The drone through the same factor: 12.2 % average and 60 % worst block, over its 6 % / 10 % target (my bench, default params, 440 Hz + noise in).

## Firmware changes that go with this
`fx_glue_common.h` defines `HX_REVERB_LITE`; arena bytes for freeze and blur grew by the sliced-load ring (+16 KB); the per-effect CPU estimates in `fx_glue_<name>.h` now use the "after" column (used by the 80 % refusal rule until the real peak is measured); the reverb arena is 300 KB (was 497 KB).

## Reverb, 4 lines vs 8 lines (host impulse response, glue defaults: decay 0.94, size 1.6, damp 5.5 kHz, rate 0.4 Hz, pre 25 ms)
| | original 8-line | optimized 8-line | 4-line (shipped) | 4-line, MODDEPTH 0.8 (shipped default) |
|---|---|---|---|---|
| RT60 (Schroeder T30 x2) | 6.37 s | 6.37 s | 6.59 s | 6.51 s |
| frequency-response ripple 300 Hz-8 kHz (sd) | 7.10 dB | 7.10 dB | 6.93 dB | 7.17 dB |
| echo density 0.1-0.2 s (fraction above 25 % rms) | 0.72 | 0.72 | 0.56 | 0.56 |
| modulation effect (IR with vs without modulation) | 2.9 dB | 2.9 dB | 2.0 dB | 2.9 dB |
| warp glide (-20 dB point, warp 0 to 1 over 2 s) | 1.81 to 4.97 s | 1.81 to 4.99 s | 1.87 to 5.28 s | 1.87 to 5.29 s |
Tail length, smoothness and warp glide are intact. The 4-line version is sparser in the first 200 ms (echo density 0.56 vs 0.72) and needs MODDEPTH 0.8 to match the 8-line modulation; both are set in `fx_glue_reverb.h` (default MODDEPTH 0.8, accel Y maps 0.5..1.0). Not auditioned by ear.

## Device numbers (bringup-1.6 on the instrument Seed, `fx[i]` / `dr` telemetry, 2026-11-17)
Measured while running with the sensors at rest. cyc = average load while the effect runs, as a fraction of one 48-sample callback (DWT cycles around the effect's process call).
| Effect | device avg | device peak | host ns/sample (this doc) | device F = (avg x 20,833 ns) / host ns |
|---|---|---|---|---|
| drone | 9.8 % | 21.5 % | 23.0 | 88.8 (143 if the drone agent's own host figure of 14.3 ns is used; my bench measures the same file at 23.0) |
| roar | 6.5 % | 6.9 % | 10.8 | 125 |
| flanger | 5.8 % | 6.1 % | 10.7 | 113 |
| reverb | not running in the readout (its 135 % peak was the wake-time memset, fixed in bringup-1.7) | | 17.7 | reverb-derived F = 82.1 (from the old 8-line number) |
So the device factor is 89 to 125, not 82: simple, cache-friendly filter code (roar, flanger) runs relatively SLOWER on the Seed than the memory-heavy reverb the factor was derived from. A better working factor from these three is about 110. The 11_bench firmware will replace this with a full per-effect table; until then multiply the "after" column above by about 1.1 to 1.5 (roar 4.3 % predicted became 6.5 % measured). Roar and flanger are therefore over the 5 % target on the device; that is a host-bench factor problem, not a regression.
At rest the callback average was 27.7 %: drone 9.8 + roar 6.5 + flanger 5.8 + reverb off + overhead. The "idle" class c4[0] = 8 % was NOT an idle chain: flanger (lidar sees nothing near, v1 maps far = more mix, so it runs at 76 %) and roar (pot 1 reads 88 %) were running. The real overhead of the chain with every effect off is now reported as `ov:[chain overhead, whole chain]` in the telemetry.

## Harmony (3 voices on the shared shifter core), host estimate
`fx_harmony.h`: 27.7 ns/sample, worst block 1.4 us on the host = 10.9 % average / 11.3 % worst block at F = 82 (about 14.6 % at the device factor of 110). Harmonizer (3 voices) + chorus + reverb together: 14.6 + 4.2 + 9.6 = about 28 % average at F = 110, well under the 60 % peak target; the 11_bench firmware measures it on the Seed (state `harmony`, and `all`). Latency: first wet energy immediately, click response peaks at 6.6 ms, mean read delay 9 to 13 ms (grain G = 2 M period, 24 ms fallback when unvoiced), measured in `tests/fx_quality.cpp`. Pitch accuracy of the +7 voice: centroid 659.1 Hz vs 659.3 Hz expected (-0.4 cents); the time-domain shifter puts sidebands at the grain-jump rate (about 27 Hz for a +7 voice), the strongest single line can sit on a sideband.

## DEVICE-MEASURED TRUTH (bringup-1.9, each effect alone at mix 1.0 plus a parameter sweep, `soak.py --phase fx*`, logs/soak_20261117_0610..0617)
Values are the effect's own average and peak load as a percentage of one 48-sample callback. This table replaces every estimate above; idle (nothing on) is 1.8 %. Host ns/sample is from `tests/fx_budget.cpp` (current files), F = device avg x 20,833 ns / host ns.
| effect | device avg % | device peak % | host ns/sample | device F |
|---|---|---|---|---|
| reverb | 11.2 | 12.1 | 17.7 | 132 |
| drone | 4.9 | 5.8 | 23.0 | 44 |
| harm | 2.3 | 2.4 | 5.8 | 83 |
| looper | 2.4 | 2.5 | 3.3 | 152 |
| freeze | 22.9 | 51.8 | 24.2 | 197 |
| blur | 15.8 | 35.0 | 19.9 | 165 |
| flange | 3.0 | 3.2 | 10.7 | 58 |
| chorus | 3.8 | 4.3 | 9.6 | 82 |
| mood | 4.6 | 5.3 | 10.0 | 96 |
| grain | 8.7 | 10.7 | 15.3 | 118 |
| delay | 6.1 | 6.5 | 11.3 | 112 |
| roar | 3.4 | 3.5 | 10.8 | 66 |
| ring | 1.9 | 1.9 | 5.7 | 69 |
| wah | 1.0 | 1.1 | 5.9 | 35 |
| autopan | 1.1 | 1.1 | 4.4 | 52 |

The device factor is NOT constant: it runs from 35 (wah, drone, autopan) to 197 (freeze, blur), median 83. The reverb-derived 82 was a fair middle; the FFT effects (freeze, blur) cost 2 to 2.4 times what the host numbers predict, the simple filters and the drone cost less. Freeze (22.9 / 51.8 %, 5 xruns) and blur (15.8 / 35 %) are the two over budget; they and the drone belong to the drone agent's FFT work. Admission control now uses these measured peaks as the cost of turning an effect on (admit only if the projected total peak is <= 70 %).

## Update after the drone agent's FFT changes (device, bringup-1.10 era)
| effect | device avg % | device peak % | host ns/sample (now) | device F | before |
|---|---|---|---|---|---|
| freeze | 5.2 | 10.0 (no xrun, fixed) | 13.8 | 31 | 22.9 / 51.8 with 5 xruns |
| drone | 7.5 | 8.6 | 15.5 | 101 | 4.9 / 5.8 at host 23.0 ns (the file changed: the host number is what moved) |
Freeze is now inside the 5 % average / 8 % worst-block target on average but its peak is 10 %; drone 7.5 % is over the 6 % target. `kAdmitPeak` (fx_glue_chain.h) now uses freeze 0.10 and drone 0.086 (bringup-1.11); blur still carries its old 0.35 until it is re-measured.

## Runtime guard (bringup-1.12)
Static admission cannot see parameter-dependent cost (sweeps, FRAME changes, the roar's 4x oversampling above DRIVE 0.6 at about 18-25 %), so the guarantee layer is dynamic: the audio thread flags overload when 8 of the last 32 callbacks went over 75 % or one went over 90 %; the main loop then sheds the most recently enabled non-fader effect (fade-out through the gate tail, refused for 5 s, its pad blinks red), one per 150 ms, repeating until the flags stop. Reverb, drone and grain (faders) are shed last (their fader is blocked until it has been at 0). Every shed is counted in `st.shed` / `shedlast` and in `state.shed`. Admission limit is now 65 % projected total peak, cost = max(device-measured peak table, the effect's own run-time peak), so the roar's heavy-fold cost is learned the first time it runs. The drone agent's roar (ADAA, 4x stage above DRIVE 0.6), `fx_halfband.h` and the freeze/blur smoother fix are in the tree and in this build; arenas fit (roar 3.1 KB of its 4 KB).

## Device combos on 1.12 (soak fx4, logs soak_20261117_0659..0747)
Harmony 3 voices (+4 +7 +11): 14.3 % avg / 15.1 % peak (the effect itself 12.4 / 12.7). Harmony + chorus + reverb: 31.1 % avg / 33.4 to 34.3 % peak (target < 60 %). Idle 1.8 %. Stress runs (all effects, random toggles): 0 xrun, 0 gap, 2 sheds per 4-minute run.
