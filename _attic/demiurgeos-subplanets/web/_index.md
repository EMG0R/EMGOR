---
id: emgor.papers.demiurgeos.web
title: Web Control
blurb: The companion TUI in a browser — telemetry, patches, power, transport
parent: emgor.papers.demiurgeos
source: ______2026NEW/DEMIURGE_OS/web
downloads:
  - files/demiurge_web.py
  - files/demiurge-web.service
tags: [python, web-ui, sse, telemetry, control-surface]
updated: 2026-07-28
draft: false
---

# Web Control

`demiurge-web` puts the terminal companion in a browser: `http://demiurge.local:8080` from any phone on the LAN. Same fields, same green/yellow/red thresholds, same patch tiles with the `▸` active marker — the design directive was "mirror the companion", and browser-native extras (sparklines, SSE push, big touch targets) are allowed only where they don't change the vibe.

## Architecture

One Python file, **stdlib only** — `ThreadingHTTPServer` serving static files, a JSON API, and Server-Sent Events. No Flask, no pip installs, no build step. Runs as its own systemd unit pinned to cores 0-2 (never the audio core), deliberately independent of `demiurge.service` — the UI must stay reachable when audio is stopped, because that's the whole point of a start button.

Every 2 seconds the daemon runs the same probes the companion uses, locally: `vcgencmd`, procfs/sysfs, `systemctl is-active`, `live.conf` and sets parsing. Each frame is written atomically to `~/.demiurge/status` as JSON and pushed to browsers over SSE — the page never polls.

## What it shows

- Temp, CPU freq, load, memory as bars; throttle-register decode (UNDERVOLT / ARM-CAP / THROTTLE / TEMP-LIM, now vs past)
- Online-core count as a brownout canary; PMIC temp and EXT5V volts; 60-second rolling xrun count
- Active patch and a parsed signal-flow row per chain: `IN ─▶ neptrPhase4 ─▶ OUT`
- Numbered patch tiles from `~/demiurge/sets/`, each with its power tag
- Sparklines over the last ~10 minutes for temp/load/xrun/EXT5V, plus a throttle-event timeline to correlate audio weirdness with power events

## What it does

Tap a tile to switch patches (one-tap confirm — it restarts audio), set power low/med/high, start/stop/restart the audio service (two-step red-arm confirm with a 3-second timeout). Actions run serially, log to `~/.demiurge/logs/web-actions.log`, and trigger an immediate status frame. LAN-open by design: no login, stage-friendly dark UI, phone-first.
