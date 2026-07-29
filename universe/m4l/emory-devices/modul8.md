---
id: emgor.m4l.emory-devices.modul8
title: MODUL8
blurb: LFO/modulation matrix — map wave shapes onto any Live parameter
parent: emgor.m4l.emory-devices
source: _M4L/EMORY DEVICES/_MODUL8_v1.2.amxd
downloads:
  - files/MODUL8_v1.2.amxd
tags: [max-for-live, modulation, lfo, live-api]
updated: 2024-05-15
---

# MODUL8 v1.2

A modulation device. MODUL8 generates LFO shapes — sine, triangle, saw (phasor), square — through a signal matrix, scales and offsets the result, and pushes it onto any parameter in your Live set via the Live API. Click **Map**, click a knob anywhere in Ableton, and that knob now moves itself. Device IDs are stored and restored with the set, with an auto-off safety when unmapped.

## Use

1. Drop `MODUL8.amxd` anywhere in a track's device chain (audio passes through untouched).
2. Hit Map, then click the target parameter in Live.
3. Pick a shape, set rate/range/offset. Done.

## Install

Drag the `.amxd` onto a track. Frozen device, no external dependencies.
