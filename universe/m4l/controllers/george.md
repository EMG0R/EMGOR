---
id: emgor.m4l.controllers.george
title: GEORGE_SYNTH
blurb: FM/AM synth voiced for an EWI wind controller, built for a collaborator
parent: emgor.m4l.controllers
source: geroge/george_v1.2.2.amxd
downloads:
  - files/GEORGE_SYNTH_v1.2.2.amxd
tags: [max-for-live, synth, fm, ewi, wind-controller]
updated: 2024-09-06
---

# GEORGE_SYNTH v1.2.2

A Max for Live synth built for George — a collaborator playing an EWI (electronic wind instrument). The voice is FM (index + ratio) with an AM section, full ADSR, pitch-shift over a ±2 range, and a built-in stereo delay (independent L/R times, feedback). The key mapping is wind-controller-native: breath and key data are split and scaled across MIDI ranges so expression rides the breath, not the mod wheel.

v1.1 of the folder also carries the standalone `george.maxpat` and a GEORGE fork of the Infinity Gauntlet firmware — this device is the current head (v1.2.2).

## Use

Drop on a MIDI track, point your EWI at it, blow. Works from a keyboard too — it's just a good FM monosynth — but breath control is what it's voiced for.

## Install

Drag the `.amxd` onto a MIDI track. Frozen device, no external dependencies.
