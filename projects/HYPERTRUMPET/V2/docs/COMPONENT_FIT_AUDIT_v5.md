# HYPERTRUMPET V2: component fit audit, v5 shell

Date 2026-11-16. Read-only audit: the shell/mount/door meshes were taken from a copy of `cad/hypertrumpet_v5.blend` made at audit start (`cad/_fit_audit_work/hypertrumpet_v5_copy.blend`). Tool: `cad/_fit_audit_work/fit_audit.py` (re-run on v5.2 by changing `SRC_BLEND`). Raw numbers: `cad/component_fit_v5.json`. Renders: `cad/_fit_audit_work/renders/`.

## Verdict

**The electronics mostly fit, but the shell is not ready to build.** 5 hard problems and 6 soft ones, ranked below. The three that matter most: the LED strip has no channel or window at all, a 45 mm TRAVEL fader does not fit the 41 mm slots, and the Hall board and Trellis cannot be installed straight in (tolerance problems of 5 mm and a rib respectively). Nothing is oversize in volume: components fill 16.2% of the pod cavity (80 of 496 cm3 as bounding boxes) and 51.4% of the battery bay, 25.6% overall (173 of 676 cm3).

## Method and what the numbers mean

- Geometry: shell, mount, door meshes exported with Blender in background mode, voxelised at 0.5 mm (point-in-mesh test at voxel centres, embree). Clearances from a 0.5 mm distance field (+-0.25 mm); anything under 1.5 mm was re-measured against the exact mesh (the "exact" values).
- Interior void: air enclosed by the wall ring in every y slice (the back at y 542.3 is treated as closed by the mount plate; face holes, USB, jack, switch, Hall windows and the pod-top slot are plugged). Pod cavity 496 cm3, battery bay 180 cm3. A 3D flood fill leaked through several 0.2-1 mm gaps, so the slice method was used.
- Parts are axis-aligned boxes. Parts with no defined position (BMS, boost, level shifter, Wago, mic pot, MPU6050, FSR) were placed by a free-space search that keeps 0.5 mm slop and a clear wire riser; these are FEASIBLE placements, not requirements.
- Panel parts butt the inner skin (y 566.2) on purpose; that contact face is excluded from the clearance.
- Frame: x along horn, y depth (control face at 568.4, skin inner 566.2, back plane about 541.6), z up, mm.
- Every dimension tagged ASSUMED is a typical-module value, not a datasheet value. The assumptions that drive the verdict are listed at the end.

## Component table

Fits = no shell/mount collision above 1 mm3 and no overlap with another component. "Min clearance" is to the shell/mount/door; "nearest" is the closest other component and the gap in mm.

| component | dims, first two boxes (mm) | source | position (mm) | fits | min clearance to shell (mm) | nearest component: gap | collision | issue |
|---|---|---|---|---|---|---|---|---|
| Daisy Seed 3 PCB | 51.1x1.6x18.0 / 42.0x2.0x16.0 | 51.1x18.0 (Electro-Smith Seed datasheet / V4 params SEED_L,SEED_W); position from verify_report_v4 ports; chip height, header length ASSUMED | x 740-793, y 544-557, z -3-15 | N | -0.25 | TPS61088 5 V boost module: 0.75 (41.5 mm3) | PCB and header row A clip the rounded -x/-z corner of the Seed lobe: 41 mm3, 1.4 mm deep at x 742-745, z -3..-1. Install by +y then 4 mm slide works. USB-C mouth 0.4 mm behind wall inner face (design). No posts/retention; headers point at the open back. |
| NeoTrellis 4x4 PCB + rear parts | 60.2x1.6x60.2 / 60.2x3.0x54.2 | 60.2 sq PCB (V4_NOTES, Adafruit 1929 listing); PCB centre (853.2,90.2) from build; rear height ASSUMED | x 823-883, y 559-563, z 60-120 | Y | 3.25 | Trellis silicone keypad stack: 0.0 (0) | Placement fits (PCB top z 120.3, 0.6 mm under the Hall board z 120.9). INSTALL: straight +y blocked by the dovetail rib (11.8k mm3); no rigid 2-leg path found (see problem 4). |
| Trellis silicone keypad stack | 60.2x3.0x60.2 | Adafruit 1611 elastomer pad ~60 sq; thickness ASSUMED | x 823-883, y 563-566, z 60-120 | Y | 1.25 (exact 2.02) | NeoTrellis 4x4 PCB + rear parts: 0.0 (0) | Butts the inner skin by design. Thickness ASSUMED. 2.0 mm to side walls. |
| uxcell 45mm B10K slide fader #1 | 9.5x6.5x45.0 / 8.0x3.0x12.0 | Adafruit 4272 (45 mm x 9.5 x 26 incl. knob), TT PS45 (60x9x6.5, 45 mm travel), Same Sky 45 mm/30 stroke; uxcell listing not found, ASSUMED | x 837-846, y 557-566, z -5-40 | Y | 1.25 (exact 1.53) | uxcell 45mm B10K slide fader #2: 1.45 (0) | Case A (45 mm = body, 30 mm travel) fits. Case B (45 mm travel, 60 mm body) fails twice: needs a 48 mm slot (have 41) and the 60 mm body hits the lidar board (z 44.6-47.5). |
| uxcell 45mm B10K slide fader #2 | 9.5x6.5x45.0 / 8.0x3.0x12.0 | Adafruit 4272 (45 mm x 9.5 x 26 incl. knob), TT PS45 (60x9x6.5, 45 mm travel), Same Sky 45 mm/30 stroke; uxcell listing not found, ASSUMED | x 848-857, y 557-566, z -5-40 | Y | 1.25 (exact 1.52) | uxcell 45mm B10K slide fader #3: 1.45 (0) | as #1; install needs 4 mm -x offset then slide over the rib. |
| uxcell 45mm B10K slide fader #3 | 9.5x6.5x45.0 / 8.0x3.0x12.0 | Adafruit 4272 (45 mm x 9.5 x 26 incl. knob), TT PS45 (60x9x6.5, 45 mm travel), Same Sky 45 mm/30 stroke; uxcell listing not found, ASSUMED | x 859-868, y 557-566, z -5-40 | Y | 1.25 (exact 1.53) | uxcell 45mm B10K slide fader #2: 1.45 (0) | as #1; install needs 12 mm -x offset then slide over the rib. 1.45 mm to its neighbour. |
| Round hole 1 part (KY-040 worst case) | 19.0x1.6x31.5 / 12.0x6.5x13.0 | KY-040 (Taiss) board ~32 x 19 x 1.6, encoder body 12x13x6.5 [typical; ASSUMED]; assignment of encoders vs pots to holes is unknown, worst case used | x 744-763, y 550-566, z 26-58 | Y | 1.25 (exact 1.53) | 74AHCT level shifter: 0.7 (0) | Fits (worst-case KY-040). 0.7 mm to the free-placed level shifter (placement is mine, movable). |
| Round hole 2 part (KY-040 worst case) | 19.0x1.6x31.5 / 12.0x6.5x13.0 | KY-040 (Taiss) board ~32 x 19 x 1.6, encoder body 12x13x6.5 [typical; ASSUMED]; assignment of encoders vs pots to holes is unknown, worst case used | x 744-763, y 550-566, z 86-117 | Y | 0.62 (exact 0.89) | 74AHCT level shifter: 2.5 (0) | TIGHT: 0.89 mm to the top-left roll wall (PCB z 85.8-117.3 vs silhouette). |
| Round hole 3 part (KY-040 worst case) | 19.0x1.6x31.5 / 12.0x6.5x13.0 | KY-040 (Taiss) board ~32 x 19 x 1.6, encoder body 12x13x6.5 [typical; ASSUMED]; assignment of encoders vs pots to holes is unknown, worst case used | x 801-820, y 550-566, z 36-67 | Y | 1.25 (exact 1.52) | NeoTrellis 4x4 PCB + rear parts: 2.9 (0) | Fits. 2.9 mm to Trellis rear parts. |
| Round hole 4 part (KY-040 worst case) | 19.0x1.6x31.5 / 12.0x6.5x13.0 | KY-040 (Taiss) board ~32 x 19 x 1.6, encoder body 12x13x6.5 [typical; ASSUMED]; assignment of encoders vs pots to holes is unknown, worst case used | x 801-820, y 550-566, z 79-111 | Y | 1.25 (exact 1.53) | NeoTrellis 4x4 PCB + rear parts: 2.9 (0) | Fits. 2.9 mm to Trellis rear parts. |
| KY-023 joystick module | 26.0x1.6x34.0 / 24.0x12.0x24.0 | KY-023 listing: PCB ~34 x 26, thumbstick housing ~24 sq; heights ASSUMED; design note says >=18.9 deep | x 768-794, y 544-566, z 55-89 | Y | 1.25 (exact 2.03) | Round hole 2 part (KY-040 worst case): 5.69 (0) | Fits with 12 mm housing. Depth stack 22 mm incl. header pins of 25.2 available: Dupont female sleeves on the 5-pin header would NOT fit; solder wires. |
| VL53L0X breakout | 18.5x1.6x13.3 / 12.7x8.5x2.54 | generic GY-530 ~13.3 x 18.5 (ASSUMED; Pololu 2490 is 13x18); window 11x8 from build (LIDAR_POS) | x 843-862, y 555-565, z 45-58 | Y | 1.25 (exact 1.63) | Trellis silicone keypad stack: 2.25 (0) | Fits sideways (18.5 along x). 2.25 mm under the Trellis silicone, 4.5 mm above fader tops. Install via a 8 mm -x offset then slide. |
| Twidec mini toggle SPST | 14.0x13.0x8.0 / 5.0x10.0x6.0 | Twidec MTS-10x class mini toggle (6 mm bushing, ~13x8 body, ~14 mm below panel incl. lugs) ASSUMED; hole position from V5_PRINT_READY | x 776-794, y 525-538, z -38--30 | N | -0.25 | eMagTech 1S 10A BMS: 8.14 (12.5 mm3) | Rectangular 13 x 8 body corners clip the 14 mm round spot-face: 12.5 mm3, 0.7 mm. Install from the bay is fine. |
| BLLNDX 3.5 mm panel jack | 14.0x9.0x9.0 | PJ-3.5 style panel jack, 6 mm bushing, ~14 deep ASSUMED | x 878-892, y 551-560, z 2-10 | N | -0.25 | uxcell 45mm B10K slide fader #3: 10.1 (63.0 mm3) | 9 x 9 body overlaps the dovetail rib top (y 551) by 0.5 mm: 63 mm3. Hole is 4.4 mm above the rib. Install from inside blocked by the rib for a straight +y move; slides in from the bay-side cavity (z<6) instead. |
| Hall rail board (3x SS49E) | 81.0x20.0x1.6 / 4.1x3.0x4.5 | BOM/V5_PRINT_READY board 81x20x1.6; SS49E datasheet TO-92S 4.05x3.0x1.5; pocket from build POCKET_* | x 810-890, y 524-544, z 121-127 | Y | 0.25 (exact 0.11) | NeoTrellis 4x4 PCB + rear parts: 14.82 (0) | Seats in the pocket with 0.1-0.5 mm to its walls. INSTALL: the board is 81 long; at z 121-127 the cavity depth is only 559.5 at x 810 and 563.5 at x 889 (roll), the board must start at y>=565 to enter the pocket: blocked 5.5 mm at the -x end (241 mm3). |
| 21700 cell + PCB holder #1 | 77.0x24.0x24.2 | 21700 cell 21.7 x 70.0 mm (IMR); holder dims for ASIN B0GFSMXJF7 NOT in repo (V4_NOTES used 77x24x21), ASSUMED | x 806-882, y 508-532, z -49--25 | Y | 0.25 (exact 2.8) | 21700 cell + PCB holder #2: 0.4 (0) | Envelope 77 x 24 x 24.2 ASSUMED; 1.8-2.8 mm to bay walls, 0.4 mm to the other holder. Insert through the door opening: clear. |
| 21700 cell + PCB holder #2 | 77.0x24.0x24.2 | 21700 cell 21.7 x 70.0 mm (IMR); holder dims for ASIN B0GFSMXJF7 NOT in repo (V4_NOTES used 77x24x21), ASSUMED | x 806-882, y 532-556, z -49--25 | Y | 0.25 (exact 1.8) | 21700 cell + PCB holder #1: 0.4 (0) | as #1. |
| eMagTech 1S 10A BMS | 45.0x10.0x3.0 | eMagTech 5-pack listing; dims ASSUMED (not found) | x 782-827, y 543-553, z -23--20 | Y | 0.75 (exact 0.73) | 21700 cell + PCB holder #2: 1.35 (0) | Placed on top of the cells (z -23.3..-20.3): 0.73 mm under the bay roof, 1.35 mm above the holders. Dims ASSUMED. |
| TPS61088 5 V boost module | 6.0x18.0x28.0 | generic TPS61088 breakout ~ 28x18 (ASSUMED) | x 771-777, y 545-563, z 16-44 | Y | 3.25 | Daisy Seed 3 PCB: 0.75 (0) | Free placement found at (771-777, 545-563, 16-44), 0.75 mm from the Seed. Fits. |
| 74AHCT level shifter | 5.0x12.0x25.0 | 74AHCT125 breakout ~ 20-25 x 12 (ASSUMED) | x 741-746, y 548-560, z 58-83 | Y | 6.01 | Round hole 1 part (KY-040 worst case): 0.7 (0) | Free placement found. Fits. |
| Wago 221-415 star ground | 13.5x20.0x28.0 | Wago 221-415 5-conductor; dims ASSUMED (datasheet not read) | x 782-796, y 544-564, z 16-44 | Y | 2.25 | Daisy Seed 3 PCB: 0.75 (0) | Free placement, 0.75 mm from the Seed. Fits, but this is the biggest loose part (28 x 20 x 13.5 ASSUMED). |
| 10k mic-level pot (no panel hole exists) | 10.0x10.0x12.0 | BOM: 10k pot as the trumpet mic level; no shell hole for it | x 800-810, y 549-559, z -6-6 | Y | 5.79 | MPU6050 (GY-521): 3.75 (0) | No panel hole in the shell for it (4 round holes = 2 KY-040 + 2 pots). Fits only as an inside trimmer. |
| MPU6050 (GY-521) | 21.0x10.0x16.5 | GY-521 typ 21.2 x 16.4 (ASSUMED) | x 798-819, y 549-559, z 10-26 | Y | 7.25 | Wago 221-415 star ground: 1.75 (0) | Free placement. Fits. |
| FSR402 | 1.5x19.0x19.0 | Interlink FSR402 d18.3 active 12.7, tail ~ 56 total (datasheet) | x 821-822, y 545-564, z -14-5 | Y | 2.04 | MPU6050 (GY-521): 5.26 (0) | Fits as an object but the shell has NO window or pad location for it: cannot be pressed. |

Xvive TX units: external plug-ins, not packed. Wiring bundle: not modelled as geometry, checked as a route (below), allowance = 6 mm clear corridor.

## Fader length check

Slot 41.0 x 5.6, lever 3 mm, so the lever centre has 38 mm of travel. The BOM says "45mm B10K"; the uxcell listing dimensions were not found, and the same-sounding parts differ: Adafruit 4272 is 45 mm overall length, TT PS45 is 60 mm long with 45 mm travel, Same Sky 45 mm parts have 30 mm stroke.
- If 45 mm = body length (about 30 mm travel): fits, 8 mm of slot slack.
- If 45 mm = travel (body about 60 mm): MISMATCH. Needs a 48 mm slot (7 mm short) and the 60 mm body (z -12.5..47.5) collides with the lidar board (z from 44.6) by 2.9 mm.
- Caliper one fader before printing.

## LED strip (FCOB, 5 mm wide, about 157 px, about 1 m)

**The shell has no channel, groove, exit slot or translucent window for the strip.** The V4 ribbon and strip channel were removed in the v4 scope change; the only exterior openings are the Trellis, fader, lidar, round, joystick, USB-C, jack, switch and Hall openings. Interior space exists (the strip is only about 10 cm3), but nothing can display it. The BOM line "LED strip, trumpet" has no geometric support.

## Install order and reachability (shell off the mount, open back)

Straight +y motion from the back plane (y 530), a rigid x/z offset then a slide at final depth if needed, tested against the shell only.

| part | path | blocked mm3 | ok |
|---|---|---|---|
| Trellis silicone keypad stack | +y from the open back (shell off the mount); NO path found (x shift +-60, z shift +-40) | 11765.1 | N |
| uxcell 45mm B10K slide fader #1 | +y from the open back (shell off the mount) | 0.0 | Y |
| uxcell 45mm B10K slide fader #2 | +y from the back offset by dx=-4, dz=+0 mm, then slide (+4,+0) at final depth | 0.0 | Y |
| uxcell 45mm B10K slide fader #3 | +y from the back offset by dx=-12, dz=+0 mm, then slide (+12,+0) at final depth | 0.0 | Y |
| Round hole 1 part (KY-040 worst case) | +y from the open back (shell off the mount) | 0.0 | Y |
| Round hole 2 part (KY-040 worst case) | +y from the open back (shell off the mount) | 0.0 | Y |
| Round hole 3 part (KY-040 worst case) | +y from the open back (shell off the mount) | 0.0 | Y |
| Round hole 4 part (KY-040 worst case) | +y from the open back (shell off the mount) | 0.0 | Y |
| KY-023 joystick module | +y from the open back (shell off the mount) | 0.0 | Y |
| VL53L0X breakout | +y from the back offset by dx=-8, dz=+0 mm, then slide (+8,+0) at final depth | 0.0 | Y |
| Wago 221-415 star ground | +y from the open back (shell off the mount) | 0.0 | Y |
| FSR402 | +y from the open back (shell off the mount) | 0.0 | Y |
| TPS61088 5 V boost module | +y from the open back (shell off the mount) | 0.0 | Y |
| NeoTrellis 4x4 PCB + rear parts | +y from the open back (shell off the mount); NO path found (x shift +-60, z shift +-40) | 11765.1 | N |
| 74AHCT level shifter | +y from the open back (shell off the mount) | 0.0 | Y |
| BLLNDX 3.5 mm panel jack | +y from the open back (shell off the mount); NO path found (x shift +-60, z shift +-40) | 586.1 | N |
| 10k mic-level pot (no panel hole exists) | +y from the open back (shell off the mount) | 0.0 | Y |
| MPU6050 (GY-521) | +y from the open back (shell off the mount) | 0.0 | Y |
| Daisy Seed 3 PCB | +y from the back offset by dx=+4, dz=+0 mm, then slide (-4,+0) at final depth | 0.0 | Y |
| 21700 cell + PCB holder #2 | -z through the bottom door opening | 0.0 | Y |
| eMagTech 1S 10A BMS | +y from the open back (shell off the mount) | 0.0 | Y |
| Hall rail board (3x SS49E) | slide -y into the pocket from the cavity (board+sensors, start y 565); blocked at x 810-890 y 556-566 z 121-128 | 241.6 | N |
| Twidec mini toggle SPST | from the battery bay (door open), +x into the spot-face | 12.5 | N |
| 21700 cell + PCB holder #1 | -z through the bottom door opening | 0.0 | Y |

## Wire routing

A 6 mm diameter corridor (distance to obstacles >= 3 mm, voxel test over shell + all boxes) connects the wire stadium riser (x 837-862, y 543-558, z -18.7..-16.5; clear width 16.0 mm in plan, 25 x 15 mm stadium) to every component, including the Seed, Trellis, boost, faders, pots, Hall pocket, joystick and the bay parts. Not tested: the bundle as a whole (60+ conductors is about 20 mm2, more than one 6 mm wire), and routing around the Hall board during its slide.

## Problems, worst first, each with the smallest shell change

1. **LED strip: no channel or window.** Smallest change: add a 6 x 2.5 mm groove plus an 8 x 3 mm exit slot to a diffuser window, or drop the strip from the trumpet BOM. Cannot be decided from geometry.
2. **Fader travel (conditional).** If the sliders are 45 mm travel: lengthen the 3 slots from 41.0 to 48.0 mm (they are at z -3..38, headroom to the Trellis web is 6.85 mm lidar gap; slot top can go to z 45 only after moving the lidar window up 4 mm or the slots down 7 mm toward the keel). If 45 mm is body length: no change.
3. **Hall board cannot be loaded.** At z 121-127 the cavity depth ends at y 559.5 (x 810) and 563.5 (x 889) because of the top roll, but the 20 mm board must start with its front edge at y 545. Short by 5.5 mm at the -x end, 2.5 mm at the +x end (241 mm3 blocked). Smallest change: shorten the board to 71 mm (and the pocket by 10) or open the channel 6 mm taller toward +z at the ends, or accept a tilted insertion.
4. **Trellis (and lidar, faders 2-3, jack) install past the dovetail rib.** The rib (x 857-897, y 541-551, z -6..108) is in the straight back path of the Trellis (11.8k mm3). A rigid two-leg path was not found because the cavity silhouette notch near x 795-801, z 110-114 clips the PCB corner on a shifted slide; a 3-leg or tilted install probably exists but was not proven. Smallest change: cut a 26 x 62 mm window through the non-engaging part of the rib (x 857-883, z 58-121; keep the dovetail neck x 871-885 only below z 58) or enlarge the notch.
5. **Seed lobe corner collision.** The Seed PCB and header row clip the rounded -x/-z corner by 41 mm3 (1.4 mm deep at x 742-745, z -3..-1). Smallest change: add 1.5 mm to the lobe corner radius at that spot, or move the Seed +1.5 mm in x and the USB opening with it (USB mouth is already 0.4 mm behind the wall).
6. **Panel jack overlaps the rib top by 0.5 mm.** Jack hole centre y 555.2 is 4.4 mm above the rib top (y 551). Smallest change: raise the jack hole 1 mm (y 556.2) or use a jack body <= 8 mm tall; install from the bay side.
7. **Toggle corner touches the spot-face.** 13 x 8 body in a 14 mm round recess: 0.7 mm. Smallest change: spot-face diameter 14 -> 16 mm or turn the switch 90 degrees (8 mm along y).
8. **Pot/encoder hole 2 is tight, 0.89 mm.** (KY-040 PCB at x 744-763, z 86-117 against the roll.) Fits as modelled; use the pot (smaller) in that hole or rotate the PCB 90 degrees.
9. **No hole or window for the 10k mic-level pot and the FSR402.** The 4 round holes are 2 encoders + 2 macro pots. Smallest change: one more 8.7 mm round hole for the mic pot (or fix it as an inside trimmer), and a thumb-pad or flat spot for the FSR.
10. **No retention features.** Seed, Trellis, faders, boards and the free-placed modules have no posts, ribs or clips in the shell (V4 notes say "posts not modelled"). Adds mass of printed standoffs; budget 5 mm of y near the Seed and Trellis.
11. **Open back in the assembled state at the Seed lobe.** With the mount fitted, the plate outline ends at x about 795 for z < 25, so the Seed lobe (x 740-795, z -3..25) is open to the air behind. Verify in the slicer; add a 2 mm closing wall or a cover if this is not intended.
12. **Pod top slot.** A top-down scan found no solid above z 100 over x 845-862, y 547-552.5 (about 6 x 17 mm). Plugged in the flood. Verify in the slicer: if real, it is an unintended opening over the Hall rail roof.

Tight spots under 1 mm (clearance or neighbour gap): Hall board in its pocket (0.1-0.5), KY-040 hole 2 (0.89), BMS to the bay roof (0.73), holder-to-holder (0.4, assumed envelopes), free-placed boost/Wago/shifter next to the Seed (0.7-0.75; movable).

## Things that could not be determined

- Real dimensions of 12 parts (marked ASSUMED in the table): holder envelope (B0GFSMXJF7 not in the repo), BMS, boost, Wago, toggle, jack, KY-023 height, fader listing, silicone pad, Seed top chips and headers.
- Whether the user intends encoders or pots in each of the 4 round holes (worst case used).
- The pocket x/z slop and tolerance on real prints.
- The live `cad/hypertrumpet_v5.blend` was changed by another agent after the audit copy: at 04:13 the shell volume was about 358,900 mm3 vs 318,400 mm3 in the audit copy, and the back appears closed by a 1.6 mm plate at y about 541-545 plus a logo engraving. If that back wall is intended, the cavity can only be loaded through the battery door and the wire stadium, and every conclusion above about back-loading changes. Re-run `fit_audit.py` on the final v5.2 file.

## Assumptions that most affect the verdict

1. Fader: 45 mm = body length (fits) vs travel (does not).
2. Battery holder envelope 77 x 24 x 24.2 for two parallel cells (the bay is 111.9 x 53 x 30.4, so there is margin; BMS sits on top).
3. Back plane closed at y 542.3 for the volume (assembled state), open for loading (shell off the mount).
4. KY-023 stack 12 mm + PCB, no Dupont sleeves; Trellis PCB position (853.2, 90.2, top z 120.3) from V4 notes.
