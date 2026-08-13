---
id: emgor.web-synth.ciesen
launch: ciesen.html
title: CIESEN
blurb: Generative ambient ChucK synth, live in-browser — works on iPhone
parent: emgor.web-synth
source: ____EMGOR_ONLINE/EMGOR/_CIESEN_audio.ck
downloads:
  - files/_CIESEN.ck
links:
  - { label: "Play CIESEN", url: "ciesen.html" }
  - { label: "Full chuGL version (desktop, WebGPU)", url: "ciesen-gl.html" }
tags: [chuck, webchuck, chugl, webgpu, generative, ambient]
updated: 2026-08-12
draft: false
---

# CIESEN

**Language: ChucK.** A generative ambient piece written in ChucK and run *in
the browser* via WebChucK — the same audio engine that would run on the
desktop VM, compiled to the web. The default build here is the
maximally-compatible one: real ChucK audio (`webchuck`, AudioWorklet-based,
no SharedArrayBuffer) with a lightweight Canvas2D UI that mimics the look of
the original WebGPU visuals — flat emissive shapes, one glow pass, no lights
or 3D. It works on iPhone, where the ChuGL/WebGPU build cannot (WebGPU is
still behind a Safari feature flag on iOS). The full chuGL version — same
audio, real WebGPU visuals via `webchugl` — is one click away for
desktop/Chrome.

The ensemble lives on a C-major pitch table spanning six octaves. Every voice
is a hand-built instrument routed through its own gain bus into a stereo
master:

- **Sine pads** — each note gets its own `Pan2`, its own place in the stereo field
- **Birds** — per-voice panned chirps
- **Rain** and **plucks** — textural layers on their own panned buses (the pluck
  arp is wired up but intentionally muted)
- **Kick** — a low anchor under the ambience

Nothing repeats; note choice, timing, and panning are probabilistic. Six
draggable orbs (PITCH, KICK, WAVES, SIN, STORM, BIRD) control the mix live —
audio-only ChucK globals the JS visuals read and write every frame.

Of the three web-synth languages, this is the *strongly-timed* one: ChucK's
sample-accurate `=>` time model, running in a tab. Play it at the link above;
the complete ChucK source (both the audio-only and full ChuGL versions share
the same instruments) is the download.
