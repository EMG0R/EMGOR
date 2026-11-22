# HYPERTRUMPET V2 shell: v1 audit and v2 spec (concept stage)

2026-11-14. Nothing in `~/Documents/3D_PRINT/trumpy4r/` was modified (copies in `cad/v1_reference/`, 9 files, shasum-verified identical). Tags: **[M]** measured by me on the files, **[W]** from web search, **[G]** general knowledge (unverified), **[I]** inference. Builds on `V1_PRINT_AUDIT.md`; its inventory is not repeated. Model frame everywhere: x = horn axis (mouthpiece 620 -> bell rim 1150), y = lateral (pod side is +y), z = valve axis (up). 1 unit = 1 mm.

## 1. v1 joint failure: mount <-> compartment

**What the joint is [M].** The comp (`_showHOLE_v3.2.stl`) is a 12 mm flat tray (y 552-564) that floats 12-14 mm off the mount plate (plate top y ~538-540). The two parts meet at 3 sites only. Each site = a **loose rectangular tongue on the comp** entering an **open-top rectangular cup on the mount**. The prior audit's "7 floating lumps" are these: they are the cups (28-face boxes, inside the mount STL) and tongues (12-face boxes, inside the comp STL), overlapping their parents as separate shells.

| Site | x, z centre | Comp tongue (x * z * length y) | Cross-section | Mount cup, outer | Cup pocket | Wall (sides / z / floor) | Tongue-to-pocket clearance |
|---|---|---|---|---|---|---|---|
| 1 | 742, 71 | 4.4 * 24.9 * 16.5 | 110 mm2 (blade) | 11.4 * 36.1 * 16.5 | 6.1 * 28.1 * ~13 deep | 2.67 / 4.0 / 0.89 | 0.85 per side in x, 1.6 in z |
| 2 | 885, 34 | 4.45 * 4.1 * 20.7 | **18 mm2** | 14.4 * 13.1 (+ an inner second cup 12.6 * 7.4) | 6.7 * 5.8 * ~16 | 2.9 / 2.6 / ~1 | 1.1 x, 0.85 z |
| 3 | 905, 113 | 4.4 * 4.2 * 17.4 | **18 mm2** | same family as site 2 (two nested cups) | 8.7 * 6.9 | ~2.7 | ~2 mm x |

- Tongue embed depth in the comp body: 3.8 mm (site 1), 8 mm (site 2), 5 mm (site 3). Free length from body face to tip about 12.5 mm. [M]
- Cup rim stops 0.75 mm short of the comp back face (rim y 551.25, face y 552). Tongue tips reach y 539.3-539.6, plate top is 538-540.2: tongues bottom out in the pocket. [M]
- Two nested cup shells at sites 2 and 3 are leftover iterations (different sizes, skewed ~0.5 mm). Slicer unions them; the real pocket is the intersection nobody designed. [M]
- **There is no retention.** Straight tongues, 0.85-2 mm slop each side, no detent, no undercut, no dovetail. The only things holding the comp on are friction, glue, and the wires. [M geometry; I that glue was used]
- 3mf: Bambu Studio 2.3, **PLA**, 0.2 layer, 2 walls, 15% grid infill, 5 top / 3 bottom layers, support threshold 30-38 deg, auto brim. [M]

**Print orientation (decoded from the 3mf build transforms) [M].** Comp: `z' = -y`, so the front face (y 564, with the Trellis holes) is on the bed and the **tongues stand straight up from the back face, 12.5 mm tall, perpendicular to the layers**. Mount: `z' = -z`, printed on its top edge (143 mm tall), cups are horizontal fins off a vertical plate (layers parallel to their length, the strong way).

**Why it snapped [I, from the geometry above].**
1. Sites 2 and 3: an 18 mm2 (4.4 * 4.1) peg loaded sideways is bending with tension across the layer interface (layer-adhesion strength, typically 40-60% of in-layer for PLA [G]). Section modulus ~12.5 mm3; with a sharp internal corner at the root (no fillet, stress concentration ~2-3 [G]) and ~12 MPa effective, tip load at failure is roughly **10-15 N (~1 kgf)**. A Trellis press, a fader push or a bump on a 150 g hanging tray is that order. Site 1 is a 110 mm2 blade, ~6x stronger in its weak direction: probably survived.
2. The 0.85-2 mm slop turns steady load into impacts at the tongue root, and the tongue is the only lever (the cup walls are not in contact until the slop is used up).
3. Cup walls (2.7 mm side walls, 13 mm tall cantilever fins) are the second candidate. They are printed in-plane so they are less likely to delaminate; they would crack at the plate junction (sharp corner, 0.89 mm floor wall inside the plate) if the tongue levers them.
4. The 0.75 mm rim gap means the comp rests on nothing but the pegs.
Which block broke (tongue or cup) is **not known**; EMGOR can see it in one glance (question 1).

## 2. v1 "had to break one leg off": reconciliation

**Corrections to the prior audit's #1 cause [M].** The model's tubes are NOT all thin. `Cylinder.005`, which the mount contacts over long strips (348 surface samples within 0.3 mm), is **13.4 mm OD**, matching a real outer slide tube (12.5-13 [G]). The 10 mm tubes (`Cylinder.045/.046/.006`) are the inner slide stockings (real ~11.7-12 [G]; a 10 mm OD tube cannot even contain a .459" = 11.66 mm bore, so those specific parts are undersized in the model). The mount's pockets mostly touch the correctly-sized outer tubes. That explains why the fit felt "very accurate", and why one leg, not all, was wrong. The audit's "everything is 1.2-1.5 mm too tight" claim is retracted as a global statement; it holds only for the inner-stocking contacts.

**Measured interferences (300k mount-surface samples against the model, inside-test per watertight part) [M]:**
- 180 samples are inside the horn model: `Torus.001` 41 samples, depth up to **0.7 mm** (x 903-907, y 493-496, z 120-124); `Cylinder.046` 115 samples up to 0.34 mm (x 888-889, y 512-514, z 25-51); `Cylinder.006` 10 / 0.27; `Cylinder.045` 10 / 0.23 (x 763); `Cylinder.005` 4 / 0.21.
- Everywhere else the clearance is 0.0-0.6 mm. Nothing is designed with tolerance; FDM over-extrusion alone (0.1-0.2) eats it.

**The legs [M dimensions].** Thin plates, not clips: top blades 3.6 mm thick * 82 / 40 mm wide at z 117-124 (x 726-808 and 895-940), reaching to y ~495 (43 mm cantilever); lower blades 2.9-5.2 mm thick * 54-72 mm wide at z 24-47 (x 726-808), reaching y ~498; a right vertical blade 7.7 mm * 84 mm tall at x 888-895 (y reach 505); four 8-9 mm hooks/posts 16 mm tall at the casing-block ends (x 807-822 and 888-896, z 6-22), plus two at z 78-98.

**Most likely leg [I], ranked:**
1. **Top-right blade, tip on the `Torus.001` hook (x 895-940, z 120-124).** Only interference above 0.5 mm (0.7 mm), at the end of the longest/thinnest cantilever (3.6 mm). Forcing a 0.7 mm collision at a 43 mm lever snaps or bends it; also the shard at (922.5, 537.5, 109.5) and the 1.2 mm thin walls there. Support under this horizontally-printed 3.6 mm plate (threshold 30-38 deg) leaves a rough underside and removal can crack it.
2. **Right vertical blade vs. the 1st-slide tube exits (x 888-895).** 7.7 mm thick, sits exactly where `Cylinder.046/.006` leave the casing block (x >= 888): 0.34 mm interference over a 26 mm length.
3. A lower blade between the two legs of the 3rd slide (z 37-47, 5.2 mm thick) if the real leg gap is narrower than the model's (7.4 mm between the 10.1 stockings; ~4.8-5.3 mm with 12.5-13 mm tubes, which is less than the 5.2 mm blade). Plausible but needs his horn's number.
Not proven which; it is a one-photo question (question 1). Side finding [M]: the lower blades sit in the 3rd-slide leg plane (y 486-520, z 24-56), so they may also limit slide travel.

## 3. Standard Bb trumpet vs the v1 reference model

Web search (2026-11-14) confirmed bore, bell and mass only; **casing OD, tube OD and valve pitch could not be sourced online, they are [G] and flagged**. Compare against the Sketchfab model in `trump` (not his horn) [M].

| Dimension | Standard / typical | v1 model [M] | Delta (model - typical) |
|---|---|---|---|
| Bore | .459-.462 in = 11.66-11.73 mm [W] (Bach/Yamaha ML .459, L .462) | n/a (OD 10 < bore) | inner stockings impossible |
| Outer slide tube OD | 12.5-13.0 [G] | 13.4 (`Cylinder.005`) | +0.4 to +0.9 |
| Inner slide / stocking OD | ~11.7-12.0 [G] | 10.1-10.2 | -1.6 to -1.9 |
| Leadpipe / main tubing OD | 12.5-13.0 [G] | ~10 at x=1000 | -2.5 to -3 |
| Valve casing body OD | brief: 23-25; my recollection 20-22 [G]; **unverified** | 19.2 | -1 to -6 |
| Valve cap OD | ~23-26 [G] | 20.3-21.2 | -2 to -5 |
| Valve pitch (centre to centre) | ~22-25 [G] | 22.3 | -0 to -3 |
| Casing height | ~100-110 [G] | 104 | ~0 |
| Height incl. buttons | ~145-160 [G] | 141 | -4 to -19 |
| Bell rim OD | 122-127 [W] (122.2 Bach 43; 125; 127) | 123.2 | within range |
| Bell flare (rim to r=14) | ~80-100 mm [G] | 83 from rim to r=14 (profile s:r = 0:61.6, 10:44.4, 20:32.3, 34:24.6, 50:19.5, 65:16.2, 83:14.0) | plausible |
| Leadpipe-to-bell-axis offset | ~30-40 [G] | 31.2 (y 494 vs 525.2) | ok |
| Overall length | ~480-530 [G] | 530 | ok |
| Mass | 0.97-1.08 kg [W] | n/a | n/a |
| Balance point | near valve block [G, unverified] | n/a | measure |

Net: the model is good where it matters for a bell-mounted spiral (rim, flare, axis offset) and weak on casing/cap OD and the inner tubes. Nothing in v2 should depend on its tube or casing diameters; use his calipers (section 9).

## 4. Prioritized change list

1. **Replace the peg/cup joint with a full-length dovetail rail + flex latch.** Why: the v1 joint carried all load through 18 mm2 pegs oriented to fail. Numbers: one rail running along z (the print-up axis), 100 mm long, neck 14, head 22, height 6, flank 45 deg (printable unsupported at the layer plane since the profile lies in the layers), 0.25 mm sliding clearance per side. Shear area at the neck ~1400 mm2 vs 18. In-plane flex latch at the lower end: PETG tab 1.5 mm thick, 8 wide, 18 long, 1.2 mm detent, strain ~2.5% (limit ~3%) [calc]. No screws. Details in section 5.
2. **Print in PETG, grip surfaces in TPU 95A.** Why: PLA creeps, is brittle, and is hard on lacquer when rigid. 4 wall loops, 0.2 layer, 40% gyroid in the mount, comp walls 1.6 mm. TPU liners 1.2 mm in 1.0 mm recesses with 2 mm retention lips, printed in place or press-fit.
3. **Remove every cantilever > 20 mm and every blade < 5 mm thick; fillet every root R >= 3.** Why: the 43 mm, 3.6 mm blades are the break candidates. Replace with 6 mm ribbed beams or make them TPU.
4. **Add 0.4 mm clearance per side to rigid contact, 0.0 to 0.3 interference only on TPU.** Why: v1 had 0.0-0.6 mm and 180 interpenetrating samples (section 2).
5. **Drop the hook on `Torus.001` and the right blade's tube-end contact; move both 3 mm off.** Why: the two measured interferences (0.7 mm, 0.34 mm).
6. **Clear the 3rd-slide leg plane (y 486-520, z 20-56) and the 1st-slide plane, ring (x 782-820, z 51-83) of all mount geometry.** Why: slide access/travel is a hard requirement and the v1 lower blades sit in that plane.
7. **Round everything** (edge fillets R 6-10 on the pod, R 3 min elsewhere, pod corners R 12); remove all floating bodies; one STL per part. Why: appearance and the 7 loose islands in v1.
8. **Battery heel in the palm rest (cells low, near the valve block).** Why: 140 g of cells is 25% of the load; low and central minimises CG shift. See sections 6 and 8.

## 5. Joint, mount and grip (v2)

Keep v1's placement: pod on the +y side of the casing block, plate at y ~538, hook posts at the casing-block ends and top blades. What changes:
- **Rail:** two options considered. (A) Rail along z on the mount plate (print the mount standing, as v1 was, so the dovetail profile lies in the layer plane; every layer holds the whole profile, so a 14 mm neck cannot delaminate). (B) Rail along x, comp printed face-down: profile stacks across layers, weaker. **Choose A.** Comp slides on from below (bottom of the valve block is free of the hand) and the latch sits at the bottom, 40 mm from the palm rest.
- **Comp-side:** female dovetail in a 4 mm thick back plate; the pod shell and the plate are one part. Print the pod face-down.
- **Latch:** 2 flex tabs, in-plane (flex along x so layers lie along the bend), release by pushing both with a pick/fingernail; spare PETG pins as hard stops.
- **Grip:** four 9 * 16 mm hook posts at the casing-block ends (as v1, z 6-22 and z 80-98), TPU-lined, 0.4 mm clearance rigid; one top blade pair only if the leadpipe/ring photo shows a place for it. Retention against the pod's pull-off torque (pod mass 100+ g at ~40 mm lever) comes from the posts + the dovetail, not from the tubes. **[I; unproven, print a coupon pair first.]**
- **Coupon strategy:** print 3 hook-ring coupons at casing OD +0.2/+0.4/+0.6 mm before the real part.

## 6. Spiral element (bell, mic, strip)

Profile used (model-derived, not his horn): s = distance from rim; r(s) = 61.6, 44.4, 32.3, 24.6, 19.5, 16.2, 14.0 at s = 0, 10, 20, 34, 50, 65, 83; extrapolated 10.5 at 150 and 7.2 at 230 [G]. Bell axis y 525.2, z 97.7.

| Item | Value |
|---|---|
| Axial span | s = 20 -> 140 mm (starts 20 mm behind the rim bead, ends 17 mm before the brace plane at x=993) |
| Pitch | 18.7 mm axial per turn |
| Turns | 6.4 |
| Ribbon axial width | 12 mm (gap between turns 6.7 mm) |
| Ribbon height off the bell | 3.0 TPU standoff + 8.5 ribbon = 11.5 mm; centerline of the LED channel at r + 7.2 |
| Helix radius (channel centerline) | 39.5 mm at s=20 down to ~17.7 at s=150 |
| LED path length | **~970 mm** (computed, trapezoid integral along the surface at pitch 19.9, s 20-150; at pitch 18.7, s 20-140 it is 1010). Use the 1 m strip with ~30 mm for the lead; 157 px at 160 LED/m |
| Strip | BTF FCOB 5 mm wide, 5 V; channel inner 7.0 wide * 6.0 deep; diffuser 0.8 mm opal PETG/white-light-guide film, 3 mm above LEDs |
| Min strip bend | radius 17.7 mm in the plane of the flat face (benign, FCOB handles ~10 mm [G]) |
| Wall | 1.2 mm (3 loops); diffuser separate piece, snaps over the channel lips |
| Mass | **~45-55 g** [I from section areas ~40 mm2 * 0.9 m * 1.27 g/cm3, + 6 g TPU] |

**Attachment (concept, not proven).** The closed helix cannot pass over the 123 mm rim while sitting on the narrow end, so it is **open on the leadpipe-facing side (centre 180 deg, 100 deg gap per turn)**. Per turn: a PETG arc covering ~260 deg, then a TPU 95A bridge across the gap (continuous ribbon, so the strip is continuous). The ring springs on sideways like a bangle: the TPU bridges stretch ~20-30% to let the PETG arcs pass the 65 mm bell width at s=20. Contact only on 12 TPU standoff pads (8 * 8 * 3 mm, 85A, 6 turns * 2), 3 mm thick, no rigid-to-lacquer contact. Retention: pad interference 0.4-0.6 mm + the conical wedge on the bell; add a Velcro/silicone band through two slots at the throat end if needed. No adhesive, no screws. **Fallback if the TPU bridge does not hold:** a split clamshell with two 480 mm strips (two channels, no continuity across the split).
- **Leadpipe clearance:** the leadpipe axis is 31.2 mm from the bell axis; leadpipe surface at ~25 mm. At s >= 100 the ribbon outer radius is r + 11.5 ~ 24 mm, so only **~1-3 mm clear** at the throat end (model-derived). The leadpipe presence at those x is uncertain. Spiral ends at s=140 to stay clear; a measurement is needed (question 4).
- **Brace:** rods at x 991-995, y ~494.5, z 37-100, radial distance >= 31 mm from the bell axis; ribbon outer radius at s=140 ~ 22.5 mm: **~8 mm clear**, and the spiral stops 17 mm short axially.
- **Hands:** left hand is at the valve block (x 820-890), right hand at the buttons and leadpipe; the spiral lives at x 1010-1130, 120+ mm from either (see palm-rest assumption).
- **Mic mount [I].** First turn (s=20) carries a 4 mm post/arm (35 mm) with a socket for the clip-on mic body; mic capsule ~12 mm behind the rim plane, 15-20 deg inward, ~30 mm off the wall at the top (+z). Real socket size needs the mic model (question 7). Rim bead is not touched.
- **Acoustic risk [I]:** +50 g of ribbon on the bell flare damps bell vibration a little; keep pads off the rim and keep the ribbon off the rim bead.

## 7. Palm rest (left hand, underside)

Concept (assumes the left hand cups the valve block from below and the pod's weight offsets to +y):
- **Pad:** x 800-900 (100 mm long, centred on casing 2 at x 853), y 508-580 (72 mm, running from under the casings out under the pod), z -40 -> -6 (28 mm thick, hangs below the bottom caps at z -4).
- Hand dims it is sized for: adult palm width 80-95 mm, palm length 95-110 mm [G]. Bottom surface convex, longitudinal radius 70 mm, transverse radius 45 mm, 11 mm corner radius, 2 mm TPU 85A skin, 4 mm finger-clearance reliefs at both x-ends so the 1st/3rd slide rings and bows stay reachable.
- **Contents:** 2 * 21700 side by side in the pad (cells 21.7 * 70.4 each; cavity 46 * 75 * 24, cells axis along x, keyed polarity, spring-holder pair), BMS + TPS61088 + Twidec toggle at the end nearest the pod. Cell centre of mass about 20-24 mm below the casing bottoms, x within +-10 of casing 2.
- Where the hand really holds the horn (palm, thumb, ring-finger positions) is **not known** (question 3).

## 8. Electronics placement and weight budget

Masses are catalog/typical [G]/[W], nothing weighed.

| Item | Where | g (est.) |
|---|---|---|
| 2 * 21700 5000 mAh | palm-rest heel, low | 138 |
| 2 single-cell holders | palm-rest heel | 12 |
| BMS + TPS61088 + Twidec + wiring | palm-rest end | 15 |
| Daisy Seed 3 + headers | pod, behind Trellis | 8 |
| NeoTrellis 4x4 (61 * 61 mm) | pod front face | 25 |
| 3 * 45 mm faders | pod (long axis along z) | 18 |
| 2 pots + 2 KY-040 + knobs | pod | 22 |
| KY-023 joystick | pod front, thumb side | 5 |
| FSR402, MPU6050, VL53L0X (window), 3.5 mm jack | pod | 12 |
| 3 * 49E Halls + rail + 3 magnets/caps | pod bottom / valve base (open question: which cap carries the magnet) | 15 |
| 2 de-cased Xvive A58 TX (antenna >= 1 cm off brass, at the pod outer edge) | pod top edge | 24 |
| 1 m FCOB strip + level shifter | spiral | 20 |
| Dynamic clip-on mic + cable | spiral first turn | 40 |
| Misc wiring/connectors | | 20 |
| **Electronics subtotal** | | **~374** |
| Pod shell + mount plate + rail + posts + latches (PETG + TPU) | | 100-120 |
| Spiral | | 45-55 |
| Palm rest shell + pad | | 30-40 |
| **Total pod mass** | | **~550-590 g** |

**The ~250 g target is not reachable** with 2 * 21700 and 2 TX on the horn: cells alone are 138 g. Horn + pod ~ 1.55-1.65 kg, +55%. Mass reduction options with numbers: 1 cell halves runtime (3.6 h new) and saves 69 g (-> ~490 g); move both TX + cells to a belt pack saves ~200 g (-> ~370 g) but breaks the self-contained design; a 18650 pair saves ~50 g but loses capacity; pod shell shrunk from the v1 footprint (178 * 141) to ~150 * 115 saves ~25 g.
Pod outline (concept): 150 * 115 outline + 28 mm depth (Trellis 61 * 61 + faders + joystick + Seed), rounded corners R 12. The v1 depth of 12-25 mm cannot hold the joystick module (~25 mm) or encoders; budget 28-30 mm.

## 9. Keep-out map (model frame, mm; hard no-go for any v2 part) [M for v1 positions, I for travel extents]

| Zone | x | y | z | Why |
|---|---|---|---|---|
| 3rd-slide legs and travel | 660-821 | 486-520 | 20-56 | tubes and crown; pull ~60-90 mm (-x) [G] |
| 3rd-slide finger ring | 782-820 | 494-502 | 51-83 | ring |
| 1st-slide legs and travel | 888-1080 | 486-520 | 20-56 | tubes and crown, pull to +x |
| 1st-slide thumb hook (`Torus.001`) | 903-918 | 493-496 | 104-129 | hook |
| Valve button path | 815-895 | 500-520 | 109-165 | right-hand fingers; button travel ~10-13 [G] |
| Water keys | unmeasured | | | position needs his horn |
| Main tuning slide (front) | assumed ~1000-1065 at the leadpipe plane (y ~494, z 87-102) | | | tubes at those x are present in the model; **position uncertain** |
| Bell-to-leadpipe brace | 991-995 | 493-496 | 37-100 | rods |
| Bell rim and bead | 1140-1150 | rim | | no contact |
| Mic path | in front of the bell, +z | | | free |

Allowed zones: y >= 538 beside the casing block for the pod; the palm rest below z -6; the bell flare for the spiral. Render-concept volumes (red) are in `cad/v2_concept.blend`, collection `KEEPOUT_notes`.

## 10. Open questions for EMGOR (minimal, ranked; measurements from his horn)

1. **Which v1 leg did you break off** (top or bottom, left or right, which one next to what)? One photo of the break. Decides section 2.
2. **Casing body OD (mid-body), top-cap OD and casing centre-to-centre pitch** (one caliper pass, 3 numbers). Anchors the grip and every hook radius.
3. **Photo of your left-hand grip** on the horn from the player's side and from above, with the palm contact marked, plus the wrist angle. Anchors the palm rest.
4. **Bell: surface-to-surface gap between the leadpipe and the bell tube** at the brace position, and the bell tube OD at 100 and 200 mm back from the rim. Anchors the spiral end and clearance.
5. **One outer slide-tube OD and the gap between the two legs of the 3rd slide.** Settles the v1 blade-between-legs candidate and the grip clearance.
6. Horn mass and balance point (finger under the tube; distance from the mouthpiece end).
7. Mic model (or its clip body dims), for the spiral socket. Not from the horn but needed.
Not needed to start: bore, valve travel (assume 10-13 mm and tune the Hall rail by slot).

## 11. Files

- `cad/v1_reference/`: copies of the 9 v1 source files, shasum-verified.
- `cad/v2_concept.blend`: parametric standard horn (cylinders, revolved bell), pod, dovetail rail, spiral ribbon + strip, mic, palm rest, keep-out volumes. Concept only, not CAD. The bell tube is not joined to the rest of the horn (schematic).
- `cad/v2_concept_renders/`: `v2_iso_pod_side.png`, `v2_top.png`, `v2_side.png`, `v2_bell_closeup.png`.
