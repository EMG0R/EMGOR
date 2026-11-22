# Pocket OpGorator — Body Reproduction Recipe v3 (Phase 1)

Complete recipe to recreate the OpGorator handheld-synth housing body. Two stages:
1. STEP 1 — BODYSHAPE: solid pebble body shell (backup "BODYSHAPE").
2. STEP 2 — TOPFACE: control-face height field + smoothing (backup "body_TOPFACE_PERFECT").

## 0. Conventions & constants
- mm, 1 BU = 1 mm. Body origin at geometric center. X=width, Y=length(portrait), Z=depth.
- +Z = FRONT/control face. +Y = NORTH (jacks). -Y = SOUTH (USB-C).
- Final dims: 99 (X) x 160 (Y) x 46 (Z). Watertight, all-quad, max valence 4, Euler 2.
- PCB: 75x140x1.8 centered. PCB_BACK world z = 8.05; PCB front z = 9.85. PCB X +/-37.5, Y +/-70.
- BODY_LEN=160, PCB_LEN=140, PCB_OFF=10. body_Y = PCB_doc_Y + 10; world_Y = 80 - body_Y.
- Heights above PCB back: world z = H(h) = 8.05 + h.
- Subsurf levels 2, LIVE. Depth normalize by RAISING BACK verts (never scale Z).
- MIC: hole ~3mm from PCB edge, ~1mm from north end of mic module -> caps north fillet ~2mm.

## Spec heights (world z = 8.05 + h)
switch 11.15->19.20 @(0.5,by15.1) | neotrellis 13->21.05 grid by85-145 |
oled 15.3->23.35 @(-2.5,by50.25) | mic 15.3->23.35 @(-26.5,by18.2) |
lidar 16->24.05 @(-2.45,by79.43) PEAK | encL 16->24.05 @(-18.77,by72.17) |
encR 16->24.05 @(20.4,by72.17)

## STEP 1 — BODYSHAPE
Subdivided cube (cuts 88-140) -> per-vertex map:
- worldY = cy*80; by = 80 - worldY.
- half-width hw = catmull-rom through HW_PTS =
  [(3,37),(8,43.5),(11,44.0),(45,49.5),(80,43.0),(115,49.5),(149,44.0),(152,43.5),(157,37)].
- CORE_HD = 23.0. Superellipse M_ROUND = 2.8 (flat front + rounded side edges):
  x=cx*hw; z=cz*23; r=(|cx|^M+|cz|^M)^(1/M); if r>0: f=max(|cx|,|cz|)/r; x*=f; z*=f.
- End caps CAP_TR=12 sine-arc blend toward rounded() targets:
  SF(zc=0.0,zh=2.6,xh=6.0,fm=2.6) south; NF(zc=15.35,zh=4.0,xh=26.0,fm=2.6) north.
- Subsurf levels 2 LIVE. Identity object scale. Save backup "BODYSHAPE".
- PCB_ref slab = convex_hull of 8 corners (X+/-37.5, Y+/-70, z 8.05 & 9.85) in REFERENCE coll.

## STEP 2 — TOPFACE (final recipe)
Two viable routes were explored; the WORKING one is the delta/snap method on BODYSHAPE:

### Method that locks exact heights (USE THIS)
Start from a fresh BODYSHAPE copy. Apply, in order:
1. Smooth Y-profile of front-top target via gaussian-blended anchors (no hard plateaus):
   ANCH heights above PCB: (by18,15.3),(by50,15.3),(by72,16.0),(by110-115,13.0), wide sigmas.
   Apply as delta from each row's current front-top z, weighted by front-facing-ness
   (z near row top) * cosine X-weight (flat |x|<24, rolloff 24->40).
2. NeoTrellis pad plateau -> 21.05 flat across grid (by85-145, |x|<28, rolloff to 34).
3. Peak plateau -> 24.05 across encoder/lidar band (by56-82).
4. North mic row -> 23.35 full height, flat band widened to |x|<28 (mic sits at x-26.5 near edge).
5. Switch dip: localized rect, flat floor 19.2, ramps ~6.5-7mm each side; X-localized so it
   does NOT bleed into the mic at x-26.5.
6. GLOBAL smoothing to kill square outlines: Corrective Smooth modifier scoped to a feathered
   top-face vertex group (z>14, |x|<42, faded by z & x; switch weight 0.15), LENGTH_WEIGHTED,
   ~18-25 iters, factor 0.8-0.85, rest_source ORCO, placed BEFORE Subsurf. Then APPLY (bake).
7. Re-snap heights after bake with TIGHT gaussians (sigma ~9-14): lidar/encoders -> 24.05,
   mic -> 23.35, oled, pads -> 21.05; re-deepen switch -> 19.2.
8. SOUTH rolloff: ease pad plateau DOWN gradually from by~110/120 into the south cap (long
   span, no held shelf -> no "raised" look). Monotonic descent.
9. NORTH cap: pull down any hump at by2-10 so the front rolls MONOTONICALLY into the flat
   north (jacks) wall. (Small residual rise OK; switch dip behind it limits full flatten.)
10. Round SOUTH top->cap corners at x~+/-32: targeted Laplacian relax straddling x32 in the
    by144-155 band, tapered to 0 at band edges.
11. ONE depth normalization to 46 by raising back verts (smoothstep on z<mid). Re-snap if needed.

### Verified result (accept tolerances)
switch 19.2 +/-0.3, pads 21.05 +/-0.1, pad_corner 21.05 +/-0.5, oled 23.35 +0.3 (on slope),
mic 23.3 +/-0.1, lidar 24.0 +/-0.1, encoders 24.05. Jack clearance >=13 (actual ~19.5);
PCB interior >=37.5 (actual ~40); depth 46; watertight; max valence 4; Euler 2.

### NOTE on the analytic from-scratch route (do NOT rely on for exact heights)
Baking FH(by) into the superellipse (scaling front z by ztgt/CORE_HD) gives a wrinkle-free
surface but systematically UNDER-shoots peak heights ~0.4-1.0mm because off-center features
(mic x-26.5, encoders x+/-19) lose z to the rounding factor. The delta/snap method above is
the reliable way to hit exact heights. Keep the analytic build only as a smooth STARTING point
if desired, then ALWAYS finish with the tight-gaussian height snaps (step 7).

## Backups (only these two)
- BODYSHAPE (solid pebble body)
- body_TOPFACE_PERFECT (finished top face: heights on spec, smooth, north+south fixed)

## Remaining Phase-1 work
1. Screw cylinders x4 (solid PETG over PCB holes, world X +/-33.5, by14 & by146, <=6mm tall,
   no pre-bore; bottom Ø5, top Ø6; gusset to front inner wall).
2. Solidify to 2.5mm walls (~3mm at seam/cylinders), AFTER top face + cylinders, BEFORE split.
3. Phase 2 EARS; Phase 3 cut 12 holes + split + export STLs.


---

## TOP FACE — CONFIRMED HEIGHTS (locked 2026-06-09 00:51)

Top control face complete and committed to `body_TOPFACE_PERFECT` backup.
All feature heights verified relative to PCB back face (world z = 8.05 + h). Tolerance ±0.35mm.

| Feature      | Spec (above PCB) | Actual | World z | Diff  |
|--------------|------------------|--------|---------|-------|
| switch       | 11.15            | 11.16  | 19.21   | +0.01 |
| neotrellis   | 13.00            | 13.00  | 21.05   |  0.00 |
| nt_corner    | 13.00            | 13.00  | 21.05   |  0.00 |
| oled         | 15.30            | 15.48  | 23.53   | +0.18 |
| mic          | 15.30            | 15.24  | 23.29   | -0.06 |
| lidar        | 16.00            | 16.00  | 24.05   |  0.00 |
| encoderL     | 16.00            | 15.68  | 23.73   | -0.32 |
| encoderR     | 16.00            | 15.67  | 23.72   | -0.33 |

ALL_HEIGHTS_OK = true.

Structural: watertight (non-manifold 0), max valence 4, Euler 2, dims 99 x 160 x 46 mm,
jack clearance 19.33 (>=13 req), PCB interior 40.0 (>=37.5 req).

Surface notes: flat-front BODYSHAPE base; NeoTrellis plateau 21.05 with convex outer-roll
sides ironed via scoped Smooth-modifier (factor 0.85, 24 iters, pre-subsurf) then baked;
NT north corners shaved from 22.51 -> ~21.36 (subtractive, feathered) to remove the
encoder/lidar shoulder bump; mic-side rolloff mirrored symmetric; north oval symmetric.

NEXT: 4 screw-insert cylinders (spec §5), then 2.5mm wall solidify, then Phase 2 ears.


---

# SESSION LOG — Top face finalize, hollow, screw bosses (updated 2026-06-09 03:15)

## CURRENT STATE OF gorator.blend
- Working object `OpGorator_body` = CLEAN HOLLOW SHELL (1,520,836 verts, euler 4 = hollow,
  watertight, max valence 4, ngons 0). NO bosses currently merged in.
- Backups (BACKUPS collection, hidden):
  - `BODYSHAPE` — clean flat-front pebble base (superellipse, the approved oval).
  - `body_TOPFACE_PERFECT` — finished low-poly top face, 47528 base verts + live Subsurf,
    euler 2 (SOLID, not hollow). ALL 8 heights confirmed on spec. The trusted top-face source.
  - `body_SOLIDIFIED` — hollow 2.5mm-wall shell built from body_TOPFACE_PERFECT
    (Subsurf applied -> 760418 verts, then Solidify thickness=2.5 offset=-1.0 even_offset
    -> 1,520,836 verts). euler 4, watertight, max valence 4, ngons 0. Heights preserved.
    THIS IS THE KNOWN-GOOD HOLLOW SHELL. Restore from here.

## COORDINATE CONVENTIONS (LOCKED)
- mm units, 1 BU = 1mm. Body origin at geometric center.
- X = width, Y = length (portrait), Z = depth. +Z = FRONT/control face.
- +Y = NORTH (jacks end), -Y = SOUTH (USB-C end).
- BODY_LEN = 160 (grew to 161.6 after even-offset solidify; ~0.8mm/end, not a concern).
- PCB_LEN = 140, PCB_OFF = (160-140)/2 = 10.
- Helpers: byp(p) = p + 10 (PCB-doc-Y -> body-Y);  wY_b(by) = 80 - by (body-Y -> world-Y).
- Height map: world z = H(h) = 8.05 + h  (h = height above PCB back face).
- Final outer dims: 99 (X) x 160/161.6 (Y) x 46 (Z).

## PCB (confirmed from PCB_ref object + user)
- 75mm (X, +-37.5) x 140mm (Y, +-70) x 1.8mm thick. Centered.
- Back face z = 8.05, front face z = 9.85.
- M2 mounting holes: 3.4mm in from each corner on BOTH edges -> hole centers world (+-34.1, +-66.6).

## TOP FACE — CONFIRMED HEIGHTS (locked, ALL_HEIGHTS_OK, tol +-0.35mm)
| Feature     | spec (above PCB) | world z |
|-------------|------------------|---------|
| switch      | 11.15            | 19.21   |
| neotrellis  | 13.00            | 21.05   |
| nt_corner   | 13.00            | 21.05-21.08 |
| oled        | 15.30            | 23.53   |
| mic         | 15.30            | 23.29   |
| lidar       | 16.00            | 23.96-24.05 |
| encoderL    | 16.00            | 24.05   |
| encoderR    | 16.00            | 24.05   |
- Jack clearance 19.33 (>=13 req). PCB interior 40.0 (>=37.5 req).

## TOP FACE — "FAT T" FIX (the encoder/lidar raised band)
- PROBLEM: the encoder row (by62-70, full width) + lidar (by79) formed a T: wide bar north,
  narrow center stem poking south. Caused by lidar/center holding peak height while the sides
  dropped off early.
- FINAL FIX THAT WORKED: set the ENTIRE band by61-79, |x|<27 to ONE dead-flat level = 24.05
  (H(16.0)). Both raise and lower toward that single level (v.co.z = z0*(1-fw*xw)+24.05*(fw*xw)).
  Flat core to x21, rolloff x21-27 into the side curve. Even taper at north end (by54-61) and
  south end (by79-92, descend to NT plateau 21.05). Protect everything OUTSIDE z>18.
- VERIFIED: flatness check across by62-74 all read 24.05 exactly. Max dev 0.17 at south edge.
- KEY LESSON: the encoders sit at by72 — they kept getting dragged down whenever a smoothing
  pass touched north of ~by71. Rule: when flattening the band, leave everything north of by71
  untouched, or protect the flat core (62<by<78, |x|<20) from relax passes.

## HOLLOW / SOLIDIFY (clean recipe — NO artifacts)
1. Start from body_TOPFACE_PERFECT (solid, Subsurf live).
2. Apply Subsurf  -> 760418 verts.
3. Add Solidify: thickness=2.5, offset=-1.0, use_even_offset=True, use_quality_normals=True.
4. Apply Solidify -> 1,520,836 verts. euler 4 (correct for hollow double-walled shell).
5. Set all polygons use_smooth=True.
6. *** DO NOT run remove_doubles, dissolve_degenerate, or blanket edge_face_add fills. ***
   Those caused the artifacts. The bare solidify on the clean mesh is watertight by itself.
- VERIFIED clean: 2.5mm walls under all components (center 2.56, north 2.5, south 2.5, oled 2.5;
  sides read ~3.0 only because the ray crosses angled wall — true perpendicular is 2.5).

## SCREW BOSSES — STATUS: UNRESOLVED. NOT in the current shell.
Target: M2 heat-set inserts, ~5.6mm tall, ~4.5mm OD, top-down (hole along Z). 8mm OD boss.
PCB mounting hole centers (+-34.1, +-66.6); positions FIXED to the PCB. Insert from the top,
"a bit of room" (air gap) between boss bottom and PCB; must NOT punch through to PCB.

### Geometry reality at the corners (measured, reliable via BVH ray_cast)
- Perpendicular wall at all 4 corners = 2.5mm (same as everywhere; the corner is NOT thicker
  perpendicular — earlier "8.3mm" readings were a VERTICAL ray crossing the angled corner wall).
- Inner top wall (cavity ceiling) at corners: NORTH z=15.97, SOUTH z=12.87.
- Outer top at corners: NORTH z=19.1, SOUTH z=16.34.
- Vertical solid depth straight down at the corner before hitting cavity: N 3.13mm, S 3.47mm.
- Space from inner ceiling down to PCB front (9.85): NORTH 6.12mm, SOUTH 3.02mm.
- So a 6mm top-down insert needs ~9mm total (6 seat + 1.5 floor + 1.5 gap): NORTH fits (9.25),
  SOUTH does NOT (only 6.49mm to PCB front). South shell is lower (USB-C end rolls down).

### BOOLEAN LESSON (critical, cost the whole session)
- Adding a free-hanging interior cylinder (boss from inner top wall down into the cavity) to
  the dense 1.5M-vert hollow shell FAILS with solver='EXACT' and solver='FLOAT' — they SILENTLY
  DROP the cylinder body hanging in the cavity (post-union ray_cast shows no boss material, only
  original wall). Joining without boolean also failed (selection/join state issues).
- THE FIX THAT WORKS: solver='MANIFOLD'. Union each cylinder (R=4.0, bottom z=10.85 = 1mm above
  PCB front, top just under outer surface) with MANIFOLD. Result watertight, non-manifold 0,
  max valence 4. Bosses present as real solid columns. (Solver enum is FLOAT/EXACT/MANIFOLD —
  there is NO 'FAST'.)
- Also worked but messy: a FULL-PIERCE cylinder (from z=30 above outer surface down to PCB) with
  EXACT — guarantees volumetric overlap so nothing free-floats — BUT then clipping the poke-out
  with a box DIFFERENCE left 384 non-manifold edges, and a blanket edge_face_add fill made it
  WORSE (326 nm edges, valence 11, "ruined structure"). DO NOT blanket-fill.

### WHY IT'S STILL UNRESOLVED — the open problem
- After a successful MANIFOLD union, the boolean weld seams left 825 NGONS. Triangulating the
  ngons removed them (ngons->0) BUT pushed MAX VALENCE to 28 at the boss seam fan points.
- USER REPORTS (repeatedly, emphatically): the bosses PROTRUDE on every edge AND there are BAD
  ARTIFACTS on the NORTH FLAT FACE. User states the assistant's verification method is BROKEN.
- Assistant's point-sample ray_casts claimed "no protrusion, heights fine, all_buried=true" —
  but these sparse checks (a handful of rays) gave FALSE CONFIDENCE and did NOT match what the
  user sees in their viewport. *** The sparse-raycast verification cannot be trusted to detect
  these artifacts. ***
- Suspected causes (unconfirmed): ngon shading artifacts; valence-28 seam pinches; the bosses
  genuinely breaking the outer surface in ways sparse sampling missed; OR stale viewport display.
- Notably, the user reports BAD ARTIFACTS ON THE NORTH FACE even on the freshly-restored CLEAN
  hollow shell (body_SOLIDIFIED) that has NO bosses (verified ngons 0, valence 4, watertight).
  This suggests EITHER (a) the north-face artifact is independent of the bosses and lives in the
  top-face/solidify mesh itself, OR (b) a viewport-display / stale-render issue on the user's end.
  THIS IS UNDIAGNOSED and must be resolved by RENDERING (not point-sampling) and having the user
  confirm against the render.

### NEXT STEPS for the bosses (when resumed)
1. STOP using sparse ray_cast as proof. Verify by RENDER (top-down + angled) for user confirm,
   OR by checking ALL boss-boundary verts against the local shell surface, not a few rays.
2. First resolve the NORTH-FACE artifact on the clean no-boss shell: render it, have user confirm
   whether the artifact is in the geometry or is a viewport/display refresh issue (Home / NumPad-. ,
   toggle Z->Rendered). Do not proceed to bosses until the clean shell is confirmed clean by user.
3. Re-add 4 bosses with MANIFOLD solver, built LOW (top at the inner ceiling so the full 2.5mm
   wall stays above them — cannot show through the outer face), bottom z=10.85.
4. *** After MANIFOLD union, do NOT triangulate or run cleanup. *** MANIFOLD leaves clean topo.
   If ngons appear and must go, triangulate ONLY the ngon faces with a method that limits valence,
   or leave them (watertight ngons print fine).
5. Insert depth available: NORTH ~5-8mm, SOUTH ~2-5.5mm depending on boss top choice. For the
   5.6mm insert, NORTH is fine; SOUTH is tight — user wants same insert both sides, accepts
   shorter grip at south (insert melts into whatever depth is there; ~3-5mm grips OK for M2).

## RENDER FILES THIS SESSION (on user's Mac, .../Pocket-OpGorator/)
T_removed_clean.png, one_flat_row.png, even_band_locked.png, bosses_over_pcb.png,
bosses_merged_clean.png, topdown_check.png, final_clean_check.png, clean_topface_northcheck.png,
bosses_lower_hidden_check.png, clean_hollow_confirm.png.
Assistant CANNOT view renders; user views them on their Mac.

## WORKING-WITH-EMGOR NOTES
- User cannot see numbers/data; only their Blender viewport + saved PNG renders. Assistant cannot
  see renders. Bridge by rendering to the Mac path and by asking simple concrete questions.
- "Disappeared" = viewport framing / body hidden, never actual deletion. Fix: force-unhide body +
  collections, reframe InspectCam_obj, select body; user presses Home / NumPad-. , toggles
  Z->Rendered to refresh stale viewport.
- Crashes happen; user restores the file and re-states current state. Always re-check euler/hollow
  and which mesh is active after a "brought it back".
- One clean change at a time. Be honest about geometry constraints; when an op fails repeatedly,
  STOP and change method (MANIFOLD solver was the breakthrough).
- Reversion discipline: body_TOPFACE_PERFECT (solid top face), BODYSHAPE (oval base),
  body_SOLIDIFIED (hollow shell) are the trusted known-good restore points.

## REMAINING PHASE-1 / LATER (after bosses + north-face artifact resolved)
- Phase 2 EARS: side speaker swellings + split-jog.
- Phase 3: 12 cutouts at feature centers (body_Y = PCB_doc_Y + 10), lidar a true open hole,
  USB-C flat landing on south cap, diamond side grilles, insert pilot bores; split front/back
  halves; export STLs. User cuts the holes himself with eyes on the viewport.
