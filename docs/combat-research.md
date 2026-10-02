# NO MANS GOR — combat research (2026-10-01)

Sourcing note: NMS complaints are from Steam threads, Kotaku, TechRadar. Arcade-game
"what they do right" is established design knowledge of those games (search results
only confirmed the broad strokes); treat the numbers in section 3 as starting values to tune by feel.

## 1. Top NMS combat criticisms, and what the best arcade games do instead

| # | NMS criticism | Source | Arcade fix |
|---|---|---|---|
| 1 | Auto-follow/auto-aim is the real aiming system; manual aim is "effectively impossible", so fights are a chore | [Steam: How much dogfighting?](https://steamcommunity.com/app/275850/discussions/0/3819656549000851692/) | Star Fox / Squadrons: you aim; a lead indicator shows where to shoot. Skill is reading the pip, not holding a magnet |
| 2 | Enemies out-maneuver you by ~10x; no ship matters | [Steam discussion](https://steamcommunity.com/app/275850/discussions/0/3279192886583916240) | Enemies are slower or equal in turn rate but have a role; the player's edge is boost + drift |
| 3 | One exploit beats everything: roll backwards, hold fire | [Steam discussion](https://steamcommunity.com/app/275850/discussions/0/3279192886583916240) | Enemies that flee/flank, bolts you must dodge, so standing still-and-shoot loses |
| 4 | Pirates spawn on top of you, instant aggro, no warning | [Steam: pirate spawn](https://steamcommunity.com/app/275850/discussions/4/360672383118978617/?l=czech), [Kotaku](https://kotaku.com/no-mans-sky-outlaw-update-pirate-invasions-bases-planet-1848794834) | Everspace 2 / Rogue Squadron: spawn at distance with a radar blip + callout ("3 hostiles, 2 o'clock"), you get 3-5 s to line up |
| 5 | NPCs fight predictably, repetitive, enemies circle you | [Steam: boring PvE](https://steamcommunity.com/app/275850/discussions/0/3819656549000851692/) | Distinct roles with different movement (see #5 below). Star Fox 64 rivals each hunt a specific wingman |
| 6 | Damage sponges, shallow progression, "not a combat game" | [Steam: avoiding combat](https://steamcommunity.com/app/275850/discussions/4/1458455461491986847/), [TechRadar Outlaws](https://www.techradar.com/news/no-mans-sky-outlaws-update-revamps-space-combat) | Kills in 4-8 hits, clear hit feedback; toughness comes from role, not HP bloat. Bosses have a weak point you can see |
| 7 | No sense of speed in fights | [Steam discussion](https://steamcommunity.com/app/275850/discussions/0/3279192886583916240) | Star Fox/Squadrons: debris, streaks, passing-by whoosh, FOV kick on boost. Speed comes from references, not raw number |
| 8 | No maneuvers: no evade, no flip, no drift; only steer and shoot | [Steam: How much dogfighting?](https://steamcommunity.com/app/275850/discussions/0/3819656549000851692/) | Star Fox barrel roll/U-turn/somersault; Everspace 2 dodge + boost; Squadrons drift/flip-and-burn. One button, one cooldown |
| 9 | Weak telegraphs: you get hit by things you never saw | [Steam: How much dogfighting?](https://steamcommunity.com/app/275850/discussions/0/3819656549000851692/) | Every enemy attack has a visible wind-up (glow, lock tone, tracer) 0.4-1 s before damage |
| 10 | Combat is a side-feature, no wave pacing: constant trickle or random swarms | [Kotaku](https://kotaku.com/no-mans-sky-outlaw-update-pirate-invasions-bases-planet-1848794834) | Rogue Squadron / Star Fox: scripted waves, peak then breather, boss as finale |

(Camera: not a major documented NMS complaint in sources found; the real issue is #1, where the
camera lags enemies. Fix = direct mouse, already done in rev 2.)

## 2. What NO MANS GOR already has (do not rebuild)

Dive-bomb runs, soft aim assist, allies, bosses with weak point, difficulty ramp, flee at 45% HP (planned),
star streaks, camera shake, localStorage progress. This doc only lists what is missing.

## 3. TOP 5 mechanics to add, ranked

### 1. Lead indicator + target cycling (fixes #1, #3)
- What: a small pip ahead of the targeted enemy showing where to aim; T cycles targets (nearest-in-cone first), a bracket and distance on the target.
- Why: turns aim assist from a magnet into a skill; lets you REMOVE most of the magnet.
- How: pip = target.pos + target.vel * (dist / boltSpeed), iterate twice (convergence is enough). Project to screen, draw a DOM ring. Cut magnet strength to 40% of current and cap at 2.5 deg cone (from ~4). T = cycle by angle to nose; max lock range 60 L. Pip turns solid when within 1.5 deg of reticle.

### 2. Enemy roles with telegraphs (fixes #5, #9, #3)
- What: three roles, one per wave type. **Interceptor**: fast, 60% HP, flanks and tails you, fires short bursts. **Bomber/Spitter**: slow, 150% HP, fires slow big orbs you can dodge, never turns fast. **Sniper**: hangs at 80 L, charges a visible beam 1.2 s then fires one 25-dmg shot.
- Why: kills the predictable circle-and-shoot; each needs a different response (chase / punish the slow turn / break line of sight).
- How: role = a flag selecting speed mult (1.3 / 0.6 / 0.5), turn rate, fire pattern. Telegraph = eye core glow ramps over 0.5 s (interceptor burst), orb is visible 1 s of travel (bomber), laser line drawn 1.2 s (sniper). Wave 1 only interceptors; add bomber wave 2; sniper wave 3. Spawn mix per wave: 60/25/15%.

### 3. Dodge roll + boost-drift chaining (fixes #8, #3, #7)
- What: double-tap A/D = barrel roll sidestep (Star Fox). Holding Shift through a hard turn gives a drift: the ship keeps its velocity vector while the nose swings, and releasing Shift exits with a speed burst.
- Why: gives the player a real answer to bolts and a reason to move; NMS has none. Drift exits make boost feel like a combo.
- How: roll = 0.45 s, lateral impulse 6 L sideways, 40% damage reduction (not full i-frames), cooldown 1.5 s. Drift: while boost held and |yawRate| > 40 deg/s, set velocity slip to 0.4/s (instead of 3/s). On release within 1.2 s of drifting: +25% speed for 1.0 s, decaying. HUD cooldown pip.

### 4. Pre-spawn warning + stagger directions (fixes #4, #10)
- What: waves spawn 150-250 L out, never within 100 L; HUD shows a bearing arrow and "WAVE 3: 5 hostiles" 3 s before they commit. Enemies arrive from 2 different directions.
- Why: removes the "pirates on top of you" feeling and gives you a breather to line up.
- How: spawn on a sphere radius 200 L around the player, reject points within 60 deg of forward only if the previous wave came from there (spread). 3 s grace (enemies do not fire, fly in at 1.5x speed). Edge-of-screen red chevrons for off-screen enemies, fading as they enter view. Breather: 8 s of no new spawns after a wave clears, before the next announce.

### 5. Hit feedback pack (fixes #6, #7)
- What: hitmarker tick on hit, distinct shield-pop vs hull-crack sound/color, enemy flinch (0.1 s speed dip + 6 deg yaw kick), brief hit-stop 40 ms on kills, kill confirm ring on the HUD.
- Why: makes 6 hits feel like 6 hits, so HP does not need to be bloated to feel powerful; replaces "damage sponge" with "satisfying chain".
- How: on hit: reticle scales 1.0 -> 1.4 in 80 ms, white tick sound at pitch 1 + (combo * 0.05), combo resets after 1.5 s. On kill: freeze game dt for 40 ms, 24-particle burst, +1 kill ring pulse at reticle. Enemy HP target for wave 1: 6 hits, wave 3: 10 hits, not the current ×4 inflation.

## 4. Do NOT do

- **Do not** add full Newtonian physics or flight assist toggles. It is an NMS-style arcade game; velocity chasing nose is already right.
- **Do not** add i-frame spam dodge (full invulnerability, no cooldown). It becomes the new roll-backwards exploit.
- **Do not** raise enemy HP to make fights "last". Sponge is complaint #6. Add roles and flee instead.
- **Do not** make flee unkillable: if a fleeing enemy is always out of range, players quit. Flee must be catchable with boost within about 4 s.
- **Do not** add a loot/economy/upgrade screen for combat. Out of scope and slows the arcade loop.
- **Do not** add missiles with lock-on that auto-hit. Re-creates the auto-aim problem. If missiles exist: dumb-fire, slow, dodgeable, damage focus on bosses.
- **Do not** make hits invisible: no damage you cannot see or hear.
- **Do not** make allies the main damage source. Weak allies that draw fire are right; allies that kill for you repeat NMS auto-win.
- **Do not** pause or slow-mo the whole game on boss intros longer than 1 s; breaks flow in a browser tab.
- **Do not** raise enemy numbers above ~10 live. Browser frame rate and readability both die first.

## 5. Suggested build order

1 lead pip + target cycle (cheapest, biggest feel change) -> 5 hit feedback -> 2 roles -> 4 spawn warning -> 3 dodge/drift.
