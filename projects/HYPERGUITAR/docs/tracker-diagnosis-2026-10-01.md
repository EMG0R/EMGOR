# hyperGuitar per-string pitch tracking: diagnosis and improvement plan

2026-10-01. Read-only analysis of `bela/hyperGuitar6chPan/{pitch_tracker,shifter,dsp_core,render}.cpp`, `docs/pitch-tracker-overhaul.md`, and 212k lines of live `rt_printf` diag (`bela-live-diag.txt`). Mining script: `mine_diag.py` (`--tail 110000`, `--extra`), latency model: `latency_model.py`, raw outputs `mine_out.txt`, `mine_extra_out.txt`. All percentages below are from the **last 110 000 lines** (one continuous process run, 0 restarts) unless marked ALL.

## 0. The one finding that reframes the rest

**The log contains no trace of tracker v2.** v2 reports bleed as `dbgGate 2` *while* `E` (dbgE0) is above the silence gate, and uses a new code `g6`. In 212k lines there are **0** `g6` lines and only 0.0-0.3 % of `g2` lines per 10k-line chunk have `E >= 9e-8` (v1's `g2` is by definition `E < gate`). The whole log, including the "recent" tail, is **v1** (NSDF on dbuf, fixed 6-hop blank, 2-hop consensus, no bleed model) feeding the current shifter. The v2 doc itself says "host-verified only, nothing deployed". So: everything the player feels today is v1, and v2's synthetic numbers are still unverified on the rig. The first deliverable is therefore not a new algorithm; it is **deploying v2 and re-running `mine_diag.py` as the acceptance test**, with the fixes below layered on.

## 1. What the live log says (tracker v1 + current shifter)

`->TRANS` is never printed (shifterOnset flips the mode per-sample; the hop printer only sees exits), so **TRANS exits = onsets**.

### 1a. Attacks: how the dry pass-through hands over to shifted audio

| string | onsets | relocked clean (rl=3, ~3 hops after lock) | exit at rl=0 (pitch-idle path >=7 hops, or dead zone >=26 hops) | rl 1-2 (dead zone) | fell to DRIFT (>=19 hops, splicer) | DRIFT falls that were SILENT (ghost onset) |
|---|---|---|---|---|---|---|
| s0 C  | 13223 | 58 % | 30 % | 3 % |  9 % | 13 % |
| s1 F  | 11993 | 44 % | 41 % | 3 % | 11 % |  6 % |
| s2 Bb | 16439 | 53 % | 37 % | 4 % |  7 % | 49 % |
| s3 Eb | 13827 | 65 % | 23 % | 5 % |  7 % | 73 % |
| s4 G  | 14646 | 52 % | 28 % | 4 % | 16 % | 63 % |
| s5 C  |  8432 | 43 % | 34 % | 5 % | 18 % | 65 % |

Only 43-65 % of attacks hand over on the fast path. Of the TRANS->DRIFT falls, 49 % had `nl > 18` (>= 55 ms of conf < 0.45) and 51 % were dead-zone exits (>= 75 ms in TRANS). On the plain strings, 16-18 % of attacks end in the period-repeating splicer, and two-thirds of those were on a string carrying no note (E below the gate): **ghost onsets**. Overall 26 % (s4) and 32 % (s5) of onsets end with `E < GATE_RMS^2` vs 6-8 % on s0/s1 ([H3]). The onset detector (`envF > 2.2 envS + 4e-4`, dsp_core.cpp:133) fires on a neighbour's -23 dB bleed because the plain strings' own level (env ~0.02) makes the 4e-4 floor negligible against a wound-string pick. Each ghost arms a 50 ms `onsetRefract`, so **a real pick within 50 ms of a neighbour's pick (every strum, every arpeggio) is not registered as an onset at all**: no TRANS, no attackBlank, no picker reset, and v2's post-pick window would start at the ghost, 5-40 ms of bleed-only data early.

### 1b. Co-locks: one physical note, several trackers

Strict test: two **confident** (c >= 0.92) `md` lines on *adjacent* strings within 12 log lines with T within 2 %.

| string | confident transitions | strict adjacent unison co-lock | top shared T (pair) | most frequent confident notes (absolute, mean cents) |
|---|---|---|---|---|
| s0 C  | 9161 | 44 % | T372/T396 (s0-s1) | Bb2 +26, A2 +15, C2 +28 |
| s1 F  | 8116 | 57 % | T372/T396 (s0-s1) | F2 +33, A2 +16, Bb2 +26 |
| s2 Bb | 10749 | 46 % | T163/T244 (s2-s3) | Ab3 -33, F#3 -35, **C#4 -31** |
| s3 Eb | 10182 | **75 %** | **T163** x1256 (s3-s4) | **C#4 -36 (20 %)**, F#3 -37, B3 -36 |
| s4 G  | 9324 | **72 %** | **T163** x767 (s4-s5) | **C#4 -35 (15 %)**, C4 +37, F#3 -31 |
| s5 C  | 5126 | 51 % | T163 (s4-s5) | **C#4 -33 (27 %)**, C4 +41, B3 -33 |

T163 (270 Hz, "C#4 -35c", the open top string in this segment) is the single most frequent confident lock on **four of six strings**, always at the same -31..-36 cent intonation offset, i.e. the same physical vibration. s3 locks to the top string's open note (20 %) four times more often than to its own open E3-41c (5 %). No fingering produces that. These are bleed locks, exactly the v1 failure the v2 doc predicted (38-49 % of bleed-only hops confidently locked at -23..-17 dB). Octave (2:1) co-locks are small by comparison (1-9 %). Median E of a co-locked s4/s5 transition is 1e-6..5e-6 (-57 dB FS), 20-25 dB under a real pick on the same string ([H4]): quiet strings are the victims, loud ones the sources. Side note: the "recent" segment's open strings sit at Db-35c (C+65c), the earlier segment at C+13..+25c; `TUNE_MARGIN` only covers flat, and `OPEN_HZ` says C, so the per-string prior is loose but not violated.

### 1c. Mid-note dropouts and misses

- GRAINS->DRIFT while audible: 5.5-13.6 % of GRAINS->DRIFT events have `g0 && E >= gate`; 85-94 % are `g3/g4` (no clean period) with median E 1.3e-7, i.e. decaying tails just above the gate. Steady-state `st` snapshots while sounding are 76-93 % GRAINS, 0-3.5 % DRIFT, c >= 0.92 on 66-91 % (s1 and s4 worst at 66/79 %). Mid-note DRIFT is **not** the dominant complaint any more (the 09-10 dwell/hysteresis fixes hold).
- Octave errors: confident T an octave above the open string 1.5-6 %, consistent with real 12th-fret playing; no systematic octave failure in the log.
- "Below open": 8-11 % on s4/s5 vs <1 % on s0/s2/s3 are neighbour notes (F#3 -35c on the G string), i.e. co-locks again, not tuning.

## 2. Pick-to-shifted-audio latency as implemented

Stages (code): onset detector -> (v2) tracker acceptance needs `PT_SKIP 48 + 1.75 T` samples of post-pick data (`PT_WMIN_FRAC 0.75` behind a lag of T), single-hop accept only if 2T was evaluable (3.5 T) else +1 agreeing hop; `attackBlank` is cleared on acceptance (no cost if accepted < 6 hops); shifter needs `relockHops >= 3` = acceptance hop + 2 (5.8 ms); `transHoldUntil` = onset + 10 ms always (strum coherence); `grainMix` ramps 1/max(T,96) per sample: -3 dB at T/2, full at T. `latency_model.py`, ms from pick, v2 tracker:

| string | T (open) | tracker accept | relock (TRANS->GRAINS) | shifted -3 dB / full | 12th fret: accept / relock / full |
|---|---|---|---|---|---|
| s0 C 65 Hz | 674 | 31.9 | 40.6 | 48.3 / 55.9 | 17.4 / 23.2 / 30.9 |
| s1 F  | 505 | 26.1 | 31.9 | 37.7 / 43.4 | 14.5 / 20.3 / 26.0 |
| s2 Bb | 378 | 20.3 | 26.1 | 30.4 / 34.7 | 11.6 / 17.4 / 21.7 |
| s3 Eb | 283 | 14.5 | 20.3 | 23.5 / 26.7 | 11.6 / 17.4 / 20.6 |
| s4 G  | 225 | 11.6 | 17.4 | 20.0 / 22.5 |  8.7 / 14.5 / 17.1 |
| s5 C 262 Hz | 169 |  8.7 | 14.5 | 16.4 / 18.3 |  8.7 / 14.5 / 16.7 |

Onset detection itself is 0.2-0.8 ms on a clean step (simulated) but the code's own measurement is +16 ms on a low C whose fundamental builds slowly; v2's `localizeOnset` back-dates it, v1 does not. **v1 (what is running):** 17.4 ms blank + 2-open-period window flush + 2 hops = 28-50 ms to a correct pitch (doc, measured on the harness), + 5.8 ms relock + fade -> **35-60 ms** to shifted audio on single notes, 506 ms outliers on s5. Then the paths in 1a: 23-41 % of attacks exit at >= 20-75 ms and 7-18 % at >= 55 ms into the splicer. Reference points: GR-300 3.3 ms (PLL waveform following, no pitch number), Yamaha G50/Axon 11.1-11.7 ms, TriplePlay 7-14 ms claimed, Derrien ~1 period (12 ms at 82 Hz), Fonseca 1.1 periods, Roland patent 1.5 periods with two agreeing measurements. v2's 8.7-31.9 ms accept is at the Reboursiere "2 periods" floor; the **shifter adds 6-24 ms on top** (relock + hold + fade), and the 10 ms strum hold is binding on the top three strings. The 3.5 T single-hop rule costs a hop on most fretted notes.

## 3. The bleed model vs the pickup

Facts found (sources below): the pickup is a **Submarine Pickups SubSix** (magnetic, 12 N42 magnets, six micro-coils), not a Cycfi product; SOS reports "a little crosstalk between adjacent strings", no dB figure published. Cycfi's GK-5 measurement (the only published hex crosstalk numbers): peak -23.4 dB worst, -40 dB best, averages -38.6..-50.6 dB; "peak crosstalk is what really matters"; recommended gate "about 12 dB below the loudest excitation". Cycfi Nu v2 spec: -49 dB at 10.2 mm spacing. The rig's own single measurement (render.cpp comment): -23 dB adjacent, -37 dB next-but-one, matching the GK-5 worst case. No published real-time inter-channel canceller exists for magnetic hex pickups (GuitarSet's KAMIR is offline).

v2's constants: `PT_BLEED_C1` -17 dB (6 dB pessimistic), `PT_BLEED_HARD` +3 dB, `PT_BLEED_SOFT` +9 dB unison-only. With C1 = -17 dB, a string must be > -14 dB relative to one adjacent neighbour (or > -11 dB relative to two) to start a note at all. The doc's own contest test: a real note -12 dB under both neighbours never resolves in 14/24 cases. On this pickup the two plain strings are **~4.6 dB quieter** than s0 in steady state (median env 0.020-0.022 vs 0.034; median confident E 1e-6..7e-6 vs 8.6e-4 on s0, i.e. 20-30 dB lower at transitions, partly because they are so often locking onto bleed). A lightly played G or high C under a hard-picked low C in an open chord is routinely 10-14 dB down: v2's hard rule will **silence quiet real notes in chords** on exactly the strings the player complains about, while -17 dB is only 6 dB above the measured peak, so loud transients will still leak through the soft (unison-only) rule. The model is also unscaled per channel: it compares raw powers, so the plain strings' lower sensitivity is read as "more buried".

**5-minute calibration on the rig (no restart, it already prints `md`/`st`):**
1. Mute all strings but one with the fretting hand; pick that string hard 5x at the 5th fret, wait for decay. Repeat for all six (3 min). The `md` lines on the *other* five strings during each pluck carry `E` (dbgE0): peak bleed power relative to the source string's own `E` is the coupling; take the **max** over the five plucks, per (source, victim) pair; next-but-one pairs too. Expect -20..-28 dB adjacent on a bar pickup.
2. Play one open chord (C-F-Bb-Eb-G-C) soft, then hard; note the per-string `st` `e` (env) spread. That spread (expected 4-8 dB plain vs wound) is the per-channel gain trim.
3. Set `C1` = measured max adjacent + 3 dB, `C2` = measured next-but-one + 3 dB, and multiply each string's published `gPw` by its gain trim (plain strings up). Re-mine: `[H4]` strict co-lock should fall under 10 % and `[H3]` ghost-onset share under 10 %.

## 4. State of the art, judged for a Cortex-A53 at block 64, 6 channels

| approach | what it buys | cost on A53 | verdict |
|---|---|---|---|
| v2 NSDF, post-pick window, range prior (Reboursiere/McLeod) | ~1.75-2 periods, 0 octave errors on harness, 3.9k MAC/call mean | fine (v1 was 9.1k and ran) | **deploy first**; it is written |
| Cycfi zero-crossing-guided period detector + BACF, onset-predicted first estimate | ~1 period, 47-59 ns/sample on x86; bitwise XOR/popcount; harmonic bias stage | trivially cheap | best candidate for the **first estimate** (publish a provisional period at 1 period, confirm at 2) |
| Roland peak-hold-then-zero-cross (US5018427/8), LPF at 4x open | 1-1.5 periods, inherently bleed-resistant (own peaks dominate the decaying hold), no correlation | ~10 ops/sample | strong candidate for a **crosstalk-robust confirmation path**; needs per-string hysteresis tuning |
| GR-300 PLL waveform following | 3 ms, no pitch number | cheap | only usable for a *resampling* shifter that just needs a splice length: the PLL period is noisy for several periods; **not** for PSOLA grain spacing before lock |
| Axon transient-pattern NN (US5929360) | sub-period from bridge/fret reflections | small NN, but needs piezo-like transient capture and training | research only; magnetic bar pickup smears the reflection pattern |
| Derrien ESPRIT + harmonic likelihood | ~1 period, few ghost notes | SVD per hop per string: too heavy at block 64 | no |
| adaptive LMS crosstalk canceller (victim -= sum w_k * source_k) | removes in-phase magnetic bleed before tracking; 2 taps/neighbour | 12-24 MAC/sample/string, cheap | **worth a bench test**: magnetic bleed is near-instantaneous and linear, so a 2-5 tap LMS with the loud neighbour as reference should give 10-20 dB; GuitarSet/KAMIR confirms bleed is additive and removable |
| NMF/ICA | full separation | frame-based, >1 period latency, heavy | no |

## 5. Why it "doesn't feel good", ranked

1. **Bleed co-locks (v1 has no bleed model).** 72-75 % of s3/s4 confident transitions share a neighbour's period; the top string's open note is the most frequent lock on four strings. Audible as: shifted output on a quiet string following the loud string's pitch, chords collapsing into the loudest note's octave/formant, quiet strings re-tuning mid-note. Evidence: 1b, doc section 5.
2. **Hand-off latency dry -> shifted.** Clean path 17-56 ms (open) with the shifter stage adding 6-24 ms; 35-60 % of attacks take the slow paths (>= 20-75 ms, or the splicer). The player hears his own dry pick, then the shifted note blooms in late and sometimes not at all. Evidence: 1a, section 2.
3. **Ghost onsets on the plain strings + 50 ms refractory.** 26-32 % of s4/s5 onsets are on a string with no note; the refractory then eats real picks in strums, so the real pick of a quiet string is often never an "onset". Evidence: [H3], dsp_core.cpp:133-135.
4. **Bleed suppression of quiet real notes** (v2 risk, not yet live): -17 dB coupling with no per-channel gain trim will hold back soft notes under loud wound strings. Evidence: doc contest table, env spread.
5. Mid-note DRIFT dropouts: now 0-3.5 % of sounding snapshots; the 09-10 fixes worked. Residual: s1/s4 at 14 % / 8 % of snapshots with c < 0.45.
6. Octave/harmonic errors: 1.5-6 % octave-above-open, within normal playing; no evidence of a systematic octave problem in the log.

## 6. Improvements, ordered

| # | change | expected gain | cost | risk |
|---|---|---|---|---|
| 1 | **Deploy v2 as-is** (drop-in, owner's permission to restart), then re-run `mine_diag.py --extra`; acceptance = `g6` lines appear, `g2` with `E >= 9e-8` appears, [H4] co-locks drop from 70 % toward < 15 %, [D] rl=3 share rises | removes finding 1 for bleed <= -17 dB; pick-to-correct 28-50 -> 9-32 ms | 0 code; one restart | CPU unmeasured on A53 (worst call 51k vs v1 65k MACs, should be fine); PT_* tuned on synthetic material |
| 2 | **Calibrate the bleed model** per section 3 and add per-string gain trims to `gPw` (plain strings +4..6 dB) | stops v2 from muting soft notes in chords; tightens C1 to the real -23..-26 dB | 30 lines + 5 min at the bench | none if numbers come from the rig |
| 3 | **Relative onset gate** (Cycfi): require `envF > 2.2 envS + 4e-4` AND string pw > predicted bleed x 2 (reuse `predictedBleed`), or compare envF with the loudest string's envF - 12 dB; shorten `onsetRefract` to 20 ms | kills most ghost onsets; real picks in strums register again, so v2's post-pick window starts at the pick | ~15 lines dsp_core.cpp | a 12 dB relative gate can miss a soft pick simultaneous with a hard one; keep the absolute path as fallback |
| 4 | **Shorten the shifter hand-off**: relock on 2 hops when v2 reports a post-onset snap (ACQ->LOCK is already a 0.90-confidence decision), drop `transHoldUntil` to 5 ms or make it neighbour-only, start `grainMix` fade at relock-1 hop | -3..-9 ms per attack on every string; the top strings move from 14.5 -> 9-11 ms | ~10 lines shifter.cpp | more exposure to a wrong first estimate: pair with 5 |
| 5 | **Provisional period at one period** (Cycfi/Roland): on ACQ, publish a provisional `period` from the first two zero-crossing/peak intervals after the pick (filtered ring already exists) with `pitchConf` 0.75; the grain legs and SOLAD can run on it, confirmed or corrected by NSDF a period later; glide the period, never step it | shifted audio starts ~1 T + 3 hops after the pick (s0 open: ~24 ms, s5 ~12 ms) | ~80 lines in pitch_tracker.cpp, cheap | a wrong provisional is audible as a 1-period pitch glide: acceptable on the up leg (soft), judge on the down/formant legs |
| 6 | **2-5 tap LMS bleed canceller** per adjacent pair on the tracker's analysis input only (not the audio path), reference = neighbour's raw channel, step size gated to when the neighbour is >= 10 dB louder | +10..20 dB effective isolation; lets C1 be relaxed and quiet notes through | ~24 MAC/sample/string; ~1 % of block budget | convergence during transients; bench on hosttest first with recorded hex stems |
| 7 | **Peak-hold + zero-cross confirmation** (Roland) as a second opinion in `matchesLouderNeighbour`: a bleed-only channel's own peak train is the neighbour's, a real note's is not | catches co-locks the power model misses (loud transients) | ~40 lines | tuning per string |
| 8 | Instrument the log: print a `on s<str> ...` line at shifterOnset with `gSampleStamp`, and the stamp on `md` lines, so attack latency is measured on the rig instead of modelled | makes every number above measurable | 2 rt_printf | log volume (keep the 2 s `st` cadence) |

Not recommended now: ESPRIT, NMF/ICA, a GR-300 PLL as the primary tracker, and any change to `GATE_RMS` or the dwell floors (the mid-note flapping fixes hold in the log).

## Sources

- Cycfi, "Roland GK crosstalk deep dive" (2026-02): https://www.cycfi.com/2026/02/roland-gk-crosstalk-deep-dive/ ; Nu v2 capsule -49 dB spec: https://www.cycfi.com/projects/nu-v2-capsule/
- Submarine Pickups SubSix review (SOS): https://www.soundonsound.com/reviews/submarine-pickups-subsix
- Cycfi pitch detection series: https://www.cycfi.com/2018/03/fast-and-efficient-pitch-detection-bitstream-autocorrelation/ , https://www.cycfi.com/2018/06/fast-and-efficient-pitch-detection-synth-tracking/ , https://www.cycfi.com/2020/07/fast-and-efficient-pitch-detection-revisited/ , https://www.cycfi.com/2021/02/fast-and-efficient-pitch-detection-power-of-two/
- Derrien, DAFx-14: https://hal.science/hal-01089401/document ; Reboursiere et al., NIME 2010: https://www.nime.org/proceedings/2010/nime2010_415.pdf ; Fonseca & Tavares, SBCM 2019 (1.1-period bass tracker): https://sol.sbc.org.br/index.php/sbcm/article/download/10433/10300/
- Converter latencies: https://gr300.com/MIDI_SPEED.htm ; GR-300 PLL: https://en.wikipedia.org/wiki/Roland_GR-300 ; Roland patents US5018428A, US5018427A: https://patents.google.com/patent/US5018428A/en , https://patents.google.com/patent/US5018427 ; Axon US5929360A: https://patents.google.com/patent/US5929360A/en ; AX100 review: https://www.soundonsound.com/reviews/blue-chip-axon-ax100
- TriplePlay: https://fishman.com/tripleplay-support/ ; Jamstik 6-15 ms: https://jamstik.com/pages/how-it-works
- Vetter helmholtz~ (SNAC) and low-latency splicing: https://www.katjaas.nl/helmholtz/helmholtz.html , https://www.katjaas.nl/pitchshiftlowlatency/pitchshiftlowlatency.html
- GuitarSet / KAMIR offline bleed removal: https://ismir2018.ircam.fr/doc/pdfs/188_Paper.pdf
- Repo: `docs/pitch-tracker-overhaul.md`, `pitch_tracker.cpp` header, `shifter.cpp` 610-812 and 902-944, `dsp_core.cpp` 86-135, `render.cpp` 176-190.
