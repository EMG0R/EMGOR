# NO MANS GOR — fly the galaxy (design contract)

Status 2026-09-30: Phase 1 built and verified locally in Chrome (enter, throttle, pulse,
sector streaming, Esc out to nearest system incl. uncharted ones, Esc back in). Not yet
committed or deployed. Not tested with real pointer lock by a human yet.

## What it is

**NO MANS GOR** is the name of the whole thing. A ship parked in the galaxy. Click it (or press **Esc** on the galaxy view) and the
same scene becomes a No Man's Sky style flight game: third-person chase cam, mouse
steering, throttle, boost, pulse drive. Fly between the real planets (galaxy.json),
fly past the edge into procedurally generated uncharted systems that keep spawning
forever. **Esc** drops you back into the normal galaxy view, orbit-camera framed on
whichever system you are nearest, hash route updated. **Esc** again puts you back in
the ship exactly where you left it.

Same scene, same bodies, same positions, same camera object. No second renderer, no
page swap, no loading screen. The galaxy page is the game.

## Non-negotiables

- First paint of the homepage does not get heavier. `ship.js` is a separate ES module
  that `galaxy3d.js` lazy-imports after `galaxy-ready`, on `requestIdleCallback`
  (fallback `setTimeout 1500`). By the time anyone finds the ship it is loaded.
- Phones: not loaded at all. Gate = `matchMedia('(pointer: coarse)').matches` OR no
  `document.body.requestPointerLock` OR `innerWidth < 900`. No ship mesh, no label,
  Esc keeps its old behavior. Touch controls are a later phase.
- No GLB/texture downloads for the ship. Procedural low-poly geometry, EMGOR violet.
- Every procedural body is built by the SAME pipeline as real planets (seedIdentity +
  buildBodyObjects + the same shaders), so uncharted worlds look like family.
- New real content (new markdown under `universe/`) shows up automatically: ship mode
  reads the live node tree, never a copy. Procedural space starts outside an exclusion
  sphere derived from the root system radius, so a growing galaxy never collides.
- Zero idle cost when not in ship mode beyond one parked mesh + one DOM label.

## Phases

1. **Flight** (now): ship, chase cam, NMS controls, HUD, Esc toggle both ways,
   nearest-system re-focus, infinite procedural sectors, target-and-inspect (F).
2. **Polish**: engine audio (after first gesture), warp tunnel on pulse, star streaks,
   touch joystick for phones, gamepad.
3. **Landing**: approach a planet below a threshold altitude, atmosphere entry, swap
   to a surface scene (quadtree terrain sphere patch driven by the same noise/palette
   as the planet shader), ship touches down, exit on foot (WASD). Possible, it is the
   same trick NMS uses (hidden scale swap at the atmosphere line). Not in phase 1.

## Controls (NMS mapping)

| input | action |
|---|---|
| mouse (pointer-locked) | pitch / yaw (nose follows cursor, with lag) |
| A / D | roll |
| W / S | throttle up / down (S past zero = brake, then slow reverse) |
| Shift (hold) | boost |
| Space (hold) | pulse drive: 0.6 s spin-up, then ~10x speed, drops when released or near a body |
| F | inspect target: exit ship, focus that node, open its overlay |
| Esc | exit ship (pointer-lock release IS the exit signal) |
| Esc on galaxy view | re-enter ship (only once ship.js is loaded) |

Ship states: `docked` (parked, orbiting slowly near the black hole, label "SHIP"),
`piloting`, `away` (left somewhere, galaxy view active; Esc returns).

## Engine port (owned by galaxy3d.js)

`window.EMGOR_GALAXY.engine` is set once bodies exist, before `galaxy-ready`:

```js
engine = {
  THREE, scene, camera, renderer,
  get composer(),           // may be null
  get time(),               // orbital time (s)
  root, byId, drawOrder,    // live node tree; nodes have wx, wy (x, z), anchor, mesh, sysR, bodyR, pal
  renderedRadius(node),     // the on-screen-truth radius: uniformSizeFor(node, node.parentNode)
  makeBody(spec),           // spec {id, title, parentNode|null, sysR, sizeF, wx, wy, depth} ->
                            //   node with pal/shader identity seeded from id, anchor added to scene.
                            //   NOT inserted into drawOrder; caller animates anchor.position itself.
  disposeBody(node),        // removes anchor, disposes materials (geometry is pooled, never disposed)
  setPilot(fn | null),      // fn(dt) runs each frame INSTEAD of the orbit camera build; while set:
                            //   labels hidden, HUD (.hud) hidden, pointer/wheel/click galaxy input
                            //   ignored, every real body rendered at alpha 1 and size
                            //   renderedRadius(node); focus/trans untouched.
  focusNode(node, fromCamera), // orbit-cam focus on a real OR procedural node. Procedural nodes
                            //   (not in byId) get crumb = node.title, no route write, no overlay.
                            //   fromCamera=true: blend from the camera's current pose (ship cam)
                            //   into the orbit rig over FLY_DUR instead of snapping.
  openOverlay(node),        // real nodes only
  nearestSystem(x, y, z),   // real nodes only: argmin over drawOrder of dist/sysR, returns node
  onEscape(fn | null)       // when set, galaxy-view Esc (no overlay open) calls fn() instead of zoomOut
}
```

Loader: after `galaxy-ready`, if the phone gate passes, `import('./ship.js')` and call
its default export `mount(engine)`.

## ship.js (owned by ship.js + style/ship.css)

- Builds the ship mesh, docks it on a slow orbit at 0.35 × root sysR, Y slightly above
  the plane. A DOM button `.ship-label` ("NO MANS GOR") tracks its projection each frame
  (same visual language as `.planet-label`). Click → `enter()`.
- `enter()`: request pointer lock, `engine.setPilot(step)`, `engine.onEscape(enter)`,
  show HUD, chase cam blends from current camera pose to behind-ship over 0.9 s.
- `step(dt)`: integrate controls → ship quaternion/velocity; chase cam with spring lag;
  stream sectors; HUD update; target pick (ray from ship nose, cone 6°, nearest).
- `exit()`: `engine.setPilot(null)`, `engine.focusNode(engine.nearestSystem(...) or
  nearest procedural system, true)`, `engine.onEscape(enter)`, state = `away`.
- `pointerlockchange` to unlocked while piloting → `exit()`.
- Procedural sectors: cubic chunks, side 3000 world units, keyed by `hash32('sec:'+cx+','+cy+','+cz)`.
  Load the 3×3×3 chunk neighborhood around the ship; dispose chunks that leave it.
  A chunk whose center is inside `exclusionR = root.sysR * 1.8` of the origin spawns
  nothing. Otherwise 0–2 systems: one sun (gas biome, sizeF 1.6) + 2–6 planets on the
  same orbit math (ORBIT_MIN/MAX fractions, BASE_ROT drift, seeded phase). Y is drawn
  from a gaussian with sigma 0.15 × chunk so space reads as a disk, not a cube.
  Names from a seeded syllable generator; label on HUD as `UNCHARTED · <NAME>`.
- Flight tuning (world units/s): cruise max 420, reverse 80, boost 1100, pulse 7000.
  Pulse auto-drops within 4 × renderedRadius of any body. Collision: soft push-out at
  1.2 × renderedRadius, no damage.
- HUD (`#ship-hud`, built by ship.js, styled by `style/ship.css` injected on mount):
  center reticle, target name + distance, speed readout, throttle bar, pulse charge
  bar, bottom hint line ("ESC exit · F inspect · SPACE pulse"). Violet/cyan, mono
  caps, same vars as galaxy.css. No DOM churn: update textContent/style only when the
  value changes.

## Verified 2026-09-30 (local, automation-driven frames)

- Homepage first paint unchanged; ship.js (33 KB) + ship.css (4.5 KB) load on idle.
- Pulse to 12k units out spawned ~36 uncharted bodies, no console errors.
- Esc out of uncharted space framed the generated sun with its name in the crumb.
- Known: with pointer lock refused (automation), Esc fallback path is used; real
  browsers use the pointerlockchange path.

## Files

- `js/galaxy3d.js` — engine port + loader + Esc routing (small, additive)
- `js/ship.js` — the game
- `style/ship.css` — HUD + ship label
- `docs/ship-mode.md` — this file

## Revision 2 (2026-09-30, after Emory's first flight)

Emory's notes: mouse has latency he doesn't want; ship must be much smaller next to
planets and much more detailed; planets should take noticeably longer to reach; drop
the infinite procedural generation for now (bounded space, can get pretty far); and
the priority is ENEMIES with a difficulty curve and death → restart.

- **Mouse**: direct. Pointer delta maps straight to turn rate this frame, no
  smoothing, no cursor re-centering, no "nose lag". Only a tiny clamp on max rate.
- **Ship scale**: length = 0.06 × renderedRadius of a root-level planet (was 0.25).
  Chase cam distance scales with it. Hull gets real detail: fuselage with canopy,
  swept wings with tip fins, twin engine nacelles, underslung gun pods, panel-line
  vertex colors, two small thruster glows (not one big ball).
- **Travel time**: cruise max 140 u/s, boost 360, pulse 2600. Pulse spin-up 1.0 s.
- **Space**: finite. Soft boundary sphere at 3.5 × root.sysR: beyond it the ship is
  gently steered back and the HUD says "EDGE OF KNOWN SPACE". No sectors, no
  makeBody at runtime. (Procedural code removed from ship.js, not kept dormant.)
- **Combat**:
  - LMB fires twin lasers (cyan bolts, 2 × 6/s, range 40 L, hitscan-ish fast
    projectiles). Enemies have hull HP; hits spark, death = short debris burst.
  - Player: hull HP 100, shield regen. HUD: HP bar, wave counter, enemy count,
    kill count.
  - Enemies: small low-poly drones in a hostile palette (ember red / acid green),
    engine glow, simple AI states: approach → strafe pass → turn around; fire
    red bolts when the player is inside their cone and within range.
  - Difficulty curve (waves spawn on a timer, 20–40 s apart, near the player but
    outside view, each wave announced on the HUD):
    1. **Wave 1**: 2 slow drones, inaccurate, low damage. Regen outpaces their
       DPS, so you cannot die; you can ignore them entirely.
    2. **Wave 2**: 5 faster drones with real aim. Regen can't cover it; you must
       dodge or fight. This is the jump.
    3. **Wave 3+**: steady "chill" level: 3–4 drones per wave, moderate aim, a
       regen pause of 4 s after each hit. Passive play survives if you keep
       moving; engaging is where the fun is. Every wave after 3 adds +1 drone
       every 3 waves, capped at 8.
    - Enemies only aggro within 25 L of the player; outside that they drift and
      patrol, so passive flight is possible.
  - **Death**: screen flash, 1.5 s, then restart: ship back at the dock, waves
    reset, kills reset. Still in piloting state.
  - Esc still exits to the galaxy view at any time; enemies freeze while out.

## Revision 3 (2026-09-30, queued behind rev 2)

- **Hull**: `js/ship-hull.js` is the NMS hauler replica built from Emory's photos
  (magenta boxy fuselage, orange V, tan cargo band, three ribbed boosters, big blue
  ring engine + four white thruster slots, legs, antennas). ship.js imports it.
- **Allies**: from wave 3 on, friendly ships (same hull module, different livery:
  violet/cyan) fly in and fight alongside you. No friendly fire in either direction
  (ally bolts ignore the player and allies; player bolts ignore allies). Allies
  pick the nearest enemy, strafe it, and regroup near the player when idle. 1 ally at
  wave 3, 2 at wave 5, 3 at wave 7 max. Allies can be killed and return next wave.
- **Enemies are alive**: alien-hybrid creature ships, not drones. Organic silhouettes
  (segmented bodies, fin-wings, a glowing eye core), biolum ember/acid palette,
  bodies that flex/pulse (vertex sine in the shader), and movement with a swim-like
  wobble. Same AI states as rev 2.
- **Bosses**: after wave 3, every 3rd wave (6, 9, 12, ...) a boss spawns with the
  wave: a giant alien monster, each bigger than the last (scale × 1.4 per boss), HP
  bar across the top of the HUD with its name, slow, tanky, fires bolt spreads and a
  slow homing orb; weak point = the eye core (2× damage). Allies focus it. Killing it
  ends the wave.
- **Respawn**: death restarts the CURRENT wave (same wave number, enemies respawn),
  not the whole game. HP full, position at the dock.
- **Progress persists**: `localStorage['nmg']` = { wave, kills, bestWave }, written on
  every wave start and every death; restored on mount, so a refresh resumes at the
  start of the wave you were on. Clearing is a HUD "reset" link in the away state.

## Revision 4 (2026-09-30)

- **Dive-bomb AI**: enemies make attack RUNS: line up far out, dive at you fast while
  firing, pass, peel away to a loop point 20–30 L off, re-line, repeat. They never
  hover on top of the player.
- **Aim assist**: when the reticle is within ~4° of an enemy, the nose is pulled
  toward it with a soft magnet (capped rate), released the moment the mouse moves
  hard. Ally/boss weak points count as targets too.
- **Scale**: everything 3× smaller again relative to the planets (L = 0.02 × refR).
  Planets are massive; crossing between them takes a long time at cruise, boost
  feels very fast in ship-lengths, pulse is the only sane way between systems.
- **Stars**: the starfield becomes a real 3D sky (galaxy3d.js): a Points sphere at
  the camera's position that inherits none of its rotation, so every pan, orbit and
  roll moves the stars realistically in the galaxy view AND in the ship. Nebula
  backdrop stays 2D but gets a slow parallax from camera yaw.

## Revision 5 (2026-10-01) — NMS scale, locked

Measured: rendered root-planet radius refR ≈ 102 world units. NMS ratio planet radius :
ship length ≈ 1000 : 1 (20 m ship, ~20 km planet). So:

- **L = refR / 1000** (≈ 0.10 u). Chase cam, bolts, spawn/aggro, allies, bosses all in L.
- **Speeds (world u/s)**: cruise 1.5 (15 L/s), reverse 0.4, boost 5 (50 L/s), pulse 20
  (200 L/s). Pulse spin-up 1.5 s. Neighbor planets are ~600 u apart → ~30 s on pulse;
  across the galaxy ~100 s. Cruise between planets is not viable, as in NMS.
- **Pulse exit**: auto-drop at 1.5 × R (+ speed lookahead) so the last leg is a boost run
  of ~10–15 s to the surface, not a 3-minute crawl. Pulse cannot be engaged inside 1.5 R.
- **Enemies bigger + slower**: creature length 4 L (wave 1 6 L), boss base 40 L, ×1.4 per
  boss. Enemy speeds relative to cruise: wave 1 0.6×, wave 2 1.0×, wave 3+ 0.9×; dive
  multipliers unchanged. Enemy fire ranges scale with their size.
- Camera near plane must stay below 0.3 L: galaxy3d sets 0.05 (ok). Logarithmic depth is
  what makes a 0.1 u ship next to a 100 u planet render cleanly.

## Revision 6 (2026-10-01)

- **Enemies**: HP ×4 (wave 1: 120, wave 2: 180, wave 3+: 240, scaling +10%/wave); length
  ×1.5 again (creature 6 L, wave 1 9 L). Hit sparks scale with size so damage reads.
- **Allies weaker + follow your lead**: ally bolt damage ÷3, fire rate ÷2, HP 50. Allies
  never fire until the player has fired at least once in the current wave; after that
  they engage. They still fly runs and draw fire.
- **Earlier/more enemies**: first wave at 3 s; waves every 12–20 s; wave 1 = 3, wave 2 = 6,
  wave 3+ = 4–5 (+1 every 2 waves, cap 10).
- **Start at wave 1 every time**: persistence keeps bestWave + kills only; wave is not
  restored on load (the restore code stays behind a flag `RESUME_WAVE = false`).
- **Slash commands** (Minecraft style): while piloting, pressing `/` opens a one-line
  input at the bottom of the HUD (pointer lock is released, keys are captured by the
  input, Enter runs it and re-locks, Esc closes it without exiting the ship). Commands:
  `/wave N` or `/waveN` (jump to wave N, respawns enemies), `/boss` (spawn the next boss
  now), `/heal`, `/kill` (clear all enemies), `/allies N`, `/god` (toggle invulnerable),
  `/reset` (clear saved progress), `/help`. Unknown command → red one-line error.

## Revision 8 (2026-10-01) — the docked ship is a thing you fly INTO

Emory: "why is there still a NO MANS GOR planet? it should be a much larger ship that I
can click on, then it zooms into it and the perspective shifts."

- **Galaxy-view ship**: while docked/away, the hull is rendered at GALAXY scale: length
  = 0.9 × renderedRadius of a root-level planet, slowly orbiting its dock slot, slowly
  yawing so it reads as a ship from the orbit cam. Label stays under it.
- **Boarding cinematic (click or Esc)**: 1.6 s. Camera flies from the orbit rig toward
  the ship's tail while the hull scales from galaxy scale down to true L (log-space
  lerp), ending EXACTLY in the chase-cam pose at true scale; pointer lock is requested
  on the click; HUD fades in during the last 0.4 s. No cut, no snap.
- **Exit**: reverse of the same move: setPilot(null) only after the camera has pulled
  back out and the hull has scaled back up to galaxy scale, then the orbit rig takes
  over from that pose (engine.focusNode(root, true)).
- Remote ghost ships (multiplayer) use the same two scales: galaxy scale when their
  owner is docked, true scale when flying.

## Revision 9 (2026-10-01) — feel, fx, combat depth

Emory's brief: ramp difficulty 4× faster; enemies must flee after enough hits so they
can't be killed head-on; Shift/Space work in reverse; speed vignette far subtler but add
Star Wars star streaks; real propulsion exhaust and bolts with depth, cheap; camera shake
on hit and a touch on fire; ship weight back to an exponential curve (heavier than now,
faster than NMS). And: research NMS combat criticisms, beat them.

### A. Feel (ship.js)
- **Weight curve**: input delta → target rate through `sign(d)·|d|^1.6` (precise small
  moves, fast big ones); rate chases target with MOUSE_WEIGHT 6.5 (was 11); add
  **bank**: roll follows yaw rate (−0.35 × yawRate, spring 4/s) so turns lean like a
  real craft; velocity keeps a little lateral slip (vel chases forward×speed at 3/s,
  was 4) so the ship drifts through turns.
- **Reverse**: throttle < 0 → Shift = boost in reverse (0.7 × boost), Space = pulse in
  reverse (0.6 × pulse). Same spin-up, same drops.
- **Camera shake**: on hit, shake amplitude = dmg/20 × 0.25 L decaying at 6/s; on fire,
  0.03 L kick per volley. Applied as a camera-local offset after the chase pose; never
  touches ship state. Boss hit = 2×.
- **Speed vignette**: current FOV kick halved (boost +4°, pulse +9°), CSS vignette opacity
  ×0.35. The sensation of speed comes from streaks (B) instead.

### B. FX module (NEW js/ship-fx.js, owned separately, cheap)
`export function createFx(THREE, scene, camera, L) -> fx` with:
- `fx.streaks` : Star Wars streaks = one THREE.Points (600 pts) in a 60 L box around the
  camera, wrapping; vertex shader stretches each point along the ship's velocity in
  clip space (uVel, uStretch); alpha ∝ speed above cruise; at pulse they become long
  lines. Zero allocation per frame; `fx.setMotion(velVec3, speedNorm, pulsing)`.
- `fx.exhaust(n)`: per thruster, a cone mesh (8 segs) with an additive ShaderMaterial:
  length ∝ throttle/boost/pulse, noise flicker via uTime, cyan→violet gradient, plus the
  existing small glow sprite at the base. `fx.setExhaust(i, intensity)`.
- `fx.bolt`: bolts become core + halo (two instanced quads via InstancedMesh, cap 160)
  with muzzle flash sprite 80 ms; `fx.spawnBolt(pos, dir, color)`, `fx.flash(pos)`,
  `fx.impact(pos, color, size)` (12-particle spark burst from a pooled Points).
- `fx.update(dt, camera)`. Every pool pre-allocated; no per-frame `new`.

### C. Combat depth (ship.js, after research doc docs/combat-research.md)
- **Ramp**: wave N uses the old curve's wave 4N (wave 3 ≈ old 12): HP, count, dmg,
  speed, aim all follow; bosses every 2nd wave from wave 4; cap counts at 10.
- **Flee**: at ≤ 45 % HP an enemy enters FLEE: 3× speed burst away from the player for
  2.5 s, out of bolt range, then regenerates 15 %/s for 3 s while circling at 40–60 L,
  then returns. Head-on kills are impossible; you kill by pursuit (boost), by leading
  the flee vector, or by catching them during the regen circle. Enemies telegraph the
  flee with a flash of the eye core.
- Fold in the research doc's top 3 recommendations (to be written in rev 9b once the
  research lands).

## Revision 10 (2026-10-01) — users, ships, upgrades (foundation now, catalog later)

- **`/user NAME`**: profile switch/create. Profile = { color, hull, upgrades, kills,
  bestWave, created }. Stored locally in `nmg.users`; the relay only ever sees name +
  color + hull id. Later: server-side profile store on gorcave so a user follows you
  across devices (needs a secret per user, not a password: `/user NAME KEY`).
- **Ship registry** (`js/ships/index.js`): manifest `{ id, name, class, build(THREE),
  stats: { speed, turn, hp, guns } }`. Hull modules one per file (`js/ships/hauler.js`
  = today's ship-hull.js moved). Catalog starts from the NMS classes: hauler, fighter,
  explorer, shuttle, exotic, living ship, solar, sentinel interceptor; then EMGOR
  originals. `/ship ID` picks one (the only new command).
- **Upgrades**: earned by wave/boss kills, saved on the profile: engine (cruise/boost),
  weapons (dmg, fire rate), shield (hp, regen), pulse (spin-up). `/upgrades` lists.
- Commands total after this: /wave /peaceful /hostile /difficulty /user /color /ship
  /upgrades /help. That is the cap; anything else gets a UI, not a command.

### Rev 9b — combat depth, decided (from docs/combat-research.md)
1. Lead pip (target.pos + vel·dist/boltSpeed) drawn on the HUD; T cycles targets; aim
   magnet reduced to 40 % strength and a 2.5° cone.
2. Enemy roles: interceptor (1.3× speed, 60 % HP, 60 %), spitter (0.6× speed, 150 % HP,
   slow dodgeable orbs, 25 %), sniper (holds 80 L, 1.2 s visible charge beam, 15 %).
   Wave 1 interceptors only; spitters from wave 2; snipers from wave 3. Distinct
   silhouettes/colors from ship-enemies.js.
3. Dodge roll: double-tap A/D → 0.45 s barrel roll, 1.5 s cooldown, 40 % damage cut.
   Boost-drift: slip 0.4/s while boosting through a turn; +25 % speed for 1 s on release.
4. Spawn warning: spawn 150–250 L out, never inside 100 L; 3 s grace with HUD bearing
   arrows; 8 s breather between waves.
5. Hit feedback: reticle tick scales + pitches up with combo; 40 ms hit-stop on kills;
   enemy flinch on hit. Enemy HP returns to ~6 hits (wave 1) → ~10 hits (wave 3); the
   FLEE mechanic (rev 9 C) is what prevents head-on kills, not sponge HP.
Do-not list honored: no energy management, no inertia-only flight, no HP sponges.

### Rev 9c — fun loop, decided (from docs/combat-fun.md)
1. Kill-fed shield orbs + chain: kills drop an orb worth 12 shield (auto-collect within
   6 L, drifts toward you if within 20 L), chain window 4 s, chain ≥ 3 doubles the orb;
   passive regen reduced to 40 % of today's. Aggression heals; hiding does not.
2. Graze → Overdrive: enemy bolts passing within 1.5 L add a graze (HUD meter, 10 to
   fill); full = 4 s Overdrive (fire rate ×1.5, dmg ×1.25, reticle goes hot).
3. Focus (hold Q): world dt ×0.35 for ≤1.5 s, 8 s recharge; input stays real-time.
   Z = allies focus your current target (they peel off everything else for 8 s).
4. Wingman in peril: an enemy on an ally's tail for 2 s → HUD callout + bearing arrow;
   10 s to break it or the ally drops to 40 % HP.
5. Boss phases at 100/66/33 %: eye exposed only during the 2 s telegraphed attack;
   each phase change spawns 2 interceptors; boss gains a new attack per phase.
6. REJECTED: painted homing multi-lock (auto-hit missiles are the NMS complaint).
Tempo: 60 s cycle = approach 0–8 s, engage 8–35, climax 35–50 (chase the fleeing),
breather 8–12 s. Every 4th wave is a short spike + longer breather. Boss waves replace
a normal wave and run 90–120 s. Wave timer/counts in rev 6/9 bend to this rhythm.

### Rev 8 addendum — main-menu ships
- Remote players' ships render at the current galaxy-scale size with their user name.
- YOUR ship renders 3× that, label "YOUR SHIP" until a `/user` name is remembered in
  localStorage, then the name. Only your ship is clickable; clicking it boards.

## Revision 11 (2026-10-01) — Minecraft chat + nerfed gorcave

- **Chat UI**: the `/` line and a chat log styled like Minecraft: pixel font
  (Monocraft, OFL, lazy-loaded at mount, never on first paint), grey translucent
  rows bottom-left above the hint line, newest at the bottom, lines fade after 10 s,
  T or `/` opens the input (T opens it empty for plain chat to other players; `/` with
  the slash). Plain text (no slash) is multiplayer chat, relayed to everyone online.
  Styles in NEW style/ship-chat.css.
- **`/gorcave <text>`** (quotes optional): a sandboxed "nerfed gorcave" answers in the
  chat as neptr. Server side lives in the relay (`server/nmg-relay/gor.js`): a plain
  Anthropic Messages API call, NO tools, system prompt = `_NERFED_GORCAVE.md` only,
  max 300 output tokens, 1 request / 5 s per client, 60 / hour global, 400-char input
  cap, replies broadcast to the room as `{t:'gor', from:'neptr', text}`. API key from
  an env file on the Pi, never in the repo. The real Claude Code gorcave agent
  (:8787 /api/chat) is NOT reachable from the game, by construction (different process,
  different path, no proxying).
- **Persona** `~/EMGOR_SKILLS/_NERFED_GORCAVE.md`: neptr's voice (from _NEPTR.md) with
  only public facts about Emory and the site (from universe/**), explicitly no
  credentials, no memory/ files, no device control, no personal data; it knows it is
  the lobby version of neptr and says so if asked to do anything real.

### Rev 10 addendum — data that never breaks
- Every saved blob (`nmg`, each profile, relay `hi`) carries `v`. One `migrate()` on
  load fills defaults and NEVER drops unknown fields. Future versions add fields, never
  rename or repurpose. Relay ignores unknown fields.
- Theme color = one `profile.color`: hull main paint, thruster glow, exhaust base, bolt
  color, ghost label, chat name. Decals stay orange/tan.
- Built-in test user `claude`, orange (#FF7A1A). All automated checks run `/user claude`.

### Rev 9d — pulse ramp, evasive hunters, frame budget
- Pulse: exponential ramp while Space is held, τ ≈ 3 s, max ~60 u/s; release decays 1 s.
- Enemies never hold still and never fly straight at you: seeded lateral weave + slanted
  spiral approach in every state; they close distance while sliding to your sides.
- Frame budget: step() < 2.5 ms with 10 enemies on an M4; DOM writes only on change;
  far-enemy AI at half rate; squared-distance culls before any real test.

## Revision 12 (2026-10-01) — planets you can fly into

### A. Planet surfaces (NEW js/ship-planet.js)
Principle: the surface IS the planet shader. galaxy3d's PLANET_FRAG colors a planet by a
seeded noise height field (uSeed/uFreq/uWarp/uSeaLevel/uBands, palette hi/lo/sea). The
surface module copies that exact GLSL noise into a vertex shader, so terrain up close is
the same continents you saw from orbit. Nothing is allocated until you are within 3 R.

- `createPlanetSurface(engine, L) -> ps`; `ps.update(dt, shipPos)`; `ps.active` (node or
  null); `ps.floorAt(pos, out)` → `{ r, n }` surface radius along pos's direction + normal
  (JS port of the same noise, used ONLY for collision of ship/enemies, a few samples per
  frame); `ps.dispose()`.
- **Near mode** (dist < 3 R of a real body, R = engine.renderedRadius): one reusable
  terrain patch, a 128×128 grid of a spherical cap (~25° wide, narrowing as you descend)
  centered under the ship, re-centered when the ship moves > 1/8 of its width. Vertex
  shader: direction → noise height (amplitude 0.04 R, sea flattened at uSeaLevel) →
  position; fragment: same palette logic as PLANET_FRAG + slope darkening + distance
  fog in the planet's atmo color. The orbital planet mesh fades out below 1.6 R so the
  patch takes over without a seam; above 1.6 R both show (patch is a detail layer).
- **Atmosphere** (< 1.4 R): sky dome tint = pal.atmo, fog density ramps with depth,
  stars fade, bloom unchanged. Pulse cannot start inside 1.4 R and auto-drops at 1.5 R.
- **Collision**: inside 1.25 R the hard shell is replaced by terrain: floor = floorAt(pos).r
  + 1.5 L; the ship slides along it, never through it. Enemies use the same floor + 2 L.
- **Flora** (< 1.3 R): one InstancedMesh of ~500 low-poly trees (3 variants, palette-
  tinted, 60 tris each), placed by seeded hashing of the patch cells so the same spot
  always has the same trees; re-seeded when the patch re-centers. Only on land, not sea.
- **Creatures** (< 1.2 R): one InstancedMesh of ~40 critters (2 variants: a 6-legged
  walker, a hovering jelly) wandering along the floor with a sine gait, seeded per cell,
  despawned when the patch moves on. Purely decorative; no collision.
- **Budget**: ≤ 5 draw calls total for a near planet; patch displacement is 100 % GPU;
  the JS noise port is called ≤ 20× per frame.

### B. Combat + dock (ship.js, ship-enemies.js, ship-hull.js)
- **Enemies through atmosphere**: enemies ignore the orbital shell, follow the terrain
  floor + 2 L, and can be chased in and out of atmosphere. FLEE prefers diving toward a
  nearby planet (then skims the surface) so chases go planetside.
- **Stable motion**: enemy paths are WORLD-anchored (weave and spiral computed in world
  space around waypoints), not displacements relative to the player, so you can fly
  around an enemy and it reads as a solid object moving through space.
- **Bosses**: 2.5× bigger again, near-stationary (slow drift ≤ 0.3 u/s), hold a distance
  band of 60–90 L from the player: if you fly away they follow at that distance; if you
  close in they let you. Each boss gets ONE signature attack (seeded from its name), big
  telegraph, heavy damage (30–45 HP): beam sweep (2 s rotating beam), orb ring (12 orbs
  expanding then contracting), ram charge (telegraph 1.5 s, 3 s charge at 3× speed),
  mine field (8 mines drifting, 2 s fuse on proximity), gravity pull (drags you toward
  the eye for 3 s while escorts fire). Plus the phase attacks from 9c.
- **Ramming**: colliding with an enemy = 45 HP to the player (boss: 70) and a hard
  bounce; the enemy takes 25 % of its max HP. Don't fly into things.
- **Docked ship**: half the previous size (1.35 × refR), low-detail LOD (`buildHull(THREE,
  { lod: 'low' })`: fuselage + wings + fin + engine ring only, no decals/legs/antennas),
  parked ABOVE the orbital plane at y = +0.45 × root.sysR, x/z outside every orbit
  (1.05 × root.sysR), so it is alone against the stars and easy to spot.

## Revision 13 (2026-10-01) — solid planets, pulse anywhere, procedural enemies, titans

### A. Planets (ship-planet.js)
- **Never hollow**: the orbital planet mesh is NEVER hidden. The terrain patch draws ON
  it with polygonOffset (and its own radius ≥ the sphere's), so from inside the
  atmosphere there is always a solid ball under you and the patch is the detail.
- **Relief**: AMP 0.04 → 0.09 R, mountains with a second octave; sea stays flat.
- **Entry transition** (2.2 R → 1.4 R): the planet's atmo halo brightens and expands as
  you approach, a thin atmospheric rim pass at 1.6–1.4 R (wind streak sprites + a
  short rumble shake), then the dome/fog take over. From above it looks like the
  planet; from inside it looks like sky over ground. No pop at any altitude.
- **Scale**: L = refR / 2000 (ship half again), so planets read 2000 ship-lengths in
  radius. Patch width/grid tuned so the ground still has detail at L height.

### B. Flight (ship.js)
- Speeds ×2: cruise 3, reverse 0.8, boost 12, pulse ramp to 140 u/s (τ 3 s).
- **Pulse anywhere**: no altitude rule. Pulse drops ONLY when the swept path would hit
  terrain or a body within 0.6 s; near the ground at pulse you get a gentle auto
  pull-up (max 25°/s) so skimming is possible but ramming the ground is not.
- **Heavier, realistic**: thrust → acceleration (not velocity), mass-feel: velocity
  chases the thrust vector at 2.2/s (drift through turns), angular inertia on
  pitch/yaw (rate chases target at MOUSE_WEIGHT 5.5), bank 0.4, g-shake above 0.8 of
  max turn rate. Maneuvers: double-tap A/D barrel roll (exists), double-tap S =
  Immelmann-ish flip (0.8 s, 180° pitch + roll), hold Ctrl = drift turn (nose turns,
  velocity keeps going, 1.2 s), all with 1.5 s cooldowns and HUD rings.
- Ramming an enemy stays 45 HP; hitting terrain at > boost speed = 30 HP + bounce.

### C. Enemies (ship-enemies.js generator + ship.js AI)
- `generateEnemy(seed, tier)` → { group, stats, attacks, weakPoints }: procedural
  creature ships: a spine of 3–9 segments (scaled lathe/ellipsoids), 0–4 fin-wing pairs,
  0–6 spikes/tentacles (tapered cones, animated), 1–3 glowing eye cores, biolum
  palette per seed, shader flex. Tier 1 ≈ 6 L, each tier ×1.6. Distinct silhouettes
  guaranteed by the seed (never two identical shapes in a wave).
- Roles stay (interceptor/spitter/sniper) + new: **lancer** (charges in a straight
  line, telegraphed), **brood** (spawns 3 tier-1 larvae on death).
- **Boss hierarchy**: mini-bosses (tier 4) from wave 3 every 2nd wave; a giant boss
  (tier 6) every 4th wave with 2 mini-boss escorts; a **titan** (tier 8, ~1.5 × a
  planet's rendered radius) at waves 12, 24, …: slow, world-shaking, its attacks are
  planet-scale (gravity well pulls YOU and nearby planets; a ram that NUDGES the planet
  it hits off its orbit via engine.nudgeBody, decaying back over 60 s; a beam that
  carves a glowing scar across the terrain patch for 20 s). HP bars for every boss tier.
- `engine.nudgeBody(node, dx, dy, dz)` (galaxy3d.js): per-node decaying world offset
  added in computePositions, so a titan can push planets around for fun.
- Every attack is unique per archetype and telegraphed; damage grows with tier.

## Revision 14 (2026-10-01) — static bosses, real dodge, 8× scale, landing + on foot

### Fixes (ship.js unless noted)
- **Bosses static**: no follow band. A boss parks where it spawned (drift ≤ 0.1 u/s),
  rotates to face you, never approaches. You fly around it. Gravity well is subtle:
  a gentle drift toward it (≤ 15 % of cruise), never a pin, and it ends on its own.
- **Boss size curve**: boss length = refR × clamp(0.05 × wave, 0.15, 1.0) → wave 20 ≈ a
  planet. Titans 1.5 × refR regardless.
- **Bosses look different** (ship-enemies.js): 5 body plans chosen by name seed —
  serpent (long spine, many fins), crab (wide shell, 6 claws), jelly (dome + 12
  tentacles), leviathan (armored hull, spikes, 3 eyes), hydra (3 heads on necks). Each
  with its own flex/sway animation. Name on the HP bar.
- **Allies visible**: they hold a slot 20–40 L AHEAD of you and fly crossing passes
  through your field of view between attacks; name chevrons like players; count
  shown on HUD.
- **Surface stability** (ship-planet.js): the patch samples in the planet's LOCAL frame
  (inverse of mesh.rotation and parent orbit) so it never slides against the painted
  globe; floorAt uses the same transform. Verified by parking on the surface for 60 s
  and measuring drift vs the globe texture (must be 0).
- **Scale**: planets grow ×3 while piloting (PILOT_PLANET_SCALE on renderedRadius for
  root-level planets, blended during the boarding cinematic) and L = refR / 5000 →
  ≈ 8× bigger planets relative to the ship vs rev 13, with no float jitter (the ship
  stays ≥ 0.02 u). Sub-system planets scale ×2 (they sit closer together).
- **Speeds**: cruise 4, boost 40, pulse exponential `v = cruise·e^(t/2.5)` capped at 1500
  u/s (galaxy in ~3 s), drops only on imminent impact. Streaks/exhaust follow.
- **Barrel roll = dodge**: 4 L lateral displacement over 0.45 s on the roll side, with a
  wing-thruster puff (fx.flash at the wingtip + a short side exhaust). 40 % damage cut
  during. Cooldown ring.

### Landing + on foot
- **Atmospheric entry**: inside 1.4 R above boost speed: plasma sheath on the hull (a
  slightly larger hull copy with an additive fresnel fire shader), heat streaks, rumble,
  HUD "ENTRY". Ends when speed < boost.
- **LAND (E)**: available below 12 L altitude over land and speed < cruise: HUD "E LAND".
  Press E: ship auto-levels, descends, legs down, touchdown dust (fx.impact ×3), state
  `landed`. E again: **exit ship** → third-person humanoid (new js/ship-human.js:
  low-poly rig, procedural walk/run/jump, theme color suit, helmet visor glow). WASD,
  Shift run, Space jump, mouse orbit cam; E near the ship = board; T/Enter chat as
  usual. Players on foot are sent over the relay as `{t:'pos', mode:'foot'}` and
  rendered as humanoids by others; you can walk up to each other.
- **Takeoff**: in the ship, W from `landed` → lift-off burst, back to flight.

### Shops / NPCs / weapons (rev 15, right after)
- **Outposts**: seeded per planet (1–3 per planet on land, placed by hashing the
  planet id), a landing pad + a small building + an NPC humanoid. Visible from the
  air as a beacon. Landing on the pad = safe.
- **NPCs**: E to talk → dialog overlay in the chat font; names from the boss name
  generator; one line of seeded personality.
- **Weapons**: procedural (new js/ship-weapons.js): name + stats (dmg, rate, spread,
  projectile count, speed, color/shape, one special: pierce, homing-lite, chain, burn)
  + a tiny procedural model on the hull. Bought with units (earned per kill/boss),
  saved on the profile, `/weapons` lists, shop UI lists 4 seeded per outpost.

### Beauty (rev 16)
- Per-planet biome from palette: sky gradient + sun disc, day/night from LIGHT_DIR and
  spin, grass instancing, bioluminescent night flora, aurora on cold palettes, rings
  seen from the ground, floating rocks on exotic ones, fog color by biome.

### Later (ideas, not scheduled)
Discovery: name a planet/creature on first visit → shared registry via the relay and
announced in chat. Exocraft. Derelict freighters as dungeons. Galaxy events ("titan
sighted near PAPERS"). Leaderboard page on the site. Procedural ambient music per
planet (Strudel/ChucK in-browser, credit the tools). Mobile touch controls.

### Rev 15/16 adds from docs/nms-mechanics.md (decided)
- Rev 15: weapon and ship class letters C/B/A/S; jetpack (hold Space on foot, 2 s fuel,
  regen) + scanner visor (hold V: outposts, discoveries, friends highlighted, 8 s cd).
- Rev 16: discovery registry (first visit claims a planet/creature name via the relay,
  announced in chat, `/name` only works on your own discoveries); friend beacon
  (`/tp NAME` → request, accept → spawn beside them); scheduled weather + aurora events
  announced in chat. Skipped for now: alien language, exocraft, placeable beacons.

## Revision 17 (2026-10-01) — melee bosses, planet throws, parts library, performance

### A. Bosses fight with their bodies (ship.js + ship-enemies.js)
- Every boss spawn is unique: name, body plan, palette, limb set and attack combo are
  all seeded from (wave, spawn counter, relay epoch) — never the same twice.
- Bosses do NOT shoot. They **swing**: each body plan exposes named limbs (claws,
  tentacles, heads, tail) with animated hit volumes (capsules updated from the limb's
  current pose). Attacks are seeded combos of: sweep (limb arcs across 120°), slam
  (limb raises, telegraph, slams down a cone), grab-lunge (head/claw lunges 20 L),
  spin (full-body 360° sweep), tail whip. Telegraph = limb glow + wind-up pose
  0.8–1.5 s; damage 25–45 on contact; knockback 6 L. Escorts (spawned by the boss)
  do the shooting.
- **Hit feedback**: 2× camera shake, red flash at 35 % of today's opacity and shorter.
- **Planet-sized bosses (wave ≥ 20)**: length ≥ 1 refR. They can **throw planets**:
  telegraph 3 s (boss turns to a planet, limbs wrap it, planet glows), then the planet
  is launched along the throw vector at 60 u/s via a new engine `throwBody(node, vec,
  speed)`: the planet leaves its orbit, travels, and if its disc passes through the
  player = instant death (ignores god). After 8 s or on reaching 2× sysR from home it
  eases back to its orbital slot over 20 s (equilibrium). Only one planet in flight at
  a time; sub-planets ride their parent. HUD: "PLANET INCOMING" + arrow.

### B. Parts library (NEW js/ship-parts.js)
A modular procedural asset kit every generator draws from: fins, spikes, plates, pods,
antennae, engines, eyes, tentacles, claws, shells, domes, struts, cockpit canopies,
landing legs, with a tiny grammar `compose(THREE, recipe, rng)` → merged geometry +
emissive geometry + named attachment sockets. Deterministic, mergeable, vertex-colored,
flat-shaded, ≤ N tris per part with LOD variants (hi/lo). Used by hulls, creatures,
bosses, weapons, outposts, humans over the next passes. Goal: everything looks like one
family, more detail per triangle, no hand-built one-offs.

### C. Performance (NEW js/ship-perf.js + galaxy3d.js, ship-fx.js, ship-planet.js)
Target: 60 fps on a Chromebook-class GPU, 120 fps on the M4, same look where possible.
- Quality manager: measures frame time (rolling 2 s), steps a quality tier 0–3
  (DPR cap 1.0/1.5/2, bloom on/off + resolution, star count, patch grid 64/96/128,
  flora/creature counts, exhaust cone segments), persisted per device.
- LOD: enemies/bosses swap hi→lo part variants beyond 60 L; impostor sprites beyond
  200 L; flora instancing already; far creatures skip update.
- Render: one shared material per family (fewer program switches), frustum culling
  on everything except the sky, no per-frame uniform object creation, merged static
  outposts, bolts instanced (done), labels DOM-throttled to 20 Hz.
- Organization: split ship.js along clear seams into modules with one owner each
  (flight, combat, waves, hud, chat, net glue) WITHOUT behavior change, via a
  documented module map in docs/ship-architecture.md. Done last, by one agent, with
  a before/after test.

## Revision 18 (2026-10-01) — planet flight model (guaranteed)

Emory: landing still isn't right; needs a smoother transition, a guaranteed entrance below
the atmosphere, different flying logic with lots of air above the surface, and big,
interesting, GUARANTEED stable surfaces. These are the guarantees, each with a test.

1. **Local-frame simulation.** Inside 1.6 R of a planet the ship (and the human, and
   landed ghosts) are simulated in the planet's LOCAL frame: position/orientation stored
   relative to the planet's anchor and spin quaternion; world pose derived at render
   time (same hook as ship-planet's ground objects). The old "frame drag" is deleted.
   Entering: world→local (subtract planet velocity). Leaving at 1.7 R: local→world
   (add planet velocity). *Test:* park on the surface for 120 s of stepped time: local
   position drift = 0, world position tracks the planet exactly, terrain height under
   the ship constant.
2. **Guaranteed entry.** Crossing 1.4 R inbound at any speed starts ENTRY: speed is
   auto-braked to ≤ boost over 2 s with the burn effect, controls stay live, nothing can
   push you back above 1.4 R during ENTRY. *Test:* 20 random approach vectors at pulse
   from 4 R all end inside 1.4 R, alive unless the dive is steeper than 60° at > boost.
3. **Atmospheric flight model** (inside 1.4 R): speed caps cruise 4 / boost 12 / pulse
   40 (pulse here is a sprint, no ramp); lift: nose follows the horizon unless you
   pitch; altitude HUD (in L); below 3 L the ship HOVERS (no sink, strafe with A/D held
   + no roll); below 1.5 L and speed < 1 → auto "E LAND" prompt. Airspace: with planets
   at 8× scale the shell 1.0–1.4 R is ~2000 ship lengths deep; keep it. Leaving: pitch
   up + boost climbs; at 1.4 R outbound you get a short "LEAVING ATMOSPHERE" shake and
   speeds unlock at 1.7 R.
4. **Surface never clips.** Collision is a swept sphere against the terrain height
   function in the local frame, substepped so a frame can never tunnel (substep when
   displacement > 0.5 L). *Test:* 5 min of random flight inputs at all speeds inside
   the atmosphere: ship radius never below floor + 0.5 L, no NaNs, no teleports (max
   per-frame displacement ≤ speed·dt·1.5).
5. **Terrain continuity.** Patch recenters never pop: height at any world point is a
   pure function of local direction; re-centering only changes which points are
   tessellated. *Test:* sample height at 1000 fixed local points before/after 50
   re-centers: identical.
6. **Interesting surfaces**: relief AMP 0.09 with ridged mountains, 3 biome looks by
   palette (rocky/lush/icy) via slope+height coloring, scattered rocks (instanced, 300),
   outposts as before. Sea is flat and landable-false.
7. **Transition polish**: camera FOV eases from space (42) to atmosphere (50) over the
   entry; fog/sky ramp as before; the orbital globe's atmo halo fades as you enter so
   the horizon is the terrain, not the halo.

## Revision 19 (2026-10-02) — landing you can feel, solid ground, scary bosses, honest blasters

Emory's play report: landing failed on the orange planet; ground was see-through on a
jellyfish planet; never found the way out of the ship; wave 20 spawned 3 bosses on top
of each other that didn't approach; bolts slower than the ship and not hitting bosses;
bosses should be abstract and scary; entry/exit vibration far too strong.

### A. Planet + flight (ship.js, ship-planet.js, ship-planet-tests.js)
1. **Every planet lands or says why.** Terra planets: landable everywhere on land.
   Gas giants: no surface; descending below 1.15 R hits a cloud deck (soft bounce,
   HUD "NO SURFACE · GAS GIANT"). Sea: HUD "WATER · FIND LAND". *Test:* for every
   node in drawOrder: approach, descend, land or get the right message; 0 failures.
2. **Never see through the ground.** Causes to kill: camera near plane vs ground
   (near = 0.02 L in atmosphere/foot), patch rim taper (patch must extend past the
   horizon at every altitude; no taper to 0), back-face/holes in the patch (double-
   sided + a solid "skirt" ring below the rim), creatures/flora rendering over terrain
   with depthTest off (fix), dome/fog drawn in front of terrain (depth order). *Test:*
   from 200 random ground poses, sample 64 screen pixels in the lower half; the
   fragment must be terrain or an object on it, never sky/space (readback via a tiny
   render target with an id pass, or depth > horizon check).
3. **Getting out is obvious.** While hovering under 3 L with speed < 2, a big centered
   prompt "E · LAND". Press E: legs deploy, 1.2 s settle, dust, engine wind-down,
   camera drops to a low 3/4 view, HUD "LANDED · E EXIT SHIP · W LIFT OFF". E → on
   foot with "E BOARD" when near the ship. *Test:* scripted: hover → E → landed → E →
   foot → walk 20 L → back → E → landed → W → flying, on 3 planets.
4. **Entry/exit shake at 15 %** of current amplitude; same for LEAVING ATMOSPHERE.
5. **Blasters**: bolt velocity = ship velocity + muzzle speed (never slower than the
   ship), infinite range (life 20 s, culled beyond 3000 L), swept hit test each frame
   against every enemy/boss body sphere + boss limb capsules + eyes (eye = ×2).
   *Test:* at pulse 1500 u/s, bolts lead the ship; parked 50 L from a wave-8 boss,
   100 bolts → ≥ 90 register.
6. **Bosses in formation, slowly closing**: multiple bosses spawn on an arc 120–180 L
   out, spaced ≥ 1.5 boss lengths, hold formation (lead + wings), and close toward
   the player at 0.3 u/s until inside melee reach, then hold. Never overlap: pairwise
   separation enforced.

### B. Abstract, scary bosses (ship-enemies.js, ship-parts.js)
New body plans replace the animal ones as the default pool (old plans stay as rare
rolls, 1 in 6): **wraith** (a drifting cloud of 40–80 obsidian shards around a single
slit eye; shards snap into a blade for strikes), **monolith** (a black obelisk with
3 slowly counter-rotating rings and a vertical eye; strikes are ring slams),
**maw** (a ring of teeth around a void, tendrils trailing; lunge = the ring opens),
**hive** (a cluster of pulsing dark orbs linked by filaments; sweeps are filament
whips), **seraph** (a vertical stack of 6 rotating fin-wings around a core eye, no
body). Palette: near-black bodies, one saturated emissive accent (violet/ember/acid),
subtle flicker/glitch on the emissive, slow idle motion with sudden snaps on telegraph.
Same limb/capsule/move contract. Names unchanged (they're loved).

## Revision 20 (2026-10-02) — scale again, visible damage, sound, everything landable

### A. Engine (galaxy3d.js)
- PILOT_PLANET_SCALE root ×6, deeper ×4 (was 3/2); pilot orbit layout already derives
  from rendered radii, so moons/rings spread accordingly. Minimum pilot rendered
  radius for ANY body = 320 L (L = ship length, provided by engine.setShipLength(L)
  from ship.js) so moonlets become real worlds.
- **Floating origin** while piloting: render with the scene translated so the camera
  is near (0,0,0) (scene.position = −cameraWorld each frame before render, camera at
  the residual); all game math stays in true world coordinates; labels/projection use
  the true camera. Removes far-from-origin jitter at the new scale.

### B. Ship + combat (ship.js, ship-enemies.js)
- L = refR_pilot / 12000. Speeds unchanged in u/s (so in ship-lengths everything is
  faster; pulse cap 1500 stays). Atmosphere caps unchanged.
- **Every planet landable**: no gas-giant deck; ps gives gas giants a cloud-world
  surface; moonlet rule only below 60 L (nothing is that small after the floor).
- **NMS exit**: hold E (0.6 s, radial fill on the HUD) while under 6 L altitude → the
  ship auto-lands and you step out in one motion; tap E still lands; E near the ship
  boards; hold E on foot = board + lift-off.
- **Visible damage**: hit marker (reticle X flash) on every registered hit; floating
  damage numbers (pooled DOM, 24, ×2 crits on weak points in the accent color); boss
  HP tuned so sustained fire kills a boss in ~90 s at its wave (HP = 90 s × player
  DPS × 0.8); limb damage: each limb has its own HP (25 % of boss HP); at 0 the limb
  is DESTROYED (ship-enemies: setLimbDestroyed(i) hides its parts, disables its
  capsule, removes its moves) with a big burst + 1.5 s stagger (boss can't attack);
  destroying all limbs exposes the eye at ×4. Enemies flinch on hit (exists), and
  die with a chain-reaction burst scaled by size.
- **Loot**: bosses drop a weapon pickup (procedural, from ship-weapons; glowing crate,
  fly through to equip; shown in chat) and 3 shield orbs; regular kills 10 % chance.
- Audio hooks: fire, hit, crit, kill, limbSever, bossRoar (on spawn + telegraph),
  explosion, engine (throttle/boost/pulse), entry, land, liftoff, ui (prompt/chat).

### C. Audio (NEW js/ship-audio.js)
Procedural WebAudio, zero files: `createAudio()` → `{ unlock(), play(name, opts),
engine(state), setMaster(v) }`. Synth recipes per hook (noise bursts + filtered saws
for lasers, FM thumps for hits, layered noise + sub for explosions, a slow granular
drone for engine that follows throttle/pulse, a sub-growl for boss roars, soft clicks
for UI). Starts after the first click (autoplay rule); `/volume 0-10`. Spatialized by
distance (gain) only; no reverb beyond a cheap feedback delay on explosions.

### D. Planets (ship-planet.js)
- Gas giants: surface = cloud world (height from the same noise, palette bands, no
  trees/rocks, floating "spore" creatures), landable everywhere.
- Scale: patch/flora/rocks/outposts re-tuned for L = refR/12000 (trees 4–8 L still).

## Verification checkpoint (2026-10-02, PAUSED by Emory)

Rev 20 is fully in the working tree, uncommitted. The guarantee-suite agent was paused
mid-run. Its code is in js/ship.js, js/ship-planet.js, js/ship-planet-tests.js,
style/ship.css and parses clean. Real bugs it fixed: stale EDGE_R boundary (shoved the
ship toward the origin over outer worlds → drift, empty prompts, "embedded" engages,
g4 teleports; now edgeFit()), pulse approach stalling against fast orbits (planet-
relative thrust inside 3 R), bolt cull at 3000 L (now 12000 L). Added: pulse starts at
boost instantly (τ 1.5 s to 300), HUD titles removed except boss names/prompts, foot
controller = substepped capsule on ps.meshFloorLocal (exact rendered triangles), g14.

Resume = the same agent (transcript retained) or a fresh one with this list:
1. Un-hide `.sh-stats` (shield/alt/speed/wave readouts stay; only banners go).
2. Full g1–g14 run with 60 s-per-site g14; rerun g4 on several seeds (one run showed
   15 teleports, four later seeds showed 0 — unresolved).
3. Screenshots: terra planet filling the frame from 3 R; gas giant at 0.8 L; two
   standing-still frames one step apart (identical).
4. Confirm Esc via the engine pointer-lock path.
5. Close Chrome tab ?verify20, then commit + push (pull first; emgor-fa pushes often).

## Revision 21 (2026-10-03) — make it FUN: analysis + plan

### Analysis (why it isn't fun yet)
1. Feedback arrived late (damage numbers + audio only in rev 20, untested by a human).
2. Enemies never communicate intent or vulnerability → spraying at dots.
3. No mid-fight decisions: one weapon, no heat, no priority target, no cover, no chosen risk.
4. Fights happen at the wrong distance for the new planet scale.
5. Boss limbs can be severed but there is no reason to choose a limb.
6. Nothing to chase: no currency, shops, upgrades.
7. On foot has nothing to do.
8. Space between fights is empty; no music.

### A. Combat core (ship.js)
- Squads of 3–5 with a marked LEADER (bigger chevron, bonus units); engage ≤ 80 L; a
  squad holds a loose formation and splits on the leader's call.
- Every attack run: wind-up 0.7 s (eye flare + rising audio) → strike → STALL 1.0 s
  (enemy glows, takes ×2 damage, HUD shows "STALLED" over it).
- Weapon heat: 0–1, +0.07/volley, −0.35/s cooling; at 1.0 OVERHEAT 2 s (no fire,
  steam fx, audio); the reticle ring fills with heat. Tap fire = cool rhythm.
- Ram tool: Shift+LMB while boosting = RAM: damage = 40 + speed × 2 to the enemy,
  12 to you, 0.8 s cooldown, big shake; replaces the 45 HP punishment for intentional
  rams (accidental contact stays costly when not boosting).
- Lock-on evasion: an enemy you keep in the 2.5° cone > 1 s jinks (one-off dodge roll).
- Boss: each limb OWNS one attack; the HP bar shows limbs as pips with their attack
  icon; severing a limb removes that attack (exists) — now telegraph which limb is
  about to strike on the bar.
- Director: 60 s tension curve per wave (exists) + "quiet" waves every 5th with a
  single elite squad.

### B. Enemy roles v2 (ship-enemies.js + ship.js)
- hunter (dives), harasser (circles at 40 L, pot shots, breaks off when shot),
  bomber (slow, 150 % HP, drops 3 mines on a pass, high units); 25 % of enemies carry
  a BACK SHIELD (front immune; the glowing back is the weak point) — visual: a
  translucent dish on the front.
- Enemies use planets: hunters dive into atmosphere to shake you (exists), harassers
  hide behind moons.

### C. Economy + stores (ship.js, ship-planet.js, NEW js/ship-world.js)
- Units: kills 10×tier, leader +20, boss 500×tier, shards on planets 25 each
  (glowing crystals, 20 per planet near outposts, respawn daily), saved on profile.
- **7/11 on every planet**: every outpost gets a store: low-poly convenience store
  with a lit "7/11" sign (one string constant STORE_NAME — trademark, rename any time),
  glass front, glowing interior, a clerk NPC humanoid behind a counter, parking pad.
  Every planet gets ≥ 1 outpost (ps.outposts min 1, placed on land).
- Store UI (chat font overlay, E at the counter): 4 procedural weapons (class-tagged),
  shield upgrade (+20 max, 3 tiers), engine upgrade (+10 % cruise/boost, 3 tiers),
  snack (full heal, 50 u), sell owned weapons at 40 %. Clerk lines: seeded personality,
  3 lines, name from the boss name generator.
- Crates/orbs/shards all show units gained as floating text.

### D. On foot v2 (ship.js, ship-human.js)
- Jetpack: hold Space = infinite, fast (climb 3 heights/s, forward thrust 2× run);
  flame + audio; landing crouch. Run ×2. Scanner V: pulse ring, highlights stores,
  shards, friends, outposts for 6 s. Shards: walk/fly into them. E at the counter →
  store; E near an NPC → 1 line in chat.

### E. Ambient (NEW js/ship-space.js, ship-audio.js)
- Asteroid belts: 2 instanced belts between root orbits (~1200 rocks each, LOD),
  real collision (soft), cover (enemies lose lock behind rocks ≥ 4 L).
- Freighters: 1–2 slow NPC haulers crossing the system (parts kit 'hauler' ×30).
- Music: procedural ambient (WebAudio): 2-voice drone + slow arps, key per planet
  palette, intensity follows combat state; `/volume` covers it.

## Revision 22 (2026-10-03) — the 7/11 space station + a real black hole

### A. Black hole in flight (galaxy3d.js)
- Pilot scale for the black hole: core ×8 (disks/arcs/halo scale with it as they do),
  blended with pilotBlend; galaxy view unchanged.
- Solid: `engine.blackHole` → { pos, coreR (pilot), diskR } so ship.js can collide:
  hard sphere at 1.3 × coreR with a strong push-out + 35 HP; inside 3 × coreR a
  gravity pull (max 0.25 × cruise) and lensing shimmer (existing shader, boosted).
  Never fly-through-able.

### B. Station (NEW js/ship-station.js + ship.js docking)
- One NMS-style space station orbiting the black hole at ~2.2 × coreR (pilot), hidden
  in the galaxy view, huge in flight (length ≈ 0.8 × refR): a long spine with a
  hangar mouth (open rectangular bay facing outward, lit by landing strips), ring
  habitat, antennas, docking lights, the 7/11 sign on the mouth. Parts kit + merged.
- Interior: hangar deck with 4 landing pads, walkways, a 7/11 counter with a clerk,
  2–3 idle NPC humanoids, a weapons vendor (same inventory system as planet stores,
  tier +1), a galactic-map pedestal (opens the galaxy overlay = engine.focusNode root
  with Esc semantics), windows showing the black hole.
- **Auto-dock**: fly into the mouth trigger volume (speed any; governor brakes) →
  control taken, ship follows a spline to a free pad, legs down, "DOCKED", player is
  put on foot beside the ship on the deck (deck = floor plane; interior collision =
  simple boxes). E near the ship → board → ship auto-launches out of the mouth on the
  reverse spline, control returns at the mouth. Chat/commands work inside.
- Multiplayer: other players docked show on their pads; on foot in the station they
  render as humans (same net path as planets with a 'station' frame id).

## Revision 23 (2026-10-03) — INTERGALACTIC 7/11, items, inventory, Burger House, lingo

Priority: it must FEEL good to interact with. Chill and funny. All text and items come
from seeded sentence/word recombination (no model calls, tiny CPU).

### A. Vision effects (galaxy3d.js) — NEW post pass
`engine.vision.set({ blur, chroma, hue, wobble, double, contrast, invert, tint, fov,
timeScale })` (all 0..1 except hue in turns, timeScale 0.85–1.15) → one fullscreen
shader pass after bloom (cheap: single-tap blur via mip, chromatic offset, hue rotate,
sine wobble, double vision offset, contrast, invert flash, color tint, slow FOV
breath). `engine.vision.clear()`. Zero cost when all params are 0 (pass skipped).

### B. Items (NEW js/ship-items.js)
`generateItem(seed)` → { id, name, kind ('food'|'drink'|'snack'|'fries'), base (7-11
food: Gardetto's, Big Gulp, taquito, Slurpee, hot dog, pizza slice, donut, energy drink,
nachos, jerky, corn dog, monster, chips…), infusion (a long comedic list of fictional
and real drug names, e.g. "crack-infused", "fent", "ketamine", "shroom", "DMT",
"adderall", "lean"…), modifiers ("double", "ultra", "limited edition", "expired",
"blessed"), price, blurb (1 line, funny, recombined), effect: { duration 15–240 s,
params for engine.vision (a seeded subset with amplitudes), extras: speed ×, jump ×,
chat-font wobble }, color }. `storeMenu(storeId, n = 24)` → scrollable list;
`FRIES` = Burger House fries: effect = glow light blue 45 REAL minutes (persisted as
an expiry timestamp on the profile), no vision effect.

### C. Lingo (NEW js/ship-lingo.js)
Seeded alien lexicon per planet/station (syllable generator), sentence templates
recombined from word lists (greeting / sales pitch / gossip / warning / lore / drug
dealer patter / cashier line), rendered as a mix of translated words and alien words;
the player's "known words" set (saved on the profile) grows by buying and talking, so
NPC lines become more readable over time (NMS language mechanic). `line(npc, ctx)`
→ string; `name(seed)`; `translate(word)`.

### D. World (ship-world.js, ship-parts.js)
- The INTERGALACTIC 7/11: cartoon-accurate but oversized: wide aisles, very high
  ceilings (jetpack on top of aisles), shelves full of instanced item boxes in item
  colors, slurpee machine, coffee station, cashier counter, neon sign
  "INTERGALACTIC 7/11", glass front, parking pads; NPC shoppers wandering the aisles
  (3–6 humans, seeded names/lines), DRUG DEALERS loitering outside (2, distinct suit
  colors, patter lines, their own 6-item menu), funny characters (seeded roles:
  conspiracy guy, retired pilot, kid with a ship toy, cop who doesn't care).
  A few per planet (= per outpost) + the station one (bigger).
- BURGER HOUSE: one per planet, Dallas Burger House look (research it: a small 1951
  Dallas burger stand; use the vibe: red/white, retro sign, walk-up window), serves ONLY
  fries; the fries clerk has lines about the seasoning. Buying fries = glow.

### E. Inventory + eating (ship.js)
- Q at ANY time: infinite Minecraft-style inventory: a grid of slots = owned items
  (icons = colored item boxes with the name on hover), scroll, chat font; right-click
  an item = eat/drink: the effect timeline starts (engine.vision + extras), stackable
  effects blend, HUD shows active effects with timers; items persist on the profile.
- Store UI: scrollable menu (24+ items), buy with units, sell back 40 %.
- Fries glow: human + hull get a light-blue emissive + glow sprite until the expiry.
- NPC lines use ship-lingo; chat rows show alien words in a different color; buying
  teaches 1–3 words.

### Keybinds (locked 2026-10-03)
E = inventory (Minecraft). F = interact: tap F land, hold F land + step out, F exit ship,
F board, F talk, F shop, F map pedestal. Q = focus slow-time, Z = allies focus, V =
scanner, T = cycle target, Enter / `/` = chat, Space = pulse (ship) / jetpack (foot),
Shift = boost / run, A/D double-tap roll, S double-tap flip, Ctrl drift, Esc = galaxy.

## Revision 24 (2026-10-04) — resume where you were, land/water balance, clean entry, speeds, guidance, Minecraft inventory

1. **Esc resumes exactly**: leaving the ship (Esc) freezes the ship's pose in its CURRENT
   frame (planet-local if inside 1.6 R, station-local if docked, world otherwise) and
   re-entering restores that exact pose relative to the planet/station even though it
   moved meanwhile. Landed stays landed, on foot stays on foot (human pose too).
2. **Land/water balance** (ship-planet.js): per-planet land fraction target seeded in
   0.45–0.8; sea level solved against the height histogram so the fraction holds; small
   planets (< 600 L radius) and any planet with a single outpost are ALL land. Outposts,
   7/11s, Burger House always on land.
3. **Clean atmosphere entry**: the terrain patch's colors must EXACTLY match the painted
   globe (same palette function, same noise, same light), fade-in over 2.4 → 1.8 R with
   no visible seam; dome/haze capped at alpha 0.45 and only thick near the top; at the
   surface the sky is clear enough to see the horizon crisply; landmass shapes read the
   same from orbit and from the air (no "weird" relief pop: relief amplitude ramps in
   with altitude so the silhouette stays the globe's).
4. **Speeds**: Shift boost 60 u/s; Space pulse ramps to 450; Space + Shift ramps to 675;
   cruise 4. Pulse ramp τ 1.5 s (starts at boost speed). Governor unchanged.
5. **Guidance**: inside 2.5 R of a planet, NMS-style HUD markers for the nearest 7/11
   and Burger House (icon + name + distance in L), on-screen when in view, edge
   chevrons when not; also the outposts' pads. Same for the station mouth in space
   within 3 × coreR.
6. **Minecraft inventory**: exact Minecraft look: dark semi-transparent grey panel with
   the light/dark bevel, 9-column slot grid, 36 slots + a 9-slot hotbar row, slot
   hover highlight, item tooltip (name, blurb, effect) in the Minecraft font, stack
   counts bottom-right, right-click eat, left-click equip (weapons). Esc closes the
   inventory (and any menu). Holding Esc for 3 s (radial fill) exits to the website
   galaxy; a tap no longer exits.
7. **Pixel icons** (NEW js/ship-icons.js): procedural 16×16 pixel art per item from
   its base food/drink (cup, bag, hot dog, slice, donut, can, bottle, box, jerky strip,
   taquito…) with infusion tint, modifier badge, dithering; weapons get a gun silhouette
   in their color; cached canvases, drawn at 3× with image-rendering: pixelated.
8. **Playtests**: a subagent plays scripted sessions (space, approach, entry, landing,
   on foot, store, station) and writes docs/playtest-N.md with screenshots + a ranked
   punch list of look/feel problems; the next pass fixes the top items.

## Roadmap to a real game (2026-10-04) — what's missing, in order

The game has places and enemies but no REASONS. Everything below gives reasons.

### Tier 1 — the loop (do first)
1. **Stacking inventory as the spine**: every pickup is an item with a stack (shards,
   scrap from kills, boss cores, seeds, fuel cells, foods). Weight/slots don't matter;
   stacking + sorting + tooltips do. Selling/buying/crafting all read the same stacks.
2. **Resources on planets**: crystal nodes, plant pods, rock veins (instanced, scanner-
   visible), harvested on foot with F (3 s channel + chunks flying in) → stacks.
   Each biome yields different stuff. This is the "why land here".
3. **Crafting at the 7/11 counter**: 15 seeded recipes per store (seasoning salt,
   fuel cells, hull plates, bolt cores) from stacks → upgrades and sellable goods.
4. **Bounties and deliveries (adventures)**: the station board + any NPC can hand a
   quest: deliver N of X to planet Y's 7/11, kill a named boss, scan 3 creatures on
   planet Z, retrieve a crate from the belt, escort a freighter. Seeded, 3 open at a
   time, rewards: units + a rare item + lingo words. This is 80 % of "things to do".
5. **Discoveries**: first landing / first creature scan names it; the registry is
   shared via the relay; your profile lists them. Scanning (V) gets a payout.

### Tier 2 — progression and identity
6. Ship upgrades with visible parts (engine, wings, pods from the parts kit) bought or
   crafted; `/ship` choice among hauler/fighter/explorer; stats matter (cargo stacks,
   speed, guns).
7. Weapon mods (slots: scope/coil/chamber) instead of only buying new guns.
8. Suit upgrades for foot: jetpack boost, scanner range, sprint, storage.
9. Rank/title from bounties shown on your ghost's label.

### Tier 3 — the world feels alive
10. Freighter convoys you can trade with or pirate (choice → reputation with the two
    factions: 7/11 Corp vs the Dealers).
11. Random events announced in chat: titan sighting, meteor shower on planet X (shards
    everywhere for 5 min), fries sale, pirate blockade at the station.
12. Creature behavior: herds, predators, a tameable one (feed it fries).
13. Weather on planets: storms that push the ship, auroras, night glow.
14. Derelict freighters as mini-dungeons (interior boxes, loot, a guardian).

### Tier 4 — polish that sells it
15. Music states per place (space/atmo/store/station/combat) — exists partly.
16. Photo mode + share link (fits the portfolio site).
17. Onboarding: the first 60 s teach fly / land / shop / fight with 4 prompts, no text
    walls.
18. Mobile: touch sticks + land/shop buttons (phones are currently gated out).
19. Save/profile on the relay so a user follows you across devices (`/user NAME KEY`).

Not doing yet: base building, procedural quests with branching dialog, PvP factions.

## Revision 25 (2026-10-04) — gorCoin, crafting, your ship is your base, creatures, crew

### A. gorCoin
Units are renamed **gorCoin** everywhere (HUD "ɢ 1,240", chat, stores, lingo: NPCs
mention gorCoin by name in pitches/gossip). Earned: kills, bounties, missions, selling.
Determines what you can buy. Profile field `gor` (migrated from `units`).

### B. Crafting (NEW js/ship-craft.js)
- **3×3 crafting table** (Minecraft style) inside your ship and at every 7/11 counter.
  Drag/click stacks from the inventory into the grid; the output slot shows the result.
- **Parts catalog** sold at 7/11s: flux capacitor, servo, coil, plating, lens, battery,
  gyro, antenna, cooling fin, chip (seeded prices in gorCoin).
- **Recipes**: a hand-authored base set (≈ 40: seasoning salt, fuel cell, hull plate,
  bolt core, scope, coil mod, chamber mod, shield cell, engine kit, jetpack booster,
  creature treat, pen kit, crew pod kit…) + a **combination engine** for everything
  else: `combine(stacks[])` is deterministic: it merges ingredient tags (food,
  infusion, part, tier) into a new item with tier+1, blended effects, a generated name
  ("Flux-Capacitor Fent Burger Mk II"), and a category inferred from the dominant tags
  (weapon mod / ship upgrade / food / junk). Crafted things craft again, forever.
- Outputs: weapon mods (3 slots per weapon: scope/coil/chamber), ship upgrades
  (engine/shield/cargo/jetpack tiers with visible parts), foods, junk (sellable).

### C. Your ship is your base (NEW js/ship-interior.js + ship.js)
- From on foot beside the ship (landed or docked) press F at the hatch → the INTERIOR:
  a freighter-scale space (~10× the ship, NMS freighter vibe) built from the parts kit:
  cockpit (a window onto the real sky), cargo hold (your stacks shown as crates),
  the crafting table, a kitchen counter (eat), **creature pens** (glass pods), **crew
  pods** (bunks), a mission board, a hatch back out. Walk/jetpack inside; same foot
  controller (floor + box walls). The interior lives in a pocket location far from the
  galaxy (floating origin makes it free) with the outside sky still visible through
  windows (the real scene stays rendered). Multiplayer later: friends can visit.
- Flying still feels exactly the same; the interior is only ever entered on foot.

### D. Creatures as pets
On foot, F near a planet creature while holding fries/treats → tame (3 s channel) → it
becomes a stack item ("Jelly of Papers") → in the interior, F at a pen → it lives there
(animated, idle, named, happiness grows with feeding). Pens: 4, upgradeable.

### E. Crew (Burger House recruits) + missions
Every 3rd Burger House visit a seeded NPC offers to join (name, role: pilot/miner/
chef/scout, lingo lines, a portrait color). Crew live in pods in your ship. The mission
board lists 3 seeded missions (mine X on planet Y, scout the belt, trade run, bounty):
assign a crew member → mission runs in REAL time (3–20 min, persisted timestamps) →
returns gorCoin + resource stacks + sometimes an item; the crew member's rank rises.
NMS freighter expeditions, basically.

### F. Order of work
1. Push rev 20–24 (stable) so the station/NPCs/stores are finally live.
2. ship-craft.js + ship-interior.js + gorCoin lingo (parallel, new files).
3. ship.js wiring: gorCoin, crafting table UI, interior enter/exit, pens, pods, board.

## Revision 29 (2026-10-06)
1. **Orbits freeze in the game** (galaxy3d.js): while piloting, every body's position
   is computed at a fixed orbital time T0 (constant, same for every client, so
   multiplayer stays consistent) → planets stand still in the game like real life; the
   galaxy view keeps its drift. The boarding/exit cinematics blend between the live
   layout and the T0 layout. Spin stays (slow). This removes the "can't catch a planet"
   problem and the frame-drag/stale-position class of bugs.
2. **7/11 + Burger House redo** (ship-world.js): correct colors/shapes (7/11: white box,
   orange/green/red stripe band, red lettering on white sign, glass front with door,
   flat roof with the sign tower; Burger House: 1950s stand, red/white, angled canopy,
   walk-up window, neon script), interior scale ×2.5 (aisles you can walk between, tall
   ceilings for the jetpack), cleaner geometry.
3. **Foot movement** (ship.js): walk = today's run speed; run (Shift) = 2×; never
   through the ground (re-run g14 after). **Jetpack flight**: hold Space 2 s → FLIGHT
   mode on foot: orientation follows the mouse like the ship (pitch/yaw), W = forward
   thrust, Space = climb, Shift = fast; release → glide down; landing crouch. Terrain and
   wall collision as a capsule, substepped.

## Revision 30 (2026-10-06) — one game: character styles live in the Minecraft menu
- Everything from docs/mashup-plan.md and docs/smash-plan.md is IN the main game: you
  step out of the ship into it. GOR BRAWL (smash.html) stays as the standalone page but
  the same engine runs the proximity fights in-world.
- **Inventory = accurate Minecraft survival inventory**: check the real layout: 176×166
  panel at GUI scale, the player-preview box top-left with the model turning to follow
  the mouse, 4 armor slots + offhand, the 2×2 craft grid + arrow + output, 27-slot main
  grid + 9-slot hotbar, slot bevels and the exact grey (#C6C6C6 face, #373737/#FFFFFF
  bevels), tooltip style, the recipe-book toggle button. Ours adds a **CHARACTER tab**
  (the preview box becomes the character panel): the current style's name and pixel
  portrait, left/right arrows to switch styles (MARO/JOSHI/STEEV/SONIK + the rest once
  built; names to be finalized), each with 3 lines of what the style does, and a
  "fight stance" preview. Switching re-skins the human rig and swaps the movement
  module in place (no reload). The hotbar stays visible on the HUD like Minecraft.

### Multiplayer is a requirement, not a feature (2026-10-06)
Every new system ships with its two-window test or it doesn't ship:
- Character style rides in `hi`/roster and `pos` (1 byte) → others see your re-skin and
  animations; switching broadcasts.
- Proximity fights: the FIGHT prompt is relayed; both accept → the smash netcode (host =
  the challenger, 2-byte inputs, 30 Hz snapshots) over an `sm_*` room; spectators nearby
  see the 2D fight as a floating billboard in 3D; results broadcast.
- Mobs, hubs, villages: per-planet HOST ELECTION (lowest id present = host; handoff on
  leave) — the host simulates mobs/hub waves and sends 10 Hz snapshots; everyone sees
  the same creeper. Villages are static seeded (already shared).
- Pets, crew, glow, suit tier, hull signature, held item: all in the roster.
- Relay: add `sm_*` rooms + per-planet `mobs` channel; rate limits raised for fights.
- Test plan per pass: two Chrome windows, both land on the same planet, both see the
  same mobs and each other's style; one challenges, the other accepts; fight; results.
