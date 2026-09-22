---
id: emgor.papers.future.neuralgrid
title: NeuralGrid
blurb: A grid, ribbon, and lidar controller that speaks a symbolic vocabulary before it speaks sound.
parent: emgor.papers.future
source: ______2026NEW/neuralGrid
links:
  - { label: "Read as paper", url: "papers/nime/neuralgrid.html" }
  - { label: "NeuralGrid design document", url: "neuralgrid.html" }
tags: [neural-models, sequencer, machine-lab, raspberry-pi, nime, tabla, protocol]
updated: 2026-09-21
draft: false
---

# NeuralGrid

**Status: mid-build.** First encoder working on hardware; sensor smoothing and interpolation producing a coherent output format. Target venue: NIME. With Ajay Kapur and Jake Cheng.

## The instrument

A sensor-dense physical controller — a grid of pads, ribbons along the edges, lidar at the corners, contact mics inside — running on a Raspberry Pi. Sensor input is smoothed and interpolated into a coherent output format defined by the dimensional space those sensors actually represent, rather than by what each sensor happens to emit.

Its original context is the Machine Lab: a universal controller pre-mapped to the OSC addresses around the room, able to launch and manage the server itself, so anyone new can walk in and play every instrument in the room. The room as an instrument, not a composition platform.

## Two layers, and the first one is language

**The symbolic layer.** The design turned on a realization about *bols* — the spoken vocabulary of strokes in tabla. Tabla already formalizes what generative pattern variation tries to do: *theka* and *taal* hold a cycle, while *tihai* and *kaida* act as geometric transforms over sequences of bols. That is a complete, tested, centuries-old grammar for playing, combining, and varying phrases, and it is *spoken* — the vocabulary is vocally synthesizable by construction.

So the first model interprets gesture from spatial input through a symbolic vocabulary of this kind: recognizing phrases, holding a consistent cycle, and applying generative modulation as transforms over the symbol sequence rather than over audio. The output can be spoken aloud as readily as played, and there is room to extend the vocabulary into English symbolism.

This is a borrowing from a living tradition, and the paper treats it as one — named as such, cited to the tabla scholarship, not presented as a found abstraction. It is also well-precedented ground for this collaboration: Ajay Kapur's ETabla, Electronic Dholak, and MahaDeviBot work is exactly the translation of North Indian percussion into controllers and robots.

**The translator layer.** The second model maps symbolic output onto any synthesis engine through seven perceptual axes — frequency, loudness, harmonicity, rate, scale, space, and time. Hardcoded regions of the translator's behavior define specific *instruments*; loosening them lets more of the instrument be generatively shaped, until the whole thing is authored by the translator's reactions to the player.

The translator is **deaf**: it never hears what it renders. It responds to the player and to the symbol stream, not to the resulting sound. That separation is the point — it keeps model latency structurally out of the audio path, because a model that never listens never has to keep up.

## Why the split matters

The model never sits in the hot path. A real-time layer reads sensors, plays notes, and drives LEDs without blocking on anything; the models run beside it and stage their results, which get swapped in on a boundary. If a model takes 200 ms, nothing is late.

## What's next

Extend from the working encoder to the full sensor complement; implement the symbolic vocabulary and its transforms; then the translator and the seven-axis mapping. The Machine Lab renderer and a browser-based controller/renderer pair are the two deployment targets that would let a reader try it without building anything.

## People

Emory Smith, Ajay Kapur, Jake Cheng.
