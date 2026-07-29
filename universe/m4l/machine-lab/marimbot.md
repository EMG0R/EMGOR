---
id: emgor.m4l.machine-lab.marimbot
title: MARIMBOT
blurb: M4L interface for the Machine Lab's robotic marimba
parent: emgor.m4l.machine-lab
source: _M4L/_machineLabM4L/DELIVERABLES/MARIMBOT_v1.2.2.amxd
downloads:
  - files/MARIMBOT_v1.2.2.amxd
tags: [max-for-live, robotics, calarts, marimba, osc]
updated: 2026-03-09
---

# MARIMBOT v1.2.2

The Max for Live interface for MarimBot, the CalArts Machine Lab's robotic marimba. Compose melodically from anywhere with the embedded sample library of the actual instrument; on the lab's Skynet network, flip the top-left button to OSC mode and your MIDI moves real mallets.

## Use

- Drop on a MIDI track and play within MarimBot's MIDI note range — it's a pitched instrument, treat it like one.
- **gen** = generative mode: transport running, click the instrument's UI representation to set per-note probability; **subd** = subdivision vs BPM, **chance** = overall probability.

## Install

Drag the `.amxd` onto a MIDI track. Frozen with samples embedded. If editing loses audio files: unfreeze, drag samples back in, re-freeze, save.
