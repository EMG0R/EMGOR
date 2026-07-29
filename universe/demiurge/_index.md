---
id: emgor.demiurge
title: DEMIURGE_OS
blurb: Flashable audio-first OS for Raspberry Pi 5 — successor to CCRMA Satellite
parent: emgor
source: ______2026NEW/DEMIURGE_OS
links:
  - { label: "Full source: DEMIURGE_OS git repo (local for now)", url: "file:///Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS" }
tags: [raspberry-pi, linux-audio, pipewire, os, live-performance]
updated: 2026-07-28
draft: false
---

# DEMIURGE_OS

A flashable, audio-first operating system for the Raspberry Pi 5 — the modern successor to Stanford CCRMA's Satellite. Every major audio language ships pre-installed and pre-wired: Csound, Pure Data, SuperCollider, ChucK, Faust, Strudel, C++, Python, plus RNBO and NAM profiles as first-class stages. Edit one file, reboot, perform.

## The idea

Programs never see hardware. Everything routes through a unified PipeWire abstraction layer — one virtual sink (`demiurge-sink`), one shared MIDI bus (`Midi Through`), one global clock daemon fanning MIDI realtime `0xF8` at 24 PPQN to every language. Hot-plug is invisible: audio plays into the void when nothing is connected and resumes when something is. Plug in a second USB interface and it aggregates automatically.

The whole system is driven from `~/demiurge/live.conf` — the **only** file you edit. An ordered `chain =` list of source files; language inferred from the extension; `.dsp` and `.cpp` compiled on demand. Save over SSH and the launcher hot-reloads in under a second: untouched stages keep their PIDs, new ones start, dead ones are routed around.

## Why it exists

DEMIURGE is the platform under NEPTR, emgor's live performance instrument — a ChucK saw-voice sequencer feeding a Csound processor with 19 effect modules across 6 menus, driven by a Teensy controller. The OS exists so the instrument can survive power pulls, hot-plugs, and field gigs. It grew into a general system: seven languages verified bidirectionally (MIDI in/out, BPM sync, audio in/out) on the Pi 5, running on an isolated CPU core at a 128-sample quantum without xruns.

## Orbit map

- **os** — the core: PipeWire/WirePlumber layer, virtual I/O, global clock, boot config, M8 performance pack
- **launcher** — the Rust patch-graph engine that supervises everything
- **nam** — Neural Amp Modeler as a chain stage: drop a `.nam` file into the chain
- **web** — browser control surface: telemetry, patch switching, power, transport
- **pi-experiments** — small satellites: earlier Pi music sketches that led here
