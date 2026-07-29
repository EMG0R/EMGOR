---
id: emgor.m4l.emory-devices.light-music
title: LIGHT_MUSIC
blurb: Live visuals engine running inside Ableton — OpenGL, noise, crossfades
parent: emgor.m4l.emory-devices
source: _M4L/EMORY DEVICES/LIGHT_MUSIC_v2.3.amxd
downloads:
  - files/LIGHT_MUSIC_v2.3.amxd
tags: [max-for-live, visuals, jitter, opengl, performance]
updated: 2025-02-01
---

# LIGHT_MUSIC v2.3

The visuals rig. LIGHT_MUSIC is a Jitter/OpenGL video engine that lives in an Ableton device chain, so the light show runs from the same session as the music. Inside: video playback on GL planes, generative noise fields, brightness/contrast/saturation processing (`jit.brcosa`), crossfading between layers (`jit.xfade`), and custom `jit.gen` shader-style processing — all switchable and performable in real time.

Control inputs are wired for performance: shoulder/trigger/stick receivers suggest a gamepad at the helm, and the companion **LIGHT_MUSIC_CONTROLLER** device feeds it CC data from motion sensors.

## Use

1. Drop `LIGHT_MUSIC_v2.3.amxd` onto a track in your performance set.
2. Open the render window, load your video sources.
3. Drive it from mapped MIDI/CC — pair with LIGHT_MUSIC_CONTROLLER for sensor control.

## Install

Drag the `.amxd` onto a track. Frozen device. GPU does the heavy lifting.
