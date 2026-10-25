---
id: emgor.web-synth.pmor
title: PMOR
blurb: The pocket groovebox, ported pad-for-pad to the browser — 4x4 pads, two encoders, the OLED, built for a phone in one hand
parent: emgor.web-synth
source: ______2026NEW/Pocket-OpGorator/DaisyExamples/seed/POCKETGORATOR_CODE/POCKETGORATOR_CODE.cpp
launch: PMOR.html
links:
  - { label: "Play PMOR", url: "PMOR.html" }
  - { label: "The hardware (PMOR planet)", url: "index.html#/papers/neptr-performance-system/pocket-opgorator" }
tags: [web-audio, audioworklet, groovebox, sequencer, sampler, iphone, daisy]
updated: 2026-10-25
draft: false
order: 0
---

# PMOR (web)

**Language: C++ firmware, re-written by hand in JavaScript.** PMOR is a Daisy Seed
groovebox: a 4x4 NeoTrellis pad grid, two encoders, a 128x64 OLED, twelve wavetable
voices, four drum channels, a sixteen-slot two-second sampler, eight banks of pad-held
effects and a sixteenth-note sequencer that never stops. This page is that firmware
moved into an AudioWorklet, line for line where the sound depends on it: the same
eight additive wavetables, the same state-variable filters and tanh stages, the same
swing (even steps long, odd steps short), the same HALF/DBL/STUTTER tempo tricks, the
same lava palette drifting across the pads.

The hardware controls map onto the phone as they sit on the unit, top to bottom:
OLED, two encoders, pads. Drag an encoder to turn it, tap to click, hold to clear.
Left encoder changes page (SYNTH, DRUMS, SAMPLER, FX, PATCHES, DASH, GLOBALS);
right encoder moves through what the page shows. Hold pads on the FX page to play
effects; hold a sampler pad for a third of a second to record into it from the mic.
Patches live in the browser, sixteen slots, like the SD card.

What the port deliberately does not carry over: USB audio and MIDI, the SD card,
the battery, the bootloader. Everything you can hear, it does.
