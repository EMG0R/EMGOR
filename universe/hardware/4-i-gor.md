---
id: emgor.hardware.4-i-gor
title: 4-i-Gor
blurb: Teensy 4.0 quad-in / quad-out USB audio interface with MIDI
parent: emgor.hardware
source: ______2026NEW/4-i-Gor/
downloads:
  - files/4-i-gor.ino
  - files/4-i-gor-pcb.md
tags: [audio-interface, teensy, i2s, midi, pcb]
updated: 2026-04-07
draft: false
---

# 4-i-Gor

A quad-I/O USB audio interface on a Teensy 4.0. Shows up on the computer as a sound card and a
MIDI device at the same time.

- **4 channels out** — 2× PCM5102 stereo DAC, 112dB SNR, line level
- **4 channels in** — 2× PCM1808 stereo ADC, 24-bit/96kHz, 105dB SNR
- **MIDI in + out** — 1/8" TRS Type A on a hardware UART, optoisolated input
- **Sustain in** — 1/8" TRS footswitch

**Status: firmware written, PCB spec'd, enclosure designed.**

## Why the Teensy 4.0 handles this without heroics

The iMX RT1062 has **two independent hardware I2S buses** (SAI1 and SAI2), and the Teensy Audio
Library exposes both directly:

| Object | Bus | Direction |
|---|---|---|
| `AudioOutputI2S` | SAI1 TX | pin 7 → DAC 1 |
| `AudioInputI2S` | SAI1 RX | pin 8 ← ADC 1 |
| `AudioOutputI2S2` | SAI2 TX | pin 2 → DAC 2 |
| `AudioInputI2S2` | SAI2 RX | pin 3 ← ADC 2 |

All four share one MCLK/BCLK/LRCLK — hardware synchronized, zero clock drift. DMA moves every
sample. MIDI runs on Serial1's own UART with its own interrupt. CPU load sits around 5% at
44.1kHz on the 600MHz part, and the DSP pipeline runs 32-bit float on the hardware FPU before
I2S conversion. There is a lot of headroom left in this design.

MCLK goes to the ADCs only, never the DACs — the PCM5102 breakouts tie SCK to ground and run
off their internal PLL.

## Grounding — the part that decides whether it's quiet

Split ground plane. AGND under the audio ICs, the PCM1808 analog sections, and every jack
ground. DGND under the Teensy, USB, MIDI circuit, and the PCM1808 digital sections. The two
planes meet at **exactly one point**, a star near the Teensy's GND pin. Digital return current
never crosses the analog plane.

Ferrite bead between the 5V digital rail and each PCM1808's analog VCC. 100nF within 2mm of
every VCC/VDD/VCCA pin — shorter trace, better bypass. 10µF bulk at each rail entry point. A
500mA polyfuse between Teensy VUSB and the board's 5V distribution.

MCLK is a fast square wave; treat it as a digital signal, route it short, keep it away from the
ADC inputs and analog traces. Never run audio parallel to clock or USB.

The 6N138 optocoupler gives the MIDI input real galvanic isolation, which kills ground loops
outright. Keep that circuit physically separated from the audio section.

## The warning worth repeating

**The PCM5102A outputs roughly 6V peak-to-peak.** Loop a DAC output straight back into a
PCM1808 input and you will damage the ADC. Any loopback needs a 2:1 divider — two equal
resistors, 10kΩ and 10kΩ is fine.

## Build notes

Arduino IDE: board Teensy 4.0, USB Type set to **MIDI + Audio**, CPU speed 600MHz. 1kΩ series
resistors protect the ADC inputs from hot signals. 10kΩ pulls hold XSMT high (unmuted) and FMT
low (I2S format) on both DACs so a floating pin can never silence the interface.

Enclosure is 3D-printed, clipping or magnetting to the lander.
