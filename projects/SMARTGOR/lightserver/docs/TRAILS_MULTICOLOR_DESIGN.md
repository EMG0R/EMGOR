# Trails + multi-color generative — design note

Status: implemented, all tests passing (see `tests/test_scene.py`,
`tests/test_physics.py`, `tests/test_api.py`, `tests/test_ring.py`).
Fully backward compatible: every key below is optional and defaults to the
old behavior byte-for-byte (`test_dsl_round_trip` and
`test_example_scene_validates_and_renders` cover every `scenes/*.json`
including `scenes/archive/`).

## What already existed

Reading the engine before touching it turned up that a real trail
mechanism was already in place: `Layer.decay` (0..0.999) keeps a
per-layer `_persist` buffer and combines it with the fresh frame via
`persist = max(persist * decay, fresh)` (`layers.py::Layer.composite`).
Because this operates in index space (no spatial shift), it was already
ring-seam-safe — there's no seam to get wrong when you're not sliding
pixels around. `scenes/archive/comet.json` already uses it. So "trails"
mostly needed a richer, more discoverable syntax and one genuinely new
capability (soft blur), not a rebuild.

Multi-stop palettes (`Palette` in `color.py`, linear-interpolated,
wrapping) were also already used by `wave`/`noise`/`plasma`/`gradient`
layers, and emitters already picked a random color per particle from a
`colors` list. What was missing: palettes for particles driven by
*runtime state* (velocity/age) instead of a fixed random pick, and any
kind of color *motion* over time (palette cycling).

## New DSL keys

### 1. `"trail": {"decay": 0..0.999, "blur": 0..50}` (any layer)

Superset of the legacy top-level `"decay"` key, which still works
unchanged (`"decay": 0.9` ≡ `"trail": {"decay": 0.9}`; if both are given,
`trail.decay` wins). `blur` is new: a per-frame gaussian blur (sigma in
px) applied to the *persistence buffer only*, before it's max-combined
with the fresh render — this softens a hard, single-pixel-wide tail into
a smeared glow, independent of `master.blur` (which blurs everything,
including the moving element itself, every frame — different, coarser
tool).

Implementation: `layers.py::_circular_blur` wraps the buffer around the
0/n edge before convolving (`np.concatenate([arr[-r:], arr, arr[:r]])`
then `valid`-mode convolve). This makes trail blur seam-correct on ring
scenes, which `master.blur` (plain `np.convolve(..., mode="same")`,
unchanged) is not — documented as a known asymmetry rather than touching
`master.blur`'s long-established non-wrap behavior for existing scenes.

Validated in `scene.py::_check_trail`; both `decay` and `blur` are
optional and independently bounded (blur 0..50, matching `master.blur`'s
range).

### 2. `"palette_speed"` (wave, noise, plasma, gradient, particles)

A signed float, palette-cycles-per-second. Added as one extra term to
each layer's existing palette lookup coordinate:

- `wave`: `u = inten*0.999 + palette_speed*t`
- `noise`: `u = v*0.999 + palette_speed*t`
- `plasma`: `u = (v/3+1)*0.5 + palette_speed*t`
- `gradient`: `u = (x + scroll_speed*t)/scale + palette_speed*t`

`Palette.lookup` already wraps its input mod 1, so this is a one-line
addition per layer and is exactly a no-op at the default `0.0`. This is
the "generative color motion" ask: a field that scrolls spatially
(`scroll_speed`/`speed`/`drift`, all pre-existing) can now *also* cycle
its hue over time, independently, giving genuinely animated multi-color
fields instead of a static-hued shape that only moves.

### 3. Particle multi-color: `palette`, `palette_speed`, `color_mode`, `hue_drift`

Layer-level (on a `"particles"` layer, alongside `physics`/`emitters`):

```json
"palette": ["#ff0040", "#ffae00", "#00eaff"],
"palette_speed": 0.05
```

Per-emitter (and per hand-authored entry in `"particles"`):

```json
"color_mode": "static" | "velocity" | "age" | "index",
"hue_drift": 0.0
```

- `static` (default): unchanged legacy behavior — the particle keeps
  whatever `colors`/`color` it spawned with, forever. A layer with a
  `palette` set but every emitter left at `static` renders **pixel-
  identical** to one with no palette at all (`test_particle_color_mode_static_is_unaffected_by_palette`)
  — palette-driven color is strictly opt-in per emitter.
- `velocity`: palette position tracks current speed,
  `u = 0.5 + 0.5*tanh(vel/60)` — fast movers sit near one end of the
  palette, slow/reversing ones near the other. Good for comets whose
  color shifts as they decelerate/bounce.
- `age`: palette position tracks lifetime fraction (`age/life`, or a
  slow 5s wrap for immortal particles) — a particle's hue evolves as it
  ages, independent of motion.
- `index`: each particle gets a fixed-but-random palette offset assigned
  at spawn (`color_u`, uniform [0,1)). A burst/stream of particles comes
  out as a spread of hues across the palette rather than one flat color
  — this is the direct "true multi-color" particle mode, and it's what
  `comet-trails.json` uses for its comet heads.
- `hue_drift` (cycles/sec, per emitter): added on top of any mode as
  `+ t * hue_drift`, giving that emitter's particles their own
  independent color rotation, additional to the shared `palette_speed`.
  Two emitters with different `hue_drift` values visibly diverge in hue
  over time even sharing one palette.

Implementation lives in `physics.py`. Colors are still stored as before
(`self.color`, static, set at spawn) for backward compat and as the
fallback when no layer palette is configured; three new SoA arrays ride
alongside it: `cmode` (int8, 0=static/1=velocity/2=age/3=index),
`color_u` (float, spawn-time fixed offset), `hue_drift` (float). All
`_append`/`_append_arrays`/`_compact` calls were extended to carry them.
`ParticleSystem._dynamic_colors(t)` resolves the actually-rendered color
each frame: static particles pass their stored color through unchanged
(`np.where(dynamic[:,None], dyn, self.color)`); dynamic ones get a fresh
`palette.lookup(u)`. `ParticleSystem.render()` now takes a `t` parameter
(threaded from `SceneInstance.render` → `ParticleLayer.render` →
`system.step`/`system.render`) to drive `palette_speed`/`hue_drift`.

Deliberate scope cut: one palette per `particles` *layer*, not per
emitter. Multiple emitters in one layer share it (documented above). A
genuinely separate palette per emitter would mean per-particle palette
routing, which is a much bigger SoA change for a use case (mixed-palette
emitters in one physics world) that didn't come up in the brief; multiple
`particles` layers stacked (already fully supported, `add`/`max` blend)
covers it today.

### 4. `"projector"` (top-level, scene-wide)

```json
{
  "name": "comet",
  "layers": [ ... ],
  "projector": {"style": "react", "energy": 0.6, "palette": ["#ffdd00"], "brightness": 0.85}
}
```

Optional, any JSON object — `validate_scene` checks only that it *is* an
object (rejects a string/array/number), never its contents. The engine
never reads it; it round-trips with the scene (`GET /scene` returns it
verbatim) and is forwarded verbatim in every canvas-tap semantic packet
for a consumer like GORMAPPER to act on. Full contract, consumer
vocabulary (`style`/`energy`/`palette`/`brightness`), and the semantic
side-channel it travels over: `docs/TAPS_DESIGN.md`.

## `GET /scene`

`api.py`: mirrors the existing `GET /state` pattern (`_one(node)`,
node-aware in multi-node mode, ring-atomic — `node` accepted and ignored
— in ring mode). Returns `{"scene": <verbatim JSON that was last POSTed
via /scene or /prompt>, "node": "<engine name>"}`. Closes the gap where
an agent could set a scene but never read back exactly what's live
(previously only `GET /state` existed, which reports the scene *name*
plus fps/streaming stats, not the full JSON).

## Perf

All new work is either O(1) extra terms in existing vectorized numpy
expressions (`palette_speed`) or a small circular convolution over an
(n,3) buffer only when `trail.blur` is set (opt-in, same cost class as
the existing `master.blur`). No new per-frame Python-level loops were
added; the particle color resolution (`_dynamic_colors`) is fully
vectorized and short-circuits to a no-op (`return self.color`) whenever
no layer palette is configured or no particle uses a non-static
`color_mode` — i.e. every existing scene takes the exact old code path.
Measured on the Pi: see the deploy report in the conversation for the
achieved fps on `comet-trails.json` at full 1920px ring resolution.

## Showcase scenes

- `scenes/comet-trails.json` — three comets on a closed ring, `physics.wrap`,
  each a separate `particles` layer with `color_mode: "index"` off a
  shared warm→cool palette and a long blurred `trail`, so each comet is
  visibly multi-hued along its own body and leaves a soft fading tail
  that wraps the seam correctly.
- `scenes/aurora-drift.json` — multi-color generative ambient: a
  `gradient` bed and two `noise` layers, both cycling via `palette_speed`
  at different (incommensurate) rates so the whole field's hue slowly,
  continuously reorganizes rather than repeating a fixed loop — no two
  moments look alike over a period of hours.
