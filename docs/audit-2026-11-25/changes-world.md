# changes-world: buildings / look / camera / feel (2026-11-25, builder "world")

Owned files only: `js/ship-world.js`, `js/ship-village.js`, `js/ship-hub.js`, `js/ship-station.js`, `js/ship-foot.js`, `style/ship.css`, NEW `js/ship-look.js`.
Nothing committed. Nothing run in a browser. `bash tools/test-node.sh` = 445 pass / 0 fail (none of the node tests import these files; geometry/camera were smoke-tested with three@0.160 in a scratch dir, see "How it was checked").

## What changed, per file

### js/ship-look.js (new) — the one look
- `DUSK` = mirror of the violet-anchored family `dusk-lav` (galaxy3d.js:73, cdcc583). `palette(ps)` reads the live `ps.palette` ramp, else DUSK. `TONES` = building roles derived from it (wall, trim, dark, floor, shelf, shelfD, chrome, accent, glow, stock[4], plus the 5 ramp stops).
- `softMat(T, ps, opts)`: the one lit material = ship-planet `litHemiKey` rig (sky fill + wrapped sun key + moon key + fog) with a 4-step soft toon ramp on the key + a faint rim. Binds to `ps.shared` / `EMGOR_SKY_UNIFORMS`. `glowMat` = emissive twin. `unlitMat` = MeshBasic pass-through (hub baked light, signs, glass).
- Rounded boxes: `rboxEmit` (moved here, ship-world re-exports it), `bevelTemplate(w,h,d,rad,m)` cached by dims+radius+m (ship-geo-cache), `boxKind` (bevel / soft / flat), `bevelRadius`, `softNormal`.
- API is documented at the top of the file.

### js/ship-world.js
1. PAD THROUGH 7/11: `storeOffset(padR) = 0.866 * padR + 1.2 L + (parkZ1 - HD) * KS`; `storeMatrix(padM, out, padR)` uses it. `padFrames()` now returns `radius` (= `outposts[i].radius`, fallback `6 * OS * L`), stored as `site.padR`. Result (measured in node with a fake 15 L pad): glass front at 15.87 L from pad centre, the nearest corner of store/apron/parking is 1.28 L outside the pad flat edge, door faces the pad (+Z). `world.storeOffset` exported for tests. `layoutBurger` rejects candidates on the pad plate (hex + 2.6 L) or within 5 L of the store (penalised, never silently used).
2. ROUNDED: `makeSoup.box` = bevel geometry for boxes with every side >= 0.2 m (radius `min(0.16, 0.3 x smallest side)`, 44-48 tris from a cached template), 12-tri box with pillowed vertex normals for 0.12-0.2 m or skip-face boxes (stock blocks), flat below 0.12 m (thin boards). `rbox` also uses the cached template. Shelf racks use a rounded unit box. The compact (station) store bevels only boxes >= 0.6 m (modelled 2.3x larger).
3. PROPORTIONS: store ceiling 2.8 H (4.9 m), door 1.0 H x 1.6 H, counters 0.63 H, rack stands 0.69 H (stock top 0.93 H), shelves 0.6-1.2 H unchanged. Burger House: ceiling 2.8 H, door 1.0 x 1.6 H, sill 0.6 H, window opening 1.25-2.6 m, awning raised to 3.5 m, menu board hung at 3 m, CVS counters 0.6 H. Walls unchanged (0.3 m store/Burger; foot sub-step is 0.12 H so no tunnelling).
4. LOOK: all materials via ship-look (`makeLitMaterial` kept as a thin wrapper). Store/Burger/CVS colours from TONES: wall + trim + dark + 4 stock tones + accent; brand marks only: 7/11 stripes + red, Burger red, CVS red/green.
7. `world.interactNear(feetPlanetLocal)`: Burger House seats (5 stools, 8 bench seats), label `F · SIT`, `F · STAND` while seated. `use()` sets `SEAT.req` (ship-foot).

### js/ship-village.js
- Hut: 7.9 x 7.0 m outside (interior 6.1 x 5.2 m = 3.5 x 3.0 H), ceiling 4.9 m (2.8 H), walls 0.9 m (0.51 H, unchanged), door 1.8 x 2.85 m (1.03 x 1.63 H), windows 1.24 x 1.1 m at 2.7 m. Hut rings moved out to 20 / 32 m so the bigger huts keep clear of plaza, plots, stall (40/40 seeded test planets still build, 5-9 huts).
- Hinged door leaf per hut (own mesh, shared geometry): eases ~100 deg open inward (`DOOR_EASE 7`) when the player is within 1.5 H (2.6 m) or a villager is at the threshold.
- Bell is now a swinging mesh (static box removed, rope at the NE post). `village.ring(ctx)`: swing ~9 s, every villager (not in panic, not at night) walks to the plaza (r 4.2 m) for 30 s. Well: `village.drawWater(ctx)` = splash animation; calls `ctx.giveItem(waterItem)` if present.
- `village.interactNear(feetPlanetLocal)` -> `{ kind, label:'F · RING BELL' | 'F · DRAW WATER', use(ctx) }` (bell within 2.2 m of the NE post, else well within 3.9 m of centre).
- Boxes through the cached bevel templates (`pushBox`/`rboxG`, same rng() count per box so seeded layouts/trade/villager counts are not shifted by the bell change). Palette from TONES (wall + roof + wood accent). Materials via ship-look.

### js/ship-hub.js
- Arch top 4.35 m -> 6.09 m (3.5 H): `ARCH_Y 0.5 -> 0.7` (torches, veil, light, colliders all scale with it).
- Arch + apron blocks are rounded (bevel template, longest-edge subdivision so baked torch light keeps its resolution); dungeon masonry stays tiled (`tbox`, boss pillars + trench rims). Stone palette = ramp (moss -> lighter lavender). All materials via `unlitMat`. Tris 11380 -> 13126.

### js/ship-station.js
- Own VERT/FRAG removed: `softMat` (brighter ambient 0.5, the hangar has no sky) + `glowMat` + `unlitMat`. Soup now carries normals; boxes with every side >= 0.4 L are bevelled (radius <= 0.06 L), thinner strips stay flat. Hull/light colours from TONES (cyan/magenta/amber -> pale lavender/accent/glow). Tris 10010 -> 12818 (includes the standalone store 3620 -> 4302).

### js/ship-foot.js
5. CAMERA (constants `CAM` at the top): boom 3.2 H (was 4.5), wheel zoom 2-5 H (0.3 H per notch, only while pointer-locked and no menu), wall/roof pull-in (probes every 0.3 H along head->lens through `ctx.objPush`, snap in instantly, grow back at 6 H/s, floor 0.8 H, margin 0.15 H, smoother rate 40 while snapping), INDOOR mode when >= 2 of 4 upward probes (1.6/2.2/2.8/3.4 H) hit a roof (debounced 0.15 s on / 0.35 s off): boom 1.6 H, head 0.85 H, pitch x0.6, shoulder x0.5, FOV +10 deg (`S.fovAdd`), 0.25 s blend. Flight camera uses the same pull-in.
6. FEEL (`FEEL`): accel 0.18 s, decel 0.14 s, reversal brake 0.08 s (was 0.067 / 0.075 / 0.033). Measured: 4 H/s reached in ~0.17 s, stop in ~0.14 s.
7. `SEAT` (exported): pose freeze at a Burger House seat; any fresh move key / Space / Esc, or `SEAT.leave`, stands you up where you sat down from. **Human sit pose is not reachable from my files** (ship-human.js is not mine): the rig is squashed to 0.72 H and the camera head drops with it ("camera-only sit"). A real sit pose needs a ship-human.js `sit` animation state.

### style/ship.css
- Tokens at the top: `--hud-base`, `--hud-shade`, `--hud-edge`, `--hud-font` (the ONE stack), `--hud-r: 16px` (ONE radius), `--hud-shadow` (ONE shadow). Raw `rgba()` of violet / alert / cyan / ink / base / shade / edge / purple-bright became `color-mix(in srgb, var(--token) N%, transparent)` (28 raw rgba remain: white overlays and one-off colours with no token). `var(--mono)` and the literal Monocraft stack -> `var(--hud-font)`; radii 0/3/6/12px -> `var(--hud-r)` (small chips and bars clamp to pills, panels become soft cards; 50% dots untouched). No selectors/layout/behaviour changed.

## Constants (all in code, grep these)
| where | name | value |
|---|---|---|
| ship-world | `PAD_GAP_L`, `HEX_APO`, `PAD_R_L` | 1.2 L, 0.8660254, 6 (x OS) |
| ship-world | PD dims (makeDimsH) | H 2.8 H = 4.9 m, door gh 1.6 H, DW 0.5 H, apronZ1 HD+4.4, parkZ1 HD+10.9 |
| ship-world | `BH.H`, `BH.SEATS` | 2.8 H, 13 seats |
| ship-look | `LOOK` | STEPS 4, SOFT 0.07, RIM 0.22, BEVEL {geoMin 0.2, softMin 0.12, frac 0.3, rMax 0.16} |
| ship-village | `TMPL`, `DOOR_W/H`, `HUT_R`, `DOOR_OPEN_R/A`, `BELL_CALL_S` | 7.9x7.0x4.9 m, 1.8 x 2.85 m, 20/32 m, 2.6 m / 1.75 rad, 30 s |
| ship-hub | `ARCH_Y` | 0.7 |
| ship-foot | `CAM`, `FEEL`, `SEAT` | see top of file |

Tri counts: 7/11 5658 (budget 6000; was 3808), Burger 3660 (2868), CVS 2378 (1966), compact store 4302 (3620), hub 13126 (11380), station 12818 (10010), village max of 40 seeds 36.8k (31.5k).

## NEEDS ship.js (exact diffs; nothing here is applied)

```diff
@@ G_WALK / G_RUN (ship.js ~5291)  [task 6]
-    var G_WALK = 6, G_RUN = 13;   // in human heights (/s, /s^2); rev 29: ...
+    var G_WALK = 4, G_RUN = 9;    // in human heights (/s, /s^2); 2026-11-25 plush pass (was 6 / 13)
```

```diff
@@ foot FOV: indoor widening (ship-foot writes S.fovAdd in degrees)  [task 5]
+    var footFovAdd = 0;      // next to styFov (~7095)
@@ createFoot ctx literal (~5442), next to the styFov accessor
+            get fovAdd() { return footFovAdd; }, set fovAdd(v) { footFovAdd = v; },
@@ ship.js ~5509 (the foot/galaxy FOV line)
-        fov += (targetFov({ mode: gmode, base: baseFov }) - fov) * damp(4, dt);      // on foot 55 (galaxy stays 42)
+        fov += (targetFov({ mode: gmode, base: baseFov }) + (gmode === 'foot' ? footFovAdd : 0) - fov) * damp(4, dt);      // on foot 55 (+ up to 10 indoors); galaxy stays 42
```
(Without this, indoor mode still shortens the boom/lowers the head but the FOV does not widen.)

```diff
@@ world interactions: bell, well, Burger seats  [task 7]
@@ next to footTarget (~8880)
+    function worldInteract() { return (world && world.interactNear && world.interactNear(hum.pos)) || (VG.v && VG.v.interactNear && VG.v.interactNear(hum.pos)) || null; }
@@ footTarget(), right after the `if (gmode !== 'foot' || ...) return ftI;` guard
+        var wi = worldInteract();
+        if (wi && wi.kind === 'stand') { ftI.kind = 'world'; ftI.obj = wi; ftI.label = wi.label; return ftI; }      // seated: F stands up first
@@ footTarget(), right after the `if (world.nearCvs) {...}` line (before r28ShelfFind)
+        if (wi) { ftI.kind = 'world'; ftI.obj = wi; ftI.label = wi.label; return ftI; }      // F · SIT / F · RING BELL / F · DRAW WATER
@@ footAct(), before `else punch();`
+        else if (ft.kind === 'world') ft.obj.use({ aPlay: aPlay, giveItem: function (it) { giveItem(it, 1); } });
```
`giveItem(it, n)` is the existing ship.js:6386 helper; the well hands it `resourceItem('ice', planetName)` ("<Planet> Ice").

Optional, not needed by anything above: a precise `camClear(wa, wb)` next to `hasLineOfSight` would replace the objPush probes in ship-foot `camClip` (probes are conservative: they use the 0.26 H capsule, so the lens keeps >= 0.26 H + 0.15 H off a wall, and an aisle/gondola can also pull the boom in).

## Not done / limits
- Hut interiors are still empty (no bed/chest/table); only size, door, camera. Not in the task list.
- Sit is a squash + camera drop, not a sit pose (see above).
- The pad plate's own height (`lift` in ship-planet.js, audit A1 secondary) is not touched (not my file).
- Hub dungeon rooms stay flat tiled masonry (only the entrance arch/apron are rounded).
- Store wall thickness stays 0.3 m (0.17 H); only the huts are >= 0.5 H. Foot sub-step is 0.12 H so tunnelling is not possible at walk 4 / run 9 H/s.

## How it was checked
- `node --check` on every edited file, `npx acorn --ecma2022 --module` on world/village/foot/look, `bash tools/test-node.sh` (445 pass).
- Scratch node scripts (three@0.160 installed under the session scratchpad, not in the repo): built the 7/11, Burger, CVS, hub, station and 40 seeded villages (tri counts, NaN-free), a fake 15 L hex pad (store gap 1.28 L, door faces pad, Burger lot off the plate), village bell (7/8 villagers at the plaza after 20 s), `createFoot` with a fake ctx (boom 3.22 H, wall at 1 H -> 0.8 H snap-in then eases back at 6 H/s, ceiling at 2.8 H -> boom 1.61 H + fovAdd 10, walk ramp/decel, seat on/off).

## 10-line by-eye checklist (owner)
1. Land on an outpost: the 7/11, its apron and the 3 parking slabs are fully off the hex pad, with a visible gap; glass door faces the pad. Burger House is not on the plate.
2. 7/11 and Burger House look rounded (walls, roof, counters, awning slats), 2-3 tones + the stripe/red brand accents, no cyan/green candy colours on the shelves.
3. Inside the 7/11: ceiling is clearly higher than your head (2.8 H), the door is wider/taller, the counter is at chest height, you can reach the racks.
4. Walk into a hut: it is a real room (3.5 x 3 H), ceiling high, door leaf swings open as you come within ~2.6 m and shuts after.
5. Back up against a hut/7/11 wall: the camera snaps in instantly, no clipping; step away: it eases back out (~0.5 s).
6. Step under any roof (store, hut, Burger, awning): camera comes close (1.6 H), drops a little, FOV widens (needs the ship.js FOV diff), then eases out (0.25 s) when you leave. No flicker in doorways.
7. Mouse wheel while playing: zoom 2-5 H; default feels closer than before (3.2 H).
8. Walk start/stop: ~0.18 s up, ~0.14 s down (needs G_WALK 4 / G_RUN 9 for the speed). No digital snap.
9. Village: F at the NE well post rings the bell (swings, villagers walk to the plaza for 30 s); F elsewhere at the well splashes (needs the ship.js interaction diff for prompts). Burger booths/stools: F sits (rig squashes), W/A/S/D or Space stands.
10. HUD: one font, rounded pills/cards, same colours as before; hub arch is tall (3.5 H); station hangar is softer, pale lavender lights.
