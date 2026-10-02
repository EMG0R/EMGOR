---
id: emgor.papers.neuralgrid
title: 3. NeuralGrid
order: 3
blurb: A grid, ribbon, and lidar controller that speaks a symbolic vocabulary before it speaks sound.
parent: emgor.papers
updated: 2026-09-21
read: papers/edit.html?id=neuralgrid
draft: false
---

A sensor-dense physical controller — a grid of pads, ribbons along the edges, lidar at the corners, contact mics inside — running on a Raspberry Pi. Sensor input is smoothed and interpolated into a coherent output format defined by the dimensional space those sensors actually represent, rather than by what each sensor happens to emit.

Its original context is the Machine Lab: a universal controller pre-mapped to the OSC addresses around the room, able to launch and manage the server itself, so anyone new can walk in and play every instrument in the room. The room as an instrument, not a composition platform.
