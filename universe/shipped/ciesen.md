---
id: emgor.shipped.ciesen
title: CIESEN
blurb: A generative ambient piece you can hear right now, in a browser tab, with no install
parent: emgor.shipped
links:
  - { label: "Play CIESEN now", url: "ciesen.html" }
tags: [chuck, webchuck, generative, ambient, browser]
updated: 2026-08-12
draft: false
---

# CIESEN

**CIESEN** is a generative ambient piece — sine pads, panned birds, rain, low plucks — that
never repeats the same way twice, and it runs entirely inside a browser tab. No download, no
plugin, no DAW. Open the link and it's already playing.

## What it solves

Most of the work on this site needs software installed to experience — a DAW, Ableton, a
Raspberry Pi. CIESEN is the opposite: proof that a real, hand-written instrument (written in
**ChucK**, a programming language built for precise musical timing) can be compiled straight
to the web and run with sample-accurate timing in a browser, complete with real-time WebGPU
visuals, at zero cost to the listener.

## What's actually built

**Finished and live.** Every voice — the sine pad ensemble, the bird chirps, the rain and
pluck textures, the low kick — is a hand-built instrument with its own stereo placement, and
every note choice, its timing, and its position in the stereo field is chosen probabilistically
as it plays, so no two listens sound the same. It needs a browser with WebGPU support (recent
Chrome or Edge, or the Safari feature flag on iOS); the page explains what to turn on if it
can't start.

## Where to hear it

Play it directly: [ciesen.html](ciesen.html). The complete ChucK source is downloadable from
its home page at [web-synth / CIESEN](#/web-synth/ciesen), alongside the site's other two
in-browser instruments (a Max/RNBO synth and a live-coding pattern language).
