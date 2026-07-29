---
id: emgor.m4l.machine-lab.breakbot
title: BREAKBOT
blurb: M4L interface for the Machine Lab's BreakBot — samples local, OSC live
parent: emgor.m4l.machine-lab
source: _M4L/_machineLabM4L/DELIVERABLES/BREAKBOT_v1.2.amxd
downloads:
  - files/BREAKBOT_v1.2.amxd
tags: [max-for-live, robotics, calarts, drums, osc]
updated: 2026-02-19
---

# BREAKBOT v1.2

The Max for Live interface for BreakBot, a robotic percussion instrument at the CalArts Machine Lab. Write for it from your couch using the built-in sample library of the actual robot; on the lab's Skynet network, flip the top-left button to OSC mode and the same MIDI plays the physical machine.

## Use

- Drop on a MIDI track. Play the robot's MIDI note range like any instrument.
- **gen** button = generative mode: start the transport, click the robot's UI representation to set per-note probability, tune **subd** (subdivision vs BPM) and **chance** (overall probability).
- Top-left button toggles local samples vs live OSC.

## Install

Drag the `.amxd` onto a MIDI track. Frozen device with samples embedded. If you unfreeze and it loses audio files: drag all samples back in, re-freeze, save.
