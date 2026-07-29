---
id: emgor.m4l.machine-lab.gana-pati
title: GANA_PATI
blurb: M4L interface for the Machine Lab's GanaPati percussion robot
parent: emgor.m4l.machine-lab
source: _M4L/_machineLabM4L/DELIVERABLES/GANA_PATI_v1.2.amxd
downloads:
  - files/GANA_PATI_v1.2.amxd
tags: [max-for-live, robotics, calarts, percussion, osc]
updated: 2026-02-19
---

# GANA_PATI v1.2

The Max for Live interface for GanaPati, a robotic percussion instrument at the CalArts Machine Lab. Embedded sample library for composing anywhere; OSC mode (top-left button, on the lab's Skynet network) to perform on the physical robot.

## Use

- Drop on a MIDI track and play the robot's MIDI note range.
- **gen** = generative mode: transport running, click the instrument's UI representation to set per-note probability; **subd** sets subdivision relative to BPM, **chance** overall probability.
- Toggle local sampling / live OSC top-left.

## Install

Drag the `.amxd` onto a MIDI track. Frozen with samples embedded. If editing loses audio files: unfreeze, drag samples back in, re-freeze, save.
