# Transparent Pitch + Formant Shifter Redesign

**Date:** 2026-08-01
**Target:** `bela/hyperGuitar6chPan` — replaces the pitch/formant stages of `voiceProcess()` (dsp_core.cpp) and their support machinery.
**Top priority (user-stated):** attack clarity and attack speed. Second: "super even" sustain — zero audible modulation, transparent formant redistribution of tone weight.

## 1. Problem

The current shifter feels dynamic/reactive, and picking/fingering transients come through too loud. Root causes identified in code review:

1. **Non-COLA grain engine** (`launchGrain`/`streamRun`): grain length is chosen ad hoc (2.2–7.5× spacing), so windows don't sum to a constant → amplitude modulation at the grain rate, phase-locked to the note. Heard as "reactive" wobble.
2. **No pitch marks**: grains are anchored by delay bookkeeping, not waveform phase → grain-to-grain phase jitter → roughness.
3. **Transient mishandling**: env-driven re-anchor snaps `sdDelay` to 16 and resets all grain streams on every pick; overlapping grains can copy the pick click 2–4× (+6 dB per aligned copy); `HIGH_MIX` (0.25) passes the un-shifted high band permanently. Combined: loud, doubled, unshifted plosives.
4. **Formant as a second series grain engine** (`fsWet`/`fsDry` + env-eased `formantBlend`): second layer of AM and transient smear, reactive blend.

## 2. Research basis (three parallel deep-research reports, 2026-08-01)

- **Eventide lineage** (US 4,464,784; H949→H3000→H90): transparency on mono sources = dual-tap delay, splice only when forced, jump by correlation-verified ≈ pitch-period multiples, short crossfade. H90 "SIFT" (US 20250193632) is a polyphonic filterbank — wrong tool here; its "Auto EQ" is just interval-dependent tilt. Eventide ships NO formant correction for guitar.
- **IVL/TC-Helicon lineage** (US 5,231,671 / 5,301,259 / 5,567,901 — Whammy/Vocalist): Lent/PSOLA 2-period Hann grains re-triggered at the target rate; independent formant via grain resampling; aperiodic segments detected and passed dry exactly once.
- **Academic** (Moulines & Charpentier 1990; Bristow-Johnson JAES 1995; ESOLA 2018; Grofit & Lavner 2008; Fink/Holters/Zölzer DAFx-16; katjaas): output-domain windowing for exact COLA at all ratios; correlation-adaptive crossfade laws; transient "lock hop + single copy"; onset pointer-jump to mask tracker latency.

All three converge on the same architecture. Full reports preserved in session research (citations inline above).

## 3. Design

### 3.1 Unified hybrid engine (one per string — replaces SOLAD + up-stream + fsWet + fsDry)

A single pitch-synchronous grain stream where **pitch and formant are orthogonal parameters of the same pass** (Lent/Bristow-Johnson decoupling):

- **Grain launch spacing** `S = T/ρ` sets pitch ratio ρ (T = tracked period, slewed).
- **Within-grain read increment** φ sets formant ratio (Catmull-Rom interpolation; one-pole anti-alias pre-filter engaged when φ > 1).
- Window: Hann, length **exactly 2S, in the output domain** → exact COLA at every (ρ, φ) → zero grain-rate AM by construction.
- Grains sourced from the **newest complete pitch mark**; reuse capped at 3, alternating between the two newest marks at large up-shifts (anti robot-buzz).
- ρ = 1 or φ = 1 are just special cases; pitch-only, formant-only, and both-on all run the identical single engine. No stacking.
- Scheduler reads T only at grain boundaries, from a ~15 ms one-pole slew (hard reset on onset). No parameter may warp a grain mid-flight.

### 3.2 Pitch marks

- 2nd-order lowpass at ~1.5×f0 (coefficients per hop from tracked period).
- One peak-pick per period, search window predicted at `t_prev + T ± T/4`; hysteresis: reject marks implying >20% period change unless NSDF agrees.
- Hard resync at onset.

### 3.3 Attack path (top priority)

Per string, independent:

1. Per-sample onset detector: fast/slow envelope ratio (~1 ms / ~10 ms followers, fire at ratio ≈ 2.5) — not hop-quantized.
2. On fire: read pointer jumps to the onset (perceived attack latency ≈ detector latency ≈ 1 ms).
3. The attack region passes **exactly once at unity gain** (single-copy splice, short equal-power edge fades). Grain re-triggering inhibited.
4. φ ramps 1 → target over ~20 ms after re-lock (resampling a click by φ≠1 is audible as a "wrong" click).
5. Re-lock uses the per-string tuning prior (`p0`, fret-range bounds) for minimum-hop re-acquisition; resume pitch-synchronous mode after 2 confident hops, crossfade over ~1 period, correlation-aligned.
6. Offset/mute (existing release-edge detector): fall back per 3.5.

### 3.4 Hexaphonic leverage

- **Per-string adaptive latency**: all windows/fades scale with each string's own period — high strings run ~2–3 ms; only the low string pays its own physics.
- **Cross-string octave veto**: neighbor-string periods veto bleed-induced octave errors before they reach the shifter (extends existing `gOtherMax` gate).
- **Strum coherence**: onset timestamps shared across strings; near-simultaneous onsets (<~10 ms apart) resolve their transient windows time-aligned.
- **Per-string LPC envelopes** (LPC mode): each string models its own spectral envelope.

### 3.5 No-lock fallback

During mutes/scratches/noise (no NSDF confidence): Eventide-style dual-tap splicer — splice only when the tap leaves the middle half of the buffer, jump by the best-correlation lag (search restricted around last-known period when available), equal-power fades. Never freezes, never warbles.

### 3.6 Crossfade laws

Correlation-adaptive (Fink/Holters/Zölzer): NCC > 0.7 → amplitude-complementary (smoothstep); NCC < 0.4 → equal-power (Signalsmith polynomial); between → blend.

### 3.7 Formant modes (both built; encoder-switchable)

While FORMANT pad is armed, **main-encoder click toggles GRAIN ↔ LPC** (main click is currently unused on PAD_FORMANT; both encoders keep their continuous jobs: main = formant value, aux = glide). Mode persisted in saved state.

- **GRAIN** (default): within-grain resampling per 3.1. Zero extra CPU. IVL/TC-Helicon-proven.
- **LPC**: engine runs formant-preserving (φ=1); formant applied as true spectral-envelope warp — order-16 LPC per hop on each string (Tohkura spectral smoothing + −40 dB diagonal loading against harmonic-tracking), whiten with A(z), re-color with the frequency-warped all-pole envelope; coefficients interpolated per-sample in a stability-safe domain (reflection coefficients). φ→1 freeze during attacks applies to the warp amount identically.

### 3.8 Deletions and retentions

Delete: band-split tracking lowpass feeding the shifter + dry `HIGH_MIX` leg (engine is full-band; pick noise gets shifted with the note); env-driven transient re-anchor block; `formantBlend` easing; `fsWet`/`fsDry` streams; SOLAD (`sd*` state) and `GrainStream up` (subsumed).
Retain: DC blocker; NSDF tracker + confidence/blank/release machinery (feeds the new engine); H90-style Auto-EQ tilt but **static per interval setting, never env-coupled** (gentler than current); FORMANT-WAH and TALKBOX wobble keep working — they modulate φ target, engine handles motion smoothly by design.

### 3.9 Mix routing

Engine output = the processed leg. `gWet` = pitch wet % when pitch active; formant-only = engine at 100%. True dry = untouched input. Replaces the current dryLeg-formant special case (`fsDry`).

### 3.10 Presets

Active presets keep their exact current values on the new engine (deploy-gated on user approval at the bench):
- Pads 0–3 (formant 0.75 / 0.90 / 1.10 / 1.30, existing glides) — glide now slews φ target.
- Pads 4–7 (pitch −12 @100%, −5 @50%, −7 @50%, +12 @100%).
- Manual PITCH / FORMANT pads unchanged in UI behavior (+ LPC mode toggle per 3.7).

## 4. Budget & latency

- CPU: steady state ≈ 2 interpolated reads + 2 window MACs per sample per voice — below the current 14-grain × multi-stream cost. LPC mode adds ~order-16 Levinson per hop + FIR/IIR pair per sample per active voice. Current load 47% incl. USB bridge → comfortable.
- Latency: sustain path ≈ one output period + mark delay per string (adaptive, 2–8 ms typical); attack path ≈ 1 ms via onset pointer-jump.

## 5. Testing

- Build: existing `bela/deploy.sh` flow; verify clean compile + CPU % on target.
- Bench A/B (user): attack loudness/clarity vs old engine; evenness of sustain at −12/−7/−5/+12 and formant 0.75/1.30; GRAIN vs LPC formant A/B via encoder click; strum test for per-string transient isolation; mute/scratch test for fallback behavior.
- Instrumented: optional `gRawDiag` prints for onset-fire rate, re-lock hop count, splice NCC values.

## 6. Risks

- Pitch-mark quality on dirty/bent notes → mitigated by NSDF-predicted search + hysteresis + no-lock fallback.
- LPC mode envelope discontinuities → reflection-coefficient interpolation; if still audible, LSF interpolation as fallback.
- Deep down-shift on low string with short windows → window length follows output period by construction (COLA holds); worst-case latency accepted.
