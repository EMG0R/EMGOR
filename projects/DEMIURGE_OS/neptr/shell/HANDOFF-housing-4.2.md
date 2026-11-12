# NEPTR Housing — Phase 4.2 Handoff

Last updated: 2026-10-14
Working file: `neptr_HOUSING-4.2.blend` (this folder)
Derived from: `PHASE4.blend` (untouched — still the phase-4 record)

---

## 1. Where things are

Project folder: `_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/shell/`

A stale duplicate of `PHASE4.blend` also sits in `______2026NEW/legacy/NEPTR phase4/shell/`.
Different inode, same bytes. Ignore it.

Blender 5.1, driven over the Blender MCP addon (socket on `localhost:9876`).
Units: metric, scale 1.0, **1 Blender unit = 1 mm**. All numbers below are mm.

### Objects in the file (240 total)

| Collection | Contents |
|---|---|
| Scene Collection | `neptr_top_dome_4.2`, `neptr_bottom_cap`, `X1C_SafeArea_250` |
| `HOLE_CUTTERS` | 43 cutter objects, hidden, original transforms intact |
| `BACKUPS` | ~190 historical stages + the v-backups listed in §6 |
| `LIDAR_PARTS` | empty |

`X1C_SafeArea_250` is a 250 x 250 reference plane at z=0 (Bambu X1C bed).

---

## 2. Coordinate conventions

- Origin is the part centre. Bottom of the cap sits at z = 0.
- **z = 12.4 is the seam plane** — cap rim top and dome rim bottom both land there.
- Cap occupies z 0 -> 50 (includes internal legs). Shell rim reaches 12.4.
- Dome occupies z 12.4 -> 62.
- Corner fastener axes are at **x = +/-89.75, y = +/-89.76**.
  "South" = y negative, "north" = y positive. The two pairs are NOT symmetric in z — see §4.

---

## 3. The two working parts

### `neptr_top_dome_4.2`
- Copy of `STAGE4_top_dome_backup` — the last state in the phase-4 chain **before any cutout**.
  Chain was: STAGE4 -> STAGE7 screen -> STAGE8 panel -> STAGE10 encoders -> STAGE11/12 ports
  -> STAGE13 lidars -> ring/dotring/vents. STAGE4 is the rollback point.
- 247.99 x 206.94 x 49.60, z 12.4 -> 62
- 7,470 verts / 7,447 polys, single component, watertight
- Shell wall 2.4 thick; interior cavity clear from 12.4 to 59.6 (**47 mm of open volume**)
- At each corner fastener axis the dome is **solid from 12.4 to 19.9** (7.5 of rim-skirt wall)
- No modifiers. Hole-free — verified visually top and iso.

### `neptr_bottom_cap`
- 241.43 x 200.67 x 50.00, z 0 -> 50
- 9,218 verts / 6,950 polys, single component, watertight
- Bottom skin **2.4 thick** (2.28–4.80 in the corner regions)
- Re-derived from the pristine `PHASE4.blend` cap on 2026-09-14 after a bad boolean (see §5)

---

## 4. Corner fasteners — M3, final spec

Four, one per corner, at (+/-89.75, +/-89.76). All dimensions ray-probed, not assumed.

### Through-bore (single diameter, full depth)
- **3.60 mm** (measured min radius 1.796, max 1.800 at every height)
- Runs from the outer head recess straight through the boss top at z = 12.2
- Serves as M3 screw clearance below and M3 insert pocket above — no step, nothing to misalign

### Head recess (outer face)
- **5.98 mm** dia, up to z ~4.4
- Bearing ring with the 3.6 bore = 1.19 wide. Takes an M3 socket head (5.5) or pan head

### Boss
- **8.0 mm OD**, flat top at **z = 12.2** (0.2 clearance under the dome rim at 12.4)
- Bottom is cut to a plane fitted to that corner's inner skin, sunk 0.8 into it, so it welds
  over the whole footprint. Skin there slopes **38.0 deg** (south) / **32.0 deg** (north);
  the plane fit was accurate to 0.19 RMS / 0.60 max over a 9.2 footprint

### Insert grip (continuous wall around the bore, measured)
| Corner | boss bottom at axis | grip bottom z | **grip length** |
|---|---|---|---|
| +89.75, -89.76 | 6.43 | 5.4 | **6.80** |
| -89.75, -89.75 | 6.42 | 5.4 | **6.80** |
| -89.75, +89.76 | 5.11 | 4.0 | **8.20** |
| +89.75, +89.76 | 5.10 | 4.0 | **8.20** |

### Insert + install
- **Generic M3 heat-set, 4.0 mm OD x 4.0 mm long** (knurled brass)
- 3.6 bore vs 4.0 OD = 0.4 interference — correct for PETG
- **Insert goes in from the TOP of the boss (z=12.2), melting downward.** It cannot be fed from
  underneath: 4.0 OD will not pass the 3.6 bore in the skin
- Melt it to **~0.3 below flush** — only 0.2 of clearance to the dome rim, a proud insert fouls it
- Screw enters from outside the bottom, head in the 5.98 recess, threads up into the insert
- Do NOT burn into solid material. That was the old spec and it is what caused the splitting

### Superseded
- Earlier in this file there were flat horizontal insert seats at z 8.00/7.99 (south) and
  6.41/6.39 (north). The 3.6 through-bore consumed them. The flat boss top is now the
  square entry face.

---

## 5. Boolean rules for this model — learned the hard way

**Use `solver='EXACT'`, `use_self=False`, `use_hole_tolerant=False`.**

Measured on identical input (union of 4 bosses into the cap), counting self-intersecting
face pairs:

| Solver | self-int pairs | non-manifold edges |
|---|---|---|
| EXACT plain | **39** | 0 |
| EXACT + self + hole_tolerant | 41 | 25 |
| MANIFOLD | 1,131 | 0 |

MANIFOLD silently fails here — it leaves the boss bottom discs as internal geometry. The first
attempt looked fine (0 non-manifold, parity tests passed) and was only caught by a BVH
self-overlap check. **This is the opposite of the PMOR rule**, where MANIFOLD was required for
free-hanging cylinder unions into a dense shell. Embedded boss into a thin sloped skin -> EXACT.

Other rules:
- **Triangulate the local neighbourhood before each boolean** (faces within r=12 of the cut).
  The cap carried 66 non-planar n-gons and one 543-vert face; those are what break solvers.
- **Never triangulate all n-gons on the dome.** All 278 -> 3 non-manifold edges. EAR_CLIP -> 12.
  Only the non-planar ones -> 0. That was the prep that worked (see §7).
- Still true from PMOR: **no remove_doubles / dissolve_degenerate as general cleanup.** Targeted
  `dissolve_degenerate` limited to the degenerate faces' own edges is survivable, but it leaves
  a wire edge that must be deleted after.
- Build cutters as explicit bmesh cylinders (48 seg), recalc normals, triangulate, then join all
  four into one helper and do ONE boolean. Delete the helper after applying.

### Verification recipe (do this after every cut)
1. `BVHTree.FromBMesh(bm).overlap(itself)` -> self-intersecting pairs. Compare to the pre-cut count.
2. Non-manifold edges, wire edges, loose verts, zero-area faces, component count.
3. Ray-parity over a 6 mm XY grid: count columns with an odd number of crossings. Must match the
   pre-cut count exactly — that is what caught the failed MANIFOLD union.
4. Outer-skin diff: first-hit z before vs after, on a grid. Should be < 0.1 everywhere.
5. Interior solidity: parity test on a 3D grid of sample points inside the new solid.

**Ray-probe gotcha:** do not sample a 48-facet cylinder with 24 rays. 24 divides 48, every ray
hits a facet edge and silently misses. Use a coprime count (37) plus a small phase offset.

---

## 6. Backups in the file

| Name | What it is |
|---|---|
| `neptr_bottom_cap_backup_v2` | current cap, post-bore (latest good) |
| `neptr_bottom_cap_PRISTINE_backup` | untouched phase-4 cap, re-appended from `PHASE4.blend` |
| `neptr_top_dome_4.2_backup_v1` | current dome, post-prep |
| `neptr_top_dome_4.2_PREPREP_backup` | dome before the n-gon/degenerate prep |
| `PHASE4_top_dome_FINAL_backup` | the finished phase-4 dome with every cutout |
| + ~185 historical stages | STAGE1-13, MOLD_1-7, DOTRING_*, VENTED_*, RING_*, SURG_*, feet/legs |

Convention: snapshot **after** a successful destructive edit, named `<object>_backup_vN`.
On a reversion, delete the failed attempt and restore from the latest backup — do not snapshot
the failure.

---

## 7. Mesh health, current

| | cap | dome |
|---|---|---|
| verts / polys | 9,218 / 6,950 | 7,470 / 7,447 |
| components | 1 | 1 |
| non-manifold edges | 0 | 0 |
| wire edges / loose verts | 0 / 0 | 0 / 0 |
| zero-area faces | 0 | 3 (was 25) |
| non-planar n-gons | 0 at corners | 0 (was 120) |
| self-intersecting pairs | 45 (local to the 4 bosses) | 8 |

Both watertight and sliceable. The residual self-intersections are local face overlaps at the
boss junctions; the pristine cap already carried 8 of its own.

**Dome prep cost:** triangulating the non-planar n-gons moved the outer skirt surface by up to
0.52 at 22 of 2,893 probe points (x +/-104–120, z 13.1–16.6). The rim plane at 12.4 and every
deeper surface are byte-identical, so seam fit is unaffected.

---

## 8. Print setup

- Bambu Lab X1 Carbon, 256 bed, **PETG at 100% infill**
- Cap footprint 241.43 x 200.67 — inside the 250 safe area
- Nothing protrudes below the print face: outer-skin deviation from the pristine cap is
  <= 0.21 across ~3,000 probe points

---

## 9. Open / next

1. **Cut the holes back in.** 43 cutters wait in `HOLE_CUTTERS` with their original transforms:
   screen, 8buttons, encoders (3 variants), FSR (3), south ports (3), ports_recut, TRS (3),
   USB-C, groove segments, 14 ring/band cutters, `DOTRING_CUTTER_REFERENCE`, `all holes` + `.001`.
   Suggested order: screen and OLED window first (biggest, most central — they confirm the prep
   held), then ring / dotring / encoder row.
2. Each cut: triangulate locally -> EXACT -> run the §5 verification -> snapshot `_backup_vN`.
3. Decide whether the dotring aesthetic from phase 4 comes back or gets redesigned for 4.2.
4. Export check: STL/3mf once the cuts are in; previous exports in this folder are phase-4 era
   (`PHASE4-neptr_bottom_cap.stl`, `PHASE4-neptr_top_dome.002.stl`, `neptr_shell_phase4.3mf`).

---

## 10. Things that are decided — do not relitigate

- Screw holes are **M3**, 3.60 bore, 5.98 head recess. The `FEET_M4_*` backup names are stale.
- Inserts are melted into a **3.6 pocket from the top**, not burned into solid material.
- Boss top stays at **12.2**; it cannot go higher, the dome rim is at 12.4.
- Hole positions stay at (+/-89.75, +/-89.76) — the only place with clearance. Moving them
  inboard would buy insert depth but is not needed with 6.8–8.2 of grip.
- Dome rollback point is STAGE4. Everything after it is recoverable from `BACKUPS`.

---

## 11. 2026-10-14 port + screen cuts

All cuts EXACT / use_self=False / hole_tolerant=False, explicit bmesh cutters (48 seg), ONE boolean per batch. File saved after all passed.

### Method changes (read before cutting more)
- **Whole-mesh EXACT breaks the rim.** The dome has stacked/coincident junk verts and sliver faces at the rim (z 12.4-17, all four corners); a whole-mesh EXACT leaves 7 non-manifold edges there. Fix used on the dome: bisect at **z = 20.0**, fill the upper part, boolean only the upper part, delete the fill, weld back (remove_doubles on the z=20 loop only). Everything below z=20 is untouched. Z=20 chosen by scanning planes: z 18-19.5 moved the outer skirt 0.13-0.38; z=20.0 moved it 0.03.
- **Local triangulation pad = 3 mm** (not 12). Pad 12 moved the outer wall 0.2-0.38 (non-planar n-gons). Pad 3 moved it <= 0.013.
- **Cap:** whole-mesh EXACT re-tessellates far non-planar quads (outer skin moved 0.27 at the corner rim). Prep that fixed it: triangulate all non-planar faces first, quads `FIXED` (matches Blender's own tessellation), n-gons `BEAUTY`, except face 2541 (pentagon at -99.7, 28.5) which needs `EAR_CLIP`. Prep gave dz <= 0.044, cap metrics unchanged. EAR_CLIP for all n-gons made 12 non-manifold edges at the rim after the boolean.

### Part A - screen (dome)
- Window 53.24 x 88.55, r 0.5, centred (0,0), through the top skin.
- 4 bosses OD 6.5 at (+/-33.0, -47.1) and (+/-33.0, +42.9), z 55.7 -> 60.0 (3.9 below ceiling 59.6), plus alignment peg D3.0, z 51.7 -> 55.7 (4.0 long), 0.5x45 chamfer at the tip. Boss+peg built as one lofted solid per boss, one UNION. **No heat-set bores** (coordinator change). Peg tips measured at z 51.7. Outer top skin diff 0.00008 on a 1 mm grid over all four bosses.
- Clearance: boss inner edge x = 29.75, LCD edge 28.6 -> **1.15** (>= 0.9). Boss to window edge (26.62): **3.13**. No contact.

### Part B - south wall (inner y -101.07, outer -103.47)
Holes cut as specified (left USB pair x -44 at z 39.5/26.5 + screws x -56.5/-31.5; ethernet D-hole at (-75,31); right USB x 44 + screws 31.5/56.5; HDMI D21.5 at (75,31)). All verified open by ray.
- Min wall between holes (edge to edge, measured): USB opening to its screw hole 3.25; the two USB openings 6.0; screw (-56.5, z 26.5) to ethernet 5.59; screw (-56.5, z 39.5) to ethernet 6.91; screw (56.5) to HDMI 6.0.
- Edge to |x| = 90: ethernet outer edge x -86.7 -> 3.3; HDMI 85.75 -> 4.25. Wall outer y at z 31: x 86.7 -103.452, x 90 -103.420, x 95 -103.176 (flat to 0.05 at x 90).

### Part C - north wall
- 8 jacks D6.2 at x -18/-6/6/18, z 24.5/37.5 (pitch 12 x 13 -> 5.8 / 6.8 of wall).
- USB-C: collar OD 20.3 (ID 16.3, wall 2.0), y 93.1 -> 101.5 (0.4 fused), unioned first (solid cylinder), then D16.3 bore through wall + collar. Bore radius measured 8.14.
- Pedal TRS D10 at (41.3, 31).
- North outer-skin diff 0.0; every hole open.

### Part D - cap
- Old Pi legs were **cones, not posts**: centres (+/-24.5, -20) and (+/-24.5, +38), base R 8.9 at z 2.4, standoff R 2.5 to z 34.3, pin R 1.3 to z 38.3. Removed with 19 x 19 boxes (footprint + 0.6), z 2.4 -> 45. Box started at exactly 2.40 (not 2.42): 2.42 leaves a 0.02 film and the stacked internal faces. After the cut every leg column reads hits [2.4, 0] (clean floor). **No stub.** The legs had coincident internal faces at z 2.4 (leg base on floor top); those are gone.
- A 0.1 mm raised pad (z 2.5) near y -35..-26 under the old leg was partly outside the boxes and remains (floor max 2.5).
- Floor top over the Pi footprint (1 mm grid, 4902 pts): solid points min 2.4, max 2.5, mean 2.4046 (173 pts at 2.5, 3576 at 2.4). **Vent holes under the Pi footprint: yes, 1153 of 4902 pts (23.5 %).**
- Sled pins D4.0, top z 5.4 (3.0 above floor 2.4, 0.4 fused): (30,-40), (-31,-40), (30,40), (-31,40). All flat (dz 0) and solid within r 2.5 -> **no pin moved**. Measured pin edge +1.996 from axis.
- Joystick supports (z 50 at (-76,32),(-76,52),(-50,52), peg 45.7 at (-62,42)) verified unchanged vs backup_v2.
- Keep-outs: Pi USB-C box (X -60 -> -28.65, Y 17 -> 29): only cap floor (z 0-2.5) and dome ceiling (z 59.6-62). Joystick supports are NOT in it. HDMI box (X 60-90, Y -101 -> -61, Z 16-46) and ethernet box (X -90 -> -60, Y -101 -> -71, Z 16-46): cap has nothing there; **dome corner fastener blocks intrude** (solid z 16 -> 19.9, reaching y -84.3, x 83-90 for HDMI and x -89 -> -83 for ethernet). Extender body must clear that or sit above z 19.9. Wall inner surface also curves in near x +/-85-90 at z 45 (inner y < -99.5).

### Verification (dome / cap, final)
- Dome: 13,078 v / 12,312 p, components 1, non-manifold 0, wire 0, loose 0, zero-area 2 (pre-existing rim, unchanged), self-int pairs 2 (same as pre-cut baseline).
- Cap: 9,312 v / 10,647 p, components 1, non-manifold 0, wire 0, loose 0, zero-area 0, self-int 0.
- Ray-parity odd columns (6 mm grid, z and y scans): 0 / 0 before and after every cut. Outer-skin diff after every accepted cut <= 0.044 (cap) and <= 0.013 (dome), masked only at the cut footprints.
- Note: the BVH self-overlap count here is unique face pairs sharing no vertex; baseline for the dome is 2, not the 8 in section 7 (different filter).

### Backups created (hidden, BACKUPS)
- Dome: `neptr_top_dome_4.2_backup_v2` (window), `_v3` (bosses + pegs), `_v4` (south ports), `_v5` (USB-C collar), `_v6` (north holes, current).
- Cap: `neptr_bottom_cap_backup_v3` (legs removed), `_v4` (pins, current).
- Earlier dome `_v3`-`_v6` from the first pass (with heat-set bores) were deleted by me after the coordinator dropped the bores; `_v1`/`_v2` (pre-existing and window) untouched.

### 11b. South wall re-layout, Pi sleds, STL export (2026-10-14, later)

**South wall re-layout (dome).** Restored `neptr_top_dome_4.2_backup_v3` (bosses + pegs), re-cut south, then redid collar + north holes. Backups: `_v7` south, `_v8` collar, `_v9` north (current). Old-layout `_v4`-`_v6` are stale, kept (not deleted).
- New layout, z 31 unless noted: ethernet D-hole at x -68, HDMI D21.5 at x +68, USB-A vertical (opening 7 X x 15 Z, screws D3.5 at z 18.5 and 43.5) at x -46, -32, +40. Centre |x| < 26 bare. All holes verified open.
- Split plane for the south cut is **z = 16.0** (screw holes reach z 16.75); noop-split test: outer skin moved 0.018, non-manifold 0. Collar and north use z = 20.
- **Gotcha:** the north bore/collar cut raised self-overlaps 2 -> 5 when local triangulation used `quad_method=FIXED` / `EAR_CLIP`. The dome needs the default triangulate (BEAUTY) in the local prep; the FIXED/EAR_CLIP prep is for the cap only.
- Wall between neighbouring holes (edge to edge): USB opening to its screws 3.25; USB -46 to USB -32 7.0; USB -46 to ethernet 6.8; screw-to-screw (-46/-32) 10.5; USB -32 to HDMI/USB +40 large (> 60); USB +40 to HDMI 13.75 (from geometry, 1.5 mm to nearest of those listed as nearest-pair list).
- Nut keep-outs (inner face, z 31): D29 at x -68, D30 at x +68. Nearest non-wall dome geometry (corner fastener blocks, z <= 19.9): ethernet 5.6, HDMI 5.1 (0.5 mm sampling). Nothing intersects either circle. Wall rim skirt below the nut: inner face is at y -99.43 at z 16.5 (curves inward from -101.07), so the nut bottom edge lands on the sloped skirt. Nut to nearest USB opening: ethernet nut to USB -46 is 4.0; HDMI nut to USB +40 is 9.5.

**Pi sleds** (new collection `PI_SLED`, cap untouched): `neptr_pi_sled_z6` (visible), `neptr_pi_sled_z10` (hidden). Installed position, bbox z6 X -35.49..34.49, Y -44.49..44.49, Z 2.4..9.0; z10 same with Z max 13.0.
- Frame bars 6 wide, z 2.4-4.8, rectangle through the 4 pin centres, D9 corner pads, D4.4 pin holes, 6 wide bars from each corner to its post. Posts: D3.3 base boss, D6.0 shoulder to z 6.0 / 10.0, D2.4 pin 3.0 tall, 0.4x45 tip chamfer.
- **Deviations:** (1) left rail (x -31) is interrupted from y 14 to 32 so the sled clears the Pi USB-C keep-out (X -60..-28.65, Y 17..29); overlap pairs with the keep-out = 0. (2) D3.3 boss under each post (a D6 post on a 6 wide bar is exactly tangent and made degenerate faces). (3) z6 post loft starts at z 4.2, z10 at 4.4 (z6 hole artefact at 4.4). (4) Pockets 0.1 deep on the underside where the cap floor has raised 0.1 pads (z 2.5): done with a boolean against a cap floor slab; sled-vs-cap intersection volume 3.6e-8 mm3 (zero).
- Each: 4,352 v / 4,478 p, 1 component, 0 non-manifold, 0 wire, 0 loose, 0 zero-area, 0 self-int. Volume z6 4,465 mm3, z10 4,917 mm3.

**STLs** (binary, new files, scale 1.0, Y forward / Z up, selection only) in this folder:
- `NEPTR_4.2_top_dome.stl` 27,446 tris, X +/-124.0, Y +/-103.47, Z 12.4..62
- `NEPTR_4.2_bottom_cap.stl` 18,872 tris, X +/-120.71, Y +/-100.33, Z 0..50
- `NEPTR_4.2_pi_sled_z6.stl` 8,716 tris, X -35.49..34.49, Y +/-44.49, Z 2.4..9
- `NEPTR_4.2_pi_sled_z10.stl` 8,716 tris, same XY, Z 2.4..13
Final dome: 13,681 v / 12,973 p, 0 non-manifold, self-int 2 (baseline). Cap unchanged from section 11 (9,312 v / 10,647 p).

### 11c. Nut spot-faces (2026-10-14)
Ethernet D32 at x -68 and HDMI D33 at x +68 (z 31), y -101.10 -> -90, DIFFERENCE, split z=14, local triangulation pad 1.0 (pad 3 moved the skirt 0.104). Backup `neptr_top_dome_4.2_backup_v10`. Old-layout `_v4`-`_v6` deleted (mine). Wall left under the spot-faces: max 2.44, **min 0.52 at (x 64.5, z 15.5)** where the sloped skirt's outer face curves in: no breakthrough but thin at the bottom edge of the HDMI circle. Gap to corner blocks: ethernet 4.1, HDMI 3.6. Wall to USB: ethernet cutter edge to USB -46 opening 2.5, HDMI to USB +40 8.0. Dome re-exported (30,074 tris).

## 12. 2026-10-14 smooth rebuild (supersedes the §11 layout where they differ)

Problem: the old bases were coarse (dome 7.5k faces, ~5.5 mm quads): median dihedral between adjacent faces 7.2 deg, 95th pct 17 deg (dome) / 44 deg (cap, incl. rim), so prints looked faceted. Both parts are now rebuilt as feature-free smooth bases + a feature library.

### Method
- Clean quad source found: `STAGE2_hollow_backup` (all-quad closed body, two shells, z 0-62, wall 2.4, top skin 59.6-62, floor 0-2.4). The old dome and cap were both cut from it. `STAGE5_bottom_cap_backup` = the feature-free cap tub and matches it.
- Catmull-Clark, 3 levels, limit surface. Control cage made interpolating (4 iterations of vertex-limit correction) so the surface passes through the source vertices (plain CC shrank x by 0.10 per side). A 112-vertex zone at the side notches (x +/-120.5, y -72, z 31) had pinched rows 0.06 mm apart; those were Laplacian-smoothed (max move 2.1 on the control cage) or the cage folded. Then x*0.99964, y*0.99975 to land dims inside +/-0.1.
- Split at z = 12.4 by bisect (rim verts exactly 12.3999996 = float32 12.4), annulus bridged. Cap bottom clamped to z >= 0.
- Dihedral between adjacent faces on curved surface, base meshes: dome median 0.89 deg, p95 2.01, p99 2.8, max 4.3, 5 % of edges > 2 deg; cap median 0.91, p95 1.93, p99 2.6, max 4.2, 4 % > 2 deg. (Before: dome 7.2 / 17.3 / max 60 at the rim; cap 7.5 / 44.) Target of <= 2 deg everywhere NOT fully met: worst rounded corners are 3-4 deg. Next level (L4) is 4x the tris (over budget).
- Base dims: dome 247.991 x 206.901 x 49.629 (old 247.99 x 206.94 x 49.60); cap 241.461 x 200.707 x 12.4 (old 241.43 x 200.67). Wall 2.4 and floor 2.4 reproduced exactly. Left-over lumpiness at the lower-right dome edge comes from the source grid, not from subdivision.
- Not used: voxel remesh (follows the facets, needs smoothing, loses seam), `dissolve_limit` on 400k faces (froze Blender ~25 min; do not run it).

### Structure in the .blend
- `NEPTR_4.2` (visible): `neptr_top_dome_4.2`, `neptr_bottom_cap`, `X1C_SafeArea_250`, child `PI_SLED` (unchanged).
- `FEATURES_4.2` (hidden) / `_CUT` (15) / `_ADD` (6). Objects `<part>_<op>_<feature>`, identity rotation/scale, location = feature centre, mesh watertight, custom props `part op order applied spec centre`. Manifest: `FEATURES_4.2.json` (this folder).
- `BACKUPS_4.2` (hidden): `*_PRESMOOTH_backup` (old parts), `*_SMOOTH_base` (feature-free), dome `_backup_v01-v04`, cap `_backup_v01-v06`; each has custom prop `note`.
- `ARCHIVE_PHASE4` (hidden): old `BACKUPS` + `HOLE_CUTTERS`, untouched. Default `Collection` and `LIDAR_PARTS` left alone.
- Recipe: SMOOTH_base, then features in `order`. EXACT, use_self=False, one boolean per batch. Cutters that overlap each other must be separate batches (bores + head recesses in one batch gave a broken mesh; reverted).
  - dome: 10 window cut | 20 corner blocks, 30 screen boss+pegs, 40 USB-C collar (adds) | 51 jacks, 52 USB-C bore, 53 TRS, 54 HDMI, 55 ethernet, 56-58 USB-A (cuts) | 70/71 spot-faces.
  - cap: 10 bosses | 20 bores | 30 recesses | 40 joystick supports | 50 grill | 60 sled pins. `cap_cut_old_pi_legs` (order 0) is reference only: the clean base has no old legs.

### Feature numbers
- Dome corner blocks (new, measured from the old dome): cylinder R 7.3 at (+/-91.49, +/-91.42), z 12.4-19.9, built as cylinder intersect outer envelope inset 0.05 so they never poke through the skin (first attempt poked out 0.93). Outer skin diff after union 0.001.
- Cap bosses: 4 at (+/-89.75, +/-89.76), OD 8.0, top 12.2, bottom = plane fitted to the inner skin sunk 0.8 (normal); fits: S slope 37.99 deg, N 32.09 deg, rms 0.21. Bore D3.6 through, recess D5.98 to z 4.4.
- Joystick supports re-measured: 3 cones base R 6.0 at z 2.4, R(z) = 6.2 - 0.0835 z, flat top z 45.7 (R 2.38), pin R 1.45 to z 50, at (-76,32) (-76,52) (-50,52); peg D6 to z 45.7 at (-62,42).
- Grill: 59 diamonds |x|+|y| <= 4 (half-diagonal 4), 11 pitch checker (even rows y -33,-11,11,33: x = +/-5.5,16.5,27.5,38.5; odd rows y -22,0,22: x = 0,+/-11,22,33,44), vertical walls, matches the old cap exactly.
- Sled pins D4.0 z 5.4 at (30,-40) (-31,-40) (30,40) (-31,40). Sled z6/z10 vs new cap: 0 overlapping face pairs; pin R 2.0 in sled hole R 2.196.

### Layout (supersedes §11)
- North: 8 jacks D8.3 at x -21,-7,7,21 and z 24,38; USB-C collar OD 20.3 + bore D16.3 at (-75, 31); TRS D10 at (+75, 31). No spot-faces needed (barrel inner wall varies < 0.1 over the jack nuts; collar >= 4.2 from corner blocks).
- South (swapped): HDMI D21.5 at x -72.2 (nut D30, spot-face D31); USB-A x -50.2 and -36.2 (14 pitch); Ethernet D23.4 flat 10.3 below at x +72.9 (nut D29, spot-face D30); USB-A x +51.4. USB opening edge to nut keep-out 3.5. Ports pushed out until spot-face to corner blocks = 1.05 mm (0.1 mm search). Clear centre between USB-A inner edges: x -32.7 .. +47.9 = 80.6 mm wide.
- Thin walls: spot-face floor y -101.10 leaves **0.77 mm (HDMI, at x -71.7 z 46)** and **0.94 mm (ethernet, x 71.9 z 45.5)** at the top/bottom arcs, because the barrel wall curves in symmetrically (z 17 = z 45). >= 1.5 is impossible with a flat nut seat at the z 31 inner face. Other walls: 3.0 between each spot-face and its neighbouring USB opening, 3.25 USB opening to screw holes, 7.0 between the two left USBs.

### Verification (final)
- Dome 773,084 tris / 384,535 faces; cap 450,566 / 222,223. Both: 1 component, 0 non-manifold, 0 wire, 0 loose, 0 zero-area, self-intersecting pairs 0, ray-parity odd columns 0 (6 mm grid, z and y scans), outer-skin diff <= 0.0025 per batch off the cut footprints (dome), <= 0.023 (cap pins, grazing on diamond edges).
- 3 micro faces (area ~1e-9) left by the spot-face cut at x -98 / -78 were welded (remove_doubles 5e-4 on those verts only), re-verified.
- STL (binary, new): `NEPTR_4.2_top_dome.stl` 773,084 tris, `NEPTR_4.2_bottom_cap.stl` 450,566 tris. Sled STLs unchanged.

## 13. 2026-10-15 analytic form rebuild (supersedes the §12 CC-smoothed bases)

Why: CC level 3 only smoothed the old cage, so its lumps stayed (SE corner undulation, seam specks, blob under the SE corner). Bases are now generated from a formula, not from the old cage. Reproduce: `python3 NEPTR_4.2_form.py NEPTR_4.2_form_params.json` (numpy + scipy; mesh builder and envelope helper inside).

### Form
- Plan outline C0: closed, mirror-symmetric in x, flat top/bottom walls (y +103.46 / -103.46), curvature = cubic B-spline in arclength (K 24, G3), tangent angle integrated, L solved so heading closes at exactly -pi. Curvature sign changes: 2 (convex corner, concave waist, convex corner), min convex radius 27.0, max concave curvature 0.0128. Fit vs the old z 29.5-32.5 outline: rms 0.44, max 1.5 mm. Extents: x +/-124.00 (old 123.99), y +/-103.46, south ear 120.64 (old 120.6).
- The old part is NOT a superellipse: plan is a dog-bone (concave x waist 103.5 vs ears 124/120.6, north/south ears differ). A superellipse a(z),b(z),n(z) was rejected (residual ~20 mm).
- Inset profile d(z): quintic Bezier, D 25.5, zero curvature into the flat top/bottom (G2), vertical wall z 29.0-33.0, symmetric about z 31. rms 0.07, max 0.30 vs the old walls. Outer skin = exact parallel offset of C0 by d. Wall 2.4 = analytic normal offset (ray-checked 2.400 +/- 0.002 away from the seam rim).
- Mesh: structured grid, step set by max turning 1.5 deg, hmax 1.2 mm. Flat top/bottom/floor/ceiling = Delaunay fill. Seam z 12.4 rim annuli are single planar rows (both parts exactly coplanar), ceiling 59.6, floor 2.4.
- Old vs new surface (3D nearest distance, 50 400 samples of the old outer skin): rms 0.365, max 2.28, p95 0.91, p99 1.46. In plan-section terms the old SE corner/ear rings recede faster than a parallel offset (up to 7.5 mm at the SE inner rings near the flat edges, 3D <= 2.3): not reproduced on purpose (a parallel family cannot reproduce it without collapsing the corner).
- Flat edge corner radius 1.5 (outline min radius 27 minus D 25.5).

### Fairness (adjacent-face angle on the skin, seam rim excluded)
| | median | p95 | p99 | max | > 1.5 deg |
|---|---|---|---|---|---|
| old dome base (CC L3) | 0.35 | 1.79 | 2.56 | 4.30 | 9.5 % |
| old cap base | 0.00 | 1.68 | 2.27 | 4.22 | 7.2 % |
| new dome base v2 | 0.97 | 1.35 | 1.36 | 1.37 | 0 % |
| new cap base v2 | 0.55 | 1.36 | 1.37 | 1.37 | 0 % |
Curvature check: outline curvature has the intended 2 sign changes only; rings never fold (outline min radius 27 > max inner offset 25.5, margin 1.5).

### Root causes
- Seam specks: CC base was split at z 12.4 by bisect + bridge, which left sliver/micro triangles (old final dome: 295 slivers h<0.01 and 66 area<1e-4 within z 12.2-13.5, old base 125/22). v2 has none (min area 0.116 there); rows are generated, not cut.
- Base blob under the SE corner: the D5.98 head recess is cut flat to z 4.4 into a 38 deg skin (outer z 3.79 at the axis): 0.6 deep pit on one side, 2 mm on the other. New SE skin outer z at the axis 2.65 (N 2.57), skin 29.9/29.4 deg, so the recess is a regular pit.

### Interfaces (new vs old bases)
| | old | new |
|---|---|---|
| seam plane / coplanarity | 12.4 | 12.4, 1768 rim verts exactly float32 12.4 on both parts |
| ceiling under screen | 59.6 | 59.6 (at (0,0) and (33,-47.1)) |
| cap floor top, sled pins | 2.4 / 5.4 | 2.4 / 5.4, pins at (30,-40) (-31,-40) (30,40) (-31,40) unchanged, no sled STL change; sleds vs cap strict overlap 0 |
| fastener axes | (+/-89.75, +/-89.76) | unchanged |
| inner wall y (z 20 / 31 / 45) | 100.44 / 101.04 / 99.66 | 100.50 / 101.06 / 99.72 (outward 0.02-0.06, so ports lose nothing) |
| cap skin z at axis inner/outer N | 5.56 / 2.79 | 5.27 / 2.57 |
| cap skin z at axis inner/outer S | 6.91 / 3.79 | 5.36 / 2.65 |
| cap extents | X 120.71 Y 100.33 | X 120.87 Y 100.33 |
| dome extents | X 124.0 Y 103.47 | X 124.0 Y 103.46 |
Cap fastener bosses regenerated: plane fits slopes S 29.9 deg, N 29.4 deg (rms 0.18, max 0.66), boss bottom at axis 4.58 (S) / 4.49 (N), top 12.2. Recess floor 4.4 at all four. Corner blocks regenerated (R 7.3 cylinders clipped by the 0.05-inset envelope; cylinders sit entirely inside, bbox unchanged 98.79/98.72).

### Port positions (unchanged from §12)
North: 8 jacks D8.3 x -21,-7,7,21 z 24,38; USB-C bore D16.3 + collar OD 20.3 at (-75, 31); TRS D10 at (75, 31). South: HDMI D21.5 x -72.2; USB-A x -50.2, -36.2, +51.4; Ethernet D23.4 x 72.9; all z 31, all verified open by ray.

### HDMI / Ethernet nut seats (new)
New ADD features `dome_add_hdmi_boss` (order 45) and `dome_add_eth_boss` (46): inside disc (D30.9 / D29.9, y -101.5 up to 0.3 past the seat plane) unioned BEFORE the holes; spot-face cutters re-cut at the new seat planes: HDMI y -100.2, Ethernet y -100.4 (old floor -101.1). The cutter trims the disc flat, so there are no coplanar faces. Wall behind the seats (seat plane to outer skin, ray-measured over the whole disk): HDMI min 1.61 (at x -72.2 z 15.7), Ethernet min 1.59 (x 72.9 z 16.2); old 0.77 / 0.94. Boss thickness over the old inner wall: 0.86 (HDMI) / 0.66 (Eth) at the centre, tapering to 0 at |dz| ~11-12; the clear centre between USB-A inner edges is untouched. Nut keep-outs (D30 / D29, y -100.2 to -80) hold no dome solid (sampled). Nearest corner block to the spot-face circle 0.83 (was 1.05).

### Verification (final objects)
- Dome: 190 177 v, 1 component, 0 non-manifold, 0 wire, 0 loose, 0 zero-area, 0 strict self-intersections (float64, touching excluded; 7 raw BVH touch pairs), ray-parity odd columns 0/0 (6 mm grid z and y). Cap: 95 200 v, same, 7 raw touch pairs.
- Per batch: outer-skin diff 0 outside the cut footprints (cap and dome).
- Meshes are triangulated once before the feature booleans (quads + EXACT re-tessellation produced float32 self-intersections in untouched areas). `matrix_world` of the hidden-collection library objects is stale: build helpers from `matrix_basis`.

### Snapshots (BACKUPS_4.2, all verified)
dome v05 window | v06 corner blocks + screen boss/pegs + USB-C collar | v07 north jacks, USB-C bore, TRS | v08 boss discs + south HDMI/Eth/USB-A | v09 spot-faces (= final dome). Old v01-v04 kept. cap v07 bosses | v08 bores | v09 recesses | v10 joystick supports | v11 grill | v12 sled pins (= final cap). Old v01-v06 kept. Bases: `dome_SMOOTH_base_v2`, `cap_SMOOTH_base_v2` (note prop), v1 bases kept.

### Files
`NEPTR_4.2_form_params.json`, `NEPTR_4.2_form.py`, `FEATURES_4.2.json` (23 features, v2). STL (binary, overwritten): `NEPTR_4.2_top_dome.stl` 380 438 tris (X +/-124.0, Y +/-103.46, Z 12.4-62), `NEPTR_4.2_bottom_cap.stl` 190 648 tris (X +/-120.87, Y +/-100.33, Z 0-50). Chord error <= 0.003 on curved skin (1.5 deg steps, R >= 1.5), sled STLs unchanged.

## 14. 2026-10-15 control holes (dome only; cap unchanged = cap_backup_v12)

Revert points (file copies, never touched since): `neptr_HOUSING-4.2_PRE-CONTROLS.blend`, `NEPTR_4.2_top_dome_PRE-CONTROLS.stl`, `NEPTR_4.2_bottom_cap_PRE-CONTROLS.stl`.

### Specs used (all vertical through-holes in the flat top skin, z 59.6-62, 48-seg cutters, polygon sized so the flat-to-flat width >= nominal)
| group | features | positions (mm, y south-) | size |
|---|---|---|---|
| buttons | `dome_cut_btn_L1..L4,R1..R4` | (+-44,0) (+-55,+11) (+-55,-11) (+-66,0) | D7.4 (measured 7.40-7.42) |
| encoders | `dome_cut_enc_L2,L1,C,R1,R2` | x 0, +-42.7, +-85.4 at y -60.4 | D7.5 |
| fsr | `dome_cut_fsr_L,R` | (+-42.1,-37.6) | 6.0 x 8.0 rectangle, long axis 10 deg off Y (R toward -x going +y, L mirrored); exact corners of old `CUT_FSR_rotated_v3` |
| joystick | `dome_cut_joystick_hole` | (-62,42) | D30.7 |
| lidar | `dome_cut_lidar_L,R` | (+-25.7,67.2) | D10.5 |
Each has props part/op/order (80-87, 90-94, 100-101, 110, 120-121)/batch (7-11)/group/spec/split_z; listed in `FEATURES_4.2.json` (41 features now, `batch` + `group` added to all).

### Discrepancies vs the brief / old docs (old = ARCHIVE_PHASE4 cutters + ray scan of STAGE13 and PHASE4_top_dome_FINAL)
- Buttons, encoders, lidar (D10.5), FSR centres: match the old cutters exactly.
- **Joystick: old hole is D30.7 (cutter 30.71-30.76; dome ray scan 30.5-30.85), not D27.** Used D30.7 per "old cutters win". Centre: old holes at (+-62.5,42), cap peg at (-62,42), circumcentre of the 3 support posts (-63,42). Used peg axis (-62,42). Old dome had a mirrored hole at (+62.5,42) with no supports on the cap; not cut. If D27 is wanted: edit `CONTROLS['joystick']['d']`, recreate, rebuild (one batch).
- **FSR slot is 6 x 8 rotated 10 deg**, not an axis-aligned slot (old rotated cutter; vertical bbox 7.3 x 8.92).
- **No anti-rotation tab holes** in any old encoder cutter (all plain 64-vert cylinders, 5 variants checked). Old FINAL dome has a 4.5 thick pad (z 57.5-62) at y -69.5..-62.5 under each encoder: leftover plug of the earlier encoder row at y -65.5, not a feature; not reproduced.
- Old dome extras not requested and not cut: encoders at (+-85.4,+75) D7.5, four 20 x 30 rectangles at (+-22,+-27), lidar screw holes (x +-17.75, y 67.25) and a thickened lidar pad (z 55.4). Open question only if wanted.
- Old y of the first encoder version was -65.5 (STAGE10); final (raised) cutters and FINAL dome are -60.4 = brief.

### Axes / seats
- Top skin is flat (z 62.000, ceiling 59.600, deviation 0.000 over every footprint + 8-18 mm margin; worst: enc R2 0.014 over 8 mm = 0.1 deg). Axis tilt 0 everywhere, axis = vertical = local normal.
- Panel thickness 2.4 everywhere, inside face flat and perpendicular to the axis, so no spot-face / boss was needed (<= the 2.4-3 usable-thread assumption). No ADD features for controls. Joystick supports already on the cap (tops z 50 at (-76,32) (-76,52) (-50,52), peg 45.7 at (-62,42)), unchanged; post centres are 15.6-17.2 from the hole centre (just outside R15.35), tops at z 50 vs ceiling 59.6, no interference.

### Wall minima (edge to edge)
button-button web 8.14 | FSR to screen boss (+-33,-47.1) OD6.5 5.25 | encoder to boss 9.45 | joystick to boss 10.4 | FSR to screen window 11.83 | enc C to window 12.37 | button to window 13.67 | lidar to boss 16.86 | enc R2/L2 to corner block 20.56 | lidar to window 17.66. Ports: all >> 20 (walls). All >= 1.5 required; smallest 5.25.

### Method change (important)
Whole-mesh EXACT on the already-cut dome created a new strict self-intersection at the south wall (83.6,-100.5,20.2) far from the cut (float32 rounding of earlier cuts re-solved). Control batches therefore run EXACT on the part **above z = 50** only (bisect, close with a bridged annulus, boolean, delete annulus, weld seam; below 50 byte-identical). Screen pegs end at 51.7, nothing else crosses z 50. Cutter polygon phase is tuned per group (buttons/encoders 0.9 deg) because a cutter vertex near a base edge left needle faces < 1e-9 area.

### Verification (each batch and final)
1 component, 0 non-manifold, 0 loose, strict self-int 0 (float64, eps 1e-5, raw touch pairs 7 = baseline), ray parity odd columns 0/0 (6 mm), every sampled hole point open (buttons 888/888, enc 555/555, fsr 156/156, joy 111/111, lidar 222/222), outer-skin diff <= 1.1e-5 outside footprints (0 cells > 0.01). zero-area faces 1 = pre-existing baseline (dome face at (69.8,-103.05,20.7)). Cap baseline by the same checker: 85 zero-area (area < 1e-12, pre-existing), strict 0, unchanged. Note: strict eps 1e-5 (1e-7 flags 3 dome / 6 cap pairs that are float noise in the baseline).
Final dome 197,135 v / 394,426 tris. STL overwritten: `NEPTR_4.2_top_dome.stl` 394,426 tris, `NEPTR_4.2_bottom_cap.stl` 190,648 tris (identical to before).

### Backups (BACKUPS_4.2, each has `note`)
dome `neptr_top_dome_4.2_backup_v11` buttons | `_v12` + encoders | `_v13` + FSR | `_v14` + joystick | `_v15` + lidar (= final dome). `_v09` = pre-controls (ports only). Cumulative, so revert one type = rebuild with the helper (below) or go back to the previous vNN if it is the last one. (v10 is an old archive name, skipped.) Cap: no change, `cap_backup_v12` is final.

### Redo / revert with the helper
`exec(open('NEPTR_4.2_apply_features.py').read())` in the live session, then
- `o = rebuild('dome','TMP')` full dome from `dome_SMOOTH_base_v2`; `rebuild('dome','TMP', exclude_groups=['fsr'])` without a type; `include_groups=[...]` only some. Groups: screen, corners, ports, buttons, encoders, fsr, joystick, lidar (cap: fasteners, joystick, grill, sled).
- `verify(o, ref=live, new_feats=[...])`, `mesh_diff(a,b)`.
- Change a spec: edit `CONTROLS` / `FSR` / `PHASES` in the file, `create_control_features(only=['joystick'])` (replaces the library objects), then `w, rep = try_batch('dome', group)` on the right base state and `commit('dome', w, note)`.
- Tested: full rebuild (all 34 dome features) vs live = max 1.5e-5 mm, same topology counts. Ports-only rebuild before the controls matched v09 to 1.5e-5.

### 14b. Encoders y -65.4, joystick D27, arcade hole (2026-10-15, later)
- **Encoders moved to y -65.4** (x 0, +-42.7, +-85.4 unchanged). Feasibility: top flat to y ~ -76 (z 62.000 at -76, 61.99 at -79); min top z inside r10 of the outer pair 61.962 (0.04 mm over 10 mm = 0.2 deg), tilt at the hole ~0. Hole south edge y -69.15: 6.9 mm to the end of the flat, 34 to the outer skin. Inside (12x12 body, 7 mm = z 52.6..59.6): no dome solid in the box, inner wall at y -91 (19.7 clear), at x 91.4 inner wall 99.9 at z 59 (8.5), corner block 12.7, screen boss 9.6; cap internals top out at z 50 on the far side (supports at y 32-52), Pi stack at |x| <= 34.5. plan.md: KY-040 recessed behind panel, only shaft through a D7.5 hole.
- **Arcade hole** (new group `arcade`, batch 12, `dome_cut_arcade_hole`): D30.7 at (+62.5,42). Source: the old Phase-4 dome (FINAL, STAGE13) had TWO identical D30.7 holes at (+-62.5,42); the cap joystick supports (KY-023 posts, 20 x 26) are under the left one, so left (-x) = joystick, right (+x) = arcade. **This conflicts with "top left"**: with the front/south edge toward the performer (encoders, USB-A, HDMI/Ethernet south), -x is the performer's left and holds the joystick. If the arcade button must be top left, joystick and arcade swap sides (cap supports would have to move) - not done. plan.md arcade spec: 30 mm body, panel hole D28.5, snap-in clip (no screws); old hole is D30.7. Used 30.7 (= last housing); change `CONTROLS['arcade']['d']` if the button is tight.
- Inside clearance at (62.5,42): flat top to r22 (min 61.983), ceiling 59.58-59.6, nothing from dome, cap or sled between z 20 and 59.5 within r19 (covers a ~38 OD clip ring and ~30 deep body); inner wall x 105 (z 50) / 109 (z 36); gaps from a r19 ring: screen boss 7.3, button R2 9.2, window 16.9, LiDAR 20.4.
- **Joystick** now D27 (plan.md KY-023: cap OD ~25, panel hole D27) at (-62.5,42) = old hole centre, between peg axis (-62) and post-box centre (-63); cap travel clearance 1 mm per side, so +-0.5 offset eats half of it.
- Dome rebuilt from `dome_SMOOTH_base_v2` with all groups. Backups: `neptr_top_dome_4.2_backup_v16` encoders y -65.4 (joystick D30.7, no arcade) | `_v17` + joystick D27 + arcade = final dome. v11-v15 untouched (old y -60.4). Verified: 1 comp, 0 nm, strict 0 (raw 7), parity 0/0, open 555/555 and 222/222, outer skin diff <= 1.1e-5 off footprints. STL `NEPTR_4.2_top_dome.stl` 395,628 tris. FEATURES_4.2.json 42 features.

## 15. 2026-10-15 audit: LED dots, legs display, component fit

Revert points (copies made before the first geometry change): `neptr_HOUSING-4.2_PRE-AUDIT.blend`, `NEPTR_4.2_top_dome_PRE-AUDIT.stl`, `NEPTR_4.2_bottom_cap_PRE-AUDIT.stl`, `NEPTR_4.2_pi_sled_z6/z10_PRE-AUDIT.stl`. Older backups untouched. Code: `NEPTR_4.2_led_dots.py` (generator), `NEPTR_4.2_components.py` (proxy tables + audit + router), `NEPTR_4.2_led_dots.json` (565-dot table). Exec in the live session; both files are plain `exec(open(..).read())` modules.

### 15a. NeoPixel ring dots (library feature `dome_cut_led_dots`, group `led`, batch 13, order 140, split_z 45)
Source: old Phase-4 dome (`PHASE4_top_dome_FINAL_backup`, `DOTRING_CUTTER_REFERENCE`): 705 through-holes = 282 stations x 2-3 dots, around the **whole closed perimeter** (not only the north face; ~2.96 mm station pitch, ~822 mm path), dot D 0.9 / 1.3 (random per dot), 12 long, axis 36.9 deg up (not surface normal), rows z 52.5-57.7 on the shoulder. Firmware (`TEENSYY.ino`: `NUMPIXELS 113`, "144 - 18 - 13", ring wraps `min(dRaw, N-dRaw)`) and `plan.md` (SEZO WS2812B 144/m, 10 wide, ~3 thick) = a flexible strip glued to the INSIDE of the shoulder as a closed ring. Old inner contour at z 54.0 = 785.06 mm = 113 x 6.944 (confirms the old strip height).
- New form: inner-skin contour length hits 784.72 mm (113 x 6.9444) at **z_c = 56.202** (new inner skin is 0.018 longer per mm than old, so the strip sits 2.2 mm higher). Centreline = that contour, strip 10 wide along the shoulder profile, top edge 11 mm below the ceiling start. Min plan radius of the centreline 14.8 mm (convex corners) / 91 mm (waist): a 10 mm strip does not edge-bend that tight; expect cut + jumper or a kinked corner (NOT solved; physical strip decision).
- Dots: 565 = **5 per LED cell** (cell = 6.944 mm of arclength, start at +y north centre, CCW; pixel order ASSUMED), seed **4242**, positions by dart throwing in (arclength c, profile offset s in +-3.8), min 3D edge gap >= 1.2 (measured 1.32), D from {0.9, 1.3}, 16-gon cutters (flats >= nominal), axis = local inner-surface normal (elevation 50-66 deg on the shoulder fillet), cut -3 / +5.5 around the inner skin point (wall along axis 2.400). Phase of 22 cutters re-rolled (spec.phase_fix) to avoid sliver/strict self-int faces. All params + seed in the `spec` prop; table in `NEPTR_4.2_led_dots.json`.
- Verified: 565/565 open (5 rays each; all blocked on the pre-cut dome), strict self-int 0, parity 0/0, non-manifold 0, outer skin diff off-footprint 1.5e-5, nearest other feature >= 7.7 mm (spot-face HDMI), 3 mm rule met everywhere.
- Ring proxy (`comp_neopixel_ring`, 10 x 3, lifted 0.45 off the wall for the flat-strip sag): hits nothing; penetrates the dome 0.16 mm at one spot (LOW).
- Assumed: strip glued flat to the inner shoulder, LEDs facing the wall, no groove or clip (none existed in the old final dome). The clear silicone U-channel in plan.md is the seam diffuser, not part of this.

### 15b. Legs display
`neptr_pi_sled_z6` at x +300, `neptr_pi_sled_z10` at x +390 (y, z unchanged), z10 now visible. Installed pose: custom prop `installed_matrix` (row-major 4x4, = identity) on each + hidden copies `comp_pi_sled_z6/z10_installed` in `COMPONENTS_4.2`. Clearance checks must use those.

### 15c. COMPONENTS_4.2 (hidden): 46 objects `comp_<part>`, props `dims` (json per piece), `source` (json per piece; `ASSUMED` where no doc/brief/measurement), `attach` (cap|dome)
Sources: brief numbers (Pi 85x56, HAT 65x56.5x35, screen board 98.58x76.54 / LCD 3.9 / header 13, EC11 12x12, jack nut ~12, arcade ring ~38), plan.md (PBS-110, KY-023 34x26, TOF400C VL53L1X 20x12, MPU9250 15x25, piezo D20, Teensy 61x18, WS2812B), handoff (positions, nut keep-outs, bosses, supports), sled posts (Pi board position x -28.65..27.35, y -50.8..34.2, long axis y; Pi USB-C/HDMI face west, USB/Ethernet face south). Pi jack positions from the RPi 5 mechanical drawing (from memory).
ASSUMED (no data): Pi 5 cooler 30x30x11; GPIO riser 14 mm ("fan riser") and "35" read as total HAT stack above the Pi PCB top; plug overmolds (USB-A 13x7.2x25, RJ45 15x13x30, USB-C 12x7x20, micro-HDMI 9x8x20); Teensy location (66, 2), floor, USB toward south; IMU (-80,-20); piezo discs on the E/W walls at y 0, z 31; jack body D9 x 14 + nut 2.5 inside; USB-A flange 12x30x3 + housing 7x15.2x17; HDMI body D24x25; M23 coupler body D24x30; screen header position (south edge, x +-16.5), LCD outline 57.2x94.6; arcade body/microswitch depth; button/encoder pin depths; FSR as D18.3 head (plan.md's 43.7 square is FSR406, not 402); LiDAR PCB orientation/z; joystick gimbal body 22x22. Not modeled: wires, passives, knobs/caps outside, rubber feet, silicone trim, screen HDMI/touch cable. No amp/speaker/PSU board in plan.md or firmware, none invented.
Ethernet coupler is centred z 31 (the D23.4 hole is a D-hole, flat 10.3 below centre); an earlier reading as centre 20.7 gave a false seam conflict.

### 15d. Conflict table (proxy overlap = thickness of the common region; shells = deepest proxy-surface point inside the solid; known pass-throughs ignored: Pi holes over sled pins, KY-023 holes over cap pins, screen holes over pegs)
| A | B | before | after | note |
|---|---|---|---|---|
| usbA_A/B/C flange | dome wall | 1.16 HIGH x3 | 0.13 LOW | wall curves in 1.7 mm by z 16/46; flat seats added |
| jack_00..31 nut (8) | dome wall | 0.99 MED x8 | 0.04 LOW | wall curves in 1.0 by z 18/44; flat seat added |
| hdmi / eth nut | dome | 0.00 | 0.00 | spot-faces already flat |
| neopixel ring | dome | 0.16 LOW | 0.16 LOW | flat strip sag, held off 0.45 |
| every other proxy pair (convex) | | 0 | 0 | |
| dome lowering over cap-mounted parts | | 0 | 0 | joystick/Pi stack/Teensy/IMU vs dome-mounted parts + shell |
Tight but not touching (gap, mm): Pi RJ45 top vs HAT 0.53 (stack 14 mm riser), HAT vs riser 0.18 (seated), USB flange vs HDMI / Eth nut 1.0, jack nut to nut 2.0, screen PCB vs button nuts 2.06.
Cable routes (A*, 8 mm channel, 2 mm grid, all 16 found): USB-C 64 mm, HDMI 71, Ethernet 96, USB-A 59 / 42 / 45, Teensy USB 54, TRS 50, 8 jack cables 44 each straight to the HAT north edge. Interior bend radius estimates: USB-C FPC 4.7 (flat FPC, ok), USB-A 7-9 (below a 25 mm rule; plug straight run is modeled, bend after it is not constrained), Teensy 23, Ethernet 90.

### 15e. Changes made (all via the feature library + `rebuild`, EXACT, verified per 5)
1. v18 `neptr_top_dome_4.2_backup_v18`: `dome_cut_led_dots` (565 dots), strict self-int 0 after re-phasing 22 cutters.
2. v19 `neptr_top_dome_4.2_backup_v19` = final dome: full rebuild from `dome_SMOOTH_base_v2` with 5 new ADD features before the port cuts: `dome_add_jack_seat` (box x +-27.55, plane y 100.07, z 17.5-44.5; 0.99 thick at z 31, 0 at the ends) and `dome_add_usbA_seat_L1/L2/R1` (14 -> 12 wide, plane y -99.5, z 15.5-46.5, 1.56 thick at z 31). Reason: the flat nut and flange faces did not seat on the barrel wall. Overlapping adds in one batch (seat vs HDMI/Eth boss disc at x +-6.. 7) made EXACT return an empty mesh; seats narrowed to 12 so they stay 0.55 apart. Plane 100.05 / x 27.6 made two zero-area faces; 100.07 / 27.55 does not.
   Verified: 246 310 v, 1 component, 0 non-manifold, strict self-int 0 (raw touch 1313 = the new hole rims, was 7), parity 0/0, zero-area 1 (pre-existing), wall skin diff 0, every port/USB/screw hole open, 565/565 dots open.
3. Cap unchanged (`cap_backup_v12`). Sleds unchanged.
No control, port position or wall hole moved.

### 15f. Backups map
dome: `_v17` (pre-audit, final of 14b), `_v18` + dots, `_v19` + seats (live). cap: `_v12` (live). Proxies: `COMPONENTS_4.2`. Revert: copy `neptr_HOUSING-4.2_PRE-AUDIT.blend` over the working file, or set `neptr_top_dome_4.2.data = neptr_top_dome_4.2_backup_v17.data` and restore `*_PRE-AUDIT.stl`; to drop only the dots use `rebuild('dome', .., exclude_groups=['led'])`.
STL: `NEPTR_4.2_top_dome.stl` 495 040 tris (was 395 628), cap 190 648 (unchanged), sleds 8 716 each (unchanged). Renders: `scratchpad/renders/led_dots_top.png`, `components_xray_iso.png`, `components_xray_dome_ghost.png`, `legs_display.png`.

## 16. 2026-10-15 LiDAR brackets (dome only; cap = `cap_backup_v12`, sleds unchanged)

**Update: aim is now 15 deg outward and the dome is v23, see §17a. Numbers below describe the 0 deg v20 build.**

Revert points: `neptr_top_dome_4.2_backup_v19` (pre-brackets, = §15 final) in `BACKUPS_4.2`; `NEPTR_4.2_top_dome_PRE-AUDIT.stl` is older (pre-dots). Nothing in `ARCHIVE_PHASE4`, `Collection`, `LIDAR_PARTS` touched.

### Module and sources
- **TOF400C VL53L1X** (4-pack, 2 used on separate I2C buses), plan.md "TOF400C VL53L1X ToF Sensor": PCB **20 x 12** stadium, sensor housing ~12 x 8 on the board, **2 x D2 (M2) holes, ~16 mm c-c on the long axis** (one per ear). Firmware: Pololu `VL53L1X`, Long mode, CC30 / CC36. Not in docs (ASSUMED, flagged in `comp_lidar_*` source): PCB thickness 1.6, aperture body D8 x 1.9 above the PCB top, aperture at the PCB centre, connector end/header position (so the volume under the PCB is left fully open). Optical: VL53L1X FOV 27 deg full = 13.5 deg half.
- **Old mount (found in `STAGE13_top_dome_with_lidars_backup` = `MOLD_3_lidar_screws`, `SURG_1_no_lidar_screws`, `LIDAR_SCREWS_FILLED_FINAL`):** window ~D11 plus two small screw holes (~D1.75) at x = +-8.3 about the window centre (L: -34.2 / -17.6, R: 17.6 / 34.2 at z 61), i.e. M2 from outside through the 2.4 skin, no bosses, no inside bracket. The window and both screw holes were **tilted ~15 deg outward in XZ** (hole walls drift 0.57 mm over 2.1 mm of skin, L toward -x, R toward +x). The later `LIDAR_SCREWS_FILLED` / `LIDAR_WINDOWS_CIRCULAR_FINAL` / `PHASE4_top_dome_FINAL` removed the screws and made the windows **vertical D10.5** (= the 4.2 windows), plus a thickened collar around the window (underside z 55.4, x 18-31, y 60-73.5). So: tilt 15 deg outward existed only in the superseded STAGE13; the final old dome is vertical. `LIDAR_PARTS` is empty (no part geometry exists).
- Aim used: **0 deg (straight up)**, matching the final old dome and the 4.2 windows. 15 deg outward is one value away (below).

### Design (library features, group `lidar_mount`, batch 14, order 150/151)
- `dome_add_lidar_bracket_L/R`: 2 hollow posts per sensor at the PCB hole positions (x = +-25.7 +-8, y 67.2): OD 4.4, tip (PCB seat) z 56.2, top z 60.6 (embedded 1.0 into the 2.4 skin; outer skin untouched, 1.4 left), blind **D1.7 self-tap pilot** from the tip, cap at z 59.2 (3.0 deep, 0.4 under the ceiling). Screw: **M2 x 4 self-tap from below** through the PCB hole (PCB component side against the post tips; PCB 54.6-56.2, aperture top 58.1, hole axis crosses the window skin mid-plane z 60.8 at (+-25.7, 67.2)). Web between a post and the D10.5 window: 0.55. Aperture body D8 in the D10.5 window: 1.25 radial.
- Print: dome is assumed to print top-skin-on-bed (rim up) -- the handoff never states it; posts then grow up from the bed with axis within the aim angle of vertical, no supports (at 15 deg: 15 deg lean). If the dome prints rim-down, the posts hang 3 mm from the ceiling and need a support or brim.
- Cable exit: nothing under the PCB except the screws; the whole volume below z 54.6 around the sensors is free toward the interior (north wall 27 mm away, screen board edge y +47 vs PCB y 61-73).
- No cutters: pilots are built into the post (any separate pilot-cut batch, or a bore ending on the ceiling plane, left non-manifold edges on the bore/ceiling circle after the next boolean).
- **Aim parameter:** `LIDAR['angle_deg']` in `NEPTR_4.2_apply_features.py` (+ = outward, L toward -x, R toward +x). Change, then `create_lidar_features()` and rebuild/`try_batch('dome','lidar_mount')` + `commit`. `comp_lidar_*` proxies take their pose from the feature `spec` (box now supports `ry`). Tested at 15: clean (nm 0, strict 0), FOV clear to 23.5 deg half-angle (window needs no chamfer); posts at 15 deg have seat z about 54.3 / 58.4 and lean 15 deg.

### Method (read before touching the dome again)
- `_box_boolean` (new): EXACT on a **local slab only** (feature prop `split_box` = x -42..42, y 60..74.5, z > 50): bisect on the 4 side planes, bridge the slab's outline loops, solve alone, delete bridge faces, weld at 1e-6. Reason: any whole-part / whole-upper-part solve after the LED dots (batch 13) left 8 strict self-intersections in the dot shoulder (same noise as the §14 note). Costs ~5.5k extra tris from the plane cuts (flat top, collinear). Everything outside the box is identical: max vertex-to-surface distance outside the box 1.7e-5 both ways.
- Post polygon phase **11.9 deg** is tuned: 0.9 / 3.7 / 7.3 / 16.1 / 20.3 left non-manifold edges on the post/ceiling circle after the next boolean in the whole-part flow; 0.9 and 11.9 are both clean in the slab flow, 11.9 has the baseline 1 zero-area face.
- Library order: batch 14 comes after the dots, so a full `rebuild('dome', ...)` reproduces the live mesh. Note: `execute_blender_code` namespaces do not persist between calls: `exec(compile(open(helper).read(),'f','exec'), globals())` in every call.

### Verification (dome v20)
251 865 v / 506 150 tris, 1 component, 0 non-manifold, 0 loose, zero-area 1 (pre-existing baseline), strict self-int 0 (raw touch 1 312 vs 1 313 live), ray-parity odd columns 0 / 0, windows open 222 / 222, outer-skin diff 1.5e-5 (0 cells > 0.01), winding consistent (0 duplicate directed edges), ceiling normals all -z.
- **FOV:** rays from the aperture top at r 0 / 1.25 / 2.5 over a cone: first blocked half-angle **35.5 deg** (live dome without brackets: also 35.5, so the posts block nothing; required 13.5). At 15 deg aim: 23.5 deg.
- Proxy re-check (`comp_lidar_L/R` rebuilt: PCB 20 x 12 x 1.6 seated on the tips, sensor D8): pairwise vs all 93 pieces 0 overlaps; shells vs dome (with posts) / cap / sled 0; nothing within 3 mm; PCB y 61.2-73.2 vs screen board y <= 47 and screen bosses (+-33, 42.9) 18 mm away.

### Files / backups
`neptr_top_dome_4.2_backup_v20` (note prop) = live. STL `NEPTR_4.2_top_dome.stl` **506 150 tris** (was 495 040), cap/sleds unchanged. `FEATURES_4.2.json` 49 features (adds `split_box`). Render: `scratchpad/renders/lidar_bracket_inside.png` (straight-up ortho from inside, PCB + sensor proxy as wire, posts and D10.5 windows visible). Scene organisation unchanged (NEPTR_4.2 = dome, cap, X1C plane, PI_SLED displaced; helpers purged).

## 17. 2026-10-15 final package: 15 deg LiDAR, old-feature diff, final verification, print

### 17a. LiDAR aim = 15 deg outward (coordinator decision)
`LIDAR['angle_deg'] = 15.0` (now the default in the helper). Rebuilt on `dome_backup_v19` (not on v20/v21) after fixing the slab method (below): `neptr_top_dome_4.2_backup_v22`. Seats L (-32.24 / -16.78, 67.2) z 54.29 / 58.43, R mirrored. FOV: rays from the aperture top (r 0 / 1.25 / 2.5, cone around the tilted axis) are clear to a **23.5 deg half-angle** (VL53L1X needs 13.5), no hole chamfer needed. The vertical D10.5 window stays; the D8 sensor body (tilted) sits inside it with no shell overlap, proxy pairs 0, shells 0. The 5/222 "holes_open" misses are the vertical ray test hitting the outer post 1.3-2.4 mm under the window edge (z 54.9-56.0), not the window. v20 (0 deg) and v21 (15 deg, old slab method) are kept, superseded.
- **`_box_boolean` fixed:** the first version bisected the whole dome along the 4 slab planes (slicing across the LED dots, +5.5k tris, slivers). Now only faces overlapping the box (+3 mm) are cut, and every cut n-gon is triangulated (BEAUTY) so no collinear-vertex slivers: v22 has 0 zero-area faces (v19 had 1), raw touch pairs 5, strict self-int 0, parity 0/0.

### 17b. Old Phase-4 dome/cap vs 4.2 (read-only: `PHASE4_top_dome_FINAL_backup`, `TRULY_FINAL_cap_backup`; surface-distance clusters both ways + hole masks at 0.6-1.5 mm in the top, N, S, E, W directions)
| old feature | decision | reason |
|---|---|---|
| encoders ENC6/ENC7 D7.5 at (+-85.4, +75) | **RESTORED** (`dome_cut_enc_L2N/R2N`, group `encoders_n`, batch 15/16, v23) | plan.md/firmware have ENC1-7; 4.2 had 5. Top flat to y 78.5 (61.99 at the hole edge), LED dots >= 12.7 away, own local slab, proxies `comp_enc_L2N/R2N` (body 12x12x7: 0.15 mm corner overlap with the ceiling curve, LOW) |
| 4 rectangles 20x30 at (+-22, +-27) | skipped | the old screen window (4 openings); 4.2 has the single 53.24 x 88.55 window |
| encoders at y -60.4 (5) | skipped | moved to -65.4 by decision |
| joystick D30.7 ring | skipped | D27 by decision (KY-023 cap OD ~25) |
| LiDAR window collar (underside z 55.4, x 18-31, y 60-73.5) and the STAGE13 tilted M2 screw holes | skipped | replaced by the post brackets |
| old USB-C (-41.5, 31) D16 / TRS (+41.5, 31) D10 north holes; old south port layout (USB-A at +-56.5/82, HDMI/Eth at -70/+70) | skipped | superseded by the §12 layout (USB-C/TRS at +-75, south swapped) |
| old Pi leg cones on the cap | skipped | sled replaces them |
| dome/cap corner and shoulder lumps (SE/SW shoulder 4-9 mm, skirt 4-6 mm, cap corners 4.7 mm) | skipped | analytic form (§13) replaces the old cage; not features |
| jack holes D8.3 north, LED dots, spot-faces, seats, corner blocks, joystick supports, sled pins, grill | already in 4.2 | |
No old mount posts, clips, cable-tie points or extra screw bosses exist in either part (cap interior = Pi legs + joystick cones + corner bosses; dome interior = collars only). Not added by decision: Teensy holder, FSR backing, LED channel.

### 17c. Final verification (all installed copies together, final objects)
- Dome v23 248 795 v / 500 018 tris: 1 component, 0 non-manifold, 0 loose, 0 zero-area, strict self-int 0 (raw touch 1), parity 0/0. Cap 95 200 v / 190 648 tris: 1 comp, 0 nm, strict 0 (raw 7), parity 0/0, 85 zero-area = pre-existing baseline. Sleds (installed): 1 comp each, 0 nm, strict 0 (zero-area 11 / 9 area < 1e-9, pre-existing micro faces from the pocket booleans).
- **Holes open:** every non-LED dome/cap cutter volume sampled (dome 211 986 points, cap 11 167 points, 0.5-0.9 mm): 0 inside the final solids. 565 / 565 LED dots open along their axes. Top-skin windows 222/222 (+222 for the north encoders).
- **Assembly interference:** dome-cap 359 touching triangle pairs (the seam plane z 12.4), **0 piercing**; cap-sled 1 801 pairs (pins in holes), **0 piercing**; dome-sled 0 pairs. Vertex-in-solid counts (300 / 288 / 31) are the coplanar rim and pin contact points. Component proxies (93 pieces): pairwise overlaps 0; shells only LOW: USB-A flange 0.13, jack nuts 0.04, encoder bodies L2N/R2N 0.15 (ceiling curve).
- Backups (all in `BACKUPS_4.2`, notes set): dome `_v22` LiDAR 15 deg, `_v23` + ENC6/7 (= final). `_v19`/`_v20`/`_v21` untouched. Cap `_v12` final.

### 17d. Print package
`PRINT-4.2.md` has the per-part file, orientation, supports, X1C settings, hardware list and assembly order. Orientation decision (measured): **dome top-on-bed** (supports only on the outer shoulder fillet, 14k mm2 in the first 8 mm; rim-down = 33.8k mm2 + 200 mm bridge), **cap floor-down** (supports on the bottom outer fillet 14.5k mm2), sleds flat. No feature needed to change: posts/bosses/pegs grow up from the bed, the pilot bores open at the post tips, the holes are vertical through the flat top; the only residuals are the 4 corner-block free ends (~200 mm2 each) and the tops of the D16-23 wall holes (support enforcers or bridge).
- **STL files are now in print orientation** (dome flipped 180 deg about X: z 0-49.6, rim up; this changes the earlier convention where `NEPTR_4.2_top_dome.stl` kept the model Z 12.4-62). Dome 500 018 tris, cap 190 648, sleds 8 716 each (now z 0-6.6 / 0-10.6 instead of 2.4-9 / 2.4-13), coupon 49 746.
- **`NEPTR_4.2_fit_coupon.stl`** (generator `NEPTR_4.2_fit_coupon.py`, `build_coupon()`): 160 x 110 x 2.4 plate with the real library cutters: arcade 30.7, joystick 27, HDMI 21.5, Ethernet 23.4 D-hole, USB-C bore 16.3, LiDAR 10.5 + the real L post pair (15 deg), FSR 6x8 at 10 deg, TRS 10, jack 8.3, button 7.4, encoder 7.5, USB-A 7x15 + 2 x D3.5, a 5-dot LED cell (vertical holes; real ones are 35 deg tilted in the shoulder), 0.5 mm engraved labels on the top face; plus two seam pieces cut from the real parts around the (+89.75, -89.76) corner (cap z 0-14 with boss, bore, recess; dome z 12.4-26 with rim, corner block, skirt). 3 components, 0 non-manifold. Render: `scratchpad/renders/fit_coupon.png`. The coupon object is not kept in the blend.
- Scene: NEPTR_4.2 visible = dome, cap, X1C plane, PI_SLED displaced; helpers and orphans purged; ARCHIVE_PHASE4, `Collection`, `LIDAR_PARTS` untouched. `FEATURES_4.2.json` 51 features.

## 18. 2026-10-15 polish: thinner LED band, sled_fit, final audit

Revert points (file copies made before any change): `neptr_HOUSING-4.2_PRE-POLISH.blend`, `NEPTR_4.2_top_dome_PRE-POLISH.stl`, `NEPTR_4.2_bottom_cap_PRE-POLISH.stl`, `NEPTR_4.2_pi_sled_z6/z10_PRE-POLISH.stl`, `NEPTR_4.2_fit_coupon_PRE-POLISH.stl`; `NEPTR_4.2_led_dots_v1.py/.json` and `NEPTR_4.2_components_v1.py` keep the pre-polish generator / table. Older backups untouched.

### 18a. LED dots v2 (thin band, dome `neptr_top_dome_4.2_backup_v24`)
- Same centreline (z_c 56.2022 inner contour, regenerated on `dome_backup_v17` = pre-dots dome, identical contour), same full loop, 113 cells, 5 dots per LED = 565, same sizes D0.9 / D1.3 (287 / 278), same seed 4242 / `default_rng` scheme, 16-gon cutters, same axis rule (local inner-skin normal), cut -3 / +5.5.
- Scatter (`band_half` +-3.8, `gap_min` 1.7) replaced by **3 explicit rows** at s = -0.9 / 0 / +0.9 mm with +-0.2 jitter, c random in the cell (spill +-0.5 / wider cells only if a dart cannot be placed), dart-thrown against hole-to-hole web target 0.72 (2D, circumradius metric). Params `LED['rows'|'row_pitch'|'jit_s'|'web_min'|'web_target']` in `NEPTR_4.2_led_dots.py` (+ spec + `NEPTR_4.2_led_dots.json`, `FEATURES_4.2.json`).
- **Band height (hole edge to hole edge): 8.89 -> 3.50 mm** (centres 7.59 -> 2.20). **Min web (3D, axis segments through the 2.4 wall, circumradius of both 16-gons): 1.298 -> 0.628 mm** (>= 0.6 required). Feasibility sweep (web / placed): row pitch 1.0 0.62 / 565, 0.9 0.63 / 565, 0.8 0.68 / 565 (rows then overlap visually, not used), 0.9 at web target 0.76 drops 2 dots.
- Rebuilt with the helper (`rebuild('dome', ...)` full, 44 features, all batches): 248 852 v / 500 132 tris, 1 component, 0 non-manifold, 0 loose, 0 zero-area, strict self-int 0 (raw touch 2), parity 0/0, 565/565 dots open (2825/2825 rays). Two dots needed a re-rolled cutter phase (idx 215, 544, `spec.phase_fix`) to remove 4 sliver faces. **Everything but the dots is identical to v23** (symmetric nearest-surface distance, 108 873 samples outside 3 mm of any old/new dot: max 0.0).

### 18b. Rebuild check
Dome rebuilt by the library helper from `dome_SMOOTH_base_v2` with ports, seats, bosses, controls (ENC6/7 at (+-85.4, 75), arcade D30.7, joystick D27, FSR, buttons, encoders y -65.4), LiDAR brackets at 15 deg (local slab `_box_boolean`), cap unchanged (`cap_backup_v12`). Non-dot geometry max deviation from v23: 0.

### 18c. `neptr_pi_sled_fit` (Pi legs sized to the ceiling)
Stack, Pi PCB bottom -> screen PCB back (script `NEPTR_4.2_pi_sled.py`, `STACK`):
| item | mm | source |
|---|---|---|
| screen PCB back = bosses end 55.7 - PCB 1.6 | 54.1 | handoff §11 + proxy `screen.pcb` (PCB thickness ASSUMED 1.6); LCD 3.9 on top reaches 59.6 = ceiling underside (check) |
| Pi PCB | 1.6 | RPi 5 standard (proxy `pi5.pcb`) |
| HAT stack above Pi PCB top (incl. GPIO riser 14) | 35.0 | brief "65 x 56.5 x 35", reading as total ASSUMED; riser 14 ASSUMED (split only) |
| tallest riser, HAT top -> screen PCB back | 13.0 | **ASSUMED** = brief "screen header 13"; no other source exists |
| stack sum | 49.6 | |
| preload | 0.3 | requested |
**Pi PCB bottom z = 54.1 + 0.3 - 49.6 = 4.8** (= frame top: the shoulder height is 0, the Pi sits on the D6.6 boss tops). General rule: z = 17.8 - riser_tall. Sensitivity: riser 8.5 -> z 9.3, 7.8 -> 10.0, 11.8 -> 6.0. The z6 / z10 sleds would over-compress this stack by 1.5 / 5.5 mm. If the real riser is <= ~12.8 the shoulder reappears (min shoulder 0.2 mm; 0.05-0.2 raises). **Needs one caliper number: the real HAT-top -> screen-PCB-back distance.**
- Built by transforming the z10 sled (identical below 4.8 as z6): vertices above 4.8 shifted by (Zs - 10); at Zs <= 4.85 the shoulder wall is removed and the pin base is refilled with an angle-merge annulus. Reproduces z6 at Zs 6.0 to 4e-6 mm.
- **Design change vs z6/z10:** the Pi's two micro-HDMI overmolds (ASSUMED 9 x 8, y 8.2 / -5.3, west face) dip to z 4.1 at Zs 4.8 and hit the left rail (0.7 mm); the rail USB-C gap (y 14-32) is extended down to y -10.8 (`HDMI_RELIEF`, automatic when the plug bottom < 5.1). Verified 0 audit overlap after.
- Mesh: 4 164 v / 8 340 tris, 1 component, 0 non-manifold, strict 0, zero-area 9 (same pre-existing micro faces as z10), overhang 55 mm2 (0.1 pocket underside only, flat, no supports), 70 x 89 x 5.4 printed. Object displayed at x +480 next to z6 (x 300) and z10 (x 390), props `installed_matrix` (identity, installed pose = local coordinates), `pi_bottom_z`, `stack`; hidden installed copy `comp_pi_sled_fit_installed`; backup object `neptr_pi_sled_fit_backup_v01`. STL `NEPTR_4.2_pi_sled_fit.stl` 8 340 tris.
- Proxies: `NEPTR_4.2_components.py` now defaults to `sled_z 4.8` and has `comp_screen_riser` (5.5 x 50.8 x 13 on the GPIO row, ASSUMED footprint); proxy audit (98 pieces): the only pair overlap is that riser into the screen PCB 0.3 (= the preload; 0.4 sampled), shells LOW as before (USB-A flange 0.13, jack nuts 0.04, ENC6/7 bodies 0.15, ring strip 0.16); sled_fit vs plugs 0 after the relief; lowering conflicts 0.

### 18d. Final audit
- Dome v24 / cap / sleds z6, z10, fit (installed): 1 component each, 0 non-manifold, strict self-int 0 (dome raw 2, cap 7, sleds 19 / 17 / 5), dome + cap parity 0/0, dome zero-area 0, cap 85 (baseline). Proxies (50 objects, 98 pieces): 0 non-manifold, 0 loose.
- Cutters open: dome non-LED cutter volumes 24 550 sampled points, 0 inside the solid; cap 401 / 0; 565/565 LED dots (2 825 rays). Assembly: dome-cap 359 touching pairs (seam), 0 piercing; cap-sled_fit 1 748 pairs (pins in holes), 0 piercing; dome-sled 0 pairs. Cable routes: all 16 found with the fit sled (ethernet 96 mm, HDMI 71, USB-C 65 / R 5, USB-A 65 / 48 / 45, Teensy 54, TRS 50, jacks 44).
- Print orientation: dome overhang > 45 deg 15 859 mm2 (was 15 857), unchanged.
- STL (print orientation, binary): dome 500 132 tris (z 0-49.6), coupon 49 750, sled_fit 8 340; cap 190 648 and sleds z6 / z10 8 716 unchanged. Coupon dot cell now = one real LED cell of the v2 table (5 dots, 0.9 / 1.3, three rows).
- Scene: NEPTR_4.2 visible = dome, cap, X1C plane, PI_SLED (z6 x300, z10 x390, fit x480); helpers / orphans purged; ARCHIVE_PHASE4, `Collection`, `LIDAR_PARTS` untouched. Renders (ray-cast, flat shaded; the EEVEE / Workbench renders of the dome showed no holes because the cavity is the same colour): `scratchpad/renders/led_dots_v2_closeup.png`, `sleds_display.png`.


## 20. 2026-10-18 Ethernet hole enlarged + M3 insert pilots (dome `neptr_top_dome_4.2_backup_v25`)

Revert points: `neptr_HOUSING-4.2_PRE-ETH.blend`, `NEPTR_4.2_top_dome_PRE-ETH.stl` (= v24). Cap, sleds, coupon untouched.

### 20a. Ethernet (PETG test print: M23 coupler did not fit D23.4)
- `dome_cut_ethernet` edited in place (same name/order/batch 5): D-hole **D24.2, flat 10.7 below centre** (was 23.4 / 10.3), same centre (72.9, z 31), same flat orientation. Cutter bbox x 60.8-85.0, z 20.3-43.1, y -107..-99. FEATURES_4.2.json updated.
- Spot-face (cutter D30.0, nut_d 29) and `dome_add_eth_boss` unchanged. Seat ring (spot-face edge to hole edge) **min 2.8-2.9 mm** at the sides/top (was 3.2), 4.3 below the flat. Wall behind the seat plane (y -100.4) to the outer skin **min 1.64 mm** (top arc, z 45.6; was 1.76). No growth needed.

### 20b. M3 heat-set insert pilots (dome side)
- Finding: v24 dome corner blocks were solid; the cap has a D3.6 through-bore + D5.98 head recess (floor z 4.4) + D8 boss top z 12.2, no insert. The PRINT-4.2 text put the insert in the cap boss; as built, the M3x8 screw tip (head floor 4.4 + 8 = 12.4) ended AT the seam and clamped nothing. Inserts go in the DOME, pressed from the seam face (dome printed upside-down: pilot opens upward, no supports).
- Library: `dome_cut_insert_pilot_NE/NW/SW/SE` (group `inserts`, batches 17-20, orders 200-203, prop `split_puck` = [cx, cy, 3.4, 12.4, 19.0]). Axes (+-89.75, +-89.76), coaxial with the cap bores. **D3.70** (insert OD 4.0 - 0.3), **4.8 deep** (z 12.4 -> 17.2; insert 4.0 + 0.8 melt overflow), 0.5 x 45 deg lead-in (mouth D4.7), flat floor. Measured on the mesh: R 1.85-1.854, floor z 17.2, solid above to 19.9 (2.7), radial wall to the nearest outer surface **3.04 mm**.
- Screw: from the cap recess floor to the seam is 8.0 mm, so M3x8 has 0 engagement. **M3x12 = 4.0 mm engagement** (tip z 16.4, 0.8 above the pilot floor); M3x10 = 2 mm; do not exceed M3x13. Order M3x12. PRINT-4.2.md still says M3x8 / insert in cap boss (not edited).
- Insert install: iron straight down from the seam face, stop flush to 0.3 below.

### 20c. Helper changes (`NEPTR_4.2_apply_features.py`)
- `_puck_boolean` + prop `split_puck` (flat seam-face patch closed into a puck, solved alone, welded back). The box slab bridged wrongly at the corners (400+ non-manifold edges per corner).
- `_box_boolean` now triangulates slab n-gons and runs `_fix_slivers` (EXACT output n-gons with collinear verts tessellate to zero-area triangles; 21 appeared at the Ethernet cut).
- Ethernet applied with `_box_boolean(w, [dome_cut_ethernet], 'cut', (58.33, 88.77, -105.5, -97.21, 12.0))` on v24 (the library batch 5 has no slab prop; a full `rebuild` would solve batch 5 whole-mesh as before).

### 20d. Verification (v25)
251 267 v / 504 962 tris; 1 component, 0 non-manifold, 0 loose, 0 zero-area, strict self-int 0 (raw touch 2 = v24), ray parity 0/0. Outside the Ethernet and 4 pilot slabs (+0.5): symmetric nearest-surface distance to v24 max 2.4e-5 (< 1e-4). Ethernet hole open at the new size (104/104 rays), closed just outside the D (74/74) and below the flat (27/27). Pilots: 4/4 measured as above.
STL (print orientation x, -y, 62 - z; z 0-49.6): `NEPTR_4.2_top_dome.stl` 504 962 tris, copied to `legacy/NEPTR phase4/shell/PRINT_4.2/1_NEPTR_4.2_top_dome.stl`.

## 21. 2026-10-18 LiDAR flat seats replace the M2 posts (dome `neptr_top_dome_4.2_backup_v26`)

Revert points: `neptr_HOUSING-4.2_PRE-LIDARSEAT.blend`, `NEPTR_4.2_top_dome_PRE-LIDARSEAT.stl` (= v25). The §16 post design survives only in the backups (v20-v25) and the PRE files; `dome_add_lidar_bracket_L/R` are gone from the library.

### 21a. Seat geometry (library `dome_add_lidar_seat_L/R`, group `lidar_mount`, batch 14, orders 150/151; params in `LIDAR`)
- Axis/aim unchanged: 15 deg outward (`LIDAR['angle_deg']`), board seat plane = the old PCB top-face plane (centre (+-24.51, 67.2, 56.36), normal = sensor axis), so the sensor-to-window geometry is identical to §17a.
- Pad = prism grown from the ceiling to the seat plane, footprint 21.6 x 14.8 in the board plane, top at z 60.6 (1.0 into the skin, outer skin untouched), sides normal to the board plane (lean 15 deg). Bbox x -37.45..-14.45 (L, R mirrored), y 59.8-74.6, z 52.63-60.6.
- Locating pocket 20.4 x 12.4 x 0.8 (0.2 play per side); floor = seat plane (PCB component face rests on it), lips 1.2 thick on 3 sides, tips 0.8 below the floor (half the PCB thickness). Open end (no lip, pad flush with the board end) = connector side, ASSUMED toward the centre line (`LIDAR['open_end']` = 'inboard' | 'outboard').
- Aperture: stadium 12.6 x 8.6 (long axis = board long axis) along the sensor axis, through the pad; covers either a D8 body or a 12 x 8 housing. Above z 59.0 a vertical D11.3 cylinder is removed from the pad so no pad material sits inside the D10.5 window (r 5.25). No window change: the vertical D10.5 window does not clip the cone.
- Neighbours (proxy distance from the pad): LED strip 10.4, arcade 12.1, screen 12.7, joystick 12.9 mm. PCB + sensor proxies: 0 overlaps with the dome. Pad thickness above the seat plane at the high (inboard) end 0.65 mm of pad + 2.4 skin.
- ASSUMPTION to measure: a header or parts on the component side near the connector end would hit the pad (the pad is flush with the board end). If the header is on the component side, cut the pad back by its depth at that end.

### 21b. Ray fan (dome v26)
From the sensor face (seat + 1.9 along the axis) at offsets -2.5 / -1.25 / 0 / +1.25 / +2.5 mm along the long axis, cone half-angles 15, 18, 20 deg, 73 azimuths each, ray length to z 62.6: **0 / 365 blocked per side at all three angles**. First blocked half-angle: centre 42.5, emitter offsets 23.5 / 49.3 (the window outer edge, z 61.99). Required 15 (VL53L1X 13.5).

### 21c. Print (top on bed)
Pad grows up from the bed-side ceiling, pocket opens upward (floor and lip tips face up), aperture walls 15 deg off vertical, pad sides lean 15 deg: no supports. Only overhang > 45 deg in the lidar slab: the z 59.0 window step ring, 30.8 mm2 total for both sensors, <= 1.4 mm wide bridge, 0.6 mm above the ceiling plane. Max wall lean 15 deg. Board seats by glue or tape (no screws, no M2 hardware).

### 21d. Method and verification
- Posts removed from v25 by deleting the post faces below z 59.55 in the slab and filling the 4 ceiling circles (flat z 59.6); then both pads unioned in one `_box_boolean` slab (`split_box` -42..42, 57.43..77.07, z > 50; box edges chosen off existing vertices, 57.5 left a 1.5e-4 vertex shift).
- Pads are built in the helper (`lidar_seat_geometry`: prism ∩ z<60.6, minus pocket, aperture, window cylinder; EXACT on scratch objects). `create_lidar_features()` regenerates them.
- v26: 252 089 v / 506 606 tris, 1 component, 0 non-manifold, 0 loose, 0 zero-area, strict self-int 0 (raw touch 2 = v25), ray parity 0/0, windows open 222/222. Every vertex outside the slab matches v25 exactly both ways (max distance 0); top skin outside the windows 8e-6.
- STL `NEPTR_4.2_top_dome.stl` 506 606 tris, copied to `legacy/NEPTR phase4/shell/PRINT_4.2/1_NEPTR_4.2_top_dome.stl`. Render: `lidar_seat_inside.png` (board face-on in the pocket, oblique with board, oblique empty pocket with aperture and window step).
- Scripts updated: `NEPTR_4.2_components.py` and `NEPTR_4.2_fit_coupon.py` look up `dome_add_lidar_seat_*`. The existing `NEPTR_4.2_fit_coupon.stl` is stale (old posts, Ethernet D23.4); regenerate with `build_coupon()` (not done).

## §22 Cap: sled floor pins removed (2026-10-18)
- `cap_add_sled_pins` retired (part = `cap_retired`; kept in the library for reference). The cap was rebuilt via the helper with `exclude_groups=['sled']`. Live `neptr_bottom_cap` = `cap_backup_v13`. Revert: `cap_backup_v12` / `NEPTR_4.2_bottom_cap_PRE-NOPINS.stl`.
- Check: 1 component, 0 non-manifold, strict self-intersection 0. The only change from v12 is that the floor at the 4 pin spots is flat at z 2.4 (was 5.4).
- Joystick supports are unchanged and match the old phase-4 cap exactly: posts at (-76,32), (-76,52), (-50,52), tops at z 50.
- The Pi now stands on 4 free-standing pinned legs (`NEPTR_4.2_pi_legs_*.stl`).
