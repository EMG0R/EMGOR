---
id: emgor.hardware.lite
title: LITE
blurb: Teensy 4.1 light instrument — 20-voice generative synth in a printed bowl
parent: emgor.hardware
source: ______LITE/
downloads:
  - files/lite-bigns-wiring.txt
  - files/lite-bigns.ino
tags: [teensy, neopixel, generative, synth, quad-audio, 3d-print]
updated: 2026-03-20
draft: false
---

# LITE

A light instrument. A Teensy 4.1 driving a 25-pixel NeoPixel strip inside a printed bowl,
running a **20-voice modulated sine synth** with generative weather layers out four channels of
I2S audio. Firmware is called **BIGNS**.

**Status: working instrument.** Firmware runs, wiring is documented, the bowl is printed.

## The synth

Twenty modulated sine voices — five per output channel — each with its own carrier oscillator,
modulator, envelope, and amplifier. On top of that sit three generative layers per channel:

- **Waves** — white noise through a state-variable low-pass, slow swell.
- **Rain** — twenty independently triggered droplet envelopes through their own filters, timed
  on a Minnaert model so each drop's pitch follows its bubble radius.
- **Thunder** — a second noise chain, low-passed hard, on rare triggers.
- **Kick** — body plus sub plus click, three oscillators with independent envelopes summed and
  high-passed. Ported from the *ciesen* kick.

Everything is pitched from a hardcoded **six-octave C major table**, 48 entries from 65.41Hz to
4186Hz, with a continuous pitch-shift multiplier on top so the pot slides the whole field
without ever leaving the scale.

Signal flow per channel: five sine voices → two sub-mixers → channel mixer alongside the noise
layers → premix with the kick → master amp → one of the four `AudioOutputI2SQuad` channels.
Four discrete outputs, not stereo — the instrument is meant to be spread across space.

## Controls

| Input | Teensy 4.1 pin |
|---|---|
| Encoder A / B / switch | 6 / 8 / 2 |
| Pitch pot | 14 |
| Probability pot | 16 |
| Master volume pot | 17 |
| Joystick X / Y / button | 18 / 19 / 3 |
| Arcade button | 4 |
| I2S LRCLK / BCLK / TX1 / TX2 | 7 / 20 / 21 / 23 |
| NeoPixel indicators (2) | 5 |
| NeoPixel strip (25) | 9 |

The **probability** pot is the one that matters — it doesn't set a rate, it sets how likely each
voice is to fire on any given window. Turn it up and the texture thickens without ever locking
to a grid.

## Power warning

The 25-pixel strip runs off an **external 5V supply**, never the Teensy's regulator. The two
small indicator pixels can sit on 3.3V. The external supply's ground has to tie back to Teensy
ground, along with every pot low leg, the joystick, the encoder switch, and both buttons — one
common ground bus or nothing works reliably.

## Enclosure

`LITE_BOWL1.2.3mf` — a printed bowl that the light lives inside and diffuses through, with
`BOTTOM HOLES.3mf` as the base plate carrying the controls and cable exits.
