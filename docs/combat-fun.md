# NO MANS GOR — what makes ship combat fun (2026-10-01)

Scope: complements docs/combat-research.md (criticisms + top 5, already decided in rev 9b) and rev 9/9b/10 of
docs/ship-mode.md. Nothing below repeats lead pip, enemy roles, dodge roll, boost-drift, spawn warning, hit-stop,
flee, or the do-not list. Source note: web search confirmed only the broad strokes (links at bottom);
per-game "ONE mechanic" calls are established design knowledge, numbers are starting values to tune by ear and hand.
Units: L = ship length, u = world units. Existing speeds: cruise 15 L/s, boost 50 L/s.

## 1. Per-game: the one mechanic, why, how to build it here

| Game | ONE mechanic | Why it works | NO MANS GOR (3 lines) |
|---|---|---|---|
| Star Fox 64 | Charge shot that paints multiple locks | Risk/reward in one input: holding fire exposes you (slower, cannot spam), release pays off big. Fire becomes a rhythm, not a held button. Player picks the targets, so skill = sweeping reticle | Hold LMB 0.8 s: reticle ring fills, enemies swept by reticle (within 6 deg, <= 4) get a lock tick. Release: one homing bolt each, 2.5x dmg, speed 80 L/s, steer rate 90 deg/s (dodgeable by interceptors). While charging: fire rate 0, turn rate x0.7, so it costs you |
| Star Fox 64 (all-range) | Wingman in peril ("get this guy off me") | Gives a rescue objective inside the dogfight; stakes are social not numeric, so players care without HP bloat | Allies get a `tailed` flag when an enemy sits in their 6 for 2 s: HUD callout + ally marker pulse. Kill the tailer within 10 s: ally dies otherwise, saved ally drops a shield orb. Uses existing ally/enemy targeting only |
| Rogue Squadron | Scripted set-piece beats with a named objective | Each wave is a small story ("protect the transports") so the goal is not just "kill N". Variety of verbs from the same controls | Wave types: kill-all (default), escort (a slow ally hull that must survive 40 s), defend-point (a planet beacon takes hits). Each is a flag on wave spawn; same enemy AI, different fail condition. 1 in 4 waves after wave 3 |
| Ace Combat | Missile dance + high-G brake-turn | Defense is a skill: you out-turn a threat by trading speed for turn rate. Tail-chasing is a design bug they actively avoid; the fix is the pitch-up swing that puts the enemy in your nose in 1 s | Already have boost-drift. Add `HARD TURN`: hold S (brake) + mouse: turn rate x1.6, speed x0.55, ship slides. Dumb homing orbs (boss) become the "missile": defeated by a hard turn when it is within 8 L. 0 new input beyond reuse of S |
| Star Wars Squadrons | Engine-cut flip (drift) + power split | Power split is a readable 3-way tradeoff; flip-and-shoot rewards positioning. NMS-style arcade does not need the HUD burden | SKIP the power split (do-not list: energy management). Keep drift (already planned). Nothing extra |
| Everspace 1/2 | Ability with a short cooldown tied to build; enemies mixed so one tool is never enough; legendary loot dopamine | A mixed enemy roster plus 1-2 active abilities gives constant "which tool now" micro-decisions. Loot makes every kill a lottery ticket | Roles + dodge are already the roster/ability. Cheap loot analog: kills drop orbs (shield, overdrive). Do not build gear. See top-6 #1 |
| Freelancer | Mouse-flight with strafe + cruise as a mode | Single-hand, readable. Cruise = commitment: you cannot fight while in it, which gives travel its own pacing | Already have pulse. Optional later: Q/E strafe 0.4x cruise, lets you sidestep sniper beams without turning. Defer; roll-dodge covers it |
| House of the Dying Sun | Time dilation "tactical" slow + squad orders | Slowing lets you think inside an action game; power fantasy of foresight. Orders make allies a tool, not wallpaper | Player-held slow, not boss intro (does not violate do-not 1 s rule): Q held, game dt x0.35 for up to 1.5 s, 8 s recharge. Allies get one order: `Z` = all allies focus your current target (no new UI). Top-6 #6 |
| Chorus | Drift into a ritual ("rite") that stacks | Movement skill chains into a damage burst, so style = power. Players chase flow | Drift exits already give +25 % speed. Tie to chain system (top-6 #1): drift-exit kills within 2 s count x2 on the chain. 0 extra input |
| Rebel Galaxy Outlaw | Slow, heavy, broadside-like positioning with chill music | Low APM, high readability. Fights are positioning puzzles, not twitch. Matches Emory's chill target | Matches the tempo section below; no mechanic to steal beyond long breathers and reactive music (phase 2 audio) |
| Elite Dangerous (FA off) | Flip-and-burn; mastery ceiling | The skill ceiling is the draw for a small audience. For this audience it is a trap | Do NOT do inertia-only. Optionally a 'FA-off' hold-key (Alt): velocity decoupled from nose for 1.5 s, drift is the same code as boost-drift with slip 0.05/s. Defer; boost-drift covers it |
| Galaga / Gradius | Pattern recognition + a score that rewards doing it well | Enemies move in learnable choreography; clearing a whole formation pays a bonus; Gradius power-up capsules drop from marked enemies (kill = reward) | Wave spawns in formations: 4-6 interceptors on a spline entrance, break off after 3 s into dive runs. Kill the full formation inside 6 s: bonus orb + chain +3. Needs `formationId` on each enemy |
| Swink, Game Feel | Real-time control + simulated space + polish. 10 s of pure flying must already be fun (his "game feel" test) | Feel = input response latency (<100 ms), a physical object that resists (weight), and feedback layers | Already direct mouse + weight curve. Test: with enemies off, 20 s of flying + gunning on a rock should be fun. If not, tune MOUSE_WEIGHT/bank, not combat |
| Nijman, Art of Screenshake | Stack of cheap layers on the SAME hit: hit-stop, kick, shake, particles, enemy knockback, sound pitch | No single layer carries it; the sum does. Starting from a plain bullet and adding layers one at a time shows where the value is | Mostly in 9b #5. Missing layers to add (S): muzzle kick on the ship (push ship back 0.02 L/shot), brass-like casing sparks, enemy knockback along bolt dir, white 1-frame hit flash, kill = slight FOV punch (+2 deg, 0.15 s) |
| Doom 2016 | Glory kill = tempo tool: aggression pays HP | Resource flow points forward: low HP means go attack, not hide. Prevents turtle, creates push-forward rhythm; the animation also gives a 1 s victory beat | Direct analog: stunned enemies (end of flee-regen, 15 % HP) can be "finished" for a shield orb + chain. Plus every kill drops a small shield orb; so shield regen from play, not a timer. Top-6 #1 |

## 2. Ranked top 6 fold-ins (not in the plan)

### 1. Kill-fed shield orbs + chain meter (S/M, M)
- What: kills drop 1 glowing orb (violet=shield, cyan=overdrive) that drifts toward the player within 12 L, collected by flying through. Chain meter: kills within 4 s of the last add +1; chain >= 3 doubles shield value; chain breaks on 4 s of no kill (not on hit).
- Why fun (Doom, Galaga): pushes forward, you are paid for aggression. Replaces passive regen as the main shield source so "hide and regen" stops working. Gives every kill a visible pickup, the Everspace loot tick without gear.
- Implementation: on enemy death (the existing death handler) spawn orb in a pooled array (cap 24); per-frame magnet pull when dist < 12 L at 25 L/s; collect < 1.2 L. `shield += 12 * (chain >= 3 ? 2 : 1)`. Chain state = `{n, t}` on the player; HUD number by the reticle. Drift-exit kills count x2 (Chorus). Existing post-hit 4 s regen pause stays.
- Numbers: orb 12 shield (max 100), life 8 s, chain window 4 s. Passive regen drops to 40 % of today once orbs ship (tune so a chain of 3 roughly doubles it).
- Conflict check: do-not "no loot/economy screen": orbs are in-world, no UI. OK.

### 2. Graze meter -> overdrive (S, S)
- What: enemy bolt passing within 1.5 L of the ship without hitting adds graze; full meter (10 grazes) = Overdrive 4 s: fire rate x1.5, shield regen paused, damage x1.25.
- Why: risk/reward from shmups. Rewards flying toward fire, the opposite of hiding. Makes dodge roll and drift offensive skills. Readable: a tick sound + ring pulse per graze.
- Implementation: in the enemy-bolt update loop, track min distance to player; on bolt retire if min < 1.5 L and never hit, `graze++`, once per bolt. Overdrive is a timed flag read in the fire-rate and damage calcs. Pooled; no allocation.
- Numbers: 10 grazes, decay 1/3 s after 3 s of no graze; Overdrive 4 s, cooldown to refill is natural.

### 3. Painted multi-lock charge shot (M, M)
- What: Star Fox charge. Hold LMB 0.8 s, sweep reticle across <= 4 enemies, release for homing bolts (2.5x dmg each).
- Why: the signature on-rails verb: a skill-lock (you place them) with a cost (charging cannot fire). Gives a climax move for the engage phase and a boss opener.
- Implementation: in the fire handler, split tap vs hold; while held, accumulate `charge`, and each frame cone-test enemies (6 deg, 60 L) to push into `locks[]`. On release, spawn homing bolts with a steer rate that interceptors can break by dodging. Reuse bolt pool with `homing` target field. HUD: ring per lock.
- Numbers: charge 0.8 s, cooldown 3 s, homing steer 90 deg/s, speed 80 L/s, 2.5x dmg, lock range 60 L. Cannot lock through a planet.
- Conflict check: do-not 'no auto-hit missiles'. This is dodgeable and player-painted, with a charge cost, so it is a dumb-ish exception. Flag to Emory: if refused, ship #1/#2 and skip.

### 4. Wingman in peril (S/M, S)
- What: when an enemy tails an ally for 2 s, HUD callout "ALLY 2 TAILED" and bearing arrow; save them within 10 s.
- Why: Star Fox's best emotional beat. Gives the engage phase a mid-fight objective and a reason to use allies you already built (they currently risk being wallpaper).
- Implementation: in ally update, find any enemy within 12 L behind it in a 25 deg cone for 2 s -> `ally.tailed = {by, t:10}`. If tailer dies: ally orb + chain +2. If timer hits 0: that ally takes 60 % of its HP. A DOM line plus a marker sprite; reuse the target-bracket code.
- Numbers: 2 s detect, 10 s save, one active callout max, only from wave 3 (allies start there).

### 5. Boss phases + exposed weak point windows (M, M)
- What: boss has 3 phases at 100/66/33 % HP. Each phase has a different bolt pattern; the eye core is only exposed (2x dmg, glowing) while the boss performs its attack (a telegraphed 2 s), closed otherwise.
- Why: Gradius/Star Fox bosses are pattern puzzles: learn the telegraph, hit the window. Fixes the "sponge" feeling with a clear skill answer. Breaks tempo into rhythm.
- Implementation: boss state machine gets `phase = f(hp/maxHp)` and `core.open` boolean driven by attack state; weak-point multiplier is already 2x, gate it on `open`. Phase change = 1 s (not slow-mo) flash + shake + add 2 interceptors (adds not HP).
- Numbers: phases 100/66/33 %, window 2 s every 6 s (5 s in phase 3), adds 2 per phase, boss HP ~ 40 hits total, not more.

### 6. Focus slow-time (S, S)
- What: hold Q: game dt x0.35 up to 1.5 s, 8 s recharge. Allies get `Z`: all focus your target.
- Why: House of the Dying Sun's whole feel. Gives a panic button that is skill-gated (you still aim) and a cinematic beat. Pairs with charge shot (#3): slow, paint locks, release.
- Implementation: a `timeScale` multiplier applied to `dt` at the top of step() for world sim only, not for camera/mouse (input stays at real-time, feels like you got faster). FOV +3 deg and a subtle desaturate via CSS filter overlay at opacity 0.25. Pool the audio low-pass later.
- Numbers: 0.35x, 1.5 s, 8 s cooldown, cannot be used for the first 3 s of a wave. Bolt speed unaffected for the player (so you gain accuracy), enemies slowed.
- Not violating the do-not (>1 s boss slow-mo breaks flow): that was forced; this is player-initiated and short.

Backlog (not top 6): Galaga formations (needs formationId + spline entrance, M); Rogue Squadron objective waves (escort/defend, M); reactive music layers by tempo state (phase 2 audio); Q/E strafe.

## 3. Tempo design (chill-but-fun)

One wave = four beats, total ~55-75 s. Rhythm matters more than difficulty (Rogue Sq/Star Fox: peak then breather).

| Beat | Seconds | What happens | Player feeling |
|---|---|---|---|
| Approach | 0-8 | HUD callout "WAVE N: 5 hostiles", bearing arrows, enemies fly in at 1.5x, hold fire (9b #4 3 s grace). Music/engine hush | Anticipation, time to line up, boost toward them if brave |
| Engage | 8-35 | Interceptors first (fast, pressure), then spitters/snipers 4-6 s later (staggered, do not release all roles at once). First kills drop orbs, chain starts | Flow: chain-building, dodging, picking targets |
| Climax | 35-50 | Last 2-3 enemies enter flee/regen loops (existing flee) so the finish is a chase; charge shot and slow-time shine here. Wave-clear bonus orb | Peak. The catch-the-fleeing-one moment is the payoff |
| Breather | 50-62 (8-12 s) | Pickups drift in, shield refills a little, ally banter callout, no spawns. HUD "WAVE CLEAR" + chain total | Relief, reward, the chance to look at the planets |

- Total cycle target: 60 s (45 s combat, 15 s calm). Matches 12-20 s waves in rev 6 only if spawns are overlapping; ship rev 9 ramps 4x, so keep 60 s as the rhythm and let counts, not cadence, rise.
- Intensity curve inside a wave: 40 % -> 100 % -> 30 %. Never ramp linearly; spikes need valleys.
- Every 4th wave is a shorter, harder spike (45 s) followed by a longer breather (15 s): sawtooth, not a slope.
- **Bosses** change the rhythm: no trickle waves during a boss. Beat 1: 6 s entrance (name bar, scale reveal, no firing). Beat 2: phases are the loop (each ~25 s: pattern, window, damage, phase flash). Beat 3: final phase is the climax with adds. Beat 4: 15 s breather + guaranteed big orb drop + upgrade tick (rev 10). Total 90-120 s. Boss waves replace the normal wave; they are the "chapter end".
- Idle guard: if no enemy is within 80 L for 15 s after approach, enemies nudge toward the player (stops AFK dead air without punishing the chill player who ignores them).
- Chill clause: a toggle already planned (/peaceful) removes engage and climax; breather loop stays.

## 4. Build order (cheapest feel gain first)
1. #1 orbs + chain  2. #2 graze  3. #6 focus  4. #4 wingman  5. #5 boss phases  6. #3 charge shot
(Ship 1+2 with 9b #1 and #5 first: they alone change how kills feel.)

## Sources
- [Ace Combat dogfighting/high-G summary (fandom + Bandai Namco dev diary)](https://en.bandainamcoent.eu/ace-combat/news/ace-combat-8-wings-of-theve-developer-diary)
- [Making of Ace Combat 7](https://www.bandainamcostudios.com/en/behind-the-game/252)
- [PC Gamer: Squadrons drift/boost/power](https://www.pcgamer.com/drifting-engine-boosts-and-overcharged-lasers-how-combat-works-in-star-wars-squadrons/)
- [Unreal: Everspace 2 developer interview](https://www.unrealengine.com/en-US/developer-interviews/everspace-2-delivers-a-handcrafted-universe-brimming-with-space-combat)
- [Nijman, The Art of Screenshake (YouTube)](https://www.youtube.com/watch?v=AJdEqssNZ-U)
