---
id: emgor.hardware.ofoots
title: oFOOTS
blurb: Ten-encoder foot controller — 8 pages, 64 CCs, one expression pedal
parent: emgor.hardware
source: ____oFOOTS/
downloads:
  - files/ofoots.ino
tags: [controller, midi, foot-controller, tft, neopixel]
updated: 2026-01-23
draft: false
---

# oFOOTS

A foot controller with **ten rotary encoders**, each with a push switch, paged eight deep. Eight
encoders are the working surface; the other two are the page navigators. That's 64 rotation CCs
and 64 button CCs from a box you never take your hands off the instrument to use.

**Status: working. Display upgrade in progress.**

## Paging

Encoders 9 and 10 step page down and page up across eight pages. Every page holds its own
button states and encoder values, saved on the way out and restored on the way in, so a page is
a full snapshot rather than just a CC bank. Each of the 64 slots has a hand-assigned CC pair
chosen to spread across the map and avoid stepping on anything standard.

Switching pages flashes a count of white pixels — one for page 1, eight for page 8 — for half a
second, non-blocking, then falls back to the animation. The base color scheme is derived from
the page index, so each page has its own hue rotation and you learn where you are by color.

Two hidden toggles, both held for three seconds:

- **Encoders 9 + 10** → knob mode. Encoder rotation sends its current value on page entry
  instead of only on movement, so a hardware knob position and the software parameter re-sync.
- **Encoders 5 + 8** → reverse the button CC toggle polarity, for gear that expects the
  inversion.

Both confirm with an LED flash.

## Expression pedal

On A15 (pin 39), with a one-pole smoother at α = 0.1 and a deliberately over-ranged map — raw
0–1023 into 160 down to −28, scaled 1.07× and clamped to 0–127. That over-range is intentional:
real pedals never reach their theoretical travel limits, and the overshoot guarantees the ends
of the sweep actually hit 0 and 127 instead of stopping at 6 and 119.

Sends only when the value moves more than 2 and at most every 25ms, which keeps a foot resting
on the pedal from flooding the MIDI bus.

## Light and display

Twelve NeoPixels — eight following the working encoders through an HSV rotation seeded by the
page, plus four on the navigators that pulse a rainbow whose brightness tracks the expression
pedal position. Any pressed encoder button goes white.

The display currently runs an ILI9341 over SPI (DC 9, CS 10, RST 8). The
**Adafruit_ST7796S_kbv** driver is vendored in the project directory for the move to a larger
ST7796S panel — the sketch hasn't been switched over yet.

`testr/testr.ino` is the expression-pedal bring-up sketch: raw analog and mapped MIDI value
straight to serial at 100Hz. Start there when a pedal misbehaves.
