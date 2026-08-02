---
id: emgor.web-synth
title: WEB SYNTH
blurb: Instruments that run entirely in the browser — three languages, zero installs
parent: emgor
source: ____EMGOR_ONLINE/EMGOR (livecode.html, NEPTR.html, ciesen.html)
size: 1.4
tags: [webaudio, browser, strudel, rnbo, chuck]
updated: 2026-08-02
draft: false
---

# WEB SYNTH

Instruments that run entirely in the browser — no DAW, no download, no plugin
scan. Open a tab and you're holding the instrument.

Each one is built in a different language, and that's the point: three routes
from source code to sound-in-a-tab, each with its own compiler story.

## The 3 languages

- **STRUDEL** — TidalCycles patterns in JavaScript; a full live-coding REPL
  scheduled straight against Web Audio
- **RNBO** — a synth patched in Max, exported through RNBO to WebAssembly;
  the actual DSP graph, playable in the page
- **ChucK** — a generative ambient piece compiled to the web via
  WebChucK + ChuGL, with WebGPU visuals

Click into a leaf for the live page, the language story, and the source where
it's downloadable. The live-coding REPL experience has its own planet at
[LIVE CODE](#/live-code).
