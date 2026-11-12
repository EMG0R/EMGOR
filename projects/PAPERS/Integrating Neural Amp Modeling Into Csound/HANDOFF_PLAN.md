# NAM-in-Csound — handoff plan for paper + hardening work

Written 2026-07-03. This is a planning/context document for whoever (human or
agent) picks up the actual paper-writing and hardening work — it is deliberately
comprehensive rather than terse. See also `brainstorm-plan.md` in this same
folder for the original 4-page paper section budget and ICSC 2026 submission
constraints (deadline 2026-07-11 23:59 Rome time, 4pg hard limit, double-blind).
Don't re-derive those constraints — they're settled, read that file.

Status: both deep-research passes are complete (2026-07-03) — one on NAM's
technical background (architecture, training, prior art / novelty claim), one
on the style/conventions of Csound/DAFx/NIME-style implementation papers
(includes direct download/read of the real ICSC template + four full
ICSC2019 papers + five DAFx papers). Sections 4-5 below summarize both;
`research-nam-background.md` and `research-paper-style.md` in this same
folder have the full findings with inline citations. A third, independently
corroborating research pass (from a separate session) on the novelty-claim
question landed unprompted and has been folded into
`research-nam-background.md` §5.3 — cross-verified with concrete sources,
confidence upgraded accordingly.

---

## 1. Latency — measurement plan (not yet executed)

User's steer: "the latency is very minimal but you should still be measuring
it" — i.e. don't just assert it, produce a real number. Two complementary
measurements, because they answer different questions:

### 1a. Algorithmic/opcode-level latency (the number that belongs in the paper)

Isolates what `NAMProcess` itself adds, with zero JACK/interface/OS noise in
the way. Method: offline (non-realtime) csound render, feed a clean impulse or
sharp step through `NAMProcess` directly (same pattern as
`testing/nam/nam_pregain_calibrate.csd`'s offline-render approach, but with an
impulse instead of a multi-tone), capture input and output sample-accurately,
cross-correlate to find the output's response onset relative to the input
impulse. Report the result in samples and in ms at 48kHz/ksmps=128.

Expectation to verify, not assume: WaveNet-style causal dilated convolutions
and LSTM-style models are architecturally causal (no lookahead), so the
*algorithmic* delay should be at most one `ksmps` block (2.67ms @ 128/48k) —
the same block latency any Csound opcode has, not something NAM-specific. This
needs to be **measured**, not asserted, because (a) some NAM-adjacent
architectures use non-causal padding, and it hasn't been confirmed which
`nam::DSP` subclasses this specific profile set uses, and (b) the
`SlimmableContainer` wrapper or per-profile receptive field could plausibly
add settle time that reads as extra delay even if not architecturally
non-causal.

### 1b. Real-world validation (does 1a hold up in the actual deployed chain)

Reuse `DEMIURGE_OS/testing/latency/` — it's already built, proven, and
explicitly designed to answer exactly this class of question with **no
audio interface required** (self-clocking via the permanent `snd-aloop`
fallback, see `testing/latency/CONTINUATION.md` §1). Method:

1. `./latency.sh internal` against a blank/bypass patch (sync_layer=on, no
   NAM stage) → baseline `internal [measured]` number.
2. `./latency.sh internal` against the same patch with a NAM stage inserted
   in the chain → `internal [measured]` number with NAM present.
3. Diff the two. That diff is NAM's real-world contribution inside the actual
   sync layer, validated end-to-end, not just in a synthetic offline render.

Report BOTH 1a and 1b in the paper's Verification section — algorithmic number
+ validated-in-deployment number — rather than the current one-off "~20% CPU"
anecdote. Neither has been executed yet; this is a to-do, not a result.

---

## 2. Opcode hardening roadmap — "everything needed to be totally ready"

Organized by priority. P0 = correctness-critical, don't call the system
"solid" until these are addressed. P1 = should-fix for paper credibility and
real robustness. P2 = nice-to-have / long-term.

### P0 — correctness-critical

1. **Kill the stale `DEMIURGE_OS/nam/*` copy.** It still has the single-shared-
   model-instance bug (see `project_nam_working` memory, "still divergent"
   note) — `deploy-nam.sh` would silently regress it if ever run. Recommended
   fix: make `NEPTR phase4/csound/nam/` the single source of truth (it has
   both fixes — per-channel instances + calibrated gain staging) and either
   delete `DEMIURGE_OS/nam/src/*` + `deploy-nam.sh`, or replace them with a
   pointer/symlink/build step against the NEPTR copy. Never let two divergent
   copies of stateful, previously-buggy DSP code coexist again.

2. **Global-pointer bug: only one `NAMProcess` instance is safe at a time.**
   `publish_loader_ptr()` / `__nam_loader_ptr__` in `csound_nam_opcode.cpp` is
   a single CSound global-variable slot. If two `NAMProcess` opcodes are ever
   instantiated in one orchestra (e.g. a stereo-doubled stage, or two NAM
   stages in one chain), the second `nam_init()` silently overwrites the
   global pointer, and `NAMCount` (and anything else reading that global)
   reflects only the last-initialized instance — not a crash, a silent
   correctness bug. Fix: either key the global variable by a per-instance
   identifier (hash of `SLibDir`, or an explicit instance-index opcode
   argument), or add a runtime check that errors loudly on a second
   `NAMProcess` instantiation rather than silently corrupting shared state.
   Currently undocumented as a constraint anywhere.

3. **No correctness verification against reference NAM inference.** Current
   evidence (`testing/nam/nam_signal_test.csd`, Tier 2) proves the opcode is
   "non-silent and different from dry" — that rules out passthrough/no-op
   bugs, it does NOT prove the Csound port faithfully reproduces the trained
   model (buffer indexing, channel order, precision, or state-handling bugs
   could all still produce "different from dry but wrong" output and this
   test would pass). Fix: run the same `.nam` file + a fixed test signal
   through (a) this opcode and (b) the reference NAM engine (the official
   Python `neural-amp-modeler` package, or `NeuralAmpModelerCore`'s own
   standalone/example runner), diff the two outputs sample-for-sample or via
   a numeric error metric (e.g. ESR — see research-nam-background.md for what
   NAM's own training loss uses, reuse the same metric for consistency).
   Cover at least one profile per DSP-architecture family present in the
   library (WaveNet, LSTM, SlimmableContainer) since they have different
   state-handling. **This is the single highest-leverage addition — it turns
   an anecdote into an actual evaluation section for the paper.**

### P1 — should-fix for credibility and robustness

4. **Latency measurement** — see section 1 above, not yet executed.

5. **CPU/performance characterization across the real matrix**, not one
   nominal reading. Vary: buffer size (64/128/256 ksmps), DSP-architecture
   family, and thermal-throttled Pi state (the codebase already has throttle
   tooling — see `project_power_throttle` memory — reuse it to force a
   throttled condition and re-measure rather than only testing nominal
   clock). Report worst-case, not best-case.

6. **Sample-rate mismatch is unquantified.** `nam_loader.cpp`'s `load_one()`
   currently only logs a stderr warning ("tone will be off") when a model's
   trained sample rate differs from the engine's — no resampling, no
   measurement of how far off it actually sounds. Either implement resampling
   on load, or explicitly measure and report the pitch/tone drift at common
   mismatches (e.g. a 48kHz-trained model under a 44.1kHz engine) so this is
   a characterized, honestly-scoped limitation instead of an unquantified
   warning nobody has actually listened to.

7. **No CI / automated regression — manual scripts + ears only.**
   `testing/nam/test-nam.sh` already covers Tiers 1+2 (symbol guard + offline
   functional render) — wire it to run automatically on every build rather
   than requiring a human to remember to invoke it.

8. **`nam/design.md` is stale documentation.** It still describes the
   pre-fix single-instance architecture with no mention of the per-channel
   fix or the gain-calibration pipeline. Update it to match
   `NEPTR phase4/csound/nam/src/*` as it actually is today, or it will mislead
   the next person (including a future paper reviewer who goes looking at the
   repo).

### P2 — nice-to-have / long-term

9. **Denormal/FTZ handling unverified.** Neural inference on near-silent
   input can hit denormal floats and CPU-spike on some architectures (a
   classic audio-DSP gotcha with recursive/convolution-heavy processing).
   Verify FTZ/DAZ is enabled for the audio thread, or explicitly measure CPU
   behavior on a near-silent signal to rule this out.

10. **Recalibration is fully manual.** Adding a new profile to a directory
    doesn't trigger `calibrate_pregains.sh` — a profile with no entry in
    `nam_pregain.conf` silently falls back to the metadata/measured-loudness
    path instead of the calibrated one. Consider a lighter-weight per-profile
    auto-calibrate-on-first-load path, if it doesn't reintroduce audio-thread
    blocking.

11. **No user-facing feedback on failed profile loads.** `load_one()` logs to
    stderr and keeps the previous pair on failure — is that surfaced anywhere
    a performer would see it live (NEPTR UI, OSC error channel)? If not, a
    silently-failed profile swap during a performance is a bad failure mode
    worth at least a status LED/UI flag.

12. **Licensing audit** of any `.nam` captures bundled for
    distribution/supplementary material — ToneHunt/Tone3000 uploads have
    per-uploader licensing that hasn't been checked file-by-file. Blocking
    for public release, not for private work.

---

## 3. Demo `.csd` design — brainstorm, not yet built

Explicitly deferred by the user ("we aren't there yet") — this section is
options + a recommendation, nothing here should be implemented until signed
off.

**The question:** one file with comment/uncomment profile switching (the
original plan), separate files per profile, or something else?

### Options

**A — single file, comment/uncomment lines (original plan).** Simplest,
matches "uncomment a line to switch." Downside: mild footgun (forget to
comment out the old line, or forget which one is active), and trying several
examples back-to-back means repeated manual file edits + relaunch.

**B — one file per profile, fully separate, zero editing.** Easiest for a
reviewer or listener: just run a different named file
(`run_marshall_sl68.csd`, `run_neve_preamp.csd`, ...). Maps 1:1 to paper audio
examples/figures later. Downside: duplicates the ~15-line opcode-wiring
boilerplate N times — a future fix to the wiring itself means editing N files
and risking drift between copies (exactly the kind of divergence that already
bit this project once, see P0-1 above).

**C — single template file + command-line macro override**
(`--omacro:PROFILE_IDX=N`, no editing needed). Single source of truth (DRY),
but a bare code listing in the paper doesn't show which index is which amp —
needs a companion table.

**D — hybrid, RECOMMENDED: template file (C) + a table/README mapping names
to invocations.** One `.csd` is the single source of truth and IS the paper's
code listing (reviewers see one clean ~15-line opcode call, not five
near-duplicate files). A companion table gives copy-pasteable one-liners per
curated example (`csound --omacro:PROFILE_IDX=8 nam_demo.csd  # Marshall
SL68, compression-plateaued`). Gets B's "just run this one command for this
weird sound" ergonomics without B's duplicated-file maintenance risk, and
avoids A's edit-and-forget footgun.

### Profile curation

User's own steer: "probably will just use 3 very different amp models and
then grab like a neve preamp or some other weird stuff to showcase the
weirdest sounds we can get from it." Translating that into a concrete
showcase set:

- **3 maximally distinct guitar amps** from the already-calibrated 10-profile
  set (Table 1 in `brainstorm-plan.md`) — candidates: a clean American voice
  (e.g. `03_fender_super_reverb_1977`), a mid-gain British-voiced edge
  (e.g. `01_vox_ac30_edge` or `03_princeton_edge_v5`), and the already-
  interesting **`05_marshall_sl68_drive`** (compression-plateaued — already a
  paper talking point in section 5.3 of the plan, reusing it here as a
  showcase example ties the paper's prose directly to something a reader can
  actually hear).
- **1-2 deliberately non-amp captures**, to make the "NAM generalizes beyond
  guitar amps" claim (see research-nam-background.md §2) audible, not just
  asserted. **Not yet sourced** — need to search ToneHunt/Tone3000 for
  preamp/mic-pre/compressor captures (a Neve-style preamp was the user's own
  example, unconfirmed whether one exists in the community library). This is
  an open to-do, not a decision — don't fabricate a profile that doesn't
  exist.

### Recommendation

Build option D once unblocked: one template `.csd`, macro-driven profile
select, README table with curated invocations. Before building: search
ToneHunt/Tone3000 for non-guitar-amp captures to fill the "weird" slot(s).
Still explicitly NOT built yet per user instruction — this section is the
brainstorm output, implementation is a separate future step.

---

## 4. NAM technical background (for paper context, not replication)

Full findings with inline citations: `research-nam-background.md` in this
folder. Summary of what matters most for the paper:

- **NAM = Steven Atkinson (`sdatkinson`)**, layered as Python trainer repo →
  `NeuralAmpModelerCore` (C++ real-time DSP, Eigen-based) → official
  `NeuralAmpModelerPlugin` (iPlug2, ships **VST3 + AU only** — AAX/CLAP/Linux/
  iOS are roadmap, not shipped; LV2 exists only as an unofficial third-party
  port by a different maintainer, `mikeoliphant/neural-amp-modeler-lv2`).
- **Architectures**: WaveNet-style dilated causal convolutions and LSTM, both
  confirmed in the `.nam` JSON spec's `architecture` field; plus a
  **Slimmable/`SlimmableContainer`** mechanism (Atkinson, arXiv:2511.07470,
  Nov 2025) letting one trained model's width scale down at inference with no
  retraining — directly useful to cite as precedent for "NAM already has a
  built-in notion of tunable compute cost," supporting this paper's opcode
  design choices.
- **Training**: reamping workflow (known reference signal → real hardware →
  record output → supervised training), a specific versioned reference input
  file (`input.wav`, formerly `v3_0_0.wav`), loss = Error-to-Signal Ratio with
  optional pre-emphasis filtering — traces directly to Wright & Välimäki,
  "Perceptual Loss Function for Neural Modelling of Audio Systems"
  (arXiv:1911.08922). Community-reported training time: ~10-20 minutes.
  **Explicitly generalizes beyond guitar amps** — NAM's own `gear_type` schema
  field and site copy list "bass amps, effects pedals, preamps, outboard gear"
  as supported capture targets, with the caveat that strongly time-varying/
  dynamics-heavy devices (compressors, delay) are harder to capture well with
  the standard black-box approach — directly supports both the "why this
  generalizes" framing AND the demo file's planned "weird non-amp profile"
  showcase (section 3 above).
- **Academic lineage** (cite in Related Work): Damskägg/Juvela/Välimäki
  (ICASSP 2019, SMC 2019), the synthesis journal paper Wright/Damskägg/Juvela/
  Välimäki (*Applied Sciences* 2020, MDPI open access), Wright/Damskägg/
  Välimäki on RNN black-box modeling (DAFx-19), Wright's 2023 Aalto doctoral
  thesis, and Steinmetz & Reiss on neural compressor modeling (AES 2022,
  arXiv:2102.06200) for the "generalizes to other nonlinear devices" point.
  NAM's own docs don't explicitly cite this lineage, but the technique is the
  same family — state this distinction carefully in the paper, don't imply
  NAM's authors claim descent from these papers.
- **Novelty claim — CONFIRMED, double-corroborated.** Two independent research
  passes (this session's background-research agent, plus an unprompted
  corroborating pass from a separate session, ~19 distinct search queries
  each) found **zero prior art for NAM/NeuralAmpModelerCore/RTNeural/WaveNet-
  based amp modeling in Csound** — no opcode, plugin, repo, mailing-list
  thread, or forum post. Both passes independently found the same adjacent
  precedent in *other* computer-music environments, now cross-verified with
  concrete sources: **Max/MSP's `neural_tilde`** (github.com/apresta/
  neural_tilde — loads actual `.nam` files, the closest analog to this work,
  different host), and **SuperCollider's Proteus UGen + generic RTNeural UGen**
  (both by Sam Pluta — the RTNeural UGen family "now runs on SuperCollider,
  Max/MSP, and Pure Data" per its own announcement thread). A genuinely
  different (non-neural, circuit-model) prior Csound amp opcode also exists —
  Fink & Rabenstein's triode-stage vacuum-tube opcode, DAFx-11 — worth citing
  explicitly as "related but different technique" so a reviewer doesn't find
  it and think it was missed. **Recommended phrasing: "to the best of our
  knowledge, no prior published Csound integration exists"** — properly
  hedged, not an absolute claim, but well-supported by search depth.

## 5. Paper style / conventions research

Full findings with inline citations: `research-paper-style.md` in this
folder — built from directly downloading the real ICSC 2026 LaTeX template
and reading **four full ICSC2019 papers plus five DAFx papers (2002-2022)**
end to end, not just abstracts. This should directly reshape how the paper in
`brainstorm-plan.md` gets drafted. Key facts:

- **Template confirmed: Springer `llncs.cls`** (Lecture Notes in Computer
  Science), single column, A4, two heading levels only (`\section`,
  `\subsection`), Computer Modern fonts fixed by the class, bold/underline
  discouraged. Code goes in plain `verbatim` blocks — the template itself
  demonstrates dumping a full `.csd` file this way, so a `NAMProcess` opcode
  listing is squarely sanctioned.
- **Page limit refinement: 4 pages for content, PLUS a free 5th page for
  references only.** This changes the budget in `brainstorm-plan.md` —
  references don't need to be squeezed into the 4-page content budget at all.
- **Citation style confirmed: numbered/bracketed, Springer-LNCS-derived**
  (`[1]`, `[2]`...), alphabetical-by-author reference list, plain
  `thebibliography`/`\bibitem` — NOT APA/author-date. Matches DAFx (IEEE-style
  numbered) and NIME (ACM-format numbered) conventions too.
- **Double-blind: ICSC's own stated bar is narrow** (no author names/
  affiliations — the template ships literal `AuthorA`/`AuthorB` placeholders
  meant to stay in place through review). **ICSC's own instructions say
  nothing about project names, repo links, or hostnames** — this is a real
  gap in their guidance. The research recommends adopting **NIME's stricter,
  more explicit norm** as best practice instead, since NIME 2026's submission
  guide explicitly names **"project names, linked websites"** as things that
  must be stripped, and states "the obvious presence of author names... is
  grounds for rejection prior to review." **This is directly actionable for
  this paper**: the NAM opcode work lives inside a larger named personal
  system (DEMIURGE_OS / NEPTR) — those names, any linked repo with
  identifying account info, and self-citations must be scrubbed/phrased in
  third person for the submitted (non-camera-ready) version.
- **Structural precedent is strong and specific — four real ICSC2019 papers
  read in full, use these as direct models:**
  - **Zinemanas, Rocamora, Jure, "Improving Csound's Ambisonics decoders"
    (ICSC2019)** — the closest structural precedent for THIS paper's
    "something was broken/limited in the old approach → here's the fix"
    narrative (contrasts old `bformdec1` limitations against new
    `bformdec2`). Cite as direct genre precedent.
  - **Hofmann et al., tube-resonator opcode paper (ICSC2019)** — closest
    "opcode implementation" structural analog (Init/Runtime subsections,
    near-code-granularity description, usage example, no rigorous
    evaluation, defers validation to a separate paper + GitHub repo).
  - **Rogozinsky & Goryachev, TX81Z FM synth modeling (ICSC2019)** — the
    most empirically rigorous of the four: real-hardware-vs-model waveform
    and spectral overlays plus informal listening tests. Use as the model
    for how much "evaluation" is actually expected — informal
    time/frequency-domain comparison + listening, not a formal blind study.
  - **Fink & Rabenstein, triode-stage vacuum-tube opcode (DAFx-11)** — the
    other prior (non-neural) Csound amp-modeling opcode (see §4 above);
    also a good model for hybrid quantitative+listening evaluation.
  - **Chowdhury, RTNeural (arXiv/DAFx-adjacent)** — closest genre match for
    "real-time neural inference wrapped for audio use," has the most
    rigorous formal benchmark found in the whole survey; cite for the
    real-time-safety discussion (also cites Ross Bencina's canonical
    real-time-audio-programming reference — apparently a community-standard
    citation, worth including too).
- **Universal cross-paper structure**: Abstract → Introduction (motivation +
  related work, 20-40% of the paper) → an implementation section named after
  the artifact → sometimes a Usage/Features section → a short, almost always
  *forward-looking* Conclusion (not a results summary) → numbered references.
  **None of the four ICSC2019 papers has a section literally titled
  "Evaluation" or "Results."** Every one links a public GitHub repo — treat
  this as an implicit norm, not optional.
- **Bug/failure-mode narrative — real but modest precedent, calibrate the
  framing.** No paper found runs a clean "named bug → root cause → fix"
  structure exactly like this paper plans. Closest precedent: DAFx-02's
  Rappid paper candidly admits an unresolved CPU-dropout defect with a stated
  cause (not fixed, just named); DAFx-22 organizes an entire paper around a
  challenges→solutions taxonomy. **Recommendation, direct quote from the
  research**: frame the bug narrative "as a documented, generalizable
  engineering contribution rather than assuming an established 'bug-report
  paper' tradition" — i.e. section 5 of the paper plan (Failure modes & fixes)
  should explicitly say *why* these findings generalize beyond this one
  opcode (stateful neural inference in any block-based host), not just
  narrate "here's a quirky bug we hit."
- **Direct verdict on acceptance plausibility** (quoting the research): *"A
  paper describing 'we built a working, deployed NAM opcode and found/fixed
  two real non-obvious bugs' is a plausible, well-precedented accept at ICSC
  specifically."* Multiple real ICSC2019 papers are structurally identical
  (implementation report, informal-or-no evaluation, GitHub link, forward-
  looking conclusion). NIME's published reviewer criteria (used as the
  strongest sibling-venue analogy, since ICSC publishes no detailed rubric)
  explicitly names "implementation" as a first-class standalone contribution
  and its short-paper track explicitly sanctions papers with "no accompanying
  evaluation or one of limited scope." **The risk isn't scope, it's craft**:
  needs a clear gap-framing opening (model this on the Zinemanas Ambisonics
  paper) and a genuinely informative implementation section.
