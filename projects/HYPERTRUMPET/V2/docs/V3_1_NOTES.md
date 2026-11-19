# HYPERTRUMPET V3.1 (2026-11-15): EMGOR's measurements applied

Only the shell changed. Mount, spiral, mic, rail, caps, drawer are v3 copies in `cad/stl_v3_1/` (spiral untouched, LED length still pending). Script: `cad/hypertrumpet_v3_1_build.py` (named variables at the top: `FADER_OUTER_SPAN`, `FADER_WH`, `KNOB_LEN`, `LEVER_W`, `LIDAR_WH`, `LIDAR_FAR_EDGE_FROM_KNOB`, `GROUP_SHIFT`, `SEED_ZONE`, `JOY_DEPTH`). Check numbers: `cad/v3_1_spec_check.json`, `cad/verify_report_v3_1.json`. Renders: `cad/renders_v3_1/` (annotated face, flat-on, iso x2, left-hand).

## Faders
- Outer long edge to outer long edge = 26.0 mm, 3 slots evenly spaced, slot 4.1 mm wide: pitch = (26.0 - 4.1) / 2 = **10.95 mm**, web between slots 6.85 mm. Slot axes x = 841.75 / 852.70 / 863.65, centre z 23.3 (v1 values for x of the middle slot and z). Measured on the STL: span 26.0, pitch 11.0 / 10.8 (0.4 mm ray-scan step).
- Slot size kept at v1's 4.1 x 38.4. Sanity check [G, no datasheet retrieved]: a "45 mm" uxcell B10K slide pot is a 45 mm long body with roughly a 30 mm travel and a ~3 mm lever, so 4.1 wide leaves ~0.5 mm each side and 38.4 gives ~4 mm of end margin beyond travel + lever. If your part is a 45 mm *travel* pot, the slot must be about 49 mm: tell me.
- Knob width matters: pitch 10.95 leaves only 10.95 - knob width between knobs (assumed 8 mm knob => 3 mm gap).

## Lidar
- Stadium 11 x 8 mm (straight sides 3 mm, R4 ends), long axis horizontal (x), centred on the middle fader axis x = 852.7 (the original lidar was at x 853.1, unrotated, above the faders between faders and keys, so it stays above).
- **Interpretation to confirm:** knob lower face with the middle fader at the top of its travel -> far (upper) edge of the lidar = 25.0 mm, measured up along the fader axis. Assumed knob length 10 mm (`KNOB_LEN`, [A]) and lever 3 mm: lever at the top end of the slot -> knob lower face z = 36.0 -> lidar upper edge z = 61.0, lidar centre z = 57.0 (spans 53.0-61.0; the slot top is z 42.5, so 10.5 mm clear above the slot).
- **Conflict:** the lowest Trellis opening starts at z = 62.05, leaving a **1.05 mm web** between the lidar and the key row. Web = KNOB_LEN/2 - 3.95: a 14 mm knob gives 3.05 mm. Options: (a) tell me the real knob length, (b) accept the 1 mm web, (c) shift the whole fader/lidar block 2 mm down (faders move relative to the keys), (d) measure from the knob's top.

## Pots, encoders, joystick
Same hole sizes (8.7 x 8.3 ellipses, 29.4 x 28.0 joystick) and same relative arrangement, whole group moved +4.0 mm toward the Trellis (x +4). Gap from the nearest pot hole edge to the nearest key opening edge: 10 mm; joystick hole to nearest pot hole: 10.9 mm; all webs >= 3 mm. Hole centres verified within 0.11 mm.

## Trellis
Unchanged: 11.0 x 11.1 openings, pitch 15.05, same x/z (locked).

## Cavity
- Seed zone (Daisy Seed 3 placed vertically at x 826-850, z 62-118, clear of the rail strip that starts at x 857): clear depth behind the skin >= 22 mm (20 + 2). Measured min 22.0 mm.
- Joystick zone: stick 17 above PCB, cap 8 proud => PCB top 6.8 behind the panel back; PCB 1.6; header 2.5 + 6 mm tails => 16.9 mm, +2 => 18.9 mm required; measured min 18.93 mm. (A plugged Dupont connector would add ~14 mm; solder wires instead or tell me.)
- How it grows: the face plane is locked in x/z, so the face swells outward smoothly (smooth-max of the original plane and the depth requirement, cosine falloff): maximum +6.7 mm in y relative to v3 at the Trellis zone. The Trellis keys are therefore up to 6.7 mm further from the horn than in v3 (x/z unchanged). Outer form stays one flowing blend.

## Verification
- 17 STLs, all watertight, one body each, within the bed. Shell 101 cm3, 153 x 93 x 204 mm.
- Control table vs spec: all 25 centres within 0.11 mm, sizes within 0.4 mm (fader 3.8 x 38.0, lidar 10.6 x 7.6 on the scan; edge rounding). Horn clearance: shell 22.2 mm to caps/casings, 26.4 mm to slide tubes, 44 mm to the ring, 50 mm to the thumb hook.
- Assembly intersections: sliver-level only (0.02, 0.64, 1.9 mm3), as v3.
