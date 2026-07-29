---
id: emgor.m4l.emory-devices.efex
title: EFEX
blurb: Motion-controlled effects — your phone's tilt becomes the mod source
parent: emgor.m4l.emory-devices
source: _M4L/EMORY DEVICES/_EFEX.amxd
downloads:
  - files/EFEX.amxd
tags: [max-for-live, audio-effect, sensors, osc]
updated: 2024-05-22
---

# EFEX

An audio effect you steer with motion. EFEX listens on UDP port 3333 for sensor data from a phone (accelerometer, gyroscope, quaternion orientation), converts it to Euler angles and spherical coordinates inside a `gen` patcher, and maps pitch/roll/yaw onto effect parameters — plus a jittered LFO section (freq, amp, jitter amount) for movement when your hands are busy.

Wave your phone, the mix moves.

## Use

1. Drop `EFEX.amxd` onto an audio track.
2. Run a sensor-streaming app on your phone (anything that sends motion data over UDP/OSC), pointed at your computer's IP, port 3333. Phone and computer on the same network.
3. Tilt to modulate. The LFO section runs standalone if you'd rather not wave anything.

## Install

Drag the `.amxd` onto an audio track. Frozen device.
