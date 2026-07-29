---
id: emgor.code.omniplex
title: OMNIPLEX
blurb: One cable, every art signal — Rust multiplexed real-time transport
parent: emgor.code
source: ______2026NEW/universal_usb_protocol/
tags: [rust, protocol, usb, quic, systems]
updated: 2026-07-28
draft: false
---

# OMNIPLEX

The systems project. Audio, video, MIDI, OSC, DMX, timecode, and serial between two machines today means Dante *and* NDI *and* rtpMIDI *and* Art-Net — each with its own driver, network, or adapter box. Omniplex treats the fastest link two machines actually share (Thunderbolt/USB4 peer networking in v1) as one adaptive bandwidth pool and multiplexes **all of those channel types simultaneously** over it. Plug in, auto-detect, auto-discover, stream. Zero infrastructure, zero configuration.

Design bar, in priority order: lowest achievable latency, glitch-free under sustained load, zero config. Structural guarantees, not vibes — latency-critical traffic (MIDI/OSC/DMX) can never be queued behind bulk video, because it rides independent QUIC streams and datagrams rather than sharing a pipe; all media shares one session clock so audio, video, and lighting stay phase-aligned across the link.

## Implementation

A Rust workspace, eight crates, layered so each is independently testable:

| crate | role |
|---|---|
| `omniplex-wire` | framing / wire format (postcard + serde) |
| `omniplex-transport` | QUIC (quinn/rustls) over the TB/USB4 virtual interface |
| `omniplex-discovery` | link detection + peer discovery (mDNS) |
| `omniplex-clock` | session clock sync |
| `omniplex-mux` | channel model, priority classes, bandwidth adaptation |
| `omniplex-metrics` | built-in latency/jitter/loss instrumentation |
| `omniplex-session` | negotiation, roles, lifecycle |
| `omniplex-cli` | reference CLI |

Two deliverables by design: an open, versioned **protocol spec + SDK** meant to be neutral enough to adopt (the MIDI move), and a flagship companion app that terminates channels as native virtual devices (audio device, MIDI port, camera, COM port). Full design doc and phase-1 plan live in the repo's `docs/`. Active 2026 work — code stays in its own repo rather than as site downloads.
