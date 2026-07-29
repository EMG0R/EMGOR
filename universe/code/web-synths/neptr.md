---
id: emgor.code.web-synths.neptr
title: NEPTR
blurb: Max/RNBO synth exported to WebAssembly — play it in the page
parent: emgor.code.web-synths
source: ____EMGOR_ONLINE/EMGOR/NEPTR.html
links:
  - { label: "Play NEPTR", url: "NEPTR.html" }
tags: [rnbo, max, webassembly, synth]
updated: 2026-07-28
draft: false
---

# NEPTR (web)

A synth patched in Max, exported through **RNBO** to WebAssembly, and mounted in a plain web page. The exported patcher (`patch.export.json`) is loaded by the RNBO JS runtime and wired to three UI zones:

- a **clickable keyboard** for triggering notes
- **parameter sliders** generated from the patch's exposed parameters
- a **preset selector** for the states saved in Max

Same DSP graph in the browser as in the Max patcher — RNBO compiles the signal chain itself, so this is the actual instrument, not a recreation. Click the link above and play; no install, no audio-worklet fiddling on your end.
