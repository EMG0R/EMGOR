# AUDIT: on-foot feel, camera, HUD, vibe coherence (read-only; `node --check js/ship.js js/ship-foot.js` = OK)

Doc/code drift found first: HANDOFF says H_RATIO 0.2, jump 1.16 H / 0.53 s, walk 22 H/s. Code says H_RATIO 0.27 (ship-scale.js:5), walk 6 / run 13 H/s (ship.js:5178), jump apex 1.8 H (ship-foot.js:G_JUMP 5.34). The "22 H/s" is NOT the planet controller: it is the ship-interior controller (ship.js:8444 interiorStep, `spd=(run?44:22)*H` where H = 0.09*f, i.e. a separate scale). feel-plan.md line 5.5 already records "Walk 22 H/s -> 6". HANDOFF top section is stale.

## (A) Camera findings
- A1 Foot cam (ship-foot.js footStep tail, ~line 205-214): focus = head (1.0 H up, minus landing dip), camera 4.5 H back (`FOOT_CAM`, ship-foot.js:~19) x `styCam`, offset 0.35 H right shoulder, pitch -0.3..1.4 rad (mouse). FOV on foot 55 (ship.js:5396 targetFov; galaxy 42, `FOV` at galaxy3d.js:2688), near 0.01 L, far 200000 (log-depth-free, single huge range = z-fight risk far away).
- A2 Smoothing: `groundCam(dt, focus, tp, tq, 16)` (ship.js:5171) lerps the camera OFFSET in the focus frame with damp(16) and slerps rotation with damp(16). Rate 16 = ~60 ms time constant: tight, but position and rotation are damped independently, so rotation lags the position -> slight swim when turning. Flight variant uses rate 10.
- A3 Collision: ONLY a terrain-floor clamp: `mFloor` at the camera position, pushed out to floor + 0.04 L (ship-foot.js ~212; flight ~ same). No raycast to walls/huts/store boxes/ship hull. `objPush` (ship.js:5290) pushes the PLAYER capsule only, never the camera. Result: camera clips through hut walls, 7/11 box, the parked hull, and sits inside geometry whenever you stand with a wall behind you (4.5 H back = 8 m).
- A4 No pull-in / zoom: no wheel zoom on foot (wheel only used for photo mode, inventory, store lists), no spring-arm, no pitch-based distance. Over a steep slope behind you, the floor clamp slides the camera along the ground (OK), but nothing handles ceilings/roofs.
- A5 Hub (dungeon) HAS the correct pattern: ship.js:7587-7640 hubFootStep marches a ray from head back in 0.4 m steps to 5.5, stops at floor/ceiling/hubFree, snaps IN instantly (`if (dm < HB.camD) HB.camD = dm`) and eases OUT with damp(3). Writes camera directly (bypasses groundCam). This is the template to port.
- A6 Ship interior (ship.js:8444 interiorStep): fixed 6 H boom, clamped into the room AABB (`ikB.x clamp +-9.9`), groundCam 16. No wall ray, just a box clamp.
- A7 Landing dip `camDip` 0.05-0.35 H, decays exp(-9 dt): fine. Shake adds random jitter to camera.position (ship.js:5394), fine.
- A8 Where an indoor camera mode hooks in: (1) `footStep` tail before `groundCam`, using `objBegin()/opList` boxes (the same data objPush uses: o.inv, o.hw/hd/hh) as a ray target set + `village.walls` boxes; (2) a `hum.indoor` flag from `ps.inSite` / store `opList` containment -> shorten boom to ~2 H, FOV 62, pitch floor. Smallest change: replicate A5 as a function `camPull(gD, gC, qM)` returning clipped distance, called from footStep and flightStep.
- A9 `styCam`/`S.styFov` let styles scale distance/FOV (damp 4 / 8): second writer to FOV besides targetFov; feel-plan F5 says nothing else should write it.
- A10 Five camera behaviours in one file: chase (ship), foot, flight, hub, interior, plus cine/photo/fight. Each has its own damping constant (2.6, 10, 16, 16, 3).

## (B) Movement constants (code truth; H = human height; 1 H = 1.8 m by ship-scale HUMAN_M)
| thing | value | unit | seconds / meters | verdict |
|---|---|---|---|---|
| walk (PILOT) | G_WALK 6 | H/s | 10.8 m/s | Too fast for "walk" by 3x; it is Usain-sprint. Fine for SM64-style game feel IF world is scaled to it, but a plushy world wants slower. Rename walk=jog. |
| run (Shift) | G_RUN 13 | H/s | 23 m/s | 2.2x walk, faster than any sprint; pairs with 0.27 L H, i.e. a ship is 3.7 H, so run crosses a ship in 0.3 s. Reads as "motion blur" with 55 FOV. |
| accel / friction / turn | LOCO ACC_T 4/60 s, FRIC_T 4.5/60, TURN_T 2/60 (ship-styles.js:58) | s | 67 / 75 / 33 ms | Near-instant (SM64 table). Crisp but "digital"; no easing in/out = not plushy. Pilot dir change is instant, only reversal brakes. |
| jump | G_JUMP 5.34, up-g 7.93, fall-g 16 | H/s, H/s^2 | apex 1.8 H (3.2 m), rise 0.67 s, asymmetric fall | Floaty-up, snappy-down is good. Apex 1.8 H is large vs 4.5 H boom (head leaves frame top). HANDOFF "1.16 H" stale. No coyote time, no jump buffer (only edge `jumpHeld`). |
| jetpack | JET_UP 3 H/s climb (x jetK, ramp A 60), hold 0.2 s in air | H/s | | Hold-Space-in-air = jet; style jump vs tap handled by jetStep. OK. |
| flight (hold Space 2 s) | FL_V 36 H/s cap, thrust 70/60 H/s^2, Shift x2 = 72 H/s | | 130 m/s, 260 m/s | Reasonable as a "mini ship"; planet-scale ok. |
| dive | DIVE_V0 3, cap 48, tau 0.6 | H/s | 2^(t/0.6) | Exponential growth in 3.6 s to cap. Fine; landing impulse maps to dip + dust. |
| landing | fl.cr squash 0.22 H scale, recover 3.2/s, camDip, dust > 0.35 | | ~0.3 s | Good, only code feel feature that is "plush". |
| mouse | MOUSE_SENS 0.0019 x 1.3 rad/px | | | Applied directly to hf and pitch, undamped (good for aim, harsh for plush camera). |
| MARO (live) | MAX 6.0 H/s, ACC 6.2, V1..V3 7.9/9.75/12.9, G 22.5 | | | SM64 table; `maroTable` newer pinned table is not live (see F). Long jump 4.4 H vs gate 9 H (HANDOFF). |
| hub | walk 10 / run 18 m/s, jump v 7.5, g 22 | m | | Different unit system (meters), unrelated to H; walk is 3x human jog. |
| interior | 22/44 x (0.09 L) | | | Own scale; fine if interior is small. |
| footstep audio | stepPh += 2.4 x clamp(spd/G_WALK,.5,1.8)/s | | ~2.4 Hz at walk | cadence not synced to rig phase (see below). |
Hardcoded in two places: ship.js G_WALK/G_RUN passed in; ship-foot has a stale comment line about "walk 6 / run 13" next to a differently-stated "apex 1.2 H" comment on the next line (comment/constant disagreement, G_JUMP=5.34 gives 1.8).

Rig animation (ship-human.js:492 update): YES, procedural. Phase-driven leg/arm swing, hip bob (|cos|), torso lean 0.05-0.38 rad scaled by run, counter-twist, head counter-lean + look-around, blink, idle breath, landing squash, wings/orbs per style, jet vibration. Constants are per-rig (GAITS, ship-styles.js:520). It is a good base: stubby capsules + round mitts + round boots (ship-human.js:173-180). Weakness: at 4.5 H boom a 1 H human is ~13% of screen height; walk cycle phase is not tied to ground speed (cad param is only scaled), so at 6 H/s feet likely slide; no turn lean/banking, no anticipation on jump, no blend between jump/fall/land poses other than airborne flag + squash.

## (C) HUD inventory + clutter
Sources: one big `hud.innerHTML` template (ship.js:773, 42 distinct `sh-*` classes) + ~33 more elements created by `createElement` (ship.js:877,5238,6313,6445,6780,7847,7893,8119,8490,9245,9959,10070,10106,10458,10527) + `.ship-label` (galaxy page chip) + #ship-chat. CSS: style/ship.css (855 lines, 83 selectors) + ship-chat.css (259) + pokemans-battle.css. ~75 HUD element classes total.
On foot, simultaneously visible (typical village scene): prompt (`.gor-prompt`), hint bar (fades 10 s), hotbar (`.sh-hot`), HP (`.sh-hp`), party strip (`.sh-party`), quest tracker (`.sh-quest/.sh-qt`), HUD markers (ship + 3 + quest = up to 5, pooled DOM), toasts (`.gor-toasts`), chat log + line, coin/gor counter, weather `.sh-wx`, radar `.sh-rad`, depth chip, boss bars (3 + pips, fights only), NPC name dots, F/T prompts, inventory tooltip. ~12-15 elements on screen in a normal moment.
In flight: ribbon speed, atmo, thr/pul bars, ret reticle, target, arrows (edge), roll/chain/pip/lock/graze/focus/incoming/flip/drift indicators, wave/st-* counters, dmg numbers, hit marker, hp, entry, alt, edge... ~20-25 elements. That is a fighter-jet HUD, not minimal.
Clutter rating: on foot 6/10, in flight 8/10.
Fonts: `--hud-font: Monocraft` (a Minecraft pixel font) + `var(--mono)` (system mono) mixed; sizes 10/11/12/14px; ALL CAPS with 0.04-0.12em tracking everywhere. Palette tokens exist (ship.css:5-13: hud-bg #0d0221@.72, hud-line purple, hud-ink #E0AAFF, hud-hot cyan, hud-alert #ff3a4a, hud-coin #FFD36A) BUT raw rgba() are hardcoded all over (18x rgba(2,0,8,.95), 7x rgba(61,29,142,.55), 6x rgba(255,58,74,.9), 3x #00f0ff...). Radius: 9x 50%, 3x 12px, 3x 0, 4x 0 in chat, 1x 6px, 1x 3px: no radius token. Symbols: letter glyphs ("Z ·", "F ·"), voxel/pixel icons (ship-icons.js, 441 lines), SVG ring prompt (gp-ring), emoji-free. Mixed: bordered boxes (square) vs pills (12px) vs circles.
Centralize: all of this already hangs off `:root` in style/ship.css top: add `--hud-r` (radius), `--hud-sp`, `--hud-font-size-s/m`, replace the ~60 raw rgba()s with tokens (sed-able), drop Monocraft for one rounded face (e.g. Nunito / Quicksand via Google Fonts is permitted on artifacts but check site CSP; else system-ui rounded `ui-rounded`). Canvas-drawn bits: only `ship-icons.js` and fight overlay; the rest is DOM so one CSS file controls it.

## (D) Vibe coherence
Visual languages present, with clash level (C=clashes with the target):
1. NMS space/flight HUD + procedural planets (galaxy3d/ship-planet): cyan/violet on black, thin lines, bloom + ACES. Coherent with site; slightly sharp.
2. Voxel Minecraft inventory + Monocraft font + 3x3 craft grid (ship-items/craft/icons, elInv): C (hard pixel squares).
3. Pixel icons (ship-icons.js): C vs smooth capsule rig.
4. Rounded capsule human + pets (ship-human): the closest to the target; stubby limbs, round mitts. Keep as the anchor language.
5. Parts-kit procedural creatures (ship-parts/enemies, 15+ round primitives): medium; they are rounded but built from flat-shaded primitive assembly with lots of Toon/Basic mixes.
6. Village huts + 7/11 + CVS + hub (ship-village/world/hub): C: AABB boxes with pitched roofs, torches, fences, Lambert flat-shaded. Hard edges, realistic-ish proportions (3.1 m walls) vs plush rig.
7. 2D Smash-style brawl sprites overlay (smash-*): C: a different engine, 2D overlay on 3D page.
8. POKEMANS battle UI + dex: C mostly DOM cards (pokemans-battle.css, 43 lines).
9. Site chrome (galaxy.css): purple mono, outlined.
Renderer facts (galaxy3d.js:2671-2680, 2809-2823): WebGLRenderer antialias on, ACES Filmic tone mapping exposure 1.15, sRGB output, EffectComposer + UnrealBloomPass (strength 0.48, radius 0.45, threshold 0.82, scaled by tier). No shadow map used (grep castShadow: 0 hits), no fog on the planet surface scene (grep: no scene.fog), no outline/SSAO. Materials in ship-* files: MeshLambert (~30) + MeshBasic (~40) + a few MeshToon, 0 MeshStandard, 0 gradientMap. Planet: custom shader/ShaderMaterial in ship-planet. `flatShading`: 0 hits in js (smooth normals by default, but low poly boxes read hard).
Perf budget: bloom costs most (tiers already scale it); toon/lambert swaps are free; fog free; outline (inverted hull) doubles drawn tris on rig + creatures only: cheap; SSAO would be too expensive.

ONE unifying direction: "soft toy planet": everything is a matte, rounded, 3-tone toy (clay/felt) lit by one warm sun + purple ambient, planet and sky as smooth gradients, UI as soft rounded translucent pills with one font. Anchored on the existing capsule human.
Levers (cheapest first, with file targets):
1. Global material pass: one `softMat(color)` factory = MeshToonMaterial with a 3-step DataTexture gradientMap (or MeshLambert + emissive lift); replace Lambert/Basic in ship-world.js, ship-village.js, ship-hub.js, ship-human.js, ship-enemies.js, ship-parts.js, ship-station.js. S/M, ~60 call sites, mechanical.
2. Palette: 6 colors + player accent as one JS module (ship-palette.js) mirrored to CSS vars; recolor villagers, huts, 7/11, creatures through it (they currently pick per-file hex). M.
3. Rounded geometry helper `rbox(w,h,d,r)` (RoundedBoxGeometry from three/addons, already importable since addons are used by composer) used in huts, store shells, shelves, hull; roof as capsule/cone. M.
4. Sky/fog: `scene.fog` exp2 matched to horizon gradient color on the active planet, tied to ps biome; softens popping + gives depth/scale. S.
5. Bloom: lower threshold/higher radius on foot for a dreamy glow; keep ship tier logic. S.
6. Inverted-hull outline on rig, pets, creatures only (1.5% scale, back-face dark purple): reads as "toy". M.
7. HUD: one font, one radius (16px pills), one blur panel style; cull voxel inventory frame to rounded cards. M.
8. Shadows: blob decal under human/creatures/buildings (a CircleGeometry with radial alpha texture, 1 draw) instead of shadow maps: cheapest grounding for plush look. S.
9. Camera + movement ease (accel curves ~120-200 ms, turn lean, FOV on speed). M.

## (E) Interactivity gaps (seen, not touched / no feedback)
F-interactions that exist: shop counter, burger window, CVS heal/revive, shelf grab, resource harvest (`resFind`), tame creature (`tameFind`), wild encounter (`wildFind`, exists but ring-wild capture unwired), hub chest/door/dungeon, villager fight/talk, interior table/kitchen/board/pens/pods, station store/CVS/trade terminal.
Gaps from code and village/world module headers:
- Village: well (`village.well`), bell (`village.bell`), plots (farm), stall, torches, fences, huts: solid boxes with collision only. No F, no sound, no animation on touch. Doors are gaps, no open/close, no interior (huts are 6x5 m footprints with walls; you walk through the door gap into an empty or box-only room). Bell has a position and no ring action.
- Creatures: only TAME / throw ball / fight; no idle reactions to the player approaching (flee exists per notes), no pet/pat, no feed from hand.
- Resources: harvest gives item but "no resource-node part drops" (HANDOFF), no hit/shake feedback listed.
- Rocks (ps rocks API): decorative until fractal depth wiring; no break/kick.
- NPC villagers: greet within 3 H, talk (T) and fight (F twice). No quests from them beyond the board; no walking-to-you, no sit/bench.
- Torches/signs/stall goods: decorative, no light-on interaction, no shop on stall.
- 7/11 shelves on station deck missing; "E · TRADE unbound".
- Landing/step: no footprints, no grass sway reaction, no splash; dust only on hard landings.
- Weather/wind: footWind pushes you, no visual cue at feet.
- Jetpack: has fx+audio; walls and roofs: roof-top standing works (touched logic) but no way to climb huts (no ladder).
Fast wins: bell ring + camera nudge, door swing/auto-open, well bucket, bench-sit (F, hums), chest-like poppable crates, plant plots (F sow/harvest), pat-creature (F, heart particles), footstep dust/grass puff, NPC wave animation on greet.

## (F) Half-wired systems: finish or cut
| system | where | state | recommend |
|---|---|---|---|
| maroTable | ship-styles.js:603, tests only; not referenced in ship.js | tested, not live; live `maro()` has MAX 6 table | Cut for now (second MARO is a duplicate mechanism; EMGOR rule "don't ship two"). Keep file behind test, delete when the plush movement rewrite lands. |
| fractal depth | ship-depth.js (72 lines), ship.js:5226 HUD chip, save `depthStack` (621,649), 2542 danger input | save + chip exist, dive/climb engine not wired | Cut chip + save field unless finishing; "DEPTH n" never changes. Finish only if rock-diving is a keeper. M-L. |
| ring-wild capture (`ft.wild`) | ship.js:8199 `wildFind`, PKC.wildEncounterIndex | wildFind exists; the ring capture entry never set | Finish: it is the core loop (catch). M. |
| ship.js split | ship-foot done; hubFootStep (7587), collision (objPush/footFloor), style bridge, interiorStep still in 10985-line ship.js | half | Finish only collision+camera split (needed for camera pull-in), cut the rest as non-goals. |
| grafts not visible on 3D follower; elder GRAFT counter; trainer mons don't heal | pokemans-graft/party | partial | Out of scope here; finish minimal (visible tint/part) or hide graft menu. |
| Q17b3 / rockId vs slot | planet rocks | unwired | tied to depth: same call. |
| station: no shelves, launch 4-8 s vs 2 s, no beam, E trade unbound | ship-station | gaps | Fix launch time (S), bind E or remove "TRADE" label (S). |
| `[guess]` constants | ship-styles.js 12, 78, 133, 190, 229, 257, 315, 319, 436, 520, 595, 10146 ship.js | tuned-by-play pending | Keep, but expose a single tuning table (JSON) for the playtest pass. |
| styles roster | MARO/JOSHI/STEEV/SONIK + SEER/JELLY...: 12 styles (feel-plan) | big surface, each with own jump/camera | Cut to PILOT + 2 max for "coherent" feel; mashup styles own jump/gravity (`styleEnv` grav 22, jump 8.5) conflicting with PILOT 16/7.93. |
| two floor sources (groundTruth/meshFloorLocal/floorLocal) + `blkFloor` for STEEV | ship.js:5185 | known (feel-plan) | Collapse to one; blkFloor only for STEEV. |
| two "camera behaviours" in hub vs foot | A5/A10 | duplicate | Replace both with one `camPull`. |
| legacy comments: "ship ~4-5 H" vs 3.7 | ship-scale.js header vs HANDOFF | stale docs | Update HANDOFF "CURRENT STATE" (H_RATIO 0.27, walk 6/13, jump 1.8 H). |
Grep notes: TODO/unwired/FIXME hits are almost only `[guess]` markers; devs documented unwired items in HANDOFF instead.

## (G) Prioritized fix list
S (hours):
1. Camera pull-in: port hub ray (ship.js:7620) into footStep/flightStep using opList boxes + village.walls; snap-in, ease-out. Biggest visible "trash" fix. 
2. Fog + soft gradient sky color tied to biome; blob shadows under rig/creatures.
3. Lower walk to ~3.5-4 H/s, run to ~8; add 100-150 ms accel/decel ease and turn-lean (LOCO ACC_T/FRIC_T, pilotRamp). Retune camera boom to ~3.2 H + wheel zoom 2-6 H.
4. HUD: collapse the foot HUD to 4 things (prompt, hotbar, coin/hp, one marker); hide radar/weather/party/quest until relevant. One radius (16px), one font. Replace raw rgba with vars.
5. Update HANDOFF CURRENT STATE with real constants and the three stale docs.
6. Bell ring, door auto-open, pat-creature: cheap "alive" wins.
M (days):
7. softMat() toon/gradient factory + RoundedBox in huts/7-11/CVS/hub; palette module.
8. Port interiorStep to the same foot controller so one movement code path, one camera.
9. Finish ring-wild capture; fix station launch time/E trade.
10. Inverted-hull outline for rig + creatures; grass/dust/footprint feedback; foot-phase locked to ground speed (stride = speed / stride length) to kill foot sliding.
11. Cut maroTable + depth chip/save OR finish them (decide once).
L (week+):
12. Cut styles to PILOT + 2; delete mashup jump env; single floor source.
13. Real huts: interior (rooms, doors, furniture with F), indoor camera mode (A8).
14. Replace voxel inventory/Monocraft/pixel icons with rounded icons and one card style across inventory, store, dex, pokemans, fight HUD (the brawl overlay stays but gets the same panel skin).
15. Brawl sprites vs 3D plush: either re-skin sprites to rounded toon silhouettes or keep it as an explicit "arcade cabinet" overlay framed in the UI language (decision for the owner; other auditor).
