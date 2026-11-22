# NORRIS Phase 2 Spectral Kernels

Reference for 13 phase-2 SoundMagic Spectral kernels (Michael Norris, author of AU plugins). All are Csound opcodes (.orc files in `../csound/spectral/`). Bench files in `../scripts/bench/`.

---

## Kernel Reference

### sm_dronemaker
**One-line:** Gradually interpolates between frequency components to build evolving drones.
- Per-bin interpolation (A→B segments), staggered to avoid frame-wide pulse
- Optional comb filterbank (scale modes), flanger (magnitude notches via delay)
- Peak mode captures largest amplitude during segment; comb cascaded per note

**Approximations:** Comb half-width floor at 0.8 bin; flanger is frame-domain only (no pitch warble).

**Bench:** `sm_bench_dronemaker.csd`; `--omacro:SCN=1..7` (1=default, 2=short interp, 3=peak, 4=comb, 5=flanger, 6=gate, 7=extreme), `--omacro:FFT=N` (512*2^N).

**Limits:** 512 max peaks; memory ~7.5 KB per kernel.

---

### sm_gateandhold
**One-line:** Freezes n loudest/softest peaks for a hold envelope (fade in/hold/fade out), silencing others.
- State machine: idle → waiting → fade-in → hold → fade-out
- Threshold + brightness tilt; n-peak selection via local maxima; peak's magnitude/frequency stored
- Optional decay, pulsing, bin shift, normalize held partials

**Approximations:** Per-bin LCG random (not k-rate repeat); time in frame hops; peak selection takes Hann main lobe (peak + neighbors).

**Bench:** `sm_bench_gateandhold.csd`; `--omacro:SCN=1..7`, `--omacro:FFT=N`.

**Limits:** Up to 50 peaks; per-bin state array (iNb = N/2+1).

---

### sm_weave
**One-line:** Buffers snapshots of steady-state spectra and weaves them (hold/silence repeats) per bin.
- Waits for threshold, detects stability (level variance < max dB for min length), captures "threads"
- Up to 15 threads (oldest dropped if exceeded); each thread: trapezoid hold + optional pulse, silence, cycle
- Threads combine by power sum (sqrt(sum mag^2)); wet/dry mixes with input

**Approximations:** Stability/hold-out metrics are custom; thread starts at head of hold phase.

**Bench:** `sm_bench_weave.csd`; `--omacro:SCN=1..5`, `--omacro:FFT=N`.

**Limits:** 15 threads max; 7.5 s ring buffer (frame history); memory ~5-10 KB.

---

### sm_shimmer
**One-line:** Adds shimmer, jitter, pulsation, and spectral delay feedback for sparkle/sizzle.
- Order: feedback mix → per-bin delay (5 types: descent, ascent, LFO sine, link amp, random) → jitter → pulse → store
- Jitter: per-bin gain ±dB (frequency-dependent, held for N windows ±var)
- Pulse: square wave or sine across spectrum, travelling at Pulse rate Hz

**Approximations:** Jitter ±uniform dB (not his distribution); delay in frames (history 1-256 frames, capped ~1 MB); feedback soft-caps mag at 2.

**Bench:** `sm_bench_shimmer.csd`; `--omacro:SCN=0..7` (0=default, 1=neutral, 2=jitter max, 3-4=pulse sq/sin, 5-6=delay LFO/ascent, 7=fb 90%), `--omacro:FFT=2048` (default).

**Limits:** History depth = max(2, min(256, 1000000/iNb)) frames; mags clamped ≤4.

---

### sm_pulsing
**One-line:** Switches bins on/off pulsating across the spectrum; on/off durations scale per frequency and per cycle.
- Per-bin countdown state machine (on/off); duration = base × freqMul^(8k/iNb) × timeFactor, folded to [min,max]
- Fold modes: ping-pong (bounce min↔max), wrap (sawtooth min→max→min)
- Min/max clamp durations; if min > max they swap

**Approximations:** Freq multiplier compounds every 1/8 spectrum; time multiplier compounds per on/off cycle and is clamped 1e-3..1e3.

**Bench:** `sm_bench_pulsing.csd`; `--omacro:SCN=1..5`, `--omacro:FFT=N`.

**Limits:** Per-bin: state, remaining time, on/off factors; no Wet/Dry or Feedback.

---

### sm_harmonizer
**One-line:** Transposes spectrum by 1–3 intervals, expands each into multiple voices (Stacked/Chord/Harmonics), mixes back.
- Peaks found once per frame (Dolson/Laroche, ≤256); each region moved by integer bin offset + df
- Three intervals; per-interval: up to 10 transpositions; voices sum in magnitude (louder wins), clamped ≤8
- Phase-locked by design (df = f×(ratio−1))

**Approximations:** n=0 treated as 1; max 256 peaks, max 32 voices; regions split at peak midpoints.

**Bench:** `sm_bench_harmonizer.csd`; `--omacro:SCN=1..7`, `--omacro:FFT=N`.

**Limits:** 256 max peaks; 32 max voices total; output clamped ≤8.

---

### sm_stretch
**One-line:** Tracks spectral peaks and shifts them by quadratic law f' = β×f + α×f²/Nyquist (inharmonic stretch).
- Finds peaks (≤512); moves each region by integer bins, adds df to bin frequencies
- Beta can vary per frame (random/LFO/link amp) and quantize to harmonic-like steps
- Louder bin wins overlaps; targets past Nyquist drop

**Approximations:** w normalized to f/Nyquist; alpha=2 gives ~1 kHz→1.09 kHz, 5 kHz→7.3 kHz (musical stretch).

**Bench:** `sm_bench_stretch.csd`; `--omacro:SCN=1..5`, `--omacro:FFT=N`.

**Limits:** 512 max peaks; output in-band only (past Nyquist discarded).

---

### sm_tracing
**One-line:** Keeps or removes bins by rank (loudest n, softest n, range, or threshold gate).
- Sorts magnitudes once per frame; bins outside the keep range are zeroed
- Types: 0 keep loudest n1, 1 remove loudest, 2 keep softest, 3 remove softest, 4 keep ranks n1–n2, 5 remove, 6 keep ≥ threshold, 7 keep ≤ threshold
- Softest ignores silent bins (mag < 1e-6); ties at cut all kept (may exceed n)

**Approximations:** Ties all kept; rank-range reading custom (n2 = lower end of range).

**Bench:** `sm_bench_tracing.csd`; `--omacro:SCN=1..7`, `--omacro:FFT=N`.

**Limits:** Per-frame sort (iNb elements); frequencies untouched.

---

### sm_filterbank
**One-line:** Applies a bank of extremely narrow bandpass filters (GRM Reson-like) built from a base frequency + interval.
- Builds per-bin gain mask (only on param change): filter i at f_i = base×2^(i×int/1200) [Stacked mode]
- Chord & Harmonics modes; side bins add flanking gain (decay per step); overlaps take max
- Feedback + wet/dry mix; magnitudes multiplied by mask, frequencies untouched

**Approximations:** Gain units are dB per step (harmonic & side); snap to nearest bin (FFT-limited accuracy); mask gain capped ≤4.

**Bench:** `sm_bench_filterbank.csd`; `--omacro:SCN=1..6`, `--omacro:FFT=N`.

**Limits:** 400 max filters; mask gain ≤4; memory ~2 KB (mask array).

---

### sm_glidingfilters
**One-line:** Creates up to 100 gliding bandpass filters (Hann-shaped, moving in pitch) that fade in/out to avoid clicks.
- Filters born at Start freq ±var, drift at Pitch glide ±var (cents/s), live for Duration ±var, silence then repeat
- Frame pass mask = max over live filters (no gain pile-up); births/deaths fade over ~40% of life or 0.15 s (whichever larger)
- Random-phase silence so not all filters fire at once

**Approximations:** Fade length, Hann shape, ±var randomization (uniform) are custom; filters die early if f < 20 Hz or ≥ Nyquist.

**Bench:** `sm_bench_glidingfilters.csd`; `--omacro:SCN=1..6`, `--omacro:FFT=N`.

**Limits:** 100 filter slots max; fade floor ~67 ms; memory ~2 KB (mask + state).

---

### sm_granulation
**One-line:** Slices spectrum into grain groups, delays each grain, outputs ONLY the grains (dry path removed).
- Input written to ring buffer (7.5 s history); spectrum divided into "Bins per grain" groups; each group independently clocks grains
- Grain draws: length ±var (frames), delay ±var (frames), bin shift ±var, density coin flip; reads ring `delay` frames ago
- Output via triangular fade (Fade 100% = full triangle); overlaps merge by max; feedback soft-caps ≤1

**Approximations:** Grain length rounds to frames; feedback = previous frame output mixed by sum (clamped ≤1); phase randomization per group.

**Bench:** `sm_bench_granulation.csd`; `--omacro:SCN=1..6`, `--omacro:FFT=N`.

**Limits:** 7.5 s ring (frame history); Bins per grain ≥1; memory ~30 KB (ring 7.5 s × iNb).

---

### sm_emergence
**One-line:** Tracks spectral peaks over time, applies per-partial fade envelopes (fade-in, hold, fade-out), outputs only tracked partials.
- Finds peaks (≤512, -60 dB floor); each peak matched to previous frame within ±fuzziness bins (else born: age 0)
- Envelope by age: fade-in ramp, hold flat, fade-out linear, then 0 (or recycle if enabled)
- Only bins around tracked peaks (±Max bins per partial) output; others zeroed

**Approximations:** Fuzziness and max-bins semantics custom; L/R independent state (identical mono input = identical envelopes).

**Bench:** `sm_bench_emergence.csd`; `--omacro:SCN=1..4`, `--omacro:FFT=N`.

**Limits:** 512 max peaks; Dolson/Laroche regions (dynamic, per frame).

---

### sm_partialglide
**One-line:** Tracks partials and shifts their pitch at a set rate (cents/s); rate per partial; mirror bin flips direction below it.
- Finds peaks (≤512); each peak matched to previous-frame track (else born: age 0, rate = central ±var)
- Every frame: offset += rate×hop seconds; region moves by integer bins for coherent partial carry; louder bin wins overlaps
- Fade-in ramp; Zero Transients mutes partials seen in only 1 window; glide wraps at 14400 cents or past 0 Hz/Nyquist

**Approximations:** Glide wraps (sawtooth) rather than stops; regions split at peak midpoints; per-channel randomness not included.

**Bench:** `sm_bench_partialglide.csd`; `--omacro:SCN=1..6`, `--omacro:FFT=N`.

**Limits:** 512 max peaks; offset magnitude caps ±14400 cents (wraps); only tracked partials output.

---

## By-Ear Checklist for Emory at the Rig

**Listen for & tweak:**

| Kernel | What to hear | Parameter to move |
|--------|--------------|-------------------|
| **DroneMaker** | Smooth interpolation vs. stepped jumps | Interpolation Length (shorter = more steps) |
| **GateAndHold** | Frozen peaks vs. decay; envelope snappiness | Hold Length (longer = sustain); Fade In (snappier = shorter) |
| **Weave** | Thread density & weave rhythm; when new capture fires | Hold Length ±variance; Threshold (lower = capture earlier) |
| **Shimmer** | Jitter sparkle & pulse pattern; delay texture | Jitter Amount, Amplitude Pulse Type, Delay Variance Type |
| **Pulsing** | Bin pulse rate & direction; frequency-dependent sweep | Pulse Rate (frequency), Multiplier Freq (couples to pitch) |
| **Harmonizer** | Harmony blur vs. separation; transposition clarity | Num Transpositions per Interval (more = richer); Gain per interval |
| **Stretch** | Inharmonicity amount & direction of stretch | Alpha Stretch (0=linear, 2=musical); Beta Variance Type |
| **Tracing** | Sparse vs. dense spectrum; which bins keep | Number of Bins (smaller = sparser); Tracing Type (0-7) |
| **Filterbank** | Filter sharpness & side-bin shimmer; harmonic richness | Number of Filters; Side Bins & Side Bin Gain (wider/shimmer) |
| **GlidingFilters** | Filter pitch glide speed & direction; glide paths crossing | Pitch Glide per Second (cents/s); Start Frequency |
| **Granulation** | Grain size, delay, and density rhythm; grain texture | Grain Length (shorter = micro-granular); Grain Delay; Density |
| **Emergence** | Partial birth/death envelope, pulsation | Fade In / Hold Windows; recycle on/off (wraps vs. stops) |
| **PartialGlide** | Pitch glide direction & speed per partial; mirror effect | Pitch Shift per Second (cents/s); Mirror Bin (flips direction) |

---

*Kernels built Oct 5, 2026. Bench files include 1–7 presets each (SCN param), with L=processed, R=dry for A/B.*
