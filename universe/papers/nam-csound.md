---
id: emgor.papers.nam-csound
title: NAM × Csound
blurb: NAMProcess — real-time Neural Amp Modeler inference as a Csound opcode. Complete, submitted to ICSC 2026.
parent: emgor.papers
source: ______2026NEW/__PAPERS/Integrating Neural Amp Modeling Into Csound/deliverable/paper/nam-csound-icsc.pdf
downloads:
  - files/nam-csound-icsc.pdf
tags: [csound, neural-amp-modeler, dsp, raspberry-pi, cabbage, published]
updated: 2026-07-28
draft: false
---

# Integrating Neural Amp Modeling into Csound

**Status: COMPLETE — submitted to ICSC 2026.** The built PDF is downloadable below.

## Abstract

Neural Amp Modeler (NAM) is the premier open-source system for black-box modeling of nonlinear audio hardware with neural networks. A community library of several hundred thousand pre-trained captures exists — amps, pedals, preamps, whole chains — but until now there was no way to use them inside Csound. This paper presents **NAMProcess**, a Csound opcode that performs real-time NAM inference: it loads standard `.nam` files, switches models mid-performance without interrupting audio, adds no latency beyond the block boundary, and runs comfortably in real time on a Raspberry Pi.

## What's actually in it

- **The opcode.** A thin wrapper around the NeuralAmpModelerCore C++ library, registered as a native plugin opcode. Model selection is an ordinary k-rate parameter, so timbre choice is open to the same rule-based and generative processes as any other Csound value. Asynchronous, click-safe loading: the audio thread never allocates, parses JSON, or blocks on I/O.
- **Live volume compensation.** The genuinely new part. Community captures fed the same signal span about 10 dB, and saturating models compress — no static per-model gain can equalize them. NAMProcess meters dry input against wet output (ITU-R BS.1770 K-weighted) and rides a slow trim on their ratio. Every capture lands within ±2 dB of the dry signal, live, including captures no input gain can fix. To our knowledge no published NAM host does this.
- **A drag-and-drop Cabbage plugin** (NAM Drop, VST3/AU) — drop a `.nam` file on the window, the amp hot-swaps under your hands.
- **A Raspberry Pi guitar pedal** where 25 captures act as a bank of foot-switchable tones — 2.7 ms added latency, ~15% of one core.

## Why it matters here

This is the paper that proves the workbench loop closes: research question → opcode → measurement → plugin → pedal → stage. The same engine plays in a DAW and on the floor.

**Latency:** one k-cycle (128 samples, 2.7 ms at 48 kHz). **Stereo:** two independent model instances per channel — sharing one measures −4.7 dB crosstalk. **Compensation:** capture-library loudness spread reduced from 9.9 dB (best static probe) to 3.1 dB, live.
