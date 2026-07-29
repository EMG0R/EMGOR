---
id: emgor.code.web-synths.ciesen
title: CIESEN
blurb: Generative ambient ChucK synth with WebGPU visuals, live in-browser
parent: emgor.code.web-synths
source: ____EMGOR_ONLINE/EMGOR/_CIESEN.ck
downloads:
  - files/_CIESEN.ck
links:
  - { label: "Play CIESEN", url: "ciesen.html" }
tags: [chuck, webchugl, webgpu, generative, ambient]
updated: 2026-07-28
draft: false
---

# CIESEN

A generative ambient piece written in ChucK and run *in the browser* via WebChuGL — the same `.ck` source that would run on the desktop VM, compiled to the web with interactive WebGPU visuals on a full-page canvas.

The ensemble lives on a C-major pitch table spanning six octaves. Every voice is a hand-built instrument routed through its own gain bus into a stereo master:

- **Sine pads** — each note gets its own `Pan2`, its own place in the stereo field
- **Birds** — per-voice panned chirps
- **Rain** and **plucks** — textural layers on their own panned buses
- **Kick** — a low anchor under the ambience

Nothing repeats; note choice, timing, and panning are probabilistic. Needs WebGPU (Chrome/Edge 113+, or the Safari feature flag on iOS) — the page tells you exactly what to enable if it can't start.

Play it at the link above; the complete ChucK source is the download.
