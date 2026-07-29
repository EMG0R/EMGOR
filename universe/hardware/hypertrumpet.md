---
id: emgor.hardware.hypertrumpet
title: hyperTrumpet
blurb: Augmented trumpet — Teensy 4.1 sensor rig into Max, everything on channel 3
parent: emgor.hardware
source: ______2026NEW/hyperTrumpet/
downloads:
  - files/hypertrumpet.ino
tags: [augmented-instrument, teensy, midi, max-msp, trumpet]
updated: 2026-03-27
draft: false
---

# hyperTrumpet

A trumpet with a Teensy 4.1 strapped to it and twenty-odd continuous controllers coming out. Every
sensor maps to a USB MIDI CC on channel 3; a Max patch turns them into the actual instrument.

Built at CalArts Creative Computing. **Status: built, working, performed.**

## What it senses

| Source | CC | Notes |
|---|---|---|
| 3× slider | 20–22 | Linear expression |
| VL53L0X time-of-flight | 23 | Distance to hand / audience |
| FSR | 24 | Squeeze pressure |
| 2× pot | 28–29 | Set-and-forget offsets |
| Joystick X / Y | 30–31 | Vertical flipped so up = 127 |
| 2× encoder value | 36–37 | Relative, accumulated |
| Accelerometer X/Y/Z | 41–43 | Instrument attitude and gesture |
| Gyro X/Y/Z | 44–46 | Rotation rate |
| BME680 breath | 47 | Environmental / breath pressure |
| 2× contact mic | 48–49 | Body resonance as control signal |
| 3× valve (hall effect) | 33–35 | Non-contact valve sensing |
| Joystick, encoder, arcade buttons | 32, 38–40 | Momentary |
| NeoTrellis 4×4 pads | 50+ | Scene / mode |

Two I²C buses keep the address space clean: `Wire` (SDA 18 / SCL 19) carries the NeoTrellis at
0x2E, the BME680 at 0x77, and the VL53L0X at 0x29; `Wire1` (SDA 17 / SCL 16) carries the IMU at
0x68, read over raw I²C with no library.

The valves are read with **hall effect sensors** rather than switches — no mechanical contact
with the horn's own action, no added friction, no drilling into anything that matters.

## Light

Two NeoPixel strips (9 pixels on the main body, 6 reactive) plus the Trellis grid all run a
multi-sine generative lava lamp through a purple and violet palette with dark blue hints.

The grid is reactive to everything at once: button presses throw ripples, the joystick pushes
color and draws an arrow, accel X shifts hue, the sliders and joystick set brightness, the lidar
drives animation speed, the FSR brightens and flips a checkerboard, and the pots draw diagonal
lines. Every modulation fades back to the base animation when the input goes idle, so the
instrument always returns to breathing rather than freezing on the last gesture.

## The other half

`PERFRMNC.maxpat` is the performance patch; `midiinHyperTrump.maxpat` handles the incoming CC
routing and scaling. The Teensy is deliberately dumb — it senses, smooths, and sends. All
musical mapping decisions live in Max, where they can be rewritten between rehearsals instead of
reflashed.

Setup: Arduino IDE → Tools → USB Type → **MIDI**. Libraries: Adafruit_NeoPixel,
Adafruit_NeoTrellis, Adafruit_BME680, Adafruit_VL53L0X, Encoder.
