# Septagraph v1 — latency table

Measured with `septagraph.tools.latency_probe` (plan Task 3.4), 200
trials per stage, times in milliseconds. Software stages were measured
directly; the hardware stage is cited from prior, separate measurements
(not re-measured here — see note below).

| stage | host | p50 (ms) | p95 (ms) | max (ms) | n |
|---|---|---|---|---|---|
| controller (key event → `GridController.on_key` → `Sender.send`) | Mac (Emorys-MBP.lan) | 0.023 | 0.024 | 0.075 | 200 |
| controller (key event → `GridController.on_key` → `Sender.send`) | Pi (neuralgrid) | 0.034 | 0.043 | 1.001 | 200 |
| wire (`Sender` → loopback `Receiver` → decoded `Hit`) | Mac (Emorys-MBP.lan) | 0.067 | 0.078 | 0.160 | 200 |
| wire (`Sender` → loopback `Receiver` → decoded `Hit`) | Pi (neuralgrid) | 0.041 | 0.075 | 0.289 | 200 |
| renderer (`Sender` → native ChucK renderer, `--trace` gate print) | Mac (Emorys-MBP.lan) | 0.108 | 0.152 | 0.291 | 200 |
| renderer (`Sender` → native ChucK renderer, `--trace` gate print) | Pi | — (skipped, `--no-chuck`) | — | — | — |
| **hardware (serial link, measured separately)** | Pi (`neuralgrid`) | ~4.75–5.52 | ~6.4–8.9 | ~6.9–9.4 | 20 (×2 runs) |

Hardware row source: `neuralGrid/pi/tools/README.md` and `HANDOFF.md` §10
("Follow-up (2026-09-03, later same day): link-RTT fix" and the
`read(1)`+drain reader table) — PING→HELLO serial round trip on the
ESP32↔Pi COBS link, `pi/tools/latency_probe.py --rtt-samples 20
--led-samples 20 --stream-s 2.0`, monitor stopped, fw5. Also on record
there: LED apply latency (FULL) p50/p95 ≈ 5.18/6.56 ms, and LED apply
(PARTIAL, under real lavalamp load) p50/p95 = 5.11/6.23 ms idle, 8.31/9.81
ms under load — well under the task's <10 ms p95 partial-update target.
This row is **not** produced by `latency_probe.py` in this directory; it
is carried forward as-is per the plan's instruction not to re-measure
hardware already characterized on that separate pass. `latency_probe.py`
itself needs no serial port and was run on the Pi without stopping
`neuralgrid-monitor`.

Renderer stage skipped on the Pi: the Pi has no working performance-path
use of ChucK in this project (Global Constraint: "ChucK is not used on
the neuralGrid Pi for performance logic — ChucK = web reference
renderer"), so the probe was run there with `--no-chuck` and only the two
software stages (`controller`, `wire`) were collected.

## Reading the numbers together

The three software stages (controller, wire, renderer) are all
sub-millisecond at p95 on both hosts measured, dwarfed by two orders of
magnitude by the ESP32↔Pi serial hardware stage (single-digit
milliseconds p50/p95). The full key-press → sound chain is therefore
hardware-dominated: `key press → serial RTT (~5 ms) → controller fast
path (~0.02–0.04 ms) → wire/decode (~0.04–0.08 ms) → renderer gate
(~0.1–0.15 ms, native ChucK; a WebChucK or Machine Lab renderer would add
its own OS/network hop but nothing structurally different)`. All three
software stages comfortably clear the plan's Global Constraint of
"< 10 ms controller-side" for the fast plane, with roughly two orders of
magnitude of headroom.

## Model-on is structurally identical

The plan's Global Constraint — "the fast plane never has a model in it";
`/hit` and `/move` paths carry no inference, no blocking I/O — holds
throughout every stage measured here: `GridController.on_key` is a dict
lookup + a window projection (pure arithmetic) + OSC encode + `sendto`;
the wire stage is a UDP receive + stateless decode; the ChucK renderer
stage is OSC parse + a handful of global-variable writes that a running
DSP graph picks up on its own control-rate schedule. None of these
stages branches on, calls into, or blocks on any model — there is no
"model on" code path to diverge from the one measured. Consequently,
whatever generative or adaptive model eventually sits elsewhere in the
Septagraph system (e.g. driving `/shape` on the slow plane, or offline
catalog measurement per Task 4.1) cannot change these numbers: it isn't
on this path, by construction, and turning it on or off leaves the
fast-plane latency profile in this table unchanged.
