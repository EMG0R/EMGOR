# Playtest 1 (live emgor.online/?pt, rev 24, 2026-10-05, automated Chrome tab)

Limits: tab is throttled, frames stepped via EMGOR_GALAXY.step; teleports of shipRoot are unreliable under the floating origin, so landing / on foot / jetpack / 7/11 store / inventory / gas giant / black hole close were NOT reached. Mouse input needs pointer lock (document.pointerLockElement) to register; faked it. Console: zero errors/warnings captured (page-level window.onerror also empty).

## Screenshots
1. Home view: clean, planets and black hole read well. Good.
2. Boarding/first fly: HUD shows bottom key-hint line plus 7/11 marker at once; hint bar is long and tiny. Shield showed 3R then recovered (took hits from wave 1 right at spawn).
3. Free flight/pulse: speed readout 40-174 U/S under stepping; ship + engine glow good; purple ribbon streaks (home black-hole lensing) dominate the sky.
4. Terra 3 R: globe reads great (teal/tan land, soft atmosphere rim). Slightly flat/cartoon, no cloud layer.
5. 2 R: land shapes OK, smooth transition, no haze problem. But 4 guidance chevrons + 7/11 + wave/enemy panel all pile up top-right/bottom.
6. 1.4 R: terrain patch is smeared/stretched (streaky brown cliffs), speed-line streaks look like glitch lines, ocean is one flat teal fill, no shore foam. Marker labels overlap the WAVE/ENEMIES readout (BURGER HOUSE on top of ENEMIES).
7. 1.1 R / hover over sea: whole screen is a featureless dark-teal wash. No horizon, no waves, no sense of altitude (ALT 0.8 L). Water reads as a void.
8. Esc then re-enter: re-entry did NOT resume at the planet; ship came back in open space at the home system with stale "ALT 0.8 L" HUD text. Rev 24 item 1 failed (or my teleport broke it).
9. /wave 3: boss warning banner covers centre sky; boss "idle" at 41 units while I sat still; 16 enemy objects but HUD says ENEMIES 3. Boss never visible on screen.
10. Firing: 62 bolts fired, 2 hits (my aim was not on target, but boss bar showed no hit feedback).
11. /wave 8: three bosses (crab, wraith, crab) at 190/423/441 distance, all idle, none on screen; three stacked boss bars eat the top 20% of screen. Two crabs = samey.
12. Station interior: dark box with tan flat panels, texture streak artifacts on the left panel; readable but ugly; 7/11 marker works.
13. Black hole close: could not reach. Frames show home planet clean instead.
14. Esc hold 3 s: radial UI appears; did not exit within stepped 3.8 s (may be wall-clock based, unverified).

## Ranked punch list
1. Ocean has no detail (flat fill, no horizon/waves/foam/specular): add animated normal-wave shader + horizon fog + shore band. js/ship-planet.js
2. Esc/re-enter does not restore pose (came back in space, stale ALT): verify frozen-frame restore for planet-local poses. js/ship.js
3. HUD clutter: guidance markers overlap WAVE/ENEMIES panel and each other; cap to nearest 2, move wave panel, hide key-hint bar after 10 s. js/ship.js + style/ship.css
4. Terrain at 1.4 R smeared/streaky (stretched UVs on slopes): triplanar/steeper-slope colour variation, add detail noise. js/ship-planet.js
5. Streak lines at low altitude look like glitch geometry: fade/shorten speed lines below 2 R. js/ship-fx.js
6. Bosses idle and off screen on spawn: spawn in front of the player within 150 units, start in strike. js/ship-enemies.js
7. Three simultaneous bosses at wave 8 with stacked bars: show only the nearest boss bar, small pips for others. js/ship.js
8. Two crab bosses at once (samey): enforce distinct body type per wave. js/ship-enemies.js
9. Bolt hit feedback weak (62 fired / 2 hit, no boss bar response seen): add hit sparks + bar flash, check boss hit radius. js/ship-weapons.js
10. Station interior dark, panel texture streaking: add emissive strips, fix UV on tan panels. js/ship-station.js
11. Boss warning banner blocks screen centre: move to upper third, 1.5 s. js/ship.js
12. Enemy count HUD (3) vs actual list (16): decide what counts (incl. minions) and show it consistently. js/ship.js
Not verified (needs human or better harness): landing, on-foot, jetpack, store, inventory, gas giant, black hole, shake, docked-ship motion, see-through ground.
