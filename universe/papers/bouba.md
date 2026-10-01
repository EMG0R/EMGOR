---
id: emgor.papers.season-2026-27.bouba
title: BOUBA
blurb: A quadraphonic therapeutic sound object, built — plus a new accessible prototype and a free web version.
parent: emgor.papers.season-2026-27
source: luthier_paper_v2/rotura_submission.md
links:
  - { label: "Read as paper", url: "papers/nime/bouba.html" }
tags: [music-therapy, generative, teensy, quadraphonic, tangible-interface, wellbeing]
updated: 2026-09-21
draft: false
---

# BOUBA

**Status: built, and being rebuilt.** The first BOUBA exists and has been in consistent use for months. It appears as one of the two case studies in the [Luthier paper](/papers/season-2026-27/digital-luthier). What's active now is the second generation plus the web version, and the user-participation work that pair makes possible.

## What it is

A quadraphonic sound object providing tactile interaction with therapeutic generative synthesis, for the non-musician seeking a psychologically beneficial relationship with sound. It targets four pillars of human experience: sleep, mood, productivity, and autonomic regulation.

No screen, no app, no subscription, no account. Onboard controls select and interpolate between generative presets, trigger notes manually, and adjust volume, probability, and pitch. The state lives nowhere but inside the object.

## The built version

- **Teensy Audio Library**, centered on generative compositions that are self-sufficient but interactive
- **Four speakers**, quadraphonic, from two PCM5102 DACs amplified by four LM386s
- **No external connections but power** — a standard 9 V jack, stepped down per component, so it runs from common cables or batteries
- **About \$80 USD to reproduce**
- 3D-printed enclosure positioning the speakers for quadraphonic imaging while centralizing controls for single-handed operation

Evaluated against O'Modhrain's framework: playability deliberately low (a minimal control surface is the point), learnability high, sonic range intentionally constrained to consonant ambient territory, robustness high over two months of consistent use.

## What's being built now

**A second hardware prototype**, distinct from the original, designed for the two things the first one traded away: accessibility of reproduction, and sound quality.

**A free web version** of the same generative system, so anyone can use it without building anything.

Together these make something the object alone could not: a comparison between how people relate to a physical sound object and how they relate to the identical generative system in a browser. Very few systems can run that comparison, and it is the most interesting question either version raises.

## The research baseline

Each preset operationalizes published findings, cited in full in the Luthier paper:

- **Sleep** — white noise improving sleep in coronary care patients (Farokhnezhad Afshar et al., 2016); sine-based music reducing sleep latency 38% with increased delta power (Gao et al., 2020). Informs the filtered noise layers and generative sine waves.
- **Mood** — natural soundscapes facilitating mood recovery (Benfield et al., 2014); consonant harmonic relationships engaging paralimbic reward centers while dissonance activates amygdala regions (Blood et al., 1999). Informs the nature presets and the restriction to consonant intervals.
- **Productivity** — 45 dB white noise raising attention and creativity via stochastic resonance (Awada et al., 2022); rhythmic entrainment at 120–140 BPM improving work output (Karageorghis et al., 2011).
- **Autonomic regulation** — salivary cortisol halved post-stressor with ambient music (Khalfa et al., 2003); significant physiological stress reduction across music interventions, d = 0.380 (De Witte et al., 2019).

BOUBA does not replicate the clinical conditions of any cited study. The relationship is one of informed design, and the object makes no clinical efficacy claim.

## Where the paper goes

Not a clinical venue. The contribution is the interface and what people do with it — a tangible-interface and research-through-design paper, targeting **TEI** or **DIS**, with **Audio Mostly** as the faster route.

The evaluation is qualitative: semi-structured interviews with a normal-for-the-field sample, thematic analysis, a standardized interface-quality instrument (AttrakDiff or UEQ), and usage telemetry from the web build — which presets people actually return to, how long sessions run, what time of day. Any study involving participants goes through institutional review before a single response is collected.

## People

Emory Smith.
