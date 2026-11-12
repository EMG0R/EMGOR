# Wire Format — v1 (2026-09-03)
Status: FROZEN for v1 once Colton acknowledges (open item #3). Changes bump to v2.
Source plan: docs/superpowers/plans/2026-09-03-septagraph-implementation.md

This document is §B of the implementation plan, lifted out as a standalone
contract. It is the collaborator contract (plan v2 open item #3, "wire
encodings in bytes"). Companion documents: `AXIS_SEMANTICS_v1.md` (§A, what
the 8 values mean) and `WINDOW_MATH_v1.md` (§C, how a controller produces
them). Intent lives in `SEPTAGRAPH_PLAN.txt` §3 ("The protocol").

## §B. Wire Grammar v1

Transport: OSC 1.0 over UDP. Default ports: renderers listen **7700**; controllers listen **7701** (manifests). LAN demo: controllers send to UDP broadcast `255.255.255.255:7700` or to explicit renderer IPs; laptop demo: `127.0.0.1`. Every message address begins `/sept/v1/`.

Type tags: `s` string, `i` int32, `f` float32, `b` blob. Arrays of 8 floats are written as 8 consecutive `f`.

### Session plane — renderer → controller (port 7701)

```
/sept/v1/manifest/begin   s renderer_id  s kind("continuous"|"catalog")  i n_points  s name
/sept/v1/manifest/bounds  s renderer_id  8f lo  8f hi                       # continuous only
/sept/v1/manifest/point   s renderer_id  i point_id  8f mu  8f sigma  s instrument
                          f max_rate_hz  f vel_min  f vel_max               # catalog only, one per point
/sept/v1/manifest/end     s renderer_id
/sept/v1/manifest/request                                                     # controller → 7700, asks all renderers to re-announce
```

Renderers announce on start, on `manifest/request`, and when capability changes. Controllers treat a `begin` as invalidating that `renderer_id`'s previous manifest.

### Slow plane — controller → renderers (port 7700), sent only at gesture boundaries

```
/sept/v1/shape   s shape_id  i layer  f duration_s  i n_knots  [ f t  8f mu  8f sigma ] × n_knots
```

- A **septagraph**. Knots sorted by `t` ascending, `t[0] = 0.0`, `t[-1] = 1.0`, `1 ≤ n_knots ≤ 32`. Renderers interpolate linearly between knots.
- `layer`: 0 = play, 1 = loop. A renderer keeps the latest shape per layer. Default shape when none received: one knot `t=0, mu=K, sigma=0` (i.e. static).
- Shape `mu` is **relative**: at render, the shape is translated so `mu[0] == hit.mu`. Effective `σ_eff[i] = sqrt(hit.sigma[i]² + knot.sigma[i]²)`.

### Fast plane — controller → renderers (port 7700), < 10 ms, no model

```
/sept/v1/hit    s voice_id  i layer  i onset(1|0)  f velocity(0–1)  8f mu  8f sigma
/sept/v1/move   s voice_id  8f mu                                     # voice_id "" = global
```

- `mu` in `/hit` and `/move` is **absolute** (already window-projected by the controller). Renderers never see windows.
- `voice_id` binds release and modulation to the note; controllers guarantee uniqueness per held note (`"<controller_id>:<n>"`).
- `/move` rate 50–100 Hz per active stream; renderers may drop but never queue.

### Renderer obligations (conformance)

1. Parse all five address families; ignore unknown `/sept/v1/*` addresses silently.
2. Clamp; never crash on out-of-range.
3. Honor `onset=0` for a known `voice_id` (release); ignore unknown releases.
4. Announce a manifest on start and on `manifest/request`.
5. A `catalog` renderer, given `sigma > 0`, may choose **any** catalog point whose distance to `mu` is ≤ `sigma` (per axis) — this is orchestration freedom; with `sigma = 0` it picks the nearest.

---

## Encoding rules (OSC 1.0 recap, normative for this spec)

These are the OSC 1.0 rules that every Septagraph implementation (Python
`codec.py`, browser `sept_codec.js`, ChucK `OscIn`) must agree on. They are
restated here so the golden byte vectors in `septagraph/golden/` can be
checked by hand.

- Every message is `address` + `type tag string` + `arguments`, in that order.
- **Strings** (`s`): ASCII/UTF-8 bytes, then a NUL terminator, then NUL
  padding up to the next multiple of 4 bytes. A string whose length is
  already a multiple of 4 still gets 4 NULs (at least one terminator is
  mandatory).
- **Type tag string**: a `,` followed by one character per argument, encoded
  as a string (same NUL + pad rule).
- **int32** (`i`): 4 bytes, big-endian, two's complement.
- **float32** (`f`): 4 bytes, big-endian IEEE 754 single precision. Values
  such as `0.42` are not exactly representable; the nearest float32 is sent
  (`0.42` → `0x3ED70A3D` → `0.41999998688697815`). Decoders must compare with
  a tolerance of about `1e-7` relative, not for exact equality with the
  original double.
- **Arrays of 8 floats** are 8 separate `f` arguments — no array brackets
  (`[` `]`) and no blob. A `/hit` therefore has type tag `,sii` + 17 × `f`.
- **Bundles** (`#bundle` + 64-bit timetag + size-prefixed elements) are used
  only for manifests (one bundle = begin, bounds or points, end); every other
  message is a bare message. Decoders must accept both bare messages and
  bundles on either port.
- Message sizes (measured with python-osc): `/hit` = 124 bytes (6-char
  voice id), `/move` = 64 bytes (empty voice id), `/shape` with a 3-char
  `shape_id` and 2 knots = 208 bytes; a 32-knot shape is ≈ 2.3 kB. All fit in
  a single UDP datagram.

## Worked example — one `/sept/v1/hit`

Values (the Task 1.3 golden fixture `hit_basic.osc`):

```
voice_id = "grid:7"
layer    = 0
onset    = 1
velocity = 0.8
mu       = [0.42, 0.7, 0.9, 0.02, 0.35, 0.5, 0.1, 0.2]
sigma    = [0, 0, 0, 0, 0, 0, 0, 0]
```

Read against `AXIS_SEMANTICS_v1.md`, this hit says: voice `grid:7` starts on
the play layer, fairly loud, at about 361 Hz (`freq` 0.42), level −18 dBFS
(`loud` 0.7), mostly tonal (`harm` 0.9), no temporal modulation (`rate`
0.02 < 0.05), spectral density ≈ 0.84 cyc/oct (`scale` 0.35), dead centre
(`az` 0.5), nearly a point source (`spread` 0.1), fairly dry (`dist` 0.2),
and the renderer has no liberty (all σ = 0).

Layout of the packet:

| offset | bytes | field |
|---|---|---|
| 0x00 | 16 | address `"/sept/v1/hit"` (12 chars + 4 NUL) |
| 0x10 | 24 | type tags `",siifffffffffffffffff"` (21 chars + 3 NUL) |
| 0x28 | 8 | `s` voice_id `"grid:7"` (6 chars + 2 NUL) |
| 0x30 | 4 | `i` layer = 0 |
| 0x34 | 4 | `i` onset = 1 |
| 0x38 | 4 | `f` velocity = 0.8 → `3f 4c cc cd` |
| 0x3c | 32 | 8 × `f` mu |
| 0x5c | 32 | 8 × `f` sigma (all `00 00 00 00`) |
| total | **124** | |

Hex dump (offset, 16 bytes, ASCII):

```
0000  2f 73 65 70 74 2f 76 31 2f 68 69 74 00 00 00 00  |/sept/v1/hit....|
0010  2c 73 69 69 66 66 66 66 66 66 66 66 66 66 66 66  |,siiffffffffffff|
0020  66 66 66 66 66 00 00 00 67 72 69 64 3a 37 00 00  |fffff...grid:7..|
0030  00 00 00 00 00 00 00 01 3f 4c cc cd 3e d7 0a 3d  |........?L..>..=|
0040  3f 33 33 33 3f 66 66 66 3c a3 d7 0a 3e b3 33 33  |?333?fff<...>.33|
0050  3f 00 00 00 3d cc cc cd 3e 4c cc cd 00 00 00 00  |?...=...>L......|
0060  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  |................|
0070  00 00 00 00 00 00 00 00 00 00 00 00              |............|
```

Float32 words, for checking a decoder:

| value | float32 hex | decoded double |
|---|---|---|
| velocity 0.8 | `3f4ccccd` | 0.800000011920929 |
| mu[0] 0.42 | `3ed70a3d` | 0.41999998688697815 |
| mu[1] 0.7 | `3f333333` | 0.699999988079071 |
| mu[2] 0.9 | `3f666666` | 0.8999999761581421 |
| mu[3] 0.02 | `3ca3d70a` | 0.019999999552965164 |
| mu[4] 0.35 | `3eb33333` | 0.3499999940395355 |
| mu[5] 0.5 | `3f000000` | 0.5 |
| mu[6] 0.1 | `3dcccccd` | 0.10000000149011612 |
| mu[7] 0.2 | `3e4ccccd` | 0.20000000298023224 |
| sigma[0..7] 0.0 | `00000000` | 0.0 |

Verification: these 124 bytes were produced by `python-osc` 1.10.2
(`OscMessageBuilder("/sept/v1/hit")` with explicit `s`/`i`/`f` tags) and
independently by a hand-rolled `struct.pack(">i"/">f")` encoder with the
padding rules above; the two byte strings are identical, and
`pythonosc.osc_message.OscMessage(pkt)` decodes back to
`['grid:7', 0, 1, 0.8000000119, 0.4199999869, …, 0.0]`. The corresponding
release is the same packet with `onset = 0` (byte 0x37 becomes `00`) and
whatever velocity/mu the controller last had for that voice.

Note on the type tag: a `/hit` carries **17** floats (velocity + 8 mu + 8
sigma), so the tag string is `,sii` followed by seventeen `f` — 21
characters, padded to 24 bytes. An implementation that emits sixteen `f`s
has dropped an argument and will not match the golden vector.

## Conformance checklist

This is the list `septagraph/py/septagraph/conformance.py` (Task 1.6) tests
against a live renderer at `<host>:7700`, and what a renderer author should
tick off by hand before claiming "speaks Septagraph v1". A renderer is
**conformant** when every item passes.

- [ ] **C1 — Parses all five address families.** `/sept/v1/manifest/*`,
  `/sept/v1/shape`, `/sept/v1/hit`, `/sept/v1/move` are all accepted
  (manifest messages arrive as a bundle; the rest as bare messages).
  An unknown `/sept/v1/*` address (the checker sends `/sept/v1/whatever i 1`)
  is ignored **silently** — no crash, no reply, no log spam required.
- [ ] **C2 — Clamps, never crashes on out-of-range.** A `/hit` with
  `mu = [2.0] × 8` (and, separately, negative values and σ > 1) is realized as
  if clamped to [0, 1]. The renderer's UDP socket stays alive afterwards
  (the checker verifies this by getting an answer to a follow-up
  `manifest/request`).
- [ ] **C3 — Honors release for known voices; ignores unknown releases.**
  `/hit onset=1` for `voice_id = "x:1"` then `/hit onset=0` for `"x:1"` stops
  that voice. `/hit onset=0` for a `voice_id` never seen is a no-op.
- [ ] **C4 — Announces a manifest on start and on request.** Within the
  checker's timeout (default 3 s), a complete manifest — `begin`, then
  `bounds` (continuous) or `n_points` × `point` (catalog), then `end`, all
  carrying the same `renderer_id` — arrives at the requester's port 7701
  after a `manifest/request`. `n_points` in `begin` matches the number of
  `point` messages (0 for continuous).
- [ ] **C5 — Catalog renderers respect σ as orchestration freedom.** For
  `kind = "catalog"` only: with `sigma = 0` the renderer picks the catalog
  point nearest to `mu`; with `sigma > 0` it picks *some* point whose
  per-index distance `|point.mu[i] − mu[i]| ≤ sigma[i]` for all `i` (any
  such point is legal — this is the renderer's freedom). If no point is
  within σ the renderer falls back to nearest. (Automated checking of C5
  requires audio or a side channel; `conformance.py` v1 reports it as
  "manual".)

Additional fast-plane expectations that are not pass/fail but are measured
and reported by the checker where possible:

- `/move` handling: renderers **may drop** `/move` packets when busy but
  **never queue** them (a late `/move` must not be applied after a newer
  one).
- Latency budget: controller-side `< 10 ms` from gesture to packet; the
  renderer's own budget is declared in its README and measured by
  `latency_probe.py` (Task 3.4).

## Decisions vs plan v2

`SEPTAGRAPH_PLAN.txt` (plan v2, 2026-08-29) left some of the wire details
open or implied them differently. This document, via the implementation
plan, decides them as follows. Flagged here so the collaborator contract is
explicit about where it departs from the earlier intent document.

1. **Absolute coordinates on the wire; windows never on the wire.** Plan v2
   §3 says `/hit` carries "window-local coords" and `/shape` carries
   "septagraphs + window configs". v1 instead has the controller apply the
   window (`WINDOW_MATH_v1.md`) *before* sending, so `/hit` and `/move`
   carry absolute 0–1 coordinates and `/shape` carries only a septagraph.
   Renderers never learn that windows exist. This is simpler and a stronger
   separation: a renderer needs no controller state to interpret any packet.
2. **`K = 8`.** The `space` axis is three wire values (`az`, `spread`,
   `dist`), so the seven conceptual axes are eight floats on the wire, and
   `time` is the knot parameter `t` rather than a coordinate. Every `8f`
   in the grammar is these eight values in the order fixed by
   `AXIS_SEMANTICS_v1.md`.
3. **Shape `mu` is relative to the hit.** A `/shape` knot list is a
   trajectory *shape*; at render it is translated so its first knot
   coincides with the triggering hit's `mu`, and σ combines in quadrature
   (`σ_eff = sqrt(hit.σ² + knot.σ²)`). Plan v2 did not say whether shapes
   were absolute or relative.
4. **The Machine Lab renderer ignores `/move` in v1.** Robots have no
   continuous per-note control that maps onto the coordinate axes, so the
   Lab renderer (`lab_render.py`) acts on `/hit` only and silently drops
   `/move`. This is conformant (C1 — parsed and ignored) and is the
   expected behaviour of any renderer without continuous control, declared
   via its catalog σ.

(For completeness, the implementation plan's self-review also records a
controller-side decision that does not affect the wire: the NeoTrellis has
no velocity sensing, so the neuralGrid sends a fixed `velocity = 0.7` and
uses the E softpot as a loudness modulator instead.)
