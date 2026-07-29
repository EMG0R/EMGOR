---
id: emgor.m4l.machine-lab.lydia
title: LYDIA
blurb: M4L interface for LYDIA, one of the Machine Lab's robotic instruments
parent: emgor.m4l.machine-lab
source: _M4L/_machineLabM4L/DELIVERABLES/LYDIA_v1.2.amxd
downloads:
  - files/LYDIA_v1.2.amxd
tags: [max-for-live, robotics, calarts, osc]
updated: 2026-03-02
---

# LYDIA v1.2

The Max for Live interface for LYDIA, one of the robotic instruments at the CalArts Machine Lab. The largest sample library in the suite (~19MB frozen), so writing away from the lab still sounds like the real machine. On the Skynet network, the top-left button switches to OSC mode and your MIDI drives the physical robot.

## Use

- Drop on a MIDI track; play LYDIA's MIDI note range.
- **gen** = generative mode: transport running, click the instrument's UI representation to set per-note probability; **subd** = subdivision vs BPM, **chance** = overall probability.

## Install

Drag the `.amxd` onto a MIDI track. Frozen with samples embedded. If editing loses audio files: unfreeze, drag samples back in, re-freeze, save.
