---
id: emgor.hardware.pocket-opgorator
title: Pocket OpGorator
blurb: Pocket-Operator-format Daisy sampler/synth with a velocity NeoTrellis grid
parent: emgor.hardware
source: ______2026NEW/Pocket-OpGorator/
downloads:
  - files/pocket-opgorator-spec.md
  - files/pocket-opgorator.ino
tags: [synth, sampler, daisy, neotrellis, 3d-print]
updated: 2026-06-10
draft: false
---

# Pocket OpGorator

A custom Pocket-Operator-format sampler/synth built around a Daisy Seed. NeoTrellis 4×4
velocity-sensitive pads, two encoders, a 2.42" OLED, built-in mic and stereo speakers, 3.5mm
stereo I/O that doubles as PO/Volca sync, SD card, IMU, time-of-flight sensor, and a 10,000mAh
cell. Standalone instrument or a USB audio/MIDI interface.

**Status: PCB and housing designed, assembly pending.** Every part is verified against real
product listings and datasheets; gerbers are out; the Blender housing is mid-flight with the
ear and grille dimensions still open.

## The brain

Daisy Seed — STM32H750 at 480MHz, 64MB SDRAM, PCM3060 24-bit stereo codec with AC coupling on
both ends, so no external DAC/ADC and no coupling caps. 31 user GPIO, two user-accessible I2C
buses (I2C2 is reserved by the onboard codec), three SPI. Socketed on machine-pin female headers
so it stays removable.

The Daisy is micro-USB on every revision including Rev7 — verified, not assumed. The external
port is a USB-C female breakout on the south wall wired by short flying leads to a micro-USB
male plug. Because that hop is flexible rather than a rigid panel-to-board tie, the Daisy's
position inside the case is completely free.

## Controls and sensing

| Input | Bus |
|---|---|
| NeoTrellis 4×4 — 16 pads, 32 NeoPixels | I2C1 |
| MPU9250 9-axis IMU + VL53L1X ToF | I2C4 |
| 2× CYT1100 rotary encoders with push | GPIO |
| SF45-65 force-sensitive resistor (velocity) | A0 |

Velocity is the trick: the FSR sits sandwiched between the NeoTrellis PCB and the main board, so
pressing any pad compresses it. One sensor, sixteen velocity-sensitive pads.

## Audio path

Mic auto-switching is pure hardware, zero firmware. A MAX4466 preamp feeds the PJ-307 input
jack's normally-closed contacts; insert a plug and the spring contacts physically lift off,
handing over to the external line. The mic runs off the Daisy's linear 3.3V rail, not the 5V
boost — the quietest available rail, per Maxim's own recommendation.

Speaker mute is a hybrid. The output jack's switch is referenced to TIP rather than ground, so
driving the amp's shutdown pin straight off it picks up audio AC and misbehaves. Instead the
contact goes through an RC filter into a Daisy digital input, and firmware does the polarity
inversion on a debounced 50ms poll. Two GPIO and three passives to make it reliable.

A 250Hz one-pole high-pass sits permanently on the DSP→DAC path — 20×40mm drivers can't
reproduce below ~300Hz cleanly and the filter keeps them off Xmax at volume.

Both 3.5mm jacks double as PO/Volca sync: a ~15ms 1kHz click burst at 2 PPQN, generated and
detected in the DSP across six sync modes.

## Power

TP4056 with DW01A charges the cell and stays always-live; an XL3608 boost makes the 5V rail. The
switch sits in the battery path, not on the boost enable — otherwise plugging in USB keeps the
Daisy alive off VBUS and OFF stops meaning off. Since the battery path pulls 1.8A typical and
2.6A peak, an EG1218 slide switch (200mA rated, verified against the E-Switch datasheet after an
earlier version of this doc claimed 3A and was simply wrong) drives an AO3401 P-MOSFET load
switch instead. The switch carries microamps of gate current; the MOSFET carries the load.

The Daisy's onboard D1/D2 diodes isolate USB VBUS from the VIN rail, so the external USB-C VBUS
can be shared between the Daisy and the charger input with no external Schottky and no
recirculating charge loop. Confirmed from the Seed schematic, not guessed.

Typical draw ~1.15A at 5V, ~1.9A peak. 10,000mAh cell → roughly 5.5h moderate, 3–3.5h heavy.
NeoTrellis brightness is capped at 80% in firmware to hold the rail budget.

## Enclosure

An organic pebble/lozenge shell, 3D-printed PETG in two halves, ~100 × 141 × 46mm. The
silhouette is an hourglass/peanut curve: 77mm wide at the top, bulging to 100mm at the quarter
point, pinching to 77mm at the middle, bulging again at three-quarters. Super-rounded edges, no
text, no logo.

The front face is not flat. It's a smooth surface that rises and falls to meet each hole's
height — humping to a 16mm peak at the lidar/encoder row, easing to 15.3mm at the OLED and mic,
13mm at the pads, dipping to 11.15mm at the power switch. Cutouts are modeled as separate cutter
objects and boolean-subtracted last.

Four M2×30 socket-head screws enter from the back, run up empty corner channels clear of the
battery, pass through the PCB, and thread into brass heat-set inserts melted into solid boss
pillars in the front shell. The bosses are positioned purely from PCB hole coordinates — the
curved skin must never shift them. The bottom two are Ø5mm rather than Ø6 because they clear the
NeoTrellis corners by only about 1mm.

Speakers fire sideways out of the upper flanks into external "ears" — a flowing 6–7mm swell of
the shell wall, a curve rather than a scoop, placed below the speaker center and sweeping upward
to nudge the wavefront toward the player. Honest note: a passive external curve gives a modest
forward bias, not a real 90° redirect. Behind a diamond lattice grille.

## Open items

Ear Y-center and diamond grille rib dimensions; exact PCB Z within the rear cavity; verify the
~1mm bottom-boss clearance against the physical NeoTrellis; confirm the OLED module's header is
on the short edge (some vendor units ship with it on the long edge, which moves the connector).
