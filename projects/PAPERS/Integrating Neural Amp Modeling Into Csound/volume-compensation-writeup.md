# Dry-Referenced Live Volume Compensation in `NAMProcess`

Technical writeup + paper-integration notes. Written 2026-07-09, after the
compensation system shipped and passed its verification battery. Companion
document: `cabbage-plugin/archive/volume-compensation-history.md` (the
approach-by-approach experimental log this summarizes).

**Framing (author directive, 2026-07-09): the LIVE dry-referenced system is
the method.** Everything pre-determined — per-model calibrated input gains,
load-time probes as the mechanism, absolute-target riders — is failed
history (Section 2) and appears in the paper only as compressed motivation.
The load-time probe survives in the code solely as a first-guess seed for
unseen models (Section 1.5); it is not part of the method's claim.

---

## 1. What shipped, precisely

All of the following lives **inside the opcode** (`csound_nam_opcode.cpp`,
struct `NamCompState`, plus the load-time seed in
`nam_loader.cpp::measure_loudness_db` and the shared K-weighting filters in
`kweight.h`). Nothing depends on the Cabbage plugin; the Raspberry Pi
deployment gets identical behavior from the same `.so`.

### 1.1 The core idea: the dry signal is the loudness reference

Every previous attempt normalized models toward an *absolute* target
loudness (e.g. "make every capture measure −14 dB on a probe"). The shipped
system replaces the absolute target with a *relative* one: **the wet output
should be exactly as loud as the dry input the player is producing.** The
dry signal is what the performer hears with the amp bypassed; matching it
means inserting or swapping any model never changes perceived volume — the
definition of "usable side by side" that actually matters live.

Signal topology inside one `NAMProcess` k-cycle:

```
ain (TRUE DRY, the reference)──────────────► dry loudness meter
  │
  ├─ × in_gain (per-model pad / calibrated pregain)
  ├─ × 10^(kDriveDb/20)          (user drive — pre-model)
  ▼
NAM model (per-channel instances)
  │
  ├─ × out_gain (static seed makeup, §1.5)
  ├────────────────────────────────────────► wet loudness meter
  ▼
  × 10^(trim_db/20)              (live compensation — post-model)
  ▼
aout
```

Crucially the dry meter taps the input *before* `in_gain` and the drive, and
the wet meter taps *after* the model and static makeup but *before* the live
trim. The trim target is therefore feed-forward (it does not measure its own
effect), so the control loop cannot oscillate.

### 1.2 Metering: K-weighted, long-window mean square

Both meters are the mono mix `(L+R)/2` passed through the two-stage
ITU-R BS.1770 K-weighting prefilter (a +4 dB high shelf at 1681.97 Hz,
Q 0.7072, followed by a high-pass at 38.135 Hz, Q 0.5003; RBJ biquads
recomputed for the engine sample rate — `kweight.h`). K-weighting
approximates perceived loudness far better than raw RMS across bright vs
dark captures, which was one measured failure mode of the original 1 kHz
sine approach.

The K-weighted mean square of each block is folded into an exponential
moving average with time constant **τ ≈ 2.5 s** — a "wide average" by
design. Loudness here means "how loud has the playing been lately," not
"how loud is this instant."

### 1.3 Why it is not a compressor: the ratio cancels program dynamics

The trim target is the *ratio* of the two averages:

```
target_dB = 10·log10( dry_ms / wet_ms )
```

When the player digs in or backs off, dry and wet loudness move together
(to first order), so the ratio — and therefore the trim — does not move.
Program dynamics pass through untouched; only the *model's* gain
relationship to the dry signal is corrected. The residual second-order
effect (a saturating amp compresses more when driven harder, slightly
changing the ratio with playing level) moves the trim only at the slew
rate below, i.e. inaudibly slowly.

Four additional guards make the trim imperceptible as a dynamic process:

| Guard | Value | Purpose |
|---|---|---|
| Slew cap | 2 dB/s max | gain never moves fast enough to read as compression or pumping |
| Deadband | 0.75 dB | converged trim sits still; no perpetual micro-riding |
| Silence freeze | input block < −55 dBFS | silence never decays the dry reference or drags the trim |
| Clamp | ±18 dB | a pathological capture cannot run the gain away |

A warm-up condition (≥ 1 s of active signal on both meters, after model
load) prevents the trim from moving on startup transients.

### 1.4 Per-model trim memory

The converged trim is remembered per model *file path*
(`std::map<std::string,double>` in the opcode instance). On a model swap the
old model's trim is stored, the new model's remembered trim is restored (or
the current trim is kept as the best guess for an unseen model), and the wet
meter and its K-filters are reset. Switching back to a previously played
model is therefore instantly at the right level — measured +0.6 dB
immediately after a swap-back, inside the deadband, with no re-convergence
dip.

### 1.5 The load-time probe survives only as a seed

The static probe (in `NamLoader`) still runs once per model load, off the
audio thread, and sets the initial `out_gain`: the official NAM reference DI
(`NeuralAmpModelerCore/example_audio/input.wav`, 2 s of real guitar — the
very signal captures are trained against — embedded in the binary as a
float array) is scaled to −18 dBFS RMS, pushed through the freshly loaded
model, K-weighted, measured in 100 ms blocks with a BS.1770-style −10 dB
relative gate, and the makeup toward the target is applied at swap time.
Its only job now is to make the *first second* of an unseen model land
close; the live loop owns the result. (Level-matching the probe to a
typical playing level matters more than using the file's hot native
−12 dBFS — probing hot mis-ranks linear vs compressing captures; see §2,
rows 7–8.)

### 1.6 The linked drive: `kDriveDb`

The opcode signature gained two optional trailing arguments:

```
aoutL, aoutR NAMProcess ainL, ainR, kProfileIdx, SLibDir [, kAutoVol=1, kDriveDb=0]
```

`kDriveDb` is a pre-model gain in dB applied *after* the dry meter tap.
Because the compensator holds the output at dry loudness, turning drive up
pushes the model harder — more saturation — while the post trim
automatically pulls the level back. Measured: **+18 dB of drive changed
output loudness by +0.1 dB while crest factor dropped 4.6 dB** (the
signature of added saturation). This is the only "knob that talks to the
model" available in NAM today — the format has no parametric conditioning —
and the compensation makes it usable as a pure distortion control.
`kAutoVol = 0` disables both the static makeup and the live trim (raw
model level), for A/B honesty.

Backward compatibility: existing 4-argument call sites (the Pi pedal
orchestra) compile unchanged and get `kAutoVol=1, kDriveDb=0`, i.e. the new
behavior with no drive.

---

## 2. Experimental history (all numbers)

Benchmark used throughout: all 25 models of the curated capture library,
identical synthetic riff (110 Hz sawtooth plucks), peak 300 ms RMS of the
settled output; population stdev and total spread across the folder.

| # | Approach | stdev | spread | outcome |
|---|----------|------:|-------:|---------|
| 1 | 1 kHz sine probe @ −18 dBFS RMS, plain RMS meter (original) | — | — | audibly poor; user-rejected |
| 2 | guitar-chord multitone probe @ −18, K-weighted meter | 2.60 | 9.9 dB | best purely static probe |
| 3 | pluck-burst probe + relative gating | 3.29 | 10.7 dB | bursts under-drive compressing amps |
| 4 | #2 + naive RMS live rider (2.5 dB deadband) | 4.34 | 14.8 dB | unstable: chases decay tails |
| 5 | #2 + peak-tracking rider v1 | 2.17 | 8.3 dB | best rider; slow convergence |
| 6 | peak rider v2 (faster/tighter) | 3.22 | 12.9 dB | oscillates |
| 7 | official reference DI probe at native −12 dBFS | 6.09 | 18.8 dB | probe too hot: linear vs compressing amps diverge |
| 8 | DI probe scaled to −18 dBFS RMS | 4.46 | 14.6 dB | better; still material/level mismatch |
| 9 | **dry-referenced live compensation (shipped)** | **0.83** | **3.1 dB** | all 25 models within ±2 dB of the dry signal |

Two structural lessons fell out of rows 2–8, and both are worth stating in
the paper because they generalize beyond this system:

- **Loudness equalization is only valid near the probe's input level.**
  Saturating models compress; a probe hotter or colder than real playing
  mis-ranks clean vs driven captures (rows 7–8: the same real-guitar probe
  at −12 vs −18 dBFS swings folder spread by 4 dB). No single static probe
  can match all material.
- **Closed-loop riders on absolute targets are fragile on dynamic
  material** (rows 4–6): what the loop settles on depends on the amp's own
  envelope behavior. The fix is the *relative* (dry/wet ratio) target, which
  is invariant to program dynamics by construction.

Verification battery of the shipped system (CLI renders through the actual
built opcode, realtime-paced so the background loader behaves as deployed):

| Test | Result |
|---|---|
| Folder consistency, 25 models, same input | stdev 0.83 dB, spread 3.1 dB, every model within ±2 dB of dry |
| Drive sweep 0 → +18 dB mid-performance | loudness +0.1 dB; crest factor −4.6 dB (saturation up, volume flat) |
| Dynamics: −12 dB input step after convergence | −10.2 dB output step (residual is the amp's own compression; the trim's 2 dB/s slew cannot produce a 1.8 dB step) |
| Swap A→B→A memory | +0.6 dB error immediately at swap-back (inside deadband) |

The end-to-end AU render host additionally confirmed identical behavior
inside a real plugin host process (both arm64 and x86_64).

---

## 3. Paper integration — APPLIED 2026-07-09

`paper-draft.md` has been edited per the author directive. What changed:

- **Section 4 retitled "Live Volume Compensation"** and restructured: the
  metadata-vs-measurement problem paragraph stands; the static-calibration
  narrative and its per-model input-gain table are DELETED, compressed into
  one motivation paragraph (probe-level validity, the non-equalizable
  compression-dominated capture, absolute-target rider failure — "the
  target must be relative, and the compensation must be live"); the
  dry-referenced live system is the method, with the 1 s warm-up added and
  the seed probe reduced to one sentence.
- **Section 1 roadmap** now says "a live volume compensation system that
  references the player's dry signal" (no "two-stage"/probe framing).
- **Section 6 (pedal)**: "calibrated library" -> "library"; "uncalibrated
  additions" -> "brand-new additions" (the pedal claim now rests on the
  live loop, not offline calibration).
- **Section 7**: "calibration tooling" -> "measurement tooling".
- Submission notes updated (static table removal recorded).

The subsections below are retained as the original edit rationale; where
they said "keep Table 1", the author's directive overrode them — the table
and all pre-determined-calibration framing are out.

Original section-by-section plan (historical):

### 3.1 Abstract & Introduction
- Abstract already names "a volume compensation system"; sharpen it to the
  actual claim, e.g. "…and a dry-referenced loudness compensation system
  that keeps arbitrary community captures — and any amount of input drive —
  at the player's dry level (25-model spread reduced from 9.9 dB to
  3.1 dB)."
- §1's roadmap sentence for Section 4: "Section 4 presents a two-stage
  volume compensation system — a load-time perceptual probe seeding a live
  dry-referenced loudness loop inside the opcode — that makes heterogeneous
  community captures usable side by side."

### 3.2 Section 3.1 (Interface)
- Update the signature line to include the new optional arguments:
  `aoutL, aoutR NAMProcess ainL, ainR, kProfileIdx, SLibDir [, kAutoVol, kDriveDb]`
  with one sentence each (compensation on by default; drive in dB,
  loudness-compensated). Note backward compatibility of old call sites.

### 3.3 Section 4 (Volume Compensation) — restructure into three parts
1. **Problem (keep, tighten).** The existing metadata-vs-measurement
   paragraph stands; it motivates "trust measurement, never metadata."
2. **Static calibration and its limits (condense the current content).**
   Keep Table 1 but reframe: the fixed-point input-gain calibration and the
   non-converging compression-dominated capture are no longer just a
   limitation — they are the *motivation* for the shipped design. The
   sentence "argues for loudness-domain normalization as future work"
   becomes "motivates the loudness-domain system below." Add one sentence
   on the two structural lessons from §2 of this writeup (probe-level
   validity; fragility of absolute-target riders) — this is the
   generalizable "failure modes and fixes" material the paper's genre
   rewards, and no prior art states it for neural amp models.
3. **The shipped system (new, ~2 paragraphs + small table).** Suggested
   prose skeleton (double-blind safe, no project names):

   > Rather than normalizing models toward an absolute loudness target, the
   > opcode references the dry input itself: the wet output is slowly
   > trimmed until it matches the perceived loudness of the signal the
   > player is producing. Both sides are metered as ITU-R BS.1770
   > K-weighted mean-square averages with a 2.5 s time constant, and the
   > trim tracks their ratio, so program dynamics — which move both meters
   > together — leave the trim untouched: the mechanism is not a
   > compressor. The trim slews at most 2 dB/s, holds within a 0.75 dB
   > deadband, freezes below −55 dBFS input, and is clamped to ±18 dB.
   > Converged trims are remembered per model file, so returning to a
   > previously played capture is immediately at level. A load-time probe —
   > the published NAM reference input scaled to a typical playing level,
   > measured through the same K-weighted, gated meter — seeds the makeup
   > so unseen models start close.
   >
   > Because the reference is tapped before the input stage, a pre-model
   > drive control becomes a pure saturation control: +18 dB of drive
   > measured +0.1 dB of loudness change and −4.6 dB of crest factor.

   Evaluation numbers to report (pick the compact set): 25-capture folder
   spread 9.9 dB (best static probe) → 3.1 dB, stdev 2.60 → 0.83, all
   models within ±2 dB of dry; the drive result; the −12 dB dynamics step
   passing through as −10.2 dB; swap-back error 0.6 dB.
- **Figure/table option:** a condensed 3-row comparison (static probe /
  absolute-target rider / dry-referenced loop with their stdev+spread) is
  more page-efficient than the full 9-row history; alternatively a
  before/after bar or strip plot of the 25 per-model levels. The full
  9-approach table can live in supplementary material or the repo.

### 3.4 Section 5 (Cabbage plugin)
- Replace "The volume compensation of Section 4 is always on … Input and
  output trims are the only level controls exposed" with the current
  surface: compensation defaults on with a UI toggle (raw level for A/B);
  the input trim is now a *drive* control (loudness-compensated, per §4);
  gate, three-band tone stack, and DC blocker mirror the reference NAM
  plugin's outboard chain (gate detects pre-model, applies post-model).
  One sentence suffices for the outboard DSP; the compensation is the
  contribution.

### 3.5 Section 6 (Pi pedal)
- "Because the library is pre-calibrated, no volume ride accompanies a tone
  change" → now stronger: the same in-opcode compensation runs on the
  pedal, so even *uncalibrated* additions to the library arrive
  level-matched, and per-model trim memory makes repeated tone-stepping
  stable. (The offline calibration table remains as the seed for the
  curated set.)

### 3.6 Section 7 (Conclusions)
- Delete "loudness-domain volume compensation for compression-dominated
  captures" from future work — it shipped; the compression-dominated
  Marshall capture that defeated gain-based calibration is exactly the case
  the dry-referenced loop handles (it lands within the ±2 dB folder result).
- Optionally add as future work: per-model trim persistence across sessions
  (trims currently live for the opcode instance's lifetime) and formal
  listening validation of the K-weighted match.

### 3.7 Claim-positioning (Related Work / novelty)
- One sentence contrasting with the official NAM plugin is warranted and
  verifiable: its "Normalized" output mode applies a *static* gain from
  stored loudness metadata (or a fixed −18 target), whereas this opcode
  performs live, dry-referenced, perceptually weighted compensation with
  per-model memory — to our knowledge not present in any published NAM
  host. (The official plugin's gate/EQ are likewise outboard DSP; nothing
  drives the model itself except input gain, which is precisely what the
  linked-drive design exploits.)
- Keep double-blind hygiene: no pedal/system names, no repo link beyond the
  existing "[link withheld for review]".

### 3.8 Page-budget note
Section 4 grows by roughly half a column; the condensed comparison table
plus trimming the current Table 1 to ~5 illustrative rows (keep the
non-converging capture) should keep the 4-page limit. The failure-history
lessons can be compressed to two sentences if space is tight — but keep
them; they are the generalizable engineering contribution the venue's
reviewers reward.
