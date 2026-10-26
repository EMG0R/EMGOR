# GORCAVE LOG — unattended builder runs (append-only, newest at the bottom)

Started 2026-10-26 by the game builder manager. Format per entry in docs/GORCAVE-BUILD-HANDOFF.md.

## Q01 Error logging (warnOnce) — 2026-10-26 — see commit "Q01 Error logging"
DONE: `warnOnce(tag, e)` added once at ship.js top (before the tuning vars; 30 s per tag, console.warn). Wired to: space.update in dkTick (`space.update`), r25CraftAvail, questScan resource + creature blocks, quest reward giveItem, mission item roll, pet floor (r27 follower), scan highlights (outposts/stores + space targets), hubLife/villageLife ticks (errT reset to 0 each failure so later failures log again; replaces the one-shot console.info). ship-net.js: local copy of warnOnce (separate ES module) for `onClockSnap`. DOM, pointer lock, localStorage, audio ignores untouched.
NOTE: `space.update` at the in-flight call (ship.js step, ~5607) has no try/catch at all; left as is (a throw there already surfaces as a frame error).
TESTS: node --check ok on ship.js, ship-net.js (copied to .mjs); tests/node does not exist yet, none run.
VERIFY ON MAC: load game, fly + land + walk a village with console open: no new warns in normal play; g-tests unaffected (no logic change).
OPEN: ship-net keeps its own 4-line warnOnce (module boundary); could be shared later.

## Q02 Quality hold while hovering low + villageLife pooling — 2026-10-26 — see commit "Q02 Quality hold"
DONE: ship.js:478 perf `hold` also true when `lf.on && lf.alt < 10 * L` (no tier change under a hovering ship near terrain). ship-perf.js `applyAll(objsOnly)`: `attach()` now calls `applyAll(held())`, so while held it only rebinds fx/ps quality and skips engine pixel-ratio/bloom/stars changes. ship.js villageLife (~7294): per-mob `{id,ref,pos}` + Vector3 replaced by a pooled `vgMobPool` (consumer ship-village.js:553 copies values per frame, no retained refs).
Allocation grep: no `new THREE.Vector3/Quaternion/Matrix4` inside footStep/groundStep/step bodies (only module-level declarations); landable 8 Hz left alone.
TESTS: node --check ok on ship.js, ship-perf.js (as .mjs); tests/node does not exist, none run.
VERIFY ON MAC: hover low over terrain with /quality auto: no tier flips under 10 L; fly up, tier changes resume; walk a village with mobs nearby, mobs still move/avoid (g-tests touching villageLife).
OPEN: none.

## Q03 Landing prompt + onboarding — 2026-10-26 — see commit "Q03 Landing prompt"
DONE: ship.js landing check (~5663): `ps.landable` now sampled whenever lf.on && lf.cap && alt < 3 L; new `landGnd` (landable ground) drives an auto-damp placed before the velocity chase (~5509): speed and vel *= exp(-7.7 dt) (≈0 in 0.6 s) unless pulse held or boosting (escape). Prompt `Z · LAND` still needs vel < 2, so it appears right after the damp. ONB_TXT step 3 now says "THE SHIP SLOWS ITSELF". New one-shot hints in onbTick (o.roll after step 2 in flight: A/D x2 roll, S x2 flip, CTRL drift; o.inv on foot after step 3: E inventory, hold Z summon, F fight, T talk). Existing hints (F harvest, T talk / F fight) kept. HELP string (ship.js:3785) and `.sh-hint` (768) already match the key map; unchanged.
TESTS: node --check ok on ship.js (as .mjs); tests/node does not exist, none run.
VERIFY ON MAC: fly down at a landable planet holding W (no pulse/boost): ship stops inside 3 L and `Z · LAND` appears without releasing; over water/steep ground no damp, no prompt; Space or Shift still lets you climb out; new profile: hints show once for roll and for on-foot keys; g-tests touching landing/onboarding.
OPEN: hint bar (.sh-hint) fade is Q04.

## Q04 HUD declutter — 2026-10-26 — see commit "Q04 HUD declutter"
DONE: ship.js guideTick (~1560): after collection, per-item `d` (sq dist) + `hide`; only the GD_SHOW=3 nearest non-priority markers render, plus the ship marker (`is-ship`, on foot) and the quest target (`it.q`). Quest target = the `7/11` marker on a planet where an active quest has an undone `deliver` step (flag `gdQ7`, computed at 2 Hz in gdRefresh via activeQuests()). Pool size GD_N unchanged. Hint bar: `hintTick()` (called from updateHud) shows `.sh-hint` 10 s after entering, 6 s after any gmode change (landing counts as landed); `.is-faded` opacity 0 with 0.8 s transition (style/ship.css:148). Boss HUD: 3 bars (was 2), further bosses as `◆` pips (elBossPips). BOSS_ROWS stays 4.
TESTS: node --check ok on ship.js (as .mjs); tests/node does not exist, none run.
VERIFY ON MAC: on foot in a village: ship marker + 3 nearest NPCs/POIs only; with a deliver quest for that planet, its 7/11 marker stays even when far; hint bar visible 10 s after entering, fades, reappears 6 s on land / lift off / board / dock; spawn 4+ bosses: 3 bars + pips; marker edge labels still stack correctly.
OPEN: scan/retrieve/boss quest steps have no world marker (only deliver -> 7/11); not added.

## Q05 Fight balance — 2026-10-26 — see commit "Q05 Fight balance"
DONE: smash-engine.js: `BOT_TIERS` (tier 1/2/3 = reaction 30/18/9 frames = 0.5/0.3/0.15 s, attack chance/s 30/60/90 %) + `botAtkGate` (per-decision probability from the per-second chance); `botInput` uses tier react/lvl when `f.tier` set (tier 0 = classic behaviour, so GOR BRAWL menu bots are unchanged). Tier 3 only: edge-guard branch (stands at the ledge and swats a player off-stage) and chains attacks into hitstun (combo). Tiers 1-2 never edge-guard; recovery stays for all (else they self-KO). Fighter gets `tier` (createMatch slot.tier); smash-world.js passes `tier` through and adds it to SKIP. ship.js: `FG_TIER` by role (villager/shopper/clerk/cashier/farmer/kid = 1, trader/dealer/guard = 2, elder/golum = 3), passed in fightStart. Win with `res.dealt[0] < 1` -> "YOU WON · no hit landed · no payout", 60 s cooldown, no coin/goodgame. `FG_ASK_T` 3 -> 4 s; prompt shows `FIGHT? F AGAIN ◔◑◕● n` (glyph ring fills as the window closes). Rematch cooldown stays 60 s.
TESTS: node --check ok on ship.js, smash-engine.js, smash-world.js (as .mjs). Headless sim (idle player vs bot, 6 seeds x 120 s): avg damage dealt tier1 23, tier2 64, tier3 89; tier 3 won 6/6, tiers 1-2 did not finish in 120 s. tests/node does not exist, none run.
VERIFY ON MAC: fight a villager (slow, passive), a trader, an elder (fast, edge-guards, combos); stand idle for a whole match vs elder: lose; idle vs villager until time-out: if "won", no payout row; land one hit and win: payout + goodgame; second F within 4 s starts the fight, glyph ring advances; GOR BRAWL menu CPU unchanged.
OPEN: countdown "ring" is a text glyph in the interact label, not a drawn ring. Idle-vs-easy bots can time out rather than KO; time-out winner then follows normal result rules.

## Q06 MARO collider wall normal — 2026-10-26T16:31:11Z — see commit "Q06 MARO collider"
DONE: footStep (ship.js ~5115-5155): the old code already flagged `wallHit` on a failed straight step (not only when all slides fail) but counted any step-up > 0.05 H, so slopes triggered it and the normal was just -wanted. Now: a straight-step failure is a wall only if terrain rise > tan(70°) * step (`WALL_TAN`), reported even when a slide then succeeds; the first free slide direction gives the real tangent and the normal comes from new pure `wallNormal2(dx,dz,sx,sz,out)` (ship-styles.js, exported, no alloc: writes into styWallO). `hum.wallT` is now seconds SINCE last wall contact (init 9, reset 0 on hit, += dt each step); `ctx.wallNormal` is set while wallT < 0.12 (ship.js ~6788), same window as before. Ledge refusal (STEEV edgeGuard) still counts as a wall.
TESTS: node --check ok on ship.js, ship-styles.js (as .mjs); node --test: 4 pass / 0 fail (tests/node/wall-normal.test.mjs, new).
VERIFY ON MAC: run MARO into a vertical wall while airborne + Space: wall kick fires (g-test maroWall, ship-planet-tests.js:941, wallNormalFrames > 0, kicks > 0); walking up a ~45° slope must NOT kick; sliding along an angled wall keeps momentum; other styles unaffected.
OPEN: the 70° test uses the rise of the straight probe, not a true surface normal; a very short step on a stair edge could still read as steep if rise/step is large.

## Q07 Pre-push guard — 2026-10-26 — see commit "Q07 Pre-push guard"
DONE: `tools/banned-names.sh` (case-insensitive term list + case-sensitive Link/Peach/Tails/Toad/Steve/Notch), scans js/ docs/, exit 1 on a NEW hit. Tree already had 49 legacy mentions (Minecraft/creeper comments, quoted asks in HANDOFF), so they are grandfathered in `tools/banned-names.baseline` (file + text, no line numbers; `--baseline` regenerates). `banned-ok` on a line exempts it. `.githooks/pre-push` calls it; `git config core.hooksPath .githooks` set in this clone only. HANDOFF.md section "Pre-push guard (Q07)" documents it.
TESTS: bash -n ok; clean tree -> exit 0; planted `// Mario test` in js/ -> exit 1 (then removed). No js edited; tests/node not run.
VERIFY ON MAC: `git config core.hooksPath .githooks` in the Mac clone, then `bash tools/banned-names.sh` -> clean; push triggers it.
OPEN: baseline hides existing mentions (mostly Minecraft-style comments); a public-release scrub should clear them and empty the baseline. Hook is per-clone config, not versioned.

## BENCHMARK-1 — 2026-10-26 — see commit "BENCHMARK-1"
SCOPE: Q01-Q07 (range 641578f..HEAD). Diff (insertions/deletions, code only): ship.js 120 lines changed (~+80/-40), smash-engine.js 13, ship-styles.js +9, ship-perf.js 6, ship-net.js 5, smash-world.js 4, style/ship.css +2, tests/node/wall-normal.test.mjs +33 (new), tools/banned-names.sh +21, banned-names.baseline +49, .githooks/pre-push +2.
TESTS: node --test tests/node: 4 pass / 0 fail. No browser tests were run by the builder.
G-TESTS ON MAC (reload between batches): b1 g1-g4 · b2 g5-g8 · b3 g9-g12 · b4 g13-g15 · b5 g16-g17. Watch hardest: maroWall (ship-planet-tests.js:941, Q06), landing/onboarding tests (Q03 auto-damp), g14 parked-drift (Q03 damp could alter it), any test touching villageLife (Q02) or fightStart (Q05).
PLAY-CHECK:
- Landing prompt while cruising: hold W down at a landable planet, no pulse/boost; ship stops <3 L, `Z · LAND` appears. Over water/steep ground: no damp. Space/Shift still climbs out.
- Hint bar: visible 10 s after enter, fades, returns 6 s on land / lift off / board / dock. Roll/flip/drift and on-foot hints appear once.
- Marker count: on foot in a village, ship marker + 3 nearest + quest 7/11 only. 4+ bosses: 3 bars + pips.
- Villager fight: slow, passive, easy to beat after landing a hit. Elder: fast, edge-guards, combos.
- Idle-vs-bot payout = 0: idle vs elder loses; idle vs villager to time-out gives no payout row. Land one hit and win: payout + goodgame. F again within 4 s starts rematch.
- MARO wall kick fires on vertical walls, not on ~45° slopes. Quality tier does not flip while hovering <10 L.
- Console: no new warnOnce spam in normal play.
UNSURE:
- Q03 auto-damp (exp(-7.7 dt)) could interfere with g14 or any test that approaches the ground at low speed.
- Q06 uses terrain rise of the straight probe as the 70° test, not a true normal; stair edges may misfire.
- Q05 idle-vs-easy bots can time out rather than KO; tier numbers come from a headless sim only (tier3 won 6/6; tiers 1-2 did not finish in 120 s).
- Q04 scan/retrieve/boss quest steps have no world marker.
- Q07 baseline hides 49 legacy mentions; hook is per-clone config (Mac must set core.hooksPath).
- tests/node covers only wall-normal; nothing else in Q01-Q05 has automated coverage.

## Q08 MARO action table part 1 — 2026-10-26T16:36:32Z — see commit "Q08 MARO action table part 1"
DONE: ship-styles.js (before `wallNormal2`): exported `MARO_K` (spec constants in true H: g 22.5, v0 7.9/9.75/12.9, chain 0.17 s, acc 6.2, max 6.0, skid 11, long jump 9.0 fwd/5.6 up, back/side flip 11.6 up, side flip 1.5 fwd, crawl 1.9, triple needs >= 3.75 H/s), `MA`/`MA_NAMES` (idle walk run skid crouch crawl jump1-3 longjump backflip sideflip) and `maroTable()` = pure alloc-free state machine (`step(io, dt)`, own y/floor, exact ballistic integration). It is NOT wired into `maro()`: that still runs the env-scaled prototype (walk 22 / grav 22 / jump 8.5 in styleEnv, ship.js ~6591), whose units differ from true H; switching needs Mac feel-tuning, planned after Q09 completes the table.
TESTS: node --check ok on ship-styles.js (as .mjs); node --test: 12 pass / 0 fail (tests/node/maro-table.test.mjs, 8 new: accel, apex within 3 % for jump1/2/3, chain window boundary + slow break, triple speed gate, skid > 90 deg above 3 H/s and not at 89 deg / 2.9 H/s, side flip from skid, long jump / backflip, crouch/crawl).
VERIFY ON MAC: nothing yet (table unused by the game). maro() behaviour unchanged.
OPEN: with the spec constants a long jump covers 2*5.6/22.5*9.0 = 4.5 H, not the >= 9 H in BENCHMARK-2 / plan feel gate; either the gate means 9 H/s speed or LJ_Y needs raising (~11 H/s up) — decide at the switch-over. Walk/run split (RUN_T 4.0), crawl speed, skid duration cap 0.6 s, decel 9 and long-jump min speed 1.5 are [guess].
