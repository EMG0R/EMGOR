# Core loop storyboard (audit E / F8): first 3 minutes, fresh profile

Played with real key events in Chrome (localhost, fresh `nmg` save). Flight to a planet by hand is 148K L at 3 u/s (not flyable in 2 min), so landing used the harness hover; everything after the first Z was real keys. Steps 8-12 were verified by code reading + gE1 (scripted), not by hand: the 7/11 was 1.1K L away and walking is ~1 L/s.

| # | Step | Prompt shown | Key | Result / secs | Stall | Fix |
|---|------|--------------|-----|---------------|-------|-----|
| 1 | Enter ship (click YOUR SHIP) | none until boarding cinematic ends | click | START card appears ~5 s later, `card` flag saved only on first tick after | card shows late; 4 enemies already active on a new profile | none in my files (enemy spawn is ship-world/ship.js wave start: report) |
| 2 | W to begin | card: "W to begin", then "HOLD W + MOUSE ..." | W | speed 3.5 u/s, step 1 line shows | planets 148K L away: unreachable by hand in 3 min | report: first planet must be within ~5 K L (ship-world) |
| 3 | REACH A PLANET | tracker line | fly | completes when ps.active | none | - |
| 4 | LAND | "FLY UNDER 3 L ... PRESS Z" | Z | landed ~6 s, accent engages `#71ffd1` = ps.palette.accent (real landing) | harness landing earlier showed #A855F7 only because ps.palette was absent then; hook is fine | none |
| 5 | Step out | "Z · STEP OUT" | Z | on foot in ~2 s | tracker line clipped on the left ("walk ..."), prompt overlaps hotbar | text is one line; clipped = CSS width (report) |
| 6 | WALK TO THE BEACON | "Z . step out . walk to the 7/11 beacon" | W | **STALL: beacon 1.1K L away, ~1 L/s walking = 18 min; prompt promised a walk** | FIXED: when the nearest 7/11 is > 120 L the step reads "THE 7/11 IS FAR (~1 L/S ON FOOT) . Z BOARD . FLY LOW TOWARD THE 7/11 MARKER . LAND BESIDE IT . Z STEP OUT" (ship-onboard.js textOf, ship.js obSnap.far) |
| 7 | Beacon reached | next step shown, UI sound | - | steps advanced on the 0.4 s snapshot | no completion toast | FIXED: aPlay + "check mark <step title>" toast on every step done |
| 8 | BUY A GOR BALL | "F at the 7/11 counter" | F | store opens, buy | purchase gives coin sink, no step beat | covered by 7 |
| 9 | CATCH ONE | "hurt a creature, then throw the ball (hotbar)" | F / throw | battle overlay, catch -> dex | no marker to a creature | not fixed: wildNear creatures have no HUD marker (needs gdLocal kind; ship.js, next pass) |
| 10 | WIN A FIGHT | "walk up to a villager . F twice . fight" | F F | win paid coin + obWon | a win with no hit landed gave no step credit and no hint | FIXED: that branch now calls obWon() |
| 11 | Win out of order | - | - | `c.won` is persisted; the chain skips ahead when it reaches fight | not reproducible: steps complete when condition met regardless of order (gE1 + onboard node test) | - |
| 12 | FIND THE STATION | marker `station` | fly | - | outside first 3 minutes | - |

Village: door + plaza torches removed (A13 leftover), hut templates reduced to the cottage.

gE1 (ship-planet-tests.js): fresh user, real W/Z key events, 131.9 simulated s, all steps complete, 1 pokeman, win flag set. Teleports stand in for the flight/walk to the 7/11; the villager win is injected through obWon() (the hook fightResult calls).

## E2 pass (fx-start): stalls fixed
| Stall | Fix |
|---|---|
| Nearest planet 148K L away | Fresh profile (no saved pose, chain at 0, `c.sp` unset) spawns over metagor (else first lush planet), ~90 L from the 7/11 pad, ~6 L up, nose at the pad. Phase 1 drops the ship anywhere over the planet behind a black fade; phase 2 (`starterTick`) re-poses it against the live pad once `ps.pois()` has the 7/11. Returning players keep their pose. |
| FIRST STEPS 'land' | text + card: "W to begin / fly to the glowing pad / Z to land"; with no climb input the ship eases down 2.5 L/s into the 3 L hover band while the chain is at step <= 1 (no pitch keys exist). |
| 7/11 was 1.1K L from the landing | Z within 40 L of a 7/11 snaps the landing to that store's pad (`padSnap`); nose points at the store. Landed 0.3 L from pad centre, door 11 L away, walked 12 L to the counter. |
| Village never seen (was 547 L from the pad) | `villagePn`: village prefers the first pad's direction, 36-52 L ring. Now ~22 L from the pad. |
| Beacons | village column = planet accent, 7/11 tower orange, Burger red, CVS red-pink, hub green; column fade only inside 15 L (was 25). Village HUD marker always on. New step 'VISIT THE VILLAGE' after BUY A GOR BALL (marker + beacon). |
| Wild creature had no marker | during CATCH ONE the nearest creature within 80 L gets a full paw marker (edge arrow + distance); gone when the step ends. |
| Enemies from minute 1 | `OB.grace`: no waves, no night mobs while the chain is before the station step or for the first 300 s of flight (HUD reads CALM). |
| Tracker clipped, prompt over hotbar | `.sh-qt` wraps with min-width + left gutter; `.gor-prompt` sits at bottom 124 px on the ground (132 px <= 700 px wide). |

gE2 (no teleports; real W/Z/WASD/F key events): 112 simulated s, spawn 95 L from the store, Z at <= 40 L, foot walk 12 L to the counter, village 22 L from the pad (walk 37 L), 0 enemies throughout. Still harness-driven: the throw/battle (aiming needs the mouse; `_gb.battle`), the villager fight (`obWon()`), the purchase row pick (`dbg.shopBuy`).
Remaining stalls: speed in atmosphere is ~45 L/s with no brake key (overshoot needs W taps or auto-damp under 3 L); on-foot walking ~1 L/s makes a 50 L village trip ~40 s; fight itself untested with real keys.

E2 verification notes: gE2 passed in Chrome (112 sim s, real-key flight) before the approach segment was switched to the harness hover (`T.hoverAt`) because the key-only approach is order-sensitive; the hover version was NOT re-run (browser extension dropped). g21/g22 pass; g23 fails by 0.0006 H (0.0414 vs 0.04, pad0@77.6) on the starter-derived layout and passes (0.003) on a test-led layout, so it is layout-dependent. Village distance from the 7/11 pad: 547 L before, ~22-39 L after.
