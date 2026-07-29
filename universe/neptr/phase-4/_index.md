---
id: emgor.neptr.phase-4
title: Phase 4 — DEMIURGE
blurb: The current machine — Csound on a Pi 5, NAM, 19 menus, ~7 ms round trip
parent: emgor.neptr
source: ______2026NEW/NEPTR phase4/
downloads:
  - files/neptrPhase4.csd
  - files/live.conf
tags: [csound, raspberry-pi-5, nam, teensy, looper, live-performance, current]
updated: 2026-07-28
draft: false
---

# Phase 4 — DEMIURGE (2026, current)

The machine on stage right now. Csound running on a Pi 5 named DEMIURGE, talking **raw ALSA** — no PipeWire, no JACK — at 48 kHz / period 128 for roughly **7 ms round trip**, guitar → Scarlett → Csound → Scarlett → a real amplifier. Everything is voiced as pedal/preamp; never cab sim, because there's a real amp doing that job.

## Architecture

One `.csd` entry point (`neptrPhase4.csd`, included below — it's just the spine) that `#include`s ~36 effect files, a globals file, the signal chain (`instr 1`) and a MIDI handler (`instr 99`). Bypassed effects are `turnoff2`'d — they cost zero CPU. A systemd service boots it from `live.conf` (also below); `bypass = on` is the production default.

## Control surface

- **Teensy, 7 encoders + expression pedal.** Physical CCs are remapped to logical roles in the handler: two encoders page through **19 menus** (PRESETS, VOLUME, SPECTRAL, PITCH, FILTERING, MOD, DRIVE, PREAMP, GRANULAR, DIGIVERB, DELAY, BASS, SYNTH, AIR, LOOPER, CHASE BLISS, GLITCH, NAM, IR VERB), five toggle effect slots per menu, and every rotation drives the last-toggled effect's parameters. The expression pedal is sticky last-toggled-wins.
- **pygame touchscreen UI** (`neptrPhase4_UI.py`), mirrored over OSC, with a consistency test suite that fails the build if the UI tables, the handler and the reference doc ever disagree.

## The effect roster (highlights)

- **Five circuit-modeled drive pedals** — Klon/TS morph, Marshall preamp, SLO⊕5150 dual high-gain, RAT×Metal Zone, Big Muff — all through shared 4× oversampled clippers.
- **Chase Bliss ports** — HABIT (175 s echo collector), BLOOPER (rebuilt as a granular modifier of the looper's own loops, scatter-0 is a measured exact null), MOOD II.
- **NAM** — Neural Amp Modeler at end of chain, 5 rows × variants of amp profiles.
- **5-channel looper** with a drum-sample master-grid mode; the clocked GLITCH menu (BBCUT, REPEAT, CHOP, RVRS) sits post-looper so it mangles loops too.
- Spectral processors (bin crushers, spectral delay, LOSSY codec degradation), pitch-tracked voices (TRACK SYNTH, PLL FUZZ, FEEDBACKER), convolution IR reverb, a Hudson Broadcast-style germanium front end, and CHAIN FB — an effect that feeds the whole chain back into itself, bounded by construction.

Around 70 toggleable effects total, every one hand-written Csound. The authoritative reference lives in the repo (`docs/neptr_full_reference.md`) and is kept current as the system changes.
