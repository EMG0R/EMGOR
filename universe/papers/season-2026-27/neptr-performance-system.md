---
id: emgor.papers.season-2026-27.neptr-performance-system
title: NEPTR Performance System
blurb: The instrument as it exists now — Csound on DEMIURGE, a Python interface, a Rust supervisor, no laptop onstage.
parent: emgor.papers.season-2026-27
source: ______2026NEW/DEMIURGE_OS/neptr
links:
  - { label: "Read as paper", url: "papers/nime/neptr-performance-system.html" }
  - { label: "NEPTR on the site", url: "NEPTR.html" }
tags: [performance, csound, raspberry-pi, hyper-instruments, diy, icmc]
updated: 2026-09-21
draft: false
---

# Performance System of a Modern Digital Luthier

**Status: running.** The instrument is on stage now. Paper in progress — targets ICMC.

## Abstract

NEPTR is the brain at the center of the performance system, linking audio interfaces, DIY controllers, and hyper-instruments with internal DSP and a graphical interface. It replaces the laptop, the synthesizer, and the pedalboard onstage. This paper documents the system as a whole rather than the box alone: the operating layer beneath it, the effect architecture inside it, and the family of DIY satellites around it.

## The system, in three layers

**Underneath — a custom Rust supervisor.** The instrument boots from [DEMIURGE](/papers/season-2026-27/demiurgeos), a flashable audio-first OS whose launcher is a zero-dependency Rust patch-graph engine. It parses one config file into a directed graph of live programs, links audio and MIDI between them, and hot-reloads on save: untouched stages keep their PIDs, dead ones are routed around. The instrument survives power pulls, hot-plugs, and field gigs because the supervisor treats failure as a routing problem rather than a crash.

**The engine — Csound.** One `.csd` entry point includes the globals, the signal chain, and a MIDI handler, then pulls in the effect library. **63 effect modules** are implemented as individual Csound files, spanning circuit-modeled drive and preamp stages, granular and spectral processors, pitch-tracked synthesis, convolution reverb, delays, a multi-channel looper, and a Neural Amp Modeler stage built on the [NAMProcess opcode](/papers/season-2026-27/nam-csound). Bypassed effects are turned off and cost no CPU.

**On top — a Python interface.** A touchscreen UI written in Python, mirrored over OSC, presents 20 menus of toggleable effect slots. Encoder rotation drives the parameters of whichever effect was last toggled, and the expression pedal follows the same last-toggled-wins rule. A consistency test suite fails the build if the UI tables, the MIDI handler, and the reference documentation disagree with each other.

There is deliberate crossover between the layers — the OS work and the instrument work are the same work, done from different ends.

## Latency

**6 ms** round trip, guitar to amplifier: instrument → interface → Csound → interface → amp, at 48 kHz with a 128-sample period.

## The performance system

Around the brain sit the DIY satellites, each documented as a hardware planet on this site: the hyper-trumpet and hyper-guitar, the Quadro Punch Packer hemispherical speakers, the We-Remote BLE controller, the 4-i-Gor DIY audio interface, and the oFOOTS foot controller. Nothing onstage is bought that could be built.

## A note on NEPTR's two lives

The [Luthier paper](/papers/season-2026-27/digital-luthier) documents an earlier NEPTR — ChucK for synthesis, a Python GUI over OSC, a Teensy 4.0 on audio I/O and a Teensy 4.1 on controls. That case study captures the prototype at the moment it demonstrated the workflow. This paper documents what it became.

Both are true of their own moment, and the distance between them is itself evidence for the workflow argument: the instrument was rebuilt underneath without being replaced.

## People

Emory Smith.
