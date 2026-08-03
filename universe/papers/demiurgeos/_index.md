---
id: emgor.papers.demiurgeos
title: DemiurgeOS
blurb: Flashable audio-first OS for Raspberry Pi 5 — successor to CCRMA Satellite, and the NIME paper about it
parent: emgor.papers
source: ______2026NEW/DEMIURGE_OS ; ______2026NEW/__PAPERS/2026-2027 PAPERS.md
links:
  - { label: "Read as NIME draft", url: "papers/nime/demiurgeos.html" }
  - { label: "Full source: DEMIURGE_OS git repo (local for now)", url: "file:///Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS" }
tags: [raspberry-pi, linux-audio, pipewire, os, live-performance, dsp, tooling]
updated: 2026-07-28
draft: false
---

# DemiurgeOS

A flashable, audio-first operating system for the Raspberry Pi 5 — the modern successor to Stanford CCRMA's Satellite. Every major audio language ships pre-installed and pre-wired: Csound, Pure Data, SuperCollider, ChucK, Faust, Strudel, C++, Python, plus RNBO and NAM profiles as first-class stages. Edit one file, reboot, perform.

**Status: running.** It's the platform under NEPTR right now (Phase 4), and a paper describing it is in progress — target venue: Linux Audio Developer Conference.

## The idea

Programs never see hardware. Everything routes through a unified PipeWire abstraction layer — one virtual sink (`demiurge-sink`), one shared MIDI bus (`Midi Through`), one global clock daemon fanning MIDI realtime `0xF8` at 24 PPQN to every language. Hot-plug is invisible: audio plays into the void when nothing is connected and resumes when something is. Plug in a second USB interface and it aggregates automatically.

The whole system is driven from `~/demiurge/live.conf` — the **only** file you edit. An ordered `chain =` list of source files; language inferred from the extension; `.dsp` and `.cpp` compiled on demand. Save over SSH and the launcher hot-reloads in under a second: untouched stages keep their PIDs, new ones start, dead ones are routed around.

## Why it exists

DEMIURGE is the platform under NEPTR, emgor's live performance instrument — a ChucK saw-voice sequencer feeding a Csound processor with 19 effect modules across 6 menus, driven by a Teensy controller. The OS exists so the instrument can survive power pulls, hot-plugs, and field gigs. It grew into a general system: seven languages verified bidirectionally (MIDI in/out, BPM sync, audio in/out) on the Pi 5, running on an isolated CPU core at a 128-sample quantum without xruns.

Born from watching a class full of people fight their Pis — an integrated solution to the common DSP-on-Pi situations that would have at least doubled that class's productivity. The core pieces:

- **The sync layer** — modular patching between audio interfaces and audio code
- **The MIDI pool** — managing MIDI I/O, OSC I/O, and clock/CV signals between programs and physical interfaces
- **System behavior** — intentional power throttling, presets, auto-run programs on boot

All of it controlled through a paired bash GUI, Python GUI, or local web GUI. Inspired by Satellite CCRMA and RNBO. Ships preinstalled with every major audio language.

## Orbit map

- **os** — the core: PipeWire/WirePlumber layer, virtual I/O, global clock, boot config, M8 performance pack
- **launcher** — the Rust patch-graph engine that supervises everything
- **nam** — Neural Amp Modeler as a chain stage: drop a `.nam` file into the chain
- **web** — browser control surface: telemetry, patch switching, power, transport
- **pi-experiments** — small satellites: earlier Pi music sketches that led here

## People

Emory Smith (primary), with Primrose (Stanford); Ajay and Ge possible.

## Future work

Packaging as a public flashable image; the paired bash/Python/web GUIs unified into one release; formal latency and reliability benchmarks across languages; classroom deployment to test the productivity claim that motivated it.
