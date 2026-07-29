---
id: emgor.hardware
title: HARDWARE
blurb: Physical instruments, wearables, controllers — built by hand
parent: emgor
source: _EMGOR_SYNTH/______2026NEW, ______LITE, ____oFOOTS
links:
  - { label: "Infinity Gauntlet (M4L controllers)", url: "#/m4l/controllers" }
tags: [hardware, instruments, embedded, diy]
updated: 2026-07-28
draft: false
---

# HARDWARE

The physical half of the lab. Instruments, wearables, controllers, speakers — designed,
documented, and soldered in-house. No fab-house mystery boxes: every project here carries its
own build doc, bill of materials, wiring map, and an honest account of where it actually is.

## What lives here

| Project | What it is | State |
|---|---|---|
| [THE WATCH](#/hardware/the-watch) | Forearm AMOLED smartwatch + RF multitool, dual-ESP32 | Design locked, pre-build |
| [Pocket OpGorator](#/hardware/pocket-opgorator) | Pocket-Operator-format Daisy sampler/synth | PCB + housing designed, assembly pending |
| [neuralGrid](#/hardware/neuralgrid) | Monome-style grid that rewrites its own UI on the beat | Phases 1–4 running on hardware |
| [4-i-Gor](#/hardware/4-i-gor) | Teensy 4.0 quad-I/O USB audio interface | Firmware written, PCB spec'd |
| [We-Remote](#/hardware/we-remote) | Open-source Wii remote, ESP32-S3 BLE | Firmware complete, shell printed |
| [Quadro Punch Packer](#/hardware/quadro-punch-packer) | 10" coaxial hemisphere speaker, 360° | Physically built, in debug |
| [hyperGuitar](#/hardware/hyperguitar) | Bela-embedded augmented headless 6-string | Design locked, body on order |
| [hyperTrumpet](#/hardware/hypertrumpet) | Augmented trumpet, Teensy 4.1 + Max | Built and performed |
| [LITE](#/hardware/lite) | Teensy 4.1 + NeoPixel light instrument (BIGNS) | Working instrument |
| [oFOOTS](#/hardware/ofoots) | 10-encoder foot controller, TFT paging | Working, display upgrade in progress |

The **Infinity Gauntlet** wearable controller is documented on the M4L planet with its
Max for Live control surface — see [m4l/controllers](#/m4l/controllers).

## House rules

- **Breakout boards wherever possible.** Raw QFN/LGA silicon is near-unsolderable by hand;
  breakouts bundle the decoupling, pull-ups, and regulators and survive flex.
- **Connectors at every subsystem boundary.** JST-GH or 0.1" headers. Rework by unplugging,
  not by cutting a soldered joint.
- **Star ground, always.** Analog/RF returns separated from digital and switching returns,
  joined at one point at the battery negative.
- **Decouple everything.** 0.1µF at every IC power pin, bulk at every domain entry, local bulk
  at every spiky load.
- **PETG or ASA, not PLA.** PLA softens around 55–60°C — body heat, sun, and a hot car all
  deform it. TPU for anything that has to flex.

## 3D printing

Every enclosure here is printed in-house on a Bambu X1C. Shells live beside their projects
(`Pocket-OpGorator/gorator.blend`, `We-Remote/wii_shell_*.stl`, `Quadro-Punch-Packer/shell/`,
`______LITE/LITE_BOWL1.2.3mf`) plus a shared `3dPRINT/` library of general parts. Solid models
are built in Blender from parametric cross-sections and boolean-cut with separate cutter
objects, then split for the build volume. Mesh files are not mirrored into this repo — they are
large and iterate fast; the geometry decisions that matter are written into each page.
