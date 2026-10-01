---
id: emgor.code.supercollider
title: SUPERCOLLIDER
blurb: GEN_COLIDER — generative FM pieces, mono to 8-channel
parent: emgor.code
source: __SUPAH/
downloads:
  - files/GEN_COLIDER_1.scd
  - files/GEN_COLIDER_1.2.scd
  - files/GEN_COLIDER_1.3.MULTI.scd
tags: [supercollider, generative, fm, multichannel]
updated: 2026-07-28
draft: false
---

# SUPERCOLLIDER — GEN_COLIDER

A generative FM piece grown across three versions, ending in an 8-channel multichannel build (`1.3.MULTI` boots the server with `numOutputBusChannels = 8` and 128 MB of real-time memory).

The core is one dense `\fmSynth` SynthDef: FM carrier/modulator with percussive envelopes, a pitch envelope whose attack/hold/decay are *fractions of the amp envelope's total time* (so pitch motion scales with note length), vibrato, chorus, a feedback delay with LFO-modulated delay time via `LocalIn`/`LocalOut`, reverb, and saturation — the whole voice-plus-FX chain inside a single synth so every note carries its own space.

Above the SynthDef, the generative layer: 170 BPM grid, a master trigger probability, and a weighted note table (`normalizeSum`) leaning on the diatonic degrees with a rare tritone at weight 0.1 — the wrong note, allowed occasionally, on purpose. Global `pitchEnvAmt` / `ampEnvAmt` macros morph the whole texture from clicks to swells while it runs.

All three `.scd` versions are downloadable — v1 (stereo seed), v1.2, and v1.3.MULTI. Run top block first, then the synth block, in the SuperCollider IDE.
