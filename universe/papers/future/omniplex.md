---
id: emgor.papers.future.omniplex
title: OMNIPLEX
blurb: One cable for audio, video, and control — a Rust transport, now folding into DemiurgeOS.
parent: emgor.papers.future
source: ______2026NEW/__PAPERS/2026-2027 PAPERS.md + ______2026NEW/universal_usb_protocol/
links:
  - { label: "Read as paper", url: "papers/nime/omniplex.html" }
tags: [rust, protocol, usb, quic, systems, usb-c, networking, low-latency, streaming]
updated: 2026-09-21
draft: false
---

# OMNIPLEX

**Status: deferred — and folding inward.** The transport exists as a Rust workspace; the standalone paper waits for a season with room in it.

## The idea

Adapters and converters are used everywhere a single USB-C cable would do. Nearly every device on a modern stage or desk carries a USB-C port, and yet signal still routes through audio interfaces and video capture cards — rounds of unnecessary conversion between machines that are each already digital.

OMNIPLEX is the alternative: one cable and one system carrying audio, video, and control data between machines, with the stream arriving on the host as if it were native hardware. Devices negotiate roles on the link rather than having them fixed by whichever box owns the capture hardware, and the UI makes the bandwidth budget of a cable a first-class, inspectable object.

## What exists

A Rust workspace, not a proposal: `omniplex-wire`, `-transport`, `-mux`, `-session`, `-discovery`, `-clock`, `-metrics`, and `-cli`, alongside a driver layer and packaging.

## Why it's deferred

The transport is being absorbed into [DemiurgeOS](/papers/season-2026-27/demiurgeos) as a feature rather than shipped as its own system. Inside the OS it is justified by what the instrument needs and evaluated by whether the instrument works — a far better position than competing with industrial audio-over-IP standards on a spec sheet.

The standalone paper stays queued. When it goes out, it goes to a systems venue where transport is the subject rather than the plumbing.

## People

Emory Smith.
