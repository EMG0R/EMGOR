# POKEMANS rework: real-time wild encounter + POKEDEX (2026-11-25)

Code-complete, not browser-tested (no browser available to the builder). Pure logic is covered by node tests; the world glue is not.

## What changed per file
- `js/pokemans-wild.js` (NEW, pure): wild HP, aggro/flee state machine, attack styles, player damage scaling, partner 5 % clamp, live catch curve, per-shake roll, aim-lock cone.
- `js/ship-planet.js` (creature behaviour only): no flee-on-sight (`cr.flee = 0`); new `ps.setCreatureMode(id, mode, secs, stopH)` (0 wander, 1 chase, 2 flee); ALERT hold inside 8 H; jelly hover 0.5-1.6 L (<= ~6 H) and wobble 0.4 L; floaters drift in and hover at ~3 L instead of backing off; chase and flee speed caps.
- `js/ship.js`: WILD ENCOUNTER block (WL store, `wildBlast`, `wildTick`, `wildAttack`, chip + lock reticle + HUD text), `footFireTick` ray vs creature bodies, `wlCatchBegin/wlCatchTick` (shake phase `cshake` in `gbTick`), partner assist `ptWildAssist` in `ptTick`, `gbHint` only when a lock exists, B keydown says NO GOR BALLS, POKEDEX use, starter kit, roster (unlimited), single party chip, active partner in a free interior pen, follower uses six plans + size + planet ramp.
- `js/pokemans-catch.js`: BALL speed 30 / lift 0.12 / g 6 / reach 40 / tMax 3.0; AIM_CONE 12 deg, AIM_REACH 40, HINT_RANGE 45; PARTY_MAX 2000; `battleCatchChance` removed.
- `js/pokemans-gen.js`: six plans (`PLANS`, `PLAN_SPEC`), `bodyShape()` (height h in H, legs/arms/tail/wings, proportions, 3-slot palette), stat bias per plan + size, `PALETTE_SLOTS`.
- `js/pokemans-dex.js`, `js/pokemans-fighter.js`: new plan names mapped onto the existing silhouette drawers / fighter bases.
- `js/ship-human.js` (`createPet` only): builds biped / quadruped / serpent / floater / bird / blob from `rec` (or the seed), `size` scale, palette from `ramp` + `accent` (the planet's violet ramp via `ps.palette`); old 3 plans untouched.
- `js/ship-items.js`: `POKEDEX` item (kind `pokedex`, `itemFromKey('pokedex')`). `js/ship-icons.js`: `pokedex` 16x16 template.
- `docs/HANDOFF.md`: POKEMANS row, keys line, loop line, Known open.
- Tests: `tests/node/pokemans-wild.test.mjs` (NEW, 8), `pokemans-catch` (30/38/40 H aim cases), `pure-pokemans` + `gorball` (PARTY_MAX).

## Removed
- Pixel battle screen: `js/pokemans-battle.js` and `tests/node/pokemans-battle.test.mjs` deleted (`git rm`), `battleBegin`/`BT`/F-wild path (`wildFind`, `wildFight`, `ft.kind==='wild'`) gone. Trainers already run on the smash engine, so nothing needed the battle screen: it is removed entirely, not kept for trainers.
- 6-slot party cap, Alt+1-6 and `0` then 1-6 swap keys, `ptSwap`, TO PEN / TO PARTY buttons, per-mon bag stacks (`kind 'pokemans'` items are no longer created). Legacy `pen` ids fold into `party` on load.
- Left in place (not mine): `style/pokemans-battle.css` (dead), `ps.creatSprite*` hooks in ship-planet.js, `_gb.battle` is a stub returning null.

## Tuning constants
- `pokemans-wild.js` `WILD`: hpBase 60, hpPerStat 6 (L3 wild ~150 HP; starter blaster shot 12.8), alertH 8, fleeBelow 0.10, calmAfter 25 s, fleeSecs 8, chaseSecs 6, meleeH 2.2, rangedH 14, atkCd melee 1.6 / ranged 2.4 s, dmgBase 4 + 2.2/level (x0.8 ranged, +10 %/species tier), nameRangeH 25. Blaster never kills: a wild bottoms at 1 HP.
- Catch: `BALL_FLOOR` [0.05, 0.09, 0.15], `BALL_MUL` [1, 1.12, 1.25], curve `(floor + (1-hp)^1.5 * 0.9 * mul) * (1 - 0.06*(speciesTier-1)) + lure`. Basic ball: 5 % full HP, ~69 % at 20 %, ~88 % at 5 %. Each shake holds with chance^(1/3); shakes 0.5 s settle then 0.8 s each.
- `ship-planet.js`: `WILD_ALERT` 8*0.27 L, `WILD_CHASE_CAP` 2.8 L/s, `WILD_FLEE_CAP` 3.5 L/s (player run 12 H = 3.24 L/s, so flee <= 1.08x).
- `pokemans-gen.js`: height = exp(ln(PLAN_H) + 0.32 z) clamped 0.5-2.0, 5 % rare 2-3.5 H; `PLAN_H` biped 1.25, quad 1.0, serpent 0.95, floater 1.1, bird 0.9, blob 0.9; `STAT_BIAS` per plan.
- Follower scale = `sp.h * ind.size / 0.4` (kit nominal height 0.4 H).

## Not reached / caveats
- Wild creature body size in the world is still 2.6-4.5 L (~10-17 H) from `ship-planet.js` spawn code; the "closer to human height" variety applies to the caught POKEMANS (follower / interior), not the world creatures (outside creature-behaviour scope).
- Wild creatures all share the instanced planet mesh; chip name/level come from the seeded species, the world model does not change per species.
- `js/ship-planet-tests.js` browser scenarios that call `_gb.battle` (g24 etc.) need rewriting; `style/pokemans-battle.css` is orphaned.
- Wild attacks hit the player only (the partner is not attacked). Guardians (R27.gnd predators) are still catchable by ball using their own hp/hpMax and the same shake roll.
- Interior: the active mon takes the first free pen (no second pet system); if all 4 pens are full it is absent there.
- Full `tools/test-node.sh`: 417 pass, 14 fail, all in smash-* tests owned by other builders.

## 10-line by-eye checklist (local no-cache server)
1. New profile: pack has 3 GOR BALL + POKEDEX; B with none says NO GOR BALLS on keydown.
2. Walk toward a creature: no one runs off; inside ~8 H it stands and watches; a name/level/HP chip shows within ~25 H.
3. Blaster it: bar drops, it turns on you (lunge or violet spit), your HP drops; a jelly hovers within reach and floats in.
4. Your active POKEMANS pops out (if not out) and attacks; it never takes the wild below ~5 %.
5. Under 10 % it runs (not faster than your sprint); after ~25 s unhit it stands down.
6. "B . THROW" hint only shows when something is in the 12 deg cone; hold B: lock ring + CATCH n% on the chip; throw at 40 H works.
7. Ball wobbles 1-3 times: CAUGHT / BREAK OUT / MISSED show as big HUD text; break-out leaves the wild angry.
8. Right-click POKEDEX in the pack (E): roster opens; ACTIVE swaps who walks with you; U also opens it; no Alt/0 swap keys.
9. Caught mons vary: sizes ~0.5-2 H (rare 2-3.5), six body plans, violet-ramp colours; the pack holds no per-mon stacks.
10. Board the ship (Z): the active POKEMANS wanders in a free pen near you; F on a villager still starts a trainer brawl.

## Follow-up: world creatures share the size law, caught mon inherits the wild
- `ship-planet.js` spawn: `cr.hH` = log-normal about 1 H (0.5-2 common, 5 % at 2-3.5, same law as `bodyShape`); `cr.sz = hH * 0.27 L * plan factor` (walker 1.8, jelly 1.5, ox 2.2, strider 1.7; was flat 2.6-4.5 L = 10-17 H). Jelly hover 1.5-6 H. Alert (8 H), chase/flee caps and attack reach are H-based or read the live body radius (`creatBody`), so they rescale with the body automatically. New `ps.creatSize(id)` -> `{ h, v }`.
- `ship.js gbMon`: for a world creature the mon's `bodyRecipe` takes the wild's `h` and plan (walker/ox -> quadruped, jelly -> floater, strider -> biped), `ind.size = 1`; palette slots stay the species' (same planet ramp). Shot size == caught size.
- Deleted `style/pokemans-battle.css`. `ship-planet-tests.js`: g24, the F/ball scenario (~1498) and both chain scenarios now use `wildCatchLoop` (warp, blaster via `_gb.wildBlast`, odds rise, `_gb.catchNow`); new `_gb` helpers `catchNow`, `wildHp`, `creatSize`, `catchChance`. Syntax-checked only, not run.
