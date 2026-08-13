---
id: emgor.shipped.neuralgrid
title: neuralGrid
blurb: A touch grid that redesigns its own layout mid-song, without ever adding latency to a note
parent: emgor.shipped
links:
  - { label: "Full hardware + model writeup", url: "#/hardware/neuralgrid" }
tags: [grid, neural, raspberry-pi, esp32, nime]
updated: 2026-08-12
draft: false
---

# neuralGrid

**neuralGrid** is a grid-style touch instrument — 64 backlit pads, in the style of a Monome —
that watches how it's being played and quietly redesigns its own button layout in real time,
timed to swap exactly on the downbeat. The trick that makes it work as an instrument and not a
gimmick: the part making decisions is never allowed to slow down the part making sound.

## What it solves

Instruments that try to be "smart" or adaptive usually pay for it in feel — a machine-learning
model deciding what to do next takes time, and any delay in a musical instrument's response is
immediately audible as lag. neuralGrid splits itself into two halves that never block each
other: a real-time layer, pinned to its own dedicated processor core, that plays notes and
lights pads instantly no matter what; and a separate, slower layer that watches the performance
and prepares the *next* interface layout in the background. The new layout only gets swapped
in on a clean downbeat, so however long the model takes to think, the player never feels it.

## What's actually built

**The core loop is done and verified on real hardware** — this isn't a simulation or a
plan. Working today: the wireless communication protocol between the grid and its Raspberry Pi
brain; a playable instrument mode, sequencer, and MIDI effects mode; the "swap only on the
downbeat" mechanism; and a first working neural model (a compact ~4,500-parameter network) that
actually reconfigures the interface live. Three hand-built, hand-wired sensor layers feed
it — a grid of pads, four distance sensors, and ribbon/pressure sensors for expressive
playing.

**Still ahead:** two more planned models — one that reshapes melody and rhythm together, one
that adds a style dial — plus a round of real playtesting to tune how the interface changes
actually feel under the hands, not just whether they work.

## Where to read more

The full sensor layout, protocol design, and status of every phase is on
[hardware / neuralGrid](#/hardware/neuralgrid).
