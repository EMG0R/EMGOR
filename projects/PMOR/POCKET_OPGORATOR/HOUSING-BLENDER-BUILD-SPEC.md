# Pocket OpGorator — Housing Build Spec (for an LLM driving Blender)

> **Purpose.** This is the single authoritative spec for an LLM to model the **Pocket OpGorator** enclosure in **Blender** (via the `mcp__blender__*` tools) and produce two print-ready half-shells. It captures every locked dimension and decision. Where the conversation changed its mind, **only the latest decision is recorded here** (older conflicting numbers are gone).
>
> **House style of this doc:** hard facts are stated plainly. Anything I'm proposing about *how to build it* (not *what to build*) is tagged **`SUGGESTION:`** — those are advisory; the geometry/numbers are not.

---

## 0. How to use this doc

1. Read §1 (Conventions) and §2 (Pipeline) first — they govern everything.
2. Build in **three ordered phases**: **Phase 1 = BODY**, **Phase 2 = EARS**, **Phase 3 = HOLES**. Do not start holes until the body+ears solid is approved.
3. Every number is in **millimeters**. Coordinates are given in two frames (PCB and shell) — see §1.3.
4. Back up **before every change** (§2.2). Self-screenshot at every stage and before every boolean.
5. Treat the internal-stack Z numbers as **rough ratios** to proportion into the real **46mm** depth (§4.5) — the builder confirms exact packing physically.

---

## 1. Conventions (read first)

### 1.1 Units & tools
- **Units = millimeters.** Blender scene set to metric, **1 Blender unit = 1 mm**.
- **Tools:** `mcp__blender__execute_blender_code` for all geometry (Python / bmesh); `mcp__blender__get_viewport_screenshot` to self-verify; `get_scene_info` / `get_object_info` to confirm state.

### 1.2 Axes & faces
- **X = width** (left ↔ right). **Y = length** (top ↕ bottom, portrait). **Z = depth** (front ↔ back).
- **+Y = up the length toward the TOP/NORTH end** (audio-jack wall). **Y=141 = BOTTOM/SOUTH end** (USB-C wall).
- **+Z = FRONT = the "TOP FACE"** — the user-facing control face (OLED, pads, encoders). **−Z = BACK/rear.**
- **"North face"** = the top end wall (Y=0). **"South face"** = the bottom end wall (Y=141).
- **SUGGESTION:** put the Blender world **origin at the body's geometric center**; +Z = front/control face, +Y = up the length.

### 1.3 Coordinate frames (two of them)
- **PCB frame:** the true main-PCB outline is **75 (X) × 140 (Y) mm**. Origin = PCB **top-left corner**; X runs 0→75 to the right, Y runs 0→140 downward. All hole positions are given to the **hole center** in this frame.
- **Shell frame:** the PCB is **centered in the shell** in X and Y, so **shell = PCB + (2.5 in X, 0.5 in Y)**. Every cutout below lists both.
- **Heights (Z):** measured **from the PCB BACK face**, and **always account for the 1.8 mm PCB thickness** (the "top/bottom face of the PCB" are two real surfaces 1.8 mm apart).

---

## 2. The pipeline

### 2.1 Three ordered phases
1. **PHASE 1 — BODY:** the complete solid pebble shell (no ears, no holes).
2. **PHASE 2 — EARS:** add the two side speaker-redirect "ear" swellings + the split-jog.
3. **PHASE 3 — HOLES:** model every cutout as its own object, confirm positions, **boolean-subtract last**, then split into halves.

### 2.2 Backup method = **duplicate-and-hide** (mandatory)
- **Before every change, duplicate the working object and hide the copy.** The hidden duplicate is the backup. Name them in sequence: `body_v01`, `body_v02`, … `ears_v01`, … `final_v01`, …
- Any step is recoverable by un-hiding. **SUGGESTION:** keep all hidden backups in one dedicated, hidden collection (e.g. `BACKUPS`) so the viewport stays clean.
- Each hole/cutter is its own **named object** (with its own duplicate-and-hide backup); **SUGGESTION:** keep live cutters in a `CUTTERS` collection.

### 2.3 Confirm-before-boolean
- **Confirm every hole's location (the builder verifies with digital calipers) BEFORE subtracting it.** Self-screenshot before each boolean. Subtract cutters **last**, after the body+ears solid is approved.

---

## 3. Components that constrain the housing (size reference)

| Item | Size (mm) | Notes |
|---|---|---|
| **Main PCB** | 75 (X) × 140 (Y) × **1.8 thick** | centered in shell (+2.5 X / +0.5 Y) |
| **Battery (LiPo)** | **110 (Y) × 61 (X) × 12 (Z)** | in rear cavity, behind the PCB |
| **Component gap (PCB→battery)** | **~22 (Z)** | Daisy Seed etc. live here |
| **Daisy Seed** | 60.96 × 25.4 × ~12 tall | placement **free** (USB is a wired breakout — §8) |
| **Speakers (×2)** | 20 × 40 face, **5.5 thick** | uxcell 8Ω 2W; side-firing into the ears (§6) |
| **NeoTrellis PCB** | ~60 × 60 | daughterboard over the lower front; ~X7.5–67.5, Y75–135 |
| **Foam pads** | ~2 each | one Daisy↔battery (thermal barrier), one battery↔back shell |

---

## 4. PHASE 1 — BODY (the solid pebble)

### 4.1 Overall outer size
- **~100 (X, max width at the ¼ & ¾ bulges) × 141 (Y, length) × 46 (Z, max depth).**

### 4.2 Width (X) along the length — the key "hourglass / peanut" curve
Smooth-curving (NOT linear) through these stations:

| Y (from top) | 0 | ~35 (¼) | ~70.5 (middle) | ~106 (¾) | 141 |
|---|---|---|---|---|---|
| **Width (X)** | **77** | **100** | **77** | **100** | **77** |

- Two **bulges at the quarter-points** (Y≈35, Y≈106), **pinches** at the top, the middle (Y≈70.5), and the bottom. Equal spacing. **This is THE signature body curve.**

### 4.3 Depth (Z) along the length
- **Max ~46 mm.** Curves **more dramatically toward the bottom**, easing thinner toward the top/bottom ends.
- **SUGGESTION:** keep depth fairly full through the battery span and let it taper at the very ends; the width hourglass is the dominant visual curve, depth is the supporting one.

### 4.4 Cross-section (X–Z at any Y)
- **Flat front face + a slightly narrower flat back face**, with the **side edges rounding as close to a full semicircle as possible while still clearing the internal components** (most dramatic curves).
- The **flat front stays ≥ ~62 mm wide** so it can host the controls (easy at the 100 mm bulges; at the 77 mm pinches the 75 mm PCB has ~1 mm side margin and the flat front still clears the centered 55 mm pad / 62.5 mm OLED windows).

### 4.5 PCB placement in Z + internal stack
- PCB is **centered in X and Y**, sits **toward the FRONT** in Z.
- **Front (top) face stands 11–16 mm above the PCB back face** (varies by region — see the Height Map §4.6).
- **PCB back face sits ~30 mm in from the back/bottom outer face** → **~26–27 mm rear cavity**, all within the 46 mm depth.
- Behind the PCB: **~22 mm component gap** (Daisy etc.) → **battery 110×61×12** → thin foam pads.
- **Treat these Z figures as rough ratios proportioned into the real 46 mm depth.** The build **hugs the PCB** — nothing extends far beyond it. (The literal stack is a few mm over 46; packing is tight, confirmed physically.)

### 4.6 Front-face height map (the undulating top face)
The front/top face is **not flat** — it is a **smooth surface that rises and falls to meet each hole's height**, interpolating continuously between them. Each value is **where the hole / outer top face sits, above the PCB back face** (components stick out further; model to the hole / top-face):

| Region | Top-face height above PCB back |
|---|---|
| Power switch (top center) | **11.15** (≈11, local dip) |
| NeoTrellis pads (lower) | **13** |
| OLED (upper-mid) | **15.3** |
| Mic (top-left) | **15.3** |
| Lidar + encoders (mid row) | **16** (high point) |

- The surface **humps to a peak at the lidar/encoder row (16)**, eases down to OLED/mic (15.3) and NeoTrellis (13), and **dips at the switch (11.15)** — the switch dip ramps back to its surroundings over **~6.5 mm each side**.
- **TOP↔NORTH edge fillet is capped at ~2 mm radius.** ⚠️ **ALWAYS REMEMBER — the mic hole is jammed into the top-left corner:** its **near edge is only ~3 mm from the PCB top edge** and **~1 mm from the edge of the mic's own sub-PCB.** There is **no room for any meaningful edge roll, wall thickening, or housing encroachment in that corner** — keep the top↔north edge tight (~2 mm max) and don't let any shell feature creep over the mic.
- **SUGGESTION:** the height map is a 2-D field (varies in X *and* Y at the very top, e.g. mic 15.3 vs switch 11.15 at the same Y). Build the bulk body first, then shape the front surface to the field via a lofted/sculpted top-surface boolean or proportional-edit pulls to each feature height. This is the fiddliest part — do it before shelling.
- **HARD CLEARANCE RULE — audio jack bodies (TIGHT):** the PJ-307 jack **bodies stand 10.1 mm above the PCB back face**, so the front **inner** ceiling (= front-face height − 2.5 mm wall) must clear 10.1 mm everywhere a jack sits. → **Keep the front face ≥ 13 mm (outer) across the whole top-edge jack zone (X ≈ 10–65, Y ≈ 0–15)**, giving ~10.5 mm inner. The **switch dip (11.15) must stay LOCALIZED to X ≈ 28–48** so it never intrudes over the jacks at X15 / X60. (At ≥13 mm the right jack clears by only ~0.4–2.5 mm — it's tight; verify with a cross-section screenshot. Don't let the top-edge surface sag below 13 mm except right at the switch.)

### 4.7 Wall & split
- **Wall = 2.5 mm PETG**, applied by shelling/solidifying **after** the solid form exists; **local thickening ~3 mm** at the parting seam and at the insert cylinders.
- **Shell split:** one solid, then cut into a **front half + back half ("almost halves") at the mid-girth parting line**; each half exported separately. (The split **jogs below the ears** — see §6.)

### 4.8 SUGGESTED build order for Phase 1
> **SUGGESTION (architecture):**
> 1. Define cross-section **profile curves at Y stations** (e.g. Y = 0, 17.5, 35, 52, 70.5, 88, 106, 123.5, 141). Each profile = a rounded-rectangle / superellipse whose **X width** comes from §4.2 and **Z depth** from §4.3, with flat front/back and near-semicircle side edges (§4.4).
> 2. **Skin / bridge** the profiles into one watertight solid; smooth-shade and check the curves with a screenshot.
> 3. Shape the **front-face height field** (§4.6).
> 4. **Solidify inward 2.5 mm** (thicken seam/cylinder zones to ~3 mm).
> 5. Duplicate-and-hide → `body_v0x` at each step.

---

## 5. (PHASE 1 cont.) Fastening structure baked into the body

> Build these as part of / right after the body, because they're structural and must be exact.

### 5.1 Screw system
- **4× M2-0.4 × 30 mm socket-head cap screws**, 304 stainless. (BOM link §10.)
- Screws **enter from the BACK shell**, run up the empty **corner channels** (clear of the battery), pass through the PCB's M2 mounting holes, and thread into **M2 brass heat-set inserts** held in **solid front-shell cylinders**.
- **Back screw holes are COUNTERBORED** so the socket-cap heads sit **flush** (no bumps on the back).
- **Length check:** ~0.5 (back wall remainder) + ~24 (corner channel) + 1.8 (PCB) + ~3.5–4 (insert engagement) ≈ 30; tip buries ~10 mm short of the front face (never shows).

### 5.2 Insert cylinders (front shell) — critical
- **4× SOLID PETG cylinders that descend from the top/front face.** **NO pre-drilled bore or cavity** — the builder **melts the M2 heat-set insert straight into the solid cylinder with a soldering iron** (the melt forms its own pocket; the flat end-face self-centers, so no pilot is needed).
- Each cylinder is **straight along the Z (screw) axis**, centered **precisely over a PCB mounting hole**, so the back screw passes straight up into it. **Keep the cylinders SHORT — no more than ~6 mm long**, just enough to hold the heat-set insert (melted into the bottom / PCB-facing end). Position so the **insert sits ~2–5 mm above the PCB front face**, where the M2×30 screw tip lands (~3–4 mm engagement). *If a corner's front-face is tall enough that a 6 mm cylinder leaves the insert too high for the screw to bite, step that screw up to **M2×35** — do NOT lengthen the cylinder past ~6 mm.*
- **CRITICAL — driven by PCB hole coordinates, NOT the curved skin:** cylinder XY = the PCB mounting-hole XY only (+2.5 X / +0.5 Y to shell). The body's curvature must **NOT** shift or skew them; the 4 cylinders' spacing stays **exact** regardless of how the outer surface curves. **Extra filament fills the gap** between the curved inner wall and each cylinder; **gusset** each cylinder to the front inner wall so it isn't floating.
- **Cylinder Ø ~5–6 mm.** **Bottom two = Ø5** (clearance, see §5.4).

### 5.3 Screw / cylinder positions (LOCKED)
| Hole | PCB (X,Y) | Shell (X,Y) |
|---|---|---|
| Top-left | (4, 4) | (6.5, 4.5) |
| Top-right | (71, 4) | (73.5, 4.5) |
| Bottom-left | (4, 136) | (6.5, 136.5) |
| Bottom-right | (71, 136) | (73.5, 136.5) |

*(Assumed M2 mounting-hole positions on the 75×140 PCB — verify against the physical board, but locked for the housing. Back-face screw holes + counterbores and the front-shell cylinders all share these X,Y.)*

### 5.4 NeoTrellis clearance (TIGHT but doable — LOCKED)
- The NeoTrellis PCB (~60×60, centered on the pad grid → ~X7.5–67.5, Y75–135) sits just inboard of the bottom holes at X4 / X71.
- The **bottom cylinders clear it by only ~1 mm in X** → make the **two bottom cylinders Ø5 mm** so they tuck just outboard of the NeoTrellis corners. **Top cylinders (Y4) have ample room.** Verify the ~1 mm physically.

---

## 6. PHASE 2 — EARS (side-firing speakers + redirect curves)

**2× mirrored, on the upper LEFT and RIGHT sides.**

### 6.1 Speakers
- **20×40 mm face, 5.5 mm thick.** Orientation: **40 mm runs up-and-down (Y), 20 mm front-to-back (Z), 5.5 mm thickness across (X).**
- Fires **straight out the side (±X), 90° "like a box."** The housing is designed for **90° firing**; any slight internal cant the builder adds is a mounting detail, **out of scope for the shell.**
- **"Cube point" placement:** the speaker's **top-outer corner sits exactly on the PCB's top-side corner** → each speaker **spans Y0→40 (center ~Y20)**, outer face flush to the PCB side edge, in the rear-ish side region (behind the PCB front plane, beside the battery).
- **The speaker is NOT housed inside the ear.** The ear is purely an external redirect feature.

### 6.2 Ears (the redirect curves)
- A smooth **flowing curve** swelling out of the upper side, **~6–7 mm proud** of the body. **A curve, not a symmetric scoop** — it sweeps, it doesn't bowl.
- Positioned **a fair bit BELOW the speaker's center** (curve mass **~Y28–48**), sweeping **upward** toward the speaker to nudge the side-firing sound **a bit toward the front/user**.
  - *Honest acoustic note:* a passive external curve gives a **modest** forward bias, not a true 90° redirect.
- A **fillet-blended swelling of the shell wall** (≥2.5 mm everywhere, no thin lips, self-supporting print angles) — **a swelling, never a snap-off appendage.**
- The ear lands on the **FRONT shell**; the **mid-girth split JOGS down below each ear** so the ear prints as one seamless piece.

### 6.3 Side grille (cut in Phase 3, pattern locked here)
- Opening zone **~20 (Z) × 40 (Y) mm** on the side wall, over the speaker face.
- **DIAMOND lattice** — **maximize open area** for output **while remaining fully structurally sound** (PETG ribs ~1.2–1.5 mm between diamonds).

### 6.4 SUGGESTED build order for Phase 2
> **SUGGESTION:** add each ear as a separate **swelling solid** (a lofted/blobby bulge or sculpted mass) unioned to the body and **fillet-blended** so there's no seam line; place a thin **internal speaker pocket** (20×40×5.5, top-corner at the PCB corner); then model the **split-jog** as a curve that dips below the ear footprint before booleaning the front/back halves. Duplicate-and-hide → `ears_v0x`.

---

## 7. PHASE 3 — HOLES (cutouts)

### 7.1 Method
- Each cutout = its **own named cutter object** (duplicate-and-hide backup), in `CUTTERS`.
- **Confirm each position (calipers) before subtracting.** Self-screenshot before each boolean. **Subtract last.** Then split front/back and export each half.
- Front-face cutters pierce the (undulating) front wall; end-wall cutters pierce the north/south caps; side cutters pierce the rounded side at the ears.

### 7.2 Full cutout table (LOCKED)
All positions to **hole center**; **PCB → shell = +2.5 X / +0.5 Y**.

| # | Cutout | Face | Shape & size | Center PCB (X,Y) | Center shell (X,Y) | Key notes |
|---|---|---|---|---|---|---|
| 1 | **OLED window** | TOP | rect **62.5 (X) × 41 (Y)** | (37.5, 40.25) | (40.0, 40.75) | spans PCB X 6.25→68.75, Y 19.75→60.75; top-face 15.3 |
| 2 | **NeoTrellis grid** | TOP | **16× squares 10×10**, sharp corners, 5 gaps, 15 pitch, 55×55 grid | centers X{15,30,45,60} × Y{82.5,97.5,112.5,127.5} | X{17.5,32.5,47.5,62.5} × Y{83,98,113,128} | centered X (10 margins), bottom 7.5 off PCB bottom; top-face 13 |
| 3 | **Audio INPUT jack** | NORTH wall | round **Ø6.9** | X 15.05 | X 17.55 | **TOP-side mounted** (barrel sits above the PCB); **15.05 from the LEFT edge**; barrel center **7.3 above PCB back face** (~81% up the depth). **Body top 10.1 above back face → §4.6 hard clearance rule applies.** |
| 4 | **Audio OUTPUT jack** | NORTH wall | round **Ø6.9** | X 59.95 | X 62.45 | **15.05 from the RIGHT edge** (75−59.95) → **mirror-symmetric to #3** about the centerline (PCB 37.5 / shell 40); same Ø6.9, same **7.3 above PCB back**. The two jacks ARE symmetric — differing raw X is expected. |
| 5 | **Encoder L shaft** | TOP | round **Ø6** | (18.73, 62.17) | (21.23, 62.67) | through-hole |
| 6 | **Encoder R shaft** | TOP | round **Ø6** | (57.9, 62.17) | (60.4, 62.67) | **NOT** a mirror of L |
| 7 | **Lidar / ToF** | TOP | **stadium 10 (X) × 6.5 (Y)**, rounded ends, **OPEN hole** | (37.05, 69.43) | (39.55, 69.93) | must be open (ToF can't see through PETG); top-face 16 |
| 8 | **Mic** | TOP | round **Ø10** | (11, 8.2) | (13.5, 8.7) | top-face 15.3. **Jammed in the top-left corner: ~3 mm from the PCB top edge, ~1 mm from the mic sub-PCB edge** → sets the ~2 mm top-edge fillet cap; no shell encroachment here (see §4.6). |
| 9 | **Power switch** | TOP | **rect 7 (X) × 4.65 (Y)** | (38, 5.125) | (40.5, 5.625) | plain rectangle; sits in the 11.15 dip |
| 10 | **USB-C** | SOUTH wall | **stadium 10 (X) × 3.3 (vert)**, rounded ends | X 36 | X 38.5 | **BOTTOM/BACK side of the PCB** (it wires to the Daisy under the board) → it sits **LOWER than the top-side jacks, NOT coplanar.** **Opening bottom edge = 11.5 below the PCB TOP face → center ≈ 9.85 below the top face (= 8.05 below the PCB back face, using the 1.8 mm thickness)** → ~mid-depth (~47% up), clear of the front corner. 1 mm outboard of the PCB connector; **flat landing pad** on the curved south cap. |

> **Port planes — do NOT make these coplanar:** the two audio jacks are **top-side** mounted (high, ~81% up the depth, 7.3 above the PCB *back* face); the USB-C is **bottom/back-side** (low, ~47%, ~8.05 *below* the PCB back face). They are on opposite faces of the PCB **by design** — different heights is correct.
| 11 | **Side speaker grilles ×2** | side walls | **~20 (Z) × 40 (Y)**, DIAMOND lattice | over each speaker (Y0–40, far sides) | — | max open area, ~1.2–1.5 mm ribs |
| 12 | **Back screw holes ×4** | BACK | counterbored for M2 socket cap | = §5.3 positions | = §5.3 | flush heads |

### 7.3 SUGGESTED order for Phase 3
> **SUGGESTION:** cut in this order so verification is easy: (a) the 4 back screw holes + counterbores, (b) the top-face round holes (mic, encoders, lidar), (c) the top-face rectangles (OLED, switch, pad grid), (d) the end-wall stadiums (jacks, USB-C), (e) the diamond side grilles last (most rib geometry). Screenshot + confirm before each. Then split halves and export.

---

## 8. USB architecture (affects internal routing, not the shell holes)

- The external USB-C port is a **USB-C female breakout board (16-pin)** mounted at the **SOUTH-wall opening (#10)**, wired by short **flying leads** to a **micro-USB Type-B male connector** plugged into the **Daisy Seed's micro-USB port**.
- Because it's a **flexible wired hop (not a rigid panel-to-board tie)**, the **Daisy's location/orientation are free** — place it for best fit in the rear cavity.

---

## 9. Thermal note

- The STM32H750 runs warm but is rated for it; for intermittent musical use it's **fine without vents**.
- **Decision: no vents** (keep the clean sealed pebble). **Keep the Daisy heat sinks.** **Keep the foam pad between the Daisy and the battery** — that's the real protection (isolates the LiPo from chip heat).
- *Deferred option:* if long heavy-DSP sessions cook it, add small **side** vent slots near the Daisy (a trivial Phase-3 cutter) — back vents would sit over the battery and aren't useful here.

---

## 10. Housing BOM links

| Part | Link |
|---|---|
| M2-0.4 × 30 mm socket-head cap screws (304 SS, 30 pc) | a.co/d/00Es7j9G |
| M2 brass heat-set inserts | standard stock (melted into solid cylinders, no pilot) |
| USB-C female breakout board (Type-C, 16-pin) | a.co/d/0dv4nUcr |
| Micro-USB Type-B male solder connector (5-pin, w/ cover) | a.co/d/09tX9iMK |
| Speakers — uxcell 20×40 mm 8Ω 2W (5.5 mm thick) | a.co/d/07LGL4J69 |

---

## 11. Refine-at-build (non-blocking) items
- Exact **ear / speaker Y** (vibe-locked: speaker Y0–40, ear curve ~Y28–48) — refine visually.
- **Diamond-grille rib/hole dims** (default ~1.5 mm ribs, max open).
- Physical verifications: the 4 mounting-hole positions, the ~1 mm bottom-cylinder vs NeoTrellis clearance, the exact rear-cavity packing into 46 mm.

## 12. Verification checklist (before declaring a phase done)
- **Phase 1:** screenshot front + side + 3⁄4; width hits 77/100/77/100/77 at Y 0/35/70.5/106/141; depth ≤46; front-face heights match §4.6; PCB fits centered with ≥~1 mm side margin at the pinches; 4 solid cylinders exact over §5.3 positions; watertight.
- **Phase 2:** ears blended (no seam, no thin lips, ≥2.5 mm); speaker pockets at the cube-point; split-jog clears each ear.
- **Phase 3:** all 12 cutouts at the §7.2 centers; lidar is a true open hole; USB-C has its flat landing; diamonds structurally sound; two halves separate cleanly and export.
