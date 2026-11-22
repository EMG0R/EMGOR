# neuralGrid — CONFIG SCHEMA (Phase 3: the config boundary)

This is the schema-versioned parameter vector that separates the LIVE LAYER
(Phase 2, real-time, sacred) from the MODEL LAYER (Phase 4+, async). The
live layer only ever *reads* an active config to decide what its controls
do right now; the model layer (or, for now, a human/manual stager) *writes*
a new config and stages it; the live layer atomically swaps staged -> active
**on the downbeat**, never mid-frame, never blocking the hot loop.

Implementation: `pi/neuralgrid/config.py`. That module is the single
source of truth — this document describes it, but the code is normative if
they ever disagree.

## Header

Every config exchange (dict or vector) carries a header so a receiver can
tell a schema mismatch apart from a corrupt/garbage payload before doing
anything else:

```
{ "schema_version": 1, "vector_len": 89 }
```

- `schema_version` — bumped whenever the field list or packing order
  changes. A schema bump invalidates old vectors (they must be re-encoded,
  and any model trained against the old vector shape must be retrained).
  Old logs stay usable by tagging them with the schema version they were
  captured under.
- `vector_len` — the fixed length of the float32 parameter vector, `89`
  for schema version 1 (`neuralgrid.config.VECTOR_LEN`). Any staged vector
  whose length doesn't match this is rejected outright — this is the first
  and cheapest validity check, before even inspecting values.

## Config fields (dict form)

| # | field | type | domain |
|---|---|---|---|
| 1 | `active_menu` | enum | 0..3 — 0 Instrument, 1 Sequencer, 2 MIDI-fx, 3 Reserved |
| 2 | `note_layout` | enum | 0 chromatic, 1 rows-in-fourths, 2 scale-degrees, 3 isomorphic |
| 3 | `scale_id` | enum | 0..7 — see Scale table below |
| 4 | `root_note` | enum | 0..11 — pitch class, 0=C |
| 5 | `octave` | enum | 0..7 — octave register |
| 6 | `density` | scalar | `[0,1]` — reserved for sequencer/arp note density (Phase 4+) |
| 7 | `velocity_curve` | enum | 0 fixed, 1 row-scaled, 2 col-scaled, 3 accent |
| 8 | `velocity_fixed` | scalar | `[0,1]` — velocity used by curve 0, and as the ceiling for curves 1-3 |
| 9 | `swing` | scalar | `[0,1]` — reserved for sequencer/clock swing (Phase 4+) |
| 10 | `arp_mode` | enum | 0 off, 1 up, 2 down, 3 up-down, 4 random — reserved (Phase 4+ / mode C) |
| 11 | `echo_send` | scalar | `[0,1]` — reserved for MIDI-fx echo send (Phase 4+ / mode C) |
| 12 | `quantize_strength` | scalar | `[0,1]` — reserved for MIDI-fx quantize (Phase 4+ / mode C) |
| 13 | `led_palette` | enum | 0..5 — see Palette table below |
| 14 | `led_brightness` | scalar | `[0,1]` — global brightness multiplier applied on top of the palette |
| 15 | `channel` | enum | 0..15 — MIDI channel used by the instrument menu |
| 16 | `softpot_fn[4]` | enum×4 | one per softpot (N,S,E,W); 0 menu_select, 1 cc, 2 pitch_bend, 3 none — reserved (Phase 4+) |

Plus `schema_version` (int) carried alongside the field values in dict form
(not part of the float vector body — see Packing below).

### Scale table (`scale_id`)

| id | name | intervals (semitones from root) |
|---|---|---|
| 0 | chromatic | 0,1,2,3,4,5,6,7,8,9,10,11 |
| 1 | major | 0,2,4,5,7,9,11 |
| 2 | natural_minor | 0,2,3,5,7,8,10 |
| 3 | dorian | 0,2,3,5,7,9,10 |
| 4 | mixolydian | 0,2,4,5,7,9,10 |
| 5 | major_pentatonic | 0,2,4,7,9 |
| 6 | minor_pentatonic | 0,3,5,7,10 |
| 7 | harmonic_minor | 0,2,3,5,7,8,11 |

### Palette table (`led_palette`)

| id | name |
|---|---|
| 0 | rainbow (hue by row, matches Phase 2's original per-row hue) |
| 1 | mono_blue |
| 2 | fire |
| 3 | ice |
| 4 | purple |
| 5 | grayscale |

## Packing order (dict -> float32 vector)

`to_vector()` walks the fields **in the exact order of the table above**
and packs each one as either a one-hot block (enums) or a single
passthrough scalar clamped to `[0,1]` (scalars). This order is fixed by
schema version; changing it is a schema bump.

| offset | width | field | encoding |
|---|---|---|---|
| 0 | 4 | `active_menu` | one-hot |
| 4 | 4 | `note_layout` | one-hot |
| 8 | 8 | `scale_id` | one-hot |
| 16 | 12 | `root_note` | one-hot |
| 28 | 8 | `octave` | one-hot |
| 36 | 1 | `density` | scalar |
| 37 | 4 | `velocity_curve` | one-hot |
| 41 | 1 | `velocity_fixed` | scalar |
| 42 | 1 | `swing` | scalar |
| 43 | 5 | `arp_mode` | one-hot |
| 48 | 1 | `echo_send` | scalar |
| 49 | 1 | `quantize_strength` | scalar |
| 50 | 6 | `led_palette` | one-hot |
| 56 | 1 | `led_brightness` | scalar |
| 57 | 16 | `channel` | one-hot |
| 73 | 4 | `softpot_fn[0]` (N) | one-hot |
| 77 | 4 | `softpot_fn[1]` (S) | one-hot |
| 81 | 4 | `softpot_fn[2]` (E) | one-hot |
| 85 | 4 | `softpot_fn[3]` (W) | one-hot |

Total width = **89** = `VECTOR_LEN`.

One-hot decode (`from_vector`) takes `argmax` of each block — this makes
the codec robust to a model that outputs soft/continuous logits instead of
a hard one-hot (as Interface-VAE, Phase 4, will), without any extra
"softmax-then-round" step at the boundary.

`schema_version` is **not** encoded in the 89-float body; it travels
alongside the vector as part of the header (see above) and is what
`stage_config()` checks before even looking at vector contents.

## Codec contract (`neuralgrid/config.py`)

- `VECTOR_LEN` — module constant, `89` for schema version 1.
- `SCHEMA_VERSION` — module constant, `1`.
- `default_config() -> dict` — the config the live layer boots with;
  chosen to reproduce Phase 2's original hard-coded behavior exactly
  (rows-in-fourths layout, fixed velocity 100, channel 0, rainbow palette
  at full brightness) so Phase 3 is a pure superset, not a regression.
- `validate(cfg) -> bool` — raises `ValueError` describing the first
  problem found, or returns `True`. Checks `schema_version`, every enum's
  range, every scalar's `[0,1]` range, and `softpot_fn`'s shape.
- `to_vector(cfg) -> np.ndarray[float32, (VECTOR_LEN,)]` — deterministic,
  validates first.
- `from_vector(vec) -> dict` — raises `ValueError` if `vec` isn't shaped
  `(VECTOR_LEN,)`; otherwise always succeeds (out-of-range floats just
  clamp / argmax to *some* valid index) — a model's raw output is never
  allowed to produce an invalid config.
- No hardware imports — importable and testable on a machine with no
  serial/MIDI backend at all.

## Runtime integration (`neuralgrid/live/engine.py`)

- `LiveEngine.config` — the **active** config dict, read by menus every
  frame/keypress. Starts as `default_config()`.
- `LiveEngine.stage_config(data, source: str) -> bool` — thread-safe.
  `data` may be a config dict or an `(VECTOR_LEN,)` array/vector. Decodes
  + validates immediately (wrong length / bad schema / out-of-range ->
  rejected on the spot, logged as a warning, active config untouched,
  returns `False`). On success the decoded config is placed in a single
  staged slot (last write wins) guarded by a lock, returns `True`.
- On every clock downbeat (`Clock.on_downbeat`, which fires whether the
  pulse came from a real external MIDI clock or the clock's own
  free-running internal fallback — see `clock.py` — so this is covered
  either way with a single hook): the engine atomically takes whatever is
  in the staged slot (if anything) and swaps it in as the new active
  config reference, logging `"config swap on beat N from <source>"`. If a
  staged config also changes `active_menu`, the engine switches menus
  as part of the same swap. The swap is a plain reference assignment
  (`self.config = cfg`), never a lock held across the render/tick path —
  the hot loop is never blocked.
