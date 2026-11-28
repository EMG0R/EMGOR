# changes-world3: camera line of sight, big minimal proportions, feel (2026-11-27)

Owned files only: `js/ship-foot.js`, `js/ship-world.js`, `js/ship-village.js`, `js/ship-hub.js`, `js/ship-look.js`, `style/ship.css` (untouched, see below), plus one appended test `gW3` in `js/ship-planet-tests.js`. Nothing committed. Not checked by eye.

## 1. Camera occlusion

Root cause (by reading, not seen): the lens floor was `ground truth + 0.04 L` (0.15 H). With the plate top 0.12 H over the pad plane and the pitch range down to -0.3 (lens ~1 H under the head), the lens sat 0.03 H above the plate, so the plate rim / slab lips cut across the screen near the near plane. The probe pass also used 0.3 H spacing, a 0.15 H margin, and `MIN 0.8 H` (boom could not shorten below 0.8 H, so it could sit inside a wall).

ship-foot.js now:
- `liftLocal`: lens >= floor + 0.4 H, floor = `mFloor` ground truth (pad plane, store / apron slabs) and, over a pad, the plate top (`ps.outposts[i]` disc: `radius`, `pad.position`, `dir`, 0.12 H tall) + 0.4 H. Applied to the wanted lens before clipping and again after the head->lens interpolation.
- `camClip`: probes every 0.15 H along head->lens (objPush capsule = all store / Burger / CVS / village / hull solids) plus `plateHit` (plate disc). Pull-in to first hit minus 0.3 H, instant in, 6 H/s out, `MIN` 0.3 H. Pull-in and floor lift hard-snap the camera (groundCam rate 400); otherwise yaw follow rate 20 (was 16).
- Fade: `ship-look.js` `NEAR_FADE` shared uniform in the softMat fragment shader; ship-foot sets it to 1.2 H each foot frame. Non-up-facing surfaces (walls, ceilings, awnings, hut doors) closer than 1.2 H dissolve to ~15 % coverage with a screen-door dither (store / hut meshes are merged soups, so a per-mesh `material.opacity` would fade a whole building; dither needs no sorting or material flip and restores itself). Floors never fade. Deviation from "material transparent": intentional.
- Indoor: boom 2.2 H, FOV +8, `ROOF_PROBES [2.9, 3.6, 3.9, 4.3]` (old probes topped out at 3.4 H and would never detect a 4.2 H ceiling). Feel: accel 0.14 s, decel 0.10 s.
- `CAMDBG` export (head, lens, lifted) for tests. Hub interior walls are not in the opList (`hasLineOfSight` returns true inside the hub), so the hub is not clipped by this pass.

Test `gW3` (ship-planet-tests.js, appended): land on foot, place on the parking slabs, walk with key events to the door, aisle and counter at pitch -0.25 / 0.3 / 0.9. Per frame: segment head->actual camera vs every 7/11 / Burger / CVS box (exact slab test, no inflation) and the plate disc; camera >= 0.3 H over ground truth. Result: 475 frames, 145 inside the store, 0 violations, min height 0.396 H, 122 lifted frames, indoor FOV seen. Village solids are not covered by the test.

## 2. Proportions (1 H = 1.75 m)

| | before | after |
|---|---|---|
| 7/11 interior | ceiling 2.8 H, door 1.0 x 1.6 H, aisle 1.0 H, counter 0.63 H, 22 x 14 m | ceiling 4.2 H (7.35 m), door 1.4 x 2.4 H, aisle 1.6 H (lanes 3.8 m pitch), counter 0.9 H, 20 x 18 m, 3 gondolas |
| Hut | 6.1 x 5.2 m, ceiling 2.8 H, door 1.03 x 1.63 H | 7.875 x 7.0 m (4.5 x 4 H), ceiling 3.4 H (5.95 m), door 1.3 x 2.2 H, windows 2 x 1.8 m at 3.3 m |
| Hub | corridors 4 m, loot 5 m | corridors + loot 7.2 m (4.1 H); arenas 8 m (4.6 H), boss 14 m unchanged; gate gaps stay 4 m |

Props removed. Store: fridges, slurpee / coffee, hanging slurpee, aisle signs, end caps, beams, rooftop boxes, crates, till clutter, pole sign (tower walls + sign quads), stripes 3 -> 1 accent band. Kept: wall shelves (both walls), 3 gondolas, counter, one facade sign, 3 racks (now against the back wall; the old `racks` formula would have gone undefined with fewer lanes). Store materials: WALL, FLOOR, ACCENT only (brand orange / green / red are gone from the building; the sign texture keeps its own). Hut: bed + chest + table + hanging lamp; stools, shelf, rug, bowl, corner posts, chimney removed; colours WALL / ROOF / WOOD only (floor = roof tone). Hub: corner pillars skipped in the loot room. Burger House / CVS were not touched (not in the task list).

Tri counts (before -> after): 7/11 5658 -> 3124; Burger 3660 -> 3660; village max of 40 seeds 44116 -> 31978 (avg per hut-village 4319 -> 2970 per hut); hub 15702 -> 16014 (taller corridor / loot walls, tiles; 0.1 % over the 16k budget). 40 seeded villages still build 5-9 huts (HUT_R now 25 / 41 m).

Contracts: wall names (`back wallL wallR roof pier* sill* lintelW glass glassR`), `aisles` names (`counter gondola* shelvesL/R`), `floors`, `floorAt`, hub `walls` / `floorAt` / `chests` unchanged in shape. Removed names: `tower`, `fridges`, `slurpee`, `coffee`. Hub `rooms[].box` max y changed for corridors + loot.

## 3. Feel
accel 0.14 s, decel 0.10 s, reversal brake unchanged; yaw follow 16 -> 20. Jetpack untouched.

## 4. HUD css
No change to `style/ship.css`: nothing identifiable as idle chip clutter without seeing it.

## Verify
`node --check` + `acorn --ecma2022 --module` clean on all 5 js files. `bash tools/test-node.sh`: 329 pass / 0 fail (baseline said 445; another agent's working tree has deleted ~8 files under tests/node, e.g. styles-*.test.mjs, not mine). Browser (127.0.0.1:8123): g10 pass, g17 pass, gW3 pass, 0 console errors. g17 and g10 each block the renderer for ~60 s.

## NEEDS ship.js
None required. (`fovAdd` and the `worldInteract` hooks from changes-world.md are already applied.) Optional: expose `get world()` on the foot ctx so the LOS pass can ray the exact boxes instead of capsule probes; the probes are conservative (extra 0.26 H).

## By-eye checklist
1. Pad -> 7/11 at pitch low (mouse up so the camera drops): no plate edge or slab lip slicing the view; camera stays >= 0.4 H over the ground.
2. Walk through the door: ceiling is far above (4.2 H), door is wide, aisles are wide, camera does not pass through the frame; walls near the lens go see-through (dither) and come back.
3. Back against the glass / a gondola: camera snaps in with no clipping, eases out ~0.5 s.
4. Indoor in the 7/11 and a hut: boom shortens to 2.2 H, FOV widens a little, no flicker in the doorway.
5. Hut: big room, bed + chest + table, one lamp; door swings; windows large.
6. Hub corridors / loot room are tall; arenas unchanged.
7. Walk start / stop feels ~0.14 s up / 0.10 s down, camera yaw slightly snappier.
8. Check the dither is not distracting on the door frame while entering; if so lower `CAM.FADE_R` (ship-foot.js) or `0.15` in ship-look.js.
