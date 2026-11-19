# V5 print-ready pass (2026-11-16)

Printer: Bambu X1C, 0.4 nozzle. Material: PETG for pod/mount/door (door latch strain 1.15 percent at 0.8 mm engagement; PLA is marginal for the latch), PETG or PLA for caps. Layer 0.20 mm, 4 walls, 20 percent gyroid (pod/mount), 100 percent for the door arm region (print door with 5 walls), caps 0.12-0.16 mm, 5 walls, 40 percent.

## Audit table (oriented STLs in cad/stl_v5_print/, mm)
| part | manifold, single body | faces | print extents | up axis | support area mm2 | thin (<1.6) clusters |
|---|---|---|---|---|---|---|
| shell_pebble | pass | 250000 | [195.7, 85.3, 177.2] | +x | 11560 | 8 |
| mount_original_arms_plate | pass | 76442 | [138.4, 60.1, 220.6] | +x | 4245 | 8 |
| battery_door | pass | 30000 | [119.2, 52.3, 17.3] | +z | 366 | 8 |
| valve_cap | pass | 40000 | [22.6, 22.6, 11.6] | -z | 123 | 8 |

All four parts fit the 256 mm bed in the chosen orientation. Trapped voids: none in any part. Gap (<0.8 mm) clusters: only the door's intended 0.25 mm latch/recess clearances.

## Orientation and support
- shell_pebble: stands on its -x/+x end (195.7 mm tall), 11.5k mm2 overhang area, support needed under pad, keel roof and rail. Supports will touch the pad underside and the rail; the control face is not a support face. Bridge spans over 10 mm exist (cavity roof, battery bay roof 85 mm): use tree supports, 0.2 mm Z gap.
- mount: stands on end (220 mm), 4.2k mm2 overhang. Fit faces stay support-free by choice of axis (check slicer preview).
- battery_door: flat, plate down, arms up, 366 mm2 support (latch barb ramps), best first-layer contact 5420 mm2.
- valve_cap: open end down or ceiling down; 123 mm2 support, 205 mm2 bed contact, small warp risk.

## Thin-wall findings (violations, not fixed)
Voxel audit at 0.5 mm (pod/mount) and 0.25 mm (door/caps): clusters are listed in cad/print_audit_v5.json. Most are voxel-edge slivers of 2.2 mm walls (<20 mm3 each). Real ones: pod back edge strip near y 541.5-542 along x 897-903 (fillet runout); mount notch floor ledge z 116-117 near y 531-533 (flange tapers to under 1.6); door perimeter lip 0.3-1 mm at plate edges (intended chamfer); cap snap slit edges (intended).

## Hall rail (v5.0)
Board assumed 81 x 20 x 1.6. Pocket 82 x 21 x 8.0 (generous, z 120.9-128.9). Loads from the pod interior through the open +y side of the pocket; -x end closed, no exterior slot, push slot removed. Sensor holes 9 x 7 mm through the valve-facing wall, centre z 123.9. Trellis PCB top (assumed 120.3) to board path: 0.6 mm clearance after raising the slit 1.8 mm (sensor top now sits about 0.8 above the full-press magnet plane; sensor is 12.3 mm lateral, no collision). Cavity depth behind the face 20.0 mm: with the wall open, the board starts at y 545-565 and slides -y, margin 1.0 mm to the face wall. Rail underside re-derived from the mount notch floor (116.4): rail bottom 116.6, gap 0.2, mount/shell intersection volume 0.0 mm3.

## Battery housing, switch, door (v5.0)
- Bay clear 111.9 x 53 x 30.4. Battery test box 85 x 50 x 30 at x 805-890, y 506.7-556.7: intersection with shell 0.0 mm3, with door 0.0 mm3.
- Switch zone x 781-805 (24 mm deep) x 53 x 30, overlap with battery box 0. MTS-102 hole 6.3 mm at y 531.7, z -34.0, wall at hole 2.5 mm (inner 14 mm spot-face), 12 mm flat land on the face.
- Wire pass-through 25 x 15 stadium, 1.2 mm rounded lips, into the pod cavity.
- Door: perimeter clearance 0.25; hook tab+nose (-x), single flex latch (+x) 14 mm long x 1.5 mm thick x 14 wide, 0.8 mm engagement, strain 1.15 percent; thumb lip tongue follows the housing curve, proud up to 1.2 mm at the +x end. Door x shell 0.21 mm3 numeric contact (hook nose vs recess), not a collision of consequence.
- Not tested by physical assembly: hook-and-rotate install path; barb retention force.

## Openings audit (all intended)
Trellis 16 (11 x 11), 3 fader slots (5.6 x 41), lidar stadium, 4 round holes + joystick 29 x 28, USB-C 7.5 x 13, 3.5 mm jack 6.3, switch 6.3, 3 Hall holes (9 x 7), battery bay (door), wire stadium (internal). No unintended exterior openings after removing the old Hall push slot.

## Fits
Valve cap bore 18.2 vs 18.0 button (0.1 radial), magnet pocket 10.4 x 3.3, dovetail clearance per DT table (unchanged), door perimeter 0.25, latch 0.8.

## Open / to caliper
Battery holder/cell dimensions (now only a 85 x 50 x 30 envelope), button height 5.5, valve travel 13, Trellis PCB position (assumed centred on the key grid, top z ~120.3), Hall board width (assumed 20.0), casing OD 19.2, mount smoothing fit deviation (see below).

## Not fixed
Pad/keel crease at the pad top-left still visible in the 3/4 render; mount fit deviation vs v1 not re-measured after final smoothing; pod stands 196 mm tall in print (tall but within bed); thin ledge at mount notch edge.


## V5.1 smoothing pass (2026-11-16)
- Battery housing: 8 mm outer radii all round (bay + 8 mm wall; flat bottom = door opening so the door stays flush), +1.8 mm convex crown on the side faces, 22 mm smooth-union fillet into the pod below z -6 (no blend near the flat control face: face y 568.36-568.44), 2.6 mm volumetric blur of everything below z -14 (kills the pad/housing crease), opening r 0.75 over the whole shell (convex edges rounded >= 0.75, thin slivers removed). The flat top shelf is gone; a soft ridge still shows where pad and housing meet.
- Mount: variable smoothing. Exact within 2 mm of the horn, sigma 1 mm elsewhere. Exact point-to-surface deviation of horn-fit points vs v1: max 0.171, p99 0.074, median 0.011 mm, 99.6 percent within 0.1 mm. (The 0.4-0.7 mm figures reported earlier were sample-spacing artefacts of the metric.) Notch-floor thin ledge removed by local opening (r 0.85).
- Shell back-edge sliver (x 897-903, y 541.5-542) removed by the global opening. Remaining thin clusters in the shell are 9-12 mm3 strips at y 546.5, z 129.5 (rail pocket roof, x 815-881, about 1.4 mm wall) and small voxel-edge slivers. SL_WALL raised to 2.0.
- Door: arm thickness 1.6 (strain 1.37 percent at 1.0 mm tip deflection), lip ramp moved so the plate bottom is the first layer, intersection with shell 0.0 mm3, box 85 x 50 x 30 overlap 0.0 mm3, switch wall 2.5 mm.
- Fillet sweep: shell done by the opening; door and caps not swept (thin latch/snap features by design); mount edges rounded by smoothing.
- Imperfect: shell chord error p99 0.073 mm (250k face budget; 0.05 needs about 350k), door 0.086 mm; hand-rest mirror difference 0.86 mm; a faint ridge at the pad/housing junction in the hand-rest 3/4 render; a slightly faceted slope on the housing's +x lower side; the 1.4-1.6 mm rail pocket roof strips.


## V5.2 (2026-11-16)
- Top skin: the earlier slot in the top of the pod was the roof being clipped by the top of the build grid (z 138) and then removed by the thin-wall opening. Grid now goes to z 148 and a continuous 2.3 mm skin follows the whole outer surface. Ray check from above: no see-through anywhere.
- Hall pocket: end walls were 1.2 mm and got eaten (pocket open at both ends). Rail now x +-43.6 (2.6 mm end walls), pocket roof skin 2.3 mm, one continuous slab-to-roof wall replaces the four webs.
- Genus: expected 31 intended openings (+1 for the dovetail channel end that opens into the cavity, which is the sliding assembly path); actual 33. One extra tunnel could not be located; ablations of webs, plate carve, slab, rib, door rebate, groove and notches did not change it. It stays open.
- Switch: 6.3 mm at y 531.7, z -34.0 = centre of the housing -x face bbox. Ray from x 700 along +x passes the hole and the whole bay (first hit x 892.9). Wall at the hole 2.5 mm (x 771.0-773.5). 22 x 14 x 10 zone behind it: 0.1 mm3 against the shell (corner), 0 against the battery box.
- Door: clam cover, parting line at z -35.5 on the side band, lip 1.5 mm, bead 0.7 mm in a 0.8 mm groove (continuous), strain about 1.1 percent (cantilever estimate, 12 mm free length), 3 nail notches 8 x 1.2 mm on the -y side, 0.2 mm gap, lip scalloped around the switch land. No hook, no arm. Battery 85 x 50 x 30 box: 0 mm3 against door and shell. Door faces 100k.
- Palm dip: about 55 x 40 mm, 2-4 mm deep, centred x 837, z -36, solid body behind (min wall 20.7 mm).
- Logo (V-carved, mirrored for reading from +y): block 48.4 x 14.4 mm, big caps 4.9 mm, small 2.3 mm, on the control face lower-left over the Seed housing (margin 6 mm to openings and edges), depth 1.35 mm (small 1.1), stroke 1.0 / 0.8 mm at the surface, local wall thickened inside, 3.55 mm left under the deepest point. Genus unchanged (33), watertight, single body, 236.7k faces. EMGOR (small line) is crowded at 2.3 mm cap height.
- Open: shell chord p99 0.097 mm; door lip 1.5 mm is under the 1.6 mm floor by design (flex lip); thin strips 39-49 mm3 in the shell (bay wall near the bottom, pocket roof near z 127); mount fit surfaces not re-measured after the rail length change (mount x shell 0 mm3).


## V5.3 (2026-11-16)
Source of truth is still `cad/hypertrumpet_v5.blend` (shell and door replaced, snapshot `cad/snapshots/hypertrumpet_v5_3.blend`, pre-change copy `cad/snapshots/hypertrumpet_v5_2_pre53.blend`). Sources: `cad/hypertrumpet_v5_3_fixes.py` (driver) + `cad/v5_3_src/` (one script per stage). STLs in `cad/stl_v5_3/`, oriented print STLs in `cad/stl_v5_print/` (v5.2 ones archived in `cad/_attic/stl_v5_print_v5_2/`), renders in `cad/renders_v5_3/`, audit block `_v5_3` in `cad/print_audit_v5.json`, genus evidence `cad/genus_v5_3.json`, fit re-run `cad/component_fit_v5_3.json`.

**Build method.** The housing (z < -9) was rebuilt from a 0.25 mm voxel union of shell + door (gaps closed), cut at a level plane, smoothed (Gaussian 0.75 mm, Taubin), meshed with marching cubes, decimated in Blender and spliced onto the untouched pod with manifold3d. Everything else is exact mesh booleans. Face counts: shell 224,074 (budget 250k), door 84,220 (100k), caps 40k each. The mount is 102,442 faces (unchanged, above the 76k budget it already exceeded in v5.2). Shell genus 33, door genus 0, all parts watertight and single-body. Shell x door, shell x mount, door x mount, caps x all, battery box 85 x 50 x 30 (x 805-890, y 506.7-556.7, z -48.9..-18.9) x shell/door: 0.0 mm3.

**Logo.** New glyphs: blocky angular caps, each stroke a convex polygon whose V-roof is the exact straight-skeleton roof, mitred joints, chisel-pointed free ends, union of roofs = no joint spikes. P has a closed bowl, O is a closed octagon ring, G is the same with an opening and an inward bar, R has bowl + leg. Block 70.0 x 20.6 mm (v5.2: 48.4 x 14.4), centre x 787.5 z 21.7 on the control face, big caps 7.11 mm (stroke 1.0, depth 1.2), small caps 3.56 mm (stroke 0.9, depth 0.85), >= 5.0 mm margin to the openings and the face edge (flat-face map at 0.5 mm; the 72 x 22 target does not fit at 5 mm). Walls 24-30 degrees, 2.2 mm of wall under the deepest point (new inside boss to y 565.0). Genus unchanged (33). Renders: `v5_3_logo_straight.png`, `v5_3_logo_raking.png`; THE / HYPERTRUMPET / EMGOR all readable.

**Palm dip.** 7.5 mm carve (measured 7.6 mm against the undisturbed surface), 62 x 41 mm superellipse, C2 smootherstep blend, along the hand-rest face normal, centred x 837. Minimum wall from the dip surface to the battery cavity 11.4 mm. No housing growth needed.

**Top edge.** Variable-radius opening of the housing envelope (radius 8 mm at the bottom ramping to 12 mm from z -38 up), removed 13.2k mm3, with a 2.3 mm guard around the battery cavity: full radius on the hand-rest side, clipped to about 10 mm where the bay wall is thin. The pod's own top edge was not touched.

**Battery door.** One level parting plane at z = -44.0 all the way round, hand rest included (door = bottom slice of the whole housing, tub with the same cavity; door top at z -44.2 = 0.2 mm gap). Switch land (hole z -34 +-3.15, spot-face 16 mm to z -42) sits 2.0 mm above the plane at the nearest point, 4 mm above the 12 mm land. Snap: 1.5 mm lip, 6 mm tall, bead 0.5 mm on its inner face, 0.2 mm gap in a groove and bead pocket in the shell (outer skin above the groove 1.8 mm), ring omitted over the switch zone (x < 800, y 517-547), 3 nail notches 8 x 1.2 mm at x 805 / 837 / 869 on the -y side. Floor skin 1.97-2.44 mm (v5.2: 1.4). Strain of the 6 mm lip was not re-derived, bead sized down to 0.5 for that reason; not tested on a print.

**FSR402 pad (bell side, +x).** Disc diameter 29.0, centre x 908.5, z 112.8 (top Trellis row), face coplanar with the control face (y 568.4), 4 mm root disc plus a hulled root block into the +x side wall (x 889-903) and two 3 mm gusset ribs, 1 mm rim chamfer, 0.6 mm deep dia 19 locating recess, 0.6 mm deep 7.5 mm wide tail groove from the disc centre to x 893 (the pod edge). No wire hole. Pad body to mount: 5.6 mm minimum. Pad centre x 908.5 > Trellis max x 881.2 (Trellis right edge to the pod +x edge 13 mm, to the -x edge 28 mm, so +x is the nearer edge). Printable without support on the visible face in the +x-up orientation (the disc sits on the top end); not checked in a slicer.

**Fit fixes.** Seed lobe corner: outward bump (+3.4 mm max, below the USB mouth) plus a 1.0 mm clearance cut around the Seed corner. The re-run audit (see below) still lists header rows touching the back rim by 0.5 mm (114 mm3), which was not in the pre-v5.2 audit. Toggle: spot-face 14 -> 16 mm, collision 0 mm3 (exact clearance 0.0). Jack: NOT fixed; the only geometry fix is relief in the dovetail rib, which is locked. Hall board and NeoTrellis insertion paths: not done (audit still reports both blocked). Tight spots (KY-040 hole 2, BMS, holders): not touched. The fit_audit re-run (`_fit_audit_work_v5_3/fit_audit.py`) reports pod cavity 1.84 million mm3 (v5.2 audit 0.5 million), so its interior flood leaks on the new door geometry and its install sweeps and panel-part collisions (0.1 mm skin difference) are not trustworthy; treat `component_fit_v5_3.json` as unverified.

**Genus.** 33 is correct, not an extra tunnel. Openings into the one air region: 25 control-face (16 Trellis + 3 fader + lidar + 4 round + joystick), 3 Hall, USB-C, jack, switch, battery bay, dovetail channel, plus the open back (the mount plate closes it in assembly) = 34, genus 33. The v5.2 expectation of 32 omitted the open back. Plug test (`genus_v5_3.json`): after plugging the 25 face openings genus 8, plus the back 7, plus 3 Hall 4, remaining 5 intended openings. Shell genus stays 33 after all v5.3 edits.

**Thin walls.** Door floor 1.4 -> >= 1.97 mm. The v5.2 bay-wall strip (48.9 mm3) is gone with the new split. Still open in the shell: pocket-roof strip 39.6 mm3 (x 810-890, y 545.3-546.3, z 127-127.5), back-rim strips beside the dovetail 55 / 28 mm3, small slivers (<= 18 mm3). Door lip ends (<= 14 mm3) and groove-end slivers are by design.

**Open / not verified.** FSR pad slicer check, snap fit by print, jack clearance (locked), Hall and Trellis install paths, fit_audit re-run reliability, hand-rest surface is still slightly wavy (voxel pipeline). The door printed on its side per the audit's support score; a tub-up orientation (+z) needs 9.1k mm2 support.


## V5.4 surface-quality rebuild (2026-11-16)
Why: v5.3 stacked voxel blur, morphological ops and local patches; the housing, hand rest and door were lumpy and the pod/housing seam crumbly. Sources in `cad/v5_4_src/` (`rebuild.sh` chains `stageB.py`, Blender decimation, `finish.py`), snapshot `cad/snapshots/hypertrumpet_v5_4.blend` (pre-change: `hypertrumpet_v5_3_final_pre54.blend`), STLs `cad/stl_v5_4/` and oriented `cad/stl_v5_print/` (v5.3 copies in `cad/_attic/stl_v5_print_v5_3/`), audit block `_v5_4` in `cad/print_audit_v5.json`, renders `cad/renders_v5_4/`.

**Housing, hand rest, door = one analytic field** (`fieldA.py`), meshed once with marching cubes at 0.3 mm and no post-blur or smoothing: rounded body from smooth-max of planes (vertical edges R11, bottom R9, top R10 to R14 on the hand-rest side, radius changes smoothly), tilted hand-rest face, bay floor as a cubic spline of the measured floor so the door skin follows it (2.0 mm), thin-wall pod ring (morphed between measured sections at z -8.5 / -7 / -5 / -3.7) joined to the housing top with smooth-min k = 12 (concave R about 12 into the pod, no shelf). Cavities are analytic: bay x 781-892.9, y 505.2-558.2, roof z -18.7 (with a 2.3 mm wall guard), switch hole 6.3 mm and 16 mm spot-face at y 531.7 z -34 (flat land, no crater), 25 x 15 wire stadium. The shell above z = -5 is the unchanged v5.2 pod. Seam at z = -5 coincides with a section node; the old crumbly ridge and the +x-end bar are gone (a radius step at y = 560 that caused a crease was replaced by a smooth blend).

**Saddle.** Cosine dip along the length: 103 mm of the 132 mm face (78 percent), centre x 837, depth 4.6 mm measured at z -40 (5.0 at the centre), minimum radius of curvature along x about 100 mm.

**Door.** Same level parting plane z = -44.0, now cut from the same field: door top z -44.2 (0.2 mm gap), 1.5 mm lip 6 mm tall and 0.5 mm bead as field offsets of the outline, groove and bead pocket in the shell (outer skin 1.8 mm), ring omitted over the switch zone, 3 nail notches 8 x 1.2 mm. Floor skin 2.0 (min 2.3 measured vertically). Door 85,000 faces genus 0; shell 223,016 faces genus 33, both watertight single body. Intersections: shell x door 0.004 mm3 (numerical contact), mount x shell 0, mount x door 0, caps 0, battery box 85 x 50 x 30 x shell/door 0.

**Logo.** Same text, size (70.0 x 20.6 mm) and placement. The control face is planarised (vertices within 0.12 mm of y 568.4 snapped), a window around the logo is replaced by a 0.4 mm uniform-triangle slab (inside boss to y 564.9, wall under the deepest point 2.3 mm), then V-carved. Flat-face vertices y 568.4000-568.4003, face normal deviation 0.22 degrees max outside the letters (limit 0.5). Normals: Blender split normals (sharp edges above 35 degrees) so the flat face and the facets shade clean; straight-on and raking renders show no streaks. Edge length is 0.4 mm, not 0.25, to stay in the face budget.

**FSR402 pad.** Rebuilt as one SDF: flat circular face dia 29.0 (y 568.4, ~230 segments), 1.5 mm rim round-over, conical root flare and two blended gussets into the pod side wall, no recess and no tail groove. Pad centre x 908.5 > Trellis max x 881.2. Mount clearance was 5.6 mm in v5.3 (same placement, not re-measured).

**Seed corner clearance and toggle.** Toggle spot-face 16 mm is now in the field. The v5.3 outward bump for the Seed corner was removed because it read as a lump; the Seed lobe corner clearance (1.4 mm clip) is therefore OPEN again.

**Open.** Thin strips: pocket-roof 39.5 mm3, back-rim strips beside the dovetail 54 mm3 each, wire-stadium rim 23.5 mm3; door lip ends and notch edges by design. Fit audit not re-run (unreliable since v5.3). A small chip remains where the pad rim meets the old pod roll corner. The door is printed on its side by the audit's support score; tub-up needs about 9k mm2 support. No slicer or snap test.


## V5.5 (2026-11-16)
Sources `cad/v5_5_src/`, snapshot `cad/snapshots/hypertrumpet_v5_5.blend` (pre-change `hypertrumpet_v5_4_final_pre55.blend`), STLs `cad/stl_v5_5/` and oriented `cad/stl_v5_print/` (v5.4 copies in `cad/_attic/stl_v5_print_v5_4/`), audit block `_v5_5`, renders `cad/renders_v5_5/`. Shell 223,502 faces, door 85,000, both watertight single-body, shell genus 33, door genus 0, shell x door 0.004 mm3 (numerical), mount x shell/door 0, battery box 85 x 50 x 30 clear.

**FSR lobe (flush).** The pad cylinder is gone. `lobe.py` grows the pod's control-face outline with a circular ear (r 15, centre x 907, z 112.8, top Trellis row): plan-view smooth union of the circle and the pod edge band (k 12, concave fillets about r 10 at the top and bottom junctions), the roll-off and the 3D shape are functions of that plan inside-distance, so the face is exactly coplanar with the control face (lobe face vertices y 568.4 with 0.02 mm spread, no step, no rim line, no recess). The ear thickens toward the pod (3 mm at the rim, 1 mm more per mm inboard) so its back blends into the pod side wall with no visible cylinder side and no gussets. Deviations: roll-off on the ear is 5-6 mm wide, 5-7.5 mm drop (pod edge is 8 x 12) so a flat dia 20 mm remains for the 18.3 mm FSR head; r is 15 not 14.5. Faint ribbing remains on the rim from decimation, and a small crease where the ear top meets the pod's rolled top corner.

**Font.** Sheeping Cats by Chequered Ink, downloaded from dafont (personal-use licence): `cad/fonts/Sheeping Cats.otf` (+ Straight variant and `FSLA_NonCommercial_License.html`). Text outlines come from Blender's font object (`bl_text.py`, flattened at 24 steps) and are cut as a flat-floor engrave, 1.2 mm deep, straight walls, flat floor (y 567.2), in the same planarised face patch with a 0.4 mm uniform slab and split normals. Same three lines, same centre (787.5, 21.7), same size relationship (small line cap = half the big line). The font is wider and taller per letter than the v5.4 caps, so the block is 60.5 x 21.0 mm (big cap 7.3, small 3.6) to fit the same margin; wall under the floor 2.3 mm. The heavy italic spikes read at straight-on but the small THE / EMGOR lines are tight.

**Mouth in the bottom: the dovetail channel's lower end.** The pod's thin-wall ring has a gap in its back wall at x 868-884 (the channel footprint); extruded down to the housing top in v5.4 it left a large open mouth into the pod cavity beside the mount plate. Now capped with a rounded block x 866.5-891.8, y 540.4-550.5, z down to the housing roof, top at z -5 (the channel floor, the mount's stop; the mount enters from the top). Mount x shell is still 0. Genus stays 33: the intended openings are unchanged at 34 (25 face + open back + 3 Hall + USB + jack + switch + bay + dovetail channel).

**Wire hole.** It was not lost: the 25 x 15 stadium (x 837.5-862.5, y 543.5-558.5, centre x 850) goes through the housing roof inside the pod. Ray checks: a vertical ray from the bay at (850, 551, -30) passes through and reaches the pod roof (z 137.8); a column scan of the roof finds a through-footprint x 838-862, y 545-558. It sits behind the face plate and can only be seen from the open back. The flat "recessed panel" was the housing top inside the pod ring seen through the mouth; with the mouth capped the top flows into the junction fillet.

**Open.** Seed lobe corner clearance, thin strips (pocket roof, back-rim beside the dovetail), fit audit not re-run, no slicer/snap test.


## V5.6 (2026-11-16)
Sources `cad/v5_6_src/`, snapshot `cad/snapshots/hypertrumpet_v5_6.blend` (pre-change `hypertrumpet_v5_5_final_pre56.blend`), STLs `cad/stl_v5_6/` and `cad/stl_v5_print/` (v5.5 copies archived in `cad/_attic/stl_v5_print_v5_5/`), audit block `_v5_6`, renders `cad/renders_v5_6/`. Shell 234,404 faces genus 33, door 85,000 genus 0, watertight single-body; shell x door 0.004 mm3, mount x shell/door 0, battery box clear.

**Logo.** Only HYPERTRUMPET is Sheeping Cats (50.0 mm wide, cap height 6.0 mm, 17 percent narrower than the 60.5 mm of v5.5). THE and EMGOR are the custom angular glyphs (3.2 mm caps, 0.84 mm strokes) merged to single outlines and cut in the same flat-floor 1.2 mm engrave as the big line (straight walls, flat floor, flat 0.4 mm slab, split normals). Block 50.0 x 20.6 mm, centred x 787.5, z 21.7; line gap 4.2 mm each side. Judged on the straight-on render.

**FSR lobe.** Rebuilt in one field: plan-view smooth union (k 12) of a circle (r 17.5, centre x 908.5, z 111) with the pod's own face-edge outline including its rolled top corner, and the same 8 x 12 roll-off profile as the pod edge applied to that plan distance, so the side wall is the pod's rolled edge, not a cylinder. The flat keeps dia 19 or more (radius 17.5 minus 8). The ear thins to 1 mm at the rim and thickens inboard so there is no side wall. The lobe mesh is Taubin-smoothed before decimation, which removed the rim ribbing; the crease at the top where it met the pod roll is gone. Lobe face vertices y 568.4 with 0.02 mm spread.

**Trellis openings.** All 16 openings were sectioned at y 567.3: every one is a clean 11.0 x 11.1 rectangle (area 121.1-121.4 mm2, identical to v5.2). The L-shapes and curved sliver seen in the render are interior geometry behind the holes (rail, keypad and mount features), not damaged hole walls.

**Open.** Seed lobe corner clearance, thin strips, fit audit, slicer and snap tests.


## V5.7 final (2026-11-16)
Sources `cad/v5_7_src/`, snapshot `cad/snapshots/hypertrumpet_v5_7.blend` (pre-change `hypertrumpet_v5_6_final_pre57.blend`), STLs `cad/stl_v5_7/` and `cad/stl_v5_print/` (v5.6 copies archived in `cad/_attic/stl_v5_print_v5_6/`), audit block `_v5_7`, renders `cad/renders_v5_7/`. Shell 361,952 faces (budget 400k), door 110,000 (120k), mount 102,442 (110k, unchanged); watertight, single-body; shell genus 33, door genus 0; shell x door 0.004 mm3 (numerical), mount x shell/door 0, battery box clear.

**The big hole was the underside of the -x pod lobe, not the dovetail channel.** Rays from the +x end scan only the jack hole; rays from below over the lobe (x 735-775, y 538-570) went straight into the cavity (the ring's -x wall ended at z -9 with nothing under the Seed lobe, 15 x 20 mm). v5.7 adds a 3 mm plate (z -9..-6, x below 772.3) from the filled z -8.5 outline of the ring, smooth-min k 3 into the ring walls. Re-scan: every first hit over the lobe is the plate (z about -9), no cell opens into the cavity; end-face scans on both ends show only the USB and jack. The round hole on the housing -x face is the switch hole (intended). Genus stays 33: that opening was contiguous with the open back, so the intended count is unchanged at 34 (25 face + back + 3 Hall + USB + jack + switch + bay + dovetail channel).

**Wire hole** is now 30 x 14 mm (the space between the pod back wall and the bay edge allows 14, not 18): x 822-852 (centre on the housing, x 837), y 545-559 toward +y, through the housing roof from the bay into the pod cavity. Ray test: vertical rays from the bay at (837,552) and (850,555) pass through to the pod roof (z 137.5). The pod back wall is closed (the only back mouth was capped in v5.5), so the hole cannot be seen by looking in from the back; the views are `v5_7_wire_hole_bay_up_door_hidden*.png` (looking up into the bay) and `v5_7_wire_hole_cavity_down_cutaway*.png` (shell cut at z -2, looking down at the pod floor).

**Faces.** Upper pod 110k, housing 130k, lobe 45k, logo/text patch about 90k, door 110k. No decimation of the flat interior was needed beyond that.

**Text** is 20 percent bigger (HYPERTRUMPET in Sheeping Cats 60 mm wide, THE / EMGOR custom glyphs 3.84 mm caps), block 60.0 x 24.9 mm, centred at x 789.0, z 17.85 (v5.6: 787.5, 21.7: 1.5 mm to the right and 3.9 mm down as seen from +y). Centre from the free region (flat face distance map with the openings, slot ends and face edge as obstacles): the largest-margin position for the block has side margins 8.5 / 7.5 / 8.0 / 8.3 mm (all at least 4 mm). Flat floor 1.2 mm.

**FSR ear** is now D-shaped: straight top and bottom edges (z 93.5 and 128.5) run tangent into the pod outline, only the bell end is round (r 17.5, end centre x 908.5, z 111), same 8 x 12 roll-off all round, flat dia 19 or more, face coplanar. The lobe's inner cut follows the pod's side wall by height, which also removed a handle that appeared when the straight edges were first added. A small sharp corner remains where the top edge meets the pod's rolled top corner.

**Open.** Seed lobe corner clearance, thin strips, fit audit, slicer and snap tests.


## V5.8 (2026-11-16)
Sources `cad/v5_8_src/`, snapshot `cad/snapshots/hypertrumpet_v5_8.blend` (pre-change `hypertrumpet_v5_7_final_pre58.blend`), STLs `cad/stl_v5_8/` (shell, door, and the mount WITH rods) and `cad/stl_v5_print/` (v5.7 copies in `cad/_attic/stl_v5_print_v5_7/`), audit block `_v5_8`, renders `cad/renders_v5_8/`. Shell 392,950 faces (400k), door 110,000 (120k), mount 107,460 (110k); all watertight and single-body; shell genus 38, door 0, mount 0. Shell x door 0.004 mm3 (numerical), mount+rods x shell 0.0, door x mount 0.0, battery box 85 x 50 x 30 clear.

**Lobe underside.** The flat 3 mm plate stays as the skin (>= 2 mm) but its right-angle shelf is gone: a closing (radius 10 mm) of the whole region under the lobe adds a smooth concave skin from the lobe bottom into the housing and wall below (1,356 mm3 added, only below z -8.9). The banding on the lobe end wall was reduced by masked Taubin smoothing (70 iterations, USB hole edges excluded); some roughness remains where the old pod mesh is coarse.

**Text** moved 2 mm right and 2 mm up (centre x 787.0, z 19.85); size and fonts unchanged.

**Pod back fully open.** The thin back wall (y 541-544, x 745-897) is removed; a rim stays on the side walls (cut starts 3 mm inside the sloped side wall, so rim = wall 2.2 + 3 mm), the floor (housing top / lobe plate) and the roof (above z 117.5); the dovetail rib and channel (x 855-897) and the Hall rail pocket (z >= 116) stay. v5.5's channel-bottom cap block is kept (mount stop).

**Genus 38 (v5.7: 33).** Recount: faces 25, Hall 3, USB, jack, switch, bay door, dovetail channel and the open back are the openings; the open back is now one large window plus the channel and +x end windows instead of one small mouth, which raises the count of independent paths; the second bay-to-pod wire hole (big hole) adds one handle. A ray scan from behind finds the main back window (x 753-854, z -13..117) and four smaller ones (the 3 Hall sensor windows and a +x end window x 897-904, z 117-128). I did not reconcile every handle individually; the measured genus is 38 and the shell is a single watertight body.

**Battery wire hole.** 40 x 24 mm (x 797-837, y 524-548) rounded, 1.5 mm edge radius, in the housing top below the valves/mount, from the bay up into the pod back; the 30 x 14 hole (x 822-852, y 545-559) is kept and the two merge in the roof. Wall around the bay stays >= 2.3 mm. 40 x 24 is as large as fits between the bay roof edge and the pod floor (a 35 x 20 minimum was asked). Render `v5_8_back_open_pod_big_hole.png` looks down at it from behind.

**Rods and sockets (like v1).** v1 (`_mount_v3.stl`, `_showHOLE_v3.2.stl`) joined mount and pod with small printed pegs/tabs on the mount seating in holes in the shell. v5.8 adds four dia 5.0 mm rods to the mount (tip chamfer 0.5, root fillet r 2.0-3.0 into the mount plate, rod length from the mount face to y 549.2) and four dia 5.4 mm sockets (0.2 mm clearance, 0.5 mm lead-in chamfer, 8.2 mm deep from the rim plane y 541.6) in dia 11 mm bosses on the pod rim. Positions (x, z) and mount face y: R1 (738, 72, y 536.8) on the -x side wall (bar back to the wall), R2 (892.5, 111, y 538.4) at the +x top beside the dovetail rib, R3 (859, -8.5, y 539.3) on the housing top between the wire hole and the channel cap, R4 (894.5, 45, y 536.5) in the +x rib/wall. Min rod-to-socket gap 0.2 mm. Nearest component boxes (placement assumptions): faders 4.0 mm (R3), wire hole 3.9 mm (R3), Hall board 4.7 mm (R2), Trellis 7.3 mm (R2), KY-040 9.7 mm (R1), joystick 24.5 mm, jack 16.2 mm, Seed and USB over 50 mm. Dovetail kept. No crush rib. Assembled check: mount+rods x shell = 0.0 mm3.

**Open.** Seed lobe corner clearance, thin strips, fit audit, slicer and snap/rod fit tests (the 0.2 mm socket clearance is untested on a print).


## V5.9 (2026-11-16)
Sources `cad/v5_9_src/`, driver `cad/hypertrumpet_v5_9_fixes.py`, snapshot `cad/snapshots/hypertrumpet_v5_9.blend` (pre-change = `hypertrumpet_v5_8.blend`), STLs `cad/stl_v5_9/` and oriented `cad/stl_v5_print/` (v5.8 copies in `cad/_attic/stl_v5_print_v5_8/`), audit block `_v5_9`, renders `cad/renders_v5_9/`. Shell 388,348 faces, door 110,000, mount 107,562; all watertight, single body; shell genus 34, door 0, mount 0. Intersections: mount x shell 0.0, mount x door 0.0, shell x door 0.004 mm3 (numerical contact, as before).

**-x lobe, sweep, junction: regenerated, not patched.** The old lobe end, plate/shelf, strut, v5.8 closing fill and the pod-floor ramp were cut out and rebuilt from ONE analytic field (`stage2_lobe.py`, marching cubes at 0.125 mm, decimated once with the rest of the shell). Terms: the measured pod sweep profile (outer 10.5 x 15 mm face roll, 2.3 wall, 2.72 face plate, 5.3 x 2.4 rim foot; identical at z 60..100) swept along the measured -x outline x_o(z) with perpendicular plan distance; a tilted, rounded underside (tilt 0.35, convex round ~R10) smooth-min'd (k 12) into the analytic housing body of fieldA (x 771, top -16.4, R10/R14), so the underside leaves the lobe wall, rolls under and sweeps in one S curve (convex ~R10 then concave ~R12-15) into the housing wall, reaching x = 771.0 exactly at z = -35 (seam in a flat wall). The fill is clipped at the open-back plane y = 541.6 with a r 0.95 edge round; the cavity (floor -5.93, wall 2.3 thickening to 2.75 below z 72, so skin >= 2.5 except the 2.4 rim foot) blends over x 772.3..782 into the measured housing cavity sweep; the old floor spurs/hump are gone. USB-C stadium 7.5 x 13 at (y 555, z 6.25) measured from the v5.8 mesh and re-cut unchanged. Seams: a curve 14 mm inside the outline (flat face plate, coplanar, face ripple matched), the z = 100 plane (profile identical to the old pod), z = 2 / x = 782 planes. R1 rod boss/socket re-added from `rods.py`.
**Torn hole closed:** a 15 x 4 mm wedge through the face plate above the junction (x 764..784, z -3.3..1.3) was part of the old mess; the new region covers it, a small prism patch closes the last 2 mm.
**Dovetail removed.** Slab (rib, channel, recessed panel, stop block) and the cap remnant behind the rim plane are cut away, the floor restored at the measured housing top (-16.4), the +x wall inner surface continued straight from the measured arc; foot (y < 544) of the +x wall, rod bosses R2-R4 and the roof foot kept. Mount: dovetail tab removed, plate back restored (fork agent, `v5_9_src/mount_dovetail.py`; 0.04 mm buried fill, rods untouched).
**Rim.** The regenerated lobe rim is r 0.95 from the field; the old rim (top, +x, bosses) got r 1.0 swept corner removers on its convex rim-plane edges (rod socket mouths and concave edges skipped). Back-floor spurs removed (floor flat at -16.4 behind the rim plane).
**Genus 34 (was 38).** 25 face-plate holes (16 Trellis, joystick, 2 pot, 2 encoder, 3 fader slots, 1 lidar slot), USB-C, jack (+x end, 6 mm), switch hole, 3 Hall windows (8.8 x 5.6), wire path (the 40 x 24 and 30 x 14 holes merge into one), battery door opening, open back. That is 33 independent openings; 1 further handle is NOT yet attributed (see V5.10).
**Open.** Old pod above z 100 and its top roll still show faint decimation streaks under raking light; one face hole (x 753.5, z 41.7) has a chipped edge; logo and everything else unchanged.


## V5.10 audit pass (2026-11-16)
Sources `cad/v5_9_src/stage5_hole.py` (+ `shots8.py` render set), snapshot `cad/snapshots/hypertrumpet_v5_10.blend`, STLs `cad/stl_v5_10/` and oriented `cad/stl_v5_print/` (v5.9 copies in `cad/_attic/stl_v5_print_v5_9/`), audit block `_v5_10`, renders `cad/renders_v5_10/` (front, back, top, bottom, -x end, +x end, two 3/4, glossy, low raking key). Shell 391,214 faces, door 110,000, mount 107,562; watertight, single body; genus 34 / 0 / 0; mount x shell 0.0, mount x door 0.0, shell x door 0.004 mm3.
Change in this pass: the pot hole at (753.7, 41.7) had a chipped, ragged edge (area 50 vs 56.5 mm2 of its twins). Rebuilt as a plate disc + revolved cutter (r 4.25 bore, R 0.75 lips on both faces, measured on the good twin); all four round holes now 56.5 mm2.

| Spec line | Result | Evidence / what is open |
|---|---|---|
| Ultra smooth surfaces, v5.4 housing quality | PARTIAL | -x lobe, sweep, junction, back rim, slab area regenerated from fields: clean in all 8 renders. NOT regenerated (still v5.2/5.8 meshes): pod top-edge roll and Hall hood, +x end wall (step at the foot near z 60), FSR ear/pod roll crease at the top (known since v5.7). Faint hairlines appear on some wall areas at one grazing light angle (decimation of the full-res field mesh). |
| No unintended holes; genus explained | PARTIAL | Genus 34. Identified: 25 face-plate holes (16 Trellis, joystick, 2 pot, 2 encoder, 3 fader slots, lidar slot; listed by position in `holes.py` output), USB-C, 3.5 mm jack (+x, y 555.5 z 6), switch, 3 Hall windows, wire path (40x24 and 30x14 merged = 1), battery door opening, open back = 33. One handle is NOT attributed; cap tests with crude boxes were inconclusive. Needs a voxel Euler-density localisation. The torn 15 x 4 mm wedge hole in the face plate above the junction is closed. |
| Intended openings present | PASS except sizes | 16 keys, 3 faders, round holes, joystick, USB, jack, switch, wire hole, open back, door opening present. Hall windows measure 8.8 x 5.6 (spec ~9 x 7): FAIL on height. |
| Pod smooth pebble, flat face, open back, clean rounded shard-free rim, dovetail removed | PASS (rim r 0.95 lobe / r 1.0 old rim, rod mouths and concave edges excluded) | dovetail rib, channel, panel, stop block and cap remnant removed; floor spurs removed. |
| 4 rods + 4 socket bosses only fastening, 0 intersections | PASS | mount x shell 0.0 after the mount dovetail removal. Clearances to components not re-measured this pass. |
| -x lobe: one big concave sweep, no shelf/right angle/strut | PASS | field-built S sweep (convex ~R10 then concave ~R12-15) reaching x 771.0 at z -35. |
| Housing + hand rest: radii >= 8-10, blend >= 12, broad saddle | PASS (unchanged v5.4 field) | saddle depth measured 4.9 mm at z -36 over most of the length. |
| Palm dip ~6-8 mm deep, ~60x40 | FAIL | measured total hand-rest dip 4.9 mm (saddle + dip together; v5.3 doc says dip 2-4 mm). Needs a fieldA re-run with a deeper dip; the door parting/lip structure shares that surface, so not patched blind. |
| Interior battery space 50x85x30, 0 overlap | not re-run | region untouched (v5.8: 0 mm3). |
| Door: one-level wrap, bead, 0.2 gap, 3 notches, floor >= 1.6 | PASS (unchanged) | 110,000 faces; lip scalloping by design is the thin cluster. |
| Switch MTS-102, flat land, wall 1.6-3.0 | PASS (unchanged) | hole present at the -x face; not re-measured. |
| FSR ear: D-shape, flat coplanar face, same roll | PASS with known crease | small sharp corner where the top edge meets the pod roll, open since v5.7. |
| Logo geometry, size, position | PASS (unchanged) | legible in the front render. |
| Hall rail: 3 windows ~9x7, no exterior slot | PARTIAL | windows 8.8 x 5.6; no exterior slot. |
| Mount: v1 arms, horn contact +-0.1, smooth, clean edge | PARTIAL | dovetail removed, plate back restored with the original ~0.3 mm ripple; every vertex outside the patch unchanged; horn contacts not re-measured. |
| Valve caps unchanged | PASS | |
| Printability: watertight, single body, bed, walls, faces | PASS with notes | shell 391,214 / door 110,000 / mount 107,562; all fit 256 mm in the chosen orientation. Thin (<1.6 mm) clusters in the audit: shell 141, 85, 40 mm3 largest (the 2.4 mm rim feet and thin strips are not part of these), mount 12 mm3, door 21 mm3 lip. |
