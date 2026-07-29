---
id: emgor.demiurge.launcher
title: Launcher
blurb: Zero-dependency Rust patch-graph engine — 460 KB, event-driven
parent: emgor.demiurge
source: ______2026NEW/DEMIURGE_OS/src/demiurge-launcher-rs
downloads:
  - files/live.rs
  - files/Cargo.toml
tags: [rust, supervisor, patch-graph, hot-reload]
updated: 2026-07-28
draft: false
---

# Launcher

`demiurge-launcher` is the brain: a zero-dependency Rust crate (stdlib only), ~460 KB stripped, cold start under 50 ms, steady-state CPU under 1%. Installed to `/opt/demiurge/bin/` and started at boot by `demiurge.service`. It is the only launcher DEMIURGE ships.

## What it does

1. **Parses** `live.conf` into an internal `Program` / `Edge` graph — users never see the graph types, only the config file.
2. **Aggregates** every USB audio class device macOS-style: smaller-channel-count interfaces fill FL/FR first, additional devices fill RL/RR on `demiurge-sink`. The pass is idempotent — it diffs against current PipeWire links and applies only what changed, which is what makes hot-plug re-runs safe.
3. **Launches** each stage via per-language `demiurge-run-*` wrappers that bake in the PipeWire-JACK shim environment. The launcher never embeds language CLI knowledge — tweak a wrapper, not the engine.
4. **Links** the audio graph, matching each language's stereo port-naming dialect, and scrubs undeclared auto-connections (Faust loves to grab hardware playback at startup; the scrub pass catches it).
5. **Wires MIDI** — every stage subscribed bidirectionally to `Midi Through`, plus hot-plugged physical controllers.
6. **Listens**: a thread tails `pw-mon` and debounces USB add/remove events, a child-watcher polls `/proc/<pid>`, and a 2-second heartbeat catches drift.

## Graceful failure

The graph is directed and self-healing. A middle stage dying re-links its upstream to its downstream — `A → B → C` with B dead becomes `A → C`, and the mix keeps playing. When B comes back, the original graph is restored on the next state-change tick.

## Hot reload

The launcher polls `live.conf`'s mtime once a second and reconciles by `(id, file)`: unchanged stages keep their PIDs (no xruns), added stages start, removed stages stop, the graph re-applies. It also writes a read-only device inventory into a fenced block at the top of `live.conf`, so the file you edit always shows what the Pi currently has plugged in.

`live.rs` (in files/) is the parser — a good entry point into the crate.
