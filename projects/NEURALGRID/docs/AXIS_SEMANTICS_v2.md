# Axis Semantics — v2 (2026-10-02)

Supersedes `AXIS_SEMANTICS_v1.md`. One change: **space becomes a point on a
sphere.** Everything else is byte-identical.

## §A. Axis Semantics v2

`AXES = ("freq","loud","harm","rate","scale","az","spread","dist","el")`,
`K = 9`.

| idx | name | 0–1 meaning (perceptually linear) | default physical map |
|---|---|---|---|
| 0 | `freq` | log-frequency | `hz = 20 * 1000^k` |
| 1 | `loud` | perceptual level | `db = -60 + 60*k` dBFS |
| 2 | `harm` | noise → pure tone | harmonic energy fraction `= k` |
| 3 | `rate` | log temporal-modulation frequency | `hz = 0.5 * 128^k`; k < 0.05 ⇒ static |
| 4 | `scale` | log spectral-modulation density | `cyc_per_oct = 0.25 * 32^k` |
| 5 | `az` | azimuth, **full circle** | `deg = -180 + 360*k` (0.5 = front) |
| 6 | `spread` | spatial width | 0 = point source, 1 = fully diffuse |
| 7 | `dist` | distance | 0 = near/dry, 1 = far/wet |
| 8 | `el` | **elevation** | `deg = -90 + 180*k` (0.5 = ear level) |

## What changed and why

v1 could not express a point in 3D. `az` spanned only −90…+90, so the back
half of the room was unreachable, and there was no elevation at all: space
was a flat half-plane. Ambisonics and VBAP are driven from azimuth,
elevation and distance, so a renderer could not be fed a real source
position.

v2 makes `space` four values — `az`, `el`, `dist`, `spread` — which is
spherical position plus source width. Still 7 conceptual axes; 9 wire
values.

## Migration from v1

**`el` is appended as the LAST index, not inserted beside `az`.** Indices
0–7 therefore keep their meaning, and v1 data stays valid for them. This
was chosen over the tidier grouping precisely so existing catalogs did not
have to be reinterpreted.

**`az` values DO change meaning.** The range widened, so the same `k` now
denotes a different angle. To preserve the physical angle:

```
deg   = -90 + 180 * k_v1
k_v2  = (deg + 180) / 360
      = 0.25 + 0.5 * k_v1
```

**`el` default when widening a v1 vector:** `0.5` (ear level) for a
position (`mu` / `center`), `0.0` for an `extent` or `sigma` (elevation not
playable / no spread).

Applied to `renderers/lab_map.json`: 186 points, 372 vectors. v1 copy kept
at `lab_map.json.v1backup`.

## Unchanged from v1

Everything in §A indices 0–4, 6 and 7. `time` is still the trajectory
parameter and still never a coordinate. σ is still renderer liberty, in
axis units, travelling beside μ, distinct from a window's `extent`.
Out-of-range values are still clamped, not dropped. Everything in `K` is
still continuous.

## Done

- `septagraph/axes.py` — `AXES`, `to_physical`, `from_physical`
- `septagraph/types.py` — `ZERO`/`HALF`/`ONE` follow `K` automatically
- all Python coordinate widths made `K`-relative (998 tests pass)
- `renderers/lab_map.json` migrated
- golden wire fixtures regenerated (`--write-golden`)
- `web/js/` — `sept_codec.js`, `viz.js`, `window.js`, `webchuck_render.js`,
  `trackpad_ctl.js`, `webcam_ctl.js`

## Not done

- **`chuck/sept_render_osc.ck`** still assumes 8. It has hand-written
  per-index branches (`else if (n == 8)`), so widening it is a manual pass
  through the renderer, not a constant change. The ChucK renderer will
  reject or misread v2 frames until that is done.
