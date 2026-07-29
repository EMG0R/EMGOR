---
id: emgor.m4l.machine-lab.modulettes
title: MODULETTES
blurb: M4L interface for the Modulettes, the Machine Lab's small modular robots
parent: emgor.m4l.machine-lab
source: _M4L/_machineLabM4L/DELIVERABLES/MODULETTES_v1.2.2.amxd
downloads:
  - files/MODULETTES_v1.2.2.amxd
tags: [max-for-live, robotics, calarts, percussion, osc]
updated: 2026-03-09
---

# MODULETTES v1.2.2

The Max for Live interface for the Modulettes — the Machine Lab's small modular robotic units. Like the rest of the suite: embedded sample library for composing anywhere, OSC mode (top-left button, on the Skynet network) for playing the physical hardware.

## Use

- Drop on a MIDI track; each unit maps into the device's documented MIDI note range.
- **gen** = generative mode: transport running, click a unit's UI representation to set per-note probability; **subd** = subdivision vs BPM, **chance** = overall probability.

## Install

Drag the `.amxd` onto a MIDI track. Frozen with samples embedded. If editing loses audio files: unfreeze, drag samples back in, re-freeze, save.
