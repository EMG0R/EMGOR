# NEPTR 4.2 housing: print + assembly sheet

Final dome = `neptr_top_dome_4.2_backup_v26` (flat LiDAR seats, handoff §21; Ethernet D24.2 + insert pilots §20; thin LED band §18), cap = `cap_backup_v12`, default sled = `neptr_pi_sled_fit` (z6 / z10 kept as fallbacks). All STLs are **already in print orientation, binary, mm** (import, do not rotate; slicer "drop to bed" is fine). Design notes: `HANDOFF-housing-4.2.md` (§16 LiDAR (superseded by §21), §17 package, §18 polish, §20 Ethernet + inserts, §21 LiDAR seats).

## Files

| File | Qty | Tris | Size (mm) | Orientation |
|---|---|---|---|---|
| `NEPTR_4.2_fit_coupon.stl` | 1 (print first, PLA ok) | 49 750 | 160 x 153 x 13.6 | plate outer face on the bed, labels up; seam pieces rim/floor down. **Stale vs v26: still carries the old LiDAR post pair and Ethernet D23.4; regenerate (`build_coupon()`) before trusting it for LiDAR / Ethernet fit** |
| `NEPTR_4.2_top_dome.stl` | 1 | 506 606 | 248.0 x 206.9 x 49.6 | **top skin on the bed (rim up)**, flipped 180 deg about X from the model |
| `NEPTR_4.2_bottom_cap.stl` | 1 | 190 648 | 241.7 x 200.7 x 50 | **floor on the bed** (as modelled) |
| `NEPTR_4.2_pi_sled_fit.stl` | 1 (**default sled**) | 8 340 | 70 x 89 x 5.4 | flat, no supports. Pi PCB bottom z 4.8 (computed from the stack, see handoff §18c; the stack's tallest-riser value is ASSUMED, measure it, change `STACK['riser_tall']` in `NEPTR_4.2_pi_sled.py`, regenerate) |
| `NEPTR_4.2_pi_sled_z6.stl` | fallback | 8 716 | 70 x 89 x 6.6 | flat |
| `NEPTR_4.2_pi_sled_z10.stl` | fallback | 8 716 | 70 x 89 x 10.6 | flat; use when the measured stack is shorter (Pi bottom z 10) |

Dome 248 of 256 mm on the X1C: **no brim** (skirt only, <= 2 mm), centre it.

## Orientation, supports, overhangs (measured on the final meshes, 45 deg threshold, bed-contact faces excluded)

- **Dome, top on bed** (chosen). Overhang > 45 deg: 15.4k mm2 total (> 60 deg: 10.2k), max 90 deg. 14.0k mm2 of it is the outer shoulder fillet in the first 8 mm of height (the outline grows from the flat top, so the outer edge starts horizontal): **tree supports, build-plate only, on that band** (outside surface; sand the first 8 mm of the shoulder). Rest (~1.4k mm2): four corner-block free ends (R7.3 faces at z 19.9, ~200 mm2 each, reachable from the open rim) and the top arcs of the large wall holes (HDMI D21.5, Ethernet D23.4 D-hole, USB-C bore D16.3: bridge or add a support enforcer; jacks D8.3, TRS D10, USB-A 7x15 and the 1.4 deep spot-faces D30/31 print as-is). Rim-down is worse: 33.8k mm2 with a 200 x 240 mm bridged ceiling.
- **Features already consistent with top-on-bed:** screen bosses + D3 pegs, LiDAR seat pads (grow up from the ceiling, pocket + lips face up in this orientation, sides lean 15 deg; only a 31 mm2 / 0.6 mm-high bridge ring at the window step), jack/USB-A seats, ENC/button/joystick/arcade/FSR holes (vertical through the flat top, no overhang) all grow upward from the bed or are vertical.
- **Cap, floor on bed** (chosen). Overhang > 45 deg: 14.5k mm2 (> 60 deg: 9.3k), all in the first 7 mm: the bottom outer fillet. **Tree supports, build-plate only** there. Bosses (sloped skin), joystick cones, grill diamonds, sled pins, D5.98 head recesses (6 mm bridge) need nothing. Rim-down would be 40.9k mm2 plus a full-floor bridge.
- **Sleds** (fit, z6, z10): 55 mm2 of 0.1 mm pocket underside only; no supports. Dome overhang total unchanged by the LED polish (15 859 mm2 > 45 deg vs 15 857).
- Print the coupon first: the 12 mm seam pieces (cap corner + dome corner) test the M3 stack, insert pocket, seam z = 12.4 and the corner block; stack them rim to rim.

## Suggested Bambu X1C settings (dome / cap / sled)

- PETG (Bambu Generic PETG or equal, 255 C nozzle, 70 C textured plate, no brim, glue stick on smooth plate). PLA is fine for the coupon and for fit checks.
- 0.4 nozzle, 0.20 mm layer ("Standard"), outer wall 60 mm/s.
- **Walls 6** (6 x 0.4 = 2.4 = the skin: it prints solid), top/bottom 5 layers, infill 15 % gyroid (only the corner blocks, bosses and LiDAR pads see it; the previous 100 % PETG spec also works, adds weight and time). Seam position: aligned/back; Arachne wall generator on.
- Supports: tree (organic), **build plate only**, threshold 40 deg, top Z distance 0.2, interface 2 layers, 0.1 interface gap. Enforcers on the large wall-hole tops if sag matters.
- Sleds: same material, walls 3, infill 30 %.
- Ironing off. LED dots D0.9 / 1.3 are 16-gons (v2: three rows, band 3.5 mm tall, hole-to-hole web >= 0.6 mm): drill with a 1.0 mm bit if they close (coupon cell tests this at the real size; if the 0.6 mm webs fuse on the printer, widen with `LED['row_pitch']` and regenerate).

## Hardware implied by the design

| Item | Qty | Where / note |
|---|---|---|
| M3 heat-set insert 4.0 OD x 4.0 | 4 | DOME corner blocks: D3.7 x 4.8 pilots (0.5 lead-in) on the seam face at the 4 corners; melt in flush with the seam |
| M3 x 12 screw (pan/socket, head <= 5.5) | 4 | from the cap bottom (head in the D5.98 recess) through the D3.6 cap bore into the dome inserts (4.0 engagement). M3 x 8 does NOT reach. |
| M3 screw + nut (length ~ 8 for a 2.4 wall + flange) | 6 | 3 USB-A panel extensions, 2 holes each (D3.5, 25 apart); ASSUMED nut seat, none modelled |
| 8 panel pushbutton PBS-110 (D7), 7 rotary encoders KY-040 (D7.5, ENC1-7 per firmware), 1 arcade button 30 mm snap-in (D30.7 hole), 1 KY-023 joystick (D27 hole, on cap supports), 2 FSR402 (6x8 slot), 2 TOF400C VL53L1X (glued/taped into the seat pockets, no screws), 8 panel jacks (M8 class, D8.3 holes), 1 PJ-307 TRS (D10), 1 IP67 USB-C panel (D16.3 + collar), 3 USB-A panel extensions, 1 HDMI coupler (D21.5, nut D30), 1 Ethernet M23 coupler (D24.2 D-hole, flat 10.7 below centre, nut D29) | per the list | parts, not fasteners |
| Screen (Pi touch) | 1 | sits on 4 bosses with D3 pegs; **retention is not modelled** (no screws, no insert bores): glue / tape / M2.5 nuts are your call |
| Pi 5 on sled posts | 1 | D2.4 locating pins, no retention modelled; the stack Pi -> 14 mm GPIO riser -> HAT -> tallest riser -> screen is compressed 0.3 mm between the sled and the screen bosses (that is the retention; the screen has no screws) |
| Tallest riser (HAT top -> screen PCB back) | 1 | ASSUMED 13 mm; this number sets the sled height (sled Pi-bottom z = 17.8 - riser) |
| LED strip | 1 | glued flat on the inner shoulder, LEDs facing the 565 dots (no channel modelled) |
| Teensy, IMU, piezo discs, cables | | no mounts in the design (excluded by decision) |

Part count: dome, cap, 4 Pi legs + coupon. No M2 / M2.5 heat-sets are needed by this design.

## Assembly order

1. Print + test the coupon: every hole size, the LiDAR seat pocket with a real TOF400C (after regenerating the coupon), the seam pieces, one LED cluster.
2. Print cap, sled, dome. Remove tree supports from the shoulder (dome) and bottom edge (cap).
3. Dome: heat-set the 4 M3 inserts into the corner pilots on the seam face (iron straight down, stop flush with the seam face, not proud).
4. Cap: `pi_sled_fit` onto the 4 sled pins (z6 / z10 if the measured stack is shorter), Pi 5 + riser + HAT on the sled, Teensy / IMU on the floor, KY-023 joystick board on its 3 cones + peg.
5. Dome (upside-down on foam): encoders, buttons, arcade, FSR on the ceiling, screen on the 4 bosses, LiDAR boards onto the seats (component side against the pocket floor, sensor housing through the aperture, glue or tape, connector end toward the centre line; no screws), LED strip on the shoulder, jacks, TRS, USB-C panel, USB-A extensions (M3 + nut), HDMI + Ethernet couplers (nuts in the spot-faces).
6. Wire, then lower the dome onto the cap (seam z 12.4), four M3 x 12 from below.

## 2026-10-17 update: free-standing Pi legs replace the sled
- The sled is superseded (old sled files deleted from the print folder; sources remain in the shell folder). The cap floor pins are removed from the cap file (2026-10-18). An already-printed cap with pins still works: the pins sit outside the Pi footprint.
- Use 4 free-standing legs, one per Pi mounting hole: foot D10 (spans the floor grill slots), body D6 up to the Pi underside, then a D2.4 x 2.6 pin (0.4 chamfer) that goes up into the Pi mounting hole (D2.7). Print upright on the foot, no supports.
- Heights (leg = Pi bottom z minus 2.4 floor): `pi_legs_fit` 2.4 (z 4.8, estimate), `fit_plus2` 4.4, `fit_plus4` 6.4, `z10` 7.6.
- The pin pokes 1.0 above the Pi PCB top; if a HAT standoff also uses that hole, the pin may need trimming.
- The stack pushes the screen up against the ceiling, which locks the Pi in place.
