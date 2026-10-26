# NO MANS GOR — HANDOFF (2026-10-13)

## 2026-10-14 surface/villages/fight pass (branch `surface-vibe`, from main a31f994, NOT merged, NOT verified by Emory in person)
Shipped in code and driven in headless Chrome (g1-g17 batches below). Nothing seen by eye.
1. **Mounted structures** - measured first: village mesh matrices at DRAW time and `hum.w` match the spun planet pose to ~1e-12 L (g17), so the render path was not the slide. Real causes found: sites without a terrain pad (village/hub arch on uneven curved ground; 36 m huts had up to 0.1 H floor-vs-ground error) and wind drifting an idle player 0.3-0.47 L/s. Fix: `ps.setSitePad(slot, dir, flatL, blendL)` (ship-planet.js; 5 shader pad slots = 3 outposts + village + hub, same flatten in hFrac, patch shader, globe shader), registered by `sitePads()` in ship.js `sitesTick` at 3 R; wind only pushes you while moving and outside any pad ring (`ps.inSite`). Test g17.
2. **Villages** - 6-10 villagers always out by day (roles keep robe colours), huts at human scale (VS=BIG=1: door 2.2 m, wall 3.1 m + roof, cottage 6x5 m), greet within 3 H. Existing pitched roofs, door gaps, well, paths, torches, fences, farm plots kept. 7/11 + Burger House doors: unchanged (not re-verified).
3. **Surface feel** - jump 1.16 H in 0.53 s (G_JUMP 4.36, up-gravity 7.93, fall gravity 16 H/s2, measured), Shift dive v = v0*2^(t/0.6) cap 48 H/s from the speed you have, landing camera dip, crouch/dust/thump kept. NOT done: ship ~4-5 H long (ship is ~11 H; H = 0.09 L is hard-coded in ~90 places across ship.js/ship-human/ship-enemies/ship-world, needs its own pass); mashup styles keep their own jump (`styleEnv`).
4. **Blaster** - no LMB->thrust path exists in code (grepped ship.js, galaxy3d); a quick click (down+up inside one frame) dropped the shot, now latched 0.12 s so every click fires; hold fires at the weapon rate; Shift+LMB ram unchanged.
5. **Keys** - Z = ship (land, board, step out of hatch, launch, lift off / summon by HOLD); F = shop/shelf/harvest/tame/hub + FIGHT on a human; T = talk on foot; X = wingmen focus fire. Prompts, hint bar, HELP, onboarding updated. Tests use Z.
6. **Fights** - F on villager/shopper/clerk/dealer -> "FIGHT? F AGAIN" (3 s) -> `createFightSession` local vs CPU bot (style by role; elders/golum level 3), terrain-sampled stage, 2 stocks, overlay over the 3D page, controls WASD/Space/J K L I (LMB = J, RMB = K), Esc forfeits. Win: +25/50/100 gorCoin (villager/shopper/clerk, trader/dealer, elder/golum), seeded "good game" line (ship-lingo `goodgame`, `taunt`), 60 s rematch cooldown (10 s after a loss). Foot controller + ship keys are held by `cmdOpen = true` during the fight. A clerk standing at a counter resolves to the shop, not a fight.
7. **Menu colours** - found: adaptive-quality tier (bloom off, pixel ratio 1, 40 % stars) stayed applied on the galaxy page and kept re-evaluating while away; the active planet's `uSeaLevel` stayed at the land-balance value after exit. `RS0` snapshot at ship.js init, `rsRestore()` on `finishExit` and when the quality manager attaches; mismatches are logged `[ship] render restore (...)`. Quality manager holds while not piloting; `ps.release()` leaves the active planet.
Test it: `const t=await import('./js/ship-planet-tests.js'); await t.runAll(EMGOR_GALAXY.engine, EMGOR_SHIP, {only:['g17']})`. By hand: land on any rocky/lush planet, Z to board/step out (F no longer does), walk to the green village column, stand still 20 s (no drift), walk through a hut door, F on a villager twice to fight, hold Space then Shift in the air, leave to the galaxy menu from a planet surface and compare bloom/colours.
Unverified: everything by eye; 7/11 + Burger House door walk-through; fight feel/balance (an idle player beat an elder bot in the one headless run, so win conditions need a human look); hint bar width; relay not involved.
Key map: Z ship, F shop/fight, T talk, X wingmen focus fire, E inventory, V scan, Q focus, Esc forfeit a fight.

## 2026-10-14 structures/summon/surface pass (branch `structures-vibe`, NOT merged, NOT verified by Emory)
Shipped in code, syntax-checked (node --check + acorn). Browser: g1-g16 run once on the {4,3} build: g1 g3-g8 pass, g2 g9-g16 fail (embedding, see Rev 32); scale reverted to {6,4}, rerun did not complete — RE-RUN runAll FIRST. Nothing seen in person.
1. **Structures** — outposts 2.5x (`ship-planet.js OUT_S`: pad, flat ground, tower, beacon, NPC spot; `ship-world.js` 7/11 + Burger House via `ps.outScale`,
   NPC humans counter-scaled `hs = 1/OS`, walk speeds too; collision unit = `interior.unit` = KS). Village 2.5x (`ship-village.js VS`: huts, doors, well,
   stall, plaza, paths, torches; villagers/golums unchanged). Hub arch 2.5x (`ship-hub.js ARCH`, dungeon unchanged). Placement: first outpost = flattest
   land inside 0.15 rad of `ps.siteDir` (planet->ship at activation, remembered per planet in localStorage `nmg-site-<id>`); village searched in the same
   cap first (`villageSite`, exported, predicted at 3 R by `sitesTick` in ship.js and reused at build). Sky beacon columns: `ps-cols`, ONE instanced draw
   (outposts at the tower, village, hub). HUD markers with distance for all of them inside 3 R.
   Test: fly toward any rocky/lush planet; at <3 R orange (first outpost) / purple (others) / green (village, dungeon) columns stand on the near side;
   markers show distance. `EMGOR_SHIP` debug: `ps.columns()` count. LIMIT: siteDir is per client (multiplayer peers can see different first-outpost / village spots).
2. **Ship summon** — on foot away from the hatch, hold F 1 s (`SUMMON SHIP` ring). Search <=2 s for land, strut slope < 0.45, not in a store / Burger House /
   village wall, 2.5-6 L from you; ship lifts, flies (<=8 s), lands with leg deploy + settle. >500 L or nothing found: set down directly. Ship marker (cyan,
   distance) always on the foot HUD (same marker system). Test: walk 50 L away, hold F, watch it land, tap F at the hatch.
3. **Surface** — landing + landed settle on the max `meshFloorLocal` under 3 struts (`strutFloor`); exitShip feet on the rendered ground under the hatch;
   `ps.update` now runs BEFORE the foot step (dome re-synced after). Foot sounds `step / jump / footLand` (ship-audio.js). Jetpack (Emory mid-pass ask):
   climb ramps 1x->3.5x over 1.2 s of Space, Shift in the air dives (16 H/s), FLIGHT mode Shift alone pulls down, landing crouch + dust + thump.
   Test: land on rough ground, step out: feet on the drawn ground, ship struts touching.
4. **Far planet rendering** — measured (see ship-mode Rev 32): globe was 48x34 (3168 tris) / 32x22 / 20x14 spheres with NO relief, terrain up to ~0.08 R.
   Now the ACTIVE planet's globe swaps to 192x128 (48,768 tris; 128x88 at quality <2) with `GLOBE_VERT` = same height function as the patch, relief eases in
   3.0->2.6 R, patch uRel = 1 (no flat-silhouette fake). PILOT_PLANET_SCALE stays {6,4}: {4,3} failed g2/g9/g11/g16 (embedded moons) — reverted.
   Test: approach from 3 R; mountains on the limb, no pop at the patch fade (2.4-1.8 R), no globe poking through the patch.
5. **Finding things** — in flight: station + nearest 7/11 world + nearest village world anywhere (edge arrows); inside 3 R exact markers. On foot: NPC dots
   (clerks, shoppers, dealers, villagers, creatures) within 200 L, on-screen only, distance; V lists them in chat + tags them. Markers now update AFTER the step
   (`pilotFrame`), composed with the synced planet pose.
6. **Audit** — A1: landed/foot poses also sent planet-local (`frame:'planet'`, pid, lx..lqw); receivers compose with their own planet pose
   (`planetToWorld`). Relay `server.js` passes the fields — **relay NOT redeployed to gorcave** (old relay drops them; clients fall back to world poses).
   A2: planetFail disposes + rebuilds after 1 s (max 3), on-foot player boards the ship, floors guard `ps === null`.
**Regression fix (same day, second pass)** — two regressions vs main found by the harness, both fixed:
- **Stranded on foot** (g9 left `gmode 'foot'`, `land.node` = supercollider R 20.4, feet at r 702.6 = archive's floor; g9/g10/g11 then "cannot approach", g16 "no planet").
  Trace (gmode-write log): g9 lands on supercollider normally (`startLanding` <- `ship.land`, `touchdown`, `exitShip` <- `onKeyF`, ps.active = supercollider);
  no key changed gmode. Root cause: audit A4 moved `ps.update` BEFORE the foot step and fed it `hum.w` (last frame's smoothed feet), and `exitShip` never wrote
  `hum.w`, so the first foot frame passed ps the PREVIOUS walk's spot (generative-pieces). ps re-activated on the parent (archive), `footStep`'s `mFloor` read
  archive's floor (702) in supercollider's frame, the feet jumped there, and every later frame confirmed archive. Not a moonlet issue (supercollider is
  > 60 L). The broken pose was also SAVED (profile pose on pagehide) and restored on the next load. Fix (`ship.js`): `groundPos()` feeds ps.update this
  frame's feet / parked hull composed on `land.node`; `exitShip` refuses unless `ps.active === land.node` and seeds `hum.w`; `applyResume` (foot) seeds `hum.w`;
  the foot step runs only while `ps.active === land.node`, else `boardShip()` (a restored stranded pose self-heals: verified from the bad saved pose).
- **843 shader errors** (`'color' : undeclared identifier` at `vC = color`, `vFragDepth does not match`): the test-only `ps.idPass` (g9 see-through check)
  cloned each mesh's vertexShader without `vertexColors`/`defines`, and gave built-in materials three's default vertex (no logdepth varyings).
  Fix (`ship-planet.js` idPass): ShaderMaterials keep defines + vertexColors; others get `idVert` (begin/project_vertex + logdepthbuf). Gameplay never hit it.
- Tests after the fix (one run, `{only:['g2','g9','g10','g11','g16']}`, 598.5 s, starting from the stranded saved pose): g2 PASS (20 trials, endRatioMax 1.347,
  maxCross 40 u/s, speedAt2s 12, hpLoss 0); g9 PASS (260 poses, 15718 px, 0 bad, camUnder 0, minCam 0.168 L); g10 FAIL only on `boardBig:false` at all 3
  sites (pre-existing on main; landed/foot/walked 20 L/reboarded/flying all true); g11 PASS (entry 0.0243 L, leaving 0.165 L, hpLoss 0); g16 PASS (1250
  frames, drift 0, nan 0). Console: 0 errors, 0 shader errors for the whole run.
Unverified: everything in person, two-window test of A1, perf of the hi-res globe on a Chromebook, store interior feel at 2.5x (shelves are 2.5x taller).

For a fresh Fable session taking over. Read this, then `docs/ship-mode.md` (the full spec history,
revisions 1–30), `docs/mashup-plan.md`, `docs/smash-plan.md`, `docs/multiplayer-plan.md`,
`docs/playtest-1.md`, `docs/playtest-2.md`. Memory rules: `~/EMGOR_SKILLS/_EMORY.md` is loaded
every session; the token-preservation rule is in `memory/emory/token-preservation-mode.md`.

## Pre-push guard (Q07)
`tools/banned-names.sh` greps js/ + docs/ for third-party character/game names and exits 1 on any NEW hit. Existing mentions (comments, Emory's quoted asks) are grandfathered in `tools/banned-names.baseline` (file + line text). Enable once per clone: `git config core.hooksPath .githooks` (pre-push runs the script). After deliberately adding an allowed mention, regenerate with `tools/banned-names.sh --baseline` and review the diff; or put `banned-ok` on the line. Skipped files: the handoff/queue/log/mashup-plan docs that must name the rule.

## What this is
A No Man's Sky style game living inside emgor.online's Three.js galaxy (the portfolio site).
Click the ship on the home page → same scene becomes the game. ~30k lines across `js/ship*.js`,
`js/smash-*.js`, engine hooks in `js/galaxy3d.js`, a WebSocket relay in `server/nmg-relay/`.
Deployed by Netlify from `main` (github EMG0R/EMGOR). Relay runs on gorcave (Pi 4) as
`nmg-relay.service`, port 8797, Funnel path `wss://gorcave.taila85593.ts.net/nmg`.

## Live right now (commit 7d930cf)
Flight (weighted mouse, roll dodge, flip, drift, pulse ramp, boost), 6× planets with a floating
render origin, planets FROZEN in orbit while piloting (fixed orbital time T0), landing (F / hold F
to step out), on-foot capsule controller (walk 22 H/s, run 44, jetpack, hold-Space 2 s = FLIGHT
mode), solid terrain with 15 guarantee tests (`js/ship-planet-tests.js` runAll g1–g15),
atmosphere entry, land/water balance, biomes, landmarks, weather (wind pushes you), creatures
(herds/skitterers/floaters, tame with fries → pets), resources + harvesting, INTERGALACTIC 7/11
(correct look, 2.5× interior, shelves you grab from, cop + shoplifting heat, cart/checkout, clerk,
shoppers, dealers outside), Burger House (fries = blue glow 30/45/60 real min), gorCoin economy,
stacking inventory in an ACCURATE Minecraft layout (E; player preview; armor = suit tiers; hotbar
on HUD; CHARACTER tab switches movement styles), 3×3 crafting (99 recipes incl. blueprint-locked),
items (78 foods × 96 infusions, vision effects via engine.vision, daily specials), lingo (alien
words learned by buying/talking), branching NPC conversations with moods, quests from any NPC
(deliver/bounty/scan/retrieve/escort/harvest) + HUD tracker, onboarding for new profiles, your
SHIP IS YOUR BASE (interior: crafting table, kitchen, 4 pet pens, 4 crew pods, mission board,
cargo crates, trophy wall, observation window), crew recruits at Burger House → real-time
missions, combat (squads w/ leader, wind-up→strike→stall, heat ring, ram tool, lock-on evasion,
enemy roles hunter/harasser/bomber, back shields, abstract bosses with limbs you sever, planet-
throwing titans, formation spawns, damage numbers), loot (weapon crates, shards), weapons w/ mods,
ship hull kinds (hauler/fighter/explorer) + visible upgrades, station near the black hole (auto-
dock, deck, 7/11, shipyard, bounties board, trade terminal, map pedestal), asteroid belts (cover,
mining → chunks), freighter convoys (hail/trade/attack → crates, reputation), derelicts (dock,
loot, guardian), black hole solid + gravity, sound (procedural WebAudio: sfx, engine, ambience
beds, Lydian music, role voice blips), quality manager (`/quality`), relay (ghost ships, foot
humans, glow, hull signature, style, chat, `/gorcave` neptr bot via the Pi's Claude CLI with zero
tools, profiles across devices `/user NAME KEY`, discoveries, events, fight rooms `sm.*`, planet
host election `pl.*`), movement STYLES on foot (MARO/JOSHI/STEEV/SONIK — working names; mechanics
from public docs, our art), STEEV collidable persistent blocks, NIGHT MOBS (kreeper/ender/zomby/
skelly — working names) with planet-host netcode, GOR BRAWL (`/smash.html`): deterministic 2D
platform fighter, 13 original fighters mimicking the classic cast's movesets, bots, hot-seat,
gamepad, finals meter, NO items; `js/smash-world.js` runs fights in-world (host/guest verified).

## State at handoff (2026-10-13, HEAD 8945b3c, tree clean, all pushed → Netlify)
Session 2026-10-07..13 (Fable main window, one Sonnet builder at a time). Commits, newest first:
- 8945b3c **Ship pose persists across sessions.** `pf.pose {v:1, t, mode:'space'|'planet', ...}` built from the
  Esc-resume snapshot; planet mode stores planet id + sub-mode (fly/landed/foot) + local dir + radius offset +
  facing (+ human pose on foot). Saved on exit, pagehide, visibilitychange-hidden, every 10 s while piloting.
  Restored on the first `enter()` per page load; missing planet / any error → dock spawn. `/respawn` or
  `EMGOR_SHIP.pose.clear()` forgets it. Space-mode browser-verified (distance 0 after reload). Planet/foot
  restore NOT tested. Limit: a space pose is a fixed world point; planets have orbited on by the next session.
- 5fa6378 **Landing fix (unconfirmed by Emory).** Descent re-targets `land.rg` from `floorLocal` every frame;
  landed + foot re-settle on `floorLocal` every frame; `ship-perf.js` quality manager gets `hold()` and ship.js
  passes `hold: gmode !== 'fly'` (no tier changes while landing/landed/foot/interior/hub); hub + village now build
  only in landed/foot, one per frame (village first, hub next), never during 'landing'.
- 66726b7 hub/village per-frame hooks in try/catch; village solids only on the active planet; village 1.6x (`BIG`).
- f00bafd **Villages** (`js/ship-village.js`, wiring in ship.js, lingo roles additive): one per lush/rocky planet,
  flattest of 220 seeded directions ≥0.3 rad from outposts; 5–9 huts (3 templates), well+bell, market stall, farm
  plots, fences, torches, path ribbons; 2 merged meshes (day mesh dims at night). Walls `solid:true,
  indestructible:true`, STEEV placement/ray ignore them. Villagers farmer/trader/elder/kid, wall-clock schedule,
  inside at night, greet ≤3 H, humans created lazily near the player. Trader = 6 seeded trades via the existing
  trade UI (stock per UTC day). Elder opens the quests UI. Golums 1–2 per village, 4.5 m, 600 HP, no loot, hunt
  mobs in the village radius and anyone who hits a villager (`village.onHit`, `golum.target`), melee via mobHurt.
  House glyph scan marker + guide POI. Profile `pf.vill`. Hook `EMGOR_SHIP.village` {info, goto, night, talk,
  trade, hit, make}. Limits: no pathfinding (golum can stall on a hut wall); villagers unkillable; mobs don't
  target golums; all village/golum fights are local per client; village exists only after you land.
- 6f9e024 **Hub wired.** Per-planet hub built lazily, seeded by planet id (`hubSpot` deterministic), rides in
  `world.group.parent`. F at the arch enters (`F · ENTER`); F in the start room / walking to z<1.4 exits. Skull
  scan marker "DUNGEON". Hub-local foot controller `hubFootStep` (walls, `floorAt`, wall-clamped cam). Arena
  `lock` → waves from `hub.waves()` via the mob system; last wave clears + unlocks. Boss = `generateGroundEnemy`
  tier 4 ×3, summons at 50 %, drops coin + blueprint. Chests via F (LOCKED until cleared). `pf.hubs[planetId]`.
  Ambience uses the 'interior' bed. Hook `EMGOR_SHIP.hub` {enter, exit, info, clear, goto, open, face, raw,
  state}. Limits: hub mobs are LOCAL-ONLY (excluded from pl.* snapshots); boss slam/charge phases not built;
  ender can't teleport inside; forced exit mid-fight rebuilds the hub.

### The landing bug (RESOLVED 2026-10-13, awaiting Emory's live confirmation)
Root cause: commit 094f8c3 made the engine render planet spin on a LAGGED clock (`spinTime = time - spinLag`, lag grows 0.75 s per second of piloting)
but ship.js `syncPlanet` / `lfOmega` still used `engine.time` and the full `spinRate`. The ship (landing, landed, foot, local-frame flight, floor samples)
was placed against a planet rotated by engine.time, then updateBodies re-rotated the drawn planet to spinTime: angle error = spinLag * spinRate, unbounded.
Ship hovered/buried by the terrain-height difference between two longitudes and slid over the drawn ground at 0.75 * spinRate.
Fix: engine exposes `spinTime` and `spinRateMul` (= d spinTime/dt); ship.js uses both. Guarantee test g16 (rendered-frame parity): old code drift 1919 L, fixed 0.
Why nobody reproduced it: the step() harness clock barely advances, so spinLag was about 0; the old measurements below (floorAt moving at a fixed world position, -21 L after touchdown) were this drift.
Old notes kept for history: 5fa6378 (re-settle every frame, hold quality) was a symptom patch and stays harmless.


### Not started this session
- In-world FIGHT (spec below unchanged). A builder was launched and stopped before editing; nothing on disk.
- Bug sweep: STEEV right-click placement 0 (input race), MARO wall kick never fires, g14 flaky, duplicate
  pl.host on enter, banned-name pre-push grep script, efficiency pass (per-frame allocs, dead code, draw calls).
- Not verified by Emory in person: hub, villages, golums, 1.6x scale, marker correctness, NPC walk-up F (builder
  only verified via hooks; villagers were inside huts during its test), pose restore on a planet.

### Session rules that were added
- One Sonnet builder at a time (he sets the count); no experimenting/playing in the browser: read diff + console
  first, one repro + one confirm, no screenshots. He was explicit about token spend.
- `.claude/settings.local.json` now has `defaultMode: bypassPermissions` + blanket allows (his request).
- Known-good comparison worktree pattern: `git worktree add --detach <tmp> 938ddd6`, serve with a tiny
  no-store http.server on another port (the `/private/tmp/claude-501/nocache_server.py` script was gone; a copy
  is at `~/.claude/jobs/4e9aa8fc/tmp/nocache.py <dir> <port>`). Remove the worktree when done
  (`git worktree prune`).

## State at the 2026-10-07 handoff (historical)
- `js/ship-hub.js` — mob hub dungeon per planet (11 rooms, 3 arenas, WARDEN boss, chests w/
  blueprints). Built + screenshot-verified. COMMITTED but NOT wired into ship.js.
- Villages: NOT STARTED (the agent was stopped before editing). Spec below.
- In-world FIGHT prompt: NOT STARTED. Findings below.

## Emory's standing rules for this project (read `_EMORY.md` too)
- "Lock it in" = stop asking, build. One question at a time with a recommendation.
- Short replies. He doesn't read long text. Lead with status.
- Builders are Sonnet subagents; main window plans/specs/deploys/smokes. He now caps at TWO
  builders at a time ("token preservation mode"), sometimes says go to 5–6 then decay to 3.
  Never haiku. Disjoint file ownership per agent; one owner for `js/ship.js` at a time.
- Every pass: parse with `node --check` AND `npx --yes acorn --ecma2022 --module js/ship.js`
  (node --check once missed a brace swallowed by a comment). Keep files runnable between edits:
  he play-tests from the local no-cache server on :8123 (start:
  `python3 /private/tmp/claude-501/nocache_server.py "$PWD"`; recreate that tiny script if gone).
- Chrome MCP tabs are background-throttled: drive frames with `window.EMGOR_GALAXY.step(0.016)`;
  pointer lock is refused in automation; `/user claude` is the orange test profile.
- Push often (he beta-tests live). `git pull --rebase --autostash` first: another session
  (emgor-fa) commits papers/overlay hunks to `js/galaxy3d.js` with partial staging.
- Relay deploy: `rsync -az --exclude node_modules --exclude sandbox --exclude data server/nmg-relay/
  server@gorcave.lan:~/nmg-relay/ && ssh server@gorcave.lan 'sudo systemctl restart nmg-relay'`.
  Unit needs ProtectHome=false, MemoryMax=400M, CPUQuota=150% (the Claude CLI child). No API key
  ever — he refuses raw API costs; `/gorcave` uses the Pi's Claude login.
- Multiplayer is a REQUIREMENT: every new system ships with a two-window test (local relay:
  `node server/nmg-relay/server.js` with NMG_DATA=/tmp/nmgdata ALLOW_NO_ORIGIN=1 on 8796; pages on
  127.0.0.1 auto-use it).
- No items in Brawl. In-world fight stage = terrain where the fighters stood.
- Names MARO/JOSHI/STEEV/SONIK/kreeper/ender/zomby/skelly are working names he chose; mechanics
  from public docs only, never decomp code, never third-party art/sounds/names. There is a
  banned-name grep idea in docs/mashup-plan.md; make it a real pre-push script.
- Villages (NEW, his latest ask): houses NOT destroyable (no block breaking on hut walls);
  village guardians like iron golems ("golum" working name): big slow protectors that fight mobs
  and anyone who hits villagers; villagers with homes, day/night routines, trader, elder quests.
- Symbols not words on HUD markers. Minecraft inventory must stay pixel-accurate.

## Architecture cheat sheet
- `js/galaxy3d.js`: site galaxy + "ship mode port" (`window.EMGOR_GALAXY.engine`: setPilot,
  setPilotBlend, renderedRadius, makeBody, nudgeBody, blackHole, vision pass, floating origin,
  pilot planet scale ×6/×4 + 320 L floor, orbit freeze T0, setClock, onFrame, setBloom, …).
- `js/ship.js` (~600 KB): the game. States: docked/piloting/away; gmode fly/landed/foot/
  interior/docked; local-frame sim inside 1.6 R of a planet; big debug API `window.EMGOR_SHIP`
  (cmd, enter/exit, rev25/rev27/rev28/mashup/quests hooks, stepN).
- Modules (one owner each): planet (terrain/atmo/outposts/weather/creatures/landmarks/pois),
  world (7/11, Burger House, shelves/cop, resources, villages WIP), station, space (belts/mining/
  convoys/derelicts), interior (base), hub (dungeon), enemies (generators: creatures, bosses, ground,
  mobs), parts (procedural kit), hull, human (rig/suit/pets), styles (locomotion state machines),
  items/craft/quests/lingo/icons/weapons, fx, audio, perf, net, smash-*.
- Keybinds: E inventory, F interact/land (hold = land+exit), Q focus, Z allies, V scan, T target,
  Enter/`/` chat, Space pulse/jetpack, Shift boost/run, A/D×2 roll, S×2 flip, Ctrl drift, Esc hold
  3 s = exit to site. Commands: /wave /peaceful /hostile /difficulty /user /color /ship /weapons
  /volume /quality /help (+ /gorcave, /smash alias planned).

## Next steps (priority)
0. CONFIRM LANDING with Emory on the live site (see 'The landing bug'). If still broken, get real evidence
   first (console + altitude log over a real F-hold), do not guess again.
1. In-world FIGHT + two-window tests (spec below); hub mobs onto planet-host netcode; golum pathfinding.
2. Brawl pass 3/4: real relay fights in production (two devices), results → gorCoin/rep, audio/
   polish; `/smash` cabinet in the station 7/11.
3. Mashup pass: more styles (the rest of the 13), wall-kick on cliffs, SONIK loops (needs
   curved colliders), style-specific abilities in fights.
4. Roadmap tiers in docs/ship-mode.md (discoveries UI, photo mode, mobile touch, onboarding v2).
5. Known flakies: g14 parked-drift (wind disabled in test, still flaky 50 %), STEEV right-click
   placement intermittent, MARO wall kick never fires, duplicate pl.host on enter (harmless).

## Wrap-up findings from the stopped agents

### Villages (js/ship-world.js + js/ship-lingo.js) — to build from scratch
Spec: one village per lush/rocky planet, seeded on flat land >= 0.3 R from the first outpost;
5-9 huts (3 templates: boxes + roofs + door gap), a well, path decals, torches at night, fences;
`world.village = { pos, huts:[{box, door}], villagers:[{human, name, role: farmer|trader|elder|
kid|guard, pos, home, lines, trades?}], bell:{pos}, walls }`; villagers walk hut->well->paths by
day, go home at night (ps night value), greet within 3 H via lingo; trader = 6 seeded trades
{give, get}; elder = quest hook; lingo roles farmer/trader/elder/kid/guard added (additions only);
<= +4 draw calls (instanced huts/fences/torches), humans only within 40 L; in scanTargets + pois
(kind 'village'). EMORY ADDS: hut walls are INDESTRUCTIBLE (mark `solid:true, indestructible:true`
so STEEV block-breaking ignores them); replace the plain guard with GOLUMS (working name,
iron-golem style): big slow protectors (parts kit, ~2.5 human heights) that fight mobs and anyone
who hits a villager; expose `guard.target` and `world.village.onHit(villagerId, attackerId)`.
Where in ship-world.js: createWorld ~line 765, attach/site picking ~789, scanTargets ~973,
converse/npcMood ~1116.

### In-world FIGHT (js/ship.js + js/ship-net.js) — to build from scratch
- ship-net.js already routes pl.host/pl.snap/pl.ev to hooks.onPl (onMsg switch ~line 386) and
  exposes plEnter/plLeave/plSnap/plEv. NO sm.* handling yet: add send helpers for sm.challenge/
  accept/join/leave/in/snap/end + an onMsg case forwarding sm.challenge/start/member/no/in/snap/end
  to a hook, plus `onMessage(fn)`/`offMessage(fn)` on the net object (createFightSession calls
  both).
- Relay: `t` must be the FIRST key in sm.* frames; sm.start carries {room, host, members, spec,
  seed} only — no terrain samples — so both clients sample the same 128 heights deterministically
  from the two fighter positions (send positions in the challenge/accept payload or read ghosts).
- smash-world.js: createFightSession({mode, seed, stageSamples Float32Array(128), skyPalette,
  fighters:[{slot, styleId, name, color, isLocal, isBot, id}], net:{send,onMessage,offMessage},
  roomId, stocks, onSfx}); use session.el (overlay, pointer-events none, z 60), setInput(mask),
  update(dt), onResult(fn), renderBillboard(session) for the spectator sprite.
- Flow: on foot within 6 H of another player (or an NPC with fighter:true, 30 % of shoppers/
  dealers) -> FIGHT glyph; F = sm.challenge (NPC = local mode w/ bot); on sm.start pause the foot
  controller, build the session, 3D behind at low alpha; nearby players spectate or see the
  billboard; result -> gorCoin +-50, rep, chat, back to foot at the same spot; Esc = forfeit.
- Also fix: STEEV right-click placement intermittently 0 (input race), MARO wall kick never fires
  (wallNormal timing), g14 flaky (first-site visual / parked drift).
- Two-window test: local relay `NMG_DATA=/tmp/nmgdata ALLOW_NO_ORIGIN=1 node server/nmg-relay/
  server.js` (8796; 127.0.0.1 pages auto-use it); A and B on the same planet: same mobs, each
  other's style, challenge/accept/fight/results, host handoff when A leaves.

---

## EMORY'S ASKS — complete ledger, his words, with status
Legend: ✅ done and verified · 🟡 built but partially verified / needs tuning by him · ❌ NOT done
(or only planned). Anything ❌ or 🟡 is open work. Quotes are his dictation (typos kept).

### Core concept
- ✅ "a space ship you can click on and when you do, it transforms the site into a full web game
  level game. completely based on the space ship mechanics / controls of no mans sky"
- ✅ "fly around my galaxy", "when you hit escape it will toggle in and out of the ship based on
  wherever you are" (Esc resumes the exact pose in the planet frame)
- ✅ "share the exact mechanics of the main site with the game part", "no time in between… site
  to load immediately" (lazy module load on idle; phones gated out)
- ✅ "if it detects ur on a phone it wont load it" — ❌ "let's get mac version working first" means
  a mobile/touch version is still OWED later (Tier 4).
- ❌ "procedurally generate as you fly away so there is no edge" — he later said "screw the
  procedurally generated thing for now"; bounded space with a soft edge. Revisit only if asked.
- ✅ "could we do the landing on planets too" — yes, done (planets, gas giants as cloud worlds).
- ✅ "refer to the name as no mans gor"

### Flight feel
- ✅ "mouse movement has latency… I don't want" → then "too light, split the difference… more
  weight" → then "exponential curve to the weigth of the ship more like how it was originally
  just also faster than no mans sky" (weighted mouse + bank, current MOUSE_WEIGHT 5.5)
- ✅ "ship much more detailed and much smaller relative to the plannets" (hauler replica from his
  photos; now L = refR/12000 with ×6 pilot planet scale)
- ✅ "scaling is wrong… orders of magnitude bigger… match no mans sky", "10x scale of plannets",
  "overall scale not just landing" (several passes; floating origin)
- ✅ "space bar start a lot faster", "longer u hold space… exponentially faster", "max speed 150%
  with the space bar then shift is a 50% boost", "make the ship top speed 2x faster"
- ✅ "when im in reverse i need space and left shift to go that fast in the other direction"
- ✅ "I don't like the bloom… when I go faster… a lot more subtle… Star Wars style… stars trailing"
- ✅ "fire animation coming out of the back… space propulsion", "blasters… more depth"
- ✅ "shaking when u get hit, and slightly less every time the blaster fires"
- ✅ "vibration from entering and exiting atmosphere should be like 15% as intense"
- ✅ "my shots are not affecting the bosses", "blaster should have infinite range", "equally
  fast away from me" (bolts inherit ship velocity, swept hits)
- ✅ "stars rotate along with all page rotation… realistic" (3D star sky)
- ✅ "particles coming off the black hole… far more subtle, and stars way in background more
  prominent"
- ✅ "black hole needs to be way bigger / not flythroughable in flight mode"
- ✅ "planets are orbitting so fast… when you enter the game the orbit slows down to not moving…
  rotation is secretly an illusion" (orbits frozen at T0 while piloting)
- ✅ "ensure that the ship does not apear offscreen before it is clicked… prime spot but not
  conflicting w the plannets", ✅ "when i spin the univers around… the ship does not move with
  it, ensure it does" (world-anchored dock, glides back if hidden)
- ✅ "ship before u click on it is half the size, less detail and stays farther away"
- ✅ "ship that you fly INTO… zooms into it and the perspective shifts"

### Combat
- ✅ "enemies to start appearing and it gets harder and harder and when u die it restarts.
  first enemy is easy to avoid, health regens… second is significantly harder, then chill"
- ✅ "difficulty of wave12 should be wave3" (ramp ×4); ✅ "enemies fly away super fast after
  enough hits so they cannot be killed head on" (FLEE)
- ✅ "enemies… alien hybrid alive ships", ✅ "bigger and bigger… giant alien monsters w health
  bars", ✅ "titans… push plannets around", ✅ "throw plannets towards you… die instantly"
- ✅ "bosses… static… I can fly around them", "when i fly away from them they stay same distance"
  → later "way more static… don't approach me" → later "formation and slowly get closer"
  (current: spawn 90–140 L ahead, close slowly, melee reach)
- ✅ "bosses… swing arms / tentacles around more than shooting, then the other ships it spawns
  shoot u" (limb capsules + escorts)
- ✅ "more shaking less red when you get hit"
- ✅ "bosses more obscure / abstract and scary" (wraith/monolith/maw/hive/seraph/eclipse/choir)
- ✅ "if i fly directly into an enemy it should do massive damage"
- ✅ "enemies dive bomb you", "never stays in same place… to sides… trying to evade you",
  "auto focus… when you are pretty close to pointing at them"
- ✅ "allies… flying in front of me across my vision", "allies are beating all the enemies, make
  their ships weaker and not shoot until i do"
- ✅ "enemies… way bigger and slower especially at first", "way more health, make them even bigger"
- ✅ "look into no mans sky space combat criticisms", "what makes the combat fun" (docs/combat-*.md
  → squads, stall, heat, ram, roles, shields, orbs, graze, focus, limb sever, loot)
- ✅ "combat is so boring i need the game to be more fun" (rev 20–21); 🟡 he has not confirmed
  it's fun yet — FEEL TUNING BY HIM IS OPEN.

### Planets / landing / on foot
- ✅ "guaranteed entrance bellow atmosphere", "lot of space in the air", "big interesting,
  guaranteed STABLE surfaces" (rev 18 guarantees g1–g9)
- ✅ "planets are hollow", "transition from the atmosphere… ground being deeper"
- ✅ "ground kept being transparent. Make sure you can never see through the ground"
- ✅ "some plannets i land and it is all like water… if it only has 1 then it is all land"
- ✅ "once i get in atmosphere it gets super hazy / weird", "landmasses look weird"
- ✅ "no phantom sun… the shadow of other planets and the black hole is there"
- ✅ "no gas giants every plannet should be landable"
- ✅ "I didn't see an option to get out of the ship" → ✅ "same button as no mans sky… get out of
  your ship and can run around" → keybind resolved: E = inventory (Minecraft), F = land/exit/
  board/interact ("if those r the same button prioritize Minecraft menu")
- ✅ "humanoid character get out and walk around, jump run etc"
- ✅ "jetpack that has infinite time and is fast and make the run 2x faster"
- ✅ "the character when it get's out of ship jitters a lot and would walk through ground"
- ✅ "player normal walk the run speed, then run is double speed"
- ✅ "jetpack goes up but also when you move forward orientation changes… like piloting a ship…
  hold space for 2 seconds" (FLIGHT mode)
- ✅ "arrow pointing like no mans sky guidance to the burger house and 7/11"; ✅ "arrows show
  symbols not words to declutter"
- ✅ "remove all the like entering orbit, cant land bc of water titles… keep the boss name stuff"
- ✅ "beauty that is procedurally generated", "burning / atmosphere entry effect", "land button
  like in no mans sky" (hold F)
- ✅ "trees / creatures moving around… none of it loads… until u get close enough… consistent w
  how the planet looks from above" (same shader noise)
- 🟡 "enemies… chase them through the plannets / into atmosphere and back out" (coded FLEE
  dives to planets; not visually confirmed by him)

### Economy / stores / items
- ✅ "7/11 stores on every plannet with the NPCs", "intergalactic 7/11 is it's name"
- ✅ "7/11 look cartoonish accurate like aisles… wider… super high cielings… jetpack on top of
  isles and npcs walking around", ✅ later "7/11 and burger house need correct coloring / shapes…
  bigger scale interior" (redone rev 29)
- ✅ "drug dealers and just like funny characters all procedurally generated text from basic
  sentence recombination not ai vibe low processing"
- ✅ "crack infused gardettos or fent big gulp… procedural generated combination of drug infused
  7-11 foods", ✅ "scroll menu for items"
- ✅ "hit q [→ E] and it opens infinte menu… as many slots as items player has… minecraft style",
  ✅ "right click on a food you can eat it then your vision is blurred / affected… 15s to 4
  minutes", ✅ "each item needs a pixely actual drawn representation"
- ✅ "minecraft inventory… match exactly to the minecraft aesthetic… hitting escape escapes that
  menu, u have to hold escape for 3s to escape back to main website"
- ✅ "the minecraft menu (which needs to look and feel more accurate to the minecraft menu check
  that shi) make a section to see and change ur character"
- ✅ "burger house… dallas burger house… only serve fries. fries make you glow light blue for
  45 real minutes"
- ✅ "money, call it gorCoin… NPCs will mention gorCoin"
- ✅ "CRAFTING… 3x3 crafting table… buy flux capacitors etc… combining machine parts and fent
  burger style stuff… infinite crafting of things you craft into other things… into weapons /
  ship upgrades"
- ✅ "procedurally generated weapon generation" (generator + shop + mods + bolt shapes)
- ✅ "/user (yournamehere) so users can pick their ship, start getting upgrades… save their ship
  configurations", ✅ "expandable data system… doesn't break anyones info" (versioned migrate),
  ✅ "user named claude… orange", ✅ "every player should have a central color… theme"
- ❌ "we will go through and modularly create a huge list of ships to customize… starting based
  on all the no mans sky ones" — only hauler/fighter/explorer exist. OPEN.
- ✅ "shops u can land and interact with NPCs and buy weapons"

### Your ship is your base / crew / pets
- ✅ "when u get out of ship make it like 10x bigger than the no mans sky ship… inside your ship is
  your base", ✅ "take creatures from plannets and bring them in your ship… little home… pens"
- ✅ "recruit allies every once and a while at burger house, then they live in your ship and have
  little pods and will go on missions for you… increase your gorCoin and bring u other resources"
- 🟡 Emory has NOT yet seen the interior / pets / crew in person (he was mid-beta on older pushes).

### NPCs / dialog
- ✅ "procedurally generated npc conversation especially in ways that feel like alien language"
  (lingo + learned words + branching converse)
- ✅ "I haven't seen the space station 7/11 or any NPC yet, any NPC dialog get it all happening"
  (it was unpushed at the time; now live)

### Multiplayer
- ✅ "each player running the simulation locally… server just be informing the ships", ✅ "another
  ship to appear… amount of ships per all the people that are on at once", ✅ "friendly fire is on
  between player", ✅ "simulations are in line with each other" (shared orbital clock), ✅ "arrow of
  whatever color of the player's ship", ✅ "/color changes main color… default random color"
- ✅ "/gorcave 'text'… sandbox version of the full model… nerfed… can't change my lights" and
  "fuck that [API key], use same mechanism that the gorcave server used" (Pi Claude CLI, no tools)
- ✅ "make sure multi player is tight in all of this" (requirement section + relay rooms/host election)
- 🟡 Real two-device test of in-world fights / mobs / villages: NOT done (only local two-tab tests
  of ghosts, chat, profiles, events).
- ❌ "we need money… remember progress across refreshes" ✅ local; cross-device via `/user NAME
  KEY` ✅ built, 🟡 not used by him yet.

### Performance / organization
- ✅ "make sure my FPS stays super high" (quality manager, LOD, allocation audit)
- ✅ "optimization… run super well even on chrome book… hella elegant, well organized" —
  🟡 the MODULE SPLIT of js/ship.js (docs/ship-architecture.md) was planned and NEVER done; ship.js
  is ~600 KB. OPEN.
- ✅ "procedurally generated 3d asset vibes… asset components modularly combined" (ship-parts.js)
- ✅ "token preservation mode… no haiku… 2 builders"

### Smash / mashup (newest, mostly OPEN)
- ✅ "a way to play super smash bros on the website" → GOR BRAWL at /smash.html (13 fighters, bots,
  hot-seat, gamepad, deterministic, finals meter)
- ✅ "smash is 2d and ill mimic some main characters, yoshi, mario, steve… classic smash… 12 of
  them… in moves and behavior and feel" (13 archetypes incl. Steve)
- ✅ "no items in smash", ✅ "stage is wherever characters were in the world" (session builds from
  terrain samples) — ❌ but the in-world FIGHT prompt is NOT wired (see findings above).
- ✅ "I want them all in the same game… when u get out of ship" — partially: styles ✅, mobs ✅,
  ❌ fights in-world, ❌ hub wired, ❌ villages.
- ✅ "switch your character… thier movement style… n64 mario… sonic" (MARO/JOSHI/STEEV/SONIK live;
  ❌ the other styles from the plan not built; ❌ wall-kick never fires; ❌ SONIK loops)
- ✅ "minecraft… creepers and endermen and zombies and skelletons on the plannets randomly" (night
  mobs) — 🟡 not seen by him yet.
- ❌ "certain areas 1 per plannet are like the mob hub… minecraft dungeons 3d fighting vibe" —
  module built, NOT wired.
- ❌ "the minecraft villages, this is huge aspect i want, not destroyable houses, iron golumns etc."
  — NOT started. Spec in this doc.
- ❌ "pokemon… call them pokemans… full procedurally generated… catch… 1 at a time… follow you
  around" — planned only (docs/mashup-plan.md). "don't bring them into smash yet".
- ❌ "call them different names like maro, joshi, steev" — working names in place; the final
  naming pass + the banned-name pre-push grep script NOT done.
- ❌ "find the decompiled smash melee… decompiled n64 mario for mechanics" — DECLINED for code
  (legal); mechanics taken from public docs instead; he accepted ("no genuine copyright
  infringement").

### Still owed from early asks (easy to forget)
- ❌ Mobile/touch controls (phones are gated out).
- ❌ Photo mode + share link; discovery UI page; leaderboard on the site.
- ❌ Module split of ship.js.
- 🟡 Playtest punch lists (docs/playtest-1.md, -2.md): most fixed; re-run a playtest after villages.
- 🟡 `/smash` command / arcade cabinet in the station 7/11 (plan) — only the standalone page exists.
