# changes-world2: hut interiors, hub rooms, pad plate (2026-11-25, builder "world2")

Owned files only: `js/ship-village.js`, `js/ship-hub.js`, `js/ship-planet.js` (pad geometry + its shader only), `js/ship-look.js` (untouched, nothing needed). Nothing committed, nothing run in a browser.
`bash tools/test-node.sh` = 445 pass / 0 fail; `node --check` + `acorn --ecma2022 --module` clean on all three files. Scratch node checks (three@0.160, scratchpad `w2/`): 40 seeded villages build, 10 villages simulated 150 s day->night (0 villager inside a prop, 85 villagers end inside), hub old-vs-new compare.

## Per file

### js/ship-village.js (task 1)
- `furnish(h, dr, yb, y1)` runs per hut after the floor. Local frame per hut: a = lateral (+-3.05 m), b = depth from the hut centre toward the back wall (door inner face = -2.6, back wall = +2.6); a seeded per-hut flip (`hashStr(pid:bed:idx) & 1`) mirrors the layout. Door lane |a| < 1.2 is free of every solid (door path never blocked).
- Props (m, 1 H = 1.75 m): bed 1.75 x 3.5, 0.79 tall (0.45 H): rounded frame + mattress/blanket + accent pillow + flat headboard; chest 1.05 cube (0.6 H) body + lid + gold latch; table 1.4 x 1.0, top 0.79, 4 legs, a gold bowl; 2 stools 0.5 x 0.5 x 0.5; wall shelf at 2.0 m on the back wall (not solid, above head) with 3 small items; rug = 2 soft discs (r 1.3 + r 0.62, 14/10 sides); hanging lamp = cord + cap (day mesh) + shade/bulb in the GLOW mesh (EP, warm [1.5 1.1 0.55]), so it dims by day and brightens at night through the existing `setNight` (`emiMat.color` 0.28 -> 1). Colours from TONES only.
- Draw calls unchanged (props merged into `village-day` / `village-glow`). Tris: village max of 40 seeds 36.8k -> 44.1k (+~800 per hut; legs/latch/boards are 12-tri flat boxes, the rest cached bevel templates).
- Collision: bed, chest, table, 2 stools are `mkBox('hut<i>-bed|chest|table|stool')` pushed into `walls` -> `village.solid.a` (-> objPush), `village.overlaps`, LOS. Same path as hut walls (min.y = floor - 0.1). Not in `village.huts[].walls`.
- `village.interactNear(p)` now checks huts first (inside the interior +0.4 m, |dy| < 2.5 m, nearest of bed/chest within 1.7 m = ~1 H), then bell / well as before.
  - bed -> `{ kind:'rest', label:'F · REST' }`, `use(ctx)` = `village.rest(h, ctx)`: `ctx.heal(1, 'rest')` if it exists, else only `ctx.aPlay('ui')`; always sets `village.rested = { t, hut, id }`.
  - chest -> `{ kind:'chest', label:'F · OPEN' }`, `use(ctx)` = `village.openChest(h, ctx)`: one item per chest per UTC day via `ctx.giveItem(item)` (50% `resourceItem(<seeded kind>, planet)`, 50% a seeded `storeMenu('village:<pid>:chest<i>', 8)` item, balls filtered; seeded by pid + hut + UTC date). Claimed only when `giveItem` exists and does not throw; second use the same day = sound only. `village.serialize().chests` / `restore()` persist the claim (same-day entries only).
- Villagers: indoor idle spot = standing beside the bed (`h.fur.spot(slot)`, slot = order within the hut, 3 spots, facing the bed). Home path = door -> lane point (a=0, b=-1.6) -> spot; leaving reverses it. First-frame night snap uses the spot. Villagers inside are still `inside` (placement/greet/punch logic unchanged) but their rig is drawn while the PLAYER is inside that hut (`inHutNow`).
- New API: `village.rest(h, ctx)`, `village.openChest(h, ctx)`, `village.chestItem(h)`, `village.rested`, `huts[i].fur` (internal: spot, lane, bed/chest rects, inner rect).

### js/ship-hub.js (task 2)
- Constants: `PLINTH_H 0.16`; palette `STONE_A = mix(dark, lo, .55)` (walls, ceiling), `STONE_B = mix(lo, mid, .4)` (floor skirting, pillars, frames, plinths, torch brackets); `SCONCE = mix(mid, white, .4)` = the baked torch light colour (`WARM`) AND the flame instance colour (violet, was orange / amber); ambient floor 0.10/0.09/0.14 -> 0.15/0.13/0.20; stone jitter +-4% (was +-15%).
- Rounded pieces (all baked-lit via `vtx`, strips split to a tile so torch light keeps resolution): `coveStrip` concave floor skirting r 0.5 (side walls; end walls outside the door gap; boss rear wall full) and ceiling cove in arenas/loot/boss; `cornerPillar` quarter-round r 0.55 in the 4 corners of arenas, loot and boss; `roundPillar` 8-sided r 0.85 replaces the 8 boss `tbox` pillars; rounded door-frame jambs + lintel (`box(..., seg 1)`, bevel template) at every gate + both loot doors; rounded plinth 1.7 x 0.16 x 1.3 under each chest (chest instance matrices lifted by `PLINTH_H`; `chest.pos` unchanged).
- Verified old vs new in node: `interior.walls`, `interior.rooms`, `floors`, `chests[].pos` identical (JSON), `floorAt` identical on a 31 x 14 x 106 grid (0 diffs). Tris 13126 -> 15702 (budget 16k), draws unchanged (9).
- Not routed through rounding: the tiled flat wall/floor planes (they are the baked-light surface), trench rims.

### js/ship-planet.js (task 3)
- New `PAD_T = 0.12 * 0.27` L (0.12 H = 0.0324 L, H_RATIO 0.27). Pad instance `lift` 0.1 L -> 0 (seated on the flattened ground). Old plate tops were +0.20..0.26 L = 0.74-0.96 H; studs 0.34 L.
- Geometry = `GeoBuilder.prototype.lathe(cx, cz, profile, sides)` (new): one soft plate (top r <= 0.84 at PAD_T, rim at 0.8 PAD_T, 3-step bevelled edge down to -0.03 L below the ground so a slope shows no gap), a violet ring (r 0.30-0.42, +0.011 L soft lip), 6 low dome studs (top PAD_T + 0.04 L = 0.27 H, was 1.25 H). 434 tris (checked: all normals outward, max y = 0.27 H from the studs).
- Shader (OUT_VERT, shared by pads + buildings): `aPart >= 3` = emissive violet `uVio` (0.68, 0.52, 1.0) x (aPart - 3); pads use 3.8 ring / 3.6 studs. Building parts (< 3) unchanged. `uVio` uniform added in `outMesh`.
- Landing consistency (checked in code): `strutFloor` (ship.js:4711) reads `ps.meshFloorLocal` = the analytic pad PLANE, never the plate; `ps.groundTruth` = max(terrain-with-pad-plane, registered slabs); g25 only probes 7/11 + Burger slabs vs groundTruth. None of them reads the plate mesh, so nothing moved: the ship lands on the plane and its struts end up <= 0.12 H inside the plate (was 0.74-0.96 H); the store/apron slabs sit on the plane exactly as before.

## NEEDS ship.js (not applied)

```diff
@@ footAct(), the existing  else if (ft.kind === 'world') ft.obj.use({ aPlay: aPlay, giveItem: ... })  (changes-world.md diff)
-        else if (ft.kind === 'world') ft.obj.use({ aPlay: aPlay, giveItem: function (it) { giveItem(it, 1); } });
+        else if (ft.kind === 'world') ft.obj.use({ aPlay: aPlay, giveItem: function (it) { giveItem(it, 1); },
+            heal: function (frac) { /* F · REST: restore frac (1 = full) of the on-foot player's health; wire to the foot HP var / existing heal helper */ } });
```
Without `heal`, REST only plays the sound and sets `village.rested`. Without the changes-world.md `worldInteract` hook nothing here is reachable (bed / chest prompts ride the same `village.interactNear`).

Optional (feel): foot floor = the pad PLANE, plate top is 0.12 H above it, so on the pad plate the feet are ~0.12 H inside the plate (0.21 m). To make feet ride the plate, `ps.groundTruth`/`meshFloorLocal` would need `+ PAD_T * L` inside the plate radius; deliberately not done (it moves the ship/7/11/test floors).

## Constants
| where | name | value |
|---|---|---|
| ship-village | `IN_A`, `IN_B`, `IA_R` | 3.05 m, 2.6 m, 1.7 m |
| ship-village | bed / chest / table / stool heights | 0.79 / 1.05 / 0.79 / 0.5 m |
| ship-hub | `PLINTH_H`, cove R, corner pillar r, boss pillar r | 0.16, 0.5, 0.55, 0.85 m |
| ship-planet | `PAD_T` | 0.0324 L (0.12 H) |

## 8-line by-eye checklist
1. Enter a hut: bed (blanket + pillow) on one side, chest back corner, table + 2 stools opposite, rug and a lamp in the middle; the door lane stays clear; you cannot walk through bed / chest / table / stools.
2. Hut at night: lamp shade glows warm (dim by day); a villager stands beside the bed facing it while you are inside.
3. Bed: prompt F · REST within ~1 H (sound; heals once ship.js `heal` is wired). Chest: F · OPEN gives one item, again the same UTC day gives nothing.
4. Hub: two stone tones (deep violet walls, lighter lavender floor/pillars), violet torch flames and light, no orange.
5. Hub arenas/boss: soft skirting along the floor, quarter-round pillars in every corner, round boss pillars, rounded frames at every gate; gates still open/close and nothing blocks the doorway.
6. Hub loot room: each chest on a low rounded plinth, lid still opens.
7. Pad: the plate is a thin low disc (knee-high lip, ~0.12 H), rounded edge, a soft violet ring, 6 low violet studs; 7/11 apron/parking sit next to it with no 1 H step.
8. Land on the pad: the ship sits on the plate without floating or visibly sinking (struts only ~0.12 H into the plate).
