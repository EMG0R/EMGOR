---
id: emgor.hardware.neuralgrid
title: neuralGrid
blurb: A monome-style grid that rewrites its own interface on the downbeat
parent: emgor.hardware
source: ______2026NEW/neuralGrid/
downloads:
  - files/neuralgrid-protocol.md
  - files/neuralgrid-grid_link.ino
links:
  - { label: "Dimensional architecture map", url: "neuralgrid.html" }
tags: [grid, neural, raspberry-pi, esp32, nime, research]
updated: 2026-07-17
draft: false
---

# neuralGrid

A monome-style grid instrument on a Raspberry Pi 5 that **rewrites its own interface on the
beat**. A zero-latency live layer plays now; tiny local neural models watch how the player plays
and author a new interface config; the swap happens quantized to the global clock, so model
latency is never felt.

Target: a working instrument *and* a NIME paper.

**Status: Phases 1–4 done and verified on hardware. Phases 5–6 open.**

## The two-tier idea

The whole design turns on one rule: **the model never sits in the hot path.**

- **Live layer** — real-time, pinned to an isolated CPU core, never blocks on anything. Reads
  pads and sensors, plays notes, drives LEDs, sends MIDI.
- **Model layer** — async, off the isolated core. Watches the performance, writes a
  schema-versioned parameter vector.
- **The boundary** — the live layer picks up the staged config and atomically swaps it in on the
  downbeat. Model takes 200ms? Doesn't matter. The swap is always on time.

This is the same hot-reload pattern as DEMIURGE_OS's `live.conf`, applied to an instrument's
entire interface.

## Platform

Runs on **DEMIURGE_OS** on a stock Pi 5 16GB — no AI HAT. The OS provides the MIDI bus, the
global clock (24 PPQN, ch16 CC119 bpm / CC118 retune / CC117 transport), hot-reloadable config,
PipeWire audio, and core isolation via `isolcpus=3`. We build on it, not from scratch.

The MCU is a **classic ESP32 WROOM** running a deliberately dumb bridge — the Pico was scrubbed
mid-project. All logic is Pi-side. Firmware sends raw key and analog events; the Pi sends LED
frames. COBS-framed binary protocol both directions with a HELLO handshake.

## Hardware

- 4× Adafruit NeoTrellis 4×4 tiled 2×2 = **8×8, 64 RGB pads**, one shared I2C bus at addresses
  0x2E–0x31.
- 4× VL53L1X lidar in the corners on a second I2C bus, XSHUT addressing.
- 4× SoftPot ribbons, 4× FSRs for velocity, piezos — all analog through a 74HC4067 16-channel
  mux into a single ADC pin.

Do not run 64 pixels off a microcontroller's 3V3 pin. External 5V with a level shifter on
SDA/SCL, or external 3.3V and skip the shifter.

## The four-mode stack

**A** instrument · **B** sequencer · **C** MIDI effects — all hand-coded and gig-ready, the
safety floor that works if every model is turned off. **D** is the neural brain, reconfiguring
over A/B/C. Mode D is the paper; A–C are the gig.

## The models

Three tiny local models, all sub-1M parameters, all trained in-house, all running off the
isolated core.

- **Interface-VAE** — continuous, sequence-aware encoder that reconfigures the interface. A
  supportive mirror, not a director. **Shipped: 4,561 parameters, pure numpy.**
- **Melody/Rhythm VQ-VAE** — factorized so the rhythm latent is preserved while the pitch latent
  is modulated, conditioned on incoming harmony. Works in scale-degree/contour space, which
  makes it tuning-agnostic: same behavior under 12-EDO, uneven temperaments, and microtonal
  scales. Phrase source is live capture.
- **Style conditioning** — a conditional VAE, a dial on how the melody modulates. Emergent
  first, explicit second.

Train order: rhythm → harmony → style.

## Where it actually is

- [x] **Phase 1** — COBS binary protocol both ways, HELLO handshake, LED render confirmed on the
      real WROOM.
- [x] **Phase 2** — live layer: engine, instrument mode (press → note + light), sequencer demo
      with clock-locked playhead, softpot menu switching, MIDI out, clock-lock to the DEMIURGE
      clock. *Interactive feel still needs hands on it.*
- [x] **Phase 3** — config boundary: schema v1, 89-element vector, staged→active atomic swap on
      the downbeat.
- [x] **Phase 4** — Interface-VAE trained on synthetic bootstrap, wired as `--brain
      {off,model,manual}` with off as the default safety floor. The adaptive loop closes. This is
      the headline. *Musical and visual behavior of the morphing still needs ears on it.*
- [ ] **Phase 5** — melody/rhythm VQ-VAE with live capture, plus style conditioning.
- [ ] **Phase 6** — session logging, retrain on real data, RAVE integration, scale to the full
      sensor set.

## Integration

Priority output is MIDI/CC on the DEMIURGE bus — it stands alone. The bonus path is a latent-OSC
stream (`/neuralgrid/latent/style|rhythm|interface`) for a collaborator's RAVE-adjacent neural
audio program to consume latent-to-latent. Purely additive: nothing breaks if RAVE is absent.

There is also a [dimensional architecture map](neuralgrid.html) — a diagram of the dimension
spaces the instrument moves through, not of the hardware.
