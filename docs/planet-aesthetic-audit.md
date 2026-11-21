# NO MANS GOR: planet aesthetic audit (2026-11-20)

Owner's verdict: "the planet vibe is too Roblox basic; I need way more minimalist and flowy."
Scope: audit and plan only. No code changed. Branch `gorcave-backlog` @ c08f10e.
Screenshots (Mac scratchpad, not committed; dir `/private/tmp/claude-501/-Users-emgor-Documents--EMGOR-SYNTH-----EMGOR-ONLINE-EMGOR/a852a1e8-dac7-44bc-90c2-8a7439b0ecbd/scratchpad/`): `aes-01-landed-day-metagor`, `aes-02-village`, `aes-03-orbit`, `aes-04-descent-dusk-glow`, `aes-05-night-glowtrees`, `aes-06-slab-rocks-jellies`, `aes-07-forest-night`, `aes-08-shore-water`, `aes-09-water-2`, `aes-10-711-exterior` (.jpg).

## 1. What is on screen

| shot | what reads |
|---|---|
| orbit (metagor) | lumpy lime ball, high-frequency bumps, pale sea blob; reads as a noise texture on a sphere |
| landed, metagor | flat lime ground, bright lime faceted cones/blobs; sky is the black DOM starfield showing through. The ship's thrusters bloom to white |
| village | beige walls, flat lavender roofs, brown boxes; unlit (MeshBasic), no fog, no shading. A purple box golem. Reads as a stock Roblox place |
| night, gorcave | pale peach faceted trees bloom to white blobs; black sky; green-brown ground with no value structure |
| rocks / jellies | rocks are 5x3 UV-sphere clusters that read as floating slabs; the jellies are the best thing on screen (soft glow, slow pulse) |
| shore | teal water with wave normals and foam; it works. Sand is flat beige, sky is black-green |
| 7/11 | a grey-white slab with a purple edge, sitting in brown haze |

## 2. Diagnosis: why it reads "Roblox"

The game has no sky, so every frame is lit objects floating in space. Every surface is faceted, scattered uniformly, and drawn in mid-saturation random hues at the same value. Nothing in the frame is darkest or lightest on purpose, and nothing moves slowly.

Offenders, ranked by visual weight:

1. **No sky by day.** The dome alpha is capped at 0.45 (`ship-planet.js:2801` `uAlpha = depth^0.8 * 0.45`; `:763` `seeThru = mix(0.4, 0.84, uSun)`), and the canvas is transparent over the DOM starfield (`galaxy3d.js:2671` `alpha: true`, `:2674` clear alpha 0). On the ground at noon you still see black space and stars. This single choice removes atmosphere, horizon, and mood from every frame.
2. **Fog is dark and thin.** Fog colour is `(atmo+hi)/2 * fb * 0.6 + 0.02` (`ship-planet.js:2792`), so the horizon fades to a muddy dark tone, never to a luminous sky. Density `0.78/(400L)*0.42` (`:2796`) gives f(400 L) of about 0.10: no atmospheric perspective, so no depth layering.
3. **Palette system has no anchor.** There are 10 unrelated families (`galaxy3d.js:74-85`: coral, gold, mint, rust, lavender, crimson, absinthe, orchard, dune, rose), each planet rolls one independently (`:167-172`, "max variety, no family relationship"). Only `lav-heath` touches violet. Every planet uses lo/hi at similar value (L 0.45-0.75), which gives a mid-value, mid-saturation soup. Flora colour is a lerp lo→hi plus 12 % accent-coloured trees (`ship-planet.js:2224-2225`): confetti.
4. **Lighting is one half-Lambert with no sky term.** In the terrain it is `mix(0.14, 0.98, smoothstep(0.2, 0.88, ndl))` (`:343-345`), and props use `mix(0.3, 1.0, ...)` (`:381-383`, `:554-555`). No hemisphere colour and no ambient tint from the sky, so shadows are grey-black instead of violet. Village, store, and human rigs use a **hard-coded** light `(-0.62, 0.52, 0.4)` with `0.7*ndl + 0.3` and no fog (`ship-world.js:75-87`, `ship-human.js:30-43`). The village is **completely unlit**: `MeshBasicMaterial` (`ship-village.js:328`), tinted by `setNight` (`:339-341`).
5. **Faceted geometry everywhere.** `GeoBuilder` emits non-indexed flat-normal triangles (`ship-planet.js:1051-1106`). Trees are 5-7-sided cones and 7x4 spheres (`:1411-1424`), rocks are 5x3 sphere clusters (`:1491-1494`), and world/human shaders take normals from `dFdx/dFdy` (forced facets). This is the core of the Roblox/low-poly-asset-pack look.
6. **Prop silhouettes are generic toys.** The conifer, lollipop round tree, and mushroom/palm sit at similar heights (`sh = (4 + r2*4)*1.5 L`, `:2206`). Three variants are mixed per cell at random (`:2206` `Math.floor(rr*3)`), so there are no families, groves, or clearings. Lush caps at 700 instances (`:2198`), placed nearest-first in a lattice, which gives uniform density.
7. **High-frequency noise on the ground.** The terrain adds `fbm(p*6.3)` colour noise (`:260`), `tn = fbm(sp*uFreq*24)` multiplicative (`:287`, `:291-297`), and 2-octave `dn` detail at ±27 % (`:310-312`). The slope darkening at `:356` turns every slope grey-purple. The result is speckle instead of gradient bands.
8. **Bloom eats the pale palettes.** UnrealBloom (0.48, 0.45, **threshold 0.82**) (`galaxy3d.js:2822`) and the flora top glow `col += vCol*uGlow*...*1.8` (`ship-planet.js:394`) at night turn every pale tree into a white blob (aes-05, aes-07). The thrusters also white out the left third of the landed frame (aes-01).
9. **Motion is fast and twitchy.** Sway runs at `uTime*1.7` (`:420`), water normals at `uTime*1.3-2.4` (`:334-336`), and creature gaits at 7 Hz (`:433`). Nothing moves at lava-lamp speed: no clouds, no cloud shadows, no slow sky drift.
10. **Structures are their own art style.** The 7/11 is white `0xF4F2EE` walls with neon trims (`ship-world.js:149`, `:142`). Village roles are saturated primaries (`ship-village.js:43` `0x8FD06A, 0xFFD36A, 0xB08CFF, 0xFF8AE0`). The player suit is orange. None of these share the planet's palette.
11. **HUD and world disagree.** The HUD is disciplined violet + cyan (`style/ship.css:6-13`). The world under it is lime, coral, and peach, so the HUD is the only violet thing on screen.
12. **Tonemapping is half-applied.** The renderer is ACES at exposure 1.15 (`galaxy3d.js:2678-2679`), applied once by OutputPass, while every planet material declares `toneMapped:false` (`ship-planet.js:1398, 1433, 1479, 1502`) and hand-clamps its own ranges. ACES desaturates and shifts hue on the pale palettes; there is no single exposure owner.

## 3. Target look

A planet is one colour field, not a collection of objects. The sky is a tall, opaque, slow-breathing gradient (violet zenith → warm haze horizon) that every distant thing dissolves into. The ground is broad smooth value bands by height and slope with no visible noise, and the light comes from one low warm key plus a cool violet sky fill, so shadows are violet, not grey. Props are few and repeated: tall thin vertical strokes, smooth rounded stones, and soft blob canopies in three sizes, gathered in groves with empty space between. Everything moves at 0.05-0.3 Hz (sway, cloud shadows, aurora, sky bands), like a lava lamp. Value hierarchy is fixed: sky lightest, far land mid, near land darker, one accent hue per planet reserved for glow (flora tips, windows, creatures, HUD). Night is deep indigo with soft stars and bioluminescent accents, not black space.

Touchstones (text only):
- **Sable**: flat-shaded smooth forms, strong silhouettes, a big empty sky gradient. Minimalism by subtraction.
- **Journey**: atmospheric perspective, one warm key, sand as value bands, very slow motion.
- **Gris**: watercolour gradient fields, one hue per space, an accent reserved for meaning.
- **Exo One**: alien planets read as sky and terrain only; enormous fog, slow cloud motion.
- **Proteus**: few prop families, colour as mood, day/night as the main event.
- **Monument Valley**: pastel value control, soft gradient sky, no texture noise.
- **Outer Wilds**: per-planet identity from sky colour and one landmark silhouette.
- **Kentucky Route Zero**: negative space, low saturation with one glowing accent.

## 4. Ranked plan

Perf context: about 300k tris for terrain + sky, p95 above 8 ms. Every item below is shader constants, fewer props, or folding work into passes that already exist. No new render passes, no shadow maps.

| # | what | where | expected effect | cost | perf risk |
|---|---|---|---|---|---|
| 1 | **Unified colour system.** Replace 10 random families with 6 violet-anchored families, each a 5-stop ramp `[deep, low, mid, high, sky]` + one `acc`. Keep `hi/lo/sea/atmo/acc` derived from the ramp so the orbital shader is unchanged in shape. Flora/rock/creature colours sample the ramp (no 12 % accent trees). | `galaxy3d.js:74-85` PALETTES, `seedIdentity :189-202` (jitter hue ±0.02 max); `ship-planet.js seedFlora :2224-2225`, rock/creature seeding | every planet reads as one designed field; violet shared across all; accent becomes meaningful | S | none |
| 2 | **Lighting model + tonemap owner.** One shared `LIGHT_GLSL`: hemisphere (sky = horizon stop, ground = deep stop) + one warm wrapped key, used by PATCH_FRAG, INST_FRAG, OUT_FRAG, ship-world FRAG_LIT, ship-human FRAG. Village moves off MeshBasic onto it. AgX tonemap (r160 has it), exposure 1.0, materials stop self-clamping. | `ship-planet.js:343-345, 381-383, 554-555`; `ship-world.js:75-87`; `ship-human.js:30-43`; `ship-village.js:328`; `galaxy3d.js:2678-2679` | violet shadows, warm lights, one consistent light across terrain/props/buildings/people | M | ~0 (same ALU class) |
| 3 | **Sky dome redesign.** Opaque by day (alpha → 1 at depth 1 when sun up; space shows only at night/high altitude). 3-stop gradient (horizon haze → mid → zenith violet), soft sun disc + wide halo, slow lava-lamp band noise (2-octave, 0.02 Hz), soft round stars at night (not 1-px cell hashes). Fog colour = horizon stop exactly. | `ship-planet.js DOME_FRAG :741-797`, `update :2792, :2800-2801` | biggest single change: every frame gets an atmosphere and a horizon | S | ~0 (24x16 sphere already drawn) |
| 4 | **Atmospheric perspective + height fog.** Fog toward the *sky* colour with luminance lift; 2-3x density on foot; height term (denser in valleys: `exp(-h/H0)`); sun-side in-scattering (`pow(dot(V,L),8)` tint). Cap 0.93 → 0.98 so far ridges dissolve. | `ship-planet.js PATCH_FRAG :360-363`, INST/OUT frags, `:2792-2796` | layered depth: near dark, mid violet, far sky; hides LOD seams for free | S | none; can shrink flora fade radii (fewer instances) |
| 5 | **Terrain as gradient bands.** Delete `fbm(p*6.3)` (`:260`), `tn` modulation (`:287, :291-297`), `dn` detail (`:310-312`). Replace with ramp lookup by `vH` (5 stops, smoothstep bands) + slope pushes one stop darker (not grey, `:356`). Optional 3-step toon quantize of the key term with soft edges (0.08). | `ship-planet.js PATCH_FRAG :251-313, :356` | smooth Sable/Journey ground; reads minimal from orbit to foot | S | **saves** 3 fbm/vnoise calls per fragment (fill-bound win) |
| 6 | **Prop silhouette language.** Three families only: *stalk* (tall thin tapered spire 8-14 L, 3 sizes), *blob* (smooth ellipsoid canopy on a thin stem), *stone* (smooth rounded pebble, flattened). Indexed smooth normals (12-16 sides) instead of flat facets. Lush: groves (cluster by low-freq noise threshold, 60 % of cells empty), cap 700 → 350. Delete mushroom/palm and conifer-stack. | `ship-planet.js GeoBuilder :1051-1106` (add smooth-normal path), `build() :1410-1424, :1491-1494`, `seedFlora :2198-2206`, `seedRocks` | fewer, bolder silhouettes with negative space; stops reading as asset-pack low-poly | M | net down: half the instances; tri count per tree up ~1.5x, total similar |
| 7 | **Slow generative motion.** Sway `uTime*1.7` → `0.35` with a slow travelling gust wave (world-space noise, 0.05 Hz). Cloud shadows: one low-freq noise projected on terrain (`col *= 1 - 0.18*cloud(sp*k + wind*t)`). Water: halve wave speeds and keep specular. Aurora always-on faint on violet planets at night. | `ship-planet.js :420, :334-336`, PATCH_FRAG, `AUR_FRAG :513-527`, weather | the lava-lamp feel; world breathes instead of jitters | S | 1 vnoise per terrain fragment (offset by #5 savings) |
| 8 | **Bloom only on emissives.** Threshold 0.82 → 1.0 in linear HDR, strength 0.48 → 0.35; emissives (flora tips, windows, jellies, scan glow) pushed to 1.5-3.0 explicitly; flora top glow `*1.8` → accent-only `*2.5` on 10-20 % of instances. Thruster glow clamped on foot. | `galaxy3d.js:2822`; `ship-planet.js:394`; ship thruster mats | no white blobs; glows become rare, intentional accents | S | can lower `bloomScale` since less to blur |
| 9 | **Structures match the world.** Village/7/11/hub walls take the planet ramp (`mid` walls, `deep` roofs, `acc` windows); round/soft profiles where cheap (bevelled roof eaves, cylinder towers); keep brand red/green only on the 7/11 sign. Fog + shared lighting on all. | `ship-village.js:138, :328, :339-341`; `ship-world.js:142, :149` + store builders; `ship-hub.js` | buildings sit *in* the field instead of on top of it | M | none |
| 10 | **Creature + character palette tie-in.** Creatures from the ramp (`low/mid`) with `acc` eyes/glow; villager ROLE_COL → ramp-relative shades (role by shape/hat, not hue); player suit gets a violet/white default with the orange as an unlockable. | `ship-planet.js` creature seeding; `ship-village.js:43`; `ship-human.js` defaults | characters belong to the planet; HUD violet echoes the suit | S | none |
| 11 | **HUD/world agreement.** Feed the active planet's `acc` and `sky` stops into CSS vars (`--hud-hot`, `--hud-line`) on land; keep violet as the base. Thin the on-foot HUD: hide the hotbar/markers when idle for 4 s. | `style/ship.css:6-13`; ship.js HUD update | screen reads as one piece; fewer UI boxes = more minimal | S | none |
| 12 | **Post pass (folded, no new pass).** Patch OutputPass fragment: vignette (0.25 at corners), 1.5 % animated grain, slight lift of blacks toward deep-violet (`max(c, vec3(0.03,0.015,0.06))`), warm/cool split-tone by luminance. | `galaxy3d.js setupPost :2807-2840` (`onBeforeCompile` on OutputPass material) | filmic cohesion, ties every planet to the site's violet | S | ~0 (same fullscreen pass) |
| 13 | **Delete list.** Ground colour noise (`:260, :287, :310-312`); the 12 % accent trees (`:2224`); mushroom/palm variant (`:1418-1421`); flat-slab rock cluster (`:1491-1494`); 1-px hash stars by day (`:756-758`); grey slope darkening (`:356`); speckle on terra (`:270-273`); one of 3 village hut templates; torches by day. | listed | less clutter, the remaining forms read | S | **saves** time |
| 14 | **Night pass.** Night = deep indigo sky stop (not transparent), terrain lit by sky fill at 0.25 and moon key cool violet; bio spots only on `acc` instances; fireflies 400 → 120, larger, slower. | `ship-planet.js :2798, :2824, :2843-2848`, DOME_FRAG | night becomes the signature mood (violet + glow) | S | saves particles |

## 5. Do this first (items 3, 1, 2: sky, colour, light)

These three are mostly constants and change every frame.

### Step A: sky dome opaque by day + fog = horizon (item 3), `js/ship-planet.js`

`:2801`
```js
// before
a.domeU.uAlpha.value = Math.pow(ps.depth, 0.8) * 0.45; a.domeU.uStar.value = Math.pow(ps.depth, 0.8);
// after: full sky when inside the atmosphere by day, thinner at night so the real stars show
a.domeU.uAlpha.value = Math.pow(ps.depth, 0.6) * (0.55 + 0.45 * sunTerm); a.domeU.uStar.value = Math.pow(ps.depth, 0.8);
```
`:763`
```glsl
// before
float seeThru = mix(0.4, 0.84, uSun);
// after
float seeThru = mix(0.55, 1.0, uSun);
```
`:752-754`: replace the zenith with a 3-stop gradient and add a slow band:
```glsl
vec3 hor = uFogCol;
vec3 mid = mix(uFogCol, uAtmo, 0.55) * (0.35 + 0.65 * uSun);
vec3 zen = mix(uAtmo * vec3(0.55, 0.45, 0.95), vec3(0.06, 0.03, 0.14), 1.0 - uSun) ;
float bands = 0.5 + 0.5 * sin(c * 9.0 + dot(v, vec3(1.3, 0.2, 0.7)) * 3.0 + uTime * 0.12);   // needs uTime: SH.uTime in domeU
vec3 col = mix(hor, mid, smoothstep(0.0, 0.35, z));
col = mix(col, zen, smoothstep(0.25, 1.0, z));
col *= 1.0 + (bands - 0.5) * 0.06;
```
`:761`: sun disc + halo instead of the 0.10 glow:
```glsl
float glow = pow(max(sd, 0.0), 6.0) * 0.35 * uSun + smoothstep(0.9993, 0.9997, sd) * 1.6 * uSun;
col += vec3(1.0, 0.86, 0.72) * glow;
```
`:2792`: fog brighter, to the horizon colour:
```js
// before ... * fb * 0.6 + 0.02
var hz = 0.15 + 0.85 * sunTerm;
SH.uFogCol.value.set((A[0]*0.6 + Hh[0]*0.4) * hz * 0.95 + 0.04, (A[1]*0.6 + Hh[1]*0.4) * hz * 0.85 + 0.03, (A[2]*0.6 + Hh[2]*0.4) * hz + 0.08);   // slight violet bias at every hour
```
`:2796`: density ×2.4 on the ground:
```js
SH.uFogK.value = (0.78 / (400 * L)) * 1.0 * Math.exp(-altF / 450) * smoothstep(0.0, 0.04, ps.depth);
```
and at `:361` `f = min(f, 0.93)` → `0.985`.

### Step B: violet-anchored palettes (item 1), `js/galaxy3d.js:74-85`

Each family keeps the five keys the shaders already read, but values are chosen as one ramp around violet, with value contrast lo << hi and one accent:
```js
var PALETTES = [
  { name: 'dusk-lav',   lo: 0x3B2A5E, hi: 0xB9A5E3, sea: 0x1E2F57, atmo: 0xC9A8FF, acc: 0xFFC38A },
  { name: 'plum-sand',  lo: 0x5A3550, hi: 0xE8C9B0, sea: 0x2A2450, atmo: 0xF0B8E0, acc: 0x7DF0E0 },
  { name: 'iris-moss',  lo: 0x2F3B4A, hi: 0x9FB8A8, sea: 0x2B2160, atmo: 0xB8A8FF, acc: 0xF2E06B },
  { name: 'ember-vio',  lo: 0x3A1E3F, hi: 0xD98C7A, sea: 0x24164A, atmo: 0xFF9FC8, acc: 0xFFE3A0 },
  { name: 'glacier',    lo: 0x3C4470, hi: 0xDDE4FF, sea: 0x1B2448, atmo: 0xA8C4FF, acc: 0xE08CFF },
  { name: 'nocturne',   lo: 0x1F1838, hi: 0x7E6BB8, sea: 0x0F1430, atmo: 0x8C6BFF, acc: 0x6CFFD0 }
];
```
In `seedIdentity :197-201` reduce jitter to `(0.02, 0.05)` hue/lightness so the family survives. Change `lookFromPalette` (`ship-planet.js:1142`) thresholds if `lush/rocky/icy` classification shifts (verify with `ps.lookOf` on 10 planets). Flora at `ship-planet.js:2224-2225`: delete the accent branch; `tt` stays, with `col = lo*(1-tt) + hi*tt*0.85`, and set `acc` only into `floraBio` for night tips.

### Step C: hemisphere + key lighting (item 2), shared chunk

Add near `NOISE_GLSL` in `ship-planet.js`:
```glsl
// LIGHT_GLSL: sky fill + warm wrapped key. uFogCol doubles as the sky colour.
vec3 litHemiKey(vec3 base, vec3 N, vec3 up, vec3 L, vec3 sky, float night) {
    float h = dot(N, up) * 0.5 + 0.5;
    vec3 fill = mix(base * vec3(0.18, 0.12, 0.26), sky * 0.9 + vec3(0.04, 0.02, 0.08), h);
    float w = clamp((dot(N, L) + 0.3) / 1.3, 0.0, 1.0);
    vec3 key = vec3(1.0, 0.9, 0.8) * smoothstep(0.0, 1.0, w) * (1.0 - 0.85 * night);
    return base * (fill * 0.75 + key * 0.9);
}
```
Then:
- PATCH_FRAG `:343-345`: `col = litHemiKey(col, N, normalize(vUp), uLightDir, uFogCol, uNightF);` (add `uNightF` = SH.uNight).
- INST_FRAG `:381-383`: `vec3 col = litHemiKey(vCol, N, normalize(vUp), uLightDir, uFogCol, uNightF);`. Pass `vUp` from the instance position (`normalize(iw.xyz - planetCentreWorld)`) or use `uUpW` (the dome's `uUp`) as an approximation on foot.
- OUT_FRAG `:554-555`: same.
- `ship-world.js:82-85` and `ship-human.js:37-40`: replace the hard-coded `vec3(-0.62,0.52,0.4)` with uniforms bound to `SH.uLightDir` / `SH.uFogCol` (pass them from ship.js on mount) and add the same fog line as INST_FRAG. This is also where the village switches from `MeshBasicMaterial` (`ship-village.js:328`) to the ship-world lit ShaderMaterial.
- `galaxy3d.js:2678-2679`: `THREE.AgXToneMapping`, exposure `1.0`; re-check the orbit view (galaxy) since it shares the renderer; if the galaxy view needs ACES, switch per mode in the ship enter/exit hooks (`ship.js:450-467` already snapshots `toneMappingExposure`).

Verification for A-C: before/after screenshots at the same 8 spots (orbit, descent, landed noon, landed dusk, village, 7/11, shore, night), plus the g-suite perf line (p95 must not rise more than 0.3 ms).

## 6. Risks / unknowns

- Patch must stay pixel-equal to the globe at ≥ 2 R (rev 24 contract, `PATCH_FRAG :274-280`). Item 5 edits only the `uSurf > 0.001` branch; item 1 changes the globe too, so they move together.
- Opaque dome hides other planets/black hole from the ground by day (rev 19 "ghosts by day" intent at `:763`). Planets near zenith can be added back as dome-drawn discs like the moons (`:781-795`).
- AgX vs ACES on the galaxy view is untested; the site's orbit look may shift. Measure before switching globally.
- Fewer flora instances changes `rockIds`/tree-breaking (Q56) ids; keep the lattice-cell id scheme.
