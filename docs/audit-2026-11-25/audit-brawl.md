# GOR BRAWL audit (read-only). Tests: `node --test tests/node/smash*.test.mjs` = 52 pass / 0 fail / 0 skip (0.57 s).
All js/ paths relative to repo. Units: px and frames at 60 Hz, H = 36 px (fighter height), REF = 1.8 px per "reference unit".

## Headline
- Physics is a numeric copy of Melee reference values (smash-phys.js:3-6), so it is not slow by the numbers, but 36 px fighters on a 480x270 canvas, a 7-20 H camera, a 0.4/f ground accel and a 8-frame run-up read as floaty/slow. No dash, no dash dance, no wavedash, no fast ground burst.
- Fixed 60 Hz sim; all anim is frame-locked to sim state (st/mf), so a time-scale is safe (see A2).
- Bot: tier 3 is decent; tiers 1 and 2 are the problem, and most world NPCs are tier 1 (ship.js:8754 FG_TIER: villager/shopper/clerk/kid/farmer = 1, trader/dealer/guard = 2, elder/golum = 3).
- Measured (my headless run, plateau stage, idle P1, 2 stocks, 180 s): tier1 54 dmg dealt in 11049 f, idle player never KO'd; tier2 KO at f1343/1433; tier3 KO at f1165/2465 (233% on swift). tests/node/smash-bot.test.mjs says idle loses to all tiers on the ring stage within 40 s (1 stock). So "idle beats elder" in HANDOFF is not reproduced on the headless sim; it was a single in-world run (terrain stage + night/ledge differences). Needs human look, but tier 1 truly cannot kill.

## (A) Motion constants
### A1 table
| Constant | file:line | Current | Implied feel | Proposed FAST |
|---|---|---|---|---|
| gravity | smash-phys.js:11 | 0.095*1.8=0.171 px/f^2 (x0.94..1.06 by weight, :33) | full hop apex 52 px (1.45H), takeoff v 4.14 px/f, rise ~24 f, hop ~48 f (0.8 s) | 0.14*1.8=0.252 (hop ~40 f) |
| fall cap | :12 | 1.7*1.8=3.06 px/f (184 px/s = 5.1 H/s) | slow terminal; top of stage to floor feels like drifting | 2.4*1.8=4.3 |
| fast fall | :13 | 1.6x cap = 4.9 px/f | ok | 1.7x cap (7.3 px/f) |
| hop heights | :14 | full 1.45H, short 0.55H (19.8 px, ~30 f total), double 1.3H, multi 1.0H | ok in H; time scales with g | keep (apex time shrinks via gravity) |
| jump squat / short window | :17 | 3 f / 3 f | Melee-like | keep 3 |
| air speed / accel / drag | :15 | 0.95*1.8=1.71 px/f, 0.072 px/f^2, drag 0.036 | air drift ~24 f to cap: sluggish | 1.3*1.8=2.34, 0.13, 0.04 |
| run speed | :16 | runMin 2.16 .. runMax 3.24 px/f by weight (194 px/s = 5.4 H/s); stage 12H=432 px crosses in 133 f (2.2 s) | slow vs a Melee-speed rig; a whole screen (13 H) in ~2.4 s | 1.7*1.8=3.1 .. 2.4*1.8=4.3 |
| ground accel | :66 | +0.4 px/f to cap (8 f ramp), no initial dash burst | mushy start | +0.8 and first 6 f at 1.15x cap (dash burst) |
| traction / brake | :16, :67-68 | 0.108; stop = x3 (0.32/f), reverse skid x4 | ~10 f to stop from full run | 0.15; stop x4; keep pivot turn 4 f (:17) |
| dash dance | none | reversal inside 'run' with |vx|>1.4 forces 4 f 'turn' (locoStep :86) | dash dance impossible; no pivot/dashback tech | allow reversal within first 8 f of a run without turn state |
| input buffer | :18, readInput engine:190 | 6 f (tech window 12 f) | fine | 8 f |
| knockback formula | :48-50 | ((d/10+d*p/20)*200/(w+100)*1.4+18)*s+b | Melee clone | keep |
| launch speed | :19 | kb*0.054 px/f (kb 100 = 5.4 px/f) | KOs need ~100%+ on 100-weight | 0.065 (kb100 = 6.5), KO ~15% sooner |
| kb decay | :20 | 0.0918 px/f per f | kb100 flight ~59 f | 0.11 |
| hitstun | :52 | floor(0.4*kb), min 6 (engine:556), tumble at kb>=60 | long lock-out in the air | 0.36*kb (shorter, combos tighter) |
| hitlag | :51 | floor(dmg/3)+4 (3 dmg = 5 f, 15 dmg = 9 f, 25 dmg = 12 f); electric/armor/flinch:false = 2 f | OK but small hits do not crunch | keep formula, add +30% if kb>=60 and a 2 f floor extension on KO-range hits |
| DI | :21 | 18 deg max | ok | 18 |
| smash | :22 | charge 60 f, mul 1.4 | ok | keep |
| landing lag (generic) | :123 | 4 f; airdodge landing 6 f (:134) | already L-cancel-ish | keep |
| aerial landing lag | smash-moves.js:BASE nair 6 fair 9 bair 8 uair 7 dair 14, x profile q | OK | x0.75 |
| move startup/endlag | smash-moves.js:BASE | jab s3 e9; ftilt s7 e15; utilt s6 e12; dtilt s5 e11; dashatk s7 a6 e20; fsmash s12 e24; usmash s10 e26; dsmash s9 e24; nair s4 a10 e10; fair s8 e16; bair s6 e14; uair s5 e14; dair s9 a6 e17 (startup clamp 3..25, endlag cap 45 at buildMove) | Melee-ish startup, heavy endlag (tilts 15 f, smashes 24-26 f) = slow trade rhythm | scale endlag x0.8 and startup x0.9 globally in buildMove (PROFILES.q already exists per style: multiply by 0.85) |
| shield | engine:296-308, 515 | 180 meter, -0.55/f (327 f), hit drain dmg*1.4+1, shieldstun dmg*0.55+2, grab/jump/roll out | fine | keep; roll 28 f, spot dodge 24 f OK (engine:310-315) |
| airdodge | phys:132-145 | speed 3.2, x0.9/f, intangible 3..30, 40 f then helpless | long, then special-fall punish | speed 4.5, intangible 3..22, 32 f |
| respawn / KO | engine:63,497 | stocks 3 (world 2), dead 50 f, count-in 150 f, time 14400 f (world 180 s) | count-in 2.5 s is slow for rematches | 90 f count-in |

### A2 Time step and speed multiplier
- Sim is fixed 60 Hz with an accumulator: smash-engine.js:1111-1124 (`loop`, `acc >= 1000/60`, max 5 steps/frame) and smash-world.js:253-258 (`update(dt)`, same). KO slow-mo doubles the step period for 20 frames (view.slow).
- Render uses interpolation alpha = acc/period (px,py -> x,y). Sprite frames come from sim state (st/mf/animT, engine:862-873), particles/shake from view.t which ticks per sim step. Nothing uses wall-clock time inside the sim.
- Global speed: add `speed` (default 1) and change the two step periods to `1000/(60*speed)` (and `2000/(60*speed)` for slow). Everything (physics, frame data, hitlag, bot reaction, SFX pitch is unaffected) scales together and determinism is preserved (inputs still sampled once per step). Cap n at 5 -> 6 for 1.5x on slow machines. Local/bot modes only; host/guest net lockstep (smash-world.js:171-182) must stay 1.0 or negotiate it in cfg.
- Recommended: speed 1.2 globally (cheap, 1-line x2) PLUS the "FAST" physics preset above (gives airborne/run speed without making every attack frame faster). Do not stack both at full strength: 1.25x * 1.4x run speed = 1.75x which will break stage-edge geometry and bot reach tables.
- Implementation of the physics preset: PHYS is a mutable exported object, physOf caches per def in a WeakMap (phys:28-40), so patch PHYS before the first physOf call, or add `PHYS_PRESETS={classic,fast}` and `setPreset()` that clears the cache (new WeakMap). tests/node/smash-phys.test.mjs assert the classic numbers (0.095, 1.7, 0.04, 0.95, 18 deg) so classic must stay the test default.
- Note: smash-fighters.js defs carry their own `run`, `grav`, `fall`, `air` fields (e.g. swift run 1.7, grav 0.12) but physOf() ignores them (uses weight only); per-character speed differences are therefore only weight-based. Wiring def.run/grav/fall into physOf gives real fast/slow characters.

## (B) Bot findings (smash-bot.js, 344 lines)
### Per-frame behaviour
- botInput (:98) runs every sim step; writes snapshot ring of all fighters (:46-54, 32 frames), reads the foe from `react` frames ago (:134, pastOf), then runs a priority cascade: special states (egg/held mash :110, grabHold pummel+throw :111-121, respawn :122, ledge :123-127), hitlag hold (:128), hitstun DI/tech (:139-148), recovery (:152-166), projectile shield (:172-175), shield state exit/OOS grab (:178-186), cadence gate (:189), defend (:199-207), punish (:209-216), edgeguard (:219-229), air attack (:232-239), grounded neutral: jump-to-higher/drop thin/shield-grab/turn/preemptive guard/spacing/poke/approach (:243-280).
- Level/tier table (:13-16): react 20/8/3 f, atk 0.22/0.6/0.9, every 12/2/1 (decision cadence), miss 0.3/0.04/0, edge false/false/true. Level only changes thresholds `lvl>=2`/`lvl===3` through the code (defend, DI, tech, grab OOS, smash punish, preemptive guard, spacing logic). Engine maps level 1-3 -> tier (engine:96); ship passes both (ship.js:8797).
- Knowledge it does have: own frame data/reach (infoOf :24-39), foe's active hitboxes and startup via move table (threatOf :62), vulnerability windows (vulnOf :77), knockback-based advantage (advOf :41). Reads only delayed state, no foe inputs. Output is a normal mask with edge-only taps (:95-96). Deterministic (seeded rng).
### Why it is easy
1. World NPC difficulty ceiling is low: 8 of 11 roles are tier 1/2. Tier 1 (react 20 f = 333 ms, acts every 12th frame, 30% intentional whiffs, picks moves at random `sc = R_()` :294, no defend/DI/tech/grab/punish/shield because everything is gated `lvl>=2`) dealt 54 dmg in 3 min to an idle target. It also hard-skips edge/recovery 20% of the time (:158).
2. Even tier 3 never plays the air game from the ground: no short-hop/full-hop approach, no jump-in, no fast-fall (:237 is a literal no-op `tap |= 0`), aerials only when already airborne (:232). It walks straight at you and pokes at range (:277-279). A player who jumps/dashes over or spaces at its max reach is not punished.
3. No specials in neutral: K is only used for up-B recovery (:163); bN/bF/bD, projectiles and `I.ranged` (:26) are never used. Ranged/trap characters are played as melee.
4. No dash attack except in punish (pickPoke skips dashatk :292); no combo extensions, juggles, uair/usmash chains, kill-confirm (it does not compute kill % although `knockback()` is imported), throws chosen only by % (:114-120).
5. Defence is a single behaviour: shield when thr<=14, 97% of the time at tier 3 (:200). It never rolls, spot dodges, jumps out, or shield-drops; airdodge 50% at tier 3 only (:206). Shield-poke/pressure is not modelled (bot waits `shT` 8 f).
6. Edgeguard is ground-only: it stands at the ledge and smashes (:219-229), never goes offstage, no ledge-trap, no getup read, tier 2 skips it entirely (`edge:false`).
7. Recovery is hard-coded and mostly competent but uses fixed thresholds (:160-165) with no mixups, no sweetspot/ledge-hog timing, no stage-spike avoidance; ledge options random 50/40% (:125).
8. Bot behaviour is predictable: same spacing, same cadence, `atk` per-frame roll, move score is `dmg*4 - s*0.6 - e*0.5 + adv` (:293) so it spams the same highest-score poke. No adaptation to the player's habits.
9. DI is a fixed heuristic (up if |vx|>|vy| else toward center, :142-143); tech only on floor at tier 3 (random 50% at tier 2). No wall-tech/ceiling logic, no survival-DI vs KO move.
10. Idle player is not the case that exposes weakness; the weakness is a human that attacks. The headless test only proves tier 2/3 can kill a stationary target.
11. Bot gets no stat handicap/bonus; "hard" = perfect hitbox knowledge with 3 f delay, but it will not use more tools. Difficulty is a few thresholds, not a ladder.
### Engine data
- Exposed already: frameData(m) (s,a,e), hit seg/r/dmg/bkb/gr/ang/charge, foe state/move/mf/hitstun/sst/lag/stunT, pct, jumps, ledge, shield meter, S.projs (pos/vel/dmg/owner), stage solids/thin/ledges/blast, physOf(), knockback()/hitstunOf().
- Missing: launch trajectory predictor (kb -> arc -> KO test vs blast zones), hitCapsuleWorld/hurtCaps (engine-private; bot cannot import engine due to circular import) so aerial/approach prediction of foe hurtbox is approximate, foe's buffered/held inputs (intentionally hidden), landing-time predictor for foe jump arcs, move properties (is-projectile, disjoint, kill-power, combo-able flags) per move; `def.hint` is sparse.

## (C) Difficulty ladder (data in BOT_TIERS, replace 3 rows with 9; keep tier 1-3 mapping as legacy)
| L | react f | decide every | atk/frame | whiff | features added |
|---|---|---|---|---|---|
| 1 | 24 | 12 | 0.15 | 0.35 | random pokes, no DI, runs off edges 20% |
| 2 | 18 | 8 | 0.25 | 0.20 | walks + jab/ftilt, shields 30% |
| 3 | 14 | 5 | 0.4 | 0.12 | basic shield, grab on shield, simple recovery (current "tier 1.5") |
| 4 | 10 | 3 | 0.55 | 0.06 | DI toward center, floor tech, aerial after jump, projectile dodge |
| 5 | 8 | 2 | 0.7 | 0.03 | punishes whiff with tilt, short-hop aerials, specials for ranged chars |
| 6 | 6 | 1 | 0.8 | 0.01 | spot dodge/roll, OOS grab, dash attack, edgeguard (ground), smash punish (= current tier 2.5) |
| 7 | 4 | 1 | 0.9 | 0 | = current tier 3; plus full hop/SH approach, fast-fall, uair juggles, throw kill-confirm using knockback() |
| 8 | 2 | 1 | 0.95 | 0 | offstage edgeguards, ledge trapping/getup reads, wall tech, survival DI, combos by advOf>0, mixup RNG on options (not per-frame roll) |
| 9 | 1 | 1 | 1.0 | 0 | kill-percent planning (launch predictor), perfect shield-drop punish, pivot/dash-dance spacing, adapts to player's habits (counters of last 3 options), 1.15x run speed handicap-free "boss" cheat optional |
- World mapping: villager/shopper/clerk/kid/farmer L3-4, trader/dealer/guard L5-6, elder L8, golum L9 (L7 for first fights). smash.html menu: EASY=3, MED=6, HARD=8 plus a new MAX=9.
- Needs: a tier-parameter struct per feature (replace `lvl>=2/lvl===3` literals, :114-:271) so features flip on by tier flags rather than tier equality.
- Engine data to add: shared `smash-geom.js` with hurtCaps/hitCapsuleWorld importable by both engine and bot; `launchArc(kb, ang, dir)`; `koPct(def, pos, move)`; per-move tags.

## (D) Feel / feedback gaps
Exists: hitlag freeze (engine:726, f.hitlag, victim jitter :1010), view.hold freezes particles for e.lag (:949), shake = min(10, 1+kb*0.05+dmg/6) decays x0.85 (:941, offset pseudo-random 7/3 mod 5 :989), star-burst particles 5-13, hit SFX light/heavy with pitch by damage (engine:541), KO flash 3 f + slow-mo 20 f at 0.5x + blast-zone ray + 26-star burst (:950), shield/spark/break puffs, dashatk after-image, HUD card shake at dmg>=3, interpolated render, camera tracks both fighters, width clamp 7-20 H, lerp 0.08 pos / 0.04 zoom.
Input path: Input.mask() polled once per sim step (engine:1081 -> sim.step -> readInput:190), no extra frame of delay in the sim; render is interpolated (up to 1 frame lag, standard). Edge-detect + 6 f buffer. Keyboard only has smash via arrow keys; flick-smash needs <=5 f dir->A, and `dAgeX>=15` run vs tilt distinction (:222) is hard to hit on keyboard so dash attack/smash intent is fragile.
Missing vs a satisfying platform fighter:
1. No dash/dash-dance/pivot/wavedash/dash-cancel; run starts as a walk ramp.
2. Hit impact is small: hitlag only 5-12 f, no scaled hitlag for KO/kill hits, no screen hit-zoom/punch-in, no motion lines/trail on launched fighters, no impact flash sprite sized to damage, no directional spark, no sfx layering (no whoosh on attack start, no run/landing foley beyond 'land').
3. KO moment: 20 f at 0.5x is fine but no camera zoom on the KO, no blast-zone explosion at the launch line size scaled, no "last stock" treatment; fall off blast is 16H/14H/7H away, camera clamped to blast so you watch long flights with little speed cue.
4. Camera never hard-cuts; zoom speed 0.04 means fights stay at wide zoom (fighters 36 px on 270 px screen = 13%). Tighter min width (7H -> 5.5H) and faster zoom 0.06 would raise perceived speed 25%.
5. Character-level tuning ignored (physOf uses only weight; def.run/grav/fall unused) so all 13 fighters have the same speed/jump feel.
6. Air game: no fast-fall indicator, no landing dust except 'puff' on knockdown, no ledge-sweetspot juice.
7. Stocks/timer: 3 stocks / 4:00 in menu (14400 f), 2 stocks / 180 s in world (smash-world.js:61-62). Count-in 150 f. Fine.
8. Input affordances: no remappable keys/gamepad deadzone documentation, no tap-jump toggle, no on-screen controls reminder in world fights.
9. In-world only: ring is main platform 12H wide with 2 soft platforms at 2.2H, spawn/hazards none; stage is small and flat so bot's ground-only plan is enough.

## (E) Prioritized fix list
S (<= 1 hr each)
1. smash.html/world speed multiplier `speed` in the two accumulators (engine:1116, world:255); default 1.2. Fixes "slow" with 2 lines.
2. FAST physics preset in PHYS (grav 0.14, fallCap 2.4, air 1.3/0.07, run 1.7-2.4, accel 0.8, launch 0.065, decay 0.11, hitstun 0.36, buffer 8); keep classic for tests; hook def.run/grav/fall into physOf for per-character feel.
3. Bump world NPC tiers (ship.js:8754) one notch up (villager 2, trader/dealer/guard 3, elder/golum 3 + new tier) as an immediate stopgap; tier 1 is unplayable as a fight.
4. Scale hitlag +30% for kb>=60, shake cap 10 -> 14, KO zoom-punch (view.z bump on 'ko'), camera min width 5.5H.
5. Remove dead code in bot (:237 no-op), enable dashatk in pickPoke at lvl>=2, enable bot specials for def.hint.ranged.
M (half day)
6. Dash state: run start burst, dash-dance window, pivot, dash-cancel; adjust bot locomotion to match (bot depends on run, state 'run' and dAgeX at :301).
7. Expand BOT_TIERS to 9 with feature flags; refactor `lvl>=2/===3` gates to flags; add short-hop/fast-fall approach, jump-in aerial, spot-dodge/roll defence, shield-drop punish, ranged specials, uair juggles.
8. Shared smash-geom.js (hurtCaps, hitCapsuleWorld) + `launchArc/koPct` helpers for bot kill planning, KO-aware throw/smash choice.
9. Endlag/startup global x0.85 via PROFILES.q or buildMove; update smash-moves tests that pin frame counts.
10. Test additions: attacking-dummy vs bot (scripted aggressor) win-rate by tier; assert strict ordering L1<L3<L5<L7<L9 over seeds.
L (multi-day)
11. Edgeguard offstage (jump out, aerial, fast-fall stage-spike), ledge trapping and getup reads, wall tech, survival DI.
12. Adaptive bot that counters the player's last N options; telegraphed difficulty UI (L1-L9 + MAX), per-level handicap-free boss mode.
13. Full impact-juice pass: layered SFX, sprite-sized hitsparks, directional impact flashes, launch trails, KO camera cut, run/landing dust, replay of last KO.
