---
id: emgor.hardware.smartgor
title: smartGOR
blurb: A Raspberry Pi 4 room hub — 800-LED FCOB lighting, plant watering, BLE lamps, one MQTT broker
parent: emgor.hardware
source: ______2026NEW/smartGOR/
tags: [raspberry-pi, esp32, mqtt, ws2812b, home-automation, ble, plant-watering, iot]
updated: 2026-08-12
draft: false
---

# smartGOR

A DIY smart-room build: a Raspberry Pi 4 as a "little server" for the room —
Mosquitto MQTT broker, FastAPI backend, a web dashboard, Tailscale for remote
access — talking to a handful of ESP32 nodes over WiFi (MQTT, slow path) and
ESP-NOW (fast path, beat-sync). Not a product; a single physical room's
lighting, watering, and BLE lamps unified under one hub, documented in-repo
as `SMART_ROOM_MASTER_PLAN.md`.

Two control planes, on purpose: MQTT over WiFi for scenes/palettes/schedules/
telemetry, and an ESP-NOW mesh (~1–2 ms) for beat pulses and palette sync
between reactive lighting nodes, so latency-sensitive sync never waits behind
the broker.

## Subsystem A — FCOB strip lighting (live)

An 800-pixel BTF-LIGHTING FCOB WS2812B strip (5m, individually addressable)
driven by a DOIT ESP32 DevKit V1 on GPIO 13. Firmware (`fcob-stream`) is a
superset of the canonical sketch: generative nebula/pulse/chase/rainbow
modes, an HTTP JSON API, non-blocking WiFi, OTA — plus a `stream` mode that
takes real-time per-pixel frames over UDP (port 21324, a small framed
protocol: magic bytes, version, offset, count, RGB payload) so a Pi-side
renderer can push full 800px frames at up to ~30fps. 5 seconds of silence on
the UDP stream falls back to the last non-stream mode automatically.

That Pi-side renderer is **lightserver**: a Python render engine with a
layered scene DSL (particles, springs, waves, noise, plasma, gradient, solid,
paint), linear-light HDR compositing, tone-mapping, and natural-language
scene generation that shells out to the `claude` CLI to turn a text prompt
into scene JSON. It supports multiple LED nodes via a `nodes.json` registry
and ships a `--preview` mode that renders the strip as a truecolor ANSI strip
in the terminal for development without hardware attached.

**Status: deployed.** The repo's commit history shows lightserver running on
the Pi as a systemd unit, and `server-integration/app.py` — a snapshot of the
Pi's live chat-agent server — was extended to let the room's conversational
agent embed `<lights>vibe description</lights>` tags in its replies, which
get stripped from the user-visible text and POSTed to the lightserver's
`/prompt` endpoint. Chat-to-lights is live, not speculative.

Power/wiring were treated carefully in the design doc: a 5V/20A PSU sized for
worst-case current draw, power injection at both ends of the run, a
74AHCT125 level shifter on the data line, and an explicit warning against
powering the strip off the ESP32's 3.3V rail.

## Subsystem B — Miortior BLE lamps

Two ELK-BLEDOM-family BLE lamps (GATT service `FFF0`), controlled either by a
dedicated ESP32 BLE↔MQTT bridge or — the currently preferred path — directly
from the Pi's onboard BLE via BlueZ/`bleak`, removing the extra ESP32
entirely. Reactive mode is planned to lean on the lamps' built-in
music-reactive BLE command rather than custom streaming, with a documented
fallback if that turns out to be phone-app-driven rather than onboard-mic.

## Subsystem C — NeoPixel jars (reactive mesh, planned)

Per-jar ESP32 + WS2812B/SK6812 cluster, no per-jar microphone. Reactive data
arrives over ESP-NOW from a single centralized audio pipeline (see below);
one jar or a dedicated ESP32 bridges ESP-NOW to MQTT/UDP for the Pi. Documented
as the last subsystem to build — most involved, and dependent on everything
else already existing.

## Subsystem D — Plant watering + bio-data (designed, not yet built)

An 8-zone, all-I2C design: an ESP32 Feather V2 through a TCA9548A 8-channel
I2C mux to 8x Adafruit STEMMA capacitive soil sensors (5 zones active, 3
spare), and a KRIDA 8-channel I2C relay board driving 12V solenoid valves plus
a pump. Watering logic lives on the Pi (per-species moisture thresholds,
short valve bursts, one-valve-at-a-time interlock), not on the ESP32, which
just publishes moisture readings over MQTT. A tank float switch is a hard
low-water cutoff. A stretch feature — galvanic plant-tissue "bio-signal" data
from spare ADC pins — is scoped as a free-floating data stream for the
dashboard and a possible future modulation source for the lighting scenes.

## Reactive audio pipeline (locked design)

One USB mic on the Pi, no per-node mics: capture → beat/energy/FFT detection
in Python (aubio/numpy) → a compact UDP packet over WiFi to a bridge ESP32 →
ESP-NOW broadcast to every LED node. Budgeted end-to-end latency ~20–50ms,
under the ~100ms threshold where light-to-beat lag starts reading as "off."

## Dashboard

A single static-page web control surface (FastAPI + websockets) for the
strip, lamps, jars, watering zones, and generic switches, all over MQTT. Runs
in a demo mode with simulated moisture/tank data when no broker is present,
so the UI is clickable without hardware attached — a status pill flags
`◐ demo mode` vs `● live · MQTT`.

## Status

Mixed, and stated honestly per subsystem: the FCOB strip and its lightserver
render engine are **live and deployed**, with chat-to-lights control wired
into the room's conversational agent on the Pi. The dashboard is **scaffolded
and runnable in demo mode**. The BLE lamp bridge, NeoPixel jar mesh, and
8-zone plant watering system are **designed in detail but not yet built** —
the master plan document is explicit that some of this (jars especially) is
sequenced last because it depends on infrastructure (broker, scene model,
ESP-NOW channel discipline) that has to exist first.
