# changes-colors (2026-11-27): pre-cdcc583 planet colours restored

Owner liked the colours before cdcc583 ("violet-anchored 6-family ramps"). Files touched: `js/galaxy3d.js`, `js/ship-planet.js` only.

## What changed

js/galaxy3d.js
- `PALETTES`: the 6 violet families replaced by the 10 original families. Ramp machinery kept (`node.pal.ramp/deep/lo/mid/hi/sky`, `ps.palette`, `uRDeep/uRMid/uRSky`).
- `OLD_TO_NEW` removed; `familyIndexFor` rolls uniformly over 10 (same roll the game used before cdcc583, so saved planets get their old family back).
- Per-node jitter restored to the old amounts for sea/atmo/acc (0.05/0.08, 0.03/0.06, 0.04/0.06); the shared ramp hue/lightness wobble widened slightly (hue +-0.025, lightness +-0.04) to match the old hi/lo jitter.

js/ship-planet.js (violet forced regardless of family, now family-relative)
- `litHemiKey` shadow fill: `base * vec3(0.2, 0.13, 0.3)` + `vec3(0.04, 0.02, 0.09)` constant indigo -> `base * 0.2` and a `sky` term (sky = fog colour, which carries the family).
- Slope shading `col * vec3(0.6, 0.48, 0.74)` -> `col * 0.6`.
- Fog colour night floor: constant `+0.06/0.06/0.2 * night` indigo -> `0.5 * ramp deep stop * night`; constant blue `+0.08` base lowered to `+0.04` to match.
- Sky dome: night `mid` add and night zenith constants `vec3(0.03,0.03,0.14)` / `vec3(0.05,0.05,0.26)` / violet `atmo * vec3(0.55,0.45,0.95)` -> derived from `uFogCol` (deep stop at night) and `uAtmo * 0.6`.
- Not reverted: motion, DoF, cloud shadows, night exposure, moon key (cool 0.5/0.62/1.0 moonlight kept), fireflies, aurora (keyed on atmo blue, works for any family; always-on bands only where atmo is blue-dominant: coral-sea, ember-ice, dune-sky, rose-quartz, lav-heath), floating islands (look 2 or hue 255-350: lav-heath, gilded sea not used, mostly lav-heath / icy looks).
- Already family-relative from cdcc583 (kept): terrain ramp, cliffs, flora tint, rocks (deep/mid blend), foam, icy rim.

Subplanets: untouched. Every node, child or root, goes through the same `seedIdentity`/`familyIndexFor`; only the table and roll width changed. Grep: nothing else references `OLD_TO_NEW`/`PALETTES`.

## Derived ramps (deep = lo, l x0.45; mid = lerp(lo,hi,.5); sky = hi, l +0.25 cap 0.95)

| family | deep | lo | mid | hi | sky | sea | atmo | acc |
|---|---|---|---|---|---|---|---|---|
| coral-sea | #70271A | #D96D5A | #E78873 | #F4A28C | #FDECE8 | #2E8C8C | #7DE8F0 | #FFE3C8 |
| gilded | #523C15 | #B7852F | #D09F3D | #E8B84B | #F6E5BC | #6D4FA3 | #F26BD8 | #FFE9A8 |
| mint-rose | #255C47 | #5FBF9A | #7FD3B0 | #9FE6C5 | #EBFAF3 | #B5496B | #FFA7C4 | #EAFFF2 |
| tide-fire | #502917 | #B25A33 | #C97241 | #E08B4F | #F3D2BB | #2FB3A8 | #63F2DC | #FFC894 |
| lav-heath | #3C2960 | #8B6FC2 | #A28AD3 | #B9A5E3 | #F0ECF9 | #7FA893 | #D9B8FF | #F2E8FF |
| ember-ice | #49131F | #A32B44 | #BD3850 | #D6455C | #EDADB8 | #3E6C99 | #A8D8FF | #FF9AA8 |
| absinthe | #404F1A | #8FB03A | #AAC84D | #C4E060 | #ECF5CB | #7A4A8C | #E0A8F2 | #F4FFB8 |
| orchard | #743B1B | #DB8E63 | #E6A379 | #F2B98F | #FCF1E8 | #2F8C66 | #8FF2C2 | #FFE0C2 |
| dune-sky | #5C4321 | #C29455 | #D3AA72 | #E3C08F | #FAF4EB | #3573B5 | #7FB8FF | #FFEFC4 |
| rose-quartz | #672A3E | #C97A94 | #D991A6 | #E8A8B8 | #FAEBEF | #5C6FA8 | #93A8F2 | #FFD8E8 |

## By-eye checklist (nothing here was seen by eye)
1. Galaxy/orbit view: ten distinct planet hues again (coral, gold, mint, orange, lavender, crimson, lime, peach, sand, pink).
2. Land on a coral-sea, a mint-rose and an absinthe planet: ground, trees, rocks read coral / mint / lime, not purple.
3. Cliffs and slopes: darker version of the family, no violet cast on slopes.
4. Dusk and night: dark but tinted by the family deep stop (mint planet = dark green night, coral = dark red-brown), not indigo. Check the sky dome zenith at night.
5. Horizon fog during the day matches atmo glow (cyan halo on coral, pink on mint).
6. Water: sea colours (teal on coral, plum on lime, magenta-red on mint) and foam tinted by the sky stop.
7. Peaks/caps: pale family tint, not grey-violet.
8. Subplanets (menu children) still colour inherited from their parent family and still enterable.
9. Night too dark? tweak `0.5 * Dp[]` in `ship-planet.js` fog line and `uFogCol * 0.5` in the dome zenith.
