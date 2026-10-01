---
id: emgor.code.csound-suite.generative-samplr
title: EMGOR_GENERATIVE_SAMPLR
blurb: Probabilistic 8-slot sampler — drag, set chance, let it play
parent: emgor.code.csound-suite
source: csound/EMGOR_GENERATIVE_SAMPLR/EMGOR_GEN_SAMPLER_v3.2.csd
downloads:
  - files/EMGOR_GEN_SAMPLER_v3.2.csd
tags: [csound, cabbage, sampler, generative, au]
updated: 2026-07-28
draft: false
---

# EMGOR_GENERATIVE_SAMPLR v3.2

A generative sampler built as an Audio Unit (`_EMGOR_GENERATIVE_SAMPLR_v3.1.5.component`, in use). Eight sample slots, each a drag-and-drop target — the UI literally says *"drag files onto each button and click to edit :0"*.

Every slot gets four per-sample controls:

- **chance** (0–100%) — probability the slot fires on its trigger
- **freak** (0–2×) — playback rate / pitch
- **vol** and **pan**

The slot grid, waveform view (`soundfiler`), and all per-slot widgets are *generated procedurally by Csound at load time* — an `instr SampleSlot` builds each slot's UI with `cabbageCreate` and `sprintf`'d widget code, so the layout is data, not hand-placed markup. Samples resolve relative to the `.csd` via a `getFullPath` opcode and ship in a `GSresources/` bundle, with snapshot presets on top.

Point it at a folder of hits, dial chance down, and it becomes a drum machine that never plays the same bar twice.
