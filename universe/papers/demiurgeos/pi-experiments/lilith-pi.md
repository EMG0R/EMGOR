---
id: emgor.papers.demiurgeos.pi-experiments.lilith-pi
title: LILITH_PI
blurb: Pi drum machine in one Max patch
parent: emgor.papers.demiurgeos.pi-experiments
source: LILITH_PI/lilithPI.maxpat
downloads:
  - files/lilithPI.maxpat
tags: [max, drum-machine, sampler, raspberry-pi]
updated: 2026-07-28
draft: false
---

# LILITH_PI

A drum machine built as a single Max patch, targeting a Raspberry Pi rig. Five sample voices — kick, snare, two hats, tom, cowbell — sequenced and triggered from one patcher window. The sample set travels with the patch as plain WAVs.

This is the "can a Pi be a drum machine" experiment in DEMIURGE's prehistory. The answer was yes, but the plumbing hurt: getting a patch, its samples, and an audio interface to come up reliably on a small Linux box is exactly the problem DEMIURGE's one-file config and hot-plug-proof virtual sink were later built to erase.

The patch is in `files/`. Samples stay in the source folder (`LILITH_PI/` in the working archive) — drop the patch next to any kick/snare/hat/tom/cowbell WAVs and rewire.
