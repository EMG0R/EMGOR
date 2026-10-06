# GOR MASHUP — plan v1 (character styles, proximity fights, mobs, hubs, villages)

Status 2026-10-06: plan only, nothing built. Builds on docs/ship-mode.md (on-foot capsule, ps, relay) and docs/smash-plan.md v2 (2D fighter, host-authoritative net).
Rule zero: with no character switched and no mashup feature triggered, on-foot behaviour is byte-for-byte today's. Every module is lazy-loaded and optional.
Numbers below are STARTING numbers from public community docs, converted to our units, then tuned by playing. H = one human height (our capsule). Nothing here is read from game source; sources are fan wikis and physics guides only (list at the end; URLs cited from memory of the public pages, not re-fetched this pass: verify before quoting in-game text).

## 0. Shared architecture

One new concept: **StyleModule** = a locomotion state machine that replaces the body of the on-foot step, same capsule, same ground query (`ps.floorLocal/heightLocal/landLocal`), same camera.

```
style = {
  id, name, palette, rig(): skin recipe for ship-human,
  init(ctx), dispose(),
  step(dt, input, ctx) -> {vel, grounded, anim, camHint},   // owns vel; ctx gives floor n/height, wind, water
  actions: { primary, secondary, interact },                // F key and mouse map here
  hud(): optional chips (stamina, combo, block hotbar)
}
```
- ctx = `{up (unit, away from planet centre), floor:{r,n,slope}, wind, inWater, now, rng}`. Styles work in the LOCAL frame (up vector), never world axes, so they survive the planet-spin anchor (rev 14/18 rule).
- Switching: `/char <id>` chat command, plus a wardrobe pedestal in the 7/11 (ship-station.js). Switch = 0.4 s poof (fx.impact), resets style state, keeps inventory/hp/gorCoin. Switch is blocked in a fight, mob hub boss room, and while airborne above 3 H.
- Jetpack stays the PILOT default only. Other styles do not get a jetpack (their movement is the point). Landing back in the ship auto-reverts to PILOT.
- Net: the existing `{t:'pos', mode:'foot'}` packet gains `ch` (style id, u8) and the `st` field carries the style anim id. Receivers pick the re-skin by `ch`. Locomotion is simulated locally per player (as today); no style state is sent beyond pos/vel/anim.
- Re-skin: ship-human.js already has `setSuit/applySuit` + `genAtt`. Add `setStyle(recipe)` = palette + attachments (cube head, cap shape, tail, spikes, block limbs) built from the same parts kit. Procedural pixel-ish look: low-res (e.g. 24x32) per-part canvas texture, NearestFilter, 1-px dark outline pass (reuse the smash-sprites outline routine), flat-shaded boxes. No imported art.

Files (new): `js/char-core.js` (registry, switch, ctx), `js/char-styles.js` (the 4 headline styles), `js/char-styles2.js` (extras), `js/char-skins.js` (recipes). Hooks in ship.js: one `charStep` call replacing the foot-velocity block behind `if (style && style.id!=='pilot')`, one `ch` field in the pos packet (<15 lines total, integrator owns).

## 1. Character styles

Conversion: our on-foot run is ~1 H per 0.35 s today (see ship.js foot constants; read before tuning). Scale every style so "walk" = existing walk speed; ratios below are relative to that.

### 1.1 MARO (N64-era 3D platformer; names/art original: red-cap overall plumber-ish homage is NOT allowed, so the skin is a stout runner with a round cap, mustache-free, violet-and-orange)
Community-documented behaviour (Ukikipedia, SMW-style fan wiki; see sources S1):
| Move | Mechanic | Start numbers (ratio to single jump = 1.0) |
|---|---|---|
| Jump chain | A within ~0.4 s of landing while moving: single -> double -> triple | apex 1.0 / 1.5 / 2.6 (docs list ~42, ~52, ~69 initial speed at 4 grav; apex ~ v^2/2g) |
| Long jump | crouch + run speed + jump | low arc, 1.8x horizontal speed, apex 0.6 |
| Backflip | crouch + jump | apex 1.7, no horizontal |
| Side flip | turn hard at speed + jump | apex 1.3 |
| Wall kick | air + hit wall within 0.2 s + jump | reflect velocity off wall normal, up 1.0, resets air time; chainable |
| Dive | secondary in air | forward 1.6x, pitch down; on land = belly slide (friction low), hop-out on jump |
| Ground pound | crouch in air | 0.25 s hang, then drop 2.5x grav; 12 dmg to mobs in 2 H radius, small crater dust |
| Momentum | accel ramp, turn skid, slope slide (steep slope > 0.45 = forced slide, keeps speed) | accel 0.12 H/s^2-ish scaled; decel 2x accel; skid on 150-deg turn at > 60% speed |
Mapping: terrain slope from `floor.n . up`; walls = raycast against ship-interior/hub box walls and `ps` rocks/landmarks (use existing rock circle colliders in ship.js; if none for natural cliffs, wall-kick works on interiors, hub, village huts, landmarks only; pass 2 adds cliff probes via 4 `heightLocal` samples).
Action keys: Space jump, C/Ctrl crouch, mouse1 punch (jab-jab-kick, 6/6/10 dmg), mouse2 dive/pound. Cap-pickup collectibles = existing shards.
Feel gate: triple jump visible in a 6-s video: 3 distinct arcs + flip + dust. If it reads like the jetpack, retune.

### 1.2 JOSHI (dino-rider platformer; original look: round lizard with saddle-less nub nose, mint/orange)
(No numeric public tables of note; mechanics from general-knowledge fan docs, S2; numbers are ours.)
- Jump: apex 1.0; **flutter**: hold Space after apex, 0.9 s of near-zero fall with leg-kick anim (gravity x0.12, stamina ring drains), cancel on release.
- **Egg throw**: mouse1 = arc projectile, 3 s cooldown-free but needs 1 egg; eggs gained by swallowing mobs/creatures (tongue). Aim = camera reticle + auto-arc preview dotted line (8 points). Egg = 8 dmg, bounces 2x, hatches a small ps.creature-style pet 10% chance on creature kill (feeds existing pets).
- **Tongue**: mouse2 hold: 6 H reach, 0.25 s out/in; grabs small mobs/items/creatures (jelly, skitterer) into mouth -> 1 egg (swallow 0.4 s); can pull loot crates within reach. Pull on a wall hook (rocks) = swing (pass 4 only).
- **Ground pound**: butt-stomp like MARO but 8 dmg, stuns 1 s.
- Speed: run 1.0x, no sprint; high air control (0.9 of ground accel).
Hooks: eggs = ship-weapons projectile pool (add `kind:'egg'`), tongue = raycast against `ps.creatures()` + `space.crates` + mobs; creature kill callback in ship.js grants egg.

### 1.3 STEEV (block-world builder; original look: cube head, cube limbs, teal shirt; name is a placeholder, final name TBD, NOT any real game's default name, see legal list)
Public numbers (Minecraft wiki, S3): walk 4.317 m/s, sprint 5.612 m/s, sprint-jump ~7.1 m/s, jump 1.25 blocks (apex), gravity 0.08 blocks/tick^2 with 0.98 drag, body 1.8 m tall. Map 1 block = 0.55 H (H 1.8 m -> 1 block = 1 m would mean 1 block = 0.55 H; we scale blocks to 0.4 H for chunkier feel). Walking = 1.0x, sprint (double-tap W / Ctrl) = 1.3x, sprint-jump = 1.65x flat air momentum, apex 0.7 H (1.25 blocks at 0.4 H = 0.5 H + tune).
- **Place/mine**: mouse1 hold = mine target block (break time by hardness/tool: dirt 0.75 s, stone 1.5 s by hand-ish, tool tiers x3-x6 faster), mouse2 = place selected block. Reach 5 blocks (S3). 9-slot hotbar on 1-9 (reuse ship-items inventory stacks; blocks are item stacks `blk:<mat>`).
- **Grid**: a per-planet sparse voxel overlay: `Map<cellKey, mat>` in the planet's LOCAL frame, cell = 0.4 H, aligned to a local tangent grid per patch (anchor = landing pad normal). Only cells you place/mine exist as meshes (InstancedMesh per material, cap 4096 cells/planet, oldest-first eviction warning). Natural terrain mining: mouse1 on terrain digs a 1-cell hole by recording a negative cell that the floor query consults (`blockFloor(x,y,z)` wrapper over `ps.floorLocal`): pass 5 stretch, v1 only mines/places the overlay + loot blocks (rocks, flora, crates).
- **Crafting table**: place table block (4 planks); F on it opens the existing ship-craft.js 3x3 grid (recipes already there, add block recipes: planks, table, torch, door, bed). Torches raise local light (see 3.3) and repel mobs (no spawn within 6 cells).
- Persistence: overlay saved in the relay profile (rev 26 `profile`) per planet id, deltas only (placed/mined list, run-length). Local cache in localStorage fallback. Mobs and others see blocks only when `blocks` sync flag is on (v1: your own, owner-only; rev later: shared via relay `blk` deltas, rate 10/s).
Hook: new `js/char-blocks.js` (overlay, raycast DDA, meshing, persistence). ship-craft.js adds recipes only.

### 1.4 SONIK (momentum/speed platformer; original look: blue-less: spiky violet/gold hedgehog-NOT; make it a quilled fox-ish runner, sleek pointed visor)
Public physics guide (Sonic Retro Physics Guide, S4). Genesis numbers at 60 fps in px: acc 0.046875, dec 0.5, friction 0.046875, top speed 6, air acc 0.09375, gravity 0.21875, jump 6.5, slope factor 0.125 (applied by sin(angle)), rolling friction 0.0234375 (half), roll decel 0.125, spin dash release speed 8 + floor(charge/2) (charge 0..8 -> 8..12), charge decays x(31/32)... Convert: 1 px/frame at 60 = 60 px/s; our H = ~46 px (sprite scale) => top 6 px/f = 7.8 H/s (that is FAST; we cap run at 4 H/s without roll, 11 H/s rolling/boost).
- **Momentum core**: scalar ground speed `gsp` along the slope tangent (not a velocity vector). `gsp += slopeFactor * g * sin(slopeAngle)` each tick; accel/decel/friction as above scaled; leaves ground when `gsp` too low on steep slope (fall-off threshold 2.5 px/f, 0.5 s lock). Airborne converts gsp to world velocity; landing converts back along new tangent. This is the key difference from the capsule: ground speed follows the surface, so we need a tangent from `floor.n` and a continuous floor snap (stay glued to terrain at speed: raycast down 1.5 H, snap if within).
- **Roll/ball**: down while moving: friction halves, slopes boost; locked until `gsp` < 0.5.
- **Spin dash**: crouch + jump held to charge (revs up to 8 w/ sound pitch ramp), release = `gsp = 8 + rev/2` px/f equivalent (11 H/s). 
- **Homing attack**: in air, secondary = lock nearest mob/pet/shard within 8 H forward cone, dash 14 H/s for 0.35 s, bounce off for another jump (bounce 0.7 apex). Chain through multiple targets.
- **Loops of speed**: procedural **speed rails/loops** as props placed by the planet seed (Sonik only perceives them; others see them as ordinary decor): half-pipes and a loop-de-loop (radius 6 H) built from the parts kit as a drivable arc: while `gsp` > 6 px/f-equivalent and in contact, "sticky floor" switches the ground reference from planet normal to the loop normal. 1-2 per rocky planet, at flat spots; in the village/hub none. Falls back to flat decor if low-quality tier.
- Rings/shards analogue: gorCoin shards + HP: taking a hit drops coin (up to 10 spill, recollectable 3 s) instead of dying. Fits economy.
Hook: `js/char-sonik.js` is the only module touching floor tangent logic; reuses `ps.floorLocal` at 3 sample points for smoothed normal.

### 1.5 Rest of the smash roster as 3D styles (only where it maps; 8 more, original)
| Roster | 3D style | Core mechanic (start numbers) |
|---|---|---|
| PILOT | default | existing on-foot + jetpack |
| SWIFT | **dash** | double-tap = 0.35 s 6 H dash (i-frames 0.15), air dash x1, fast fall (grav x1.6 when down held), reflector spin on mouse2 (returns bolts) |
| HOPPER | hop | variant of JOSHI flutter minus tongue; pure floaty mobility |
| JELLY | **float** | 5 mini-jumps (apex 0.6 each), then glide drift; sleeps (mouse2 hold) = regen hp 5/s, vulnerable |
| SPARKLET | spark | tiny capsule (0.6 H), 1.4x speed, zap arc mouse1 (chain 2 mobs, 5 dmg) |
| HUM | puff | lightest: slow-fall by holding jump after 2nd (gravity x0.2), sing AoE (sleep mobs 3 s, 4 H radius) |
| RACER | race | accel curve to 1.6x, drift on turn (carries speed), ram damage 10 at speed |
| HAULER | tank | 1.4 H capsule, slow (0.7x), ground-pound quake, grab+throw mobs (4 H) |
| BUILDER | = STEEV | duplicate: STEEV is the BUILDER in 3D, keep one mechanism |
| BOUNTY/RANGER/TWIN/SEER | **skip in 3D v1** | projectile/zoner kits already are the equipped weapon; no distinct locomotion |
So 4 headline + 6 extras (SWIFT, JELLY, SPARKLET, HUM, RACER, HAULER) = 10 styles + PILOT. Extras are small data-driven variations on 3 shared state machines (`hover`, `dash`, `heavy`) in char-styles2.js: tuned by parameter tables, not new code.

## 2. Proximity fights (3D -> 2D smash -> 3D)

### 2.1 Trigger
- Every frame (throttled 4 Hz) test other foot-players (relay `pos`) and NPC fighters within **N = 6 H**. N scales: 6 H to prompt, 10 H to cancel. Both must be on foot, grounded, not in village/hub safe zones, not in mob combat (hp-hit last 5 s).
- Prompt: DOM chip "FIGHT <name>? [F]" (reuses the interact prompt). Press F = send `fx_ask {to, planet, style}` over the relay; target sees "<name> challenges you [Y/N]". 10 s timeout. Others within 12 H see "join [J]" while a match is open (join up to 4, mid-match: spawns at next respawn pad with 0% and the leaving-score of the match average).
- NPC fighters: seeded per planet 0-2 `fighter` NPCs (one per village, roamers elsewhere, appear from lingo-NPC pool, use ship-world NPC generator), each with a roster character + CPU level from smash-bot (1-3 by planet danger). They wander within 80 H, challenge you when you pass within 8 H and are not safe-zoned (once per 5 min cooldown; decline = 30 s cooldown, no penalty). Offline-capable: NPC fight = hot-seat/bot path only, no relay needed.

### 2.2 Scene handoff
1. Freeze both players' foot state: record `{planetId, localPos, up, style, hp, invent}`; mark `inFight` (invuln in 3D, others see a shimmering "ring" prop at the spot).
2. Build the stage from the terrain: sample `ps.heightLocal` along a line through both fighters, perpendicular to nothing in particular: the line = direction a->b extended +-24 H, 128 samples => height profile h[i].
   - Stage = polyline smoothed (3-tap) and quantised to 8 px steps: main platform = longest run with |slope| < 0.35 in the middle (flatten it to the median); slopes become angled ledges treated as solids with 45-deg limits in smash-core (add `slopeSolid` support; if too heavy, quantise to steps = pass 2 simplification); anything > 6 H below the median = blast pit; >= 2 floating rocks (from landmark/rocks) as thin platforms at y-56.
   - Width clamp 280-420 px. Water = hazard strip. Lava palettes = periodic ember hazard (reuse hangar hazard timing).
   - Output = same stage object as smash-stages.js (`{solids, thin, blast, spawns, hazards, layers}`), registered as stage id 'terrain'.
3. Sky/parallax: reuse the planet palette: `ps.info()`/`lookOf` -> far gradient sky (from sky dome colours), star noise by dusk factor (night -> stars + moon), planet disc of the neighbouring body, 3-4 cutouts of the same flora/rock instancing silhouettes drawn flat. Weather state adds haze/rain streak layer (ps.weather).
4. Open smash: `openSmash({stage:terrain, fighters:[{slot,chosen:style->roster id,human|cpu}], stocks:2, time:180, items:'light', returnTo:{...}})`. ship loop paused as today. Style -> fighter mapping: MARO->PILOT, JOSHI->HOPPER, STEEV->BUILDER, SONIK->SWIFT (or RACER), SWIFT/JELLY/... one to one; nothing mapped -> PILOT. Palette from the player's suit.
5. Net: **fights use the smash netcode** (sm_* rooms, host-authoritative): the challenger is host; relay room = `sm_open` with `planet:'<id>'` and a deterministic seed for the stage derived from the 128 samples (host sends the quantised sample array, 128 bytes, in `sm_lobby` so both build the identical stage; no determinism of noise across browsers needed).
6. Results -> back to 3D at the same local pos (restore pos/up/style; both spawn 2 H apart). Payout: winner +20 gorCoin (loser -10, floor 0) in a PvP (stake chosen at accept: 0/10/50); NPC fight: win +15, rep +1 (`space.reputation.dealers`/new `fighters` rep), loss -5. Draw/forfeit = 0. rep +1 per win vs NPC fighter (cap per hour 5 to stop farming). Banned in v1: item/inventory stakes.
7. Failures: relay drop -> forfeit; tab hidden > 20 s -> forfeit; if smash fails to load -> return to 3D immediately with no penalty.

Files: `js/fight-link.js` (trigger, prompts, terrain->stage, return, payouts; no sim), `js/smash-stages.js` (+ `terrainStage(profile, palette)` export; owned by stage owner), `js/smash-ui.js` (rematch? no: results only), relay `fx_*` rate-limited messages. ship.js: `fightLink.update(dt)` call in foot loop + pause hooks (<15 lines).

## 3. Mobs (block-world monsters, original looks)

### 3.1 Spawn rules
Reference (S3): hostile spawn at light level <= 7 (pre-1.18), despawn far away, zombies burn in daylight, creepers 20 hp, fuse 1.5 s, explosion power 3 (radius ~3 blocks), skeleton 20 hp, shoots every ~2 s (reduced when close), zombie 20 hp, speed 0.23 (~0.5x sprint), 3 dmg (normal) / endermen 40 hp, 7 dmg, aggro on eye contact.
Our system (local per player, no relay):
- `light(pos)` in 0..15: base = day factor from planet sun angle (1 at noon down to 0 at midnight; read from the sky dome/`sunDir . up`), +6 inside a torch radius (placed torches, 6 H), +4 near a village hut, +3 near outposts/7-11, -8 inside hub/caves (hub is always 0), -4 in storm. Light is also darkened by weather (storm -4).
- Spawn when `light <= 7` and `hp-hit` quiet 2 s. Rate: 1 mob try/s up to cap (night 6, dusk 3, storm +2); spawn at 16-40 H from the player on ground within terrain slope < 0.5, not in sight (frustum dot < 0.2 or occluded). Despawn at > 80 H or when light > 8 (fade out 1 s) except burning kinds.
- Per planet type (ps.lookOf): rocky = zomby+skelly, lush = zomby+kreeper, icy = skelly+ender, exotic = ender+all. Gas giants and stations none. Safe zones: village radius 25 H, 7-11 interior.
- Local per player (not synced): seed from `(planetId, playerId, floor(time/60))` for repeatability; others see no mobs from you (honest; stated in the HUD "private night"). Cap global mobs 12 (perf tier lowers: 12/8/6/4).

### 3.2 Kinds (parts kit, low-poly, 12-18 verts/part, flat colour)
| Mob | Look (original) | AI | Numbers |
|---|---|---|---|
| KREEPER | tall mossy green-grey pillar, 4 stubby feet, dark smiley-less X face (NOT the real face) | approach at 0.9 walk; within 3 H: hiss (0.4 s swell + flash), fuse 1.5 s; explode radius 3 H, 25 dmg center falling to 4 at edge; backs off if hit; defuses if > 7 H away | 20 hp; `ps.scar()` pooled; add `ps.crater(pos, r, depth)` = visual-only dark disc decal + 0.3 H dip in the floor query for 60 s (local overlay in the same floor wrapper as blocks; blocks within radius are removed) |
| ENDER | very tall thin black figure, violet-glow eyes, long arms | idles; if player looks at its head (dot > 0.97 within 30 H, 0.3 s hold) -> aggro: hurt-by-gaze: 2 dmg/s while stared at + stalks; teleports 8-16 H behind/around every 4 s or when hit/ranged (water hurts: 1 dmg/s; rain 0.5) | 40 hp, melee 7 dmg 0.5 s windup; drops pearl (blink item: 8 H teleport, 20 s CD) |
| ZOMBY | green-grey blocky humanoid, arms forward | slow chase 0.55 walk, spawns in groups of 2-4 (leader+followers), door-breaks village doors slowly; burns at day (light > 12) 1 dmg/s for 8 s unless in shade | 20 hp, melee 3 dmg/1 s; drops scrap x1-2 (stack), 5% coin |
| SKELLY | thin bone-pale humanoid with bolt-bow | keeps 8-14 H, strafes, shoots every 2 s (1 s if > 10 H), bolt speed 28 H/s, 4 dmg, arc gravity 0.2x; kites away if you close; shoots creepers? no, no friendly fire v1 | 20 hp, drops bolts (ammo) + bone-dust |
- Common: `damage numbers`, hit flash, knockback by weapon, melee with equipped weapon from ship-weapons (reuse), blocking reduces 50%, death poof + drops as `space.crates`-style items -> inventory stacks (existing stacking logic).
- Mobs avoid water/slope > 0.6. Pathing: steering + 3-sample slope check; no navmesh. Stuck > 3 s -> despawn.
- Performance: AI at 10 Hz, one InstancedMesh per kind, parts merged; frame budget < 0.4 ms at 12 mobs.

Files: `js/mobs.js` (spawn/light/AI/dispatcher), `js/mob-kits.js` (parts-kit meshes; one owner), drops via `ship-items.js` entries added (additive), `ps.crater` in ship-planet.js (small, ps owner).

## 4. Mob hub (dungeon crawler, one per planet)

- Entrance: a seeded landmark ("hatch") placed by `ps.landmarks()` extension kind 'hub' at a flat spot, 1 per planet (rocky/lush/icy/exotic; none on gas giants). Look: stairway down in a stone ring, violet pulse. F to enter (loading <0.5 s: generated interior).
- Interior: box rooms from the derelict/station deck pattern (ship-interior.js: `deck{walls,floorAt}`, doors, crate nodes; ship-space.js derelict builder). Generator: 5-8 rooms on a 3x3 grid graph, 12x6x12 H boxes joined by 3 H corridors, seeded by planetId + `hubLevel` (increments per clear; resets weekly via wall-clock week id). Everything unlit except torches/emissive panels; light level 0 inside so mobs always spawn from spawners.
- Content per hub: 2 wave arenas (room locks, 3 waves of 4-8 mobs from the planet's mob set, escalating hp x1/1.3/1.6), 1 corridor ambush, 1 treasure room (2-3 chests, F opens: coin 20-80 + 1 item), 1 boss room.
- Boss mob: "WARDEN" (original): 3 H tall, 300 hp, 3 phases (slam/summon 3 mobs at 50%/charge at 25%), telegraphed (0.8 s windups, red floor decal). Drop: guaranteed blueprint (`bp:<id>`, unlocks recipes in ship-craft.js, reuse the rev 26 derelict blueprint pool + 3 hub-only ones) + 100-200 coin + rare mob-drop stack. First clear per week; repeats give half.
- Combat: on-foot with the equipped weapon + melee from your style (so MARO jumps/pounds, SONIK homes, STEEV places a block to stall, JOSHI eats/throws eggs). No jetpack inside (disabled for non-PILOT anyway; PILOT jetpack off in hubs).
- Death: lose 10% of carried coin (min 0), respawn at hub entrance, hub resets that room only.
- Net: **hubs are local** (private instance, per player). Optional co-op (v2): relay room `hub_<planet>_<partyId>`, others see you via normal pos packets; mobs remain host-local = no mob sync in v1. State "hub co-op: not in v1".
- Save: `hubLevel`, `cleared[planetId]=weekId` in the profile.
Files: `js/hub.js` (generator + runtime), reuses ship-interior.js builder (additive export `buildBoxRoom(spec)`), `js/mob-kits.js` adds WARDEN. ship.js: enter/exit hook + 'hub' mode flag (the integrator, <25 lines).

## 5. Villages (1 per lush/rocky planet)

- Placement: seeded flat site (slope < 0.18 over 40 H radius, not water, away from outposts) from `ps.landmarks()` generator, kind 'village'. Present from orbit as a tiny light cluster at dusk.
- Procedural huts: 5-9 boxes (roof prism, door slot, window emissive, torch pole) from the parts kit, 3 styles by palette (stone/clay/wood-tone), a well (cylinder), a farm patch (instanced crop rows), a bell/post. Total < 40 draw items merged.
- Villagers: 4-8 humanoid NPCs from ship-world NPC generator + `ship-lingo.js` (per-seed lexicon + sentence recombination: dialogue works already: `npc._n` untouched, replay-stable). Roles = shop/trade (farmer sells food, smith sells blades/recipes, librarian sells blueprints). Daily schedule: day = walk between huts and farm; dusk = inside, doors closed; night = sleep (bed props).
- Trades: F on villager -> trade panel (reuse the planet-store UI): buy/sell with coin; trade table seeded per villager (3 offers: item <-> coin or item <-> item). Rep affects price (`space.reputation.dealers`: +rep => -10%/ -20%). Discoveries: each village adds a map POI (`ps.pois()`).
- Safety: 25 H radius = no hostile spawn, mobs aggro-leash back; zombies raid on a new moon event rarely (1 per 10 min at night) -> village defence quest through ship-quests.js: "defend" objective with 2 helper NPC fighters.
- Fighters: 1 NPC fighter is always in each village (challenge target).
- Net: **villages are shared world** (static seeded) but NPC positions are local-seeded schedule (time-based on wall-clock, deterministic) so all clients see roughly the same people with no packets.
Files: `js/village.js` (placement, huts, NPC routines, trades), `ship-lingo.js` (additive trade-line templates), `ship-quests.js` (defend template), `ship-planet.js` (landmark kind registration, small).

## 6. Legal line

- Mechanics, physics ratios, move categories, game-feel concepts: yes (not copyrightable; numbers from fan wikis are facts).
- NOT used: any name, art, sprites, models, sounds, music, level layouts, voice lines, dialogue, logos, exact character silhouettes/face layouts, decompiled/leaked source, ROM-extracted data. No code lifted; all implemented from scratch.
- Looks: original silhouettes only (e.g. Kreeper-like pillar must not use the real face or colours; Ender is generic tall-black-shadow but violet eyes and different proportions; Sonik must not be a hedgehog or blue). When in doubt, change the shape.
- Names in code, comments, UI, filenames, alt text, chat: ours (MARO/JOSHI/STEEV/SONIK are working names only: **we should rename before public**, because those four are too close to real marks; pick e.g. RUNT / DINO / CUBE / DASH by pass 1).
- Pre-ship grep (case-insensitive, whole tree incl. docs, comments, JSON, favicon): 
  `nintendo|mario|luigi|yoshi|bowser|peach|zelda|link\b|hyrule|pokemon|pikachu|kirby|metroid|samus|donkey|smash bros|super smash|sega|sonic|tails|knuckles|eggman|robotnik|mojang|minecraft|creeper|enderman|steve\b|herobrian|notch|zombie pigman|nether|redstone|villager trade hall|gotta go fast|chaos emerald|power star|1-up|goomba|koopa|oneshot`
  Then check any of: `ring\b` (allowed only in "ringed planet"), `rings` HUD wording, `1UP`, `coin` (allowed: gorCoin). Run `scripts/ip-grep.sh` (NEW, pass 1) as a CI-less manual gate and fail the deploy on a hit.
- Credit: S1-S4 sources are mechanic references only; list in `docs/credits.md` ("mechanics researched from public community wikis"), not in-game.

## 7. Build order (5 passes, each deployable, file ownership disjoint)

| Pass | Deliverable | Owner files (new unless noted) |
|---|---|---|
| 1 | Char core + MARO + STEEV placeholders (no blocks) + skins + `/char` + wardrobe pedestal + net `ch` + ip-grep script + rename working names | char-core.js, char-styles.js (MARO), char-skins.js, scripts/ip-grep.sh; ship.js (integrator <15 lines), ship-human.js (`setStyle`, human owner), ship-net.js (`ch` field) |
| 2 | Mobs: light model, spawn, KREEPER/ZOMBY/SKELLY/ENDER, drops, crater, HUD "night"; JOSHI + SONIK + 6 extras | mobs.js, mob-kits.js, ship-planet.js (`ps.crater`, light read, small), char-styles.js (JOSHI/SONIK), char-styles2.js, ship-items.js (additive) |
| 3 | Fight link: trigger, terrain stage, smash handoff + return, NPC fighters, payouts; requires smash passes 1-3 shipped | fight-link.js, smash-stages.js (`terrainStage`), smash.js (integrator), server relay `fx_*`, ship-world.js (NPC fighter kind) |
| 4 | Mob hub + WARDEN + chests + blueprints; Sonik loops, Joshi tongue swing | hub.js, ship-interior.js (`buildBoxRoom`), ship-craft.js (blueprints), mob-kits.js (WARDEN), char-sonik.js |
| 5 | Villages + STEEV blocks (overlay, crafting table, persistence), co-op hub optional, polish/audio/balance, join-mid-match | village.js, char-blocks.js, ship-lingo.js, ship-quests.js, ship-audio.js (additive bank), smash-ui.js |

Rules per pass: one owner per file; shared files (ship.js, ship-planet.js, relay) get only the integrator's tiny hook patches, serialized; every pass ends with node-run self tests (sim parts pure, no DOM), Chrome visual check, ip-grep clean, docs updated, Pi==local N/A (web only, Netlify deploy).

## 8. Risks

- **Scope is huge**: styles x mobs x hub x villages x fights = 5 products. Cut order if time: STEEV blocks -> villages -> extras -> hub co-op. Never cut ip-grep, never cut fight handoff correctness.
- **Wall-kick / sonik loops** need collision beyond the ground height query; v1 colliders are interiors/hub/village/landmarks only. Be honest in HUD (no promise on natural cliffs).
- **Terrain to stage** can look arbitrary; flatten the middle always.
- **Feel**: all numbers are starting guesses, tuned by playing (Emory at the keyboard, 6-s clip check per style).
- **Night mobs** may annoy; add `/peaceful` toggle (no spawn) from day one.
- **Sonic physics guide** values are px at 60 fps: always re-derive in H/s before copying; do not place 7.8 H/s default run.
- **Blocks persistence** could blow profile size: cap 4096 deltas, RLE, drop oldest with warning.

## Sources (public fan docs; no decomp, mechanics only; URLs from memory of public pages, re-check before quoting)
- S1 Ukikipedia (SM64 mechanics wiki): https://ukikipedia.net/wiki/ (pages: Triple_Jump, Long_Jump, Wall_Kick, Dive, Ground_Pound, Slope, Movement). Fan-documented jump initial speeds ~42/52/69, gravity 4.
- S2 Yoshi mechanics overview (fan wiki pages "Flutter Jump", "Egg Throw", "Tongue"): https://www.mariowiki.com/Flutter_Jump , https://www.mariowiki.com/Yoshi (mechanics only; no numbers copied).
- S3 Minecraft Wiki (mechanics): https://minecraft.wiki/w/Walking , /w/Sprinting , /w/Jumping , /w/Creeper , /w/Enderman , /w/Zombie , /w/Skeleton , /w/Spawn , /w/Light , /w/Breaking , /w/Explosion
- S4 Sonic Physics Guide (Sonic Retro): https://info.sonicretro.org/Sonic_Physics_Guide ; also /Spin_Dash and /Sonic_the_Hedgehog_(16-bit)/Physics (constants).
- S5 Melee-era fighter frame/knockback concepts are already in docs/smash-plan.md; extra: https://www.ssbwiki.com/Knockback and https://www.ssbwiki.com/Frame_data (public site, mechanics only).
