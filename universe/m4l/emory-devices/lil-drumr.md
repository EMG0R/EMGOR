---
id: emgor.m4l.emory-devices.lil-drumr
title: LIL_DRUMR
blurb: Generative step drummer that drives the KNOCK drum plugin
parent: emgor.m4l.emory-devices
source: _M4L/EMORY DEVICES/_LIL_DRUMR_v1.2.amxd
downloads:
  - files/LIL_DRUMR_v1.2.amxd
tags: [max-for-live, midi, drums, generative]
updated: 2026-05-01
---

# LIL_DRUMR v1.2

A little generative drum machine. LIL_DRUMR runs a 32-step counter clocked at 16th notes and rolls weighted random dice per step to decide which drum voice fires — probability tables you can nudge, so it grooves without repeating itself. Hits are sent as MIDI into an embedded instance of the KNOCK drum plugin, and can also route out as plain MIDI.

## Dependency note

The device hosts **KNOCK (VST3)** via `vst~` — you need KNOCK installed for the internal sounds. No KNOCK? The MIDI output still works: point it at any drum rack.

## Use

Drop on a MIDI track, start the transport, let it roll. Skew the per-voice probabilities to shape the pattern density.

## Install

Drag the `.amxd` onto a MIDI track. Install KNOCK separately for the built-in voice.
