# Axis Semantics — v1 (2026-09-03)

> **SUPERSEDED by `AXIS_SEMANTICS_v2.md` (2026-10-02).** v2 adds `el`
> (elevation) at index 8 and widens `az` to a full circle, so space is a
> point on a sphere rather than a flat half-plane. `K` is 9. Indices 0–7
> keep their meaning; `az` VALUES need remapping — see v2 §Migration.
Status: FROZEN for v1 once Colton acknowledges (open item #3). Changes bump to v2.
Source plan: docs/superpowers/plans/2026-09-03-septagraph-implementation.md

This document is §A of the implementation plan, lifted out as a standalone
contract. Companion documents: `WIRE_FORMAT_v1.md` (§B, how coordinates
travel) and `WINDOW_MATH_v1.md` (§C, how controllers produce them). Intent and
background live in `SEPTAGRAPH_PLAN.txt` §2 ("The space").

## §A. Axis Semantics v1

The **coordinate vector** `K` has 8 floats, each in [0, 1]. The seven *conceptual* axes remain; axis 6 (space) carries three sub-values on the wire.

| idx | name | 0–1 meaning (perceptually linear) | default physical map (renderer declares its own) |
|---|---|---|---|
| 0 | `freq` | log-frequency | `hz = 20 * 1000^k` (20 Hz → 20 kHz; k=0.5 ≈ 632 Hz) |
| 1 | `loud` | perceptual level | `db = -60 + 60*k` dBFS (k=0 → silence allowed) |
| 2 | `harm` | noise → pure tone | harmonic energy fraction `= k` |
| 3 | `rate` | log temporal-modulation frequency | `hz = 0.5 * 128^k` (0.5 → 64 Hz); k < 0.05 ⇒ "static, no modulation" |
| 4 | `scale` | log spectral-modulation density | `cyc_per_oct = 0.25 * 32^k` (0.25 → 8) |
| 5 | `az` | azimuth | `deg = -90 + 180*k` (0 = left, 0.5 = front, 1 = right) |
| 6 | `spread` | spatial width | 0 = point source, 1 = fully diffuse |
| 7 | `dist` | distance | 0 = near/dry, 1 = far/wet; renderer maps to its room |

- **Time** is not in `K`. A septagraph carries knots at normalized `t ∈ [0,1]` plus one `duration_s` float.
- **σ** is per-index, same units (fraction of axis range). `σ = 0` = exact; `σ = 1` = "anywhere on this axis".
- **Clamping:** renderers clamp `μ ± σ` to [0,1] and MUST NOT fail on out-of-range input (clamp, don't drop).
- **Modulators** (softpots, lidar) are controller-side bipolar deltas in [−1, 1] scaled by the window extent, applied *before* the wire. Nothing bipolar ever travels.
- Axis names are the canonical strings above; `AXES = ("freq","loud","harm","rate","scale","az","spread","dist")`.

## Notes and clarifications (non-normative)

These expand on the table; they do not change any number above.

### Seven axes, eight values

The plan v2 space (`SEPTAGRAPH_PLAN.txt` §2) has seven axes: frequency,
loudness, harmonicity, rate, scale, space, time. On the wire:

- Axis 6, **space**, is not one number. It is decomposed into `az`, `spread`,
  `dist` (indices 5, 6, 7). Hence 7 conceptual axes → 8 wire values. `K = 8`.
- Axis 7, **time**, is the trajectory parameter of a septagraph. It is never a
  coordinate; it is the independent variable `t` of the knot list in `/shape`
  (see `WIRE_FORMAT_v1.md`), normalized to [0, 1] and stretched at render by
  `duration_s`.

Everything in `K` is **continuous**. There are no enumerations, no note
numbers, no instrument indices anywhere in the coordinate. A physical
renderer's catalog is a discrete set of points *inside* this continuous
space; the space itself is never discrete.

### "Perceptually linear" and the default maps

Each index is chosen so that equal steps in `k` are roughly equal perceptual
steps. That is why `freq`, `rate`, and `scale` are logarithmic and `loud` is
in dB. The right-hand column gives a *default* mapping to physical units for
reference and for the reference renderer; it is not a wire concern. A
renderer may declare a different physical map in its manifest (for example a
robot whose reachable pitches occupy only part of the `freq` range simply
places its catalog points where they fall). Nobody translates units on the
wire; everybody speaks 0–1.

Worked values of the defaults:

| idx | k = 0 | k = 0.25 | k = 0.5 | k = 0.75 | k = 1 |
|---|---|---|---|---|---|
| 0 `freq` (Hz) | 20 | ≈ 112.5 | ≈ 632.5 | ≈ 3557 | 20000 |
| 1 `loud` (dBFS) | −60 | −45 | −30 | −15 | 0 |
| 3 `rate` (Hz) | 0.5 | ≈ 1.68 | ≈ 5.66 | ≈ 19.0 | 64 |
| 4 `scale` (cyc/oct) | 0.25 | ≈ 0.59 | ≈ 1.41 | ≈ 3.36 | 8 |
| 5 `az` (deg) | −90 | −45 | 0 | 45 | 90 |

(Inverse maps: `k = log_1000(hz/20)`, `k = (db+60)/60`, `k = log_128(hz/0.5)`,
`k = log_32(cpo/0.25)`, `k = (deg+90)/180`; results clamped to [0, 1].)

### `rate` dead zone

`rate` below `k = 0.05` (≈ 0.64 Hz on the default map) means "no
modulation" rather than "very slow modulation". Renderers set modulation
depth to zero in that region so a static sound is reachable without having to
express it as an infinitely slow LFO.

### σ (spread) versus extent

σ is **renderer liberty**: how loosely a requested point may be realized on
each index. It is in the same units as the coordinate (fraction of the axis
range) and travels on the wire alongside μ. It is distinct from a window's
**extent**, which is the controller's playable range and never travels (see
`WINDOW_MATH_v1.md`). A catalog renderer given σ > 0 may pick any point
within σ per index; given σ = 0 it picks the nearest.

### Clamping, not dropping

Any renderer receiving a value outside [0, 1] on any index (μ, σ, or
μ ± σ) clamps it and continues. Dropping the message or crashing is a
conformance failure. This keeps the fast plane robust to controller bugs and
float rounding.

### Modulators are pre-wire

Softpots, lidar height, and similar continuous controls are bipolar
deltas in [−1, 1] applied inside the controller: `mu[a] += delta * extent[a]`,
then clamp. The result is an ordinary absolute 0–1 coordinate that goes out
in `/hit` or `/move`. Renderers never see a delta.
