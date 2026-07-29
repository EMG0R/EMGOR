---
id: emgor.neptr.phase-2
title: Phase 2 — Csound on the Pi
blurb: The engine rewritten by hand in Csound, with robot faces and a Bela detour
parent: emgor.neptr
source: ______PHASE2/ + belaSAD/
downloads:
  - files/NEPTR_PHASE2_PIinto.csd
  - files/PHASE2_UI.py
  - files/bela1-neptr-phase2.csd
  - files/FACE1.png
  - files/FACE4.png
  - files/FACE7.png
tags: [csound, raspberry-pi, kivy, osc, bela, protofaces]
updated: 2026-07-28
draft: false
---

# Phase 2 — Csound on the Pi (2025)

Phase 2 is the hand-written rebuild: everything RNBO used to compile, rewritten as one Csound orchestra running natively on the Pi. This is where the engine's DNA that survives into Phase 4 first appears — `sr 48000 / ksmps 128 / 0dbfs 1`, ALSA MIDI in (`-Ma -+rtmidi=alsa`), giant ftable buffers for granular and pitch-shift work, and a global-control-signal architecture (`gk` everything).

## The pieces

- **`NEPTR_PHASE2_PIinto.csd`** — the whole instrument in one file: window tables, dual granular buffer pairs (GPS), MIDI dispatch, the effect chain. Runs headless from boot (`--daemon`, `dac:hw:2,0`).
- **`PHASE2_UI.py`** — Kivy touchscreen UI on a 720×1900 portrait display. Listens on OSC (python-osc `ThreadingOSCUDPServer`), draws animated particle circles, and hosts the **protoFACES**: seven hand-drawn robot face states (three included below) that gave NEPTR its expressions before it could speak.
- **`bela1-neptr-phase2.csd`** — the Bela port from `belaSAD/`. The engine was squeezed onto Bela's ultra-low-latency stack; the folder name records how that went. Instructive failure, kept.

## What it proved

That the instrument didn't need Max at all — a text file and a Pi were enough. What it lacked was strong timing for sequencing, which is exactly where Phase 3 went.
