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
