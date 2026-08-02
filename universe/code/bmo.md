---
id: emgor.code.bmo
title: BMO
blurb: Pure-WebGL shader scene — lava noise lensed around a black hole
parent: emgor.code
source: ____EMGOR_ONLINE/EMGOR/BMO.html
links:
  - { label: "Open BMO", url: "BMO.html" }
tags: [webgl, glsl, shader, visual]
updated: 2026-08-02
draft: false
---

# BMO

The visual one — no audio. A full-screen WebGL fragment shader — hand-rolled GLSL, no libraries — renders drifting value-noise "lava" over a deep violet field, then bends it: an inverse-square **gravitational lensing** term warps the texture coordinates around a central singularity, so the noise field streams and smears around a black hole in real time.

The whole thing is ~170 lines of JavaScript plus two shaders: hash-based `random`, smoothstep-interpolated 2D noise, time-driven domain warping, and the lensing distortion. BMO is where the galaxy's visual language (the black hole at the center of everything) gets prototyped in raw GLSL.

Open the link above; it runs anywhere WebGL 1 does, which is everywhere. For the instruments that make sound in the browser, see the [WEB SYNTH](#/web-synth) planet.
