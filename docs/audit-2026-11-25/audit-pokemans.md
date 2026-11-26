# POKEMANS / GOR BALL audit (read-only)

Tests: `node --test tests/node/pokemans*.test.mjs tests/node/pure-pokemans.test.mjs` -> 73 tests, 73 pass, 0 fail (pure logic only; no test covers approach geometry or the on-foot flow).

HANDOFF is stale on one point: `ft.wild` is not a field. `footTarget()` sets `ftI.kind='wild'` at ship.js:8745 (Q48) and `onKeyF` handles it at ship.js:5087 -> `wildFight` (8204) -> `battleBegin` (9708). The "ring-wild entry unwired" note is wrong. The wiring exists. The loop fails on geometry and on discoverability, not on missing calls.

Units: H = 0.27 L (ship-scale.js). So 3 H = 0.81 L, 20 H = 5.4 L, 25 H = 6.75 L.

## (A) Step trace

| # | Step | Wired? | Gate | Failure point |
|---|------|--------|------|---------------|
| 1 | See a creature | yes | creatures spawn per planet (ship-planet.js:2318-2340); kinds walker/jelly/ox/strider; beh grazer/skitterer/floater/strider | none |
| 2 | Approach | yes, but hostile | grazer flees at 10 L (=37 H), skitterer 6 L (22 H) at 14 L/s, strider 8 L; flee timer 3.5 s, speed 2.4x vmax (grazer 4.8 L/s) vs player walk 6 H/s = 1.6 L/s, run 12 H/s = 3.2 L/s (ship.js:5178) | ship-planet.js:2338, 2452, 2462: grazers/oxen/striders/skitterers cannot be approached on foot. Flee radius (37 H) is nearly 2x the throw reach (20 H) |
| 2b | Floaters (jelly) | yes | flee 0, but wander-toward-player stops at 6 L then backs off (ship-planet.js:2440-2447); hover 5-12 L above ground (2339) | equilibrium distance is >= 6 L = 22 H, over the 20 H throw reach (pokemans-catch.js:19 `AIM_REACH=20`, `BALL.reach=20`). Plus altitude 5-12 L = 18-44 H straight up. Only reachable with jetpack |
| 3 | Weaken (on foot weapon) | n/a by design | none needed: weakening happens INSIDE the battle (FIGHT then BALL). `battleCatchChance` = floor x (1 - hp% x 0.8), pokemans-catch.js:53 | player may expect on-foot damage; none exists and none is required. Nothing tells them |
| 4 | Buy ball | yes | `GORBALL_TIERS` pushed onto every 7/11 shelf table (ship-items.js:199), pinned to top of STORE list (ship.js:8553). 15 / 60 / 200 gorCoin | requires finding a 7/11 (outpost/store), cost 15. No tutorial, no ball in starting kit (not verified for starting inventory) |
| 5 | Hint | yes | `gbHint` ship.js:9744: shows only if catchable within `HINT_RANGE`=25 H (catch.js:20) | hint range 25 H < flee radius 37 H: grazer hint can only appear after the creature already stopped fleeing; it never does. Jelly hint (22 H) shows but throw (20 H) cannot reach -> "B . THROW GOR BALL" prompt that cannot succeed |
| 6 | Hold B | yes | keydown 'KeyB' ship.js:9802: state==='piloting', !cmdOpen, !inv/store, gmode==='foot'. Shift = best tier. No key conflict found (B unused elsewhere; HELP lists it, ship.js:881, 4052) | silent if not on foot ("ON FOOT ONLY" row) |
| 7 | Aim assist | yes | `gbPickAim` 9604: 20 deg cone, <= 20 H from hand, body sphere (grazer radius x1.5) | out of range -> no aim, no calm, ball flies unassisted (`calmCreature` only on aimed throw, 9640) |
| 8 | Arc / hit | yes | BALL.speed 15 H/s, tMax 2.2 s, sphere sweep `gbHitTest` 9690 + LOS | hit returns the ball (refund 9769) and opens battle. No "shakes"/click on world: HANDOFF's "3 shakes" now lives only in the pixel battle; GB shake/roll code is the miss path |
| 9 | Battle UI | yes | `battleBegin` 9708 imports pokemans-battle.js, injects style/pokemans-battle.css, pointer-lock released, keyboard 1-menu (W/S/arrows, Enter/Space/F/E select, Esc back) | reachable via ball hit OR F within 3 H. Fine once reached. Win on a world creature only calms it 12 s (9733); creature not removed |
| 10 | Wild F entry | yes | `wildFind` 8199: <= 3 H (0.81 L), kind catchable; priority is LAST in `footTarget` after villager, shop, burger, cvs, shelf, resource, tame, npc | priority: any nearby shelf/resource/NPC steals F; with a treat/fries in bag, `tameFind` (12 L) returns `F . TAME` first (8192, 8738) and tames to a PET, not a POKEMANS. 3 H is unreachable anyway (step 2) |
| 11 | Predators (guardians) | yes | `R27.gnd` entities are catchable by ball (9610, 9693 `e.guard`) | only these (hostile, they come to you) are practically catchable; player may not know to throw at them |
| 12 | Caught -> party/pen | yes | `gbCatch` 9686: party if < 6 else pen; dex, xp, bag stack kind 'pokemans' | works |
| 13 | U manager | yes | KeyU, foot mode only, shows "no POKEMANS yet" when empty (10101, 10108) | fine |
| 14 | Summon / follower | yes | `0` then 1-6, or HOLD Z, canSummon (9939-9952) | dead mon cannot summon; revive at CVS (counter) |
| 15 | Trainer fight | yes | villager with `.trainer` (ship-village.js:367): F twice (FG_ASK_T = 4 s) -> `fightStart` 8792 on the smash-world engine | label replaces FIGHT with BATTLE only for villagers (7835). Trainer mons never heal (fresh team each fight, no persistence): per HANDOFF |
| 16 | Wild battle vs trainer | n/a | two different battle systems: smash-world real-time (trainers) vs pixel turn-based (wild) | two mechanisms for one concept: violates "don't ship two mechanisms" rule |

## (B) Root causes (ranked)

1. Approach geometry makes wild creatures unreachable. Flee radius (grazer 10 L = 37 H; strider 8 L = 30 H; skitterer 6 L = 22 H) exceeds throw reach / aim reach / hint range (20 / 20 / 25 H); player speed < flee speed. ship-planet.js:2338 vs pokemans-catch.js:19-20. Calm (stop-flee) only triggers after aim, which needs the creature already inside 20 H. Deadlock.
2. Floaters (jelly) stay >= 6 L (22 H) away AND hover 5-12 L up; reach is 20 H straight. Hint shows, throw never connects.
3. The F-wild fallback needs 3 H (0.81 L). Unreachable for the same reason, and is lowest priority in `footTarget` (8744), pre-empted by treat-taming (8738).
4. Zero discoverability: no ball in the start kit (unverified), no prompt about "ball hit = battle, weaken inside battle", hint text promises throw at ranges where throw fails. HANDOFF text ("weaken, then B") suggests an on-foot weaken step that does not exist.
5. Win on wild world creature leaves it alive (calm 12 s) so XP-farm yes, catch no; lose = "blacked out, nothing lost" so there is no stakes or feedback.
6. Docs drift: HANDOFF row 22/56 says `ft.wild` never set; it is set (ship.js:8745). The stale note will mislead the next fix attempt.

## (C) Minimal fix list

| Size | Change | Target |
|------|--------|--------|
| S | Make the hint honest: show `B` hint only if within `BALL.reach` AND aim solvable; otherwise show "NEARBY WILD · GET CLOSER" at 40 H | ship.js:9744 gbHint, catch.js HINT_RANGE |
| S | Raise `AIM_REACH`/`BALL.reach` to 40 H, and `HINT_RANGE` to 45 H (> grazer flee 37 H); `BALL.tMax` 2.2 -> 3.0 s so arc reaches | pokemans-catch.js:11,19-20 (tests assert some values; check pokemans-catch.test) |
| S | Calm creatures when the player CROUCHES/slow-walks or while holding B (aim state) not only on throw: in `gbPreview` call `ps.calmCreature(id, 0.3)` for the picked target (GB.tgtId) | ship.js:9620 gbPreview/gbPickAim |
| S | Reduce grazer/strider flee to 6 / 5 L when the player has a ball-aim up, or add a `cd.aimedAt` flag in ship-planet.js that zeroes fleeT | ship-planet.js:2424 |
| S | Floaters: stop hover retreat at 4 L (14.8 H) and cap hover at 4 L; or let the 3D hit sphere count ground distance | ship-planet.js:2339, 2447 |
| S | F-wild priority: move `wildFind` above `tameFind` when the target is a catchable within 3 H, and raise to 8 H (2 L) | ship.js:8744, 8199 |
| S | Start kit: give 3 x GOR BALL T1 on profile creation | ensureProfile (ship.js ~605) |
| S | First-run tip row when first creature within 45 H: "hit it with a GOR BALL (B), weaken it in the battle, then BALL" | ship.js gbHint |
| S | Fix HANDOFF rows 22, 56 and the 'weaken' wording in HELP (4052) | docs |
| M | Heal trainer team: persist team hp in profile keyed by npc id, restore at CVS/day | ship.js:8794, pokemans-fighter.js:87 |
| M | Add an automated test: spawn creature at 30 H, hold B, expect `GB.tgtId` set and battle starts (ship-planet-tests.js already has `_gb.battle`) | ship-planet-tests.js:1324 area |

Verification gap: I did not run the game. Distances are from code constants; confirm in browser with `ship._gb.hint` and `ship._gb.throwBall()`.

## (D) Overhaul suggestions

- **Encounter prompt**: when a catchable enters 45 H, ring-pulse it and show a WILD card above it: species name, level, type chips, "B THROW · F ENGAGE". Seeing the species in the dex-style card makes the loop legible.
- **One battle system**: pick the pixel turn-based battle for ALL POKEMANS fights (wild + trainer), keep GOR BRAWL for human NPC duels only. Currently two engines share one fiction.
- **Weaken feedback**: an on-foot step is optional. Cleanest: ball thrown at a wild at full HP triggers "SOFT CATCH" with a visible % ring (battleCatchChance) and a 1/shake animation in the world; hit with the party mon's move (follower fights beside you, ship.js 9934) to drop HP bars shown over the creature. Wild HP bar floats over its head (lerp to 0 at catch %).
- **Approach design**: replace flee-circle with "alert -> curious -> bolt" states: crouch / slow-walk approaches (speed < 0.6 walk = no flee), lure fries/treats draw them in. This is the Pokemon Snap / Monster Hunter loop: stealth approach is the skill, ball is the finish.
- **Stakes**: lose = drop 10 % gorCoin and mons are KO'd until CVS heal; win = visible XP + drop. Currently nothing is risked.
- **UI**: HUD ball counter, party tray (6 pips with HP), battle UI already clean; add a post-catch card with the dex entry, shiny/size flair, and a "nickname" hook.
- **Trainers**: persist teams + heal at CVS; give each trainer a named lead, a quote, a rematch cooldown (FG.cd exists); badges as the progression gate for tier 2/3 balls.
- Keep names POKEMANS / GOR BALL / GREAT / ULTRA GOR BALL.

## (E) B-press trace (current line numbers; ship.js shifted ~+160 vs section A, cite these)

Path: keydown capture ship.js:9963 -> GB.aim=true -> each frame r27Foot (10039) -> gbTick (9915) -> gbPreview (9784) draws ball + 8 pips -> keyup 9969 -> gbThrow (9795) -> gbTick fly phase.

1. No target: B DOES draw. gbPreview has no target requirement; pips and held ball show whenever gbOk() (9756). It draws even with zero balls. So "arc shown only near targets" is false. No chat row appears on keydown; with no ball the error ("NO GOR BALLS · 7/11", 9800) appears only on RELEASE.
2. Arc = always a 12-H-class arc (previewDots, reach 20 H). Aim assist (gbPickAim) only latches inside 20 H / 20 deg. Beyond that the ball flies dead straight, no calm, lands, rolls, "MISSED" row (hit test 9690). The ball is NOT refunded on a miss (spent at 9801); refunded only on a hit (9769 region).
3. Consumption: stack matched by id 'gorball-N' (9801), N from gbOwned regex ^gorball-([123])$. 7/11 id = mkBall 'gorball-'+tier, stackKey same, giveItem stores r.id = stackKeyOf = 'gorball-N' (ship.js:6386). String match OK, bought balls ARE found. Hit test is planet-local (gbToLocal, GB.pp/GB.lp vs creature world body -> local); gbLocalize/gbProject re-project per frame; frames consistent. LOS check hasLineOfSight(GB.o, GB.p) (5386) can veto hits behind structures.
4. No party slot / dex / quest gate: gbCatch puts overflow in pen; trimMons only trims old.
5. try/catch: only calmCreature (9803), giveItem refund, battle import .catch (shows "battle failed to load" row, ball refunded). gbTick itself and gbPreview are unguarded.

All early returns on the B path:
- keydown 9964: e.repeat, ctrl/meta/alt held, state !== 'piloting', cmdOpen (set by inventory, store, chat, menu, dialog, battle, ring fight; cmdGuardUntil 600 ms after) -> SILENT.
- 9965: invOpen/storeOpen -> row "CLOSE INVENTORY". 9966: gmode !== 'foot' -> "ON FOOT ONLY".
- keyup 9969: !GB.aim -> silent (e.g. keydown rejected, or key released after GB.aim cleared); state !== 'piloting' -> silent.
- gbThrow: GB.on (ball already flying) silent; !gbOk -> row; no tier -> row "NO GOR BALLS".
- gbPreview 9786: !gbOk -> preview hidden silently (dead, panel open, ps.active !== land.node, e.g. standing where ps.active is another node or none: "NO PLANET UNDER YOU" only on release).
- gbTick fly: gbOk false mid-flight -> gbEnd silently (ball spent, nothing shown).
- No cooldown anywhere. Shift = highest tier (GB.best).
- gbTick runs only inside r27Foot (10045), AFTER mobTick() and footFireTick(); an exception in either skips gbTick for the frame, so no preview and no ball motion although the ball was consumed on release (5523 is not try-wrapped here; not verified at runtime).

Most likely single reason "nothing happens": the ball DOES fly but the target is out of reach and the creature never calms, so it ends as a silent "MISSED" row in the chat log. On any grazer/ox/strider you are outside 20 H and inside its 37 H flee radius, so every throw lands short, the creature is not calmed (calm only after aim lock, 9803, aim cone 9604-ish), and no hit -> no battle -> no catch. The throw "works" mechanically; the capture never happens. Secondary: `cmdOpen` still true (e.g. after closing a panel, chat, or an unresolved menu) makes keydown B return silently at ship.js:9964 with no message.
Not verified in a browser: confirm via console `ship._gb.hint`, `ship._gb.GB.aim` after holding B, and `ship._gb.battle()` (a debug battle bypasses the throw and shows whether the battle UI renders).
