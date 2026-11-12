# Integrating Neural Amp Modeling Into Csound — brainstorm plan

Target: **ICSC 2026** (International Csound Conference, Trapani, Italy).
Submission window **30 June – 11 July 2026, 23:59 Rome time** — today is 3 July, ~8 days left.
Hard constraints from the CFP, now confirmed by directly downloading and reading the
actual template (see `research-paper-style.md` for full detail):
**4 pages max content + a free 5th page for references only**; template is Springer
**`llncs.cls`** (single column, A4, two heading levels only, Computer Modern fonts,
code in plain `verbatim` blocks — the template itself demonstrates dumping a full `.csd`
this way); citations are **numbered/bracketed, Springer-LNCS style** (`[1]`, `[2]`...,
alphabetical `thebibliography`, NOT APA); **double-blind** (ICSC's own bar: no author
name/affiliation, template ships literal `AuthorA`/`AuthorB` placeholders — but ICSC's
own guidance says nothing about project names/repo links, so adopt **NIME's stricter
norm** instead: strip project names, linked repos, and self-citations must be
third-person — directly relevant since this work lives inside a larger named personal
system, DEMIURGE_OS/NEPTR); submitted via Microsoft CMT.
Sources: https://csound.com/icsc2026/submission.html, `research-paper-style.md`
(downloaded template + 4 ICSC2019 papers + 5 DAFx papers read in full).

## Angle

Engineering report, not a survey. Lead with the working opcode as the artifact.
NAM (the trained-model ecosystem: ToneHunt, Tone3000) already solved "how do you
get a neural amp model" — the contribution here is a native, real-time, hot-swappable
Csound opcode around it, plus three genuinely non-obvious bugs and how they were
diagnosed and fixed. This is the "simple but well-timed" framing from the Dr. B email:
nobody else has plugged NAM into Csound as a first-class opcode yet, and the failure
modes are specific enough to Csound's plugin ABI and to neural models' statefulness
that they're worth documenting even though each fix is small.

**Novelty claim — now verified, cite carefully.** Two independent deep-research passes
(see `research-nam-background.md` §5) found zero prior Csound integration of NAM or any
comparable real-time neural amp-modeling inference — no opcode, plugin, repo, or forum
thread. Phrase as *"to the best of our knowledge, no prior published Csound integration
exists"* — properly hedged, well-supported by search depth. Adjacent prior art DOES
exist elsewhere and should be cited as related work, not ignored: Max/MSP's
`neural_tilde` (loads actual `.nam` files — the closest analog, different host) and
SuperCollider's Proteus/RTNeural UGens (Sam Pluta). A genuinely different, non-neural
prior Csound amp opcode also exists — Fink & Rabenstein's triode-stage vacuum-tube
opcode (DAFx-11) — cite it explicitly as "related but different technique" so a reviewer
doesn't find it and assume it was missed.

**Framing calibration for the bug narrative (from `research-paper-style.md` §3):** real
but modest precedent exists for "documented engineering difficulty" papers at this venue
(DAFx-02's Rappid paper admits an unresolved defect; DAFx-22 uses a challenges/solutions
structure) — but no paper found runs a clean "named bug → root cause → fix" structure
exactly like this one. Frame section 5 as a **documented, generalizable engineering
contribution** (these findings apply to anyone porting stateful neural inference into a
block-based, potentially multichannel DSP host, not just this one opcode) — not as "here's
a quirky bug we hit." That framing is what earns it a place in a 4-page paper instead of
reading as a changelog.

## Section budget (4 pages, ICSC template)

1. **Abstract** (~150w) — one native opcode, three bugs, working deployment, calibrated
   gain staging across real community-sourced captures.
2. **Introduction** (~0.4p) — what NAM is (WaveNet/LSTM-family neural amp modeling),
   why Csound has nothing equivalent (existing opcodes are static waveshapers —
   `distort`, `powershape` — not learned nonlinear systems), why now (mature trained-model
   ecosystem means zero training burden for the Csound user).
3. **Related work** (~0.2-0.3p) — NAM itself (Atkinson) and its real academic lineage:
   Damskägg/Juvela/Välimäki (ICASSP 2019, SMC 2019), the synthesis paper
   Wright/Damskägg/Juvela/Välimäki (*Applied Sciences* 2020, open access), Steinmetz &
   Reiss on neural compressor modeling (AES 2022) for the "generalizes beyond guitar
   amps" point. Cite Chowdhury's RTNeural (closest genre match: real-time neural
   inference wrapped for audio, has the field's real-time-safety citation — Bencina's
   "time waits for nothing"). Cite Fink & Rabenstein's triode-stage opcode (DAFx-11) as
   the prior (non-neural) Csound amp-modeling opcode. One line distinguishing from
   general neural-synthesis plugins (RAVE etc.) to show awareness without scope creep.
   See `research-nam-background.md` §3-4 for full citations.
4. **Architecture** (~1p) — `NAMProcess`/`NAMCount` opcodes; `NamLoader` background
   thread with lock-free hot-swap (`shared_ptr` swap under mutex, audio thread never
   blocks); profile switching live via k-rate index, no audio dropout. One block
   diagram if time allows, otherwise prose + the OENTRY snippet as a code listing.
5. **Failure modes & fixes** (~1.2p) — the core contribution, three subsections:
   - **5.1 Scheduling.** CSound 6.18's `LINKAGE`/`AppendOpcode` maps `OENTRY.thread`
     into the flags slot; `thread=5` (init+aperf) silently never schedules the aperf
     callback, so a technically-correct aperf-based opcode produces zero output with
     no error. Fix: `thread=3` (init+kperf) — CSound always calls kopadr regardless of
     declared outypes, so `outypes="aa"` still allocates correctly. Regression test:
     one `nm` symbol check that the shipped `.so` exports `nam_kperf` and not `nam_aperf`.
   - **5.2 Statefulness across channels.** WaveNet models carry per-layer convolution
     ring-buffer state (`NAM/conv1d.h`) and LSTM hidden state across `process()` calls.
     Running L and R through one shared `nam::DSP` instance is mathematically invisible
     when L==R (mono-ish signals) — which is exactly why an earlier offline test passed —
     but produces audible "sample-rate-vibe" distortion once a real stereo signal
     (post-chorus, post-reverb) hits it, because each channel's convolution history gets
     polluted by the other channel's samples. Fix: two independent model instances per
     profile, loaded off the audio thread, swapped as an atomic pair
     (`get_active_pair`). This is the most publication-worthy finding — it's a pitfall
     specific to porting *stateful* neural inference into a block-based, potentially
     multichannel DSP host, not specific to NAM or Csound individually, so it
     generalizes to anyone doing this integration in any host.
   - **5.3 Loudness calibration.** Community-sourced captures vary in metadata-reported
     loudness by ~13–29 dB (measured range across a 10-profile set from
     ToneHunt/Tone3000), and metadata loudness turned out to not be trustworthy — the
     eventual approach measures each model directly (1 kHz probe tone at -18 dBFS RMS
     through the model, RMS over the settled tail) rather than trusting metadata, then
     converges a per-profile input pregain via fixed-point iteration
     (`g += rmsIn - rmsOut`, ±0.15 dB tolerance) against a fixed reference level.
     **Table 1** (below) is real calibration output — 9/10 profiles converge in 1–8
     iterations; one profile fails automatic convergence because the underlying capture
     is heavily compressed (output loudness plateaus within ~1 dB across a 14 dB input
     sweep), which is itself worth a sentence — a purely gain-based calibration model
     breaks down on nonlinear, compression-heavy captures, and that's a legitimate
     scope limitation of the calibration method, not a bug.
6. **Verification** (~0.4-0.5p) — three-tier strategy: symbol guard (instant), functional
   offline render (multi-tone in, checks non-silent AND different-from-dry, catches the
   specific historical failure of a test that fed silence through the opcode and passed
   anyway), live integration probe on hardware. Table adapted from `testing/nam/README.md`.
   Plus, once executed (see `HANDOFF_PLAN.md` §1, not done yet): measured algorithmic
   latency + real-deployment-validated latency delta. **Calibrate expectations against
   real precedent**: none of the four ICSC2019 papers read in full has a section titled
   "Evaluation" or "Results" — informal/qualitative comparison (waveform/spectral overlay
   + listening, per the TX81Z paper) is the genre norm here, not a formal blind study.
   Don't over-engineer this section relative to what the venue actually expects.
7. **Limitations & future work** (~0.2p) — see honesty section below; state these
   directly rather than let a reviewer find them.
8. **Conclusion + references.**

### Table 1 — pregain calibration results (real data, `testing/results/*.res`)

| profile | pregain (dB) | iterations | status |
|---|---|---|---|
| 01_ac30_saturated | -3.9 | 5 | OK |
| 01_vox_ac30_edge | 0.0 | 1 | OK |
| 02_deluxe_reverb2_lightdrive | +4.4 | 5 | OK |
| 02_fender_deluxe_reverb_clean | -4.6 | 7 | OK |
| 03_fender_super_reverb_1977 | -1.2 | 2 | OK |
| 03_princeton_edge_v5 | +0.6 | 3 | OK |
| 04_roland_jc120_clean | +3.5 | 2 | OK |
| 04_tweed_5e3_breakup | +4.0 | 3 | OK |
| 05_marshall_sl68_drive | -16.0 | 8 (auto), then hand-tuned | FAIL → manual |
| 05_tweed_deluxe_edge | +3.8 | 4 | OK |

Marshall capture: output pinned to ≈-12.4 dB regardless of input across a -10…-24 dB
sweep (compression-plateaued); best achievable residual ≈+0.7 dB. Worth one sentence
in 5.3 as the edge case that breaks the linear-gain assumption.

## Honest assessment — "is the opcode done?"

**What's earned:** it runs live, in real time, on real hardware (Raspberry Pi), produces
audibly and measurably non-passthrough amp-modeled output, both scheduling and
statefulness bugs have understood root causes with regression signatures, and gain
calibration converges and holds across 10 real community captures. That's a real,
deployed, working system — not a toy.

**What "done" overclaims — say these out loud in Limitations, don't let a reviewer
find them first:**
1. `DEMIURGE_OS/nam/*` is a stale copy that still has the single-shared-instance bug
   (bug 5.2) — if anyone runs its `deploy-nam.sh` it silently regresses. Fix this in
   the codebase regardless of the paper; don't reference that path as evidence of
   anything working.
2. Correctness evidence today is "non-silent and differs from dry" — proves the
   opcode does *something*, not that it *faithfully reproduces* the trained model.
   No diff exists against the reference NAM engine's own output on identical input.
   **This is the single highest-leverage addition if any extra time exists this week** —
   run the same `.nam` file + a fixed test signal through the reference NAM inference
   (Python/C++) and through the opcode, diff the two — turns an anecdote into an
   actual evaluation section.
3. CPU/latency claim is one data point ("~20% CPU"), not characterized under load or
   thermal throttling inside the full NEPTR effect chain. Latency specifically has never
   been measured at all (only felt as "minimal") — see `HANDOFF_PLAN.md` §1 for the
   two-part measurement plan (algorithmic via offline impulse test + real-deployment
   validation via `testing/latency/`), not yet executed.
4. Verification is manual scripts + ears, not automated regression in CI. Fine for a
   tools paper, but say so rather than imply otherwise.

State 1–4 as future work / limitations. Reviewers trust papers more, not less, when
they name their own scope honestly.

## Test artifact: demo `.csd` — SUPERSEDED, see `HANDOFF_PLAN.md` §3

A first version of this (single file, comment/uncomment profile switching) was built
2026-07-03 and then **deliberately removed** on user instruction — premature, "we
aren't there yet." The design question was reopened: one file vs. several, plus a
"weird/showcase" profile curation idea (3 very different amps + a non-guitar-amp
capture like a preamp). Full options analysis + recommendation now lives in
`HANDOFF_PLAN.md` §3 — read that before building anything here again. Short version:
recommend a single macro-driven template file (not comment/uncomment) + a README table
of curated invocations, once non-guitar-amp profiles are sourced from ToneHunt/Tone3000
(not yet done). `examples/profiles/` still has the 10 calibrated guitar `.nam` files +
`nam_pregain.conf` copied over from the working NEPTR phase4 tree — reusable once the
demo design is actually approved and built.

**Open flag, not blocking:** the bundled `.nam` files are community captures from
ToneHunt/Tone3000; redistribution licensing varies per-uploader and hasn't been
checked. Fine for a private working folder; needs a quick per-file license check
before anything (paper supplementary material, public repo) goes out the door.

## Spectrograms / audio examples

Explicitly deferred per your answer ("later vibe"). When picked back up: derive an
offline-render variant of `nam_demo.csd` (swap `-iadc -odac` for `soundin`/file output),
render dry vs. wet for 2–3 representative profiles (one clean, one edge, the marshall
compression case since it's already a talking point), generate spectrogram figures —
this becomes Figure 2 candidate and directly supports the "does real DSP work, not
passthrough" claim visually.

## Next steps, in order

See `HANDOFF_PLAN.md` for the full hardening roadmap (P0/P1/P2) and demo-file
brainstorm this supersedes in part. Consolidated order:

1. ~~Confirm ICSC 2026 dates/format~~ — done, see top (now sharpened: llncs.cls,
   citation style, free 5th reference page — see `research-paper-style.md`).
2. ~~Locate + verify calibration data~~ — done, Table 1 above is real.
3. ~~Deep research: NAM technical background + novelty claim~~ — done,
   `research-nam-background.md`, novelty claim confirmed double-corroborated.
4. ~~Deep research: paper style/conventions, acceptance plausibility~~ — done,
   `research-paper-style.md`, verdict: plausible well-precedented accept.
5. ~~Build then remove premature demo `.csd`~~ — removed per instruction; redesign
   brainstormed, not yet rebuilt — see `HANDOFF_PLAN.md` §3.
6. **P0 hardening (not started)**: fix/quarantine stale `DEMIURGE_OS/nam/*` copy;
   fix the global-loader-pointer single-instance bug; reference-NAM correctness diff
   (highest leverage single addition — see `HANDOFF_PLAN.md` §2).
7. **Latency measurement (not started)**: both parts of `HANDOFF_PLAN.md` §1.
8. Source non-guitar-amp "weird" profiles from ToneHunt/Tone3000 (needed before the
   demo file redesign can actually be built).
9. Rebuild the demo `.csd` per the approved design in `HANDOFF_PLAN.md` §3.
10. Draft the actual manuscript in the real ICSC `llncs.cls` template (downloaded,
    confirmed in `research-paper-style.md`) from this outline — model the opening on
    Zinemanas et al.'s Ambisonics-decoder paper (closest structural precedent).
11. Spectrograms / audio examples (explicitly deferred, "later vibe").
12. Scrub manuscript for double-blind compliance before CMT submission — no author
    name, no CalArts affiliation, **no project names** (DEMIURGE_OS/NEPTR — NIME's
    stricter norm, since ICSC's own guidance doesn't cover this), no personal
    hostnames/paths in figures or listings, self-citations in third person.
