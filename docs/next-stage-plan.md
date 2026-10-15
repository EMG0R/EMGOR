# NO MANS GOR — next stage plan (2026-10-14)

Plan only. Nothing here is built. Base = main 01346b4 (docs/HANDOFF.md top section, ship-mode.md rev 33).
Order recommended: A (POKEMANS pass 1) → C+D (MARO faithfulness + distinct bodies) → B (one fractal level) → E (backlog sweep). One builder per pass, commit per item.

## A. POKEMANS — buy balls at the 7/11, catch, train, battle

What exists: `ps.creatures()` / `ps.setCreatureTamed` / `ps.setFollower` (ship-planet.js ~2301), taming with fries (ship.js ~6888, profile.taken), creature generators in ship-enemies.js (parts kit), 7/11 shelves + cart + checkout + gorCoin (ship-world.js, ship.js rev 28), 78 foods × 96 infusions (ship-items.js), smash fight engine `createFightSession` (smash-world.js) now wired in-world (ship.js `FG`), lingo syllables (ship-lingo.js), relay disc.claim. mashup-plan.md §POKEMANS (2026-10-07) is the seed of this; it changes below in two places: battles ride the smash engine (not a new turn-based overlay) and upgrading is a graft system (not fixed evolutions).

### A1. Species generator (pure, seeded)
- species seed = hash(planet seed, creature kind). Per species: name (lingo syllables, 2-3), type pair from biome palette (lush→leaf, icy→frost, gas→vapor, rocky→stone, lava→ember, sea→brine) + one infusion family, base stats {hp, atk, def, spd, sp} in 20..120 drawn by tier, body recipe = the existing creature parts kit + 0-2 signature parts (fins/spikes/glow/shell/horns/tail).
- individual = species + iv roll (±15 %) + size 0.8-1.3 + shiny 1/256 (hue flip).
- move table: 4 moves from a procedural pool (name = type word + verb, power 30-120, accuracy 70-100, effect ∈ burn/slow/shield/heal/flinch/launch/drain). Type chart: 8 types, each beats 2, weak to 2, generated once per universe seed, shown in the dex.
- deterministic, no Math.random: everything from the seed so two clients agree (relay).

### A2. Balls at the 7/11 + catching
- 7/11 shelf item "GOR BALL" tiers 1-3 (gorCoin 15/60/200), stack item, hotbar. Burger House sells "LURE FRIES" (creature approaches + sits 8 s).
- Throw = existing projectile pool (kind 'ball', arc, 12 H reach, reticle preview dots). Hit a creature: catch chance = ballTier × (1 - hp %)·0.9 + 0.1; creatures lose hp from foot weapons/melee first (they already have hp in ship-enemies). Shake 3× (0.5 s each) then click or burst.
- caught → inventory stack item kind 'pokemans' with full data; species name claimed on first catch (disc.claim 'creature'); creature hidden via setCreatureTamed.
- cap: 6 in the party, rest in a PEN in the ship interior (ship-interior.js pens, already a concept).

### A3. Party + follower + commands
- ONE active follows you (setFollower path / createPet follower on foot), rides in the ship. Hotbar slot shows it; number key swaps active (0.4 s poof).
- It fights beside you against mobs/hub waves with a tiny AI using its 4 moves (cooldowns = 1/spd). T-hold on it = 4-move radial menu (T is talk now; reuse).
- XP from fights/catches; level 1-50; stats grow by base × level curve.
- HEALING (Emory 2026-10-15): NO potions/upgrade consumables. A pokemans that SURVIVES a battle is auto-healed to full for the next one. One that FAINTS is DEAD until revived at a POKEMANS CVS: a new structure (pharmacy look, red/white, same build pattern as the 7/11, seeded 1 per lush/rocky planet near the first outpost + one in the station) with a REVIVE counter (gorCoin, price by level) and a pointless but real HEAL counter (free, for the vibe). Fainted ones stay in the party as grey slots, cannot be summoned.

### A4. Upgrading, better than Pokémon ("graft" system)
- No fixed evolution lines. The BODY IS THE STATS: every signature part is a stat. Harvest parts from defeated/caught creatures and resources (existing harvesting), graft at a village ELDER (role exists) or a 7/11 "GRAFT STATION": fins +spd, spikes +atk, shell +def, glow +sp, extra lungs +hp. Each graft changes the mesh (parts kit add) and the silhouette, so a trained one looks unique.
- Infusions (the 96) feed to the pokemans: permanent +type affinity or a move re-roll; foods teach moves (food type → move type), so cooking is the move tutor. No stat potions: stats only move by grafts, level and bond.
- Bond meter from time following + wins; bond thresholds unlock a 5th "signature" move and a crit chance. Trades with other players over the relay (pl.ev) keep bond at 0.
- Size and shiny carry through. Dex page per species (discoveries UI later).

### A5. Battles — ride the smash engine, one mechanism
- A pokemans battle is a smash fight where the fighter spec is generated from the pokemans: body = its parts, 4 attack slots = its 4 moves (power→damage, type→vfx colour, effect→status), speed → movement, hp → stocks/percent. `createFightSession` already takes fighters by spec; add a `creatureFighter(pokemans)` builder.
- vs wild: catch instead of KO is allowed from inside the ring (throw ball = a move slot while a wild is weakened).
- vs NPC TRAINERS: villager role 'trainer' (new, additive) with 1-3 pokemans; F = fight (same prompt as NPC fights). Win → coin + a part drop. Elders are trainers with grafted teams.
- PvP: same session over the relay (smash net already wired, unverified) — later.
- Fallback option if the smash mapping reads badly in play: a fast turn-based card overlay (chat-font) using the same move data. Decide after one playtest, do not build both.

### A6. Persistence + net
- profile.pokemans[] (full data, ~300 B each), profile.party[], profile.pen[]; pose save already covers the follower spot.
- Species names and dex are global via disc.claim; nothing else needs the relay in pass 1.

Pass 1 = A1 + A2 + A3 (+ A6). Pass 2 = A4 + A5 trainers. Pass 3 = PvP + dex UI.

## B. Fractal scale (Fractal Block World / Ant-Man) — "as you get smaller there are more planets"

Fractal Block World's trick: every block contains a full world; the engine REBASES units when you shrink so the inner world is rendered at normal scale, and the outer world is only a skybox/parent transform. Infinite depth works because nothing is ever actually tiny: only the camera transition is.

What we already have that makes this cheap: the galaxy tree is recursive (root → planets → moons → moonlets, `drawOrder` parent-first, `engine.makeBody`/`disposeBody`), the pilot scale blend (`setPilotBlend`), the floating render origin, the log depth buffer, and `ps` is one reusable surface module keyed by a node + seed.

### B1. One level deep (prototype)
- On foot, every ROCK / landmark / "odd boulder" (ps rocks exist, 300 instanced) is an entry. Hold Z on a rock while crouched (or a 7/11 item "SHRINK FRIES") → 2 s transition: camera dives into the rock, fov pulls, the current planet's patch fades to a sky dome texture.
- Under the hood: `engine.makeBody` creates a MICRO SYSTEM: one new planet node (seed = hash(parent seed, rock index), R = normal pilot radius) plus 0-3 moons, parented to the current planet node at the rock's local position, flagged `micro:true`, hidden from the galaxy view. The ship, player and ps are moved to it with a UNIT REBASE: L stays L, the micro system is placed far from the parent (its own floating-origin island), the parent planet becomes a static skybox dome (its colours + horizon) around the micro system. Nothing is small.
- Shrink depth counter on the HUD ("DEPTH 1"). Zoom out = fly to the micro system's 3 R edge → transition back, rock re-appears under you.
- Content scaling by depth: resources rarer and higher tier (depth = loot tier), creature species get a depth suffix type, villages smaller and stranger, gravity lighter (g × 0.85^depth), day shorter. POKEMANS depth variants = the "legendaries".
- Persistence: profile.depthStack = [{planetId, rockIndex}] so pose restore rebuilds the chain; micro nodes are disposed when you climb out.

### B2. Infinite depth
- Same mechanism recursively: micro planets have rocks. Seeds chain. Keep at most 2 levels of parent domes (anything further is a static colour).
- Net: ghosts carry depthStack; players at different depths are invisible to each other; the relay room key gains the depth path.
- Discovery: deepest depth per player on the leaderboard (E).

### B3. Risks
- Floating origin + log depth already handle the scale; the danger is the parent dome parallax reading wrong. Test: a 60 s walk at depth 1, dome must not shift.
- Tests: g18 shrink → walk 20 s → climb out → same rock, pose within 0.5 L, drawOrder restored, no leaked nodes.

## C. MARO = Mario 64 mechanics, exact

Legal line: the sm64 decompilation is Nintendo's copyrighted code reverse-engineered; do NOT copy code or assets. The documented constants and state machine (Ukikipedia / community tables, already the source "S1" in ship-styles.js) are facts and are fine. Names and looks stay ours (MARO, stout runner, violet/orange).

Conversion (Mario 160 units tall = 1 H; 30 frames/s): speed u/f → H/s = ×30/160 = ×0.1875; accel u/f² → H/s² = ×900/160 = ×5.625.
| mechanic | SM64 | ours |
|---|---|---|
| gravity | -4 u/f² (air), hold A = -4 until apex then -4 | 22.5 H/s² |
| single / double / triple jump v0 | 42 / 52 / 69 u/f | 7.9 / 9.75 / 12.9 H/s (apex 1.38 / 2.1 / 3.7 H) |
| jump chain window | land → A within 5 f while fwd speed ≥ 1 | 0.17 s |
| walk accel / max run | +1.1 u/f² to 32 u/f (analog) | 6.2 H/s² to 6.0 H/s |
| skid | turn > 90° at speed > 16 u/f → skid, decel 2 u/f² | 11 H/s² |
| long jump | Z+A at speed, fwd ×1.5 (max 48 u/f), v0 30 | 9.0 H/s fwd, 5.6 up |
| backflip | Z (crouch still) + A, v0 62 | 11.6 H/s up, −0 fwd |
| side flip | turn ≥ 160° + A, v0 62, fwd 8 | 11.6 up |
| wall kick | hit wall airborne, A within 5 f, v0 52, reflect fwd 24 | 0.17 s window, 9.75 up, 4.5 H/s out |
| dive | B airborne / B at speed, fwd +? to 48 cap, v0 10..20 | 9 H/s fwd, belly slide friction 0.98/f → 0.55/s |
| ground pound | Z airborne: 10 f hang then −10 u/f gravity×?, landing 1.9 u radius hitbox | 0.33 s hang, fall 56 H/s², 3 H stomp radius |
| ledge grab | approach a ledge within 30 u below, speed < 40 | 0.19 H, auto-grab, up = hop |
| slide (steep > 38°) | forced slide, accel by slope | slope > 0.62 |
| bonk | hit wall at > 16 u/f not jumping | 3 H/s, 0.5 s stun |
| punch chain | B: jab, jab, kick (A/B frames) | 6/6/10 dmg, cancel windows 0.27 s |
- Implement as the complete SM64 action table in `maro()` (ship-styles.js, pure maths): states idle/walk/run/skid/crouch/crawl/jump1/2/3/long/back/side/wallkick/dive/slide/pound/ledge/bonk, with the frame windows above converted to seconds. The controller must report `wallNormal` on every tangential hit (today it only sets wallHit when every slide fails — audit A12 — which is why the wall kick never fires; fix in the capsule collider first).
- Camera: Lakitu-style lag follow (yaw lags 0.3 s, pitch auto-levels), no C-buttons needed.
- Feel gate: the triple-jump video test from mashup-plan §1.1; plus long jump covers 9 H, wall kick fires on any wall ≥ 1 H.

## D. Characters look totally different (not a hat on the same guy)

Today: ship-human.js parts kit + `setSuit/applySuit/genAtt`; styles = palette + attachments (`SKINS` in ship-styles.js). Plan: a RIG RECIPE layer in ship-human (one mechanism, additive):
- `rig = { headShape: cube|round|snout, headScale, torso: stout|slim|lizard|box, limbLen: {arm, leg}, limbShape: cube|cyl, feet: big|normal|none, tail?: {len, spikes}, quills?: n, cap?: shape, eyeStyle, palette }`. Proportions drive bone placement and hitbox (`hitbox.r/h` per style already exists).
- Per style: MARO stout, big round head + cap brim, big shoes, short arms. JOSHI quadruped-ish lizard: long tail, snout, saddle bump, stub arms, hops on two legs. STEEV all cubes, square head twice the width, straight limbs, no neck. SONIK quills (5 instanced cones), long legs, huge shoes, tiny torso. PILOT = today's suit.
- Animation sets per style: idle/run/jump curves are style-owned (`out.anim` already carries rig state); add a per-style gait table (stride length, bounce, arm swing, lean). MARO run leans forward with arms back; STEEV arms stiff; SONIK legs blur (3 ghost frames); JOSHI hop.
- Net: `ch` field already carries style id → receivers build the same rig. Portraits in inventory/character tab from the same recipe.
- Wardrobe pedestal in the 7/11 (mashup-plan) becomes the switch UI; `/char` stays.

## E. Backlog — everything asked that is not in main yet

From HANDOFF ledger, audit (2026-10-13), playtests, and this session:
1. Ship proportion ~4-5 H long (H = 0.09 L hard-coded in ~90 places: make H a derived constant once, then set the ratio).
2. Fight balance: an idle player beat an elder bot; CPU difficulty tiers by role, win condition check, 3 s "F again" window review.
3. Two-device multiplayer test; redeploy the relay on gorcave (`server/nmg-relay/server.js`, planet-local poses) — needs the Pi; A1 of the audit falls back to world poses until then.
4. Tests: g10 boardBig (rev 25 prompt change), g13 boss spacing 913 vs 916.5 (formation constant or test tolerance), g15 order-sensitivity (null mob `tz.w`), g4 teleportFrames environmental flake, g14 steep site.
5. HUD declutter: 9 guidance markers → nearest 3 + the ship; hint bar fades after 10 s; boss HUD clutter re-playtest.
6. Landing prompt: `F · LAND` (now Z) needs speed < 2 and no auto-brake; add auto-damp inside 3 L so the prompt appears while cruising; onboarding step 3 depends on it; teach roll/flip/drift/E/hold-Z.
7. Village scale decision (1x now vs the 2.5x asked earlier); hut interiors with furniture; villager pathing around huts; golum pathfinding.
8. Remaining 9 mashup styles; SONIK loops (curved colliders); STEEV right-click placement race; MARO wall kick (C above).
9. Hub mobs on planet-host netcode; `/smash` arcade cabinet in the station 7/11.
10. Far rendering part 2: smaller planets ({4,3}) need the approach constants (PILOT_CLEAR0, moon orbit floors) re-derived so moons never embed; hi-res globe perf on a weak GPU.
11. Mobile/touch controls; photo mode + share link; discoveries UI (dex lives here); leaderboard on the site (deepest depth, catches, wins).
12. Error swallowing → rate-limited warnOnce on gameplay paths (audit A9); per-frame allocation pass (villageLife); quality hold while hovering low.
13. ship.js module split (9.6k lines) + the banned-name pre-push grep.
14. Documents checkout: `git pull` once macOS access is back; the watch-approve hook re-add in ~/.claude/settings.json.
15. Playtest after villages: re-run docs/playtest-1/2 punch lists in person.

## Build order + cost guess (one Sonnet builder each, commits per item)
1. A1-A3 POKEMANS pass 1 (species gen, balls at 7/11, catch, follower, XP) — biggest feel win per token.
2. C + D together (MARO table + rig recipes for the 4 styles) — one builder, styles file + ship-human.
3. B1 fractal depth 1 prototype + g18.
4. A4-A5 graft + trainer battles on the smash engine.
5. E sweep in two halves (gameplay items 1-9, then platform items 10-15).
