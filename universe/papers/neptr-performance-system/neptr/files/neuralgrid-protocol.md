# neuralGrid — ESP32 ⇄ Pi Serial Protocol v1

**Status:** locked contract for Phase 1. Both firmware and the Pi live layer implement
exactly this. The ESP32 is a **dumb I/O bridge** — it sends raw sensor/button data up and
renders LED instructions down. It runs **no musical logic** and is **never rewritten by the
agent**. All intelligence (touch detection, modes, MIDI, models) lives on the Pi.

## Physical layer

- USB CDC serial (ESP32 USB → Pi USB). Nominal baud 115200 (CDC runs at USB speed regardless).
- Bytes only; all framing/structure defined below.

## Framing — COBS

Every message on the wire is:

```
COBS( payload )  ||  0x00
```

- `0x00` is the frame delimiter (never appears inside a COBS-encoded payload).
- COBS ([Consistent Overhead Byte Stuffing]) is self-synchronizing: a receiver that joins
  mid-stream resyncs at the next `0x00`. Chosen because LED payloads are arbitrary binary
  (including `0x00` bytes) and COBS removes all of them without escaping.

### payload layout (pre-COBS)

```
[ TYPE : 1 byte ] [ BODY : n bytes ] [ CRC16 : 2 bytes, little-endian ]
```

- `CRC16` = CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) computed over `TYPE || BODY`.
- Receiver: strip delimiter → COBS-decode → verify CRC → dispatch on TYPE. Drop frame on
  CRC mismatch or decode error (do not crash; wait for next `0x00`).

## Message types

### Up — ESP32 → Pi

| TYPE | Name    | BODY |
|------|---------|------|
| 0x01 | HELLO   | `proto_ver:u8, grid_w:u8, grid_h:u8, n_analog:u8, fw_ver:u8` |
| 0x10 | KEY     | `x:u8, y:u8, edge:u8` — edge 1 = press, 0 = release |
| 0x11 | ANALOG  | `n:u8, then n × u16(LE)` — **raw** ADC values 0..4095, one per channel |

- `HELLO` is emitted once on boot and again in response to `PING`. It is the handshake:
  the Pi learns grid size, analog channel count, and firmware version.
- `KEY` is emitted on every press/release edge (not batched).
- `ANALOG` is emitted at a fixed ~50 Hz (every 20 ms). Values are **raw and ungated** —
  the ESP32 does zero noise filtering or touch detection; that is the Pi's job. Channel
  order for the current build: `[0]=softpot N, [1]=softpot S, [2]=softpot E, [3]=softpot W`.
  Adding FSR/piezo later only increases `n`; the contract does not change.

### Down — Pi → ESP32

| TYPE | Name         | BODY |
|------|--------------|------|
| 0x20 | LEDS_FULL    | `192 bytes` = 64 × (r:u8, g:u8, b:u8), index i → x=i%8, y=i/8 |
| 0x21 | LEDS_PARTIAL | `count:u8, then count × (x:u8, y:u8, r:u8, g:u8, b:u8)` |
| 0x22 | BRIGHTNESS   | `level:u8` (0..255) — global NeoTrellis brightness |
| 0x23 | CLEAR        | (empty) — all pixels off |
| 0x30 | PING         | (empty) — ESP32 replies with HELLO |

- Pixel addressing matches key numbering: `x` = column 0..7 (left→right), `y` = row 0..7
  (top→bottom), consistent with `Adafruit_MultiTrellis` `(x,y)` and the 2×2 board tiling
  `0x2E 0x2F / 0x30 0x31`.
- `LEDS_FULL` is the simple path (send whole frame each render). `LEDS_PARTIAL` is the
  efficient path for sparse updates. Firmware applies the write and calls `show()` once.

## Versioning

- `proto_ver` starts at `1`. Any breaking change to framing or message layout bumps it.
- The Pi checks `proto_ver` in HELLO and refuses to drive an incompatible firmware (logs a
  clear error rather than sending frames it can't honor).

## Touch detection is NOT here

Deliberately. The ESP32 sends raw ADC. Distinguishing "finger on the strip" from "floating
noise" happens on the Pi (variance/stability + debounced onset/release), so the detection
logic can be tuned and rewritten without reflashing firmware. Hardware aid: a ~100 kΩ
pulldown from each SoftPot wiper to GND (required on GPIO34/35, which are input-only with no
internal pulldown) makes untouched channels sit near 0 and touched channels read a real
position — strongly recommended, see the wiring docs.
