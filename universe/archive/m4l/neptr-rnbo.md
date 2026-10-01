---
id: emgor.archive.m4l.neptr-rnbo
title: NEPTR (Max / RNBO)
blurb: The last Max patch before NEPTR left RNBO behind — exported to the web, archived here
parent: emgor.archive.m4l
source: ______PHASE3/neptr_p1_v6.5_BMOv2.maxpat
downloads:
  - files/neptr_p1_v6.5_BMOv2.maxpat
links:
  - { label: "Play NEPTR (web/RNBO export)", url: "NEPTR.html" }
tags: [max, rnbo, max-for-live, neptr, archive]
updated: 2026-09-30
draft: false
order: 11
---

# NEPTR (Max / RNBO)

The most recent NEPTR code that lived in Max/RNBO: `neptr_p1_v6.5_BMOv2.maxpat`, dated 2025-12-18, a ~7.3 MB patch heavy with RNBO objects — the tail end of the `neptr_v3` → `v6.5` Phase 1 lineage documented on the [Performance System of a Modern Digital Luthier](#/papers/neptr-performance-system) paper's [Phase 1](#/papers/neptr-performance-system/neptr/phase-1) planet. Earlier patches in that lineage ran 19–26 MB and stayed out of the repo; this one is small enough to ship.

RNBO compiled this patch's signal graph straight to WebAssembly — the live, playable export is the [NEPTR web instrument](#/web-synth/neptr). The `.maxpat` here is the Max source behind that export, for anyone who wants to see the patcher itself rather than just the compiled result.

NEPTR has since moved on from RNBO: Phase 4 runs Csound on DEMIURGE. This page exists so the Max/RNBO era has a plugin-shelf home instead of only living in the paper's archive.
