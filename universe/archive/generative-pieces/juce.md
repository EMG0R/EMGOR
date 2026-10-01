---
id: emgor.code.juce
title: JUCE
blurb: C++ generative synths as JUCE console apps — run one, get a composition
parent: emgor.code
source: __juco/
downloads:
  - files/piGO.cpp
  - files/coolCLICKY.cpp
tags: [cpp, juce, generative, cmake]
updated: 2026-07-28
draft: false
---

# JUCE (\_\_JUCO)

C++ experiments in JUCE, built with CMake as console audio apps — no plugin wrapper, no GUI, just `AudioAppComponent` straight to the device.

**piGO** — *"run program to generate a new composition with a unique key and BPM — great for sleep, focus and mood."* Every launch rolls a BPM between 65 and 120, a key offset between −8 and +2 semitones, and a weighted note table over a major scale — with a 20% chance the F♯ gets admitted to the scale at all, at its own random weight. A timer driven off the BPM triggers voices from a round-robin allocator. Same program, different piece, every run.

**coolCLICKY** — the percussive sibling: same architecture pushed 3× faster (135–240 BPM) with plucky ADSR settings, turning the ambient engine into a clicking, metronomic texture generator.

Both `.cpp` sources are downloadable; each is a complete program. Build with the repo's `CMakeLists.txt` against a local JUCE checkout (C++17, `juce_add_console_app`).
