# Septagraph Implementation Plan (v1, integrated)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship a working Septagraph protocol stack — spec, Python core, zero-install WebChucK controller/renderer pair, a Machine Lab renderer, and the neuralGrid as a controller — so that the same OSC stream drives interchangeable sound engines, producing the evidence the NIME paper needs.

**Architecture:** Septagraph is an OSC *sublanguage* (namespace + argument grammar) on three planes: session (`/manifest`), slow (`/shape`), fast (`/hit`, `/move`). Controllers turn device gestures into positions in an 8-value perceptual coordinate vector via a deterministic window; renderers turn coordinates + septagraphs (μ(t), σ(t) trajectories) into sound. The two sides never share anything else. One Python core library (`septagraph`) implements the grammar, coordinate math, window projection, and manifest merging; a browser bundle re-implements only the encoder/decoder so the web pair speaks true OSC bytes.

**Tech Stack:** Python 3.11+ (Pi) / 3.14 (Mac dev), `python-osc`, `numpy`, `pytest`; ChucK 1.5.x (native) + WebChucK (browser, jsDelivr); vanilla JS + `osc.js` (browser OSC encoder) + MediaPipe Tasks Vision (webcam hand tracking, optional); UDP/OSC as transport everywhere; existing ESP32↔Pi COBS link untouched.

**Spec:** `neuralGrid/docs/SEPTAGRAPH_PLAN.txt` (intent, v2). This plan **embeds the numeric spec** (§A Axis Semantics, §B Wire Grammar, §C Window Math) because those were open items; Phase 0 writes them to `docs/` as standalone files and they become the contract with collaborators.

**Paper:** `neuralGrid/paper/septagraph.tex`. Phase 5 tasks feed it.

## Global Constraints

- **Additive-only in `_EMGOR_SYNTH`.** Never move, rename, or delete existing files. New work goes under `neuralGrid/septagraph/` (new directory) and `neuralGrid/docs/`. Git lock-in commit before anything risky.
- **Machine Lab is read-only.** Never SSH-modify `~/git/machineLabCode`, never restart lab processes, never reflash any Teensy/microcontroller. The Lab renderer only *sends OSC to port 8001* on the MTIID Mac mini, exactly like a composer would. Experiments on the mini live only in `~/Desktop/GOR_STYLE/`.
- **The fast plane never has a model in it.** `/hit` and `/move` paths: no inference, no blocking I/O, target < 10 ms controller-side.
- **All coordinates on the wire are 0–1 floats**, perceptually linear per axis (§A). Renderers own the map to physical units and declare it in their manifest.
- **ChucK is not used on the neuralGrid Pi for performance logic.** ChucK = web reference renderer (+ optionally the same file on a renderer Pi as a stopgap). neuralGrid controller logic is Python.
- **Existing ESP32 firmware and `PROTOCOL.md` (COBS link) are frozen.** The controller consumes `LidarGridLink` as-is.
- **Version tag `v1` in every OSC address.** Axis additions bump to `v2`; v1 messages stay parseable.
- **No dependencies beyond:** `python-osc`, `numpy`, `pytest`, `pyserial` (already), and for the web: `webchuck`, `osc` (osc.js), `@mediapipe/tasks-vision` via jsDelivr only.
- **Commit after every task**, message prefix `septagraph:`.
- Process rules from `HANDOFF.md §9` apply: delegate implementation to subagents; keep the Lab as-is; power off ESP32 before touching wiring.

---

## Glossary (read first — these are the confusions that recur)

- **Axis** — one of the 7 perceptual dimensions. Every axis is **continuous**, a float in [0,1]. Nothing about the axes is discrete.
- **Coordinate / position (`K`, 8 floats)** — one point in the space. `space` contributes 3 floats (az, spread, dist), hence 8 values for 7 axes.
- **Septagraph** — NOT a point. A trajectory of distributions μ(t), σ(t) over the coordinate along normalized time, plus a duration. A sound *is* a septagraph; a hit *places* one.
- **Device space** — a controller's own geometry (neuralGrid: 8×8 pads = 2 discrete dims, lidar height = 1 continuous dim → 3 spatial dims; 4 ribbons = 4×1D continuous). Device space is NOT the 7-axis space.
- **Window** — the deterministic projection from device space into the 7-axis space (x→freq, y→scale, height→spread, ribbons→rate/loud/dist/harm by default). Lives in the controller only; never on the wire.
- **Catalog** — a physical renderer's set of *reachable positions* (each robot beater = one fixed point, with σ from measurement). The catalog is discrete; the space it sits in is continuous. Synths declare a continuous *bounds* instead.
- **Manifest** — how a renderer tells controllers what it can reach (catalog or bounds). Direction: renderer → controller only. Renderers never talk to each other; a synth in HYBRID mode *listens to* the Lab's manifest so it can defer near robot points.
- **`/hit`** — discrete event: voice `v` starts (onset=1) or stops (onset=0) at position μ with spread σ and velocity. MIDI note-on/off analogue, but a position instead of a note number.
- **`/move`** — continuous stream (50–100 Hz) updating the position of a *held* voice (ribbon slide while a pad is held) or a global value (lidar height). MIDI aftertouch/pitch-bend analogue, in coordinate space. Without it a held note would be frozen at its onset position.
- **`/shape`** — a septagraph + which layer it belongs to; sent only at gesture boundaries. Says *how sounds placed by hits evolve over time*.
- **Unified 0–1 rule** — every controller and renderer speaks the same 0–1 coordinates. A renderer that cannot realize an axis (robots have no `rate` control) ignores it and says so via σ=1 / catalog geometry in its manifest. Nobody translates units on the wire.

## Phase map & readiness

| Phase | What | Ready now? | Paper evidence produced |
|---|---|---|---|
| 0 | Numeric spec docs (§A/§B/§C) | **Yes** — decisions made in this plan | Protocol section, Space section |
| 1 | Python core `septagraph` (types, codec, window, manifest) + tests | Yes after Phase 0 | — (foundation) |
| 2 | WebChucK laptop pair (trackpad controller, reference renderer, bridge) | Yes after Phase 1 | Reviewer-runnable artifact; interop matrix column 1 |
| 3 | Machine Lab renderer (translator → :8001) + neuralGrid controller | Yes after Phase 1 (Lab arg format read from `GOR_STYLE/docs/INSTRUMENT_COMMS.md` first) | The prime demo; latency table; matrix column 2 |
| 4 | Catalog measurement → Lab manifest; snap/residual; webcam 3D controller; **listening experiment** | After Phase 3 | Catalog figure; synergy demo; 3 controller geometries; steerability + cross-renderer identity results |
| 5 | Paper: related work, reframed intro, origin section, latency table, matrix figure | Related work + intro **now**; tables after 3–4 | Submission |
| 6 (separate plan) | Neural renderer Pi (7-axis-conditioned), Colton integration, boundary detector + MDN model | **Not ready** — research items; own plan after Phase 3 | Matrix column 3; "deaf controller" section |

Phase 6 is deliberately excluded: neural conditioning on rate/scale and the predictive model need design work that depends on the catalog and on logged play. Writing TDD steps for them now would be fiction.

---

## §A. Axis Semantics v1 (embedded spec)

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

## §B. Wire Grammar v1 (embedded spec)

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

## §C. Window Math v1 (embedded spec)

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

---

## File Structure

```
neuralGrid/
  docs/
    AXIS_SEMANTICS_v1.md          # §A as standalone (Phase 0)
    WIRE_FORMAT_v1.md             # §B as standalone (Phase 0)
    WINDOW_MATH_v1.md             # §C as standalone (Phase 0)
    superpowers/plans/2026-09-03-septagraph-implementation.md   # this file
  septagraph/                     # NEW — all new code lives here
    README.md
    py/
      pyproject.toml
      septagraph/
        __init__.py
        axes.py                   # AXES, physical maps, clamp
        types.py                  # Coord, Knot, Septagraph, Hit, Move, Manifest, CatalogPoint
        codec.py                  # OSC encode/decode for every message (python-osc)
        window.py                 # Window, projection, cell table, snapping
        manifest.py               # ManifestStore, merge, nearest, within_sigma
        net.py                    # UDP send/recv threads, broadcast, ports
        conformance.py            # renderer conformance checker (sends fixed stream, checks manifest)
      tests/
        test_axes.py test_codec.py test_window.py test_manifest.py test_net.py
      controllers/
        trackpad_ctl.py           # laptop controller using pygame/mouse (fallback when browser not wanted)
        grid_ctl.py               # neuralGrid controller: LidarGridLink -> window -> /hit,/move
        input_manifests/neuralgrid.json  input_manifests/trackpad.json  input_manifests/webcam.json
      renderers/
        lab_render.py             # Machine Lab renderer: /hit -> OSC :8001
        lab_map.json              # catalog v0 (hand-authored) then v1 (measured)
      tools/
        sept_bridge.py            # WebSocket <-> UDP relay for browser controllers
        catalog_measure.py        # Phase 4: trigger each lab note, record, analyze -> lab_map.json
        latency_probe.py          # Phase 3: timestamps hit->sound for the latency table
    chuck/
      sept_render_core.ck         # DSP core (shared web/native) — reads globals
      sept_render_osc.ck          # native shim: OscIn -> globals
      sept_manifest.ck            # announces continuous manifest (native)
    web/
      index.html                  # one page: controller dropdown (trackpad|webcam) + WebChucK renderer
      js/sept_codec.js            # OSC encode/decode via osc.js, same grammar as py/codec.py
      js/window.js                # port of §C projection (trackpad/webcam -> mu)
      js/trackpad_ctl.js
      js/webcam_ctl.js            # MediaPipe HandLandmarker -> /move only
      js/webchuck_render.js       # decodes OSC bytes -> Chuck.setFloat / broadcastEvent
      js/bridge_client.js         # optional WebSocket to sept_bridge.py
      test/codec.test.html        # browser test page: golden vectors from py/tests/golden/*.osc
    golden/                       # byte-exact OSC packets shared by py and js tests
```

---

# Phase 0 — Spec documents

### Task 0.1: Write `AXIS_SEMANTICS_v1.md`, `WIRE_FORMAT_v1.md`, `WINDOW_MATH_v1.md`

**Files:**
- Create: `neuralGrid/docs/AXIS_SEMANTICS_v1.md`
- Create: `neuralGrid/docs/WIRE_FORMAT_v1.md`
- Create: `neuralGrid/docs/WINDOW_MATH_v1.md`

**Interfaces:** Produces the three contracts every later task cites. Content = §A, §B, §C above, verbatim, each with a header block:

```markdown
# <Title> — v1 (2026-09-03)
Status: FROZEN for v1 once Colton acknowledges (open item #3). Changes bump to v2.
Source plan: docs/superpowers/plans/2026-09-03-septagraph-implementation.md
```

- [x] **Step 1:** Create the three files by copying §A, §B, §C from this plan under the header block. Add to `WIRE_FORMAT_v1.md` a worked example section showing one full `/sept/v1/hit` message with concrete values: `voice_id="grid:7" layer=0 onset=1 velocity=0.8 mu=[0.42,0.7,0.9,0.02,0.35,0.5,0.1,0.2] sigma=[0]*8`.
- [x] **Step 2:** Add a "Conformance checklist" section to `WIRE_FORMAT_v1.md` (the 5 obligations in §B) — this is what `conformance.py` will test.
- [x] **Step 3:** Update `neuralGrid/HANDOFF.md §0` list: append the three docs and this plan (additive edit only — append lines, don't rewrite).
- [x] **Step 4:** Commit.

```bash
git add neuralGrid/docs/AXIS_SEMANTICS_v1.md neuralGrid/docs/WIRE_FORMAT_v1.md neuralGrid/docs/WINDOW_MATH_v1.md neuralGrid/HANDOFF.md neuralGrid/docs/superpowers/plans/2026-09-03-septagraph-implementation.md
git commit -m "septagraph: v1 numeric spec (axis semantics, wire grammar, window math) + integrated implementation plan"
```

---

# Phase 1 — Python core

### Task 1.1: Package skeleton + axes

**Files:**
- Create: `neuralGrid/septagraph/py/pyproject.toml`
- Create: `neuralGrid/septagraph/py/septagraph/__init__.py`
- Create: `neuralGrid/septagraph/py/septagraph/axes.py`
- Test: `neuralGrid/septagraph/py/tests/test_axes.py`

**Interfaces — Produces:**
```python
AXES: tuple[str, ...]  # ("freq","loud","harm","rate","scale","az","spread","dist")
K = 8
def clamp01(x: float) -> float
def to_physical(idx: int, k: float) -> float     # default maps from §A
def from_physical(idx: int, v: float) -> float   # inverse, clamped
```

- [ ] **Step 1:** `pyproject.toml`:

```toml
[project]
name = "septagraph"
version = "0.1.0"
requires-python = ">=3.11"
dependencies = ["python-osc>=1.8", "numpy>=1.24"]
[project.optional-dependencies]
dev = ["pytest>=8"]
[tool.pytest.ini_options]
testpaths = ["tests"]
```

- [ ] **Step 2:** Failing test `tests/test_axes.py`:

```python
import math, pytest
from septagraph.axes import AXES, K, clamp01, to_physical, from_physical

def test_axes_names_and_count():
    assert AXES == ("freq","loud","harm","rate","scale","az","spread","dist")
    assert K == 8

def test_clamp():
    assert clamp01(-0.1) == 0.0 and clamp01(1.7) == 1.0 and clamp01(0.3) == 0.3

def test_freq_map_endpoints_and_mid():
    assert to_physical(0, 0.0) == pytest.approx(20.0)
    assert to_physical(0, 1.0) == pytest.approx(20000.0)
    assert to_physical(0, 0.5) == pytest.approx(20*math.sqrt(1000), rel=1e-6)

def test_loud_rate_scale_az():
    assert to_physical(1, 1.0) == pytest.approx(0.0)      # dBFS
    assert to_physical(1, 0.0) == pytest.approx(-60.0)
    assert to_physical(3, 0.0) == pytest.approx(0.5)      # Hz
    assert to_physical(3, 1.0) == pytest.approx(64.0)
    assert to_physical(4, 1.0) == pytest.approx(8.0)      # cyc/oct
    assert to_physical(5, 0.5) == pytest.approx(0.0)      # deg
    assert to_physical(5, 0.0) == pytest.approx(-90.0)

@pytest.mark.parametrize("idx", range(8))
@pytest.mark.parametrize("k", [0.0, 0.25, 0.5, 0.9, 1.0])
def test_roundtrip(idx, k):
    assert from_physical(idx, to_physical(idx, k)) == pytest.approx(k, abs=1e-6)
```

- [ ] **Step 3:** Run `cd neuralGrid/septagraph/py && pip install -e ".[dev]" && pytest tests/test_axes.py -v` → FAIL (module missing).
- [ ] **Step 4:** Implement `axes.py`:

```python
import math
AXES = ("freq","loud","harm","rate","scale","az","spread","dist")
K = len(AXES)

def clamp01(x: float) -> float:
    return 0.0 if x < 0.0 else 1.0 if x > 1.0 else float(x)

def to_physical(idx: int, k: float) -> float:
    k = clamp01(k)
    if idx == 0: return 20.0 * (1000.0 ** k)
    if idx == 1: return -60.0 + 60.0 * k
    if idx == 3: return 0.5 * (128.0 ** k)
    if idx == 4: return 0.25 * (32.0 ** k)
    if idx == 5: return -90.0 + 180.0 * k
    return k  # harm, spread, dist are already the physical fraction

def from_physical(idx: int, v: float) -> float:
    if idx == 0: return clamp01(math.log(v / 20.0, 1000.0))
    if idx == 1: return clamp01((v + 60.0) / 60.0)
    if idx == 3: return clamp01(math.log(v / 0.5, 128.0))
    if idx == 4: return clamp01(math.log(v / 0.25, 32.0))
    if idx == 5: return clamp01((v + 90.0) / 180.0)
    return clamp01(v)
```

`__init__.py`: `from .axes import AXES, K, clamp01, to_physical, from_physical`.

- [ ] **Step 5:** `pytest tests/test_axes.py -v` → PASS.
- [ ] **Step 6:** Commit: `git add neuralGrid/septagraph/py && git commit -m "septagraph: python package skeleton + axis semantics"`.

### Task 1.2: Types

**Files:**
- Create: `septagraph/py/septagraph/types.py`
- Test: `septagraph/py/tests/test_types.py`

**Interfaces — Produces** (all frozen dataclasses, `Vec = tuple[float, ...]` length 8):
```python
@dataclass(frozen=True) class Knot: t: float; mu: Vec; sigma: Vec
@dataclass(frozen=True) class Septagraph: shape_id: str; layer: int; duration_s: float; knots: tuple[Knot, ...]
    def at(self, t: float) -> tuple[Vec, Vec]      # linear interp of (mu, sigma)
@dataclass(frozen=True) class Hit: voice_id: str; layer: int; onset: bool; velocity: float; mu: Vec; sigma: Vec
@dataclass(frozen=True) class Move: voice_id: str; mu: Vec
@dataclass(frozen=True) class CatalogPoint: point_id: int; mu: Vec; sigma: Vec; instrument: str; max_rate_hz: float; vel_min: float; vel_max: float
@dataclass(frozen=True) class Manifest: renderer_id: str; kind: str; name: str; lo: Vec|None; hi: Vec|None; points: tuple[CatalogPoint, ...]
ZERO: Vec; HALF: Vec
def vec(*xs) -> Vec   # validates length 8, clamps
```

- [ ] **Step 1:** Failing test:

```python
import pytest
from septagraph.types import Knot, Septagraph, Hit, vec, ZERO, HALF

def test_vec_validates_and_clamps():
    v = vec(*([1.5] * 8)); assert v == tuple([1.0] * 8)
    with pytest.raises(ValueError): vec(0.1, 0.2)

def test_septagraph_interp_two_knots():
    s = Septagraph("s", 0, 1.0, (Knot(0.0, ZERO, ZERO), Knot(1.0, tuple([1.0]*8), tuple([0.2]*8))))
    mu, sig = s.at(0.25)
    assert mu == pytest.approx(tuple([0.25]*8)); assert sig == pytest.approx(tuple([0.05]*8))
    assert s.at(-1)[0] == ZERO and s.at(2)[0] == tuple([1.0]*8)

def test_septagraph_requires_knot_invariants():
    with pytest.raises(ValueError): Septagraph("s", 0, 1.0, (Knot(0.5, ZERO, ZERO),))            # t0 != 0
    with pytest.raises(ValueError): Septagraph("s", 0, 1.0, (Knot(0.0, ZERO, ZERO), Knot(0.5, ZERO, ZERO)))  # last != 1 with n>1
    Septagraph("s", 0, 1.0, (Knot(0.0, HALF, ZERO),))  # single knot ok

def test_hit_clamps_velocity():
    h = Hit("c:1", 0, True, 1.4, HALF, ZERO); assert h.velocity == 1.0
```

- [ ] **Step 2:** Run → FAIL.
- [ ] **Step 3:** Implement `types.py` with `__post_init__` validation (`object.__setattr__` for clamping in frozen dataclasses); `Septagraph.at` clamps `t` to [0,1], finds the bracketing knots by linear scan (n ≤ 32), interpolates both `mu` and `sigma`. Single-knot: return that knot's values.
- [ ] **Step 4:** Run → PASS. **Step 5:** Commit `septagraph: core types`.

### Task 1.3: OSC codec + golden vectors

**Files:**
- Create: `septagraph/py/septagraph/codec.py`
- Create: `septagraph/golden/hit_basic.osc`, `move_global.osc`, `shape_two_knots.osc`, `manifest_continuous.osc` (bundle), `manifest_catalog_2pts.osc` (bundle) — generated by the test on first run with `--write-golden`, then committed and never regenerated silently.
- Test: `septagraph/py/tests/test_codec.py`

**Interfaces — Produces:**
```python
def encode_hit(h: Hit) -> bytes
def encode_move(m: Move) -> bytes
def encode_shape(s: Septagraph) -> bytes
def encode_manifest(m: Manifest) -> bytes           # one OSC bundle: begin, bounds|points..., end
def encode_manifest_request() -> bytes
def decode(packet: bytes) -> list[Hit | Move | Septagraph | Manifest | ManifestRequest]   # bundles flatten; manifests only complete on 'end'
class Decoder:  # stateful: accumulates manifest begin/point/end across packets
    def feed(self, packet: bytes) -> list[...]
```
Addresses exactly as §B. Use `pythonosc.osc_message_builder.OscMessageBuilder` and `pythonosc.osc_bundle_builder`; decoding via `pythonosc.osc_packet.OscPacket`.

- [ ] **Step 1:** Failing tests:

```python
from septagraph.types import *
from septagraph.codec import *

H = Hit("grid:7", 0, True, 0.8, vec(0.42,0.7,0.9,0.02,0.35,0.5,0.1,0.2), ZERO)

def test_hit_roundtrip_and_address():
    pkt = encode_hit(H)
    assert pkt.startswith(b"/sept/v1/hit\0")
    assert decode(pkt) == [H] or decode(pkt)[0].mu == pytest.approx(H.mu)  # float32 tolerance

def test_move_global():
    m = Move("", HALF); (out,) = decode(encode_move(m)); assert out.voice_id == "" and out.mu == pytest.approx(HALF)

def test_shape_roundtrip():
    s = Septagraph("bow", 0, 2.5, (Knot(0, HALF, ZERO), Knot(1, vec(*[0.6]*8), vec(*[0.1]*8))))
    (out,) = decode(encode_shape(s)); assert out.shape_id == "bow" and len(out.knots) == 2 and out.duration_s == pytest.approx(2.5)

def test_manifest_catalog_needs_end():
    m = Manifest("lab", "catalog", "Machine Lab", None, None,
                 (CatalogPoint(0, HALF, ZERO, "breakBot", 12.0, 0.1, 1.0), CatalogPoint(1, vec(*[0.3]*8), ZERO, "tammy", 8.0, 0.2, 1.0)))
    d = Decoder(); outs = d.feed(encode_manifest(m))
    assert len(outs) == 1 and outs[0].kind == "catalog" and len(outs[0].points) == 2 and outs[0].points[1].instrument == "tammy"

def test_unknown_address_ignored():
    from pythonosc.osc_message_builder import OscMessageBuilder
    b = OscMessageBuilder("/sept/v1/whatever"); b.add_arg(1); assert decode(b.build().dgram) == []

def test_golden_bytes_stable():
    import pathlib
    g = pathlib.Path(__file__).parents[2] / "golden"
    assert encode_hit(H) == (g / "hit_basic.osc").read_bytes()
```

- [ ] **Step 2:** Run → FAIL. **Step 3:** Implement `codec.py`. Golden files: add `tests/conftest.py` option `--write-golden` that writes all five golden packets from fixed fixtures; run once, commit the `.osc` files. **Step 4:** Run → PASS. **Step 5:** Commit `septagraph: OSC codec + golden vectors`.

### Task 1.4: Window projection + cell table

**Files:**
- Create: `septagraph/py/septagraph/window.py`
- Test: `septagraph/py/tests/test_window.py`

**Interfaces — Produces:**
```python
@dataclass class Channel: name: str; dims: int; kind: str; size: tuple[int, ...] | None
@dataclass class InputManifest: controller_id: str; channels: tuple[Channel, ...]
    @staticmethod def load(path) -> InputManifest        # JSON: {"controller_id":..., "channels":[{"name":"grid","dims":2,"kind":"discrete","size":[8,8]}, ...]}
@dataclass class Window: center: Vec; extent: Vec; assign: dict[str, tuple[int, ...]]   # channel name -> coord idx per dim; -1 = unassigned
def project(w: Window, channel: Channel, d: tuple[float, ...], velocity: float | None = None) -> Vec
def modulate(mu: Vec, idx: int, delta: float, w: Window) -> Vec
def cell_table(w: Window, channel: Channel, catalog: Sequence[CatalogPoint] = (), snap_radius: float = 0.06) -> dict[tuple[int, ...], tuple[Vec, CatalogPoint | None]]
```

- [ ] **Step 1:** Failing tests covering: center-only when unassigned; `d=0.5` → center; `d=0/1` → center∓/±extent; clamping; velocity → `loud` unless `loud` assigned; discrete cell `(i+0.5)/size`; `modulate` scales by extent and clamps; `cell_table` snaps a cell within 0.06 (∞-norm) of a point and leaves others unsnapped; table has exactly `size[0]*size[1]` entries.

```python
GRID = Channel("grid", 2, "discrete", (8, 8))
W = Window(center=HALF, extent=vec(0.4,0.3,0,0,0.4,0,0,0), assign={"grid": (0, 4)})

def test_project_center_and_edges():
    assert project(W, GRID, (0.5, 0.5)) == pytest.approx(HALF)
    mu = project(W, GRID, (0.0, 1.0)); assert mu[0] == pytest.approx(0.1) and mu[4] == pytest.approx(0.9)

def test_velocity_to_loud():
    assert project(W, GRID, (0.5,0.5), velocity=1.0)[1] == pytest.approx(0.8)
    assert project(W, GRID, (0.5,0.5), velocity=0.0)[1] == pytest.approx(0.2)

def test_cell_table_snaps():
    p = CatalogPoint(0, project(W, GRID, ((3+0.5)/8, (5+0.5)/8)), ZERO, "tammy", 8, 0, 1)
    tab = cell_table(W, GRID, [p])
    assert len(tab) == 64 and tab[(3,5)][1] is p and tab[(0,0)][1] is None
```

- [ ] **Step 2:** FAIL. **Step 3:** Implement per §C. **Step 4:** PASS. **Step 5:** Commit `septagraph: window projection + cell table`.

### Task 1.5: Manifest store + nearest/within-σ

**Files:**
- Create: `septagraph/py/septagraph/manifest.py`
- Test: `septagraph/py/tests/test_manifest.py`

**Interfaces — Produces:**
```python
class ManifestStore:
    def update(self, m: Manifest) -> None            # replaces by renderer_id
    def catalog(self) -> list[CatalogPoint]           # all points, all catalog renderers
    def nearest(self, mu: Vec, points=None) -> CatalogPoint | None      # Euclidean over 8 idx
    def within_sigma(self, mu: Vec, sigma: Vec, points=None) -> list[CatalogPoint]   # |p.mu[i]-mu[i]| <= sigma[i] ∀i
    def continuous_covers(self, mu: Vec) -> list[str]  # renderer_ids whose lo<=mu<=hi
```

- [ ] **Step 1:** Tests: update replaces; nearest picks the closest of three; `within_sigma` with `sigma=ZERO` returns only exact matches; with `sigma=[0.1]*8` returns both near points; `continuous_covers`. **Step 2:** FAIL. **Step 3:** Implement with numpy arrays rebuilt on `update` (no k-d tree yet — catalog ≤ a few hundred points, brute force is < 50 µs). **Step 4:** PASS. **Step 5:** Commit `septagraph: manifest store`.

### Task 1.6: Net layer + conformance checker

**Files:**
- Create: `septagraph/py/septagraph/net.py`, `septagraph/py/septagraph/conformance.py`
- Test: `septagraph/py/tests/test_net.py`

**Interfaces — Produces:**
```python
class Sender:  # UDP, non-blocking
    def __init__(self, targets: list[tuple[str,int]], broadcast: bool = False)
    def send(self, packet: bytes) -> None
class Receiver:  # thread, decodes with Decoder, calls on_message(obj) on its thread
    def __init__(self, port: int, on_message: Callable[[object], None])
    def start(self) / stop(self)
RENDER_PORT = 7700; CTL_PORT = 7701
def check_renderer(host: str, timeout_s: float = 3.0) -> list[str]   # returns list of failed obligations (empty = conformant)
```
`check_renderer`: sends `manifest/request`, waits for a complete manifest on 7701; sends a hit, a move, a release, an out-of-range hit (`mu=[2.0]*8` bypassing clamp via raw builder), an unknown address; passes if the renderer's manifest arrives and its UDP socket stays alive (a follow-up `manifest/request` is answered).

- [ ] **Step 1:** Test: loopback Receiver on an ephemeral port receives a hit sent by Sender within 100 ms; `check_renderer` against an in-test fake renderer (a Receiver that replies with a manifest) returns `[]`; against a fake that never answers returns `["manifest"]`. **Step 2:** FAIL. **Step 3:** Implement with `socket`/`threading` (`SO_BROADCAST` when `broadcast`). **Step 4:** PASS. **Step 5:** Commit `septagraph: udp net layer + conformance checker`. Add CLI: `python -m septagraph.conformance <host>`.

---

# Phase 2 — WebChucK laptop pair

### Task 2.1: ChucK reference renderer core (native first)

**Files:**
- Create: `septagraph/chuck/sept_render_core.ck`, `septagraph/chuck/sept_render_osc.ck`, `septagraph/chuck/sept_manifest.ck`
- Test: `septagraph/py/tests/test_chuck_renderer.py` (integration, skipped if `chuck` not on PATH)

**Design of the core** (16 voices, one per active `voice_id`, LRU steal):
- Global inputs the shim sets **per voice slot n** (0–15): `float v_mu_n_0..7`, `float v_sig_n_0..7`, `float v_vel_n`, `int v_gate_n`, `Event v_hit_n`. Shape: `float sh_dur`, `int sh_n`, `float sh_t_j`, `sh_mu_j_i`, `sh_sig_j_i` for j < 32 (play layer only in v1; loop layer = second set later).
- Per voice DSP honoring all 8 indices:
  - `freq` → `SawOsc`+`SinOsc` bank fundamental `hz = 20 * Math.pow(1000, k)`.
  - `harm` → crossfade harmonic bank ↔ `Noise` through `BPF` centered at the same hz (`Q` 2).
  - `loud` → gain `Math.pow(10, (-60 + 60k)/20)`.
  - `rate` → LFO `SinOsc` at `0.5*128^k` Hz amplitude-modulating the voice; `k < 0.05` ⇒ LFO depth 0.
  - `scale` → comb `Delay` with feedback tuned so spectral ripple density ≈ `0.25*32^k` cyc/oct: `delay_s = cyc_per_oct / hz` (document the approximation in a comment).
  - `az` → `Pan2.pan = -1 + 2k`; `spread` → mix of dry pan and a `PRCRev`-diffused copy; `dist` → dry/wet into `NRev` + lowpass `LPF` cutoff `20000 * (1 - 0.8k)`.
  - Shape: on hit, spawn a shred that walks knots over `sh_dur` seconds translating `mu` so knot0 == hit mu; σ_eff drawn once per hit: `mu_realized[i] = mu[i] + Std.rand2f(-1,1) * sig_eff[i]` (renderer liberty made audible — document).
  - Release: `v_gate_n = 0` → 80 ms `ADSR` release.
- `sept_render_osc.ck` (native only): `OscIn` on 7700, parses `/sept/v1/hit`, `/move`, `/shape`, `/manifest/request`; maintains `voice_id → slot` map; sets globals; on request sends manifest via `OscOut` to sender IP :7701 (`kind="continuous"`, `lo=[0]*8`, `hi=[1]*8`, `name="sept-render-chuck"`). Announces on start to `127.0.0.1:7701` and broadcast.

- [ ] **Step 1:** Write the integration test: start `chuck --silent sept_render_core.ck sept_render_osc.ck` as a subprocess (skip if no `chuck`), run `check_renderer("127.0.0.1")`, assert `[]`; then send 20 hits at 100 Hz and assert the process is still alive. **Step 2:** Run → FAIL (files don't exist). **Step 3:** Write the three `.ck` files per design. Manual listening check: `chuck sept_render_core.ck sept_render_osc.ck` + `python -m septagraph.tools.sweep` (add a tiny script in `py/septagraph/tools/sweep.py` that sweeps each index 0→1 over 4 s while holding others at 0.5 — this is also the *space validation round-trip*, open item #4). **Step 4:** PASS + you can hear each axis change independently. **Step 5:** Commit `septagraph: chuck reference renderer (native)`.

### Task 2.2: Browser codec + window (JS ports) with golden tests

**Files:**
- Create: `septagraph/web/js/sept_codec.js`, `septagraph/web/js/window.js`, `septagraph/web/test/codec.test.html`

**Interfaces — Produces** (ES modules):
```js
export function encodeHit({voiceId, layer, onset, velocity, mu, sigma}) -> Uint8Array
export function encodeMove({voiceId, mu}) -> Uint8Array
export function encodeShape({shapeId, layer, durationS, knots}) -> Uint8Array
export function decode(bytes) -> Array<{type:"hit"|"move"|"shape"|"manifestRequest"|..., ...}>
export function project(window, channel, d, velocity) -> number[8]    // §C, identical semantics to py
```
Use `osc.js` UMD from `https://cdn.jsdelivr.net/npm/osc@2.4.5/dist/osc-browser.min.js` (`osc.writePacket`, `osc.readPacket`, explicit type tags: `{type:"f", value}`).

- [ ] **Step 1:** `codec.test.html` loads each `../../golden/*.osc` via `fetch` (serve with `python -m http.server` from `septagraph/`), re-encodes the same fixtures in JS, and asserts byte equality; displays PASS/FAIL per vector in the page and via `console.log("[sept-test] ...")`. Also port the `test_project_center_and_edges` / `test_velocity_to_loud` assertions. **Step 2:** Open → FAIL (modules missing). **Step 3:** Implement. Float32 note: Python's `struct.pack('>f')` and osc.js both write IEEE float32 — byte equality is expected. **Step 4:** All PASS in the page. **Step 5:** Commit `septagraph: browser codec + window port, golden-tested against python`.

### Task 2.3: WebChucK renderer shim

**Files:**
- Create: `septagraph/web/js/webchuck_render.js`
- Modify: `septagraph/chuck/sept_render_core.ck` (only if a global needs renaming for the WebChucK API — keep the file identical for native and web)

**Interfaces — Produces:**
```js
export async function startRenderer(coreCkUrl) -> { feed(bytes: Uint8Array): void, chuck: Chuck }
```
`import { Chuck } from "https://cdn.jsdelivr.net/npm/webchuck@1.2.9/+esm"` (pin the current version at implementation time; check `npm view webchuck version`). `Chuck.init([{serverFilename: coreCkUrl, virtualFilename: "sept_render_core.ck"}])`, then `chuck.runFile("sept_render_core.ck")`. `feed()` decodes with `sept_codec.decode`, maps `voiceId → slot` (LRU, 16), then `chuck.setFloat("v_mu_3_0", …)` ×8, `setFloat("v_sig_…")`, `setFloat("v_vel_3", v)`, `setInt("v_gate_3", 1)`, `chuck.broadcastEvent("v_hit_3")`. Shape → `sh_*` globals. **The `.ck` core is byte-identical to the native one** — that's the point.

- [ ] **Step 1:** Add a `?selftest=1` mode to `index.html` (Task 2.4) that feeds the golden `hit_basic.osc` after user click (AudioContext needs a gesture) and logs `[sept] voice 0 gate 1` from a `<<< >>>` print in the core. **Step 2:** Verify in browser console. **Step 3:** Commit `septagraph: webchuck renderer shim (shares core .ck with native)`.

### Task 2.4: One-page laptop pair — trackpad controller

**Files:**
- Create: `septagraph/web/index.html`, `septagraph/web/js/trackpad_ctl.js`, `septagraph/py/controllers/input_manifests/trackpad.json`

**Design:** Page layout: top bar — controller dropdown (`trackpad` | `webcam`, webcam greyed until Task 4.3), renderer status (`WebChucK ready` / `bridge connected`), **Start Audio** button. Main: a large square "device space" pad. Trackpad manifest: `{"controller_id":"web-trackpad","channels":[{"name":"pad","dims":2,"kind":"continuous","size":null},{"name":"click","dims":1,"kind":"discrete","size":[1]}]}`. Default window: `center=HALF, extent=[0.45,0.3,0.3,0,0.45,0,0,0], assign={"pad":[0,4]}` (x → freq, y → scale, matching the grid defaults). Interaction: `pointerdown` → `/hit onset=1 velocity=0.7 mu=project(...)` with `voiceId="web:"+n`; `pointermove` while down → `/move` at ≤ 100 Hz (throttle with `requestAnimationFrame` + timestamp guard); `pointerup` → `/hit onset=0`. Wheel over the pad → modulates `harm` (idx 2) by ±0.05 per notch via `modulate` (bipolar delta, controller-side, per §A). Two-finger / second pointer = second voice. The pad draws: the current window's axis labels, the last 8 hit positions, and (later) manifest catalog points as warm-white dots. Every packet goes to **two sinks**: `renderer.feed(bytes)` (in-page WebChucK) and `bridge.send(bytes)` if connected (Task 2.5).

- [ ] **Step 1:** Build `index.html` + `trackpad_ctl.js`. Show in the page a live hex dump of the last packet's first 16 bytes + decoded address — reviewers should *see* real OSC. **Step 2:** Manual test checklist (write it into `web/README.md`): click → sound; drag → pitch/scale follow; release → stops within 100 ms; 2 fingers → 2 voices; wheel → tone↔noise. **Step 3:** Commit `septagraph: one-page laptop pair (trackpad -> webchuck)`.

### Task 2.4b: Real-time 7-axis visualizer (web)

**Files:**
- Create: `septagraph/web/js/viz.js`
- Modify: `septagraph/web/index.html` (right-hand panel)

**Interfaces — Produces:** `export function makeViz(canvas) -> { onPacket(decodedMsg), setManifests(list) }`. Consumes decoded messages from `sept_codec.decode` — it is a **passive listener on the protocol stream**, so the same visualizer works for any controller (trackpad, webcam, or neuralGrid via the bridge's 7701 relay, which Task 2.5 must also forward *outbound* controller traffic to WS clients when started with `--tap`).

**Design:** (a) one **radar plot** per live voice (8 spokes = the 8 coordinate values; polygon = μ; translucent band = μ±σ), voices colored by `voice_id` hash, fading out 500 ms after release; (b) two **2D slices** (`freq × scale`, `freq × harm`) showing live voice dots, the last 2 s of `/move` trails, and catalog points from received manifests as warm-white dots with their instrument label on hover; (c) a **time strip** showing the active shape's μ(t) for the play layer. Draw at 60 fps with `requestAnimationFrame`; never block packet handling.

- [ ] **Step 1:** Feed the golden `hit_basic.osc` + a synthetic manifest in `?selftest=1` mode; assert visually that one radar and one catalog dot render. **Step 2:** Implement. **Step 3:** Commit `septagraph: real-time 7-axis visualizer`.

### Task 2.5: Bridge (browser ↔ UDP)

**Files:**
- Create: `septagraph/py/septagraph/tools/sept_bridge.py`, `septagraph/web/js/bridge_client.js`
- Test: `septagraph/py/tests/test_bridge.py`

**Interfaces — Produces:** `python -m septagraph.tools.sept_bridge --ws 8766 --to 127.0.0.1:7700 [--to 192.168.0.103:7700] [--broadcast]` — a WebSocket server (stdlib `http.server` + hand-rolled RFC6455 frames, binary opcode 0x2 only; **no new dependency**) that relays each binary frame verbatim to all UDP targets, and relays any UDP packet arriving on 7701 back to all WS clients. With `--tap`, it also opens a second UDP listener on 7700 (only when no renderer runs on this host) and forwards every controller packet it sees to WS clients, so the web visualizer (Task 2.4b) can display what the neuralGrid is doing live. `bridge_client.js`: `connect(url) -> {send(bytes), onPacket(cb)}` with auto-reconnect.

- [ ] **Step 1:** Test: start bridge in-process on ephemeral ports, open a raw `websocket`-less client using Python `socket` + minimal handshake (or `websockets` is NOT allowed — write the 20-line client in the test), send the golden hit as a binary frame, assert a UDP Receiver on the target gets identical bytes; send a manifest to 7701, assert the WS client receives it. **Step 2:** FAIL. **Step 3:** Implement. **Step 4:** PASS. **Step 5:** Commit `septagraph: websocket<->udp bridge`. Now: **browser trackpad → native ChucK renderer**, and later **browser trackpad → Machine Lab**.

---

# Phase 3 — Machine Lab renderer + neuralGrid controller (the prime demo)

### Task 3.1: Read the Lab's OSC argument contract (read-only reconnaissance)

**Files:**
- Create: `septagraph/py/renderers/LAB_OSC_CONTRACT.md`

- [ ] **Step 1:** From the Mac, read-only: `ssh mtiid@192.168.0.103 cat ~/Desktop/GOR_STYLE/docs/INSTRUMENT_COMMS.md` and `cat ~/git/machineLabCode/updatedTriggerList.txt`. **Do not run anything else on the mini.** Record for each address `/breakBot /ganaPati /tammy /marimba /trimpbeat /modulettes /rattleTron`: exact OSC arg list (e.g. `i note i velocity` vs `i beater f ms`), MIDI channel, valid note/beater numbers, broken triggers ('X'), and which handlers are commented out in the front door (as of 2026-08-29: modulettes/tammy/rattleTron; tammy was re-enabled). Note GanaPati's thermal auto-shutdown rule ("faster ⇒ lower velocity").
- [ ] **Step 2:** Write `LAB_OSC_CONTRACT.md` as a table. This is the ground truth for Task 3.2's `lab_map.json`.
- [ ] **Step 3:** Commit `septagraph: machine lab OSC contract (read-only survey)`.

### Task 3.2: Machine Lab renderer

**Files:**
- Create: `septagraph/py/renderers/lab_render.py`, `septagraph/py/renderers/lab_map.json`
- Test: `septagraph/py/tests/test_lab_render.py`

**`lab_map.json` v0 (hand-authored; Phase 4 measures it):** a catalog manifest — one entry per playable beater/note across all working robots:
```json
{"renderer_id":"lab","name":"CalArts Machine Lab","kind":"catalog","target":"192.168.0.103:8001",
 "points":[{"point_id":0,"instrument":"breakBot","osc":{"address":"/breakBot","args":["i:note=36","i:vel"]},
            "mu":[0.30,0.7,0.15,0.02,0.6,0.35,0.05,0.4],"sigma":[0.03,0,0.05,0,0.05,0,0,0],
            "max_rate_hz":12,"vel_min":0.15,"vel_max":1.0}, ...]}
```
Initial `mu` values are *estimates* by ear (drum ≈ low freq/noisy/dense scale; marimba ≈ tonal/clean; each robot's real room azimuth measured with a tape measure from the audience center). Sigma nonzero on the axes you're unsure about — honest uncertainty.

**Interfaces — Produces:**
```python
class LabRenderer:
    def __init__(self, map_path, sender: Sender, ctl_targets: list[tuple[str,int]], rng=random.Random)
    def on_message(self, obj) -> None        # Hit -> choose point -> OSC to lab; Move -> ignored in v1 (robots have no continuous control); ManifestRequest -> announce
    def choose(self, hit: Hit) -> CatalogPoint | None   # within_sigma candidates -> random choice; else nearest; skip if point's last strike < 1/max_rate_hz ago (return None)
    def announce(self) -> None
def lab_osc(point, hit) -> bytes             # builds the lab's own message; velocity scaled into [vel_min, vel_max] then to the lab's int range
```
CLI: `python -m septagraph.renderers.lab_render --map lab_map.json --lab 192.168.0.103:8001 --dry-run` (dry-run prints instead of sending).

- [ ] **Step 1:** Tests with a fake UDP "lab" receiver on loopback: a hit at a point's exact `mu` with `sigma=0` sends exactly that robot's message with correctly scaled velocity; `sigma=[0.1]*8` around two close points chooses either (seeded rng, assert set over 50 trials == both); rate limiting drops a second hit 20 ms after the first when `max_rate_hz=12`; `onset=0` sends nothing; `manifest/request` yields a complete catalog manifest on the ctl port. **Step 2:** FAIL. **Step 3:** Implement. **Step 4:** PASS. **Step 5:** Commit `septagraph: machine lab renderer (catalog, sigma-choice, rate limits)`.
- [ ] **Step 6 (on site, user present):** `--dry-run` first, eyeball the messages against `LAB_OSC_CONTRACT.md`; then live with the **browser trackpad → bridge → lab_render → :8001**. Confirm the lab's server is already running (never start it yourself). This is M0 achieved from a laptop.

### Task 3.3: neuralGrid controller

**Files:**
- Create: `septagraph/py/controllers/grid_ctl.py`, `septagraph/py/controllers/input_manifests/neuralgrid.json`
- Test: `septagraph/py/tests/test_grid_ctl.py`

**Interfaces — Consumes:** `neuralGrid/pi/neuralgrid/link_lidar.py: LidarGridLink` (key events `(x, y, edge)`, analog `[n,s,e,w]` raw 0–4095, lidar `[n,s,e,w]` mm). **Produces:**
```python
class GridController:
    def __init__(self, link, sender: Sender, window: Window, manifests: ManifestStore, leds: Callable[[bytes], None])
    def on_key(self, x, y, edge) -> None       # edge 1: cell_table lookup -> Hit(onset=1, velocity=0.7 [no velocity sensing on NeoTrellis]) ; edge 0: Hit(onset=0)
    def on_analog(self, raw4) -> None          # threshold(>120)+median(5)+EMA(0.3) per strip -> delta in [-1,1] centered at strip middle -> modulate idx {N:3 rate, E:1 loud, S:7 dist, W:2 harm} -> /move for every held voice (voice-bound) at <=50 Hz
    def on_lidar(self, mm4) -> None            # min over 4 sensors, 100..800 mm -> spread idx 6 (1 - norm) -> /move global at 20 Hz
    def on_manifest(self, m) -> None           # store.update; rebuild cell_table; LEDs: warm white on snapped cells, hue by y for continuum cells (per plan §8)
    def rebuild(self) -> None
```
Voice ids `"grid:{x}{y}"`. Fast path must be: dict lookup + encode + sendto — measure it (Task 3.4).

- [ ] **Step 1:** Tests with a fake link and a loopback Receiver: key press on a snapped cell emits a hit whose `mu` equals the catalog point; release emits `onset=0` with the same voice id; analog stream while a key is held emits voice-bound `/move` with `rate` changed; untouched strip (raw < 120) emits nothing; a manifest arrival changes the LED frame (assert `leds` called with 192 bytes, warm white at the snapped index `i = y*8 + x`). **Step 2:** FAIL. **Step 3:** Implement; `touch.py` is NOT reused (obsolete assumptions, see HANDOFF §3). **Step 4:** PASS. **Step 5:** Commit `septagraph: neuralgrid controller`.
- [ ] **Step 6 (Pi):** rsync `septagraph/` to `demiurge@neuralgrid.local:~/neuralGrid/septagraph/` (additive). Stop `neuralgrid-monitor` (it owns the serial port) → run `python -m septagraph.controllers.grid_ctl --renderers 127.0.0.1:7700,<lab-pi-or-mac>:7700`. Restart the monitor afterwards. Add a `neuralgrid-septagraph.service` unit file to `septagraph/pi/` (don't enable it yet — the two services conflict on the serial port; document that in `septagraph/README.md`).

### Task 3.4: Latency probe → paper table

**Files:**
- Create: `septagraph/py/septagraph/tools/latency_probe.py`, `septagraph/py/tests/test_latency_probe.py`
- Output: `neuralGrid/paper/data/latency_v1.csv`

Measures three things, each 200 trials: (a) controller path: fake key event → `sendto` (in-process `time.perf_counter_ns`); (b) wire+decode: Sender → loopback Receiver → decoded `Hit`; (c) end-to-end with the ChucK renderer: Receiver-side shred prints `<<< now >>>` on gate, stdout timestamped by the probe (documents ±1 ms stdout jitter). Reports p50/p95/max in ms and writes CSV `stage,p50,p95,max,n`.

- [ ] **Step 1:** Test: probe (a) and (b) on loopback return p95 < 2 ms. **Step 2/3/4:** implement, run, PASS. **Step 5:** Run on the Pi as well; commit both CSVs (`latency_v1_mac.csv`, `latency_v1_pi.csv`). Commit `septagraph: latency probe + first measurements`.

---

# Phase 4 — Catalog, synergy, webcam

### Task 4.1: Catalog measurement front-end

**Files:**
- Create: `septagraph/py/septagraph/tools/catalog_measure.py`, `septagraph/py/septagraph/analysis.py`
- Test: `septagraph/py/tests/test_analysis.py` (synthetic signals)
- Modify: `septagraph/py/renderers/lab_map.json` (measured `mu`/`sigma` replace estimates; keep `"source":"measured"` per point)

**Interfaces — Produces:** `analysis.measure(wav: np.ndarray, sr: int) -> tuple[Vec, Vec]` (μ, σ over the decay; per §A): `freq` from spectral centroid → `from_physical(0, hz)`; `loud` from RMS dBFS → `from_physical(1, ·)`; `harm` = 1 − spectral flatness; `rate` from the peak of the envelope's modulation spectrum (0.5–64 Hz); `scale` from the peak of the log-spectrum's cepstral-like ripple in cyc/oct; `az/spread/dist` are **not** measured from a mono recording — kept from the tape-measure geometry. σ = std of the frame-wise values over the sound's active frames. `catalog_measure.py`: for each point in `lab_map.json`, send its lab message via `lab_osc`, record 2 s from the default input device (`sounddevice` — add as an *optional* dependency in `pyproject [measure]`), call `measure`, write back.

- [ ] **Step 1:** Tests on synthetic signals: 440 Hz sine → `freq ≈ from_physical(0,440)`, `harm > 0.9`; white noise → `harm < 0.2`; sine AM'd at 8 Hz → `rate ≈ from_physical(3, 8)` within 0.05. **Step 2:** FAIL. **Step 3:** Implement with numpy only (STFT by hand, 2048/512). **Step 4:** PASS. **Step 5:** Commit. **Step 6 (on site):** run with a measurement mic at the audience center; commit `lab_map.json` v1 + the WAVs under `neuralGrid/paper/data/catalog_v1/` (small: 2 s each, 16-bit). This is M1 and the catalog figure.

### Task 4.2: Synergy — snap + residual with ChucK

**Files:**
- Modify: `septagraph/chuck/sept_render_osc.ck` (manifest **ingest**: subscribe to 7701 and hold the lab's catalog; add `--residual` mode flag via `me.arg()`)
- Modify: `septagraph/py/controllers/grid_ctl.py` (already snaps via `cell_table`; add `--residual` to pass the flag on the wire as `sigma`? **No** — residual is renderer-side per plan §6: the *synth* defers within radius `r` of a catalog point)
- Test: `tests/test_chuck_renderer.py` extended

Residual rule in the ChucK renderer: if a catalog manifest is held and the hit's `mu` is within `r=0.06` (∞-norm) of a catalog point → silent (defer to the robot); between `r` and `2r` → linear gain ramp; beyond → full synthesis. This lets **Pi(grid) → {Lab renderer, ChucK renderer}** play the HYBRID preset: robots on catalog cells, synth filling the continuum.

- [ ] Steps: integration test — with a fake lab manifest injected, a hit at a catalog point produces the `<<< defer >>>` print, a hit 0.2 away produces `<<< synth >>>`; implement; PASS; commit `septagraph: residual rendering in chuck reference renderer`. Demo presets LAB DIRECT / CONTINUUM / HYBRID now exist as CLI flag combinations; write them into `septagraph/README.md` as the demo script.

### Task 4.3: Webcam 3D controller (web, `/move` only)

**Files:**
- Create: `septagraph/web/js/webcam_ctl.js`, `septagraph/py/controllers/input_manifests/webcam.json`
- Modify: `septagraph/web/index.html` (enable the dropdown option)

Manifest: `{"controller_id":"web-hand","channels":[{"name":"hand","dims":3,"kind":"continuous"},{"name":"pinch","dims":1,"kind":"continuous"}]}`. MediaPipe: `import { HandLandmarker, FilesetResolver } from "https://cdn.jsdelivr.net/npm/@mediapipe/tasks-vision@0.10.14"` (pin at implementation time). `d = (1 - wrist.x, 1 - wrist.y, clamp((wristToIndexMCP_px / frame_h - 0.05) / 0.25))` (hand size as depth proxy), `pinch = 1 - dist(thumbTip, indexTip)/handSize`. Assignment: `hand → [0, 4, 7]` (x freq, y scale, depth dist), `pinch → [2]` harm. **Onsets come from the keyboard** (space bar = hit at current `mu`; hold = sustain); the hand only sends `/move` at the camera frame rate (~30 Hz) for held voices — this is the fast-plane latency rule applied, not dodged. Display measured camera→packet latency in the status bar (`performance.now()` from frame timestamp) — that number goes in the paper.

- [x] Steps: implement; manual checklist in `web/README.md` (pinch → tone↔noise, forward/back → wet/dry, spacebar → onset); commit `septagraph: webcam 3D /move controller`.

---

### Task 4.4: Listening experiment (the space's empirical test)

**Files:**
- Create: `septagraph/web/experiment.html`, `septagraph/web/js/experiment.js`
- Create: `septagraph/py/septagraph/tools/render_stimuli.py`, `septagraph/py/septagraph/tools/experiment_analysis.py`
- Test: `septagraph/py/tests/test_experiment_analysis.py`
- Output: `neuralGrid/paper/data/experiment_v1/` (stimuli manifest, anonymized responses CSV, results)

**Design (pre-registered in `neuralGrid/paper/data/experiment_v1/PREREG.md` before any data is collected):**
- **H1 (steerability):** listeners identify which single axis changed at above chance (1/8) for each index. **H2 (cross-renderer identity):** in an ABX where A and B are the same coordinate on two renderers and X is a shifted coordinate (Δ = 0.25 on one axis) on one of them, listeners match X to the shifted position above chance. **Control:** a permuted-label block (axis names shuffled per participant); H1 accuracy in the control block should fall to chance — if it doesn't, the labels aren't carrying meaning.
- Stimuli: `render_stimuli.py` produces 2 s WAVs: for each index i, `mu = HALF` with `mu[i]` = 0.2 vs 0.8 (sweep pair), rendered offline through the native ChucK renderer (`chuck --silent` + `WvOut`) and, for H2, matched with the nearest measured Lab catalog recordings from Task 4.1. Stimuli manifest JSON lists file, renderer, `mu`.
- Trials: 8 axes × 2 renderers × 2 repeats = 32 forced-choice trials (H1) + 16 ABX trials (H2) + 16 control trials; randomized; ~15 min. n target 20.
- Page: consent screen (plain-language, anonymous, no PII, "designed by the protocol's author" disclosure), headphone check (3 trials of a simple left/right task; fail → excluded), trials, free-text comment. Responses POST to `sept_bridge.py --collect <dir>` (add a `/collect` HTTP route writing one JSON line per participant; no accounts, no cookies) — or download as JSON when run locally.
- Analysis: per-axis accuracy with 95% binomial CIs; exact binomial test vs 1/8 (H1) and vs 1/2 (H2); control-block comparison; report *all* axes including the weak ones. `experiment_analysis.py` writes `results.md` + a bar figure (`paper/figs/steerability.pdf`).
- Ethics: before collecting, confirm with CalArts whether IRB/ethics review is required for an anonymous perceptual study; record the answer in `PREREG.md`.

- [ ] **Step 1:** Tests for `experiment_analysis.py` on synthetic response sets: all-correct → accuracy 1.0, CI upper 1.0; uniform-random responses → not significantly above 1/8; control block with permuted labels handled. **Step 2:** FAIL. **Step 3:** Implement analysis (numpy + `math.comb` for the exact binomial; no scipy). **Step 4:** PASS. **Step 5:** Build stimuli + page; pilot with 3 people; fix; freeze `PREREG.md`; collect; run analysis; commit `septagraph: listening experiment (steerability + cross-renderer identity)`.

# Phase 5 — Paper (parallel with 2–4; two tasks start NOW)

### Task 5.1: Related work + bibliography (start now)

**Files:**
- Create: `neuralGrid/paper/related_work.tex`, `neuralGrid/paper/septagraph.bib`
- Modify: `neuralGrid/paper/septagraph.tex` (replace the `\section{Related Work and Limitations}` skeleton with `\input{related_work}`; switch `thebibliography` to `\bibliography{septagraph}` — additive: keep the old block commented)

Must cite and *differentiate* (each with a one-sentence "how we differ"): Wessel 1979 timbre space; Grey 1977; Schwarz CataRT / corpus-based concatenative synthesis (closest prior: catalog + nearest neighbor — difference: our catalog is a *renderer capability declaration* on a shared protocol, chosen by a deaf controller, with σ as orchestration freedom); Peeters timbre descriptors; Wright & Freed OSC (transport vs vocabulary); MIDI 2.0 Capability Inquiry / Property Exchange (closest to `/manifest`); MPE; SpatDIF (OSC sublanguage precedent); Hunt & Wanderley / Hunt, Wanderley & Paradis mapping; Fiebrink Wekinator; Kapur robotic musicianship + KarmetiK / Machine Lab lineage (Kapur, Diakopoulos, Vallis, Morris…); Weinberg & Driscoll / Shimon; Singer LEMUR; Engel DDSP; Caillon & Esling RAVE; Shamma / Chi–Ru–Shamma; McDermott & Simoncelli. **Every citation verified to exist** (title, venue, year) — the agent must web-check each and mark any it cannot verify as `% UNVERIFIED` rather than inventing.

- [ ] Steps: survey → `.bib` → prose (≤ 1 page two-column) → compile check on Overleaf is manual; locally run `lacheck`/`chktex` if present else skip → commit `paper: related work + bibliography`.

### Task 5.2: Reframed introduction + "Origin: playing a robot orchestra as one instrument" (start now)

**Files:**
- Create: `neuralGrid/paper/intro.tex`, `neuralGrid/paper/origin.tex`
- Modify: `septagraph.tex` (`\input` both; abstract line "we demonstrate…" rewritten to promise exactly Phases 2–4: web pair, Machine Lab renderer, neuralGrid controller, latency table; neural renderer phrased as "in progress with collaborators").

Intro beats (from the skeleton) reordered so beat 1 is the Machine Lab problem: seven heterogeneous robots, some tonal, some percussive, fixed in a room, with mechanical limits, playable today only as separate MIDI/OSC endpoints → wanting one instrument → note/channel vocabularies cannot express "a bit noisier and further left" → the coordinate space. Then MIDI's underspecification lesson, then the generative-audio control gap, then the thesis, then the matrix forecast. The Machine Lab is named with permission of the ensemble's directors (**action for the user: ask Kapur/Colton about authorship or acknowledgment before submission**).

- [ ] Steps: draft → user reads (they write final prose voice) → commit `paper: reframed intro + origin section`.

### Task 5.3: Evidence sections (after Phases 3–4)

**Files:** Modify `septagraph.tex`: Implementations & Evaluation section gets (a) `latency_v1_*.csv` as a table (p50/p95 per stage, Mac and Pi, model-off; note that model-on is structurally identical because the model is not in the path); (b) the interchangeability matrix with real cells: controllers {WebChucK trackpad, WebChucK webcam, neuralGrid} × renderers {ChucK ref (web), ChucK ref (native), Machine Lab} — every checkmark links to a video timestamp; (c) catalog figure: measured Lab points plotted on freq × scale and freq × harm slices; (d) one practice-based performance report (the first gig with LAB DIRECT + HYBRID); (e) the listening-experiment results from Task 4.4 (steerability per axis, cross-renderer ABX, control block) reported in full, weak axes included.

- [ ] Steps: generate figures with a small `paper/figs/make_figs.py` (matplotlib → PDF) → include → commit `paper: evaluation tables + figures`.

### Task 5.4: NIME template + logistics

- [ ] Look up the NIME 2027 CFP when posted (Paris, 22–25 June 2027; expect deadline late Jan/early Feb 2027); record deadline, page limit, template in `neuralGrid/paper/LOGISTICS.md`; port `septagraph.tex` into the official class (content unchanged). Commit.

---

## Self-review notes

- **Spec coverage:** Plan §2 protocol → §B + Tasks 1.3/2.1/3.2. Window → §C + 1.4/3.3. Manifests/catalog/constraints → 1.5/3.2/4.1. Residual rule → 4.2. Presets LAB DIRECT/CONTINUUM/HYBRID → 4.2 README; MIRROR (model) → Phase 6, excluded by design. Artistic defaults (x→freq, y→scale, softpot map, lidar→space, LED language) → 3.3. Cold start (usable with no model) → the entire plan is model-free. Boundary detector, layers/loop, capture-to-septagraph, MDN model, DDSP/Colton → Phase 6 (separate plan, after Phase 3).
- **Decisions this plan makes that the v2 plan left open or implied differently** (flag to user): (1) renderers receive **absolute** coordinates; windows are controller-internal and never on the wire — contradicts "/shape: septagraphs + window configs" in plan v2 §3; simpler and stronger separation. (2) `space` = 3 wire values → `K = 8`. (3) Shape `mu` is relative to the hit. (4) NeoTrellis has no velocity → fixed 0.7; velocity comes from the softpot E as a modulator. (5) `/move` is ignored by the Lab renderer in v1.
- **Type consistency:** `Vec`, `Hit`, `Move`, `Septagraph`, `Knot`, `CatalogPoint`, `Manifest`, `Window`, `Channel`, `Sender`, `Receiver`, `ManifestStore`, `cell_table`, `project`, `modulate` used identically across Tasks 1.2–3.3. JS names mirror in camelCase.
