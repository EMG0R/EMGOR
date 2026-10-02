---
id: emgor.papers.nam-csound
title: 6. NAM × Csound (2027+)
order: 6
blurb: NAMProcess — real-time Neural Amp Modeler inference as a Csound opcode, with live volume compensation. Complete.
parent: emgor.papers
updated: 2026-09-21
read: papers/edit.html?id=nam-csound
zip: papers/zips/nam-csound.zip
draft: false
---

Neural Amp Modeler (NAM) is the most widely used open-source system for black-box modeling of nonlinear audio hardware using neural networks. A community library of several hundred thousand pre-trained captures exists — over 350,000 on TONE3000 alone — but there has been no way to use them inside Csound. This paper presents **NAMProcess**, a Csound opcode that performs real-time NAM inference. It loads standard `.nam` files, switches between models without interrupting audio, adds no algorithmic latency, and runs comfortably in real time on systems like the Raspberry Pi. A live dry-referenced volume compensation system built into the opcode holds every capture in a heterogeneous 25-model test library within ±2 dB of the player's volume.
