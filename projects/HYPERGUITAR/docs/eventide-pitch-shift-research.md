# Eventide pitch-shifting research (H910 → H949 → H90)

Deep-research findings used to build the hyperGuitar per-string octave engine.
101 agents, 18 sources fetched, 25 claims adversarially verified (24 confirmed, 1 refuted).
Date: 2026-07-09.

## Core architecture (all Eventide time-domain shifters) — HIGH confidence, 3-0

A circular delay line written at constant rate, read by **two taps at a
pitch-scaled rate**; read-pointer jumps are masked by crossfading between the
taps (directly analogous to rotary-head tape shifters).

- **H910 (1975)**: no signal analysis. Two varying delays offset so when one
  hits its limit the other is mid-excursion; simple triangle/linear crossfade,
  both taps always audible. Glitches = phase differences between taps during
  the fade (heard as cancellation/volume drop); glitch rate grows with pitch
  ratio. (Agnello quote via Reverb; Valhalla DSP; Eventide retrospective.)

## H949 "intelligent splicing" — the key advance — HIGH confidence, 3-0

**Patent: Anthony Agnello, US 4,464,784 "Pitch changer with glitch minimizer"**
(filed 1981-04-30, granted 1984-08-07, assigned Eventide Clockworks).
Hardware: optional LU618 / "ALG-3" de-glitch board (~$740 in 1979), Reticon
5101 CCD correlator.

Mechanism: compute a correlation between **two time-displaced segments of the
stored signal** and jump the read pointer to the **lag of maximum
correlation**, so the fade-in segment is *phase-aligned* with the fade-out
segment. Phase-aligning the splice is what lets a **short crossfade** be used
without glitching. (Patent says "autocorrelation"; mathematically it's a
cross-correlation of two displaced segments.)

### Concrete patent parameters (MEDIUM confidence — exemplary, not mandated)

| Parameter | Value |
|---|---|
| Delay RAM | 1024 words ≈ 25 ms @ 40 kHz write clock |
| Read clock | 20–80 kHz variable → pitch ratios 0.5–2.0 |
| Correlation | 256-sample (6.25 ms) reference vs 512-sample (12.5 ms) region |
| Recompute | every ~6.25 ms (one passage says 12.5 — internal inconsistency) |
| Splice offsets | constrained to ¼–¾ of buffer (6.25–18.75 ms) |
| Offset weighting | linear ramp ×0.9 (shortest lag) → ×1.0 (longest): bias toward longer offsets = fewer splices/sec |
| Crossfade | linear complementary ramps (two multiplying DACs), "soft splice" like a beveled tape splice |

### Crossfade length tradeoff — HIGH confidence, 3-0

- Too **long** → metallic comb-filter coloration (two taps at constant spacing, both audible).
- Too **short** → glitch/stutter from phase cancellation.
- Phase-aligned splices collapse this tradeoff — that's the whole trick.

## H949's two algorithms / H90 continuity — HIGH confidence, 3-0

- Algorithm #1: slow gentle crossfades — no hard glitches but "swimming" at extreme ratios.
- Algorithm #2: H910-style random splices, upgraded to intelligent splicing (LU618).
- The **H90 manual still documents the same Algorithm 1 vs 2 duality** —
  the H949 lineage remains the conceptual basis of current Eventide
  time-domain shifting. **No public documentation of H8000/H9/H90 internals
  exists** (no patents, papers, or reverse-engineering write-ups surfaced).

## Modern low-latency refinement: Vetter's SOLAD — HIGH confidence, 3-0

Katja Vetter, katjaas.nl ([solad~] + [helmholtz~] Pure Data externals):
single delay line, read pointer jumps **forward for downshift by exactly one
detected period** (pitch-synchronous splicing), crossfades mask the jumps.
Pitch from SNAC (Specially Normalized AutoCorrelation). Because splice
positions come from up-to-date pitch info rather than a blind buffer search,
the read tap can hug the write pointer: **average latency < 10 ms** (variable).

Per-string guitar with known tuning (our case) lets the tracker's search range
be tightly constrained per string — better than any pedal seeing a mixed signal.

## Parallel art (NOT Eventide — attribution hazard)

US 4,415,772 (Eppler et al., Variable Speech Control Co., 1983): BBD-based
splicing with level-and-slope match detection. Reusable software patterns:
(a) inhibit match-searching for a fixed interval after each splice;
(b) **forced splice on timeout/silence** (energy detector) so the splice always
happens. A claim conflating this with the Eventide method was REFUTED 0-3.

## Guitar-specific tracking notes

- Cycfi bitstream autocorrelation (BACF): XOR on zero-crossing bitstream,
  detects within ~2 cycles; cheap enough for per-string embedded tracking.
- Theoretical minimum delay for monophonic pitch detection ≈ 1 period of the
  lowest note (~14.4 ms for Db2 at 69.3 Hz); practical rule ≈ 2 periods.
- Octave-down reproduction fundamentally needs ~1.5–2 periods of the input
  note buffered: latency floor on low Db ≈ 20+ ms, ~7 ms on high Db. The H90
  cannot beat this either.

## Open questions from the research

1. What the H8000/H9/H90 add beyond H949 splicing (multi-resolution
   correlation? transient detection? adaptive fade lengths?) — undocumented.
2. Optimal correlation-window / crossfade lengths at 44.1 kHz for guitar
   registers — patent's 6.25 ms window is shorter than one low-string period;
   scaling windows with the per-string period (what we do) is the sensible
   extrapolation.
3. Transient handling: gate pitch-sync jumps during pick attacks, fall back to
   correlation search, or force splices on timeout.
4. Whether "intelligent splicing" coloration noted in the H90 manual comes from
   the long-lag bias weighting — tunable per string.

## Key sources

- Patent US 4,464,784 (primary): https://patents.google.com/patent/US4464784A/en
- Valhalla DSP de-glitch write-up: https://valhalladsp.com/2010/05/07/pitch-shifting-the-h949-and-de-glitching/
- Eventide H949 Flashback: https://www.eventideaudio.com/blog/flashback-7-1-the-h949-harmonizer/
- Eventide H949 plug-in page: https://www.eventideaudio.com/plug-ins/h949-harmonizer/
- Reverb, H3000 tech history (Agnello quotes): https://reverb.com/news/tech-behind-eventide-h3000-ultra-harmonizer
- Vetter, low-latency pitch shifting (SOLAD): https://www.katjaas.nl/pitchshiftlowlatency/pitchshiftlowlatency.html
- Vetter, helmholtz~ SNAC tracker: https://www.katjaas.nl/helmholtz/helmholtz.html
- Cycfi BACF: https://www.cycfi.com/2018/03/fast-and-efficient-pitch-detection-bitstream-autocorrelation/
- US 4,415,772 (VSC parallel art): https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/4415772
