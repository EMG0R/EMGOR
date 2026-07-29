---
id: emgor.hardware.quadro-punch-packer
title: Quadro Punch Packer
blurb: 10" printed hemisphere speaker — 4 coaxials at the cube-vertex angle plus a sub
parent: emgor.hardware
source: ______2026NEW/Quadro-Punch-Packer/
downloads:
  - files/quadro-punch-packer-build.md
  - files/quadro-punch-packer-circuit.png
tags: [speaker, quad, 3d-print, amplifier, battery]
updated: 2026-05-08
draft: false
---

# Quadro Punch Packer

A 10" printed hemisphere speaker with 360° coverage. Four 3.5" coaxial two-way drivers around
the dome plus a 4" sub in the base firing down into the floor. Quad input from two 1/8" jacks,
or stereo merge on a 4PDT toggle. Bluetooth always on and always mixed in. Battery powered,
charges from any USB-C PD brick.

**Status: physically built, in debug.** Enclosure geometry is final and print-ready; the
electronics are wired and partially working. See the honest list at the bottom.

## Geometry — CCRMA placement math

Bambu X1C build volume is 256mm, so the hemisphere is 254mm outer diameter with 8mm solid walls,
119mm inner radius, 3.53L internal volume, fitting with 1mm of margin per side.

The four coaxials sit at **θ = 54.7° from the apex — arctan(√2), the cube-vertex angle** — the
mathematically optimal polar angle for four equally spaced drivers on a hemisphere, maximizing
solid-angle coverage uniformity. That puts each driver axis 35.3° above horizontal, 68.7mm above
the equator and 97.1mm out from the center axis, with 146.5mm of great-circle spacing between
neighbors. An 89mm coaxial frame leaves a 57.5mm gap — plenty for 8mm walls and ribbing.

Each driver position needs a flat mounting pad printed into the dome, compensating 6.6mm of
spherical sag across a 78mm cutout. In the final model those pads are exponential flares rather
than flat frustums: `r(t) = 45 + 27·(e^(2t/T) − 1)/(e² − 1)`, thirty stacked segments from Ø90mm
at the base to Ø144mm at the top, blending organically into the hull with no visible step rings.

A horizontal cap seals a 1.29L chamber for the sub; the coaxials share the 2.24L above it as an
infinite baffle.

## Signal chain

```
jacks ─┬─ [4PDT toggle: quad / stereo] ─┐
       │                                 ├─► [JFET drain nodes ×4] ─► amps ─► drivers
BT ────┴─ AD828 preamp ─ 8.2kΩ ─────────┘   (BT bypasses the switch entirely)
```

Bluetooth stays stereo in both modes and plugging in a jack adds signal on top rather than
cutting it. The MH-M18 outputs only 200–400mV, so an AD828 preamp brings it to the ~1V the
TPA3116 needs for full power.

Master volume is four J201 JFETs used as voltage-controlled resistors, one per channel, all four
gates driven from a single 10kΩ pot wiper through 1MΩ resistors. One knob, four channels
attenuating together, channels staying electrically separate. The sub sums off the four
post-JFET nodes, so it tracks the master volume for free. Built dead-bug — components floating,
held by their own solder joints, hot glue over the assembly once tested.

Crossovers: the sub amp has a built-in adjustable low-pass, set to ~400Hz. The coaxials get a
passive first-order high-pass — and rather than buying bipolar caps, two 220µF polarized caps
per driver are wired negative-to-negative for a ~110µF non-polarized equivalent. That lands the
corner at 363Hz instead of 400Hz, which is inaudible. Eight caps, already in the kit, zero
purchase.

## Power

3S Samsung 30Q pack (11.1V, 33.3Wh) behind a BMS. 3S is the sweet spot: 2S gives ~6W/channel and
wastes the drivers, 4S gives ~25W and exceeds what small coaxials are comfortable with, 3S gives
~14W/channel matching the 30W RMS rating with Class D efficiency at its best.

Charging is a USB-C PD trigger board pulling 15V from any PD brick into a **DPS3003 digital
CC/CV module** set to 12.6V / 1.5A, through a 1N5822 Schottky into the BMS. The DPS3003 replaced
an XL4015 for one blunt reason: the XL4015 uses blind trim pots with no display, its wipers get
damaged from repeated adjustment, and one bad session without a multimeter renders it unusable.
The DPS3003 shows exact voltage and current and sets with buttons. Worth the extra $5.

| Usage | Draw | Runtime |
|---|---|---|
| Background / conversation | 5–7W | 4.5–6 hrs |
| Music at room level | 10–12W | 2.5–3.5 hrs |
| Loud | 18–22W | 1.5–2 hrs |

Roughly 92–98dB in a room — about 7–10dB louder than a JBL Charge 5, which reads as twice as
loud — with genuinely 360° coverage and a real sub instead of passive radiators. ~2.6kg, mostly
the 8mm solid walls. Not waterproof, no app. Parts actually used: about $200.

## Print

Two pieces, split at z = 12mm. `QPP_v7_1_floor` prints upright, `QPP_v7_1_shell` prints
inverted. Both watertight — zero non-manifold edges, zero boundary edges — in PETG at 100%
infill, 30–50 hours total.

The geometry rebuild taught three things worth writing down. Unioning all four coax pads at once
before the shell union collapses the geometry into a single octant; do one at a time with
`transform_apply` between. Building a hollow shell first and carving pads into it produced 762
non-manifold and 477 boundary edges; the solid-first pipeline (solid hemisphere → carve cavity →
add features) produced zero. And a centering ring at r = 119.5mm interfered with the dome inner
wall by 0.07mm — trimmed to r = 119.0 for 0.43mm clearance, comfortably above PETG tolerance.

## Where it actually is

Built and working: battery pack, BMS, power bus, all three amp boards powered and decoupled, 5V
rail, Bluetooth paired, JFET volume stage soldered, drivers wired with crossovers, 4PDT switch
installed, full signal chain run. Panning confirmed across two speakers, which proves the jack
grounding is right.

Still broken:

- **AMS1117 damaged** — outputting 8V instead of 5V after overheating. Replace first; it's
  upstream of everything else on this list.
- **Bluetooth module dead** — also overheated. (Lesson already learned once: an AMS1117 linear
  regulator drops 6V as heat at this input voltage, dies on any short, and has no real
  overcurrent protection. Five AMS1117s and two BT modules died before switching the BT rail to
  an MP1584EN switching buck.)
- **Volume pot not sweeping** — either the left leg isn't grounded or the bad AMS1117 is feeding
  it the wrong range. Fix the regulator, then retest.
- **Two speakers always on regardless of input** — 4PDT wiring suspect. Diagnose by unplugging
  the jack entirely and checking whether they go silent.
- **J201s possibly cooked** — they saw 8V on their gates from the failed regulator. Two are
  already out and need replacing from the 20-pack.

Enclosure open items: a stepped driver cut (currently Ø75×79 at full depth with zero clearance
on the screw axis), mechanical attachment between floor and dome beyond the slip-fit ring, and a
wire routing channel between the halves.
