# Window Math — v1 (2026-09-03)
Status: FROZEN for v1 once Colton acknowledges (open item #3). Changes bump to v2.
Source plan: docs/superpowers/plans/2026-09-03-septagraph-implementation.md

This document is §C of the implementation plan, lifted out as a standalone
contract. It is controller-side only — nothing here ever appears on the
wire (see `WIRE_FORMAT_v1.md` §B, "Decisions vs plan v2" item 1). Companion
documents: `AXIS_SEMANTICS_v1.md` (§A, what the 8 output values mean) and
`WIRE_FORMAT_v1.md` (§B, how the resulting coordinates travel). Intent
lives in `SEPTAGRAPH_PLAN.txt` §4 ("The window").

## §C. Window Math v1

A **window** is axis-aligned: `center[8]`, `extent[8]` (half-widths, ≥ 0), and an **assignment** mapping controller channels → coordinate indices.

Controller channel model (from the input manifest): each channel `c` has `dims ∈ {1,2,3}`, `kind ∈ {"discrete","continuous"}`, and for discrete channels a `size` per dim (e.g. grid `[8,8]`). A gesture supplies a **device coordinate** `d ∈ [0,1]^dims` (discrete: `d = (i + 0.5) / size`).

**Projection** for assigned index `a = assign[c][dim]`:

```
mu[a] = clamp(center[a] + (2*d[dim] - 1) * extent[a], 0, 1)
```

Unassigned indices take `mu[a] = center[a]`. **Velocity** maps to `loud` unless the window assigns `loud` elsewhere: `mu[1] = clamp(center[1] - extent[1] + 2*extent[1]*velocity, 0, 1)`.

**Modulators**: `mu[a] += delta * extent[a]`, then clamp (`delta ∈ [−1,1]`).

**Snapping** (when a catalog manifest is merged): for discrete channels, each cell's projected `mu` is replaced by the nearest catalog point if `dist_inf(mu, point.mu) ≤ snap_radius` (default 0.06; `dist_inf` = max over indices of |Δ|). Result is precomputed per window as a **cell table** so `/hit` is a lookup, not a search.

**Density field** (for gravity, later): sum of per-point Gaussians with `σ = point.sigma + 0.02`, evaluated on demand; not on the hot path.

## Notes and clarifications (non-normative)

These expand on the formulas; they do not change any equation above.

### Where the window lives, and what it never does

A window is pure controller-side state: `center[8]`, `extent[8]`, and the
assignment map from device channels to coordinate indices. It is produced
and updated by the controller (manually via the web UI in early
milestones, later by the deaf sequence model between gestures — see
`SEPTAGRAPH_PLAN.txt` §4–5). It is never serialized onto the wire; renderers
have no notion of a window and cannot reconstruct one from `/hit` or
`/move` traffic. This is the wire-side decision recorded in
`WIRE_FORMAT_v1.md`'s "Decisions vs plan v2" #1 — the window is applied
*before* a message is built, so every outgoing coordinate is already
absolute.

### `center` and `extent` versus `μ` and `σ`

Do not confuse the four per-axis quantities in play:

- `center[8]`, `extent[8]` — the **window**: where a controller's playable
  region currently sits in the 8D space, and how wide it is per axis. Both
  are controller config, never transmitted.
- `μ[8]` — the **output** of the projection: one point inside the window,
  computed from a specific gesture. This is what goes out in `/hit` and
  `/move`.
- `σ[8]` — **renderer liberty** (`AXIS_SEMANTICS_v1.md`), attached to a
  `/hit` or a shape knot, describing how loosely the renderer may realize
  `μ`. It has no relationship to `extent`; a wide window (`extent` near
  0.5) and a precise hit (`σ = 0`) coexist without conflict — the window
  says "where gestures can land", σ says "how exactly this one lands".

### Two layers, not one window

Per `SEPTAGRAPH_PLAN.txt` §4, a config holds exactly two windows in v1:
`layer 0` (play) and `layer 1` (loop) — the same `layer` field used in
`/shape` and `/hit` (`WIRE_FORMAT_v1.md` §B). The loop layer's window
defaults to **frozen** so a captured loop keeps its sound while the play
window continues to travel; unfreezing the loop window is an explicit
control, not automatic. The window math above (`§C`) is identical for both
layers — layer only selects *which* `center`/`extent`/`assignment` a given
gesture projects through.

### Discrete device coordinates

For a discrete channel (e.g. the neuralGrid's 8×8 pad matrix), the device
coordinate is the cell center, not an edge or a random offset within the
cell: `d = (i + 0.5) / size` for cell index `i` along that dimension. An
8-wide axis therefore produces device coordinates `{0.0625, 0.1875, …,
0.9375}` — evenly spaced, none at exactly 0 or 1. This is what makes the
cell table in the snapping step well-defined: every discrete gesture maps
to exactly one of `size[0] × size[1] × …` fixed device coordinates, so the
projected `mu` per cell can be precomputed once per window placement.

### Velocity as a special case, not a channel

Velocity does not come from a device channel in the assignment map; it is
a scalar attached to the gesture itself (from `NoteOn`-style controllers)
and defaults onto `loud` (index 1) using the *same* center/extent
projection form as any other axis, just with `d` replaced by `velocity`
directly (no `2*d-1` recentering, since velocity is already a 0–1 quantity
representing "how hard", not a bipolar position):

```
mu[1] = clamp(center[1] - extent[1] + 2*extent[1]*velocity, 0, 1)
```

If a window's assignment map explicitly places another channel on index 1
(`loud`), that assignment wins and velocity is not applied to `loud`; per
the neuralGrid decision recorded in `WIRE_FORMAT_v1.md` §B ("Decisions vs
plan v2", trailing note), the NeoTrellis has no velocity sensing, so the
neuralGrid controller sends a fixed `velocity = 0.7` and uses the E
softpot as an explicit loudness modulator instead — i.e. it assigns `loud`
via a modulator channel rather than relying on the velocity default.

### Modulators recap

A modulator (softpot slide, lidar height) is a controller-side bipolar
delta `delta ∈ [−1, 1]`, read continuously and applied to whichever axis
it is assigned to: `mu[a] += delta * extent[a]`, then clamp to [0, 1].
This is the same mechanism `AXIS_SEMANTICS_v1.md` describes as "modulators
are pre-wire" — nothing bipolar is ever transmitted; the result of this
addition is an ordinary absolute coordinate that goes out via `/move`
(held note) or is folded into the next `/hit`.

### Snapping is a controller optimization, not a renderer behavior

Snapping (nearest catalog point within `snap_radius`, default 0.06,
`dist_inf`) happens **before** the wire, inside the controller, once a
catalog manifest has been merged into the active window. It is distinct
from the renderer-side freedom in `WIRE_FORMAT_v1.md` C5 (a catalog
renderer picking among points within σ of an *unsnapped* `mu`) — a
controller that snaps sends an already-precise `mu` (typically with
`σ = 0`, since it has picked an exact reachable point), while a controller
that does not snap leaves the choice to the renderer via σ. Both are
legal; v1 controllers snap for discrete channels because the cell table
makes it a lookup, not a search, keeping the fast plane's < 10 ms budget
(`SEPTAGRAPH_PLAN.txt` global constraint, restated in the implementation
plan's Global Constraints).

### Density field is explicitly deferred

The density field (sum of per-point Gaussians, `σ = point.sigma + 0.02`)
is listed for a later "gravity" feature — a soft pull on window placement
toward catalog-dense regions between gestures. It is **not** part of the
per-gesture projection in the hot path; `§C` states it is "evaluated on
demand", and no v1 task in the implementation plan wires it into
`grid_ctl.py` or the model. It is documented here because it uses the same
manifest-merge data as snapping and belongs in the same file.
