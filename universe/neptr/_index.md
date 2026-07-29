---
id: emgor.neptr
title: NEPTR
blurb: Flagship live guitar instrument — four generations of one machine
parent: emgor
source: _____pi / ______PHASE2 / ______PHASE3 / ______2026NEW
links:
  - { label: "NEPTR web (RNBO export)", url: "NEPTR.html" }
tags: [instrument, guitar, raspberry-pi, csound, chuck, max, live-performance]
updated: 2026-07-28
draft: false
---

# NEPTR

NEPTR is the instrument. One lineage, 2023 → now: a guitar effects computer built, gigged, torn down, and rebuilt four times. Every phase kept the same job — sit between a guitar and a real amplifier and be playable with your feet and two hands mid-song — and threw out everything else.

## The lineage

**Phase 1 (2023–2025) — Max/RNBO on a Pi.** Patches `neptr_v3` through `v6.5`, compiled through RNBO onto a Raspberry Pi running the rnbooscquery image. Kivy touchscreen UI, custom button PCBs, a 3D-printed enclosure. Performed at ICMC. Ended at `neptr_v6.5(endPHASE1)`.

**Phase 2 (2025) — Csound, native.** RNBO out, hand-written Csound in. One big `.csd` running straight on the Pi, a Python/Kivy face-drawing UI (protoFACES), and a side quest porting the engine to Bela.

**Phase 3 (2025–2026) — ChucK.** Strongly-timed rewrite: `neptrSYNTH1.ck` and the ChuckTer terminal instrument, Teensy hardware I/O, a new 3D-printed body and head. The `cheese` ChucK FX library grew out of this era and became its own thing.

**Phase 4 (2026–now) — the current machine.** Csound on a Pi 5 ("DEMIURGE") talking raw ALSA at ~7 ms round trip, 19 menus of circuit-modeled and spectral effects, Neural Amp Modeler at the end of the chain, a 7-encoder Teensy controller, a 5-channel looper, and a pygame touchscreen. This is the one on stage.

**SPEAK** rides alongside: a Speak-&-Spell voice synth that became the voice of "neptr" — a fully offline AI companion living in the same hardware family.

Zoom into a phase for the real documentation and source artifacts.
