---
id: emgor.code.csound-suite.emgor-synth
title: EMGOR_SYNTH
blurb: 9-partial additive synth with a routable LFO, in one .csd
parent: emgor.code.csound-suite
source: csound/EMGOR_SYNTH/EMGOR_SYNTH_v1.2.csd
downloads:
  - files/EMGOR_SYNTH_v1.2.csd
tags: [csound, cabbage, synth, additive, midi]
updated: 2026-07-28
draft: false
---

# EMGOR_SYNTH v1.2

A MIDI additive synth in a single Csound/Cabbage file. Nine vertical sliders draw the harmonic series directly — drawbar-style, partials 1 through 9 — into a lowpass filter with cutoff and resonant peak, shaped by a full ADSR.

One LFO, three shapes (saw down, saw up, square), routable to **pitch** or **amp** via radio buttons, with rate up to 100 Hz so it crosses from tremolo into audio-rate territory. A global pitch-offset slider detunes the whole instrument around center.

Details:

- On-screen keyboard plus external MIDI (`--midi-key-cps` mapping, velocity to amp)
- Snapshot preset system: save, recall, remove `.snaps` from the UI
- `0dbfs = 1`, stereo out, host-driven sample rate — built to live inside a DAW

The purple-on-black Cabbage skin is the house style. Download the `.csd`, open it in Cabbage, and it is the entire synth.
