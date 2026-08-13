---
id: emgor.shipped.neptr
title: NEPTR
blurb: A guitar effects computer built to replace the laptop on stage — four rebuilds since 2023
parent: emgor.shipped
links:
  - { label: "Full lineage: all four phases", url: "#/neptr" }
  - { label: "Play the web version", url: "NEPTR.html" }
tags: [instrument, guitar, raspberry-pi, csound, live-performance]
updated: 2026-08-12
draft: false
---

# NEPTR

Laptops make bad stage gear: they're slow to boot, they update themselves at the worst
moment, and a crash mid-song is a real risk. **NEPTR** is the alternative — a small, dedicated
computer that sits between a guitar and an amplifier, running effects and processing
in real time, controlled entirely with feet and hands mid-song, with none of a laptop's
fragility.

## What it solves

Guitar effects processing has two options: physical pedals, which are excellent but
one-trick and expensive to stack; or a laptop running a DAW, which is powerful but
built for a studio, not a stage. NEPTR is neither — a purpose-built instrument that keeps a
laptop's flexibility (dozens of effects, deep signal processing) while behaving like a pedal:
instant-on, foot-operable, and built to survive a power cut mid-set.

## What's actually built

Four full rebuilds since 2023, each one performed with, not just prototyped:

- **Phase 1** (2023–2025) — Max/RNBO patches on a Raspberry Pi with a touchscreen and custom
  button PCB. Performed at ICMC.
- **Phase 2** (2025) — rewritten from scratch in Csound, running natively on the Pi.
- **Phase 3** (2025–2026) — rewritten again in ChucK, with a new Teensy-based control
  board and 3D-printed body.
- **Phase 4 (2026 — current)** — the version on stage now: Csound running on a Raspberry Pi 5
  at roughly 7 milliseconds round-trip latency, 19 effects across circuit-modeled and spectral
  processing, the neural amp modeling engine from [NAM × Csound](#/shipped/nam-csound) at the
  end of the chain, a 7-encoder hardware controller, a 5-channel live looper, and a touchscreen
  face.

## Where to read more

The full four-phase build history, with source and hardware detail for each generation, is on
[NEPTR](#/neptr). A version exported for the browser is playable directly at
[NEPTR.html](NEPTR.html).
