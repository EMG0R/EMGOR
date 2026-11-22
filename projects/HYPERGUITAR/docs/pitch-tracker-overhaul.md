# Pitch tracker overhaul (v2): what guitar-to-MIDI does, and what changed

2026-09-24. Scope: `bela/hyperGuitar6chPan/pitch_tracker.cpp`, a drop-in replacement for the
per-string tracker. The interface is unchanged. No other firmware file was touched.

- **v1 archive:** `bela/archive/pitch_tracker_v1.cpp.txt`, byte-identical to the working-tree file
  before this change. To restore it: `cp bela/archive/pitch_tracker_v1.cpp.txt bela/hyperGuitar6chPan/pitch_tracker.cpp`.
- **Harness:** `bela/hyperGuitar6chPan/hosttest/pitchbench/`. It is new, and every number below comes
  from it.
- **Status:** **host-verified only.** Nothing was deployed, and nothing has been heard on the real
  SubSix. See "What is NOT verified" before trusting any of it.

---

## TL;DR

| synthetic test, bleed -23 dB | v1 | v2 |
|---|---|---|
| pick -> correct pitch, single notes, p50 / p90 / max | 28.1 / 45.4 / **506** ms | **12.7 / 23.2 / 31.7** ms |
| same, Karplus-Strong material | 33.9 / 60.6 ms, **7 of 54 never correct** | 12.7 / 23.2 ms, 0 never |
| strum (6 strings within 25 ms) p50 / p90 | 28.0 / 48.7 ms | 15.0 / 24.0 ms |
| legato (hammer-on/pull-off, no pick) p50 / p90 | 24.9 / 39.1 ms, 1 never | 16.3 / 27.7 ms |
| KS legato | 30.4 / 42.0 ms, **10 of 48 never** | 13.9 / 22.0 ms |
| fast repeated picks (100 ms), alternating notes | 26.6 / 44.4 ms | 17.8 / 31.2 ms |
| bleed-only strings claiming a confident pitch | **38.5 % of hops, 167 retunes** | 0 %, 0 retunes |
| octave errors while confident | 0 | 0 |
| 200 c bend + 30 c vibrato: period error mean / max | 7.2 / 21.7 c | 5.0 / 17.2 c |
| correlation MACs per call: mean / worst | 9.1 k / 65 k | 3.9 k / 51 k |

The slow oscillating "beading" on the two top strings is **not** caused by the tracker. It
reproduces with a perfect, oracle-fed period. The cause is the pitch-UP grain leg in `shifter.cpp`
meeting the inharmonicity of plain strings. See "The beading on strings 4-5".

---

## 1. What guitar-to-MIDI systems actually do (research summary)

Tags: **[DOC]** documented or measured, **[MKT]** manufacturer claim, **[FORUM]** hearsay.

These sources were gathered in a separate research pass. The citations are given as reported and
were not all re-fetched while writing this note.

### The physical floor: one to two periods of the fundamental

- **Derrien, DAFx-14, "A very low latency pitch tracker for audio to MIDI conversion".** It states
  that the minimum delay is `1/f0min`: an 80 Hz low E gives 12.5 ms, and a 2-period window by rule of
  thumb gives 25 ms. YIN on guitar measured ~30 ms, "approximately twice the theoretical minimum".
  Derrien's own offline ESPRIT + harmonic-likelihood method got the correct note after ~12 ms, against
  20 ms for YIN. [DOC]
  https://www.dafx14.fau.de/papers/dafx14_olivier_derrien_a_very_low_latency_pitch_.pdf
- **Reboursière et al., NIME 2010.** Strat with a GK-3, per-string yin~ and sigmund~. The best
  accuracy/latency trade-off came from a window "closest to two periods of each of the open strings",
  with hop = window/3. [DOC] https://www.nime.org/proceedings/2010/nime2010_415.pdf
- **Measured converter latencies**, from a pick-to-synth loopback at 44.1 kHz:
  - GR-300: 3.3 ms
  - Yamaha G50 (Axon-licensed): 11.5 ms
  - GI-10: 18.6 ms
  - GR-700: 28.5–29.9 ms
  - GM-70: 40 ms

  [DOC, amateur with a stated method] https://gr300.com/MIDI_SPEED.htm. Derrien measured a GR-55 at
  ~50 ms average.
- **Fishman TriplePlay:** 7 ms (high notes) to 14 ms (low notes) for pitch detection alone. [MKT]
  https://www.zzounds.com/item--FSMTPEXPRESS

**Why the GR-300 gets 3 ms:** it never computes a pitch number. Each string is squared into a pulse
wave that hard-syncs a VCO through a PLL. [DOC] https://en.wikipedia.org/wiki/Roland_GR-300. That is
waveform-following, not pitch detection, and it is not available to a PSOLA shifter, which needs the
period.

### How they get close to that floor, specifically on guitar

1. **Per-string isolation is the whole premise.** Every system that tracks fast is monophonic per
   string. A hex pickup turns polyphonic guitar into six monophonic problems with a *known range
   each*. TriplePlay's manual says per-string channels improve tracking. [DOC]
   https://images.thomann.de/pics/atg/atgdata/document/manual/310144_tripleplay1_4_user_guide.pdf
2. **Period measurement in the time domain, from a low-passed string signal.** The Roland patents
   describe zero-cross-interval and peak (max/min flip-flop) period extractors fed from a low-pass
   filter. They note that zero-crossing "does not operate well" when harmonics survive the filter.
   A Casio patent measures the zero-crossing interval right after a held peak, which rejects harmonic
   crossings, and scales the peak-hold decay to the string's pitch. [DOC]
   - https://patents.google.com/patent/US5018428A/en
   - https://patents.google.com/patent/US5018427A/en
   - https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/4357852
3. **Use the string's range as a prior and size windows to it** (Reboursière, above). A string can
   only sound from its open note up to about the 24th fret, which removes most octave-down candidates
   for free.
4. **Onset first, pitch second.** Cycfi's BACF tracker runs a per-sample onset detector and batches
   pitch over at least two cycles. It predicts the pitch early from "the running stream of peaks or
   the zero-crossings on, or immediately preceding note onset". Its argument is that anything under
   ~20 ms is not heard as out of tune. A median of three estimates (previous, normal polarity,
   inverted polarity) cut errors significantly. [DOC blog]
   - https://www.cycfi.com/2018/06/fast-and-efficient-pitch-detection-synth-tracking/
   - https://www.cycfi.com/2021/02/fast-and-efficient-pitch-detection-power-of-two/
5. **Axon's "pattern recognition from the transient".** Patent US5929360 (Blue Chip / Yamaha)
   detects the pulses that travel from the pluck and reflect off the bridge and the fret. Their
   transit times give the vibrating length, so the pitch can be found before steady state. A
   factory-trained neural network classifies the pulse sequences, and a frequency meter validates
   the result. [DOC] https://patents.google.com/patent/US5929360A/en. Axon markets a "3 ms window"
   [MKT], but the licensed G50 measured 11.5 ms, roughly one low-E period.
6. **Sense the fret instead of the pitch.** Yamaha G10 (ultrasonic reflection), SynthAxe and the
   Starr Ztar use fret contact, and Jamstik combines fret scanning with a hex pickup. This is the only
   way under one period, and it needs different hardware. [DOC/MKT]
   - https://www.muzines.co.uk/articles/yamaha-g10/2313
   - https://en.wikipedia.org/wiki/SynthAxe
   - https://jamstik.com/pages/faq

### Octave errors: why a bridge hex pickup invites them

- **Pickup position combs the partials.** A pickup near the bridge is ~10 dB down on the fundamental
  compared with the neck position. [DOC] https://till.com/articles/PickupResponse/. At GK/SubSix
  distances (~2 cm) the fundamental is well below partials 3–10, so the raw waveform is
  harmonic-dominated. This is the mechanism behind octave-up and harmonic lock-on errors.
- **Stiffness makes the partials sharp:** `f_n = n f0 sqrt(1 + B n^2)`. Autocorrelation over a
  full-band signal measures a compromise period that is biased sharp, while a fundamental-locked
  period is not. [DOC] https://en.wikipedia.org/wiki/Inharmonicity
- **Derrien's harmonic likelihood** "naturally avoids octave errors". An octave too low predicts
  extra partials, and an octave too high leaves empty harmonic slots, so both are penalised.

### Crosstalk (bleed)

- **Measured GK-5 crosstalk** (Cycfi, Feb 2026), electromagnetic only with the other strings muted:
  worst peak -23.4 dB, averages -38 to -51 dB. The author's recommendation is to gate onsets
  **relative to the loudest currently excited string** (about 12 dB below it), not at an absolute
  level. He also calls peak crosstalk, not average, the number that matters. [DOC]
  https://www.cycfi.com/2026/02/roland-gk-crosstalk-deep-dive/
- **Piezo hex (Ghost, RMC) is not crosstalk-free.** Cycfi says piezo has "the worst" crosstalk
  because of mechanical coupling through the saddles. Reviews report chord "dithering". [DOC opinion]
- **GuitarSet** (ISMIR 2018, magnetic hex) removed bleed *offline* with kernel additive modelling.
  It notes that a muted string in a chord "still produces a clear pitch and onset" on its own
  channel. [DOC] https://archives.ismir.net/ismir2018/paper/000188.pdf. No published *real-time*
  crosstalk canceller for magnetic hex pickups was found.
- **Every commercial system** exposes per-string sensitivity and says calibrating it is the most
  important setup step (GR-55 training guide, TriplePlay manual). [DOC]

### Note-off

Note-off in these systems is:

- a level threshold,
- an envelope whose decay rate follows the pitch (Casio patent), and
- a sustain-vs-clean-data trade-off exposed to the player (Axon "Note-Off limit").

The hyperGuitar already had a better-than-threshold detector: the fretting-release edge detector in
v1. v2 keeps it unchanged.

### What that means for this instrument

The tracker feeds a PSOLA/SOLAD shifter, so it must produce a **period**, not just a note number. It
cannot use fret sensing or the GR-300 sync trick. The transferable ideas are:

- the pick is the time reference,
- analyse only post-pick data,
- evaluate each candidate period as soon as roughly one of its own periods of data exists,
- use the per-string range prior,
- guard octaves explicitly, because a bridge pickup starves the fundamental,
- judge crosstalk *relative to the neighbours*,
- snap on a pick, and require consensus without one.

---

## 2. What was wrong with v1 (measured with the harness)

v1 ran NSDF on `v->dbuf` with a fixed window of 2 *open-string* periods, blanked 6 hops after every
onset, then needed 2 agreeing hops. Five separate problems stacked up.

1. **The latency was structural.**
   - The blank alone is 17 ms.
   - The window then still held the previous note until it flushed, which takes 2 open periods plus
     the lag: 47 ms of history on the low C, and the same 47 ms at the 12th fret.
   - The consensus added 2 more hops.

   Measured single-note pick-to-correct times: 22–50 ms, flat across the neck. On the low C it was
   49.2 ms open and still 32.3 ms at the 22nd fret.
2. **The analysis filter moved during the analysis.** `dbuf`'s lowpass is `harmCut × f(tracked)`,
   re-aimed every hop with a 0.25 glide in `updateLP()`. After every note change the cutoff glides
   for several hops. A filter whose group delay changes inside the correlation window stretches the
   waveform, so it reads a different period. Measured: +40..+55 cents transient readings for
   ~15 ms after a low-string note change.
3. **The top strings aliased.** On strings 4–5, `harmCut` = 10 puts the cutoff at 10 × f0, which is
   far above the /4 decimated Nyquist (5.5 kHz). The box-car decimator doesn't stop that. Aliased,
   inharmonic upper partials of fretted plain strings capped the NSDF clarity at ~0.88, under the
   0.92 confidence bar, for hundreds of ms. Measured:
   - high C, 10th fret: correct only after **506 ms**
   - Karplus-Strong material on strings 3–5: 7 of 54 single notes never became correct
   - KS legato: 10 of 48 never became correct
4. **The refinement gave up silently.** The full-rate refine searched ±3 samples around the coarse
   lag and discarded its result when the peak sat outside that range. That happens on every note
   whose coarse estimate is off by more than about 0.5 % on the low strings.
5. **Crosstalk was only guarded for one specific case** (exactly 2× a louder neighbour). Strings
   carrying only a neighbour's bleed claimed a confident pitch on 24–60 % of hops, depending on bleed
   level (-30 to -12 dB), and retuned their period 105–252 times per test. The synth was protected
   only by the separate dominance test in `dsp_core.cpp`. The shifter, FM, sustainer and resonator
   on those strings were tuned to the neighbour's note.

---

## 3. The beading on strings 4–5 (the owner's second complaint)

Symptom: a slow oscillating, "beading" sound on the pitch/formant output of the two top OPEN
strings (G 196 Hz, C 261.6 Hz, tuned a little flat).

**Tracker range:** fine.

- `TUNE_MARGIN` 1.12 gives 196 cents of flat headroom.
- The top strings have range `lagMin..lagMax` = 9..47 / 12..63 decimated.
- Tuning 25 or 50 cents flat changes nothing measurable.

**Tracker period:** v1 read the top strings +3.1 c sharp (sd 3.4–3.7 c, excursions to +14 c). The
cause is inharmonic plain-string partials pulling a full-band autocorrelation. v2 reads +1.1 c
(sd 0.55–1.05 c, range -3..+4 c).

**But the tracker is not the cause.** The harness can overwrite `v->period` / `periodGood` after
every `trackPitch` with the *exact* true period (`PB_ORACLE=1`). The beading metric is the slow
modulation of the output's partials 1–6 in 50 ms frames, detrended over 500 ms. It is the same with
the oracle as with the tracker:

| shift | s4 G wet / dry (tracker) | s4 (oracle) | s5 C wet / dry (tracker) | s5 (oracle) |
|---|---|---|---|---|
| none | 3.1 / 3.1 % | 3.1 % | 5.2 / 5.2 % | 5.2 % |
| formant 1.3 | 6.8 / 3.1 % | 6.3 % | 4.0 / 5.2 % | 4.0 % |
| **+12** | **30.1** / 3.1 % | **30.3 %** | **25.0** / 5.2 % | **24.7 %** |
| **+7** (oracle run only) | — | 19.4 % | — | 20.6 % |
| -12, SOLAD (oracle run only) | — | 3.2 % | — | 7.1 % |

Across strings at +12 with the oracle period, the modulation rises with pitch:

| s0 | s1 | s2 | s3 | s4 | s5 |
|---|---|---|---|---|---|
| 7.7 % | 7.2 % | 15.1 % | 14.5 % | 25.9 % | 24.4 % |

It also scales with the stiffness constant B:

| B | s4 | s5 |
|---|---|---|
| 0 | 9.2 % | 9.2 % |
| 1× | 25.9 % | 24.4 % |
| 2× | 29.7 % | 29.3 % |

**Cause, confirmed:** the pitch-UP grain leg (`legRun(&sh->up, …)`, S = T/rho, grains 2S long,
source advancing one mark per launch) repeats or skips whole source periods by design. On an
inharmonic string, partial k drifts off the harmonic lattice at about `f0 · B · k³ / 2` Hz, so every
repeat makes a phase jump in the upper partials. The result is partial-level AM at rates of 0.3–3 Hz
on the top strings. That is the slow "beading".

The rate grows with f0 and with B, and B is largest on plain, slack strings. A C-standard tuning
tuned flat lowers tension, which raises B. That is exactly the top two open strings. SOLAD down and
the formant leg are essentially clean.

**For whoever owns `shifter.cpp` (I did not edit it).** Fixes tried in scratch copies of
`shifter.cpp`, with the oracle period, at +12:

| change | s0 | s3 | s4 | s5 |
|---|---|---|---|---|
| current shifter, and identically with the up leg on the formant leg's `stab=1` smoothing | 8.0 % | 15.1 % | 30.0 % | 24.7 % |
| picker LP at 1.0·f0 instead of 1.5·f0 | worse everywhere | | | |
| **up-leg grains 4S long (= 2 source periods at +12), 0.5 gain** | **4.0 %** | **11.7 %** | **19.7 %** | **14.7 %** |

Longer up-leg grains are the only lever that moved the metric. They are only exactly COLA at rho = 2
as tried (4S at spacing S sums to a constant 2). A general version needs `L = 2T` with
per-sample window-sum normalisation, and `SH_NGRAINS` (6) must cover the extra overlap at high
ratios. Longer grains trade transient smear for less beading.

The more complete fix is an up-shifter that does not repeat periods of an inharmonic source every
period:

- a resampling SOLAD-up (read faster, splice back several periods at a time), or
- a phase-vocoder up leg.

Also worth knowing: the formant leg is already stable because of `STAB_C`. Its residual
(6.8 % vs 3.1 % dry on s4) is small.

**Not verified:** that the rig's beading is this mechanism rather than, or as well as, the other
candidates:

- the DRIFT splicer flapping on quiet top strings,
- a neighbour's bleed being granulated at this string's period (-23 vs -80 dB bleed made little
  difference in simulation: 30 vs 26 %),
- the sympathetic comb (`effects.cpp`) sitting at `p0`, the NOMINAL open pitch. On a string tuned
  25 c flat that comb beats against the string at `f·(1-2^(-25/1200))` ≈ 3.8 Hz on the high C.
  Only relevant if SYMPATHETIC is on in the patch.

A 10-second recording of each top string, open, +12 on and off, would settle it.

---

## 4. The v2 design

Every item maps to section 1. Constants are at the top of `pitch_tracker.cpp`.

| # | Mechanism | Source idea | Constants |
|---|---|---|---|
| 1 | **Post-pick analysis only.** The window starts at the located pick plus a 48-sample skip, and a lag becomes evaluable once 0.75 of its own period of new data sits behind it (window `min(n - lag, 2·lag, corrWin)`). High notes resolve in ~2 of their own periods. | Derrien floor; Cycfi "onset first" | `PT_SKIP`, `PT_WMIN_FRAC`, `PT_WCAP_FRAC` |
| 2 | **Retrospective pick location.** `shifterOnset` fires up to 16 ms late on the low strings, because its time constants scale with the open period. On each onset, 32-sample first-difference energy blocks over the prior 25 ms locate the pick burst, and the window starts there (never later than the stamp). | pick transient = time reference | `PT_LOC_BACK` |
| 3 | **Own fixed analysis filter.** 4th-order Butterworth per string at 1.3 × the highest note (floor 1 kHz: 1.0/1.0/1.0/1.0/1.15/1.53 kHz), kept at full rate (refine) and /4 (sweep). No glide artifacts, no aliasing, and an NSDF peak wide enough for the step-2 grid. | Roland/Casio LPF-before-period | `PT_AA_FLOOR`, `PT_AA_TOPMUL` |
| 4 | **Range prior plus octave guards.** First key max (McLeod) with every coarse peak's skipped neighbours filled in first. An early single-hop acceptance is allowed only if 2× the lag was evaluated, otherwise 2 agreeing hops are needed. Harmonic lock-on guard: a clearly higher NSDF (+0.05) at 2–4× the lag promotes to that lag. v1's half-lag guard is kept. | range prior; bridge pickup starves the fundamental; Derrien's harmonic-slot logic | — |
| 5 | **Crosstalk level model.** Each string publishes its ~10 ms power. Predicted bleed = Σ neighbours × coupling (-17 dB adjacent, -29 dB next-but-one: 6 dB more pessimistic than the -23/-37 dB the rig measured). At ≤ +3 dB over predicted bleed, a string never starts a note. At ≤ +9 dB, a candidate equal (±2 %) to a ≥3 dB-louder locked neighbour's pitch is that neighbour. Bleed is reported as silence (dbgGate 2, conf 0, period held), which triggers the shifter's silent-string freeze. | Cycfi relative-to-loudest gate | `PT_BLEED_C1/C2/HARD/SOFT/PTOL` |
| 6 | **Snap vs consensus.** After a pick: the first estimate ≥ 0.90 is the note, or ≥ 0.75 on 2 agreeing hops (a quiet string whose clarity is capped by a louder neighbour's bleed). Without a pick (legato, missed onset): 2 agreeing hops, 3 for an exact octave or a weak read. | TriplePlay/GR "trigger" logic | `PT_ACCEPT`, `PT_ACCEPT_LO` |
| 7 | **attackBlank ends on acceptance**, not after a fixed 6 hops. It still counts down, so an unresolved note behaves as it did in v1. | — | — |
| 8 | **Steady-state windows.** Once a note has held still for 8 hops, the window grows to 4 periods (coarse, capped at corrWin) and 3 periods (refine). This averages the beat against a neighbour's bleed. A legato tripwire (2-period NSDF at the current lag < 0.6) and a signed motion detector (bends, vibrato, ~2 c/hop) drop it back to 2 periods at once. | — | `PT_STEADY_HOPS`, `PT_WCAP_STEADY` |
| 9 | **Refinement** on the filtered full-rate ring: a ±1.8 % grid, then halving steps, then a parabola. Memoised, ≤ 11 lag evaluations. | — | — |
| – | **Kept from v1 unchanged:** the release edge detector (releaseBlank), the hysteretic GATE_RMS silence gate (still measured on `v->dbuf` over corrWin, so the device calibration holds), and the decay-tail freeze. | — | — |

Interface notes:

- `pitchConf` still reports the honest clarity, so the shifter's 0.92 relock bar and the synth's
  tonal gate keep their meaning.
- One new `dbgGate` value exists: **6** = waiting for enough post-pick data. The shifter only tests
  `dbgGate == 2`.

**Real-time safety:**

- No allocation, locks or syscalls, and no recursion.
- All state is file-static: six ~12 KB rings, ~16 KB of shared scratch, no big stack arrays. v1
  put ~5 KB on the audio stack per call.
- `trackPitch` runs to completion on the audio thread and is never re-entered.
- Host builds are clean under ASan and UBSan across every scenario.
- `-Wall -Wextra` is clean.
- `#error` if `DECIM != 4`.

**CPU** (host op count, correlation-loop multiply-accumulates, all scenarios):

| | mean per call | worst call | worst 64-sample block, 6 strings |
|---|---|---|---|
| v1 | 9.1 k | 65 k | 148 k |
| v2 | 3.9 k | 51 k | 130 k |

v2 additionally runs its front end: 2 biquads plus a power follower per sample per string, about
1.5 k flops per call. That front end is not in the op count. Host microseconds are not
representative of the A53, so treat the op counts as the transferable number.

---

## 5. Before / after (harness numbers)

**Material.** Additive stiff-string plucks (`add`) and Karplus-Strong (`ks`) through a velocity pickup
2.2 cm from the saddle, with:

- inharmonicity rising with fret,
- pick-position comb,
- +6 c pick glide,
- an aperiodic pick scrape plus a 40 Hz thump,
- device-level amplitudes, with the top two strings -5 dB,
- a 1e-4 noise floor,
- adjacent-string bleed swept from -20 to -30 dB (next-but-one 14 dB lower).

**Tuning:** C-F-Bb-Eb-G-C, the `OPEN_HZ` from `render.cpp`.

**Definitions.** "Correct" = `v->period` within 35 cents of the truth and staying there for 40 ms.
"Lock" = correct, plus `pitchConf ≥ 0.92` and `attackBlank == 0` (the shifter's relock condition;
the shifter then needs 3 such hops). Latency is from the pick instant in the source.

### Per-string pick-to-correct latency, single notes, `add`, -23 dB (ms)

Frets 0 / 3 / 5 / 7 / 10 / 12 / 15 / 19 / 22:

| string | v1 | v2 |
|---|---|---|
| s0 C 65 Hz | 49.2 46.0 45.7 45.4 39.3 41.9 38.7 35.5 32.3 | 31.7 28.5 25.4 22.2 21.9 24.5 21.3 15.2 12.0 |
| s1 F 87 | 38.0 34.8 34.5 34.2 28.1 30.7 30.4 27.2 26.9 | 23.5 23.2 20.0 19.7 16.5 19.1 13.0 12.7 12.4 |
| s2 Bb 117 | 32.7 29.5 29.2 28.9 28.6 28.3 25.1 24.8 24.5 | 18.2 17.9 17.6 14.4 14.1 13.8 10.6 10.3 10.0 |
| s3 Eb 156 | 30.3 27.1 26.8 26.5 26.2 25.9 25.6 25.3 25.0 | 15.8 15.5 15.2 12.0 11.7 11.4 11.1 10.8 7.6 |
| s4 G 196 | 27.8 27.6 47.6 27.0 29.6 26.4 23.2 22.9 22.6 | 13.3 13.0 12.7 12.4 9.3 9.0 8.7 8.4 8.1 |
| s5 C 262 | 42.8 25.1 24.8 27.4 **506.1** 23.9 26.6 23.4 23.1 | 8.0 10.6 10.3 10.0 9.7 9.4 9.1 8.8 5.6 |

The low C open resolves in 31.7 ms, about 2.07 of its 15.3 ms periods. That is the regime every hex
converter lives in: TriplePlay claims 14 ms on its low notes, whose tuning is higher than this low C.

### Summary across scenarios (p50 / p90 / max ms, "never" = never correct)

| scenario | v1 add | v2 add | v1 ks | v2 ks |
|---|---|---|---|---|
| single, -23 dB | 28.1 / 45.4 / 506 | 12.7 / 23.2 / 31.7 | 33.9 / 60.6 / 81, never 7 | 12.7 / 23.2 / 31.7 |
| strum, -20 dB | 28.6 / 50.7 / 312 | 15.4 / 24.4 / 33.3 | 30.5 / 45.8 / 86, never 14 | 15.6 / 24.7 / 33.3 |
| strum, -23 dB | 28.0 / 48.7 / 299 | 15.0 / 24.0 / 33.3 | 29.2 / 43.6 / 74, never 12 | 15.1 / 24.7 / 33.3 |
| strum, -30 dB | 27.8 / 48.1 / 290 | 14.5 / 24.0 / 33.3 | 29.6 / 42.3 / 78, never 10 | 14.5 / 24.2 / 33.3 |
| legato | 24.9 / 39.1 / 45, never 1 | 16.3 / 27.7 / 33.4 | 30.4 / 42.0 / 75, never 10 | 13.9 / 22.0 / 24.7 |
| repeat (100 ms) | 26.6 / 44.4 / 47 | 17.8 / 31.2 / 32.8 | 27.8 / 43.1 / 63, never 7 | 15.5 / 24.9 / 26.7 |
| 3× pick scrape | 31.2 / 47.7 / 506 | 14.1 / 28.7 / 32.7 | — | — |
| notes at -10 dB (0.3×) | 27.8 / 45.4 / 506 | 13.0 / 23.2 / 34.4 | — | — |

Octave errors while confident were **0 in every run, v1 and v2**.

"Visited a wrong note before settling" was:

- v2: 0 in every scenario except one 12 ms blip under a 3× pick scrape
- v1: 0–3 per scenario

### Crosstalk: strings carrying only a neighbour's bleed (single-note test, `add`)

| adjacent bleed | v1 confident-lock hops | v1 retunes | v2 confident-lock | v2 retunes |
|---|---|---|---|---|
| -30 dB | 23.8 % | 105 | 0 | 0 |
| -23 dB | 38.5 % | 167 | 0 | 0 |
| -20 dB | 43.4 % | 188 | 0 | 0 |
| -17 dB (= v2's model) | 48.9 % | 213 | 1 hop | 0 |
| -15 dB (beyond the model) | 55.8 % | 236 | 15 hops | 10 |
| -12 dB | 59.9 % | 252 | 1.2 % | 100 |

Palm-mute scratches (aperiodic bursts) and noise only: 0 confident locks for both v1 and v2.

### The other side of the bleed rule: a quiet real note between two loud neighbours

The test plays a quiet real note between two louder neighbours sounding different notes. The table
gives pick-to-correct p50 / p90 in ms, with the number of notes never correct out of 24.

| target level vs neighbours | v1 add | v2 add | v1 ks | v2 ks |
|---|---|---|---|---|
| 0 dB | 28.8 / 37.2 | 15.3 / 22.7 | 30.7 / 37.2, never 2 | 15.5 / 22.7 |
| -6 dB | 33.6 / 41.9 | 17.3 / 24.5 | 31.6 / 39.0, never 5 | 16.1 / 22.7 |
| -12 dB | never 22/24 | never 14/24 | never 20/24 | 16.1 / 38.5, never 1 |
| -18 dB | never 24/24 | never 24/24 | never 21/24 | never 17/24 |

This is the price of the pessimistic coupling. A real note 12 dB under *both* neighbours is at the
hard bleed line and is often held back. v1 was worse here.

`PT_BLEED_C1` is **the knob to calibrate on the rig**. Measure the real worst-case adjacent bleed
(peak, not average, as Cycfi advises), then set C1 about 3 dB above it.

### Steady state (all six open strings ringing, -23 dB bleed, tuned 25 c flat)

| string | v1 mean / sd / range (cents) | v2 mean / sd / range (cents) |
|---|---|---|
| s0 | +1.5 / 2.1 / -4.5..+6.1 | +1.9 / 1.9 / -2.7..+6.1 |
| s1 | +2.2 / 2.4 / -3.6..+7.6 | +1.6 / 1.4 / -2.0..+4.3 |
| s2 | +2.4 / 2.1 / -1.7..+8.0 | +1.3 / 0.8 / -0.6..+3.0 |
| s3 | +1.5 / 2.5 / -8.7..+8.4 | +0.8 / 2.2 / -28..+12 (one spike in the bleed-dominated tail) |
| s4 | +3.1 / 3.7 / -6.4..+13.3 | +1.1 / 1.1 / -3.1..+4.2 |
| s5 | +3.2 / 3.4 / -4.1..+14.0 | +1.2 / 0.55 / -0.9..+2.7 |

Without bleed, v2's hop-to-hop period step is 0.04–0.15 c.

The existing `hosttest/run_tests` suite (10 checks: accuracy, note-change snap, re-pick stability,
COLA evenness, formant, LPC) passes unchanged with v2.

---

## 6. What is NOT verified

**Everything above is synthetic.** It is evidence that the logic does what it claims on signals with
the documented failure modes of guitar. It is not proof that it works on the SubSix.

- **Real pick transients** are messier than the model: finger noise, fret buzz, pick angle, string
  scrape on wound strings, palm-mute thumps, harmonics, and dead notes. `PT_SKIP`, `PT_WMIN_FRAC`
  (0.75; 0.5 was faster but less safe) and `PT_ACCEPT` were tuned on synthetic material.
- **The real bleed** is not a scaled copy. It may be filtered, phase-shifted, or partly mechanical,
  and the -23/-37 dB figure is one measurement from one pluck. The coupling constants need a rig
  calibration.
- **`shifterOnset` timing on the rig.** The retrospective localizer assumes the pick is the loudest
  first-difference burst in the 25 ms before the stamp.
- **The beading diagnosis** is proven in simulation only. The shifter-side remedy was tried only as a
  rho = 2 special case in a scratch copy.
- **CPU on the Cortex-A53.** Only host op counts were measured. The worst call is lower than v1's, but
  it still has not run on the Bela, and the full-chain `hosttest/bench` does not currently build (see
  "Files").
- **Not listened to.** None of this was heard. The owner plays through the rig live, and nothing was
  deployed.

**Suggested on-rig check, before trusting it on a gig:**

1. Deploy with the rest of the next build.
2. Pluck each string open and at the 5th/12th/17th fret, with pitch +12 on.
3. Listen for the relock point (the TRANS dry attack handing over to shifted audio).
4. Strum open chords and confirm that idle strings stay silent in the shifted mix.
5. Mute one string and hammer its neighbours.
6. `dbgGate` values to watch:
   - 2 on a bleed-only string is intended
   - 6 means waiting for post-pick data
   - a string stuck in 3 or 4 means it never gets a clean period

---

## 7. Files

**Changed:**

- `bela/hyperGuitar6chPan/pitch_tracker.cpp`: rewritten (v2).

**Added:**

- `bela/archive/pitch_tracker_v1.cpp.txt`: v1, verbatim. It is not compiled: it sits outside the
  project directory and `deploy.sh` only copies top-level `*.cpp`.
- `bela/hyperGuitar6chPan/hosttest/pitchbench/pitchbench.cpp` and `Makefile`: the harness.
  - Scenarios: `single strum legato repeat bend silent steady contest`
  - Materials: `add` or `ks`
  - Bleed dB is swept on the command line.
  - Environment variables:
    - `PB_VERBOSE` (per-note lines)
    - `PB_TRACE=s:fret` (hop-by-hop trace)
    - `PB_ORACLE=1` (feed the true period, steady scenario)
    - `PB_BSCALE` (scale inharmonicity)
    - `PB_PICKNOISE`, `PB_AMP` (stress the pick scrape / note level)
  - Build targets: `make` builds v2, `make old` builds v1 from the archive, `make ops` builds the
    op-count build.
- This note.

**Rebuilt, not edited:** `hosttest/run_tests`, the tracked test binary, is now linked against v2.

**Untouched:**

- `hyperguitar.h`: no new declarations were needed.
- `render.cpp`, `dsp_core.cpp`, `shifter.cpp`, `synth.cpp`, `presets.cpp`, `effects.cpp`,
  `trellis_ui.cpp`, `hg_*`.

**`hosttest/bench` does not build right now.** `render.cpp` now reads
`context->audioOutChannels`, which the bench's Bela stub lacks. This is unrelated to the tracker,
and it was left alone because `render.cpp` is being edited in parallel.
