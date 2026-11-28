# changes-spine (2026-11-27)

One spine: walk, fly, interior, night mobs, subplanets. Verified by node tests + headless-Chrome scenarios, NOT by eye.

## 1. Cuts (git numstat, added/removed)
| file | +/- | what |
|---|---|---|
| js/ship-styles.js | 7 / 717 | MARO/JOSHI/STEEV/SONIK + 8 variant styles, maroTable, STYLES, createStyle, skinFor, reflect/regen/throw maths. KEPT (imported by unowned ship-foot.js / ship-human.js): normRig, rigParts, GAITS (PILOT only), wallNormal2, jetStep/mkJetSM/JET_K/jetThrust. Full original: `js/_attic/ship-styles.full.js` |
| js/ship.js | 202 / 655 (net) | style system (list/select/cycle/portrait/ability text), STEEV blocks (BK*, bkTarget, blkFloor, bkPlace/Break/Explode), style step/fx/egg/grab/throw/reflect/regen, tongueGrab, `profile.blk/crew/board/rep`, crew recruit + missions + mission board + crew pods menu, convoys (HUD markers, hail, piracy, escorts, crates, blockade event), derelicts (dkSite/dkTrigger/dkOnDock/dkLoot/dkTick/dkCollide, guardian, `R27.dsite`), char-tab style arrows + keys |
| js/ship-space.js | 4 / 293 | convoys, derelicts, crates, reputation, debugNear (742 -> 453 lines). Ambient freighters + mining kept |
| js/ship-interior.js | 66 / 99 | crew pods (setCrew, crewModels), mission board screen + canvas. Right wall is now 4 decor bunk bays, cockpit-side console has no screen |
| js/ship-station.js | 8 / 28 | station mission board screen, `interior.board`, `interior.missions()` |
| js/ship-craft.js | 3 / 7 | crew-pod-kit, pod-refined, captain-suite, crew-ration recipes; `pods` upgrade slot |
| js/ship-net.js | 5 / 11 | style in hi/pos messages, setStyle, ghost re-skin |
| tests/node | 7 files deleted, 3 rewritten | deleted maro-table, maro-table2, styles-fuzz, styles-variants, styles-q46, style-abilities, style-hitbox, rig-parts (-585 lines). rig.test trimmed to PILOT + rigParts; jet-input rewritten for the single walker; pure-ship slot list lost `pods` |

Net count: tests 445 -> 329 pass / 0 fail (all 116 removed tests exercised cut features only).
Also fixed: `hpMax` undefined in a heal hook (now HP_MAX). Kept as inert stubs because ship-foot.js still calls them: `styleStep` (returns null), `styleFx`, `styEggTick`, `styleSync`, `styU/styUpW/styWallO/styFov`, `curStyle` (null), `stylesMod` (trimmed module, for wallNormal2). `ship.mashup` debug name kept (night mobs live there).

## 2. Interior toggle design
- F in flight with nothing else to do (F had no flight binding): `enterInteriorFlight()` -> gmode `ifoot`, `IXF.on`. Prompt `F · GO INSIDE` (dim, only when no LAND/DOCK prompt).
- Autopilot (`ixFlightTick`, runs each frame before `interiorStep`): ship velocity vector and quaternion held, no input, no damage (the flight step, enemies and bolts do not run; combatRoot hidden). Planet guard: per body, the allowed inward speed is `min(sqrt(2 * 30 L * gap), gap / dt)` where gap = distance - (terrain floor if inside 1.25 R of the active planet, else 1.15 R hard shell) - 3 L, so it stops exactly 3 ship lengths short and slides tangentially. `ps.update` keeps terrain streaming.
- Interior hull rides the ship: group pose = ship pose with the cockpit window centre half a ship length ahead of the hull centre, so the window shows the live sky/planets. Player spawns at `interior.spawn` (aft), facing down the hall.
- F inside the new `interior.cockpit` zone (label `F · BACK TO FLYING`) -> `leaveInteriorFlight()`: gmode `fly`, velocity/heading as the autopilot left them, `throttle`/`speed` re-seeded so the flight model keeps chasing that speed, camera snapped to the chase pose. Hatch (Z) is disabled while in flight. One 0.25 s black fade each way (switch is instant under it).
- Landed/on-planet entry via the hatch is unchanged. Pens unchanged. The active POKEDEX POKEMANS is `interior.companion` (pet rig, follows the player through the hall; no longer occupies a pen).
- Save/resume while inside in flight resumes flying where the autopilot has the ship. `leaveInterior()` routes to the flight exit when `IXF.on`.
- Debug: `EMGOR_SHIP.rev25.enterInteriorFlight() / leaveInteriorFlight() / ixf`.

## 3. Night mobs (`ship-enemies.js` MOB_KINDS, 7; new kinds appended so snapshot indexes hold; MOB_CAP stays 12)
| kind | H | speed H/s | hp | tier | behaviour | looks |
|---|---|---|---|---|---|---|
| zomby | 1.0 | 1.2 | 20 | 1 | pack chaser (2-4), burns in day | rocky lush exotic |
| skelly | 1.0 | 2.2 | 20 | 1 | ranged archer, strafes | rocky icy exotic |
| kreeper | 0.94 | 1.8 | 20 | 1 | charging fuse bomb | lush exotic |
| ender | 1.6 | 3.2 | 40 | 2 | teleporter, angered by being looked at | icy exotic |
| brute (new) | 2.3 | 0.7 | 90 | 3 | slow heavy, visible 0.9 s windup, 14 dmg | rocky icy exotic |
| lurker (new) | 0.83 | 2.4, x4.2 pounce | 28 | 2 | ambusher: spawns only within 12 op-units of a structure, motionless until you are < 7 H (or shot), pounces 1.1 s, re-hides beyond 30 H | rocky lush exotic |
| skitter (new) | 0.86 | 4.6 | 14 | 1 | fast skittish: rings you at 7-13 H, dashes in every ~3 s for one bite, then flees; flees when hit | lush icy exotic |
All die to blaster damage (`mobHurt`), pay `rewardKill(tier)` (gorCoin + party XP), drop items (new drops reuse goo / bone / gor coin / warp pearl). Distinct body per kind (rolled from boxes: boulder hulk, six-legged shell, long-legged dog-bird).

## 4. Tests run
- node: `bash tools/test-node.sh` 329/329. `node --check` + `acorn --ecma2022 --module` clean on every edited file; no-undef lint clean except 5 pre-existing (`shopSell`, `sessionStorage`, `MouseEvent`).
- browser scenario `js/ship-interior-tests.js`: `run()` (18 checks, pass), `runMobs()` (12 checks, pass).
- g10/g17/g24: see report.

## NEEDS (files I do not own)
- js/ship-foot.js: still imports/calls the style bridge (`styleSync/styleStep/styleFx/styEggTick/styU/styUpW/styWallO`, `S.curStyle`, `S.stylesMod`, `sty ? jetThrust : 1`, `!!sty` args). Collapse to the single walker, then delete the stubs in ship.js (`// Spine cut` block, `styleStep` etc.) and `jetThrust`/`styled` from ship-styles.js.
- js/ship-human.js: comments naming STEEV/ship-styles recipes; `setRig` styled hitbox paths only used by styles.
- js/ship-planet-tests.js: g15 (mashup on foot) and the gF1/gF2/gF3 feel tests (style list MARO/JOSHI/STEEV/SONIK/SWIFT) call `mu.setStyle`, `mu.place`, `mu.BK`, `mu.cellAhead`, `mu.tongue`, `ship.mashup.style`; they will fail. Any test using `rev27.hail`, `rev27.dkSite`, `space.convoys/derelicts/crates`, `R27.dsite`, `rev25.fastForward/assign/hire`, `pf.board/crew` is dead. Harness sets `rev25.noInterior = true`; my scenario flips it back.
- js/ship-world.js: Burger House `candidate` / `recruit(visits)` crew candidate generator, `bhr.candidate` (nothing reads it now).
- js/ship-lingo.js: `recruit` line key and crew/mission lines in `mission` topic (crew + board references); safe to leave, they are text only.
- js/smash-moves.js, smash-world.js: MARO/JOSHI/STEEV/SONIK are also GOR BRAWL fighters (separate system, left alone).
- js/ship-village.js line ~17: comment "STEEV placement refuses".
- style/ship.css: `.sh-cvm` convoy marker rules unused.
- server/nmg-relay/server.js: still stores `style` and `crew` fields from `hi` (clients no longer send them; harmless).
- docs/mashup-plan.md: describes cut movement styles and blocks; should be marked historical.
- tools/banned-names.baseline: lines for `ship-styles.js` removed content may now be stale (guard only flags NEW hits).

## By-eye checklist
1. Fly, press F: black flash fades in 0.25 s, you stand at the aft hatch facing the hall, cockpit window shows moving stars/planets, ship keeps drifting.
2. Aim at a planet at speed, go inside, wait: the planet stops growing, ship halts about 3 ship lengths off the terrain.
3. Walk up the ramp to the pilot seats, `F · BACK TO FLYING`: flash, chase camera, same speed, no jolt.
4. Landed: Z at the hatch still enters/exits the interior; pens work; the active POKEDEX POKEMANS follows you through the hall.
5. Hall right side (bunk bays) and cockpit left console look finished without crew/board.
6. Inventory CHAR tab: shows PILOT text only, no arrows; Tab cycles inv/char/dex.
7. Night on a rocky/lush/icy planet: see all 7 silhouettes over a session; brute reads huge and slow; lurker sits by a house until you get close; skitter circles and darts; shoot each dead, coins + XP tick.
8. Station dock / launch still works (derelict branches removed from the dock path).
9. Two tabs (relay): ghosts show the plain rig, no console errors on hi/pos.

## Manager follow-up (2026-11-27, autopilot brake)
- Bug found by the interior scenario: the autopilot braked on world-space velocity and on the LOCAL floor radius. Planets spin, so the terrain radius under a parked ship swung 1.013-1.071 R at one world point (measured on a 12.9k L planet); a ship held 3 L over the floor would be hit by rotating relief, and an orbiting body could drift into a stopped ship.
- Fix (ship.js ixFlightTick): hold reference = the hard collision sphere (HARD_F 1.15 R) + 3 L; brake on the radial velocity RELATIVE to each body (per-body velocity tracked as lfBegin does). Entered while already inside the hold radius (low atmosphere): climb out at min(0.5 * deficit, 60 L/s) and stop exactly at the hold radius; the climb is per body and never touches a player's own outward coast.
- ship-interior-tests.js: planet case split in two (coast-in stops at 3.00 L outside the hard sphere, at rest relative to the planet; low entry climbs out and holds, max 60 L/s); waits for the lazy ship-interior.js import; forces flight first (a resumed profile starts on foot). 23/23 in Chrome.
