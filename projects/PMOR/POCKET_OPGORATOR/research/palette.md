# PMOR NeoTrellis palette — mint housing edition

Housing PETG: RGB(52, 235, 171) = OKLCH(L 0.84, C 0.168, H 163.6°). All colours designed in OKLCH (perceptually uniform lightness/chroma steps), gamut-mapped to sRGB with hue and lightness preserved, then quantised to 0–255 for the WS2812Bs.

## Colour-theory basis

* Complement of the housing: -16° (hot pink) → FX DOWN.
* Square tetrad: 164° (housing) / 254° (blue) / -16° (pink) / 74° (yellow) → FX UP, FX DOWN, FX LEFT.
* Split-complement: -46° (violet) / 14° (red-orange) → FX RIGHT (292°, pulled bluer for separation), DRUMS (35°).
* Analogous: 128° lime → GLOBALS; the only green, kept ≥20 ΔE from the housing.
* The housing itself is *not* used as a pad colour; the old sampler teal (168°) was ΔE≈8 from the housing and has been moved to sky-cyan (215°).
* Firmware hues kept where they fit: lava blue 256°, lava purple 302°, patches yellow 93°, kick red 30°.
* Everything is high-chroma and L 0.6–0.9: at 1/4 brightness through translucent PETG, low-chroma colours collapse to grey-mint.
* Mint PETG transmits green ≫ blue > red. Reds/pinks lose ~35% relative to green, so warm colours are specified slightly brighter/more saturated than their cool counterparts, and the 'white' states carry extra red.


## 1 · FX families

| Key | Full RGB | Hex | ¼ RGB (firmware >>2) | OKLCH | Notes |
|---|---|---|---|---|---|
| FX UP · stutter/spatial | `77,167,255` | `#4da7ff` | `19,41,63` | L0.71 C0.15 H250° | Tetrad blue (housing +90°). Keeps the firmware's lava blue hue (256°). |
| FX RIGHT · modulation | `135,63,255` | `#873fff` | `33,15,63` | L0.57 C0.26 H293° | Violet (292°) — between the tetrad blue and the complement; near the firmware lava purple (302°), pulled 10° bluer and darker so it separates from FX DOWN. |
| FX DOWN · distortion/ringmod | `255,65,180` | `#ff41b4` | `63,16,45` | L0.69 C0.25 H349° | Hot pink (350°) — the complement of the mint (344°) nudged +6° toward red and lightened, for the most aggressive FX. |
| FX LEFT · reverse | `255,210,46` | `#ffd22e` | `63,52,11` | L0.88 C0.17 H92° | Tetrad yellow (housing −90°, nudged to the firmware's existing 93° yellow). |
| FX HOME · blend of four | `193,138,212` | `#c18ad4` | `48,34,53` | L0.71 C0.12 H317° | OKLab mean of the four FX colours, chroma floored at 0.12 → a soft orchid; distinct from every FX row and from the mint. |

## 2 · Engine identities

| Key | Full RGB | Hex | ¼ RGB (firmware >>2) | OKLCH | Notes |
|---|---|---|---|---|---|
| DRUMS | `255,82,35` | `#ff5223` | `63,20,8` | L0.67 C0.22 H35° | Coral/red-orange (35°) — warm pole opposite the cool housing; keeps the firmware kick red. |
| SYNTH | `59,109,244` | `#3b6df4` | `14,27,61` | L0.58 C0.21 H265° | Indigo blue (265°) — deeper and bluer than FX UP; keeps the firmware's synth-blue family. |
| SAMPLER | `0,219,255` | `#00dbff` | `0,54,63` | L0.82 C0.15 H215° | Sky cyan (215°). The old sampler teal (168°) is almost identical to the housing (ΔE≈8) and vanished — moved +50° toward cyan. |
| PATCHES | `255,215,65` | `#ffd741` | `63,53,16` | L0.89 C0.16 H93° | Unchanged firmware yellow (93°) — the user's favourite. |
| GLOBALS | `148,214,0` | `#94d600` | `37,53,0` | L0.80 C0.21 H129° | Lime (128°) — analogous to the mint (−36°), the one green in the set; reads as 'system/settings'. ΔE 22 from the housing, the tightest but still above threshold. |
| DASH | `200,174,250` | `#c8aefa` | `50,43,62` | L0.80 C0.11 H300° | Lavender (300°, moderate chroma) — calm home tint; the only pale purple in the engine set. |

## 3 · Drum quadrant tints (all within ±30° of DRUMS)

| Key | Full RGB | Hex | ¼ RGB (firmware >>2) | OKLCH | Notes |
|---|---|---|---|---|---|
| KICKS | `238,20,1` | `#ee1401` | `59,5,0` | L0.60 C0.24 H30° | Deepest, most saturated — hue −5° (toward red). |
| SNARES | `255,126,0` | `#ff7e00` | `63,31,0` | L0.73 C0.19 H52° | Lighter, hue +18° (toward orange/amber). |
| HATS | `255,184,106` | `#ffb86a` | `63,46,26` | L0.83 C0.13 H67° | Lightest, lowest chroma — peach; hue +30°. |
| PERC | `250,65,107` | `#fa416b` | `62,16,26` | L0.66 C0.22 H13° | Hue −22° (toward rose/pink) at mid lightness. |

## 4 · Functional states

| Key | Full RGB | Hex | ¼ RGB (firmware >>2) | OKLCH | Notes |
|---|---|---|---|---|---|
| FOCUSED PAD (warm white) | `255,214,190` | `#ffd6be` | `63,53,47` | L0.90 C0.06 H52° | Warm white, ~3000K. Mint PETG passes green/blue and eats red, so a neutral white comes out minty; the extra red compensates and it reads white through the housing. |
| SAMPLE-FILLED PAD | `122,241,255` | `#7af1ff` | `30,60,63` | L0.89 C0.11 H206° | SAMPLER hue, lightness pushed to 0.90 — 'brighter sampler'. |
| STEP CURSOR | `215,235,255` | `#d7ebff` | `53,58,63` | L0.93 C0.03 H248° | Cool ice-white (~7500K) — moves, so it separates from the static warm-white focus by temperature as well as motion. |
| DELETE 3 | `155,31,27` | `#9b1f1b` | `38,7,6` | L0.45 C0.16 H28° | Red ramp start (dim). |
| DELETE 2 | `223,35,33` | `#df2321` | `55,8,8` | L0.58 C0.22 H28° | Red ramp middle. |
| DELETE 1 / GO | `255,41,39` | `#ff2927` | `63,10,9` | L0.64 C0.24 H28° | Red ramp end — maximum chroma pure red. |
| SAVING | `255,165,0` | `#ffa500` | `63,41,0` | L0.79 C0.17 H71° | Amber pulse (70°) — between drums-orange and patches-yellow, used nowhere else. |

## 5 · Lava-lamp base (full-scale keys, dimmed 1/16 in use)

| Key | Full RGB | Hex | ¼ RGB (firmware >>2) | OKLCH | Notes |
|---|---|---|---|---|---|
| LAVA A · blue | `79,161,255` | `#4fa1ff` | `19,40,63` | L0.70 C0.16 H254° | Same hue as FX UP; run at >>4 (≈1/16) in firmware. |
| LAVA B · violet | `148,100,238` | `#9464ee` | `37,25,59` | L0.62 C0.20 H296° | Same hue family as FX RIGHT; >>4. |
| LAVA C · gold | `238,188,74` | `#eebc4a` | `59,47,18` | L0.82 C0.14 H85° | Lower-chroma yellow; >>4. Gold + blue + violet is the firmware's existing lava triad minus the green (which disappears into the mint). |

## 6 · ΔE2000 audit (full-brightness sRGB; flag threshold < 20)

### vs housing mint

| Family | ΔE2000 | |
|---|---|---|
| UP | 44.3 | ok |
| RIGHT | 60.3 | ok |
| DOWN | 89.1 | ok |
| LEFT | 37.1 | ok |
| DRUMS | 70.9 | ok |
| SYNTH | 55.3 | ok |
| SAMPLER | 30.5 | ok |
| PATCHES | 35.8 | ok |
| GLOBALS | 22.0 | ok |
| DASH | 44.6 | ok |

### between families

| | UP | RIGHT | DOWN | LEFT | DRUMS | SYNTH | SAMPLER | PATCHES | GLOBALS | DASH |
|---|---|---|---|---|---|---|---|---|---|---|
| UP | — | 33 | 47 | 58 | 51 | **19** | 21 | 58 | 66 | 25 |
| RIGHT | 33 | — | 26 | 80 | 49 | **16** | 47 | 80 | 97 | 29 |
| DOWN | 47 | 26 | — | 69 | 35 | 35 | 71 | 69 | 88 | 25 |
| LEFT | 58 | 80 | 69 | — | 42 | 72 | 50 | **2** | 23 | 59 |
| DRUMS | 51 | 49 | 35 | 42 | — | 50 | 57 | 43 | 64 | 43 |
| SYNTH | **19** | **16** | 35 | 72 | 50 | — | 38 | 72 | 73 | 27 |
| SAMPLER | 21 | 47 | 71 | 50 | 57 | 38 | — | 49 | 48 | 36 |
| PATCHES | 58 | 80 | 69 | **2** | 43 | 72 | 49 | — | 22 | 58 |
| GLOBALS | 66 | 97 | 88 | 23 | 64 | 73 | 48 | 22 | — | 71 |
| DASH | 25 | 29 | 25 | 59 | 43 | 27 | 36 | 58 | 71 | — |

### Flags

* UP vs SYNTH: ΔE 19.1
* RIGHT vs SYNTH: ΔE 15.7
* LEFT vs PATCHES: ΔE 1.8

Context note: FX colours and engine colours never appear on the same page, so cross-set near-misses (FX UP / SYNTH, FX RIGHT / SYNTH — both blue families) are tolerated; LEFT / PATCHES are deliberately the same yellow. The hard constraints — every pair *within* the FX set and *within* the engine set ≥ 20, every family vs housing ≥ 20 — all pass.


Drum quadrant ΔE from DRUMS: KICKS 9, SNARES 14, HATS 26, PERC 20 (small on purpose — same family).

FX-set minimum ΔE: 25.9 (RIGHT–DOWN).

## Firmware drop-in

```cpp
// 1 · FX families
static const uint8_t C_UP      [3] = { 77,167,255};  // FX UP · stutter/spatial
static const uint8_t C_RIGHT   [3] = {135, 63,255};  // FX RIGHT · modulation
static const uint8_t C_DOWN    [3] = {255, 65,180};  // FX DOWN · distortion/ringmod
static const uint8_t C_LEFT    [3] = {255,210, 46};  // FX LEFT · reverse
static const uint8_t C_HOME    [3] = {193,138,212};  // FX HOME · blend of four
// 2 · Engine identities
static const uint8_t C_DRUMS   [3] = {255, 82, 35};  // DRUMS
static const uint8_t C_SYNTH   [3] = { 59,109,244};  // SYNTH
static const uint8_t C_SAMPLER [3] = {  0,219,255};  // SAMPLER
static const uint8_t C_PATCHES [3] = {255,215, 65};  // PATCHES
static const uint8_t C_GLOBALS [3] = {148,214,  0};  // GLOBALS
static const uint8_t C_DASH    [3] = {200,174,250};  // DASH
// 3 · Drum quadrant tints (all within ±30° of DRUMS)
static const uint8_t C_KICKS   [3] = {238, 20,  1};  // KICKS
static const uint8_t C_SNARES  [3] = {255,126,  0};  // SNARES
static const uint8_t C_HATS    [3] = {255,184,106};  // HATS
static const uint8_t C_PERC    [3] = {250, 65,107};  // PERC
// 4 · Functional states
static const uint8_t C_FOCUS   [3] = {255,214,190};  // FOCUSED PAD (warm white)
static const uint8_t C_FILLED  [3] = {122,241,255};  // SAMPLE-FILLED PAD
static const uint8_t C_CURSOR  [3] = {215,235,255};  // STEP CURSOR
static const uint8_t C_DEL3    [3] = {155, 31, 27};  // DELETE 3
static const uint8_t C_DEL2    [3] = {223, 35, 33};  // DELETE 2
static const uint8_t C_DEL1    [3] = {255, 41, 39};  // DELETE 1 / GO
static const uint8_t C_SAVE    [3] = {255,165,  0};  // SAVING
// 5 · Lava-lamp base (full-scale keys, dimmed 1/16 in use)
static const uint8_t C_LAVA_A  [3] = { 79,161,255};  // LAVA A · blue
static const uint8_t C_LAVA_B  [3] = {148,100,238};  // LAVA B · violet
static const uint8_t C_LAVA_C  [3] = {238,188, 74};  // LAVA C · gold
```
