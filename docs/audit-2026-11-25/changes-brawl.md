# GOR BRAWL changes 2026-11-25 (builder: brawl)

Owner: "not fun to play, faster motion, CPUs actually hard to beat". Files touched: `js/smash-phys.js`, `smash-moves.js`, `smash-engine.js`, `smash-bot.js` (rewritten), `smash-world.js`, `smash-ui.js`, `smash.html`, NEW `js/smash-geom.js`, tests `smash-bot` (rewritten) / `smash-fast` (new) / `smash-phys` + `smash-feel` (pin CLASSIC). Nothing committed. `ship.js` untouched (see NEEDS).

## 1. Physics presets (smash-phys.js: `PHYS`, `PRESETS`, `setPreset(name)`, `onPreset`)
`FAST` is the default at import; `CLASSIC` is selectable (`setPreset('classic')`, smash.html toggle, `SMASH.preset('classic')`). `setPreset` patches PHYS in place, clears the physOf cache, and rebuilds the data-driven normals (startup/endlag/landing scale are baked at build time). Both net peers must run the same preset (default FAST on both).

| Constant | CLASSIC (before) | FAST (default) |
|---|---|---|
| gravity | 0.171 px/f^2 | 0.252 (0.14 u) |
| fall cap / fast fall | 3.06 px/f / 1.6x | 4.32 / 1.7x, and the fast fall SNAPS to speed, D held counts (was: needed a fresh press, ramped slowly) |
| full hop | 49 air frames | 39 (apex still 1.45 H) |
| air accel / air speed / drag | 0.072 / 1.71 / 0.036 | 0.234 / 2.34 / 0.072 |
| run speed range | 2.16..3.24 (by weight only) | 3.06..4.32 (by def.run: swift 3.94, racer 4.32, hum 3.06, hauler 3.19) |
| ground accel / dash | 0.4, no burst | 0.8; first 6 f at 1.15x cap (accel 0.9) |
| dash dance | none (forced 4 f turn) | reversal inside the first 8 f of a run flips instantly (no turn state); later reversals pivot |
| traction / stop mult | 0.108 / x3 | 0.15 / x4 |
| input buffer | 6 f | 8 f |
| launch speed / decay | 0.054 / 0.0918 per kb | 0.065 / 0.11 (KO ~15-20 % sooner; 140 % fsmash flies 1.5 s) |
| hitstun | floor(0.4 kb) | floor(0.36 kb) |
| hitlag | floor(d/3)+4 | same, +30 % when kb>=60, 14-18 f when the launch arc leaves the blast zone |
| landing lag generic / air-dodge | 4 / 6 f | 2 / 4 f; aerial landing lag x0.75 |
| move startup / endlag | x1 / x1 | x0.9 / x0.8 |
| air dodge | speed 3.2, intangible 3..30, 40 f | 4.5, 3..22, 32 f |
| count-in | 150 f | 90 f |
| sim speed (local / CPU / NPC fights) | 1.0 | 1.15 (accumulator in smash-engine `loop`, smash-world `update`; host/guest net stays 1.0; cap 6 steps/frame) |
| camera width / lerp pos / zoom | 7..20 H / 0.08 / 0.04 | 5..12 H / 0.10 / 0.12 (fighters = 22-25 % of screen height at 4.5 H apart; world ring clamp 5..13 H) |

`physOf` under FAST honours `def.run / grav / fall / air` (reference units; clamped ratios) so the 13 fighters differ; CLASSIC keeps the old weight-only formula. Stage size unchanged (12 H, blast 16/14/7 H).
Other engine fixes made on the way: attacker order in `detectHits` rotates per frame (same-frame trades no longer always go to P1); 1v1 respawns on the two outer pads (P1 used to respawn at centre = seat advantage); up specials (noGrav moves) can snap to the ledge on the way down.

## 2. Impact (engine events + view, view-only, not in the sim hash)
Hit event gains `ko`, `str` (0/1/2), `lx/ly` launch vector. Hitlag scaled per above. Sparks: str 0 = star burst, 1 = + 4 directional streaks, 2 = + 8 streaks + impact flash. KO-range hit: zoom punch 0.16 (decays x0.8/f), KO event punch 0.1, shake cap 14. Launch trail behind a victim faster than 3.2 px/f. SFX through the existing `sfx` hook, 3 strengths: `hit_light`; `hit_light`+`hit_heavy`@0.85; `hit_heavy`@0.75 + `hit_crit` (smash.html maps `hit_crit -> explosion`). `ship.js` passes no `onSfx` today, so in-world hits stay silent until it does.

## 3. Bot ladder (smash-bot.js, `BOT_TIERS[1..9]`, flags not tier equality)
| L | react f | decide every | atk | whiff | shield | notes (flags) |
|---|---|---|---|---|---|---|
| 1 | 24 | 12 | .15 | .35 | 0 | random pokes, 20 % recovery skip, may run off edge |
| 2 | 18 | 8 | .25 | .20 | .30 | |
| 3 | 14 | 5 | .40 | .12 | .55 | grab OOS, recovery always (rec 1) |
| 4 | 10 | 3 | .55 | .06 | .70 | DI to centre, tech .5, short-hop aerial approach .35, projectile specials, punish stuns (punish 1) |
| 5 | 8 | 2 | .70 | .03 | .80 | tech .8, spot dodge / roll .3 |
| 6 | 6 | 1 | .80 | .01 | .88 | punish whiff endlag, survival DI (arc simulation over 9 sticks), ground ledge guard, full hop .2 |
| 7 | 4 | 1 | .90 | 0 | .93 | fast-fall, out-ranged bait (<=0.6 s), mixed ledge getups, full hop .4 |
| 8 | 2 | 1 | .95 | 0 | .96 | offstage edgeguard (jump out with DJ + up-B kept), kill-confirm (launch-arc KO test on moves and throws) |
| 9 | 1 | 1 | 1.0 | 0 | .98 | kill-percent planning, dash-dance spacing, habit reading (shield / attack counters), dodge .75 |
Common: exact hurt/hit capsule tests (BRAWL_API) instead of box reach; move choice = score + seeded noise (`vary` 9 -> 1.5) - repeat penalty over the last 3 moves; neutral special = projectile when the def has one (detected from bN); recovery rewritten (double jump first, up-B at the apex, aims at the ledge zone, gets out from under the deck; fighters whose bU does not rise are excluded). `smash-geom.js` (`BRAWL_API`: hurtCaps, hitCapsuleWorld, hitTouches, launchArc, koPct, launchDir) breaks the engine<->bot import cycle. Engine maps `level` 1-9 (default 6) / `tier` straight to this ladder.

Harness (pooled over pilot, swift, hauler, ranger, both seats where noted, 120 s, 2 stocks):
- L9 vs idle: KO inside 600 f of the go in 15/15 (5 seeds x 3 fighters; measured 255-510 f).
- L9 vs L1: 10/10 (pilot), 40-0 pooled. L7 vs L3 40-0, L9 vs L5 36-4, L5 vs L2 >= 80 %.
- L5 vs L5: 16-24 pooled (even within noise).
- vs the mash-fsmash dummy: L1-L3 lose, L4 50 %/10 %, L5 95 %/30 %, L6 100 %/95 %, L9 100 % (MARO) / 65 % (SONIK).
- Recovery: L3 and L9 return from 32 offstage positions per fighter (MARO, SONIK, HUM, JOSHI); fails only where physically out of reach (see test).

## 4. NEEDS ship.js (not edited here)
`ship.js:8907` (FG_TIER) and `:8963` (fighter record). `smash-world.js` already remaps legacy `tier` 1/2/3 -> L4/L6/L8 so an unpatched ship still gets a real fight; replace it with explicit levels:
```diff
-    var FG_TIER = { villager: 1, shopper: 1, clerk: 1, cashier: 1, farmer: 1, kid: 1, trader: 2, dealer: 2, guard: 2, elder: 3, golum: 3 };      // bot difficulty: 1 easy, 2 medium, 3 hard (smash-engine BOT_TIERS)
+    var FG_TIER = { villager: 3, shopper: 3, clerk: 4, cashier: 4, farmer: 3, kid: 3, trader: 5, dealer: 5, guard: 6, elder: 8, golum: 9 };      // CPU level 1-9 (smash-bot.js BOT_TIERS); villager L3-4, trader/guard L5-6, elder L8, golum L9
@@ line 8963
-isBot: true, id: info.id, level: role === 'elder' || role === 'golum' ? 3 : 2, tier: FG_TIER[role] || 1 }
+isBot: true, id: info.id, cpuLevel: FG_TIER[role] || 4 }
@@ line 9028 (xp scaling keeps working: FG_TIER[role] is now 3..9)
-(FG_TIER[info.role] || 1) * 4
+Math.ceil((FG_TIER[info.role] || 3) / 2) * 4
```
Optional: pass `onSfx: (n, p) => aPlay(...)` to `createFightSession` for the layered hit cues (`hit_light`, `hit_heavy`, `hit_crit`). Rolling speed: NPC fights already run at 1.15 because the world session is mode `local`.

## 5. By-eye checklist (play smash.html)
1. Start a match: count-in is 1.5 s, motion reads clearly faster; switch PHYSICS to CLASSIC (top right) and back: FAST should feel snappier, CLASSIC floaty.
2. Full hop is quick (~0.65 s), holding down after the apex drops you at once; short landing lag after aerials.
3. Tap a direction twice fast: dash burst; flick left/right inside the first 8 frames of a run: dash dance (no turn lock).
4. Fighters fill about a quarter of the screen height at mid range; the camera zooms in quickly when you close in.
5. Light hit = tiny freeze + sparks; heavy hit = longer freeze, streaks along the launch direction, shake; a hit that will KO freezes ~15 f with a zoom punch and a layered hit sound.
6. Flung fighters leave a trail; KO flash + slow-mo still there.
7. CPU LEVEL 1-9 (top left, default 6): L1-3 are punching bags, L6 shields / dodges / punishes whiffs, L9 edgeguards and does not repeat one move; none of them SD off a recovery they could make.
8. In the world: elder (L8) and golum (L9) after the ship.js diff above; unpatched ship gets L4/L6/L8 for tier 1/2/3.
