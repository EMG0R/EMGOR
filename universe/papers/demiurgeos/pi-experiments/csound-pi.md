---
id: emgor.papers.demiurgeos.pi-experiments.csound-pi
title: Csound on Pi
blurb: Headless Csound sketches — generative drums and nonlinear reverb
parent: emgor.papers.demiurgeos.pi-experiments
source: __csound_PI
downloads:
  - files/drums1.csd
  - files/Nonlinear_Daylight-EMGOR.csd
tags: [csound, generative, raspberry-pi, headless]
updated: 2026-07-28
draft: false
---

# Csound on Pi

Two self-contained `.csd` files written to run headless on a Raspberry Pi — no GUI, no editor, just `csound -odac file.csd` and sound.

## drums1.csd

A generative rhythm engine. A master driver instrument sweeps BPM continuously (50–150) and derives its own subdivision count from the current tempo, so the groove density breathes with the speed. Synthesized voices from two wavetables (sine + noise), everything self-scheduling — no score, no sequencer, no input.

## Nonlinear_Daylight-EMGOR.csd

A reverb-first piece: room size sits near the edge of infinite (0.98) and is itself modulated by random walks, so the space never settles. Written as a long-form ambient generator — start it and leave the room.

Both predate DEMIURGE and demonstrate the pattern it later formalized: a Csound file **is** the instrument, the Pi **is** the performer. Under DEMIURGE either file drops straight into a `chain =` line and inherits the clock, the MIDI bus, and hot-plug-proof output for free.
