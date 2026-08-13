---
id: emgor.shipped.demiurgeos
title: DemiurgeOS
blurb: An operating system for the Raspberry Pi built so audio code never has to fight the hardware
parent: emgor.shipped
links:
  - { label: "Full technical writeup", url: "#/papers/demiurgeos" }
tags: [raspberry-pi, linux-audio, pipewire, os, live-performance]
updated: 2026-08-12
draft: false
---

# DemiurgeOS

A normal Raspberry Pi running audio software is fragile in ways that only show up on
stage: plug in a second interface and the routing breaks, the audio driver drops out on a
reboot, and every programming language — Csound, Pure Data, ChucK, SuperCollider — wants to
own the sound card for itself. **DemiurgeOS** is a version of Raspberry Pi OS rebuilt from the
ground up to fix exactly that, with every major audio language pre-installed and pre-wired to
share the machine cleanly. It's built as a modern successor to Stanford CCRMA's long-running
"Satellite" project, which solved this for an earlier generation of hardware.

## What it solves

Every audio language on a Pi normally wants direct access to the sound card, so running two at
once — or hot-swapping a USB interface without a reboot — breaks things in ways that are hard
to debug live. DemiurgeOS routes every program through one shared virtual audio device and one
shared MIDI clock instead, so languages can be swapped, layered, or hot-reloaded without ever
touching the hardware directly. Editing one config file and reconnecting over SSH updates a
running performance rig in under a second.

## What's actually built

**Running now, not a prototype.** It's the operating system underneath NEPTR (this lab's main
stage instrument) today. Verified on real Raspberry Pi 5 hardware: seven audio languages
tested bidirectionally for MIDI and audio I/O, running on a CPU core reserved just for audio,
with no glitches at a 128-sample buffer — a tight enough margin that most general-purpose Linux
setups can't hold it reliably.

**Not yet done:** a public, ready-to-flash disk image (right now it's a set of scripts and a
git repo, not a download); a single unified control app (there are currently three separate
control surfaces — bash, Python, and web); and a formal, published benchmark of the latency
and reliability numbers above.

## Where to read more

The full architecture — the audio routing layer, the clock design, the one-file
configuration system — is documented on [papers / DemiurgeOS](#/papers/demiurgeos).
