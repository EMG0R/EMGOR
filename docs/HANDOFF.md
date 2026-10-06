# NO MANS GOR — HANDOFF (2026-10-07)

For a fresh Fable session taking over. Read this, then `docs/ship-mode.md` (the full spec history,
revisions 1–30), `docs/mashup-plan.md`, `docs/smash-plan.md`, `docs/multiplayer-plan.md`,
`docs/playtest-1.md`, `docs/playtest-2.md`. Memory rules: `~/EMGOR_SKILLS/_EMORY.md` is loaded
every session; the token-preservation rule is in `memory/emory/token-preservation-mode.md`.

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

## State at handoff (everything committed, tree clean)
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
1. Commit `js/ship-hub.js`; finish villages (+ golums, indestructible huts); finish in-world
   FIGHT + two-window tests; wire hub + villages into ship.js (F at the gate enters the hub; mobs
   from `hub.waves`; chests; village trades/quests; golum AI); push; deploy relay.
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
