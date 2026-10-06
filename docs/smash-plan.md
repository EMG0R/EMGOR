# GOR SMASH — plan v2 (pure 2D)

Status 2026-10-06: plan only, nothing built. Supersedes v1 (2.5D, 6 fighters). Platform fighter, 2-4 players, browser, entered from NO MANS GOR.
IP rule, hard: moves, physics and roles follow the classic 64-era platform-fighter archetypes (that is game design, not IP).
Names, sprites, stages, sounds, UI chrome, logos are ALL original. No Nintendo/Mojang names or art anywhere, including code comments, file names, alt text.
Rule zero: SMASH never opened = ship.js is byte-for-byte unchanged in behavior.

## Decisions

- **Where:** arcade cabinet in the 7/11 (ship-station.js interior), `/smash` chat alias. Cabinet = lobby. (Unchanged from v1.)
- **Renderer:** dedicated 2D engine, own `<canvas>`, WebGL2 sprite batcher with Canvas2D fallback. No Three.js in SMASH. Own rAF; ship.js loop paused while open.
- **Net:** host-authoritative (kept). 3 stocks, items, 3 stages (kept; stage count 3 total, see build order).
- **Pixel look:** low-res internal 480x270 framebuffer, integer-scaled, nearest filter, violet-dark palette. Sprites 32x48.

## 2D engine

- Fixed 60 Hz sim (accumulator, max 5 catch-up steps), render interpolated. Sim has no DOM, no canvas import.
- World units: 1 u = 1 sprite px. Gravity, speeds below are in px/frame. Stage ~ 320-420 px wide.
- Camera: tracks fighter centroid, zoom 0.75-1.5 (fit all), clamped to blast box. Screen shake on hitlag.
- Layers per scene: 4-6 parallax layers (far sky, mid, near, stage, foreground). Scrolling by camera x * layer factor (0.1/0.25/0.5/1/1.2).
- Sprite batcher: one atlas texture per fighter + stage tiles; one draw call per layer; zero idle CPU when SMASH closed.

### Procedural sprites (js/smash-sprites.js)

No image files. Sprites are painted at load into an offscreen canvas from small recipes, same approach as js/ship-icons.js: a palette per fighter,
rectangles/pixels per body part (head, torso, arm, leg, prop), hand-set offsets per frame. ~13 fighters x ~64 frames = ~830 frames x 32x48 = one 2048x2048 atlas total.
- Recipe = `{pal:{skin,hair,main,accent,dark,glow}, parts:{head,torso,armF,armB,legF,legB,prop}, poses:{name:[frame,...]}}`.
- Pose frame = per-part `{dx,dy,rot90?,flip?}`. Frame generator draws parts in order with 1-px black-violet outline pass (flood outline of silhouette).
- Re-skin: our human rig look (createHuman palettes), pets (createPet shapes), bosses (wraith/monolith/maw plans) simplified into pixel parts.
- Animation set per fighter (all frame counts at 60 Hz with hold ticks): idle 6, run 8, jump 4, fall 4, attack A/B/C/D 6-10 each (the 4 moves), hit 4, launch 6 (tumble spin), shield 2, land 3, plus 1-2 fighter specials. Budget 56-70 frames.
- Everything also has a flat-color capsule/box debug view (`?smashdebug`): hurtboxes green, hitboxes red.

### Backgrounds (js/smash-stages.js, layered pixel scenes drawn at load)

Each scene is 4-6 canvases generated procedurally from the galaxy theme (violet gradient sky, star noise, planet discs with ring, station silhouettes, 7/11 sign glow).
3 stages (all: solid main platform + 2 thin pass-through platforms, blast box, 4 spawn points):
1. **PLATEAU** — planet plateau, huge ringed planet in the sky layer, slow moon crossing. 360 px main, 2 floating rock platforms at y-56.
2. **HANGAR** — station hangar deck, rail lights, one pad platform moves up/down (8 s), docking beam hazard every 20 s (60-frame telegraph, 4 dmg, small kb). 400 px.
3. **ROOF** — 7/11 roof, lit sign as a solid mid platform, AC-unit side platform, vent updraft every 15 s. 300 px, small, night, station lights parallax.

## Physics and ruleset (shared by all fighters)

Per-fighter numbers in the table below; defaults here. Gravity/fall in px/frame^2, px/frame.
- **Weight** (baseline 100): scales knockback received: `kb_taken = kb * (100/weight) ^ 0.8`. Heavier = survives longer.
- **Knockback:** `kb = base + growth * (pct/100)`; launch speed `0.03*kb` px/frame^... tuned so 100% on a mid move at 0.9 weight ~ 3 stage-widths per second. Angle per move (deg, 0 = forward, mirrored by facing; 361 = sakurai-style "low angle on ground, 44 in air", call it AUTO).
- **Hitstun** = floor(kb*0.4) frames; DI bends launch up to 15 deg. **Hitlag** = floor(dmg/3)+3 frames both sides, shake + SFX duck.
- **Stocks 3**, blast box = stage +-(150 px side, 120 px top, 100 px bottom). K.O. = leave box. Respawn 90 frames on a hover pad, 120 inv. Time 4:00; timeout = stocks then lowest pct.
- **Shield** 180 frames of health, break = 90 stun. Roll 24 frames, 12 inv. Drop through thin platforms. One double jump unless fighter says (Jelly 5, Hopper flutter, Hum 5).
- **Ledge:** v1 grab ledge snap; hold 1.5 s then forced drop. Recovery moves in table ("K^").
- **Final move:** 1 per stock, meter from dealt+taken dmg; L+K. Generic = fighter-themed boss-limb cameo (wraith swipe sprite 128 px wide, 18 dmg, base 90 growth 4). Per-fighter flavor in table; same hit logic.
- **Items (from `generateItem`, ship-items.js, kept):** every 12-20 s, max 2. food = heal 10-25; drink = speed x1.3 for 8 s; snack = dmg x1.25 for 8 s; fries = 1-hit armor, tier sets duration; `pulse` = shockwave on use, `mirror` = reflect next projectile, `tunnel` = 40 px teleport, `dreamy` = slow others 3 s. K picks/uses, throw = 6 dmg projectile. Despawn 12 s. No gorCoin in v1.

## Roster v1: 12 fighters + 1 (13)

Numbers: Wt weight, Jmp jump apex px (full hop, double jump apex ~85%), Fall max fall speed px/f (fast-fall 1.6x), Run dash speed px/f, Grav px/f^2, Air air-drift px/f.
Move cells: `dmg / base-kb / growth @ angle  startup-active-end`. J = neutral jab/tilt, J> = forward strong, K = special, K^ = up special / recovery. Move names ours.
Names are original; each is an archetype re-skin of our humans/pets/bosses.

| # | Name (re-skin) | Archetype | Wt | Jmp | Fall | Run | Grav | Air |
|---|---|---|---|---|---|---|---|---|
| 1 | **PILOT** (human, jetsuit) | all-rounder | 100 | 60 | 1.7 | 1.5 | .095 | .10 |
| 2 | **TWIN** (human, taller, green scarf) | floaty, slippery, misfire | 95 | 72 | 1.45 | 1.3 | .080 | .12 |
| 3 | **HAULER** (human mech-miner, huge) | heavy, grabber | 135 | 48 | 1.9 | 1.1 | .115 | .07 |
| 4 | **RANGER** (human w/ hood, bomb pouch) | projectile zoner | 104 | 58 | 1.8 | 1.3 | .100 | .09 |
| 5 | **BOUNTY** (human, armor, arm cannon) | armored zoner | 110 | 54 | 1.6 | 1.0 | .090 | .09 |
| 6 | **HOPPER** (pet walker, lizard) | egg fighter, floaty jump | 98 | 55 + flutter | 1.5 | 1.3 | .090 | .11 |
| 7 | **JELLY** (jelly pet) | light, multi-jump, copy | 80 | 58 x5 | 1.2 | 1.1 | .070 | .13 |
| 8 | **SWIFT** (pet, sleek, reflector) | fast fall, fast attacks | 90 | 62 | 2.3 | 1.7 | .120 | .10 |
| 9 | **SPARKLET** (pet, yellow, tiny) | tiny, fast, electric | 76 | 58 | 1.7 | 1.6 | .095 | .11 |
| 10 | **HUM** (pet, pink puff, singer) | lightest, 5 jumps | 62 | 58 x5 | 1.1 | 1.0 | .065 | .14 |
| 11 | **SEER** (human child, backpack) | psy projectiles, odd recovery | 94 | 58 | 1.65 | 1.2 | .092 | .10 |
| 12 | **RACER** (human, helmet, fast) | fastest, heavy hits | 104 | 58 | 2.1 | 2.0 | .105 | .09 |
| 13 | **BUILDER** (human, cube-blocky; generic crafter) | place blocks, mine, TNT | 100 | 58 | 1.75 | 1.35 | .098 | .09 |

BUILDER is a blocky crafter; no Mojang names, no creeper/pickaxe-logo art. Sprite is a cube-head, cube-limb humanoid in OUR palette.

### Moves (4 each, + signature mechanic)

Cell format: `dmg/bkb/gr @ang start-active-end`. Projectiles say P (speed px/f, life frames). Ang = degrees from forward (90 = up).

1. **PILOT** — fireball, cape-flip, coin-punch archetype.
   - J: 3-hit jab 3/5/10 @AUTO 3-2-12; hit3 5/12/60 @45
   - J>: shoulder-check 10/20/80 @40 8-3-22
   - K: **plasma ball** P 5/0/0 @-5, speed 3.2, arcs and bounces, life 120, cooldown 20; 2 live max
   - K^: **boost-punch** 9/25/70 @80 5-2 then rise 62 px (the "up-B"), first frame 14/45/90 @80 (the sweet-spot is the first 3 frames); one use per air. Sig: **flip-turn**: the K-down spin reflects projectiles (reflect 6 frames).
2. **TWIN** — slow, floaty, "misfire" (1 in 8 K^ fires a 45-frame super-launch 23/60/110 @90, otherwise a weak pop).
   - J: 3/5/10 @AUTO 4-2-14; J>: dash-hit 12/22/85 @40 11-3-30
   - K: **plasma ball** weaker 4/0/0 P speed 2.4, drops straight down (low arc); K^: **rocket hop** 7/20/60 @90 rises 58 px; **MISFIRE** 1/8 chance: 23/60/110 @90 with a 45-frame startup; Sig: floaty jump (air time +30%), 3-frame landing.
3. **HAULER** — giant punch, cargo throw archetype.
   - J: **jab** 4/6/12 @AUTO 4-2-14; J>: **haymaker** 15/35/100 @38 20-4-38 (hyperarmor frames 10-20)
   - K: **GIANT PUNCH** charge 0-120 f; released: dmg 8 -> 28, bkb 20 -> 60, gr 80 -> 130 @38; charged punch gets armor frames 1-30
   - K^: **spin-up**: 8 hits 1.5 each, rises 36 px, final hit 6/30/80 @80; Sig: **cargo throw** (grab + hold): the held victim can be carried while walking (0.6x speed); fwd throw 11/30/90 @38; back 12/28/95 @145.
4. **RANGER** — bombs, boomerang, hookshot (grapple).
   - J: **blade** 3-hit combo 3/5/10, 3/5/12, 5/16/40 @45, 4-2-14; J>: **overhead slash** 11/22/85 @45 15-3-34
   - K: **bomb** (neutral, thrown arc speed 2.2), explodes 12 f after landing, 8/22/80 @AUTO; K ^ (up+K): **spin blade** 3/6/60 multi-hit + 9/25/90 final @90 rises 46 px
   - K>: **boomerang** P 5/8/30 @35, goes out 90 px, returns; pulls back on return hit; Sig: **grapple** (down+K) 2/0/0, tethers to ledge/platform, 120 px reach, lets you cling to the ledge from a distance.
5. **BOUNTY** — charge shot, bombs.
   - J: **arm swing** 4/6/12 @AUTO 5-2-16; J>: **cannon bash** 12/22/90 @38 16-3-34
   - K: **charge shot** charge 0-90 f (hold), P speed 5, 4/0/0 uncharged -> 25/45/110 @ 45 charged; keep charge while shielding
   - K^: **screw jump** 3 hits 1.5 + final 5/30/80 @80, rises 70 px; Sig: **morph bombs** (down+K) roll into a ball (low hitbox), drop bombs 7/15/70 @80 with a short blast, can bomb-jump for +30 px recovery.
6. **HOPPER** — egg lay, flutter jump, egg throw.
   - J: **tongue snap** 3/5/10 @AUTO 5-3-14; J>: **headbutt** 12/22/85 @40 10-3-26 (armor frames 6-10)
   - K: **egg lay**: grabs victim in the tongue and traps them in an egg (45-90 frames based on pct). Freeze damage 3/14/60 on break. Cooldown 40
   - K^: **egg throw** P arcs speed 2.8, bounces, 12/25/80 @AUTO; aim with up/down; K> **ground-pound** 12/30/90 @270 (stalls then drops)
   - Sig: **flutter jump**: second jump = 45 frames of flutter (+ vy), armored head (frames 1-12), K^ in air = egg throw rise 40 px. **Shield = egg shield (never breaks, 1 egg per stock, 2 s).**
7. **JELLY** — multi-jump, inhale copy.
   - J: **wobble** 3/5/10 @AUTO 3-2-12; J>: **hammer drop** 16/40/110 @45 32-3-44 (slow, huge)
   - K: **INHALE**: swallow, grabs victim frames 8-18. Absorbed -> `copy` = gains the victim's neutral K. Spit 7/20/70 @AUTO. Hold a copy up to 20 s
   - K^: **ascend** 5/8/20 @90, rises 50 px, glides; Sig: **5 jumps**, 0.65x fall speed while gliding, in-air jump 36 px each. Shield = **stone**: 3 f startup, invuln 0.6 s, can't move, lose 10% pct per use cooldown 180.
8. **SWIFT** — reflector, illusion.
   - J: **fast jab** 2/3/10 @AUTO 2-1-8, rapid; J>: **dash kick** 11/24/90 @40 6-3-16
   - K: **blaster** P 3/0/0 speed 6, no knockback flinch; K> **reflector** 6-frame invulnerable shield, reflects projectiles 1.5x damage, 1/20 knock 8/20/80
   - K^: **illusion dash** 8 hit (dash 130 px along stick direction), 5/16/60 @AUTO; **fire-bird** (K^): 20 f charge then 10/30/70 dash, burns 3 dmg dot; Sig: **fast fall** 2.3, short hop 42 px, smaller hurtbox on crouch.
9. **SPARKLET** — quick attack, thunder.
   - J: **zap-jab** 3/4/10 @AUTO 3-2-10; J>: **tail whip** 9/18/80 @45 8-3-24
   - K: **sparks** P 4/0/0 speed 4, slow, pass-through ground, 90 f; K^: **QUICK DASH** 2 dashes (130 px + 130 px), tap direction after the first, 3/0/0 on contact, final dash 2/0/0; no hit stun, super-recovery. 
   - K> (down+K): **THUNDER**: cloud summoned above (P-ish 80 px up, delay 40 f), bolt 16/50/100 @270 (down), hits self if under the cloud for 8/15/60 (the dumb part). Sig: **tiny hurtbox** (24x30), air speed 0.11.
10. **HUM** — rest, sing.
   - J: **pound** 3/5/10 @AUTO 4-2-14; J>: **double-slap** 9/18/80 @45 8-3-24
   - K: **SING**: 40-frame wave, sleeps all in 80 px (sleep 80-180 f by pct; the sleeper takes +30% dmg), 50 f endlag
   - K^: **REST**: falls asleep on hit frames 1-3, hitbox 24/75/165 @80 (huge launch), asleep 160 frames afterward; if it misses you lose a stock soon. Sig: **5 jumps**, 0.5x gravity on the 5th, nearly the lightest; roll = `puff-bounce` +20 px.
11. **SEER** — psy-fire/thunder, bat.
   - J: **bat swing** 3-hit 2/4/10, 3/5/12, 7/18/55 @AUTO, 5-2-18; J>: **bat swing (fwd)** 14/30/100 @40 13-4-32
   - K: **psy-spark** P 5/5/20, zig-zag wobble, 120 f, homes mildly; K^: **psy-bolt** thrown upward, moves under stick control, hits self for 8/20/80 on return (the recovery; if it hits the player they are launched at it: 7/30/80 @90).
   - Sig: **absorb** (K> down+K): 12 f window, absorbs projectiles/energy and heals 1.2x dmg%; reflects nothing.
12. **RACER** — knee strike, flame-fist archetype, dash.
   - J: **fast jab** 3/5/10 @AUTO 3-2-10; J>: **KNEE STRIKE** 15/50/130 @40 (sweetspot first 3 f only, 24/60/140) 8-3-26
   - K: **FLAME FIST** startup 62 f, 25/60/130 @40; huge launch, moves 36 px forward
   - K^: **talon-climb** 2 hits, 5/18/60 @80 then 3/10/20; **K> (dash+K)**: **speed-lunge** 9/25/80 @45 3 hits, 130 px dash; Sig: **dash speed 2.0**, 15-frame turn dash, run-stop endlag 14.
13. **BUILDER** — block placement, mining, TNT, minecart, anvil.
   - J: **pick swing** 4/8/20 @AUTO 6-2-16; J>: **sword-hack** 10/20/85 @40 10-3-24
   - K: **place block** (neutral K): a 32x32 block appears in front, solid platform, 3 s lifespan, max 3; K> (forward) **mine**: shoves the block, 5/10/30
   - K^: **minecart** (up+K): 60-frame ride, forward hits 8/25/80 @45; rises by launching off of a ramp 46 px; K> down: **TNT** (placed block): fuse 90, 18/50/110 @80, hits builder too at 75%; **anvil** (hold down+K + jump): drops 16/40/130 @270 spikes (meteor), 60 f startup.
   - Sig: **resource**: collects blocks by mining (max 12); anvils cost 3 blocks, TNT 2, cart 4. A crafting table is the home base: standing on it (K held) crafts.

### Final moves (1/stock, 100% meter)
PILOT: ion burst beam 14 dmg x 5; TWIN: dual-star barrage; HAULER: jackhammer slam 30 dmg; RANGER: triple-edge flurry; BOUNTY: lance beam sweep; HOPPER: giant egg roll; JELLY: stomach swallow all; SWIFT: jet dash run (jet dash across stage); SPARKLET: lightning storm; HUM: lullaby sleep all; SEER: psy-wave; RACER: blue-flame punch; BUILDER: bedrock slab drop. All use the same hit logic: 1 scripted hitbox list in the move table.

## Controller (same as v1)
Keyboard P1 WASD + J/K/L; P2 arrows + `,` `.` `/`; gamepad API, standard mapping, deadzone 0.25, up to 4 pads. Input frame = u16 mask + 2 analog bits. 6-frame buffer for J/K/jump. Tap-jump, fast fall, tilt strength via 2 analog bits. Phones unsupported.

## Hit / hurt boxes
Pure 2D: hurtboxes = 3 AABB/circle per fighter (head, body, legs) from the current frame's rect list; hitboxes = per-move circles/capsule segments placed relative to the sprite pivot with per-frame offsets. Segment-vs-circle test; a hit lands on a victim once per swing. Data lives in `js/smash-moves.js` (tables only). Overlay `?smashdebug`.

## Net: host-authoritative (kept from v1)
Same as v1; only snapshot content changes (2D: x,y,vx,vy int16 /16, pct u16, state u8, anim u8, af u8, face 1 bit, inv/armor flags u8). ~14 B/fighter, ~70 B per snapshot at 4 fighters. Guests send inputs 60 Hz (2-3 B), host snapshots at 30 Hz, events (hit, ko, item, final) piggyback, guest predicts own movement only, smooth-correct < 3 px, snap above. Rooms `sm_open/sm_join/sm_lobby/sm_in/sm_snap/sm_end/sm_leave` as in v1 (relay fans out only, caps 4, drop idle 60 s; raise per-conn rate bucket to 60 in / 30 out). Offline = hot-seat + bots, net layer optional (no import in smash-core). Rollback and lockstep rejected (determinism of trig/ordering across browsers, stalls over Funnel RTT 40-120 ms).

## UI (DOM over canvas, style/smash.css)
Bottom cards: swatch, name, big pct (white -> yellow -> red past 100, shake on hit), 3 stock pips, final meter. Top timer. 3-2-1-GOR countdown, K.O. flash (2-frame white + victim-color text 40 f), off-screen arrow with pct, pause (Start/P), results podium (K.O.s, dmg, items, time) with REMATCH/CABINET. Lobby: pick 1-13, stage, stocks/time, each slot human/CPU/off, room code. Up to 4 slots.

## CPU bot (js/smash-bot.js, in pass 1)
Priority list: recover off-stage, shield incoming hitbox, attack when in range with a random move, approach; 3 levels (reaction 12/8/4 f, mistake rate). Per-archetype hint table (zoner keeps distance, grappler closes).

## Audio (procedural, ship-audio.js, additive smash bank)
hit_light/heavy by kb, ko (sweep + sub), shield/break, jump/land, item spawn/pick, final charge/fire, countdown, victory sting, per-fighter voice blips (generated chirps by archetype). Stage ambience per stage; 3 short seeded music loops, +8% tempo at 1:00 left. Hitlag ducks music 6 dB.

## File ownership + build order

| File | Owner | Role |
|---|---|---|
| js/smash-core.js | sim | fixed-step sim, physics, knockback, stocks, items, no DOM |
| js/smash-moves.js | sim | all 13 move/stat tables (data only) |
| js/smash-sprites.js | sprite | procedural 32x48 sprite recipes + atlas painter |
| js/smash-render.js | render | WebGL2/Canvas2D batcher, camera, parallax, debug view |
| js/smash-stages.js | stage | stage geometry + parallax scenes + hazards |
| js/smash-input.js | input | keyboard + gamepad mask |
| js/smash-ui.js + style/smash.css | ui | HUD, lobby, results |
| js/smash-net.js | net | rooms, snapshots, prediction |
| js/smash-bot.js | sim | CPU |
| js/smash.js | integrator | `openSmash()/closeSmash()`, glue |
| js/ship.js, ship-station.js, ship-audio.js | integrator/audio | cabinet + `/smash` + pause ship (<20 lines); cabinet mesh (<15); smash bank at EOF |
| server/nmg-relay/server.js | relay | `sm_*` rooms, pass 3 only |

Passes (each playable and deployable):
1. **2D engine + 4 fighters (PILOT, HAULER, SWIFT, JELLY) + 1 stage (PLATEAU) + hot-seat/bots.** Core sim, sprites, renderer, input, bot, minimal HUD, cabinet + `/smash`, debug overlay. Done when 2 keyboards + 2 bots fight and a K.O. works.
2. **Remaining 9 fighters + 2 stages (HANGAR, ROOF) + items + finals + gamepad + lobby.**
3. **Netcode.** smash-net, relay rooms, prediction, forfeit. Test 2 tabs, then over the Funnel with 100 ms throttle.
4. **Polish.** Audio bank + music, results podium, hitlag/camera tuning, balance with bots, per-fighter finals, gorCoin payout off.

## Risks
- **Scope:** 13 fighters x 4 moves x ~64 sprite frames is the real cost, not the sim. If pass 2 runs long: ship 10 fighters, keep BUILDER and HUM last. Never cut net or bots.
- **Procedural pixel art** can read as mush. Do 2 fighters first in pass 1, show Emory, lock a style (outline, palette limit 8 + 4 shades) before generating the rest.
- **Feel numbers are guesses.** Weight/jump/fall/dash and every move cell above are starting points, tuned by playing. Table design makes tuning cheap.
- **Movesets are close to the classic cast.** That's a mechanics choice; keep names, sprites, voices, SFX, stage art and text original. Run a final grep for banned proper nouns before ship (Nintendo, Mario, Zelda, Pokemon, Minecraft, Creeper, Steve, Smash, etc.).
- **Net over Funnel** unknown RTT; warn above 120 ms. Relay rate bucket must be raised; load test before opening up.
- **Perf:** one atlas, 4 fighters, 6 parallax layers: trivial on M4; keep draw calls < 40.
- **Keyboard ghosting** on 3-key combos; gamepad fixes.
