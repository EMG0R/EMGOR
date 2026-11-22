# HYPERTRUMPET V3 build notes (2026-11-15)

Rebuild after "controls are in the wrong places". Every control is now placed from the ORIGINAL `hyperFRday2.blend` (read-only copy in `cad/v1_reference/`); the supports are the ORIGINAL v1 mount. v2 files are untouched. Source of truth: `cad/hypertrumpet_v3_build.py` (names at the top: `ORIGINAL_CONTROLS`, `FADER_POS`, `LIDAR_POS`, `P`), `cad/hypertrumpet_v3_blend.py`. Re-run: `python hypertrumpet_v3_build.py all` (about 40 s with the v1 voxel cache), then Blender `-b --factory-startup --python hypertrumpet_v3_blend.py -- cad/hypertrumpet_v3.blend cad/renders_v3`. Tags: [M] measured on the file, [A] assumed, [G] general knowledge.

## 1. What was extracted from the original blend [M]
- `_holes.001` (25 cutter bodies) and `attemp3COMPshowHOLE.001` (hole loops sliced at several y levels; both agree). The final comp is a tilted 12 mm plate; its face is the plane y = 533.29 + 0.03507 x - 0.03660 z (least squares over 15 loops, max residual 0.56 mm). The cutter bodies are the controls; `_sexBACKUP` is the old peg/cup joint (dropped), `smothingBlock` is a 12-face helper box, `trellis2_lid` is the 70 x 70 x 4 mm lid (a 4 x 4 key grid plate, parked far from the horn).
- No FSR, IMU or encoder-body openings exist in the original comp. The only openings are the ones in the table. (IMU and FSR were internal in v1; v3 adds an FSR recess in the palm lobe, see below.)
- Important correction to v2: the pots/encoders and the joystick are on the LEFT (low-x) side of the Trellis in the original, not on the right; the faders are vertical slots 24 mm apart below the keys; there is one lidar window between the faders and the keys.

## 2. Control table: original vs v3 (diff measured on the built shell STL by ray scan through the front skin)

| Control | Original centre (x, z) | Original size (x, z) | v3 centre | Centre diff mm | v3 size |
|---|---|---|---|---|---|
| trellis_00 | (830.6, 67.6) | 11.0 x 11.1 | (830.70, 67.65) | 0.11 | 10.8 x 10.8 |
| trellis_01 | (830.6, 82.7) | 11.0 x 11.1 | (830.60, 82.75) | 0.05 | 10.6 x 10.8 |
| trellis_02 | (830.6, 97.7) | 11.0 x 11.1 | (830.70, 97.75) | 0.11 | 10.8 x 10.8 |
| trellis_03 | (830.6, 112.8) | 11.0 x 11.1 | (830.60, 112.85) | 0.05 | 10.6 x 10.8 |
| trellis_10 | (845.6, 67.6) | 11.0 x 11.1 | (845.70, 67.55) | 0.11 | 10.8 x 11.0 |
| trellis_11 | (845.6, 82.7) | 11.0 x 11.1 | (845.70, 82.65) | 0.11 | 10.8 x 11.0 |
| trellis_12 | (845.6, 97.7) | 11.0 x 11.1 | (845.70, 97.65) | 0.11 | 10.8 x 11.0 |
| trellis_13 | (845.6, 112.8) | 11.0 x 11.1 | (845.60, 112.85) | 0.05 | 11.0 x 10.8 |
| trellis_20 | (860.7, 67.6) | 11.0 x 11.1 | (860.60, 67.65) | 0.11 | 10.8 x 10.8 |
| trellis_21 | (860.7, 82.7) | 11.0 x 11.1 | (860.60, 82.75) | 0.11 | 10.8 x 10.8 |
| trellis_22 | (860.7, 97.7) | 11.0 x 11.1 | (860.70, 97.75) | 0.05 | 11.0 x 10.8 |
| trellis_23 | (860.7, 112.8) | 11.0 x 11.1 | (860.80, 112.85) | 0.11 | 10.8 x 10.8 |
| trellis_30 | (875.7, 67.6) | 11.0 x 11.1 | (875.80, 67.55) | 0.11 | 10.8 x 11.0 |
| trellis_31 | (875.7, 82.7) | 11.0 x 11.1 | (875.80, 82.65) | 0.11 | 10.8 x 11.0 |
| trellis_32 | (875.7, 97.7) | 11.0 x 11.1 | (875.70, 97.65) | 0.05 | 10.6 x 11.0 |
| trellis_33 | (875.7, 112.8) | 11.0 x 11.1 | (875.60, 112.85) | 0.11 | 10.8 x 10.8 |
| fader_0 | (828.5, 23.3) | 4.1 x 38.4 | (828.55, 23.30) | 0.05 | 3.8 x 38.0 |
| fader_1 | (852.7, 23.3) | 4.1 x 38.4 | (852.75, 23.30) | 0.05 | 3.8 x 38.0 |
| fader_2 | (876.9, 23.4) | 4.1 x 38.4 | (876.85, 23.40) | 0.05 | 4.0 x 38.0 |
| lidar | (853.1, 52.9) | 6.4 x 4.0 | (853.20, 52.80) | 0.14 | 6.2 x 3.8 |
| round_0 | (749.7, 46.8) | 8.7 x 8.3 | (749.75, 46.85) | 0.07 | 8.4 x 8.0 |
| round_1 | (749.7, 96.5) | 8.7 x 8.3 | (749.75, 96.55) | 0.07 | 8.4 x 8.0 |
| round_2 | (806.7, 56.3) | 8.7 x 8.3 | (806.75, 56.35) | 0.07 | 8.4 x 8.0 |
| round_3 | (806.7, 90.0) | 8.7 x 8.3 | (806.65, 90.05) | 0.07 | 8.6 x 8.0 |
| joystick | (776.7, 72.3) | 29.4 x 28.0 | (776.70, 72.30) | 0.00 | 29.0 x 27.6 |

Max centre difference 0.14 mm; sizes are 0.0-0.4 mm smaller (edge rounding and mesh smoothing). Notes: the 4 round holes are 8.7 x 8.3 ellipses in the original (tilted cylinders), kept as ellipses; which two are pots and which two are encoders is not in the file. Joystick = the 29.4 x 28.0 hole at (776.7, 72.3). Trellis key pitch is 15.05 mm; pad openings 11.0 x 11.1.
**Named variables for the incoming measurements:** `FADER_POS`, `FADER_WH`, `LIDAR_POS`, `LIDAR_WH` (all in `hypertrumpet_v3_build.py`); changing them and re-running `shell` moves the openings.
Face depth: the original face plane is used exactly, plus a 1.2 mm crown (max) in the middle of the pod; y therefore differs from the original by up to 1.2 mm, x/z by <= 0.14 mm.

## 3. Supports: the ORIGINAL v1 mount, with these changes only
Main body of `_mount_v3.stl` (the 7 floating lumps, which are the old cups, are dropped). Everything v1 had is kept, including the top-right blade that v2 deleted. Changes, all listed (v1 main body 268.2 cm3 -> v3 278.4 cm3):
1. Cut: the analytic standard-dimension horn (casing 18.0, caps 21, slide U-legs 12.6, ring, thumb hook, brace, leadpipe) plus 0.4 mm clearance, only where the original is inside that envelope (volume removed, mm3):

| Removed mm3 | Centre | Bounds | Cause (nearest horn item) |
|---|---|---|---|
| 1343 | (775, 503, 25) | x 751-800, y 497-507, z 24-28 | slide3_leg0 |
| 863 | (759, 502, 40) | x 747-791, y 496-507, z 36-43 | slide3_crown |
| 435 | (891, 513, 27) | x 888-895, y 506-516, z 22-35 | slide1_leg0 |
| 407 | (891, 513, 47) | x 888-894, y 510-516, z 40-54 | slide1_leg1 |
| 290 | (832, 519, 54) | x 828-836, y 518-519, z 24-80 | casing1 |
| 140 | (853, 519, 56) | x 850-856, y 518-519, z 33-82 | casing2 |
| 87 | (907, 495, 122) | x 904-910, y 492-497, z 120-124 | saddle_hook |
| 37 | (816, 501, 22) | x 812-821, y 496-504, z 22-22 | slide3_leg0 |
| 15 | (874, 519, 38) | x 873-877, y 519-519, z 34-46 | casing3 |

 Reading: v1 was fitted to 10.2 mm slide tubes; a real 12.6 mm tube needs the lower-left blades and the right blade trimmed by 0.3-1.3 mm; the thumb-hook interference (0.7 mm in v1) is cut away with 73 mm3 off the top-right blade tip, which keeps its length and thickness.
2. Added: a dovetail rail boss on the plate front (neck 14, head 22, height 6, z 8-100, 14.4 cm3) that the shell slides onto. Nothing else is added.
3. Not done: no TPU pads (supports kept exact), no new fillets (v1 geometry as is). The surface ripple in the renders is the v1 mesh plus 0.5 mm voxel/Taubin processing; original ripples were there too.
4. Slide access: the cut leaves 0.54 mm clearance to the slide legs, 0.71 mm to casings and caps, 0.67 mm to the thumb hook, 11.2 mm to the finger ring, 3.3 mm to the leadpipe. The original lower-left blades still sit in the 3rd-slide leg plane (that is the original geometry you asked to keep); they hug the legs and may limit how far the 3rd slide can be pulled. I did not remove them.

## 4. Shell (pod + palm support, one flowing form) [`shell_flowing_pod_palm.stl`, 100 cm3]
- Built from a smooth union (polynomial smooth-min, k = 16-22) of rounded blocks and capsules wrapped around the controls: Trellis block, fader bank, joystick disc, two pot/encoder columns, lidar bridge, and a palm lobe, rounded R8 on the edges, 2.2 mm skin (2.6 mm in the lobe). No box primitives, no flat slabs.
- Open-backed cup: it slides onto the rail boss with an open back so PCBs can be fitted from behind; a rail strip with a dovetail channel (0.8 mm clearance per side) closes the back. [A: latch tab not modelled in v3; add the v2 flex tab if the rail is loose.]
- Palm lobe: flows out of the shell BELOW the faders (top of the lobe is z 2, the fader slots start at z 4) and sweeps toward +y, i.e. toward the player's left side in the model frame [A: I read "left" as the lateral +y direction, tilted slightly down: axis (853, 549, -29) -> (858, 600, -40), radius 31 -> 35]. Say if "left" meant the other way (flip the sign of `lobe_p1[1]` and the drawer direction).
- Cells: sliding drawer cartridge as before (47 x 80 x 24 mm, keyed 2-cell), bay open at the dome end of the lobe; FSR402 recess (1.2 mm deep) on the upper-lateral lobe surface with a 3.4 mm tail hole into the bay [A: placement is a first guess].
- Where the controls are relative to the hand: the lobe top is 3 mm below the fader slots, so a hand resting on the lobe puts the fingers at the faders (z 4-43) and the Trellis keys (z 62-118) without leaving the lobe.
- Optional flowing wrap-arms around the bottom valve area: not built (they would add contact near the bottom caps); the shell does not touch the bottom caps (nearest 22 mm).
- Electronics: Seed, BMS, boost, TX and PCBs have no seats modelled in v3; the interior cavity behind the face is 10-17 mm deep (face plane to rail strip). [A] TX bays and PCB posts are not drawn.

## 5. Spiral band (two printed parts, one flowing ribbon)
- One continuous ribbon from the shell's right side (plugs into a slot at (892, 553.5, 52), between the faders and the Trellis) over a 72 mm bridge, then onto the bell at s = 215 mm from the rim (bell tail near the valve block, x ~ 935) and up the whole bell to s = 8 mm at the rim. Total rotation 220 deg = 0.61 turn (limit 1.0), ribbon 12 mm wide, 1.5 mm gap to the bell surface, U channel 7.0 x 6.4 mm with groove for a 0.9 mm diffuser. It never enters the 123-223 deg sector, so it is open at the leadpipe/brace by construction (brace 33.6 mm away, leadpipe 18.7 mm).
- **Strip capacity: channel path 308 mm (bridge 80 + bell 228) = about 49 pixels of the 160 LED/m FCOB strip; the 1 m strip has to be cut to about 0.3 m** (more if you accept running it into the shell: the strip enters the channel at the shell slot).
- Printed in two parts (X1C bed): `spiral_A_bridge_and_tail` (168 mm) and `spiral_B_bell_and_mic_arm` (150 mm), joined by a 14 mm scarf-lap with 4 snap pins at s ~ 112 mm; no screws. `spiral_diffuser` (191 mm, 0.9 mm) stops 14 mm before the arm.
- Bell flare is the Sketchfab profile through PCHIP (rim 123.2 mm); the bell tube behind s = 83 is extrapolated [A]. Measured clearances: 1.5 mm to the bell surface and rim bead.

## 6. Mic arm
- The ribbon ends at the rim at 120 deg (top, leadpipe side), continues as a slim 11 x 8 mm arm that rises over the rim bead (>= 1.4 mm away) and ends in a round 15.6 mm socket with a click groove, 25 mm in front of the rim, aimed INTO the bell at 54 deg to the bell axis (mostly toward the axis).
- Inserts (round plugs, rotate 360 deg in the socket): tilt -4 / 0 / +6 deg about the tangent axis (+ = steeper toward the axis), boss for a 3/8"-16 brass insert (9.7 mm hole); use a 3/8"-16 male to 5/8"-27 male adapter for a generic mic holder. The mic body is unknown.
- Mic check with an 11 mm radius x 75 mm envelope on the insert axis: the body stays on one side of the bell wall at every station and clears the rim bead. Minimum clearance to the bell wall/rim: tilt -4 deg 1.3 mm, 0 deg 4.4 mm, +6 deg 6.5 mm (v2's mic crossed the wall; this does not). A tilt of -8 deg or less crosses the wall (computed -1.4 mm at -3 deg with the 50 deg aim, so the inserts stop at -4 deg here with a 54 deg aim).

## 7. Hall rail and valve caps
- Decision: **sensors stay above the top caps, magnets move into finger caps that replace the buttons; free stem travel is 13.5 mm** (cap underside at z 133 at rest, sensor top at 119.5; 13.5 mm available, 0.5 mm margin at 13 mm travel; 13.5 mm gap at rest vs the required >= 5 mm). v2 had ~8 mm. `hall_rail_49E` clips onto the three top caps (rail 2.0 mm thick at z 117.5-119.5, 0.25 mm clearance on the caps, 49E pockets 5.6 x 5.6 mm at y +9.5 from each stem). The alternative (sensors under the bottom caps) was not needed.
- `valve_cap_1/2/3` + `valve_cap_3_spare` (one STL each): 16 mm finger cap with a magnet lobe at y +9.5 over the sensor; pocket 10.4 mm dia x 3.3 mm deep from the underside (face toward the sensor), a 0.9 mm wide glue relief groove in the pocket ceiling and a 1.3 mm vent hole to the top; stem snap bore 3.9 mm x 5 mm deep with three relief slits (snap-fit around the original button stem/threads [A: stem 4.0 mm and engagement depth are model values; check on the real stem]); no steel washer seat. BALLER: 9-peak zig-zag raised band (1.0 mm high, 2.5 mm ridge, > 0.8 mm features) on the side, eight 2.4 mm polka-dot dimples (0.6 mm deep, rim ring only) and three on the lobe; the finger top is smooth. Cap height 7.0 mm (+3 mm over the original button): [A] check finger feel. Mesh is slightly lumpy in the closeup (0.14 mm voxel + Taubin smoothing).

## 8. Verification (`cad/verify_report_v3.json`)
- 17 STLs: all watertight, single body, within the 256 mm bed (largest: mount 221 x 62 x 146; shell 157 x 93 x 204; spiral A 168 mm).
- Assembled intersections: mount x spiral A 0.02 mm3, spiral A x B 0.64 mm3 (pin/hole contact), spiral A x diffuser 1.9 mm3 (all slivers); mount x shell, shell x spiral, shell x rail: 0.
- Nearest approach (rigid parts to the reference horn): mount 0.54-0.71 mm (slides/casings/hook) as above; shell 22 mm to casings and caps, 26 mm to slide tubes, 44 mm to the ring, 50 mm to the thumb hook; spiral 1.5 mm to the bell, 18.7 mm to the leadpipe, 33.6 mm to the brace, 22 mm to slide tubes; Hall rail 0.25 mm to the caps (clip seat).
- Water keys: positions unknown; the v2 assumed spheres were not used in v3. Nearest horn feature to anything printed other than the arms is >= 22 mm.

## 9. Open issues (honest)
1. Overlay render: the original comp ghost z-fights with the v3 face (same plane), so only the right side shows green; the table above is the real proof.
2. The Trellis face is 16 separate 11 mm windows with 4 mm webs (the original layout); no recess/lip for the elastomer sheet was invented.
3. The shell back is open; the rail boss is the only mechanical interface; latch and wire passes to the lobe are not modelled.
4. Scarf-lap joint and the bridge plug are untested prints; print the spiral segment first.
5. Left/lateral direction of the palm lobe is an assumption (section 4).
