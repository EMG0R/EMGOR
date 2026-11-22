# neuralGrid housing — boolean cutter set (rev 2, 2026-09-03)

`neuralgrid_cutters.blend` — 72 closed manifold prisms, **12 mm tall**
(Z −6 .. +6), origin-centred. Boolean-DIFFERENCE out of the top plate.
Scene: Metric / unit scale 0.001 / millimetres.

## Collections

| Collection | Count | Opening |
|---|---|---|
| `CUT_pads` | 64 | 12.6 × 12.6 mm square, 0.8 mm corner fillet |
| `CUT_softpot_slots` | 4 | 13 × 6 mm slot, 1.0 mm fillet, **13 mm runs radially** |
| `CUT_lidar_ovals` | 4 | 7 × 10 mm ellipse, alternating 0°/90° around the ring |

`NG_CUT_PARAMS` (empty) carries every dimension as a custom property.
`make_cutters.py` regenerates the whole set from those numbers.

## Symmetry

Everything is **C4 rotational** — rotate the plate 90° about Z and the cutout
pattern maps exactly onto itself. That is the most symmetry available given each
ribbon has its tail at one end only; a mirror-symmetric layout is impossible
without a second dummy hole per edge.

- **Softpot slots** sit at the *end* of each 113 mm span, and parallel pairs are
  on opposite ends: N at x = +53.5, S at x = −53.5; W at y = +53.5, E at y = −53.5.
- **Lidar ovals** alternate 0° / 90° / 0° / 90° going NE → NW → SW → SE. That
  alternation *is* the C4 symmetry — a 90° rotation carries each oval onto the next.

## Verified numbers

- Pad pitch 15.000 mm both axes, zero drift across all 8 columns/rows.
  Centres at ±7.5, ±22.5, ±37.5, ±52.5. Grid outer extent 117.6 × 117.6 mm
  (1.2 mm margin inside the 120 mm tiled Trellis block).
- **Web between pad holes: 2.4 mm** — that is the bezel wall.
- Softpot span: slot far edge at 56.5 mm, body far edge at −56.5 mm →
  **113.0 mm, centred on the edge**. Softpot body length implied: **107.0 mm**
  (56.5 back to 50.5). Check that against your part.
- Softpot ring offset 70 mm (centre → ribbon centreline). Slot rotated 90° from
  rev 1: 13 mm now runs perpendicular to the edge, 6 mm along it.
- Lidar ring offset **67 mm** (was 70). Aperture inner edges clear the 120 mm pad
  block by 3.5 mm on the short axis, 2.0 mm on the long axis. **Do not go below
  ~66 mm** or the aperture fouls the block corner.
- Cutter depth **12 mm** (Z −6 .. +6). Clears a 4 mm top plate with 4 mm spare on
  each face — enough that no boolean ever sees a co-planar surface, without the
  cutters towering over the model in the viewport. If the plate ever exceeds
  ~10 mm, raise `cut_depth`.

## PETG wall thickness — recommended

PETG's modulus is ~2.0 GPa vs PLA's ~3.5, so it deflects noticeably more at the
same thickness. For a plate people lean on:

| Feature | Thickness | Why |
|---|---|---|
| Top plate | **4.0 mm** | 3 mm PETG visibly flexes over a ~120 mm unsupported span perforated by 64 holes |
| Side walls | **3.0 mm** | 3.0 mm = 7 perimeters at 0.4 mm nozzle, solid through |
| Floor | **3.0 mm** | |
| Bezel walls between pads | 2.4 mm (as cut) | already generous; also kills LED bleed |
| Ribs under the top plate | 2.0 mm × 8 mm tall | run them between pad rows if you want to stay at 3 mm plate |

Slicer: 0.2 mm layers, **5+ perimeters** (perimeters carry the load, not infill),
30–40 % gyroid, 4 top / 4 bottom. PETG wants a slow first layer and no part
cooling on layer 1 or interlayer bonding suffers — that is what actually breaks
under vibration, not wall count.

## Assumptions to caliper

1. Silicone button 12.0 mm square → hole 12.6 mm (0.3 mm/side). If yours differ,
   set `pad_open` = measured + 0.6.
2. Pitch 15.0 mm (self-consistent with the 120 mm tiled block; some sources give
   15.24 mm from a 60.96 mm PCB — measure across all 8 and divide).
3. The 113 mm span is taken as slot-far-edge → softpot-far-edge along the ribbon
   axis, with the slot rotated 90°. If you measured it with the slot the other
   way round, `SLOT_AXIAL` in the script becomes `slot_len` and the offset moves
   to 50.0 mm.

## SoftPot cross-check against the Spectra Symbol datasheet

Checked the 113 mm number against the real part rather than trusting it:

| Spec (100 mm active SoftPot) | Datasheet | Our model |
|---|---|---|
| Overall length P | **115.86 mm** | 113 mm span — 2.9 mm short of P, but see below |
| Body (P − tail) | 115.86 − 12.70 = **103.16 mm** | 107.0 mm of room → **3.84 mm slack** ✓ |
| Tail length T | **12.70 mm** | slot's 13 mm dimension ✓ |
| Tail width | **7.11 mm** | passes through the slot's 13 mm dimension, 5.89 mm margin ✓ |
| Active width | **10.16 mm** (NOT 20 mm) | affects the surface channel, not these cutters |

Verdict: the 113 mm holds. The reason it is *less* than the 115.86 mm overall is
that the tail bends down into the slot instead of lying flat past it, so the tail
length is spent vertically, not along the edge. The body gets 107 mm of run for a
103.16 mm part.

**Two things that would break it:**
1. If your SoftPots are the **200 mm active** version, overall is 215.86 mm and
   113 mm is nowhere near enough — set `span_total` to ~213 and everything else
   follows.
2. The handoff guessed the ribbon is 20 mm wide. It is **10.16 mm** active
   (~12.7 mm including the border). Any surface channel or recess you cut for the
   ribbon body should be sized off 12.7 mm, not 20 mm.

Source: Spectra Symbol SoftPot datasheet rev F3.

## Boolean notes

- Join a **duplicate** of `CUT_pads` (Ctrl+J) and run one boolean — 64 modifiers
  is slow and fragile.
- Nothing here is unioned to a shell; cutters only.

Author: emgor. Additive-only: nothing existing was moved, renamed, or deleted.
