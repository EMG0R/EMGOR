---
id: emgor.code.csound-suite.emgor-fx
title: EMGOR_FX
blurb: Stacked stereo FX rack — phaser to bitcrush — shipped as VST3
parent: emgor.code.csound-suite
source: csound/EMGOR_FX/EMGOR_EFFECTz_v3.csd
downloads:
  - files/EMGOR_EFFECTz_v3.csd
tags: [csound, cabbage, fx, vst3]
updated: 2026-07-28
draft: false
---

# EMGOR_FX v3 (EMGOR_EFFECTz)

A vertical stack of stereo effects in one Cabbage plugin, built and running as a VST3 (`FX_$_DAN.vst3`). Each section sits in its own color block, labeled with a glyph instead of a name — you learn the rack by ear.

Signal path sections:

- **Phaser** — rate, feedback, depth, and up to 256 allpass orders
- **Chorus** and **flanger** — separate sections, separate LFOs
- **Pitch shifter**
- **Modulated delay** (`vdelayx`) with feedback
- **Waveshaping** — foldover distortion, bit reduction, power shaping
- **AM / ring section** and a master **width / gain / dry-wet** utility row

Ships with a named preset bank that tells you what it is actually for: *dry, chorus, ensemble, stepr, stepr noise, hello robot, light delay, slapback delay, tremelo, amplitude modulation, am bitcrush, crazy sauce.*

The built VST3 (~8 MB) lives with the source in the working folder; the full `.csd` is downloadable here and compiles to the same plugin in Cabbage.
