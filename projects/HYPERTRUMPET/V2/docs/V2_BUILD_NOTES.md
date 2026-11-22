# HYPERTRUMPET V2 CAD build notes (build v01, 2026-11-14)

Source of truth: `cad/hypertrumpet_v2_build.py` (all dimensions named in the `P` dict at the top; voxel/SDF geometry, one STL per part) and `cad/hypertrumpet_v2_blend.py` (assembles `cad/hypertrumpet_v2.blend` and renders). Re-run: `python hypertrumpet_v2_build.py all` (about 2 min, venv with numpy scipy scikit-image trimesh manifold3d shapely fast-simplification mapbox-earcut matplotlib), then `Blender -b --factory-startup --python hypertrumpet_v2_blend.py -- out.blend render_dir`. v1 files in `cad/v1_reference/` are read-only inputs (arms are derived from `_mount_v3.stl`). Snapshots: `cad/snapshots/*_v01.*`. Tags: [M] measured on files, [G] general knowledge/unverified, [A] assumed, [I] inference. Frame: x along horn, y lateral (pod on +y), z valve axis, mm.

## 1. Parts (cad/stl_v2/, 32 STL, all verified watertight single bodies, all fit the 256 mm bed)

| STL | Material | Bbox (mm) | Vol cm3 | Print note |
|---|---|---|---|---|
| mount_arms_plate | PETG | 217 x 148 x 62 | 272 | stand on its top edge as v1 (plate vertical, layers across arms) |
| tpu_pad_01..15 | TPU 95A | ~30 x 15 x 5 each | 0.4-0.9 | glue/press into the 1.7 mm recess they sit in |
| pod_face_panel | PETG | 135 x 122 x 18 | 77 | face down, 0.2 layers; panel 2.0 mm |
| palm_rest_dome | PETG (TPU skin optional) | 112 x 87 x 50 | 215 | dome up on its flat top needs supports in the channel; or print on its +x end |
| elec_bay_cap | PETG | 30 x 30 x 13 | 8 | flange down |
| battery_drawer | PETG | 80 x 48 x 24 | 25 | flat, lip slits across the print direction |
| spiral_ribbon_mic_arm | PETG | 173 x 134 x 71 (local frame, see `stl_offsets.json`) | 47.5 | axis vertical, supports in the arm; one body: helix + arm + socket |
| spiral_diffuser | clear/white PETG, 0.9 mm | 118 x 70 x 69 | 6.2 | vase-mode style helical strip |
| mic_angle_insert_00/10/20/30deg | PETG | ~25 x 22 x 30 | 8.5 | swap-in plugs |
| hall_rail_49E | PETG | 69 x 25 x 8.4 | 4 | flat |
| valve_cap_magnet_carrier (x3) | PETG | 19 x 15 x 4 | 0.7 | flat, 10 x 3 magnet from below |
| coupon_grip_ring_c02/c04/c06, coupon_spiral_segment | PETG | 25 x 14 / 54 x 48 x 44 | 3 / 8 | fit tests |

Spiral parts and the segment coupon are exported centred on (1090, 525, 110) so float32 STL precision does not break manifold (assembled position = STL + offset in `stl_offsets.json`).

## 2. What EMGOR asked for and what was built

1. Arms (v1 geometry kept). The arm volume is `_mount_v3.stl` with y < plate rear, thickened 1.2 mm per side (min blade 3.6 -> 6.0 mm), R3 root fillets (closing), 1.2 mm edge rounding, cut by the standard-dimension horn plus clearance, and cut by the open-air keep-outs. Measured result: rigid arm surfaces are 1.23 mm from the analytic horn items and 1.23 mm from the Sketchfab-derived tubes/ports; the 0.4 mm clearance rule holds with margin because every contact surface is recessed 1.7 mm and filled by a TPU pad (pad spans -0.2 to +1.6 mm from the horn, i.e. 0.2 mm interference at the pad face; measured -0.24 to -0.33 on the pads). No region thicker-than-needed check failed: no region thinner than 5 mm with volume > 150 mm3 [M, opening test]. The two v1 interfering spots (top-right blade on the thumb hook, right blade on the 1st-slide exits) are removed by the saddle/slide keep-outs rather than nudged: that cost the top-right blade entirely. The lower-left blades inside the 3rd-slide leg plane are cut back to the plane edge. Arm volume vs v1: 128 cm3 of v1 arms -> the v2 mount is 272 cm3 including the new 10 mm plate and the rail.
2. Snap-fit, no screws. Pod joint: dovetail rail along z (neck 14, head 22, height 6, 0.4 mm per side clearance) with an in-plane flex tab (floor of the plate channel, x 845-878, z -8 to -1, 7 mm wide, bump 1.2 mm into a notch in the pod rail). Rest joint: same dovetail along x on the plate bottom (x 825-877) with lead-in ramp, tab and bump in the rest channel floor. The v1 pegs and cups are gone.
3. Face panel 2.0 mm. Cavity behind the panel 8.0 mm, back wall 2.0 mm, face at y = 553.0. Part-by-part mounting check (thread/shaft lengths are [G]; confirm with the real parts):

| Part | Panel feature | Check |
|---|---|---|
| KY-040 encoder (M7 bushing, ~7 mm thread) | 7.2 mm hole | 7.0 - 2.0 = 5.0 mm thread proud of the face: holds a 3 mm nut and the knob grub |
| B10K 16 mm pot (M7 bushing, 6-7.5 mm) | 7.2 mm hole + 14 mm back-wall window | 4.0-5.5 mm proud: nut 2.5 mm fits; body 9 mm deep + pins use the window |
| KY-023 joystick | 20 mm hole, 35 x 27 mm seat frame | stick 17 mm above PCB; PCB top 6.5 mm behind the panel back; cap ends ~8.5 mm proud of the face |
| uxcell 45 mm slide faders (3) | 47.0 x 3.4 slots, 15 mm pitch, x 830-877, z = 33 / 18 / 3 | body <= 7.5 mm under the panel; handle passes the slot; clip ribs at x 822 / 885 |
| NeoTrellis 4x4 (61.7 mm board) | one 59.3 x 59.3 window (1 mm frame), 4 snap pins at +-27.4 mm | PCB back at y 544.1, key tops 553.9 (stack 9.8 [G]), 0.9 mm proud of the face; no screws |

4. Layout (r2 numbers kept, faders re-derived). Trellis centre (853.5, 76.35); joystick (912, 108) (z 112 -> 108 so the 35 mm module frame clears the pod top at z 127; thumb saddle is at z 104-129); pots at (904, 926; z 80), encoders at (904, 926; z 50); faders horizontal below the pads at z 33 / 18 / 3, 13 mm below the bottom Trellis edge (z 45.5). Fader row 1 is 34 mm below the ring finger row (z 69) and the little-finger row (z 54): reachable by ring/little finger while index/middle stay on the pads [I]. Pod face y 553 is within the 53 mm grip limit measured from the casing far face (y 500) [I].
5. Palm rest (not gripped). Broad convex dome 112 x 87 x 50 mm, longitudinal radius 220, transverse 120, R12 on every edge, smooth analytic surface (no contour terraces). FSR402 recess (19 mm) in the surface at (856, 559) with a 3.4 mm tail hole into the electronics bay [A: placement is a first guess, move it where the palm actually presses]. Cell bay 47.6 x 25.4 x 82 mm, open at the +x end; electronics bay 25.5 x 26 x 118 mm, open at the -x end with `elec_bay_cap` (3.5 mm jack hole, toggle hole, USB-C slot). Cable slot up to the pod bottom.
6. Slides priority. Keep-out volumes (6 mm air around slide legs over 100 mm of travel each way, 10 mm around the ring and thumb hook cylinders through the full y range, 16 mm radius spheres at the assumed water-key positions, the button/finger volume above the caps, and the Hall-rail zone) are subtracted from the arms. The pod and palm rest are outside them (pod nearest approach 19.4 mm to any slide tube, 41 mm to the ring, 42 mm to the thumb hook). Design principle: nothing sits within 6 mm of a slide, nothing within 18 mm of the ring or saddle; the plate outline keeps v1's receding notches at the ring (left) and hook (top right).
7. Spiral on the bell. s = 20 -> 140 mm from the rim, pitch 18.7 mm, 6.4 turns, ribbon 12 mm wide, base 2.0 mm off the bell (pad allowance; measured 1.4-2.0 mm to the bell surface), C-gap 100 deg centred 173 deg (toward the leadpipe) where the ribbon thins to a 2.4 mm PETG flexure. U channel 7.0 wide x 6.4 deep, groove for the 0.9 mm diffuser. LED path ~970 mm for the 1 m strip. The bell flare is the Sketchfab profile through PCHIP, rim 123.2 (standard 122-127); not verified on his bell.
8. Mic arm. The helix end continues into a solid 14.2 x 11.3 mm arm that rises 24 mm over the rim and ends in a round socket (20.4 mm bore, click groove) aimed 12 deg back of radial-inward. The four inserts are round plugs (0/10/20/30 deg) that rotate 360 deg in the socket (direction + angle both adjustable, click-retained). The insert boss takes a 3/8"-16 brass insert (9.7 mm hole); a 3/8"-16 male to 5/8"-27 male stud adapter then fits any generic clip-on dynamic mic holder (5/8"-27 is the common mic-clip thread; the mic body is unknown). Rim clearance of the printed parts to the rim bead: >= 1.4 mm (bell+rim min).
9. Rounded appearance: pod outline R14 and R3 edges, plate R3 fillets, dome R12, helix/arm chamfers.

## 3. Battery drawer (keep r2 design)
Tool-free sliding cartridge, 80 x 46.4 x 24.2 mm, finger notch and ribbon slot in the +x face, two flex detent bumps (0.4-0.8 mm) in matching bay notches, two cradles for bare 21700 cells with snap lips (3 lip segments each). Cells run along x; the polarity key is geometric: positive end wall has a 8.8 mm recess for a glued 8 x 1.2 mm button, negative end wall has a 9.6 mm centre post that stops a reversed button. [I: untested; I replaced the Bistook spring holders with printed cradles + nickel-strip contacts, say if you would rather keep the holders and I will size the cartridge to them.] Chain: cells -> contacts -> wire slot (8 x 6 mm) -> electronics bay (BMS, boost, Seed, 2 TX, toggle, jack).

## 4. Verification (`cad/verify_report.json`, `cad/clearance_summary.json`)
- STL: 32/32 watertight, 1 body, positive volume, within bed. Assembled pairwise intersections: 0 mm3 (rest x cap 0.0, spiral x diffuser 0.0).
- Mount (rigid) to horn: 1.23 mm to casings/caps/buttons; 3.05 mm to the 1st-slide leg roots (arm pad on casing 3, slide pulls away from it), 6.4 mm to all other slide legs/crowns; 18 mm to the finger ring; 22 mm to the thumb hook; 4.1 mm to the leadpipe; 0.8 mm to the assumed 3rd-slide water-key sphere (assumed position, r 16, that sphere already includes 16 mm of air). TPU pads: -0.24 to -0.33 (intended 0.2 interference).
- Pod: 15 mm to casings/caps, 19 mm to slide tubes, 41 mm to the ring, 42 mm to the thumb hook. Palm rest: 9.6 mm to the bottom caps, 35 mm to slides. Hall rail: 0.25 mm to the caps (clip seat), keeps 1.0 mm above the cap tops, 4.5 mm to the leadpipe.
- Spiral: 1.43 mm to bell/rim bead (ribbon 1.4-2.0 to the surface), 8.1 mm to the leadpipe, 19.5 mm to the brace, 28 mm to the 1st-slide crown.
- Bed: largest part is the mount at 217.5 x 147.5 x 62 mm; spiral 173 x 134 x 71 mm (no split needed).

## 5. Guesses and known problems (read these)
1. Hall rail vs valve travel: stems show 16 mm above the caps [M model]; the rail leaves ~8 mm of stem travel before the magnet carrier hits it. If the real piston travel is 10-13 mm the rail must move to the bottom caps; the top rail only works for <= ~8 mm travel. Nothing else in the design depends on it.
2. Pad and arm surfaces come from a 0.5 mm voxel grid plus Taubin smoothing: smooth to the eye, but expect ~0.1-0.2 mm surface error; the pad outlines are ragged (see render). A re-mesh at 0.3 mm is possible if you want cleaner pads.
3. Clearances were measured against my standard-dimension horn (casing 18.0 mm given by EMGOR; slide U-legs 12.6, pitch 21.5, cap 21.0, bottom cap 19.5 are [A]/[G]) and against shell-inflated Sketchfab parts for tubes/ports the standard horn does not model. The Sketchfab ports near casing 1 are not drawn in REF_HORN; some TPU pads hug them (in the render they look unsupported).
4. Water keys: positions assumed (3rd-slide crown and main tuning-slide bend). Measure when the horn is available; keep-outs are one-line edits in `keepout_items()`.
5. Faders/pots/encoders/joystick/Trellis dimensions (slot 47 x 3.4, M7 thread lengths, 61.7 mm board, 35 x 27 joystick) are catalogue-type numbers [G]; the pod has no way to correct a wrong hole except a reprint.
6. Latch tabs (flex 1.2 mm, 32 mm long, 2-3.5 mm thick) are strain-checked on paper only; release method for the rest latch is not designed (push the tab down with a 1.2 mm tool through the open +x channel end).
7. Retention of the arms on the real horn is unproven: only the TPU pads (0.2 mm interference) hold them. Print the three grip-ring coupons (casing OD + 0.4 / 0.8 / 1.2 mm diametral, i.e. 0.2 / 0.4 / 0.6 per side) and the spiral segment first.
8. The mic: body unknown. Proxy in the blend is a 18 mm x 70 mm cylinder.
9. The REF_HORN tuning-slide and bell-back tubes in the blend are schematic.

## 6. Renders (cad/renders_v2/)
`v2_iso_pod_side.png`, `v2_iso_other_side.png`, `v2_top.png`, `v2_left_hand_view.png`, `v2_bell_mic_closeup.png`, `v2_underside_palm_rest.png`. Judgement: the pod, dome and spiral read clean and rounded; the arms read as organic v1 slabs with visible slide/ring clearance; the pads are ragged blobs; the mic arm is bulky (socket boss 25 mm) but clears the rim.

## 2026-11-17: WS2812B strip physically damaged
The user found the FCOB WS2812B strip physically damaged. The "first ~12 px flicker, rest frozen" symptom seen 2026-11-16/17 is attributed to that damage, not to firmware (not proven either way). The strip firmware now uses register-level DMA (bringup/fw/ws2812_dma.h: DMA2 S6 -> SPI2, completion by NDTR==0 + 2 ms, since EOT never fires); telemetry strip.dma/done/ndz show it running (done 238, fb 0 on 2026-11-17). Re-test strip behaviour only on a replacement strip.

## 2026-11-17: HAL SPI2 end-of-transfer finding (WS2812 strip)
With the strip data on PB14 through `SPI_CFG2_IOSWP` (the MISO and MOSI functions swapped so the master's transmit data leaves on the MISO pin), the SPI2 EOT flag never sets in this configuration. Consequences: libDaisy's `HAL_SPI_Transmit` (blocking) and its DMA path (`DmaTransmit`) both wait for EOT and run into their timeout, so every frame took about 20 ms (5.2 ms of data plus a 14.7 ms HAL EOT timeout) and the main loop stalled up to 22.5 ms; the DMA variant sent one frame and never reported completion. Measured on bringup-1.8.
**Workaround (in firmware, `fw/ws2812_dma.h`, wired into `stripFrame`):** a register-level send that does not use EOT: the data is pushed straight to the SPI2 TX FIFO (or by DMA2 stream 6 where used) and completion is taken from TXC / the DMA NDTR reaching 0 instead. With it a frame costs its 5.2 ms of data. What remains is physical: with audio IRQs active the blocking send sees FIFO-dry gaps up to 87 us whenever an IRQ exceeds the roughly 41 us the WS2812 tolerates before it latches (`strip.sg`, `sgmax`, `n` count them); the DMA path would avoid that but could not be validated on the damaged strip, so it should be retested on a new strip.
