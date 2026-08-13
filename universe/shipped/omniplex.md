---
id: emgor.shipped.omniplex
title: OMNIPLEX
blurb: One cable carrying audio, video, MIDI, and lighting at once, instead of four separate boxes
parent: emgor.shipped
links:
  - { label: "Full proposal and architecture", url: "#/papers/unified-streaming" }
tags: [rust, protocol, usb, streaming, systems]
updated: 2026-08-12
draft: false
---

# OMNIPLEX

Anyone who has run a live show or a studio session knows the tangle: one box and protocol to
send audio over a network (Dante), a different one for video (NDI), another for MIDI
(rtpMIDI), another for stage lighting (Art-Net) — each with its own driver, its own network
settings, and its own way of failing. **OMNIPLEX** is a system that carries all four kinds of
signal down one cable at once, auto-detecting what's plugged in and setting itself up with
no configuration.

## What it solves

Every device today already has a USB-C or Thunderbolt port carrying enormous bandwidth, and
yet the audio/video/lighting/control world still routes signal through separate adapter boxes
that each do their own analog-to-digital conversion along the way. OMNIPLEX treats the direct
link between two machines as one shared, prioritized pipe: time-critical signals like MIDI and
lighting can never get stuck behind a slow video stream, because they ride their own
independent lanes rather than sharing one queue — and everything shares a single clock, so
audio, video, and lighting stay in sync with each other automatically.

## What's actually built

**In progress, honestly.** This is the newest project on this planet and it should be read
that way. What exists right now is a real, working Rust codebase — eight separate modules
(wire format, network transport, device discovery, clock sync, channel routing, monitoring,
session handling, and a command-line tool), each one independently tested. What does **not**
yet exist: a finished end-user application, a public release, or the paper that will describe
it — that paper is still a proposal, targeting NIME and related venues in 2026–2027.

## Where to read more

The full design — the protocol layout, the eight-module breakdown, and the research proposal
behind it — is on [papers / Unified Streaming](#/papers/unified-streaming).
