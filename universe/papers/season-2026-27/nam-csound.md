---
id: emgor.papers.season-2026-27.nam-csound
title: NAM × Csound
blurb: NAMProcess — real-time Neural Amp Modeler inference as a Csound opcode, with live volume compensation. Complete.
parent: emgor.papers.season-2026-27
source: ______2026NEW/__PAPERS/Integrating Neural Amp Modeling Into Csound
downloads:
  - files/nam-csound-icsc.pdf
  - files/nam-opcode/csound_nam_opcode.cpp
  - files/nam-opcode/nam_loader.cpp
  - files/nam-opcode/nam_loader.h
  - files/nam-opcode/kweight.h
  - files/nam-opcode/di_probe.h
  - files/nam-opcode/CMakeLists.txt
  - files/nam-opcode/build-on-pi.sh
  - files/nam-opcode/test_nam_passthrough.csd
  - files/nam-opcode/nam_bench.csd
  - files/nam-opcode/analyze.py
links:
  - { label: "Read as paper", url: "papers/nime/nam-csound.html" }
  - { label: "The paper (PDF)", url: "papers/nime/render/nam/nam-csound-icsc.pdf" }
tags: [csound, neural-amp-modeler, dsp, raspberry-pi, cabbage, opcode, complete]
updated: 2026-09-21
draft: false
---

# Integrating Neural Amp Modeling into Csound

**Status: complete and submitted.** The paper is written, the PDF is downloadable below, and the opcode source ships with it. The venue slipped to 2027; the work did not. It runs inside [DEMIURGE](/papers/season-2026-27/demiurgeos) and inside [NEPTR](/papers/season-2026-27/neptr-performance-system) today.

## Abstract

Neural Amp Modeler (NAM) is the most widely used open-source system for black-box modeling of nonlinear audio hardware using neural networks. A community library of several hundred thousand pre-trained captures exists — over 350,000 on TONE3000 alone — but there has been no way to use them inside Csound. This paper presents **NAMProcess**, a Csound opcode that performs real-time NAM inference. It loads standard `.nam` files, switches between models without interrupting audio, adds no algorithmic latency, and runs comfortably in real time on systems like the Raspberry Pi. A live dry-referenced volume compensation system built into the opcode holds every capture in a heterogeneous 25-model test library within ±2 dB of the player's volume.

## The opcode

```
aoutL, aoutR NAMProcess ainL, ainR, kProfileIdx, SLibDir [, kAutoVol, kDriveDb]
icount       NAMCount
```

`SLibDir` points at a folder, scanned recursively for `.nam` files at init. The sorted file list gives every model a stable index, and `kProfileIdx` picks one at k-rate. Because model choice is just another k-rate value, tone selection is open to the same rule-based and generative processes as everything else in Csound — selection procedures in Koenig's sense, not preset browsing.

Everything touching files or weights runs on a background thread. The audio thread never allocates, parses JSON, or blocks on I/O; per k-cycle it takes one uncontended mutex acquisition to copy a pointer to the active model pair. A failed load keeps the previous model running, so a corrupt file cannot interrupt a performance, and swaps land on a block boundary, click-free.

## Live volume compensation

The genuinely new part. Fed the same signal, the 25 representative captures span **17.3 dB** (σ 4.3 dB) even after a load-time normalization probe, and embedded loudness metadata does not predict the ranking. No pre-computed correction can close the gap, because saturating models compress — one capture holds its output within 1 dB across a 14 dB input sweep, so no per-model gain equalizes it at all.

So the correction is live. Dry input and wet output are metered as ITU-R BS.1770 K-weighted mean squares with a 2.5 s time constant, and a slow output trim tracks their ratio. Neither meter observes the trim's own effect, so the loop is feed-forward and cannot oscillate.

Measured on the shipped system: the live loop collapses the 17.3 dB spread to **3.6 dB** (σ 0.74 dB), all but one capture within ±2 dB of the dry signal. On the guitar material used during development, 3.1 dB (σ 0.83 dB) against 9.9 dB (σ 2.60 dB) for the best static probe evaluated. Because the dry reference is tapped before the input stage, `kDriveDb` pushes the model into saturation without a level change: +18 dB of drive measured +0.1 dB of loudness and −4.6 dB of crest factor.

## Implementation notes worth passing on

- **Opcode registration.** Registered the conventional way for an audio-rate generator — `OENTRY` with `thread = 5` — the a-rate callback never gets scheduled under Csound 6.18's plugin path, and the opcode emits silence with no error. What works is `thread = 3`, computing the whole `ksmps` block inside the k-rate callback; declaring `outypes = "aa"` still allocates the a-rate outputs.
- **Stereo statefulness.** NAM models are stateful — WaveNet layers keep convolution history, LSTMs carry hidden state. Running both channels through one shared instance convolves every block against the other channel's immediate past, measuring **−4.7 dB crosstalk** on decorrelated stereo material. NAMProcess loads two independent instances per file and publishes them as a pair.
- **Sample-rate mismatch.** A model trained at a different rate gets wrapped in a Lanczos resampling container so the network always runs at its trained rate. A 48 kHz model under a 44.1 kHz engine matches the matched-rate reference within 0.3 dB; without it the learned filtering stretches by the rate ratio and the voicing audibly shifts.

## Latency

The supported architectures are causal, so the opcode adds no algorithmic delay — offline impulse renders put response onset within one sample of the input impulse. What remains is the `ksmps` block granularity every Csound opcode shares: 128 samples, **2.7 ms** at 48 kHz in the pedal deployment. CPU load with a full-quality WaveNet-family model has been informally observed around 15% of the audio core during performance.

## Where it ships

Three deployments: the raw opcode, a drag-and-drop **Cabbage VST3/AU plugin** whose window is a single drop target, and **tone selection in the Raspberry Pi pedal** where the 25-capture library acts as a bank of foot-switchable tones.

## Downloads

The submitted PDF, plus the opcode source, build files, test orchestra, and benchmark tooling.

## People

Emory Smith.
