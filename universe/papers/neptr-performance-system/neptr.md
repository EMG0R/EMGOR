---
id: emgor.papers.neptr-performance-system.neptr
title: NEPTR
blurb: The instrument — Csound on a Pi 5 (DEMIURGE), between a guitar and a real amplifier
parent: emgor.papers.neptr-performance-system
source: ______2026NEW/NEPTR phase4/
downloads:
  - files/neptrPhase4.csd
  - files/live.conf
links:
  - { label: "Download current (DEMIURGE)", url: "papers/zips/neptr-current.zip" }
  - { label: "Download legacy (phases 1–3)", url: "papers/zips/neptr-legacy-phases-1-3.zip" }
  - { label: "NEPTR web (RNBO export)", url: "NEPTR.html" }
tags: [instrument, guitar, raspberry-pi-5, csound, nam, teensy, looper, live-performance]
updated: 2026-10-02
draft: false
---

# NEPTR

NEPTR is the instrument: a guitar effects computer that sits between a guitar and a real amplifier and is playable with your feet and two hands mid-song. It has been built, gigged, torn down, and rebuilt four times since 2023 (Max/RNBO on a Pi, then native Csound, then ChucK, then now). The older phases are in the legacy download.

It currently runs as Csound on a Pi 5 called DEMIURGE, talking raw ALSA at 48 kHz / period 128 for roughly 7 ms round trip. Around 70 toggleable hand-written Csound effects across 19 menus, Neural Amp Modeler at the end of the chain, a 5-channel looper, a 7-encoder Teensy controller with expression pedal, and a pygame touchscreen. Everything is voiced as pedal or preamp, never cab sim, because a real amp does that job.

The `.csd` entry point (`neptrPhase4.csd`, just the spine) and the `live.conf` the systemd service boots from are included below. DEMIURGE itself, the OS around it, is covered in its own paper.
