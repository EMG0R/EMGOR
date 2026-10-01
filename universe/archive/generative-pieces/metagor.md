---
id: emgor.code.csound-suite.metagor
title: metaGOR
blurb: Self-playing ambient Csound piece — weighted notes, giant wet reverb
parent: emgor.code.csound-suite
source: csound/metaGOR.csd
downloads:
  - files/metaGOR.csd
tags: [csound, generative, ambient]
updated: 2026-07-28
draft: false
---

# metaGOR

A composition, not a plugin. Run the file and it plays itself: a metronome ticks at a nominal tempo of 50, each tick has a 65% chance of firing a note, and pitches come from a weighted random table (total weight 26, hand-tuned so the tonic and fifth dominate) shifted by a per-note random key offset.

Everything lands in a reverb set to **fully wet** with a room size around 0.98 — an implausibly large space — and both room size and high-frequency damping are themselves randomly modulated, so even the room breathes.

No UI, no MIDI, no input. `-odac` and listen. The whole piece is one short `.csd`, downloadable above.
