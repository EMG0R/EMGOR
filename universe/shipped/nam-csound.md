---
id: emgor.shipped.nam-csound
title: NAM × Csound
blurb: A neural network that models real guitar amps, running live inside a 30-year-old audio language
parent: emgor.shipped
links:
  - { label: "Full paper, abstract, and PDF download", url: "#/papers/nam-csound" }
tags: [csound, neural-amp-modeler, dsp, raspberry-pi, published]
updated: 2026-08-12
draft: false
---

# NAM × Csound

**Neural Amp Modeler (NAM)** is a free tool guitarists use to "capture" the exact sound of a
real amplifier — plug the amp in, run a test signal through it, and a neural network learns to
reproduce it. Thousands of musicians have shared their captures online, covering everything
from vintage tube amps to boutique pedals no one can afford. The catch: until this project,
there was no way to use any of them inside **Csound**, one of the oldest and most flexible
programming languages for sound.

## What it solves

Two problems, both real. First, access: hundreds of thousands of free amp captures existed
with no Csound path in. Second, a quieter problem nobody had solved even for other hosts —
different captures are recorded at wildly different loudness, so swapping between them live
means constantly riding a volume knob to avoid a jump or a clip. NAMProcess measures the
dry input against the wet output as it plays (using the same loudness standard broadcast
audio uses) and rides a slow, continuous gain trim so every capture lands within about 2 dB
of the others — automatically, while you play.

## What's actually built

This one is finished, not planned:

- **A working Csound opcode** — model swaps happen live, mid-performance, with no audio
  dropout and no added latency beyond one processing block (2.7 milliseconds at 48kHz).
- **A drag-and-drop plugin** (NAMDrop, VST3/AU) built with Cabbage — drop a capture file on
  the window and the amp changes underneath your hands.
- **A Raspberry Pi guitar pedal** running 25 captures as foot-switchable tones, using about
  15% of one CPU core.
- **A paper** — "Integrating Neural Amp Modeling Into Csound" — with the above measured and
  written up, submitted to ICSC 2026.

## Where to get it

The full abstract, the measured numbers, and the built PDF are on the paper's own page:
[papers / NAM × Csound](#/papers/nam-csound).
