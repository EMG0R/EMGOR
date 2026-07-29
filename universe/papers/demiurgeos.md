---
id: emgor.papers.demiurgeos
title: DemiurgeOS
blurb: A thoughtfully designed operating layer for DSP on the Raspberry Pi. Planned.
parent: emgor.papers
source: ______2026NEW/__PAPERS/2026-2027 PAPERS.md
tags: [planned, raspberry-pi, linux-audio, dsp, tooling]
updated: 2026-07-28
draft: false
---

# DemiurgeOS: A Thoughtfully Designed System for DSP on the Raspberry Pi

**Status: PLANNED** — target venue: Linux Audio Developer Conference (definitely).

## Abstract

*Formal abstract TBD.* DemiurgeOS provides a unified interface for managing a Raspberry Pi as a DSP machine: a terminal UI, a local web GUI, and a local Python UI for high-level control of the system.

## The shape of it

Born from watching a class full of people fight their Pis — an integrated solution to the common DSP-on-Pi situations that would have at least doubled that class's productivity. The core pieces:

- **The sync layer** — modular patching between audio interfaces and audio code
- **The MIDI pool** — managing MIDI I/O, OSC I/O, and clock/CV signals between programs and physical interfaces
- **System behavior** — intentional power throttling, presets, auto-run programs on boot

All of it controlled through a paired bash GUI, Python GUI, or local web GUI. Inspired by Satellite CCRMA and RNBO. Ships preinstalled with every major audio language.

## People

Emory Smith (primary), with Primrose (Stanford); Ajay and Ge possible.
