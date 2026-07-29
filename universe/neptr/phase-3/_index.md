---
id: emgor.neptr.phase-3
title: Phase 3 — ChucK
blurb: Strongly-timed rewrite — ChuckTer, neptrSYNTH1, Teensy I/O, a new body
parent: emgor.neptr
source: ______PHASE3/
downloads:
  - files/neptrSYNTH1.ck
  - files/ChuckTer_v1.2_noGL.ck
links:
  - { label: "cheese — the ChucK FX library born here", url: "#/code/cheese" }
tags: [chuck, teensy, 3d-print, synthesis, live-coding]
updated: 2026-07-28
draft: false
---

# Phase 3 — ChucK (2025–2026)

Phase 2's weak spot was timing. ChucK's whole premise is timing. Phase 3 rebuilt NEPTR as a strongly-timed ChucK system running at 320 BPM with probability-driven sequencing.

## The instruments

- **`neptrSYNTH1.ck`** — the synth voice: three engines (WAVETABLE / CLASSIC / PHYSMOD), classic shapes with PWM, full note + pitch envelopes (the signature move: a −12 semitone pitch envelope on every hit), filter section, all parameters live-steerable.
- **`ChuckTer_v1.2_noGL.ck`** — the terminal instrument, 90 KB of ChucK: synth-mode switching across all three engines, the sequencer, MIDI in (`testMIDIin.ck` was the probe), and an earlier chuGL visual variant (`sad/chuGL1.ck` — the `noGL` suffix tells you which one survived on the Pi).
- **Hardware.** `TEENSYY.ino` — the first Teensy firmware in the lineage (encoders + buttons over USB-MIDI, carried forward into Phase 4) — and `AUDIO_IO.ino`. New 3D-printed shell: `NEPTR_p3` BODY v5 + HEAD v3.
- **`PHASE3_UI.py`** — the touchscreen UI, evolved from Phase 2's.

## cheese

The **cheese** ChucK FX library grew directly out of this era's effect experiments and became its own project — it has its own planet. Follow the link above rather than expecting its docs here.

## Why it ended

ChucK was a joy to write and expensive to run: the Pi budget got eaten by the VM before the effect count got near what the instrument needed. Phase 4 went back to Csound, kept the Teensy, and kept ChucK around as an optional sequencer voice.
