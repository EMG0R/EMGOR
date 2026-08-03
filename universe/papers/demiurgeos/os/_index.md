---
id: emgor.papers.demiurgeos.os
title: OS Core
blurb: PipeWire abstraction layer, global clock, one-file config
parent: emgor.papers.demiurgeos
source: ______2026NEW/DEMIURGE_OS/config
downloads:
  - files/demiurge-virtual.conf
  - files/50-demiurge.conf
  - files/demiurge.service
  - files/demiurge.conf.default
  - files/live.conf
  - files/demiurge-clock.cpp
tags: [pipewire, wireplumber, alsa, clock, systemd, realtime]
updated: 2026-07-28
draft: false
---

# OS Core

Pi OS Lite Bookworm (64-bit) as the base. PipeWire + WirePlumber own audio; PulseAudio and jackd2's runtime are gone. Three layers: hardware at the bottom, the PipeWire abstraction in the middle, DEMIURGE's launcher and config on top.

## The abstraction layer

Two `libpipewire-module-loopback` instances create `demiurge-sink` — an Audio/Sink with `node.linger=true`, so it exists whether or not hardware does. Every program connects here, never to a device. A WirePlumber policy (`50-demiurge.conf`) pins the sink at session priority 0, kills HDMI audio routing, prioritizes USB audio class devices, and ignores Bluetooth. MIDI is the kernel ALSA seq `Midi Through` bus (`snd-seq-dummy`, client 14:0); a udev rule auto-merges physical controllers on hot-plug. One audio hole, one MIDI pool.

## The global clock

`demiurge-clock` is a small C++ ALSA seq daemon — the one service no existing binary provides:

- MIDI realtime `0xF8` at 24 PPQN via `clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME)` — absolute-deadline timing, no drift
- BPM broadcast once per beat on `ch16 CC119` (0..127 → 40..240 BPM); retune via `CC118`, transport via `CC117`
- Optional Ableton Link: flip `link = on` and the Pi shares tempo and transport with Live, Max/MSP, and any Link peer on the network

Every audio language already reads `0xF8` and CC — piggybacking on existing wiring means zero new reader code per language. The read/write split (CC119 out, CC118 in) means the daemon never filters its own echo.

## One config surface

`/boot/firmware/demiurge.conf` holds one `launch =` line pointing at `~/demiurge/live.conf` — the file you actually edit. Globals (`bpm`, `link`, `quantum`, `sync_layer`), one or more ordered `chain =` blocks, an optional `midi =` sidecar block. Parallel chains sum at the sink for free. A `sync_layer = off` bypass mode hands the interface directly to one program over raw ALSA for absolute-minimum latency.

## The M8 performance pack

What turns the Pi into an instrument: RT limits for `@audio`, quantum 128 @ 48 kHz, `performance` governor pinned at boot, a safe Pi 5 overclock (2800 MHz, +50 mV), `isolcpus=3` paired with `CPUAffinity=3` so the launcher and every audio child run exclusively on the isolated core, `threadirqs`, and a thermal watchdog on a 10-second timer. All reversible. An idempotent installer (`setup-demiurge.sh`, ten phases) reproduces the whole system on a fresh flash.
