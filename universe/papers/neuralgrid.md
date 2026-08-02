---
id: emgor.papers.neuralgrid
title: NeuralGrid
blurb: A sensor-dense physical interface for live sequencing with adaptive neural models. Planned.
parent: emgor.papers
source: ______2026NEW/__PAPERS/2026-2027 PAPERS.md
links:
  - { label: "Read as NIME draft", url: "papers/nime/neuralgrid.html" }
  - { label: "NeuralGrid design document", url: "neuralgrid.html" }
tags: [planned, neural-models, sequencer, machine-lab, raspberry-pi, nime]
updated: 2026-07-28
draft: false
---

# NeuralGrid: A Physical Interface for Live Sequencing and Improvisation Using Adaptive Neural Models

**Status: PLANNED** — target venues: NIME, SMC. A deep design document already exists — see the link above.

## Abstract

NeuralGrid is a tactile interface rich with sensors for interacting with an adaptive neural model. The model is pre-trained and runs locally on a Raspberry Pi, with a secondary layer of real-time adaptation that dynamically adjusts the sequencer and MIDI effects to match the user's inputs and reactions. It can feed internal synthesis or operate purely as a sequencer, with parameter mapping across OSC and MIDI to drive complex interfaces and mechanical instruments — the Machine Lab.

## The vision

Reflecting on the Machine Lab system, a universal controller is the logical next step: pre-mapped to the OSC addresses around the room, able to manage the server itself. Turn NeuralGrid on (which launches the server) and anyone new to the room can immediately, intuitively play every instrument in it — the room as an *instrument*, not just a composition platform.

Planned hardware: 4×(4×4) NeoTrellis pads — 64 fully assignable LED pads — with FSRs beneath for velocity, 4 lidar sensors on the corners, 4 ribbon sensors along the edges, and contact mics inside functioning as sensors. Sized for one or many users, with a default mode that is extremely intuitive. The Raspberry Pi constrains model size but keeps the access point recreatable. Hoped-for extensions: onboard synthesis and integration with AI timbre-transfer models (RAVE-adjacent territory).

## People

Emory Smith (primary), with Colton; Ajay possible.
