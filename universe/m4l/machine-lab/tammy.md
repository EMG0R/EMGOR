---
id: emgor.m4l.machine-lab.tammy
title: TAMMY
blurb: M4L interface for TAMMY, one of the Machine Lab's robotic instruments
parent: emgor.m4l.machine-lab
source: _M4L/_machineLabM4L/DELIVERABLES/TAMMY_v1.2.amxd
downloads:
  - files/TAMMY_v1.2.amxd
tags: [max-for-live, robotics, calarts, osc]
updated: 2026-02-19
---

# TAMMY v1.2

The Max for Live interface for TAMMY, one of the robotic instruments at the CalArts Machine Lab. Same deal as the rest of the suite: a sample library of the real instrument for writing anywhere, and OSC control (top-left button, on the Skynet network) to play the physical robot.

## Use

- Drop on a MIDI track; play TAMMY's MIDI note range.
- **gen** = generative mode: with the transport running, click the instrument's UI representation to set per-note probability; **subd** = subdivision vs BPM, **chance** = overall probability.

## Install

Drag the `.amxd` onto a MIDI track. Frozen with samples embedded. If editing loses audio files: unfreeze, drag samples back in, re-freeze, save.
