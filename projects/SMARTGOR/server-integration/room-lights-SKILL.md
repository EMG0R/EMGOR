# Skill: Room Lighting Control (lightserver / LED ring)

You can physically control the room's LED lighting right now by running
`curl` commands against `lightserver`, the Python/FastAPI service on the Pi
at `http://localhost:8800`. This is real hardware — a curl you run actually
changes the light in the room. Issue the curl, don't just describe it.

## The ring model — read this first

The room has **one continuous 1920-pixel LED ring**, stitched from 3 ESP32
strips (`fcob-strip` 0-799, `fcob-strip2` 800-1279, `fcob-strip3` 1280-1919,
seam wraps from 1919 back to 0). `lightserver` renders it as ONE physics/
graphics world and slices the frame per strip automatically. You always
address the whole ring — there is no per-strip control, and you should
never think in terms of individual strips. Positions in scenes are ring
indices `0..1919`; pixel 1919 is physically adjacent to pixel 0.

Any change you make **persists** (survives restarts) until something else
is posted. There's no separate "commit" step.

## Two ways to control it

### 1. Quick path — natural language vibe

For simple/vague requests ("cozy ember glow", "slow blue rain"), just POST
the text and let the server's own LLM pipeline turn it into a scene:

```bash
curl -s -m 8 -X POST http://localhost:8800/prompt \
  -H 'content-type: application/json' \
  -d '{"text":"aurora borealis, very slow"}'
```

Response includes `"source"` (`llm`, `llm-retry`, or `fallback`) and the
generated `"scene"`. Use this when precision doesn't matter.

### 2. Full path — author scene JSON yourself

For anything specific, complex, or precise ("three orbiting suns", "green
background with bouncing collision dots", "a comet chasing sparks with long
trails"), **write the scene DSL directly** and POST it to `/scene`. This is
where full programmability lives — you are not limited to the canned vibes
below.

```bash
curl -s -m 8 -X POST http://localhost:8800/scene \
  -H 'content-type: application/json' -d @scene.json
```

Any `?node=` query param is accepted and silently ignored (the ring can't
be split) — never try to target one strip.

## The Scene DSL

A scene is one JSON object:

```jsonc
{
  "name": "my scene",
  "master": {                        // all optional
    "brightness": 1.0,               // 0..4 overall gain
    "blur": 0,                       // gaussian blur sigma, px, 0..50
    "tone_map": "clip",              // "clip" (exact colors) | "reinhard" (soft HDR headroom)
    "strobe": {"mode": "pulse",      // "none" | "pulse" (sine breathing) | "strobe" (square)
               "rate_hz": 0.5, "depth": 0.4, "duty": 0.5}
    // NEVER use "strobe" fast (>3 Hz) or high "depth" — photosensitivity risk.
  },
  "layers": [ /* rendered bottom-up, each composited onto the last */ ]
}
```

Every layer has: `"type"` (required), `"blend"`: `"add"|"max"|"alpha"|"boost"`
(default `add`), `"opacity"`: 0..2 (default 1), `"decay"`: 0..0.999
(per-frame trail persistence — 0 = no trail, 0.85-0.95 = long glowing
trails). `"boost"` is a relative brightening lens: `dst *= 1 + rgb*opacity`
— good for a bright "hot" moving highlight riding on top of an ambient bed
without washing it white.

**Trails, richer form** — `"trail": {"decay": 0..0.999, "blur": 0..50}` is
a superset of plain `"decay"`: same fading-tail semantics, plus `"blur"`
(gaussian sigma, px) softens just the trail into a smear instead of a
hard-edged tail. Trail blur wraps the ring seam correctly, so it's safe to
use freely on ring scenes. `"decay": x` alone still works and is exactly
`"trail": {"decay": x}`.

**Palette motion** — `wave`/`noise`/`plasma`/`gradient`/`particles` layers
all take `"palette_speed"`: signed float, palette-cycles-per-second. `0`
(default) is static; nonzero continuously rotates that layer's hue mix
over time — this is the go-to knob for "generative", "ever-changing",
"never repeats" requests, independent of any spatial scroll the layer
already has.

### Layer types

**`particles`** — physically simulated points, splatted with a soft
(gaussian) or hard falloff. The most powerful layer type.

```jsonc
{
  "type": "particles",
  "physics": {
    "wrap": true,                    // RING SCENES: almost always true — positions
                                      // wrap mod 1920, no walls, splats cross the seam
    "gravity": {"mode": "direction", "accel": -140}      // signed accel toward px 0
             // | {"mode": "point", "point": 960, "accel": 80}  // pull toward a spot
    "damping": 0.5,                  // 1/s velocity friction
    "bounce": {"enabled": false, "restitution": 0.85},   // wall bounce (ignored if wrap:true)
    "collisions": false,             // particle-particle elastic collisions
    "wind": {"accel": 30, "vary_hz": 0.2, "vary_amount": 0.8},
    "turbulence": {"accel": 60, "scale": 80, "speed": 0.5},  // noise-driven jitter
    "attractors": [{"pos": 960, "strength": -50,          // neg = repel
                    "falloff": "constant|linear|inverse_square", "radius": 200}],
    "drive": {"top_speed": 128, "min_speed": 42.7,        // scripted "orbiting traveller":
              "reverses_per_min": 1.5, "smooth_s": 2.5,   // velocity wanders min..top,
              "mod_period_s": 37, "mod_period2_s": 23}    // direction reverses smoothly
  },
  "emitters": [{"pos": [0, 1920], "rate": 6, "vel": 20, "vel_jitter": 15,
                "lifetime": 4, "lifetime_jitter": 1.5, "colors": ["#ffe090"],
                "size": 1.4, "size_jitter": 0.4, "mass": 1, "burst": 0,
                "color_mode": "static", "hue_drift": 0}],  // multi-color, see below
  "particles": [{"pos": 100, "vel": 220, "color": "#ffb030", "size": 2.5,
                 "mass": 1, "lifetime": null}],           // explicit initial particles
  "springs": [{"a": 0, "anchor": 960, "k": 14, "damping": 0.25},   // hooke to a fixed point
              {"a": 0, "b": 1, "k": 6, "damping": 0.15, "rest": 260}], // pairwise
  "render": {"falloff": "gaussian|hard", "fade_out": true},

  "palette": ["#ff2d55", "#ffe94a", "#33c6ff"],  // optional: enables multi-color particles
  "palette_speed": 0.05
}
```

**Multi-color particles.** Add a layer-level `"palette"` (a list of hex
stops), then set `"color_mode"` per emitter (or per hand-authored
`"particles"` entry) to pull that emitter's color from it instead of its
own static `colors`/`color`:

- `"static"` (default) — unchanged; ignores the palette entirely.
- `"index"` — each particle gets a random-but-fixed offset into the
  palette at spawn, so a stream/burst comes out as a spread of hues, not
  one flat color. **Use this for "multi-color" requests** — a rainbow of
  sparks, comets each a different hue, etc.
- `"velocity"` — hue tracks current speed (fast vs. slow ends of the palette).
- `"age"` — hue tracks lifetime fraction (color shifts as the particle ages).

`"hue_drift"` (cycles/sec, per emitter) layers extra independent rotation
on top, so different emitters sharing one palette still visibly diverge
in hue over time.

**`wave`** — sum of traveling sinusoids:
`"waves": [{"wavelength": px, "speed": px/s, "amplitude": 0..1, "phase": rad}]`,
plus `"palette"` (list of hex) or `"color"`, `"bias"`. On the ring, prefer
wavelengths that divide 1920 evenly so the wave tiles seamlessly.

**`noise`** — scrolling fBm field (lava/embers/clouds): `"scale"` (px),
`"speed"`, `"drift"` (px/s spatial scroll), `"octaves"` 1-6, `"contrast"`
0..10, `"threshold"` 0..1, `"palette"`, `"brightness"` 0..4.

**`plasma`** — classic plasma interference pattern: `"scale"`, `"speed"`,
`"palette"`, `"brightness"`.

**`gradient`** — scrollable palette wash: `"palette"`, `"scroll_speed"`
(px/s), `"scale"` (px per palette cycle — **use 1920 or 1920/2/3/4... for
ring scenes** so it tiles the loop with no seam), `"brightness"`.

**`solid`** — flat fill: `"color"`.

**`paint`** — explicit per-pixel assignment, the "any LED any color"
escape hatch: `"pixels": {"0": "#ff0000", "960": "#00ff00"}`,
`"background": "#000000"` or `null` (null = only listed pixels are touched,
everything else shows the layers underneath).

### Colors & palettes

Hex strings `"#rrggbb"` (sRGB; gamma-corrected automatically). Palettes are
lists of hex colors, evenly spaced around the value range and wrapped —
use 3-5 stops for a smooth gradient.

### Ring-specific rules

- Set `"physics": {"wrap": true}` on every `particles` layer in a ring
  scene unless you deliberately want walls (you almost never do on a ring —
  there are no ends).
- Use `"gradient".scale = 1920` (or an integer divisor) and wave
  wavelengths that divide 1920, so patterns tile without a visible seam.
- Think in terms of "around the loop", not "top/bottom" — there is no top.
- Use `"drive"` for anything that should orbit continuously (suns, comets,
  a moving highlight) instead of hand-rolling velocity/gravity hacks.

## The projected wall — `"projector"` block (v2)

The room also has a projected wall, addressed via an optional top-level
`"projector"` key alongside `"master"`/`"layers"`. Include one for
whole-room vibes (almost always, unless explicitly ring-only):

```jsonc
"projector": {
  "style": "particles"|"aurora"|"flowfield"|"wash"|"react"|"bleed",
  "params": {"density":0-2,"speed":0-2,"turbulence":0-2,"hue_drift":0-1,"fade":0-2},
  "energy": 0.6, "palette": ["#ff2f6e","#00e0ff"], "brightness": 0.85,
  "glsl": "<fragment body>"          // OVERRIDES style/params when present
}
```

Simple/ambient vibes: `style`+`params`. Distinctive/characterful vibes:
author `"glsl"` instead — a fragment body defining
`vec3 wall(vec2 uv, float t)` (uv 0..1 across the wall, y down).
Available: `ringAt(u)->vec3` (calibrated LED colors at ring position u),
`uEnergy` (0-1.5 smoothed room energy), `uPalA`/`uPalB` (vec3 dominant
colors), `uComets[8]` `vec4(u,age_s,vel,strength)` + `uCometCount`
(ground-truth moving objects), `rhash`/`rnoise`/`rfbm` noise helpers.
Broken glsl never blanks the wall (hot-compiled, last-good fallback) —
be bold but syntactically correct. After posting a glsl scene, check the
wall status endpoint (GORMAPPER's docs — `TODO: exact URL`, expected
under `/api/wall/status`) and regenerate `projector` if it reports a
compile error.

**Core mechanic — "a pin dropped in water"**: a moving object crossing
the LED window strikes the wall like a surface — its ripple/wake
**expands from that u position all the way to the screen edges** (full-
screen reach, never a strip-hugging band). **Color truth**: sample
`ringAt(u)` at the event's own position and let *that exact color* flow
outward from u — what leaves the strip at u continues down the wall
from u. **Doctrine** (center of gravity, not a formula — vary style/
motion between scenes, full creative latitude, you decide per scene):
bias to particle systems, aurora-like fields, ripple/wake propagation;
ring data is influence not image — energy drives spawn/tempo, `ringAt(u)`
drives color at contact, comets are events. Never a mirrored strip.

Worked example — comet ripples (pin-in-water rings, colored by `ringAt`
at each comet's own position):

```json
"projector": {
  "palette": ["#0a1a3a", "#2fe0c0", "#7a2fe0"],
  "glsl": "vec3 wall(vec2 uv, float t) { vec3 col = vec3(0.0); for (int i = 0; i < 8; i++) { if (i >= uCometCount) break; vec4 c = uComets[i]; float d = distance(uv, vec2(c.x, 0.5)); float radius = c.y * 0.6; float ring = exp(-pow(d - radius, 2.0) * 60.0) * exp(-c.y * 0.3); col += ringAt(c.x) * ring * c.w * (0.5 + uEnergy); } return col; }"
}
```

## Room zones — spatial requests

Fetch live calibration points before honoring a spatial request ("light
the front", "sweep back to front"):

```bash
curl -s -m 8 http://localhost:8800/calibration
# {"points": {"frontL":1712,"frontR":293,"backL":1214,"backR":796,
#             "projL":1737,"projR":259}, "updated": "..."}
```

Zones are consecutive points walking the ring forward (wrap through the
1919->0 seam where needed):

| zone  | range (forward from -> to) |
|-------|-----------------------------|
| front | frontL -> frontR (wraps the seam) |
| right | frontR -> backR |
| back  | backR -> backL |
| left  | backL -> frontL |

`projL`/`projR` mark the projector wall's own ring section, not a
room-side zone. Use a zone's index range to confine paint pixels,
emitter `"pos"` windows, or attractor placement — e.g. "sweep from back
to front" = an emitter/attractor traveling from the back range toward
the front range over the animation.

### Worked examples (all verified against the real parser)

**1. Simple color wash** — one layer, flat teal gradient, no motion:

```json
{
  "name": "teal-wash",
  "master": {"brightness": 0.8, "tone_map": "clip"},
  "layers": [
    {"type": "gradient", "palette": ["#003030", "#00c0a0", "#003030"],
     "scale": 1920, "scroll_speed": 0}
  ]
}
```

**2. Multi-layer: ember bed + drifting sparks** — noise field for a lava
bed plus a particle layer of emitted sparks with trails:

```json
{
  "name": "embers-with-sparks",
  "master": {"brightness": 1.0, "tone_map": "reinhard"},
  "layers": [
    {"type": "noise", "blend": "add",
     "palette": ["#100000", "#ff3000", "#ffb020"],
     "scale": 320, "speed": 0.15, "octaves": 3, "contrast": 1.6, "brightness": 0.8},
    {"type": "particles", "blend": "add", "opacity": 1.1, "decay": 0.9,
     "physics": {"wrap": true, "bounce": {"enabled": false}, "damping": 0.1,
                 "turbulence": {"accel": 60, "scale": 80, "speed": 0.6}},
     "emitters": [{"pos": [0, 1920], "rate": 6, "vel": 20, "vel_jitter": 15,
                   "lifetime": 4, "lifetime_jitter": 1.5,
                   "colors": ["#ffe090", "#ff8030"], "size": 1.4, "size_jitter": 0.4}],
     "render": {"falloff": "gaussian", "fade_out": true}}
  ]
}
```

**3. Moving elements: dueling comets** — dim ambient bed plus two bright
comets orbiting the ring in opposite directions with glowing trails
(same pattern used by the boot-default "orb" scene — see
`scenes/default.json` for another good reference):

```json
{
  "name": "dueling-comets",
  "master": {"brightness": 1.0, "tone_map": "reinhard"},
  "layers": [
    {"type": "gradient", "palette": ["#04021a", "#0a0430", "#04021a"],
     "scale": 1920, "scroll_speed": 2, "brightness": 0.4},
    {"type": "particles", "blend": "add", "opacity": 1.3, "decay": 0.92,
     "physics": {"wrap": true, "bounce": {"enabled": false},
                 "drive": {"top_speed": 220, "min_speed": 90, "reverses_per_min": 0,
                           "smooth_s": 1.0, "mod_period_s": 20, "mod_period2_s": 13}},
     "particles": [{"pos": 0, "vel": 220, "color": "#ff5030", "size": 4.0, "lifetime": null}],
     "render": {"falloff": "gaussian", "fade_out": false}},
    {"type": "particles", "blend": "add", "opacity": 1.3, "decay": 0.92,
     "physics": {"wrap": true, "bounce": {"enabled": false},
                 "drive": {"top_speed": 220, "min_speed": 90, "reverses_per_min": 0,
                           "smooth_s": 1.0, "mod_period_s": 17, "mod_period2_s": 11}},
     "particles": [{"pos": 960, "vel": -220, "color": "#30a0ff", "size": 4.0, "lifetime": null}],
     "render": {"falloff": "gaussian", "fade_out": false}}
  ]
}
```

**4. Multi-color trailing comets** — one shared `"palette"` on a
particles layer, `color_mode: "index"` per particle to give each a
different fixed hue off it, plus `"trail": {"decay":0.93,"blur":2.5}`
for a soft seam-safe blur. (See `scenes/comet-trails.json` in the
lightserver repo for a full 3-comet version, `scenes/aurora-drift.json`
for a `palette_speed`-driven generative ambient, and a worked
`"projector"` glsl example above — all good copy-from references.)

## Reading back what's live

```bash
curl -s -m 8 http://localhost:8800/scene    # the exact scene JSON currently running
```

Use this to check what's playing before you decide whether to change it,
or to grab the current scene as a starting point to tweak.

## Direct pixel painting

For "light up exactly these LEDs" requests, skip the DSL and POST indices
directly (indices are **ring** indices 0..1919):

```bash
curl -s -m 8 -X POST http://localhost:8800/pixels \
  -H 'content-type: application/json' \
  -d '{"pixels":{"0":"#ff0000","640":"#00ff00","1280":"#0000ff"},"base":"black"}'
```

`"base"`: `"black"` (only listed pixels lit) or `"current"` (merge onto
whatever paint scene is already running).

## Inspecting & controlling state

```bash
curl -s -m 8 http://localhost:8800/state    # current scene name, fps, streaming, per-strip online status
curl -s -m 8 http://localhost:8800/scene    # the exact scene JSON currently running
curl -s -m 8 http://localhost:8800/nodes    # node registry + ring segment layout
curl -s -m 8 -X POST http://localhost:8800/stop     # stop streaming (strip falls back to onboard effect after 5s)
curl -s -m 8 -X POST http://localhost:8800/resume   # resume streaming
```

**Always verify with `GET /state` after posting a scene** — check
`"scene"` matches what you set, `"streaming": true`, and that
`fps_actual` is close to 30. If a segment shows `"online": false`, that
strip is offline/unreachable; the rest of the ring still works.

## Restoring the default

To go back to the boot-default standing scene (dark purple lava-lamp bed
with a slow orbiting highlight):

```bash
curl -s -m 8 -X POST http://localhost:8800/prompt \
  -H 'content-type: application/json' -d '{"text":"default"}'
```

This won't necessarily reproduce it exactly (it's NL-generated). For an
exact restore, POST the literal contents of `scenes/default.json` from the
lightserver repo to `/scene` if you have access to that file; otherwise
`{"text":"calm dark purple lava lamp ambient"}` via `/prompt` gets close.

## Practical rules

- Always address the whole ring. Never try to target `fcob-strip`,
  `fcob-strip2`, or `fcob-strip3` individually — that bypasses the ring
  abstraction and will look wrong (wrong offsets, visible seams).
- Changes persist until something else is posted — no need to "save".
- Prefer `/scene` with hand-written JSON over `/prompt` whenever the user
  wants something specific, precise, multi-part, or physically simulated
  (orbiting, bouncing, colliding, springs) — `/prompt` is best-effort NL
  generation and can miss nuance.
- Keep total brightness sane — this lights a room people sit in. Avoid
  `master.brightness` above ~1.5 and never use fast/deep strobe.
- After any change, confirm with `GET /state`.
