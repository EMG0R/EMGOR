# AUDIT: buildings / structures / interior camera (NO MANS GOR)
Tree audited: branch main @ 0a59189. NOTE: main was fast-forwarded (pull --ff-only, 17:53) DURING this audit; I re-read everything on 0a59189 (the Q55/Q60/Q61 human-scale rewrite + E2). docs/HANDOFF.md "CURRENT STATE" table is stale (H_RATIO 0.2, 64x40 store) -> real H_RATIO = 0.27 (ship-scale.js:6).
Node tests (bash tools/test-node.sh): 428 tests, 428 pass, 0 fail, 0 skipped.
Units: H = 1.75 m = 0.27 L. 1 store/hut metre = 0.571 H = 0.1543 L (K = H_RATIO*L/1.75, ship-world.js:1088).

## (A) ROOT CAUSES

### A1. "platform goes through the 7/11"  -- confidence HIGH (geometry, from code; not seen in browser)
The 7/11 was moved ONTO the outpost landing pad when it was shrunk to human scale. Pad radius was left alone.
- Pad: hex, radius PAD_L = 6*OUT_S = 15 L (ship-planet.js:69-70; instance X/Z scale `sp = PAD_L*L` at :1816-1822; outpost.radius :1846). Hex vertices at 0/60/..deg (hex() ship-planet.js:~1100 uses cos/sin(i/6*2pi)) -> apothem 0.866*15 = 13.0 L along pad Z.
- Store placement: ship-world.js:1151-1153
  `makeTranslation(-0.75*OS*L, 0.05*L, -(4.5*OS*L + PD.HD*KS))`, scale KS, and KS = K (no longer K*OS) at :1089.
  Before f30b031 (Q55/Q60) this was `-(8.93*OS*L + PD.HD*KS)` with KS = K*OS and a 64x40 m store: glass front 22.3 L out, apron/parking out to ~16 L, i.e. just clear of the 13-15 L pad.
- Now (PD = makeDimsH(22,14), ship-world.js:236; K = 0.1543 L/m), in pad-local L:
  glass front z = -11.25; store body z = -13.4..-11.25 (22x14 m = 3.4 x 2.2 L); x centre -1.875 (+-1.7);
  apron (AZ0..AZ1 = HD-0.3..HD+4.4 m, :~470) z = -11.3..-10.6; parking slabs (PZ0..PZ1 = HD+3.2..HD+10.9 m, PX = -8.5/0/8.5 m) z = -10.8..-9.6.
  => EVERYTHING (store, apron, 3 parking slabs) is inside the 13 L pad apothem; the store back wall (z -13.4) sits on/just past the pad edge. Mismatch: store offset 11.25 L < pad radius 15 L (apothem 13 L).
- Why it "goes through": the pad mesh (ship-planet.js:1647-1653, instance Y scale = L at :1819-1821) is flat discs at y = 0.10/0.12/0.14/0.16 L, plus 6 posts to 0.34 L, all lifted by `lift = 0.1*L` (:1818) above the pad plane. => plate tops at +0.20..+0.26 L = 0.74..0.96 H (~1.3-1.7 m) above the plane, posts to +0.44 L. The store floor is seated ON the plane (seatFloors, ship-world.js:1160-1185, floor top ~+0.03 L); store walls are 3.15 m = 0.49 L tall. So the pad plate/rim/lit ring cuts through the 7/11 walls at chest..head height and hovers over the apron/parking slabs. Nothing in layout (storeMatrix, seatFloors, floorPut/fitPads) knows about the pad MESH, only the pad plane.
- Secondary, verify by eye: the "a hair above the ground" comment (:1817) vs lift 0.1 L = 0.37 H. If the plate really floats 0.74-0.96 H above the ground the pad is a table, independent of the store. g23/g25 test ground truth (plane) not the visible hex mesh (same blind spot Q55 noted).
- Burger House (layoutBurger ship-world.js:1202) candidates are (-6,-4)*OS*L = (-15,-10) L etc.; first flat one wins (always cand 0 inside the 37 L flat disc). That is ~18 L from pad centre, outside hex at z=-10 (|x|<=9.2) -> no overlap. CVS is 60 L away. Tower (ps-bld, +27.5 L on pad +X) no overlap.
- FIX (S): in storeMatrix use the real pad radius: front offset >= apothem + margin + parking depth = 13 + 1 + (10.9-... PZ1-HD = 10.9 m*K = 1.68 L) = ~15.7 L => replace `4.5*OS*L` by `(padRadiusL*0.866 + 1 L + (PZ1-HD)*K)` where padRadius comes from `ps.outposts[i].radius` (= PAD_L*L; padFrames :1115 currently returns only {id,name,m,key}: add `radius`). Alt: shrink pad (PAD_L) and keep store at 11.25 L (changes ship landing size, riskier). Re-check burger cand 0 stays clear of the store (store x -5.5..1.7 L, z -17.5..-9.6 after fix; burger (-15,-10): clear).
- Add a visible-mesh test: g-test that samples the store footprint corners vs pad hex (|x|, |z| vs 15*cos30) -> fails today.

### A2. "going into villager house doesn't adjust the camera"  -- confidence HIGH
There is NO interior/occlusion camera on foot.
- Foot cam = ship-foot.js:258-277: focus gC = head (1 H up); wanted pos gD = gC + back-pitch dir * FOOT_CAM(4.5)*H*styCam (:267) + 0.35 H right shoulder (:268); only constraint = floor clamp `mFloor(fsT...)` (:273-275, ground/slab only, never walls/roofs); smoothed by groundCam (ship.js:5279) rate 16 (:277).
- Structure solids exist (opList from objBegin, ship.js:5353: store walls incl. roof 'wall', Burger House, CVS, village hut walls + roof slabs) and `hasLineOfSight(wa, wb)` (ship.js:5386, uses losBoxes) already ray-tests them for mobs/bolts -- never used for the camera.
- The ONLY camera with wall clearance is the hub dungeon: ship.js:7741-7752 (steps d 0.5..5.5 in 0.4 steps, floorAt/ceiling/hubFree, HB.camD eases out slowly, snaps in instantly). That is the model to copy.
- Huts: ship-village.js:209-259. Footprint 6x5 m, walls 0.9 m thick (TW :42), wall top y1 = yb+2.65 m, solid roof slab y1..y1+0.35 (:227), rounded dome above (:245). Interior free space 4.2x3.2 m x 2.65 m = 2.4 x 1.8 H, ceiling 1.5 H. Camera sits 4.5 H (7.9 m) behind head at +pitch: always outside the walls (door-wall gap 1.7 m) so the view is a solid wall/roof between lens and player. Not even a door gap view. Also: huts are EMPTY (no furniture/F targets), villagers "inside" are hidden (setPath/`v.inside`, :574, :407) -> nothing to see even with a good camera.
- 7/11 interior has the same problem and is worse: 8 H deep, ceiling only 1.8 H (3.15 m) vs 4.5 H camera: camera passes through back wall/roof. Q60 compressed the interior to human scale without touching the camera (ceiling 21 H -> 1.8 H).
- Proposed mechanism (3 layers, in order):
  1. (M) Camera occlusion pull-in in ship-foot.js after gD (:267-272): binary/step-shorten along gC->gD using the same box solids (expose `camClear(wa,wb)` next to hasLineOfSight in ship.js, return first-hit t; pass via createFoot ctx like mFloor). Margin 0.3 H, min dist 0.8 H, ease out like HB.camD (snap in, damp(3) out). Fixes huts/7/11/Burger/CVS in one place. Clamp camera Y under the roof slab/ceiling (opList boxes cover it).
  2. (M) "Inside" state: when head point is inside a structure footprint (store `interior.inside(p)` ship-world.js:856; hut `village.huts[i].box` min/max; burger `matrix`/walls), lerp FOOT_CAM to ~2.2 H, raise FOV, lower pitch cap so the camera stays below the ceiling; also nudge toward the door axis for a 3/4 view. Hook: styCam-like scalar in ship-foot.js:162-165.
  3. (L, optional) Cutaway roof: huts/store roofs are merged into the lit mesh (village LP/LC arrays, store `bodies` instance) -> split roof into its own mesh (village :227/:245/:248 -> separate geometry; store 'roof' wall in buildStoreGeomsH) and fade/discard it when the player is inside that footprint (uniform uCut per instance).
  4. (M) Make huts worth entering: bed/chest/table with F targets + show villagers inside instead of hiding (ship-village.js setPath/inside logic).

### A3. Scale root cause of "not big/plush"
Store rewritten to 1.8 H ceiling (Q60 owner request: "ceilings too high") but camera still assumes outdoors (4.5 H). Owner now wants bigger. Single fix: interiors ~2.5-3 H ceiling + camera pull-in (A2). Data in (B).

## (B) MEASURED PROPORTIONS (H = 1.75 m = 0.27 L; planet human rig = 1 H)
| structure | footprint | wall/eave h | ceiling inside | roof top | door (w x h) | other | source |
|---|---|---|---|---|---|---|---|
| 7/11 planet | 22x14 m = 12.6x8.0 H = 3.4x2.2 L (+1.6 slab; apron 26 m wide) | 3.15 m = 1.8 H = 0.49 L | 1.8 H | 2.1 H (roof 0.8 m + eaves) | 0.9 H x 1.25 H (DW 0.45*HM half-width, gh 1.25*HM) | counter 0.55 H, shelves <=1.2 H x3 tiers, aisle pitch 2.75 m = 1.6 H, parking slab 7.5 m = 4.3 H | ship-world.js:236,~280-360,1089 |
| Burger House | 9x7 m (BH.WX 4.5, WZ 3.5) = 5.1x4 H = 0.8x0.6 L ... walls 0.3 m | 3.15 m = 1.8 H | 1.8 H | 2.1 H | 0.9 H x 1.25 H (DWW 0.9*HM, DH 1.25*HM) | counter 0.55 H, 5 stools, 4 booths (bench 0.45 m, back 0.95, table 0.75 m) | ship-world.js:501-565 |
| CVS | 10x6 m walk-up, lot 17x15 m | 3.4 m = 1.9 H | none (no interior; two counter windows x +-2.8 m) | ~2 H | n/a | still old pre-Q60 numbers (3.4 m walls, 0.96/0.75 m window pieces) | ship-world.js:598-640 |
| Hut | 6x5 m = 3.4x2.9 H outer, walls 0.9 m (0.5 H) -> interior 4.2x3.2 m = 2.4x1.8 H | 2.65 m = 1.5 H | 1.5 H | ~5.6 m = 3.2 H (slab 0.35 + dome ~2.6 m) | 1.7 m x 2.1 m = 0.97 x 1.2 H | empty inside | ship-village.js:41-44,209-259 |
| Hub arch | span 6.9 m = 4 H | top 4.35 m = 2.5 H | corridors h 4 m = 2.3 H, boss room 14 m = 8 H (hub m) | 2.5 H | gate GW 2 | | ship-hub.js:25-39 |
| Station deck | hangar 9 L wide x 4.8 L high x 26 L deep; deck human = 0.09 L (1/3 of planet H) -> ceiling = 53 deck-H | 4.8 L | | | mouth 9x4.8 L | station 7/11 CD 40x26x12 m, STORE_S = 1.5*0.09/1.75 = 0.077 L/m (1.5x human) | ship-station.js:29-31,251 |
| Pad (reference) | hex radius 15 L = 55 H; flat disc 37.5 L, blend 55 L | plate +0.20..0.26 L (0.74-0.96 H), posts +0.44 L | | | | | ship-planet.js:69-70,1647 |
| Outpost tower (ps-bld) | ~8x7 L block at +27.5 L | 4.4 L (16 H) main + mast to 14 L | | | | decorative | ship-planet.js:~1655, BLD_S 1.375 |
Observations: human proportions are now consistent (door 1.2-1.25 H, ceiling 1.8 H), but a 1.8 H ceiling with 3.4 m^2-scale rooms reads cramped/"dollhouse" with a 4.5 H chase camera; pad (55 H wide) is huge vs store (12.6 H wide) -> the outpost reads as a store lost on a runway. Station deck is a different scale again (1/3 H) -- inconsistent with planet stores.

## (C) INTERACTION INVENTORY
LIVE (ship.js footTarget :8878-8898, villageTarget :7946, hubTarget ~:7890):
- 7/11: F SHOP at counter (counterNear -> openTrade), F CHECKOUT with cart, F GRAB shelf item (r28ShelfFind), clerk T TALK (npcNear :2576), shoppers/dealers T TALK/DEAL (world.nearNpc ship-world.js:1655-1661), security/cops/heat/shards (world sim), quest deliver at counter.
- Burger House: F FRIES at window (world.nearBurger, windowPos ship-world.js~1658); clerk NPC placed at burger.clerk.
- CVS: F REVIVE / F HEAL windows (cvsUse ship.js:10141).
- Village: villager T TALK / TRADE, F fight/battle (villageTarget), trader market + quest delivery (villageTrade :7953). Only reachable when the villager is outside (inside = hidden).
- Hub: F ENTER DUNGEON, F OPEN chests, F LEAVE, room fights/gates/boss.
- Station: 7/11, CVS, trade terminal, shipyard, mission board, map pedestal (ship-station + ship.js:9270-9279).
DEAD / DECORATIVE (never referenced in ship.js; grep of VG.v.* finds only update/drain/market/nearVillager/solid/toParent/syncStock/serialize):
- Village well, bell (village.bell exists, no ring action), crop plots, market stall (solid collider only), plaza/paths, hut doors (no F, no open animation), hut interiors (empty; no bed/chest/stove).
- Burger House: booths, stools, menu board, neon, grill/fryer (visual only; only the window is interactive; no enter prompt/seat/eat-in).
- 7/11: coffee machine (aisle collider only), hanging slurpee, pole sign, racks (6 `racks` positions), parking slabs/ cart corral; no F to sit/rest.
- Outpost: pad lit ring, tower, mast, beacon (pure mesh; no tower interaction, no door).
- Hub arch: torches, banners = decoration.
- No building has: rest/sleep/save, storage chest, crafting bench in a house, hut ownership, door open/close.

## (D) PRIORITIZED FIX LIST
1. (S, P0) Pad/store overlap: storeMatrix offset from real pad radius (ship-world.js:1151; add radius to padFrames :1115 from ps.outposts[i].radius). + visible-footprint-vs-hex g-test (ship-planet-tests.js, next to g23/g25). Then eyeball pad plate height (`lift`, ship-planet.js:1818 and hex y 0.1-0.16): if the plate floats 0.74-0.96 H above ground, drop lift to ~0.005 L and y to ~0.01-0.03 L so the pad is a flat decal-thick slab (S).
2. (M, P0) Foot camera occlusion: expose camera ray test from ship.js opList (extend losBoxes/hasLineOfSight :5386) -> pass into createFoot ctx (ship.js:5427-5431) -> shorten gD in ship-foot.js:267-272 with hub-style easing (ship.js:7749). Covers huts, 7/11, Burger, CVS.
3. (M, P1) Interior camera mode: scalar styCam-style distance/pitch/FOV lerp when head is inside a footprint (store.interior.inside, hut.box, burger/CVS matrix); target ~2.2 H, ceiling-aware.
4. (M, P1) Plush scale pass: interiors ceiling 1.8 H -> ~2.6-3 H, store footprint 22x14 m -> ~30x20 m, door 1.25 H -> 1.5 H x 1.1 H, hut 6x5 -> ~8x7 m with wall 0.9 -> 0.6 m, eave 1.5 H -> 2.2 H. Edit makeDimsH (ship-world.js:~213-234: H = 1.8*HM, W/D at :236, gh/DW), buildBurgerGeoms constants (BH :501, H :505), CVS (:598-640, still old 3.4 m), TMPL (ship-village.js:44), BIG/TW (:41-42). Keep thickness >= 0.5 H (foot step tunnelling comment :42). Re-run g19/g22/g23/g25. Retune pad offset (A1) with new footprint.
5. (M, P1) Huts worth entering: interior props (bed, chest, table, stove) as foot-solids + F targets (sleep/save, storage), show villagers inside instead of hiding; door "open" state. Hook footTarget (ship.js:8878) via a `village.interact` list (ship-village.js).
6. (S, P2) Wire dead props: bell ring (village.bell.pos -> sound + villagers gather), well (drink/heal), plots (harvest via resFind), Burger enter/eat prompt.
7. (L, P2) Cutaway roofs: split roofs into separate meshes (village :227/:245/:248; store roof wall in buildStoreGeomsH) and fade on entry.
8. (S, P2) Station deck unify to planet H (deck human 0.09 L vs 0.27 L) -- design decision (hangar 53 H tall); at least match store scale.

## (E) AESTHETIC STYLE-PASS PLAN (plush / rounded / minimalist)
Current: mostly hard-edged boxes. Counts: ship-world box 204 vs rounded 48 (rbox/pillar/slab); village 29 vs 4; hub 9 vs 0; station 76 vs 0. Walls, roofs, shelves are `box()`; some rounding exists (rbox with smooth normals via rboxEmit ship-world.js:~118, pill `slab`, `pillar` domes, village loaf-dome roof :245). Lighting already unified: FRAG_LIT (ship-world.js:79-100) = hemisphere fill + wrapped key + fog, same as ship-planet LIGHT_GLSL (:237) and OUT_FRAG (pad/tower). No toon/matcap yet (characters have toon ramp in ship-human.js).
Where to centralize (cheapest hooks):
1. makeSoup.box (ship-world.js:~134): route through rbox with default `rad = min(0.12 m, 0.25*min(w,h,d))` + `m=1` => every cuboid in 7/11/Burger/CVS becomes bevelled in one edit (204 call sites). Budget: store geo must stay <= 6000 tris (ship-world.js:42, world.storeTris); use m=1 and skip hidden faces; watch ship-geo-cache (js/ship-geo-cache.js) key: bump the theme/version key so cached geometry rebuilds.
2. Village: `boxG` (ship-village.js:159) calls rboxG with no radius -> give it a default radius; walls need rad <= wall half-thickness.
3. Hub (ship-hub.js box helpers) and Station (ship-station.js `B.bx`, 76 calls): add the same rounded default (they have separate soup builders; import rboxEmit from ship-world.js as village already does :29).
4. Materials are created in: ship-world.js makeLitMaterial (:~145, exported; used by village :318 and world matBody :699), matEmi/matShard/matSign (:700-715), ship-station.js:212-217 (own VERT/FRAG), ship-hub.js:214-254 (MeshBasicMaterial = UNLIT, off-style), ship-planet.js OUT_VERT/OUT_FRAG (:536-560; pads/tower). Plan: ONE exported `STYLE` module (palette + shading params) consumed by makeLitMaterial, station matLit, hub (switch MeshBasic -> makeLitMaterial), OUT_FRAG.
5. Soft shading: in FRAG_LIT add (a) 3-4 step soft ramp (smoothstep bands on `w`, like ship-human toon ramp), (b) rim/fresnel term, (c) cheap vertical-gradient AO via vC*(0.85+0.15*h). Uniform-driven so one knob controls all structures.
6. Fewer colours: define palette (e.g. 5 tones from planet ramp + 1 accent) in STYLE; builders currently hardcode per-file constants (7/11 WALL/TRIM/DARK/FLOOR/SHELF..., ITEM_COLORS, SEVEN stripes; Burger RED/CREAM/VINYL; CVS RED/GRN; village ROOF/WALL/WOOD/PATH). Keep brand marks (7/11 stripes, burger red) as the only saturated accents.
7. Silhouette: bigger corner radii on exteriors (0.25-0.4 m), domed/soft-eave roofs on 7/11 and Burger (village already has loaf roof), pill slabs exist (SL_B 0.52). Fix the pad too: hex slab with bevel instead of paper-thin floating discs (ship-planet.js:1647).
Ordering suggestion: A1 + camera (D1-D2) first; then plush scale (D4) and style pass (E1-E5) together, since rounding radii depend on final dimensions and the geo cache key.
