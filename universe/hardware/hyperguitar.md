---
id: emgor.hardware.hyperguitar
title: hyperGuitar
blurb: Headless semi-hollow 6-string with a Bela Gem Multi living inside it
parent: emgor.hardware
source: ______2026NEW/hyperGuitar/
downloads:
  - files/hyperguitar-design.md
tags: [augmented-instrument, bela, guitar, dsp, hexaphonic]
updated: 2026-05-22
draft: false
---

# hyperGuitar

A custom semi-hollow **headless 6-string** with a **Bela Gem Multi** DSP core embedded in the
body. Ten passive audio sources — neck humbucker, bridge humbucker, a Submarine Pickups SubSix
hexaphonic pickup in the middle, and two body-mounted contact mics — digitized at 24-bit/96kHz
and processed entirely in software.

The musical core is **digital pickup blending**: every channel has a software gain and a sub-ms
delay line, so modulating them produces continuous phasor and comb-filter sweeps *between*
pickup positions that no physical selector can make. The mixer is the instrument's voice;
effects are downstream.

**Status: design locked, luthier brief out, pre-build.** Bela DSP source is in the repo
(`bela/hyperGuitar6chPan/` — dsp core, effects, pitch tracker, synth, trellis UI, presets).

## Body

Headless 6-string in **B standard** (B E A D F♯ B), 25.5" single scale with parallel frets — the
player wants a loose feel, and 25.5" delivers it at B standard without losing low-end definition
against a .060 low B. Body wood is the luthier's choice: the instrument's voice is sculpted in
DSP, so wood is for feel, weight, and looks.

**The one non-negotiable architectural rule: two separate chambers.** An acoustic chamber under
the top, carrying body resonance for the piezos to pick up. A sealed electronics chamber in the
back with a removable panel, at least 150 × 90 × 25mm interior. Solid wood wall between them.
Combine them and the electronics rattle against the top, pick up noise, and change the acoustic
chamber's voice unpredictably.

Other luthier work: a 5 × 5mm LED channel milled around the entire side rim (skipping the neck
heel and the strap buttons), flush-recessed pockets for the NeoTrellis and front-face controls, a
lens window for the lidar, edge cutouts for TRS and USB-C, ferruled cable pass-throughs, and — if
possible — a solid unchambered block under the bridge humbucker, because magnetic pickups plus
semi-hollow plus heavy DSP gain is a feedback recipe.

The SubSix needs no route at all. It slides under the strings with its own height adjuster; it
just needs 50mm of clear flat string length between the pickup rings and two cable
pass-throughs.

## Compute

**Bela Gem Multi** — 10 audio in / 10 out at 24-bit/96kHz, 8 separate analog control inputs at
16-bit, 16 GPIO, I²C plus Qwiic, USB-C device and USB-A host, ~1ms round-trip. On a
**PocketBeagle 2** (quad Cortex-A53 at 1.4GHz plus an M4F and PRUs), running Linux + Xenomai
with the per-core render API across all four cores in parallel.

**C++** for orchestration, the pickup mixer, sensor I/O, multi-core dispatch, and the pitch
shifter wrapper. **Faust** for the modulation, time, filter, saturation modules and the synth
voices — it compiles to tight optimized C++ inside Bela. Csound and Pure Data are deliberately
not used; their interpreter overhead is the documented cause of headroom struggles on classic
Bela.

## The buffer board — the only custom analog

Ten channels of unity-gain FET-input follower on one small protoboard. **Impedance matching, not
gain** — Bela has ~59dB of input gain available in software; the buffer exists to give every
passive pickup the high-Z load it wants.

For the contact mics this is mandatory, not optional: a piezo's ~15nF source capacitance into a
50kΩ input forms a ~200Hz high-pass that eats the entire bass response and makes it sound thin
and quacky. With R1∥R2 ≈ 2.35MΩ the corner drops to ~4.5Hz. TL074s, single 5V supply, mid-rail
biased, AC-coupled both ends, all passive RC, flat across the band.

## Flagship pitch shifter

The headline effect, targeting H90-class clean transposition — low artifact, transient
preserving, polyphonic across chords and mixed-string signal, wide interval range with no
quality cliff. **Signalsmith Stretch** integrated into the C++ layer: modern phase-vocoder
lineage, dedicated low-latency modes, clean permissive C++. It gets its own dedicated A53 core.
Rubber Band's real-time mode is the backup.

Beyond it: vibrato, chorus, flanger, phaser, tape-style delay, an FDN reverb (convolution
avoided to preserve headroom), analog-modeled saturation, a modulatable state-variable filter,
and modulation utilities — all hot-swappable Faust modules sequenced by the patch loader.

## Hexaphonic synth

A YIN-class pitch tracker per SubSix string emits internal note events driving a six-voice
polyphonic synth on Bela. Per-string isolation makes tracking far easier than polyphonic
detection on a summed signal — detection latency is bounded by string period, so low B (~62Hz)
locks in ~16ms and high B (~247Hz) in under 5ms.

The synth is the project's **scalable DSP load**: v1 voices are simple subtractive, and
complexity grows — FM, wavetable, additive, physical modeling — into whatever headroom remains
after the mixer, shifter, and effects.

## Controls — every one of them soft

A Qwiic chain with no soldering between devices: NeoTrellis 4×4, VL53L1X lidar, BNO055 9-axis
with onboard fusion, and a NeoDriver Seesaw that drives the perimeter WS2812 strip over I²C so
the audio thread is never held hostage by WS2812 timing. Plus two pots, two encoders, a 5-way
blade switch and a Jazzmaster slide switch on resistor ladders.

A `ControlBus` running at ~1kHz holds every source's normalized value under a string ID —
`pot.1`, `lidar`, `imu.roll`, `neotrellis.r2c3`. A **patch** is a routing graph plus a mapping
table, stored as hot-reloadable JSON. Nothing is hardwired. That's what makes it "hyper": its
behavior is patch-defined, not panel-fixed.

## Power and output

Stereo 1/4" TRS is the zero-latency wired path. USB-C doubles as a USB Audio Class gadget
straight into a DAW and as the programming connection. An optional WiFi dongle adds OSC control
and a casual UDP audio stream — convenience grade, not for monitoring.

A 5000mAh LiPo behind a load-sharing USB-C charger and a PowerBoost 1000 gives about 5 hours at
the system's ~3.5W draw, with seamless handover when USB is plugged or pulled. Polyfuse on VBUS,
protected cell, and the LiPo is never sandwiched under a module — it has to be able to vent.

## Open items

Neck and bridge humbucker models still being auditioned; the Gem Multi's full input electrical
spec is pending publication (the design is robust to it via the buffer board); the per-core
render API symbol names get confirmed at bench bring-up; the LED strip length depends on the
as-built body perimeter.
