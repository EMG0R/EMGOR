# changes-feel2 (2026-11-25)

## 0. Jetpack: likely glitch + simplification
Likely causes (ship-foot.js, before):
1. Hold Space 2 s flipped into FLIGHT mode: a second controller (flightStep) with its own orientation, camera and pose; the snap at the flip was the visible "glitch".
2. Jet thrust ramped 1x to 3.5x over 1.2 s (jetStep k), so thrust kept changing for over a second; release/re-press restarted it.
3. Shift dive was v0 * 2^(t/0.6) toward a target (exponential overshoot to 48 H/s) with a separate accel constant.
4. hum.vh (visual height) was eased 26/s on the ground but instant in the air: smoothing changed at takeoff/landing.
5. Indoor camera mode toggled from roof probes while flying over/near roofs (FOV + boom change mid-flight).
Now: FLIGHT mode, its HUD chip (.sh-flight), flightStep, FL_* and the fl.* fields removed (fl = { cr } only, landing crouch). One mechanism: hold Space in the air = burn.
Constants at the top of ship-foot.js, `JET`: RAMP 0.25 s, VMAX 14 H/s (vertical cap and target), DIVE_G 2 (Shift in air = 2x gravity, fixed), DIVE_CAP 48, VH_EASE 26 (same on ground and air) with VH_LAG 0.35 H max lag, ROOF_FREEZE 3.2 H (indoor camera held off while airborne above that over the floor).
Horizontal speed in the burn = run speed through the existing pilotRamp easing. Floor: landing already zeroes vv once (hr<=floor); no bounce path remains.
Not touched (not my files): js/ship-planet-tests.js still references fl.on (browser test 'Space hold ... flight', lines ~836-883): it will fail until that test is deleted or rewritten. js/ship-styles.js JET_K.RAMP/jetStep k are now unused by ship-foot.

## 1. Sit pose (ship-human.js, ship-foot.js)
- Rig API: `rig.pose = 'sit'|'lie'|''` (getter/setter), `rig.setPose(p)`, or `st.pose` in `update()`. Documented at the top of ship-human.js. 0.3 s smoothstep blend, breathing kept.
- sit: hips 0.12 above the group origin (seat top, about 0.4 H down), thighs horizontal, shins vertical, forearms resting, 8 deg lean. lie: supine (hips rot x +90 deg, head toward +Z, feet -Z), legs straight, arms at sides; origin = bed surface.
- ship-foot.js: squash (0.28 * SEAT.k) removed; calls o.setPose('sit') while SEAT.on and clears it only if it set 'sit' (so village beds can own 'lie'). Camera drop unchanged.

## 2. Prompt priority (ship.js)
Foot prompt: creature (TAME) and resource (HARVEST) prompts lose to the Z board line inside nearShip(); F still acts. Away from the ship F · TAME shows.

## 3. HUD (style/ship.css, ship.js)
- New tokens: --hud-warn, --hud-ok, --hud-ink-soft, --hud-white; all 28 raw rgba() replaced by color-mix() of tokens (0 left).
- Idle-hide extended (same mechanism, now 6 s): hides quest tracker, NPC dots, depth, weather, cvm, ribbon. Hotbar, HP, markers, hint bar stay. idleWake() also fires when quest/depth/weather text changes.

## 4. camClear
Not done (time spent on task 0).

## By-eye checklist
1. Sit on a stool/booth: legs bent, no squash, stand with W.
2. Hold Space on the ground-jump: burn starts after 0.18 s, full thrust in about 0.25 s, no orientation change at 2 s, no FLIGHT chip.
3. Fly over the Burger roof: camera FOV/boom stays steady; no indoor flicker.
4. Shift in air: constant 2x fall, no runaway acceleration; land: no bounce.
5. Stand next to the ship with a creature near: prompt reads Z · BOARD; walk away: F · TAME.
6. Stand still 6 s: quest tracker/NPC dots fade; hotbar and HP stay; new quest text brings them back.

## 6. Jet at the spawn pad: pushed 1.49 H down, no lift (browser repro)
Not reproduced in node (no browser); fixed by code reading. Path: ship-foot substep -> objPush(p) -> `if (hum.air && nl < hr) { hr = nl; vv = min(vv, 0) }`. objPush's parked-hull sphere (0.45 L at the hull origin, 0.4 L above the feet) resolved the capsule radially from the sphere centre: a foot sphere beside/below that centre gets a push with a -up component, so hr is lowered below the floor and vv is zeroed every substep, which is exactly "sunk, jet cannot lift, W (moving away from the hull) frees it". Box solids did the same for the lower sphere.
Fix (ship.js objPush): hull push-out drops any component along -up (sideways or up only); box push for the lower sphere drops -up too (the head sphere can still be pushed down by a real roof, which still caps vv). Result: the capsule can never be resolved below the floor by a hull/pad-side push, and the jet lifts unless physically roofed.
exitShip already places feet at mFloor (groundTruth) 0.75 L beside the hull, outside hull radius + foot radius; unchanged. If the harness still shows -1.49 H, the next suspect is ps.groundTruth vs meshFloorLocal on the pad lathe (read: groundTruth >= meshFloorLocal, so it cannot explain a sink), then hum.vh smoothing (vh now eased with a 0.35 H lag cap in air).
