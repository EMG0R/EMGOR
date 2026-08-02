---
id: emgor.papers.unified-streaming
title: Unified Streaming
blurb: One USB-C cable for audio, video, and control — kill the adapter pile. Planned.
parent: emgor.papers
source: ______2026NEW/__PAPERS/2026-2027 PAPERS.md
links:
  - { label: "Read as NIME draft", url: "papers/nime/unified-streaming.html" }
tags: [planned, usb-c, networking, low-latency, streaming]
updated: 2026-07-28
draft: false
---

# Unified System for Streaming Realtime Data

**Status: PLANNED** — target venues: NIME, ACM CHI, ACM Multimedia, IEEE RTSS.

## Abstract

Adapters and converters are everywhere when a single USB-C cable would be more efficient. This paper proposes a unified system for streaming multiple forms of data over common cables — USB-A/B/C and Ethernet — while interfacing with the host machine for maximum accessibility of everything transferred. Through an intuitive UI and backend, multiple devices network to stream high-bandwidth audio, visual, and control data at extremely low latency.

## The itch

We are all carrying devices with USB-C ports, and yet we route signal through audio interfaces and video capture cards, doing rounds of unnecessary ADC and DAC along the way. Go USB-C to USB-C and stop converting.

The frontend lets devices on the network decide who sends audio or video, while defining virtual audio interfaces and video signals on the host — low-level OS drivers make the stream appear as a native audio/video device with zero extra configuration. The UI graphically visualizes the cable's bandwidth and what data is occupying it. Internet-scale streaming is an implication for later; the priority now is the lowest-latency path over a physical cable.

## People

Emory Smith (primary), with Ilai; Jake and Ajay possible.
