---
id: emgor.neptr.phase-1
title: Phase 1 — Pi + RNBO
blurb: Max patches compiled onto a Raspberry Pi — the era that proved the idea
parent: emgor.neptr
source: _____pi/
downloads:
  - files/BMO.py
  - files/_COMBINED_UI_YAYY.py
links:
  - { label: "NEPTR web (RNBO export of this era)", url: "NEPTR.html" }
tags: [max, rnbo, raspberry-pi, kivy, pcb, icmc]
updated: 2026-07-28
draft: false
---

# Phase 1 — Pi + RNBO (2023–2025)

The original NEPTR: a Max/MSP patch lineage (`neptr_v3` → `v6.5`) exported through Cycling '74's RNBO and flashed onto a Raspberry Pi running the `rnbooscquery` image. Guitar in, effects computer, amp out — no laptop on stage.

## What was in the box

- **The patches.** `neptr_v6.3` played ICMC (with an 8-channel intro-vox piece); `neptr_v6.DALLAS.2.multi` and `v6.4` followed; `neptr_v6.5(endPHASE1).maxpat` closed the era in September 2025. The patches are ~19–26 MB each and stay in the archive — the RNBO export of this engine runs in your browser via the link above.
- **Kivy touchscreen UI.** `BMO.py` draws an animated lava-lamp face (concentric drifting circle groups, slow color transitions) — the machine's first personality. `_COMBINED_UI_YAYY.py` is the working control surface: sliders for BPM (40–800), probability, volume and sequencer params, pushed to the RNBO runtime over HTTP/OSC. Both included below.
- **input_management/.** Custom button PCBs (Gerbers fabbed as `emgor-buttonpcb`), joystick testers, Bluetooth input experiments — the first stab at NEPTR-specific hardware controls.
- **3D print.** A frog enclosure. Naturally.
- **samps/.** The 808/kick/snare/hat drum set the sequencer fired.

## Why it ended

RNBO's export was a black box: every DSP change meant a round trip through Max on the laptop, and the patch had grown past 25 MB of JSON. Phase 2 threw out the export pipeline and wrote the engine by hand.
