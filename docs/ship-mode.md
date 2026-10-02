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
