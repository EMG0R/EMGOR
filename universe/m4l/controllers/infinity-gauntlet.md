---
id: emgor.m4l.controllers.infinity-gauntlet
title: INFINITY GAUNTLET
blurb: Wearable gesture controller — a glove that sequences Ableton over serial
parent: emgor.m4l.controllers
source: ____INFINITY_GAUNTLET
downloads:
  - files/INFINITY_GAUNTLET_v3.2.1.amxd
tags: [max-for-live, wearable, hardware, arduino, sequencer, monome]
updated: 2025-01-25
---

# INFINITY GAUNTLET

A wearable gesture controller: an Arduino-based glove streaming sensor data over serial (38400 baud) into a Max for Live device that turns gestures into music. The M4L side (v3.2.1, downloadable here) parses the 13-float sensor stream through smoothing (`gen` lag processing), drives an 8-row step sequencer with per-row note maps, and mirrors state onto a monome grid — left and right 128 quadrants, live LED feedback.

## Lineage

Firmware and hardware went through five majors: v3.2 and v4.0 sketches, a v5.1 `.ino` (current firmware head), plus an EWI-adjacent GEORGE fork and a capacitive-touch (CST816) experiment. The M4L device shipped here is the v3.2.1 build — the most complete Live integration in the archive. Later versions moved toward standalone Max (`infinity.maxpat`).

## Use

Without the glove this is a monome-ready 8x8 probability step sequencer; with it, gestures steer the sequence. Drop the `.amxd` on a MIDI track, set the serial port to your glove, point the grid bindings at your monome if you have one.

## Install

Drag the `.amxd` onto a MIDI track. Hardware (glove, monome) optional but recommended — it's in the name.
