# neuralGrid housing — shell + base plate (rev 14, 2026-09-06)

`neuralgrid_housing.blend` — `SHELL_body`, `SHELL_base` and `TRELLIS_BACKER`,
all watertight.
Rebuild with `make_shell.py` (`RUN_TO` 1–5 stops at a phase).
`NG_CUTTERS` is kept in the file as the flat reference hole set, hidden.

## Form

Flat 148 mm top carrying every straight-up hole → R15 fillet rolling 0° → 22.5°
→ **18 mm flat facet at exactly 22.5°** (the SoftPot bed) → **R14 fillet rolling
it over to 88°** → near-vertical drop to the rim.

| | Value |
|---|---|
| Top face | 148 × 148 mm, flat, z = 52 |
| Corner radius | **19.2 mm at the top, 27.4 mm at the base** |
| SoftPot facet, outer edge | r = 96.37 → 192.7 mm across |
| Base rim / base plate | **211.93 × 211.93 mm** |
| Overall height | **52 mm** (base plate 0–4, shell 4–52) |
| Rim angle | 88° — essentially straight down |
| Wall | 4.000 mm true perpendicular, uniform |
| Interior clear | **44 mm** |
| Shell volume | 293.4 cm³ |
| Base volume | 175.9 cm³ |

## Curvature pass (rev 5)

The three flat planes are untouched: the top face, the 22.5° SoftPot facet, and
the base plate. Everything else was softened.

| | rev 4 | rev 6 |
|---|---|---|
| Corner radius, top | 10.08 | **19.24** |
| Corner radius, base | 14.35 | **27.44** |
| Top roll (flat → facet) | R12 | **R15** |
| Bottom roll (facet → side) | R11 | **R14** |
| Base plate ground edge | square | **R2 round-over** |
| Pad hole corners | R0.8 | **R2.0** (matches the silicone buttons) |
| SoftPot slot ends | R1.0 | **R2.5** (near stadium) |

**Corner radius was the free win** — it costs nothing in footprint.

**Cost: +6.15 mm on the base** (204.94 → 211.09). The top face went 144 → 148 mm
to buy corner radius, and the two bigger fillets each push the profile outward.
The facet was shortened 20 → 18 mm to claw some back. Still 25 mm narrower than
the 236 mm that the slicer rejected.

## Lidars moved to the edge midpoints (rev 6)

The four ovals sit **centred on each side, long axis running along the edge** —
parallel to the SoftPot each one shares a side with. They stay vertical in the
flat top, so the sensors still look straight up. This is also what the original
handoff asked for — a hand over the centre sits in all four cones.

The 90° rotation (rev 7) turns the radial extent from 10 mm to 7 mm, which is
pure gain: the aperture now clears the Trellis block by **3.5 mm** and the edge
of the flat by **3.5 mm**, up from 2.0 mm each.

Three things got better by vacating the corners:

| | rev 5 (corners) | rev 7 (edge midpoints, rotated) |
|---|---|---|
| Corner radius, top | 15.98 | **19.24** |
| Corner radius, base | 22.80 | **27.44** |
| Lidar → Trellis block | 0.50 mm | **3.50 mm** |
| Lidar → edge of flat | 1.47 mm | **3.50 mm** |

**The SoftPots now cap the corner radius, not the lidars.** A flat adhesive
ribbon cannot bend around a corner, so the facet's straight run has to stay at
least as long as the 113 mm span:

- Facet straight run at its inner edge: **118.02 mm**
- SoftPot span needed: 113.00 mm
- **Margin: 2.51 mm at each end**

That is the binding constraint. `corner_k` above ~0.29 starts curling the ends of
the ribbons off the flat.

Caliper the assembled Trellis block before printing. If your four boards measure
over 120 mm the lidar apertures move outward, and they now have 3.5 mm of room
in both directions to absorb it.

**Why it was 236 mm and is now 205 mm.** The lower surface used to be one large
arc sweeping 22.5° → 63.6°. A single arc physically cannot gain less than ~18 mm
of radius over that drop, so the base was stuck near 236 mm — which the X1C
slicer rejected as too close to the exclusion area. Replacing it with a tight
R11 fillet plus a near-vertical run gets the same visual break off the facet for
a fraction of the footprint: **31 mm narrower on both axes, and 114 cm³ (~145 g)
less PETG across the two parts.**

**192.7 mm is the hard floor.** The SoftPot facet reaches r = 96.37, so the base
cannot be narrower than that no matter what the side does. At 211.09 the side
sits 9.2 mm proud of the facet edge per side.

## Internal headroom — measured by raycast

| Radius from centre | Clear height above the floor |
|---|---|
| 0 – 74 mm | **32.0 mm** |
| 80 mm | ~29 mm |
| 90 mm | ~25 mm |

An 85 × 58 mm Pi 5 footprint centred at the origin measures **32.0 mm at all nine
sample points**.

**Budget for the central column, which the Pi and the Trellis share:**

| | mm |
|---|---|
| Clear height (measured) | 32.0 |
| Pi 5, tallest point (USB-A stack) — **unverified** | ~18.5 |
| Trellis PCB + bottom-side components — **verify** | ~5.5 |
| Under the Pi for solder joints | 2.0 |
| **Margin** | **~6.0** |

At the earlier 34 mm height this margin was **0.0**, which is why the shell was
raised to 40 mm. The official Raspberry Pi 5 mechanical drawing carries **no
vertical dimensions at all** — the 18.5 mm is ecosystem knowledge, not sourced.
Caliper your Pi's tallest point and the Trellis stack's bottom-side depth; as
long as they total under 30 mm you are clear.

## Fasteners — locked

| Item | Spec |
|---|---|
| Screw | **M4 × 12 mm countersunk** (ISO 10642 / DIN 7991), 4 off |
| Insert | **M4 heat-set, 6.0 mm OD, 8.0 mm long** |
| Base plate clearance hole | 4.5 mm |
| Countersink | 8.4 mm at 90° included, 1.95 mm deep — head sits flush |
| Screw axis | (±88, ±88) mm |

**Verified by raycast, not by reasoning:**

- Solid PETG above every screw hole: **z = 4.000 → 16.000, a continuous 12.000 mm
  column.** No voids. The 8 mm insert uses z 4 → 12, leaving 4 mm spare.
- Material around the insert: **≥ 13.08 mm radius at every level through it**
  (13.42 at z = 4.5, 13.26 at z = 8, 13.08 at z = 12). The insert needs 3.0 mm,
  so there is ~10 mm of PETG all round. Nothing to break out.

`boss_pilot = 0.0`, so the pads are solid as specified. **5.6 mm is the pilot for
this insert** if you want one — set `boss_pilot = 5.6` and re-run.

## What carries the screws

- **Four corner pads**, 34 × 34 mm rounded, z 4 → 16, at the screw axes. Each is
  clipped to the shell envelope *before* union, so it fuses into the wall over
  its full height instead of cantilevering off it.
- **A gusset ring** at the rim (inner half-width 96 mm, 6 mm tall) stiffening a
  211 mm rim.

## Phase gate — all watertight, zero non-manifold edges

| Phase | Verts | Non-manifold | Loose | Watertight |
|---|---|---|---|---|
| 1 outer solid | 2244 | 0 | 0 | yes |
| 2 cavity tool | 2312 | 0 | 0 | yes |
| 2 shell | 4490 | 0 | 0 | yes |
| 3 gusset ring | 4886 | 0 | 0 | yes |
| 3 corner pads | 5696 | 0 | 0 | yes |
| 4 cutter tool | 5280 | 0 | 0 | yes |
| 4 shell punched | 11022 | 0 | 0 | yes |
| 5 base plate | 1258 | 0 | 0 | yes |

Failures found and fixed rather than shipped:

1. **Clipping tools against the outer skin → 1094 non-manifold edges.** The tools
   ended up sharing faces with the shell. Fixed by clipping against a surface
   held 1.0 mm inside the skin, so every union is a clean volumetric overlap.
2. **Two-step counterbore left a sliver face** (4 non-manifold edges, 1
   degenerate face). Fixed by lofting countersink and shank bore as one stepped
   solid — and the 90° cone is self-supporting where a flat counterbore ceiling
   would have had to bridge.
3. **Horizontal wall offset gave zero thickness at the flat top.** Fixed by
   offsetting the meridian along its true normal, which is why the wall measures
   4.000 mm at 0°, 22.5° and 88° alike.
4. **Screw axis 3 mm from the outer skin** would have bulged the outside when the
   brass insert went in hot. Moved inboard.

## Printing

- **Print base-rim-down.** Cross-section only shrinks going up, so the whole
  outside is self-supporting — no support ever touches the visible curve.
- Interior ceiling needs support; it pulls out through the open bottom.
- 211 mm on a 256 mm bed — comfortably clear of the exclusion area. Still two
  separate plates.
- **Warping risk: the base plate is a 211 mm flat PETG slab.** Brim it, closed chamber.
- 0.2 mm layers, 5+ perimeters, 30–40 % gyroid, slow first layer, no part cooling
  on layers 1–2.

## TRELLIS_BACKER — button backing plate (rev 8)

A flat plate the NeoTrellis sits on so button presses land on something solid,
standing on four splayed pillars that reach the inside of the base. One printed
piece.

| | Value |
|---|---|
| Plate | **122 × 122 × 4 mm**, R8 corners |
| Top face | **z = 32.000** — flush against the bottom of the Trellis |
| Pillar height | **24.000 mm** (z 28.000 → 4.000) |
| Pillar splay | centres (±52, ±52) at the plate → (±61, ±61) at the floor |
| Pillar taper | Ø10 at the plate → Ø14 at the foot |
| Overall | 136 mm across the feet, z 4 → 32 |
| Volume | 70.2 cm³ |
| **Clear under the plate** | **24.000 mm** |

Derived from your measurement: top plate underside is z = 36, the Trellis bottom
sits 4 mm below it at z = 32, so the plate's top face is z = 32 and its underside
z = 28. Pillar height is `32 − 4 − 4 = 24.000` exactly — no shim, no slop.

**Verified, not assumed:**

- Watertight, 0 non-manifold edges, 0 loose verts.
- **Zero collision with `SHELL_body`** — boolean intersect returns 0 verts, 0 mm³.
- **Zero collision with `SHELL_base`** — the feet meet z = 4.000 exactly and stop.
- Pi footprint (±42.5 × ±29) clears the nearest pillar by **24.9 mm**.

**Why 4 mm is enough plate.** Corner-supported, 122 mm square, 10 N centre press:
deflection ≈ **0.16 mm**. Imperceptible under a button. Going thicker would only
eat the Pi's headroom.

**Pi budget under the backer:**

| | mm |
|---|---|
| Clear under the plate | 24.0 |
| Pi 5, tallest point — unverified | ~18.5 |
| Under the Pi for solder joints | 2.0 |
| **Margin** | **~3.5** |

Your measured 4 mm Trellis depth was 1.5 mm better than the 5.5 mm I had assumed,
which is most of what paid for the backer.

**Wiring.** No holes needed in the plate: the enclosure interior is ~190 mm across
at that height against the plate's 122 mm, so there is a ~34 mm gap all the way
round for wires to drop through.

**Printing.** Plate down, pillars up. They lean outward going up at 20.6° from
vertical, so nothing overhangs and no supports are needed.

**Two things to check before printing it:**

1. If the Trellis has bottom-side components hanging below the PCB (the seesaw
   chip, connectors), a flat plate at z = 32 lands on those first, not the board.
   Say the word and I will cut relief pockets once you have their positions.
2. The backer is located vertically by exact pillar height, but **not laterally** —
   at plate height the wall is 34 mm away on every side, so nothing registers it.
   If you want positive location, the fix is shallow pockets in the base plate
   for the four feet.

## Back panel connectors (rev 9)

Three cutouts on the **+Y wall**, in the flat 88° band. Positions are the quarter
points of the straight run, as asked.

| | Drawing | x | Cutout |
|---|---|---|---|
| Ethernet M23 | 1 | **−52.74** | **Ø23.6 round** — CONFIRMED |
| Power | 3 | **0.00** | **Ø15.8 round** — CONFIRMED |
| USB | 2 | **+52.74** | **Ø22.8 round** — CONFIRMED |

All three now confirmed against the real parts on the test plate. The Ethernet
landed **two steps past the ladder's top rung** — which confirms the drawing's
"22,00" was never the panel cutout. It is an M23×1.5 thread, so Ø23.6 is exactly
where a panel hole for it belongs.

**Spacing fix (rev 11).** These were at ±39.03 — the quarter points of the *flat
run*, not of the shell. That bunched all three into the middle ~55% of the back
with dead space either side. Quarters of the shell put them at **±52.74**, which
is what "equally distributed across the shell" actually means. Measured edge
gaps: **33.58 and 33.30 mm** (0.28 apart, invisible), with 14.14 / 13.86 mm of
flat left beyond the outer holes before the corner radius starts.

Left/right as you face the back from outside, so Ethernet at −X. All three are
nominal + 0.4 mm clearance.

**All three are round.** A flat makes a hole smaller on one side, so an
unnecessary flat blocks a round part instead of merely failing to key it —
Ø22.4 with a 1 mm flat is only 21.4 across, and a round Ø22 body would not enter.
The Ethernet drawing's 22,00 vs 21,99 is 0.01 mm apart, i.e. round; the USB gives
no flat; only the power connector has a real one (14.6 across a 15.6 circle), and
that stays round here until the test plate confirms which way its key points.
`pwr_flat = 14.6` turns it on.

**Verified by raycast through the finished shell:** Ethernet 22.36 mm, power
15.96 mm, USB 22.92 mm (0.04 is ray sampling).

**The seat is genuinely flat.** Sampled the surface normal at six points around
the Ethernet hole — all six return (0, 0.9994, 0.0349), identical. Along a
straight edge of the rounded square the wall is a true plane, tilted 2° from
vertical, and the holes are cut normal to it. No bay or boss needed.

### Why the shell grew 40 → 52 mm

The flat band on the back wall is `z_top − 24.476` — the rim, the R14 bottom
roll, the facet drop and the top roll all subtract from it. At z_top = 40 that
left **15.52 mm**, and the Ethernet needs Ø22.4 plus a seat ring. At z_top = 52
it is **27.52 mm**, giving 2.56 mm of flat above and below the hole.

Both fillets stayed at R15/R14 so the curves did not get tighter, and the base
only moved 211.09 → 211.93. Pushing the SoftPot up buys nothing: the facet
already starts 1.14 mm below the top face.

Knock-on: interior clear is now 44 mm, so the backer's pillars grew to **36.000 mm**
and there is **36 mm** under the backer for the Pi — up from 24.

### Full verification, rev 11

Everything below measured out of the finished meshes by raycast, not computed.

| Check | Result |
|---|---|
| All 9 build phases | watertight, **0** non-manifold edges, **0** loose verts |
| Wall thickness, perpendicular | **4.000** at 2°, 21.1°, 53.9°, 67.5° |
| Top plate | **4.000** (52.000 → 48.000) |
| Pad opening | **12.58** on every one of 16 sampled |
| Pad pitch X and Y | **15.000**, single unique value, no drift |
| Pad row span | **105.000** = 7 × 15 exactly |
| Ethernet hole | 22.36 (Ø22.4), z 6.58–28.80 |
| Power hole | 15.96 (Ø16.0), z 9.78–25.60 |
| USB hole | 22.92 (Ø22.95), z 6.30–29.08 |
| Flat band | 4.000 → 31.524, all three holes inside |
| Connector seat normals | **one unique value** (0, 0.9994, 0.0349) across 8 samples on both big holes — truly planar |
| Trellis gap | **4.000** (48.000 ceiling → 44.000 backer top) |
| Clear under backer | **36.000** |
| Pillar feet | all four land at **4.000**, exactly on the base's inner face |
| Insert columns | **z 4.000 → 16.000, a solid 12.000 mm**, all four |
| Backer ∩ shell | **0 verts** |
| Backer ∩ base | **0 verts** |

The one number that is not verifiable here is the Pi 5's tallest component — the
official mechanical drawing carries no vertical dimensions. With 36 mm under the
backer that no longer matters.

### Cutters are kept

Every cutter actually subtracted now lives in **`NG_CUT_USED`** (75 objects: 64
pads, 4 lidars, 4 SoftPot slots, 3 connectors), hidden but never deleted. The
boolean runs on throwaway copies.

### One bug caught here

The connector cutters were first built on basis `(Yv, d, n)`, determinant −1 —
left-handed, so the prisms inverted and the boolean silently did nothing. Spotted
because the joined cutter tool measured 94,191 mm³ when the pad holes alone are
121,900. Fixed to `(Yv, −d, n)`, which is right-handed and also puts the flat at
the top of each hole. Tool now measures 156,490 mm³.

## Confirmed from the test plate

| Feature | Value | Source |
|---|---|---|
| Power hole | **Ø15.8** round | rung 2 |
| SoftPot tail slot | **13.4 × 6.4** | rung 2 |
| Ethernet hole | **Ø23.6** round | rung 7 + 2 steps |
| USB hole | **Ø22.8** round | rung 5 |

Measured out of the finished shell: Ethernet **23.58**, power **15.78**, USB
**22.78**. The Ethernet spans z 5.97 → 29.41 inside the 4.0 → 31.52 flat band,
leaving **1.97 mm** of seat below and **2.11 mm** above.

Clearance is stored per connector (`eth_clear` / `usb_clear` / `pwr_clear`) and
per slot (`slot_clear`), so a confirmed number cannot drift when a provisional
one is set.

Widening the slot slides the tail hole inboard automatically — `H = span/2 −
slot_wid/2` — so the far edge of the slot stays exactly on the 113 mm span.
Measured out of the finished shell: slot **6.38 along the edge, 13.38 down the
slope**, far edge at **56.49**, span **112.98**. Power hole **15.78**.
No flat on any of them; the nuts do the anti-rotation.

## Intake vents (rev 13)

Uniform vertical slots on the **left, right and front** walls. Back stays solid —
the connector caps eat its flat run.

| | Value |
|---|---|
| Slot | 3.0 × 12.0 mm, stadium ends |
| Position | the eight pad-column centrelines: ±7.5, ±22.5, ±37.5, ±52.5 |
| Centre height | z 20 |
| Total open area | **817.6 mm²** across 3 walls |

Measured out of the finished shell: width **2.99**, span **z 14.04 → 25.85**,
centres landing on the pad columns to within 0.005 mm.

**Vertical, not horizontal** — a long horizontal slot forces a ~120 mm bridge on
the layer above it. Vertical slots bridge only their own 3 mm width, so no
supports touch a visible face.

**z 20 is bracketed at both ends**: the gusset ring occupies z 4→10 inboard of
the wall, so anything lower cuts into solid material instead of the cavity; the
R14 fillet starts at z 31.52, so anything higher is off the flat. Clearances came
out 4.04 mm below and 5.67 mm above.

**Sized to the outlet, not to the walls.** Flow through a chimney is set by the
smaller of inlet and outlet, and the outlet is the four lidar apertures at
~220 mm². 817 mm² of intake is ~3.7× that — enough headroom to never be the
restriction, without turning the sides into a grille that competes with the
64-hole top. The Pi runs a fan and heatsink, so the enclosure only has to
exchange air, not cool the SoC.

## Still open

- ESP32 USB port cut-out and strain relief.
- VL53L1X mounts (13 × 18 mm board, under each edge-midpoint aperture, outboard
  of the 120 mm Trellis block).
- Rubber feet recesses; internal standoffs for the Pi 5 and ESP32.
- Locating pockets in the base plate for the backer's feet, if wanted.

Author: emgor. Additive-only: nothing existing was moved, renamed, or deleted.
