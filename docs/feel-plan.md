# FEEL PLAN — why NO MANS GOR feels bad, and the audit + fix order (2026-11-06)

Owner verdict after playing 3e8d68b: "feels like shit, feels vibecoded, clunky." This doc is the plan to fix that. It is NOT a feature list. Features are frozen until the feel bar below is met.

## Diagnosis (what "vibecoded" means here, concretely)

1. **Every system was built to pass a harness test, not a hand.** g-tests check numbers (drift 0, clearance ≥ 0.8 L). Nothing checks "does the input feel immediate", "does the camera settle", "does the foot stop when I release". The tests pass; the game feels wrong. That is the root failure mode.
2. **No single owner of feel.** ~25 modules, 10k-line ship.js, 12 character styles, pokemans, brawl, villages, stations, touch, photo mode, relay. Each bolted on by a separate agent run with its own constants. Nothing was ever tuned against the previous thing. Result: 5 camera behaviours, 3 input models, 2 floor sources, 4 HUD languages (now 1).
3. **Latency and smoothing stacked.** Velocity lerps + camera lerps + FOV lerps + patch recentres + quality-tier swaps all add their own lag. Tap-to-motion is probably 3-6 frames; camera adds more. Nobody measured it.
4. **Collision is approximate everywhere.** Capsule vs analytic floor, walls as boxes via objPush, store slabs seated by sampling, pad discs, 1.5 H ramps. Each is "mostly right", so the player sees pops, slides, walk-throughs (store apron), clipping into store boxes while flying (g14 flight).
5. **Scale was wrong for a year and got patched by constants.** Walk 22 H/s → 6; H_RATIO 0.2 → 0.27; FOV 42 → 55; cameras moved. Each patch fixed one complaint and shifted another ("all zoomed in").
6. **No core loop.** What does the player DO in the first 3 minutes? Land, walk, find 7/11, buy ball, catch. Each step has friction (prompts, blocked throws, fleeing creatures, store floor) and no reward beat. It feels like a tech demo because it is a sum of demos.

## The bar (feel acceptance criteria; measurable, with a hand on the keys)

| # | Criterion | Measure | Target |
|---|---|---|---|
| F1 | Input → visible motion (foot) | frames from keydown to ≥ 1 px displacement at 60 Hz | ≤ 2 |
| F2 | Release → stop (foot) | frames to < 5 % of walk speed | ≤ 6 (SM64: ~5) |
| F3 | Jump responsiveness | keydown → leaves ground | ≤ 1 (jump-squat only in fighter) |
| F4 | Camera: no overshoot, no pop | max angular velocity spike on mode change | 0 pops; mode blends ≤ 0.4 s, monotone |
| F5 | FOV | fixed per mode | galaxy 42, ship 50 (+≤ 12 boost), foot 55; nothing else writes it |
| F6 | Ground truth | feet vs visible surface anywhere walkable incl. aprons, doorsteps, discs | ≤ 0.03 H, never walk-through |
| F7 | Frame time | 95th percentile, MacBook, village in view | ≤ 8 ms |
| F8 | First 3 minutes | land → 7/11 → ball → catch, no console, no dead prompt | every step has a visible prompt and a reward beat (sound + HUD + coin/dex) |
| F9 | Fighter | hand test by owner vs hard CPU | "feels like the reference" on 5 moves: dash, short hop nair, f-smash, shield grab, up-B recovery |
| F10 | Nothing half-shipped | every key on H panel does what it says | 0 dead/placeholder features visible |

## Audit (one Sonnet reader per row, read-only, measured in Chrome with the harness; ≤ 10 min browser each)

A. **Input latency + motion curves** (F1-F3): instrument footStep with a frame counter; log keydown→displacement; dump the accel/decel curves vs SM64 (accel 0.0 → max in ~4 f, friction stops in ~5 f). Deliver: numbers + the exact lerp/accel lines to change.
B. **Camera + FOV** (F4-F5): every camera write in ship.js/ship-foot.js/smash-world.js/galaxy3d.js; one state machine proposal. (The zoom-in fix in flight is step 0 of this.)
C. **Collision truth** (F6): walk a scripted path through village, store, apron, doorstep, CVS, hub, station deck, pad disc edge, logging feet vs meshFloorLocal vs solids; list every walk-through / pop with coordinates. Includes the store apron walk-through the owner saw.
D. **Perf** (F7): Chrome performance profile in a village: tris, draw calls, per-frame allocations (heap sawtooth), the villager rig cost (1.6-2.4k tris each), patch rebuild spikes, tier swaps.
E. **Core loop walkthrough** (F8): a fresh profile, scripted exactly as a new player: where does it stall, which prompt is wrong, where is the reward beat missing. Output: a 12-step storyboard with the fix per step.
F. **Fighter hand-feel** (F9): owner plays 5 minutes vs hard CPU and dictates one line per move; builder maps each line to frame data.
G. **Dead-feature inventory** (F10): every key/command/prompt/panel; mark working / placeholder / broken. Placeholders get removed from the H panel or finished.

## Fix order (after the audits, sequential, one builder per step, owner hand-test between steps)

1. Camera/FOV state machine (B) — done first because every other judgement depends on seeing right.
2. Input/motion curves (A): SM64 accel/friction/turnaround on foot for every style; jet and dive on the same curve model.
3. Collision truth (C): one floor query (`groundTruth(dir)` = max(terrain-with-pad, nearest structure floor)) + one solid set; delete the ramp hacks; walk-through = test failure (extend g22 to aprons/doorsteps/disc edges).
4. Perf (D): villager LOD, shared geometries, no per-frame allocs; frame budget enforced by a node test over the allocation profile where possible.
5. Core loop (E): first 3 minutes storyboard: ship intro → landing prompt → walk → 7/11 glow + marker → buy ball (coin beat) → creature nearby highlighted → throw → catch (dex beat) → fight an NPC → win (coin beat). Remove everything from the first 3 minutes that isn't that.
6. Fighter (F): frame-data retune from the owner's lines; CPU parity re-run.
7. Dead features (G): cut or finish.

## Rules for this phase
- No new features. No new styles, no new structures, no new modes.
- Every step ends with the owner at the keyboard for 2 minutes and one line of feedback, before the next step starts.
- Every feel criterion gets a harness check (ship-planet-tests.js gF1..gF8) so it cannot regress silently.
- One mechanism per job: when a fix adds a second path for the same thing, the old one is deleted in the same commit.
