# NMS mechanics vs NO MANS GOR (2026-10-01)

Sources: NMS wiki (fandom/miraheze) ship + multi-tool class tables, plus working knowledge of
the game and Hello Games GDC/patch history. Unsourced details (exact speed caps, fuel numbers)
are from memory; treat numbers as approximate. Verdicts reference ship-mode.md revs 12-16.

## 1. Landing
NMS: fly low over ground, a "land" prompt appears under ~150 u altitude and low speed. Hold it
and the ship auto-levels, descends, legs out, dust. Landing pads and flat ground are both
fine; rough terrain just tilts the ship a bit. Takeoff is a held key, costs launch thruster
fuel (Launch Thruster recharges with Di-hydrogen/Tritium plus Pulse Engine uses its own fuel).
Landing pads at outposts/stations make the auto-land snap to a marked spot.
- Verdict: STEAL (rev 14 E LAND is right). IMPROVE: snap-to-pad assist, pad glow visible from
  orbit, and tilt the landed hull to the slope normal via floorAt().n. No fuel; make takeoff
  free. Cosmetic cost only: a 1 s "launch thrusters" bar if you want ceremony.

## 2. Atmospheric entry
NMS: cinematic. Past the atmosphere line the camera shakes, the hull glows orange, fire streaks
pass the canopy, a loud rumble; speed is dampened automatically so you cannot ram the planet
at pulse speed (pulse drops on approach). Camera pulls slightly back, FOV widens, fades to sky.
Heat/burn is a visual only, no damage (except old survival-mode hull hits).
- Verdict: STEAL, rev 14 plasma sheath is on target. IMPROVE: tie sheath intensity to speed
  over a soft limit (e.g. above 1.2x boost), add FOV kick + screen-edge heat shimmer, auto
  speed-damp to boost on entry rather than a hard stop. Add a sonic-boom thump on the
  transition.

## 3. Exiting the ship and on-foot
NMS: get out beside the ship, first-person by default (third-person toggle exists). Walk,
sprint (Shift), jump, jetpack (hold Space, drains and recharges, also boosts mid-air),
melee-boost (tap melee while jetpacking to dash, with a satisfying whoomp), scanner/analysis
visor (hold a key, outlines resources, fauna, buildings, shows names and distances),
multi-tool fires/mines. Hazard bar (cold/toxic) drains in bad biomes.
- Verdict: STEAL jetpack + visor, SKIP hazard bars. Rev 14 has run/jump; add jetpack (hold
  Space, 2 s fuel, recharges on ground) because it makes tiny planets fun and costs ~nothing.
  IMPROVE: visor = hold Q, tints the world, shows teammates, outposts, discoveries as labelled
  pings. Camera: third-person default is fine for seeing others' avatars.

## 4. Trading posts / stations / outposts
NMS: space stations are an orbiting cube hub: fly in through a slot, land in the hangar,
walk around. Contains traders (sell/buy), a Galactic Terminal, a ship dealer, a Quicksilver
vendor, mission agents, an Anomaly-style crowd of aliens. Planet outposts: landing pad, a few
buildings, 1 NPC with a terminal, an archive/beacon. NPCs stand idle, turn toward you, give
a short line, E opens a menu. Aliens speak in alien language; you learn words by finding
monoliths, plaques, and by taking missions; words get swapped into English over time (the
language gimmick). Dialog is a text box, a typewritten line plus 2-3 choice buttons.
Shop UI: grid of items, price in units, left detail panel with stat bars, class badge.
- Verdict: STEAL outposts + pad + 1 NPC (rev 15). IMPROVE: language gimmick as a SHARED thing:
  each NPC speaks glyph gibberish seeded per planet; every word you "learn" (by trading, or
  by sitting through the line) is saved on the profile and shown in English thereafter; a
  global dictionary relay means friends can teach each other words. Keep shop to a 4-card row
  with class-colored border and one stat-bar compare against equipped.

## 5. Multi-tool / weapon classes
NMS: Multi-tools (and ships) have class C/B/A/S: a hidden-range roll of damage, mining speed,
scan range, slots. Wiki ranges: experimental type C +0-5% dmg ... S +15-25% dmg; alien type
C +10-15 ... S +25-35. Class sets the range of the roll inside a type; classes overlap, so a
good A can beat a bad S. Visible stats: Damage, Mining Speed, Scan Range, Slots. Upgrades:
modules in slots (supercharged slots give adjacency bonuses), purchasable or found on
crashed things. Rare S-class show as bright colored chevrons.
- Verdict: STEAL class letters and color coding, SKIP the slot/adjacency grid. For
  js/ship-weapons.js: class = {C:0.85-1.0, B:1.0-1.15, A:1.15-1.35, S:1.35-1.6} multiplier on
  dmg/rate, rolled per weapon seed, ONE special on A and S only, two on S. Name shows class
  letter. Cheap, instantly readable loot-slot-machine hit. Everyone sees your weapon's glow.

## 6. Ship classes and stats
NMS: nine archetypes (Shuttle, Fighter, Hauler, Explorer, Exotic, Interceptor, Solar,
Corvette, Living). Fighter = damage + agility, Hauler = biggest inventory + shield, Explorer =
hyperdrive range, Shuttle = balanced cheap, Exotic = weird shapes, Solar = sail, Corvette =
buildable big ship, Living = organic, grown. Class letter C-B-A-S sets bonus magnitude; wiki:
fighter unit value 0/20/70/100 % over C/B/A/S, explorer 0/10/25/50, hauler 0/30/60/80.
Stats: Damage, Shields, Hyperdrive range, Maneuverability, Slots.
- Verdict: STEAL 4 archetypes (Fighter/Hauler/Explorer/Shuttle) x class letter, SKIP the
  rest. Hook to rev 10 ships: Fighter: +dmg, +turn; Hauler: +HP, +cargo, slower; Explorer:
  +pulse speed cap and +discovery radius; Shuttle: baseline, cheap. Class letter multiplies
  one bonus. Hull builder (buildHull) already supports seeded variants: map archetype to
  silhouette so you can tell a hauler from a fighter at 1 km.

## 7. Discovery and naming
NMS: scanner shows an unnamed creature/plant/planet/system; on first discovery, you are
credited and may rename it. Discoveries upload to a shared galaxy ("Discovered by"). Pays
units + nanites; the discoveries page lists them with a completion percentage (every species
on a planet). In multiplayer, other players see your name on the thing.
- Verdict: STEAL (already "Later" in rev 16). It is the best fit for a chill multiplayer
  site: a first-visit planet claim, rename once, relay-wide registry, chat announcement,
  name hovering over the planet in the galaxy view. Cost M. Anti-grief: 20-char limit,
  profanity filter, one rename per day.

## 8. Units / nanites economy
NMS: Units = main currency, earned from selling cargo, scanning, killing, missions. Nanites
= rare mid currency from scrap, dissection, daily missions, and used for tech upgrades.
Quicksilver = premium, bought in the store, from weekly/community missions. Bulk gameplay
loop is: gather -> sell -> upgrade.
- Verdict: STEAL units only. SKIP nanites/quicksilver (confusing in a casual game). Units
  from kills, discoveries, and delivery runs. One currency, sink = weapons/hull skins/ships.
  One inventory number, one shop. Keep saved on the profile (rev 10).

## 9. Exocraft
NMS: planet vehicles summoned from a pad/beacon: Roamer (4 wheels), Nomad (fast), Colossus
(cargo), Pilgrim (hover), Minotaur (mech), Nautilon (submarine). Drive across terrain with
a camera that follows; boost and a scanner sweep; unlock via tech blueprints.
- Verdict: IMPROVE, cheap form: ONE buggy, summoned with R from the landed ship; seeded
  color; reuses ship-human walker physics on a floorAt() normal; boost on Shift. Covers the
  "too big to walk across" problem the 5000 L-radius planets will create. Cost M.

## 10. Wonders and beauty systems
NMS: planets are generated from a seed with a biome type (lush, barren, scorched, frozen,
toxic, radioactive, exotic, dead, swamp, lava, ocean); each sets palette, sky gradient,
sun/horizon, weather (storms, rain, snow, toxic clouds, extreme), flora density, fauna set,
and hazard. Terrain: voxel marching, caves, floating islands, huge mesas, waterfalls. Fauna:
body plans from parts (head/torso/legs) with seeded colors and gaits; flora: instanced
plant/tree templates with colour ramps. Day/night cycle with stars, sunset, twin moons;
bioluminescence at night. Weather is a visible, audible event (a storm rolls in, fog bank,
meteors). Rings, moons, and sky objects visible from ground. Sound: layered ambience per
biome and a procedural pad.
- Verdict: STEAL biome table + weather, SKIP voxel caves. Rev 16 covers sky/day-night/
  flora. Add: weather states per biome (rain streak particles, snow, sandstorm fog, meteor
  streaks), fauna that react to you (flee/gather), and a procedural ambient pad per biome
  (Strudel/ChucK in-browser, credit them, fits Emory's stack).

---

## Eight features worth adding (ranked)

Cost S = under a day, M = 1-3 days, L = week+.

1. **Discovery registry + naming** (M). Claim and rename a planet/creature on first visit;
   relay stores it; chat announces; shows in galaxy view. Best social hook for a chill game.
2. **Class letters on weapons and ships (C/B/A/S)** (S). Cheap loot dopamine; visible in
   other players' glows; drives shop browsing. Do with rev 15.
3. **Jetpack + scanner visor** (S). Makes on-foot the fun mode; visor doubles as the way to
   find discoveries, outposts, friends.
4. **Pad-to-pad "taxi"**: Beacon fast-travel for friends (S). Spawn on a friend's landed
   position; makes meetups instant, no walking 5000 L. Pair with a /tp request in chat.
5. **Shared alien language** (M). Per-planet glyph words, learned on interaction, global
   dictionary across players. Pure chill co-op ritual; cheap text-only content.
6. **Buggy exocraft** (M). Required once planets are enormous; pairs with passengers so two
   friends can ride (2 seats, driver steers).
7. **Weather and biome events** (M). Storms, meteor showers, aurora as scheduled shared
   events announced in chat ("Aurora over PAPERS in 2 min"); gather players in one spot.
8. **Base-light: a placeable beacon/pad + one decor item** (L). Plant a named beacon on a
   planet; others see it from orbit; cosmetic only. Gives persistence and a reason to return.
   (L only if decor catalog grows; S if just a beacon + colour.)
