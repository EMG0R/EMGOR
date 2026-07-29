---
id: emgor.m4l.emory-devices.light-music-controller
title: LIGHT_MUSIC_CONTROLLER
blurb: Sensor-to-CC bridge — accelerometer, gyro and touch become MIDI control
parent: emgor.m4l.emory-devices
source: _M4L/EMORY DEVICES/LIGHT_MUSIC_CONTROLLER.amxd
downloads:
  - files/LIGHT_MUSIC_CONTROLLER.amxd
tags: [max-for-live, midi, controller, sensors]
updated: 2025-02-04
---

# LIGHT_MUSIC_CONTROLLER

The control half of the LIGHT_MUSIC pair. This device takes incoming sensor streams — accelerometer (x/y/z), gyroscope (pitch/yaw/roll), pressure, and XY touch position — scales each to 0–127, and emits them as MIDI CCs 1–23. That turns any motion source into a mappable controller for LIGHT_MUSIC, or for anything else in Live that accepts CC.

## Use

1. Drop on a MIDI track and route the sensor input in.
2. Each sensor axis lands on its own CC (gated on/off per channel).
3. MIDI-map the CCs in Live, or route the track's MIDI output at LIGHT_MUSIC.

## Install

Drag the `.amxd` onto a MIDI track. Frozen device.
