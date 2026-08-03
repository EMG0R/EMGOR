---
id: emgor.papers.unified-streaming
title: Unified Streaming
blurb: OMNIPLEX — one cable, every art signal. Rust multiplexed real-time transport.
parent: emgor.papers
source: ______2026NEW/__PAPERS/2026-2027 PAPERS.md + ______2026NEW/universal_usb_protocol/
links:
  - { label: "Read as NIME draft", url: "papers/nime/unified-streaming.html" }
tags: [rust, protocol, usb, quic, systems, usb-c, networking, low-latency, streaming]
updated: 2026-07-28
draft: false
---

# Unified System for Streaming Realtime Data

**The system is OMNIPLEX** — the unified system for data transfers. What began as this paper's proposal is now an active Rust implementation. Target venues: NIME, ACM CHI, ACM Multimedia, IEEE RTSS.

## Abstract

Adapters and converters are everywhere when a single USB-C cable would be more efficient. This paper proposes a unified system for streaming multiple forms of data over common cables — USB-A/B/C and Ethernet — while interfacing with the host machine for maximum accessibility of everything transferred. Through an intuitive UI and backend, multiple devices network to stream high-bandwidth audio, visual, and control data at extremely low latency.

## The itch

We are all carrying devices with USB-C ports, and yet we route signal through audio interfaces and video capture cards, doing rounds of unnecessary ADC and DAC along the way. Go USB-C to USB-C and stop converting.

Audio, video, MIDI, OSC, DMX, timecode, and serial between two machines today means Dante *and* NDI *and* rtpMIDI *and* Art-Net — each with its own driver, network, or adapter box. Omniplex treats the fastest link two machines actually share (Thunderbolt/USB4 peer networking in v1) as one adaptive bandwidth pool and multiplexes **all of those channel types simultaneously** over it. Plug in, auto-detect, auto-discover, stream. Zero infrastructure, zero configuration.

Design bar, in priority order: lowest achievable latency, glitch-free under sustained load, zero config. Structural guarantees, not vibes — latency-critical traffic (MIDI/OSC/DMX) can never be queued behind bulk video, because it rides independent QUIC streams and datagrams rather than sharing a pipe; all media shares one session clock so audio, video, and lighting stay phase-aligned across the link.

The frontend lets devices on the network decide who sends audio or video, while defining virtual audio interfaces and video signals on the host — low-level OS drivers make the stream appear as a native audio/video device with zero extra configuration. The UI graphically visualizes the cable's bandwidth and what data is occupying it. Internet-scale streaming is an implication for later; the priority now is the lowest-latency path over a physical cable.

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

## People

Emory Smith (primary), with Ilai; Jake and Ajay possible.
