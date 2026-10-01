---
id: emgor.code.csound-suite.gen-sequencer
title: GEN_SEQUENCER
blurb: Grid step-sequencer prototype — Cabbage UI generated from code
parent: emgor.code.csound-suite
source: csound/_EMGOR_GEN_SEQUENCER_v1.csd
downloads:
  - files/_EMGOR_GEN_SEQUENCER_v1.csd
tags: [csound, cabbage, sequencer, prototype]
updated: 2026-07-28
draft: false
---

# GEN_SEQUENCER v1

An honest prototype. The interesting part is the technique: an `instr grid` spawns the entire step grid at runtime — each cell is a Cabbage checkbox created by `cabbageCreate` from `sprintf`'d widget code, positioned by loop indices. The UI is an instrument.

An early `SEQUENCER` instrument (a metronome walking a note array and firing events) is still in the file, commented out — the fossil record of where this was headed. The procedural-UI approach proved out here and became the slot system in **EMGOR_GENERATIVE_SAMPLR**.

Kept in the suite because prototypes are part of the work. Download the `.csd` above.
