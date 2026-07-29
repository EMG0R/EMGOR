---
id: emgor.m4l.emory-devices.granola
title: GRANOLA
blurb: Granular ring-buffer mangler — feed it live audio, gobble it back
parent: emgor.m4l.emory-devices
source: _M4L/EMORY DEVICES/_GRANOLA_v1.3.1.amxd
downloads:
  - files/GRANOLA_v1.3.1.amxd
tags: [max-for-live, audio-effect, granular]
updated: 2025-09-04
---

# GRANOLA v1.3.1

A granular delay/mangler built around a one-second ring buffer (internally named `gobble` — accurate). Incoming audio is continuously written into the buffer; playback heads look some number of samples into the past, wrap around, and re-read it with randomized, scaled grain behavior. You can also drag audio files straight in.

Descends from the lab's `audiograin` patcher family (v2, v2.2, v3 in the dependancies folder) — this is the frozen, playable head of that line.

## Use

Drop on an audio track and feed it anything — a live mic, a synth, a loop. Delay-time and grain controls sweep the read head through the buffer. Fair warning from the patch itself: beware of feedback if using laptop mic and speakers.

## Install

Drag the `.amxd` onto an audio track. Frozen device, tiny footprint (~80KB).
