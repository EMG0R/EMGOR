---
id: emgor.code.cheese
title: CHEESE
blurb: Modular ChucK FX library — nine stereo effects, one shared brain
parent: emgor.code
source: ______PHASE3/cheese/
downloads:
  - files/chorus.ck
  - files/delay1.ck
  - files/phaser.ck
  - files/reverb1.ck
  - files/ringmod.ck
  - files/spectralWah.ck
  - files/pitchShifter.ck
  - files/vibrato.ck
  - files/ANALYSIS.md
tags: [chuck, fx, modular, midi]
updated: 2026-07-28
draft: false
---

# CHEESE

A ChucK-based synth-and-FX system built like a modular rack in code. Live input (ADC) and an internal 4-mode synth feed a serial stereo chain of nine effects, all coordinated through one static global-state class `G` — every effect reads its own `gk*` parameters from the hub, and a MIDI listener maps controller CCs onto those fields (with OSC out to an external UI).

The chain, in order:

**volume pedal → vibrato → chorus → phaser → ring mod → spectral wah → pitch shifter → delay → reverb**

Each effect is its own `.ck` file, hot-loaded and bypassed dynamically at runtime by a connect loop — effects that are off are actually disconnected, not just muted. Highlights:

- **spectralWah** — FFT/IFFT frequency-domain scaling with energy normalization, not a filter sweep
- **delay1** — modulated delay line with a pitch shifter *inside the feedback path* plus tanh saturation
- **phaser** — four-stage BiQuad allpass with feedback
- **chorus / vibrato** — stereo LFO delay lines with per-channel phase offset

The eight effect modules are downloadable above, plus `ANALYSIS.md` — a full architecture writeup of the whole system (signal map, boot sequence, MIDI→globals→audio data flow). The synth engine, globals hub, and MIDI layer live in the source folder.
