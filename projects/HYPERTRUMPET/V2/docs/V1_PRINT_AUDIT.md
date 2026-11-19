# hyperTrumpet v1 print audit

Read-only audit, 2026-11-11. Nothing under the source folders was modified (Blender ran headless with `--factory-startup`, no save; meshes analysed with trimesh in a scratch venv).
Status of evidence: geometry facts are measured [M]. Fit-failure mechanisms are inferences [I] until Emory confirms on the real horn. No written notes about print or fit problems exist anywhere (memory, inbox, hyperTrumpet folder, docs): the "broke off / stretched" part is unrecorded.

## 1. Inventory

The v1 housing lives in `~/Documents/3D_PRINT/trumpy4r/` (not in `_EMGOR_SYNTH`). Searched: ~/Documents (incl. `_EMGOR_SYNTH`, excluding SAMP/node_modules/.git), ~/Desktop, ~/Downloads, `~/Library/Application Support/BambuStudio` (config/cache only, no project files), mdfind "hypertrump"/"trumpet". No .gcode, .step (printable), .f3d, .scad found for the trumpet. No Fusion files.

| Path | Size | Date | What |
|---|---|---|---|
| `3D_PRINT/trumpy4r/GETTINitDONEv3.3mf` | 12.1 MB | 2026-03-11 | Bambu Studio 02.03.00.70 plate, 1 plate, 2 objects (mount_v3 + showHOLE_v3.2). X1 Carbon, 0.4 nozzle, 0.2 mm layer, 2 wall loops, 15% infill, support threshold 38 deg. Thumbnails plate_1.png/top_1.png inside. No slice result stored (no gcode). |
| `.../_mount_v3.stl` | 28.8 MB | 2026-03-11 | The tube-hugging mount (main print part). |
| `.../_showHOLE_v3.2.stl` | 14.1 MB | 2026-03-11 | The electronics compartment ("COMP") with the opening ("showHOLE"). |
| `.../trellis2_lid.stl` | 125 KB | 2026-03-10 | 70 x 70 x 4 mm NeoTrellis lid. |
| `.../hyperFRday2.blend` (+ `.blend1`) | 30.2 MB | 2026-03-11 | Master Blender file: 75 objects, collection `trump` = reference trumpet, plus 15 housing iterations (`___mountATTEMPT1`, `_attempt3MOUNT*`, `attempt3MOUNTmesh2pls` = final mount, `_attemp3COMP*`, `attemp3COMPshowHOLE.001` = final comp, `_holes.001`, `smothingBlock`, `_sexBACKUP`). Scene unit scale 1000, 1 BU = 1 mm after x1000. STL coordinates == blend coordinates. |
| `.../hyperTrumpDemo-Cube.stl`, `Untitled-Cube.stl` | 59 KB | 2026-03-02 | Identical 7.8 x 9.9 x 0.3 mm test slivers. Not parts. |
| `.../Sketchfab_2017_08_22_13_29_36.blend` | 11.8 MB | 2026-03-10 | Third-party Sketchfab trumpet (source of the `trump` collection). Also `~/Downloads/trumpet.zip` (2018). |
| `.../startrek.blend`, `~/Documents/SCHOOL/trump.blend`, `hyperTrumpDemo.blend` | 0.1-0.35 MB | Feb-Mar 2026 | School/demo scenes, unit-scale 1 BU = 1 m, no housing. Ignore. |
| `~/Downloads/trumpet-9.snapshot.1/` (+ `.zip`) | 33 MB | 2016 | Third-party Creo/STEP trumpet (`trumpet_asm.stp`, valve/casing/waterkey parts, Hungarian names). Reference only; not analysed. |
| `~/Downloads/TRUMPET_HYPERINSTRUMENT_REFERENCE_v3.md` | | | Notes, no mounting info. |

Ignore as unrelated: `speaker_hook.3mf`, `XM5_brace_toe0_front.3mf`, other 3D_PRINT files.

## 2. Reference model caveat (important)

The "trumpet" the housing was fitted to is the Sketchfab model, not a measurement of Emory's horn [M: the housing was modelled in the same .blend]. Model checks: bell flare 123.2 mm OD (matches a real Bb bell, 4.8 in) and overall 531 mm length, so scale is roughly right. But the tubing in the model is thin: U-bends and slide tubes have OD about 9.8-10.2 mm (Cylinder.005/.006/.045/.046), versus about 12.5-13 mm on a real Bb trumpet. Valve casing body 19.2 mm OD, caps/bands 20.3-21.2 mm, 24.3 mm at the mouthpiece receiver end. Valve pitch about 22 mm. [M, model]. The task brief's 24-25 mm valve casing OD does not match the model (19-21 mm); Emory must measure his own.

## 3. Per-part analysis

### 3.1 `_mount_v3.stl` (main print part)
- Bounding box 220.7 x 69.4 x 144.8 mm (main body 220.7 x 53.4 x 144.8). Watertight, 575,762 faces, 255 cm3 of material in the main body. [M]
- Plus 7 loose floating lumps in the same STL (28/12-face boxes, 0.8-4.2 cm3 each, at about (741,543,72), (885,546,34) x2, (905,546,113) x2, tiny at (887,534,47) and (922,537,110)). These are leftover magnet/sensor pocket cutters or placeholder boxes. If printed as-is the slicer makes disconnected islands, some floating in air. [M] Worth cleaning in v2.
- Construction: organic, remeshed shell (not clips with circular bores). A circle-fit scan of every 3 mm slice on all three axes found zero circular clamp loops. The grip is a custom-moulded negative of the reference horn, i.e. fit depends entirely on the reference model being the real horn. [M]
- Wall thickness (inward ray cast, 60k samples): median 5.7 mm, P5 2.8 mm, P1 1.2 mm; only 1.5% of surface under 1.5 mm and 1.9% in 1.5-2 mm. Thinnest zones (global coordinates) about (923,537,112), (913,536,109) near the Torus.001 hook at the top valve, and (804,532,15), (770,530,25) near the lower slide tubes. [M]
- Cross-section character: base plate 3.8-5 mm thick at y 536-540 spanning 145 mm; mid region (x 820-880) is a stepped 8-12 mm thick pad. From the base plate, slanted blades reach toward the horn (y 487 to 538, about 50 mm cantilever) at x about 730-805, mean thickness 3.9-4.8 mm, spanning z 23-123 (100 mm tall); further hooks at z 6-22 (x 820, 895) 7-9 mm thick, and a thin rib at z 121-129 about 3.8 mm. [M]
- Snap/cantilever candidates (likely the pieces that broke or needed bending): (a) the 50 mm long x 4-5 mm blade legs at x 730-805, printed with layers 0.2 mm, 2 walls, 15% infill; a 4-5 mm blade with 2 walls has almost no solid core and loads across layer lines if the print orientation puts layers parallel to the bend. (b) the 3.8 mm ribs at z 121-129. (c) the hooks near Torus.001 (thinnest wall, plus 0.65 mm interpenetration, see 4). [I on which one broke; M on dimensions]
- Overhangs: the mount's concave horn-side faces and hook undersides exceed the 38 deg support threshold by design; the print needs support in the grip pockets, which leaves rough surfaces exactly where the horn is touched. [I]
- Mounting interface: no fasteners, no threaded inserts, no strap slots found. Retention is purely the moulded geometry pressing around tubes plus the hooks (and possibly the horn's own flex). Mount occupies x 719-940 (about 9 cm over the valve block and slides), y 487-540 around the tubes, z -13 to 132. [M]

### 3.2 `_showHOLE_v3.2.stl` / final comp
- Bounding box 177.9 x 24.7 x 140.8 mm (non-watertight, 281k faces); the solid comp body `_attemp3COMPMESH2.001` is 177.9 x 12.0 x 140.8 mm, 80 cm3, watertight. Located at y 539-564, i.e. on the outside of the mount's base plate. [M]
- Ray tests: the face-on wall is about 2.5-3.8 mm thick over most of the area (entry/exit 552-556 mm to 556-558 mm); interior depth available for electronics is about 12-20 mm over a 178 x 141 mm footprint. [M]
- Cavities: this is a flat tray for a Teensy 4.1 (61 x 18 mm) plus a 4x4 NeoTrellis (about 61 x 61 mm), BME680/VL53L0X/MPU6500 breakouts, FSR, sliders, pots, joystick, encoders (parts list from `V1_SCRAPE.md`). There is no battery cavity, no battery holder, no charge board pocket, no USB-C cut-out beyond the "showHOLE" opening, no 3.5 mm jack, and no mic port found. V1 was USB-tethered. [M absence, I interpretation]
- `trellis2_lid.stl`: 70 x 70 x 4 mm, 9 bodies (main plate 4.4 cm3 plus 8 small ones about 0.06 cm3 each: key caps or retention pegs). Matches NeoTrellis 4x4 outline plus margin. [M]

## 4. Fit against the reference horn (mount vs `trump` collection, 40k surface samples)

| Horn feature (model) | Dia/size | Min gap mount-to-tube | Points < 0.5 mm | Interpenetration |
|---|---|---|---|---|
| Cylinder.046 slide tube | OD 10.2 | 0.03 mm | 28 | 7 pts, up to 0.26 mm |
| Torus.001 top valve loop | OD 3.6 x 25 | 0.01 mm | 13 | 7 pts, up to 0.65 mm |
| Cylinder.045 tube | OD 10.1 | 0.19 mm | 3 | 0 |
| Cylinder.005 / .006 tubes | OD 13.4/9.8 | 0.00-0.01 mm | 69 / 31 | 0 |
| Cylinder.014 casing region | 17.7 | 0.43 mm | 10 | 0 |
| Cylinder.001 valve casing body | OD 19.2 | 0.62 mm | 0 | 0 |

So the mount has 0.0-0.6 mm nominal clearance to the reference everywhere it grips. Everything depends on the zero-clearance fit being right. No tolerance allowance was designed in. [M]

## 5. Likely fit failures and mechanisms [I, except where marked M]

1. Zero-clearance grip on the wrong tube size (highest likelihood). The model's tubes are about 10 mm OD; a real Bb trumpet's tubing is about 12.5-13 mm OD (leadpipe bore 11.7 mm ID plus wall). The mount's grip pockets were cut at 0.0-0.2 mm from 10 mm tubes [M]. On the real horn each pocket is about 1.2-1.5 mm radially too small, so the mount cannot seat without bending the blade legs and hooks. This is the most probable source of the "had to break off or stretch" event. 0.2 mm clearance would also be too tight in FDM anyway (X1C typical need 0.3-0.5 mm per side for a slip fit, 0.15-0.25 for a snug press fit with elastic material).
2. Brittle cantilevers: 4-5 mm blades 50 mm long, 2 wall loops, 15% infill, layer-line loaded. Forced over larger tubes they crack along layers. Added hooks with 1.2 mm thin walls (Torus.001 area) are the weakest point [M walls].
3. Hooks/pockets that interpenetrate the model (0.26 and 0.65 mm) mean the slicer geometry already collides with the reference tubes before any real-world error [M].
4. Lacquer/plating: brass lacquer is slippery but PLA/PETG against it with a hard rigid hook has poor contact area; with zero compliance the mount rocks on the casing. No pad/TPU interface.
5. Valve casing region: mount comes within 0.6 mm of the 19.2 mm casing. If the real casing OD is 20.5-21 mm (caps/bands 21.2 in the model), interference there too. Measure it.
6. Bell flare: the mount stops at x 940 (bell flare starts beyond x 1000), so bell interference is not an issue for v1 but is a design constraint for the v2 bell-mounted mic.
7. Mass/CG: 255 cm3 of solid plastic at roughly 1.1-1.25 g/cm3 plus 80 cm3 comp: about 350-400 g on the valve side, plus electronics. Valve-side hang can make the horn feel unbalanced and loads the hooks continuously. [I; derived from volumes, material not recorded]
8. Print: 3mf records no support-material or brim settings in the useful keys; mount_v3 has floating bodies; 0.2 layer with 0.4 nozzle on a 28 MB mesh (575k faces) gives needless slicing time but no fit effect.

## 6. Measurements Emory must take on the real trumpet (calipers, mm, 0.1 precision)

Mark every dimension at the exact position where a v2 contact point would be, and note lacquer vs bare.
1. Mouthpiece receiver OD at its widest, and leadpipe OD at 3 positions along the first 150 mm (mouthpiece end, mid, just before the first bend).
2. Tuning slide outer tube OD (main tuning slide, both legs) and the centre-to-centre spacing between the two legs.
3. 1st, 2nd, 3rd valve slide tube ODs (both legs each) and leg spacing.
4. Valve casing OD for all 3 casings (mid-body, not at caps), centre-to-centre pitch (casing 1 to 2, 2 to 3), and casing height.
5. Valve top cap OD and height (cap above casing), and the finger-button stem OD; bottom cap OD and height.
6. Water-key (spit valve) positions: x, z location and protrusion, so the mount avoids it.
7. Bell: throat OD, OD at 100 mm and 200 mm from the bell tip, bell rim OD, bell tip to leadpipe-end length, and the height of the bell rim lip.
8. Bell rim bead thickness (for a clip-on mic arm).
9. Distance between the leadpipe and the main bell tube at the point where the legs of v1 gripped (gap for fingers/hand).
10. Where the left hand holds the horn (3rd-slide ring position and the 1st-valve-slide saddle position) to keep those areas free; and where the right hand's thumb/pinky rest.
11. Valve-axis direction vs tube planes, and whether valves vertical when played: photograph straight-on and side view with a ruler in frame.
12. Bore: confirm the horn's make/model and bore (nominal .459 in = 11.66 mm) to calibrate against standard values.
13. Hall-sensor geometry: valve piston travel (finger button travel in mm, typically 10-13), spacing from casing wall to the cap top where a magnet cap would sit.
14. Weight of the horn (g) and where it balances (finger on the bell tube, mm from the mouthpiece end).
15. The real trumpet's lacquer condition (can it take a rubber pad?), whether a 3-point clamp can touch the bell rim without dents.
16. Stress the v1 print if it survives: caliper the breaks and note which feature broke, and photograph it.

## 7. v2 design requirements (derived from the findings and the stated v2 needs)

Fit and mounting
- Do not design against the Sketchfab model. Use measured values from section 6, plus 0.4 mm per-side printed clearance for rigid parts and a TPU (or foam/silicone strip) compliant liner for anything touching lacquer.
- Tool-free, low-stress mounting: strap/lacing or spring-loaded cradle (3 contact points around the valve casings, not tubes), or a Velcro/silicone-band loop through slots. No rigid cantilever over 20 mm; if a snap is used, 1.5-2 mm thick x 15 mm max length, printed with layers along the bend axis (flat on the bed), wall loops 4+.
- Keep the mount's weight under about 150 g on the horn and balance electronics near the valve block centre, not on one side. Consider battery in the lower mass position.
- Remove the zero-clearance negative moulding, no interpenetration. Fix 4-5 mm blade legs; keep every grip wall at 3 mm minimum, 4 loops.
- Parting lines and hook geometry must not trap the water key or slides; keep slide travel clear.

Electronics
- Bela Gem Stereo enclosure with USB-C, 3.5 mm stereo out, 3.5 mm in or built-in mic path, antenna clearance for the wireless TX (keep the TX at the edge of the shell, away from the brass).
- Built-in mic at the bell: small boom arm clipped to the bell rim with compliance, mic capsule in a foam windscreen cup, cable strain relief. Mic position set by the bell measurements in 6.7-6.8.
- 3 analog Hall sensors reading magnetic valve caps: sensor pockets on a rail 3-6 mm above each valve cap path, distance set by measured piston travel (6.13), adjustable by slot (1-2 mm steps), magnet caps replaceable. Keep Hall boards at least 25 mm from the Bela power stage/inductors.
- 2-3 cylindrical Li-ion cells (18650 or 21700) in a snap-in holder: dedicated bay with contact springs, polarity marked, retention by a latch that does not need tools; vent path and bay away from the player's hand; charge board with USB-C and protection (DW01 style or a power-path board). Battery bay dimensions: 18650 18.6 x 65.2 mm each, 21700 21.7 x 70.4 mm each; 3 cells in a row need about 60 x 70 mm footprint.
- NeoTrellis 4x4 on the face as in v1 (70 x 70 mm lid reusable), plus the v1 sensors from `V1_SCRAPE.md` (sliders, pots, FSR, joystick, encoders, VL53L0X, BME680, MPU, contact mics) with their pockets; v1 had no pocket drawings, so rebuild from the Teensy-era parts list.
- Wall budget: v1 comp depth is only 12-25 mm; Bela Gem Stereo plus battery bay plus Trellis needs at least 35-40 mm internal depth. Plan thickness and weight accordingly.
- Provide print-ready separate parts (shell, lid, battery tray, Hall rail, mic arm), no floating bodies, one STL per part, and keep the horn-contact parts in TPU/PETG rather than PLA.

Process
- Keep the v1 files untouched (precious archive). Start v2 as a new .blend with the measured horn as a simple parametric tube set (cylinders from section 6), not the Sketchfab mesh.
- Print a test coupon set first: 3 grip rings at measured OD +0.2/+0.4/+0.6 mm to dial in the fit before the full print.

## 8. Artifacts used
Scratch analysis (venv + scripts + exports) in `/private/tmp/claude-501/-Users-emgor/278a8985-5772-4bed-a071-371e08de9afc/scratchpad/` (disposable). Source files unchanged.
