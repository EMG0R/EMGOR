"""Scene DSL: validation + runtime instantiation.

A scene is plain JSON:
{
  "name": "comet",
  "master": {"brightness": 1.0, "blur": 0.0, "tone_map": "clip",
             "strobe": {"mode": "pulse", "rate_hz": 0.5, "depth": 0.4, "duty": 0.5}},
  "layers": [ ...layer objects... ]
}
See README.md for the full layer reference.

Optional top-level "projector" key: any JSON object, validated only for
shape (must be an object if present; a string/array/number is rejected).
The engine never interprets it -- it is stored verbatim with the scene,
returned as-is by GET /scene, and forwarded verbatim in every canvas-tap
semantic packet (see taps.py / semantic.py / docs/TAPS_DESIGN.md). It
exists purely so a projection-mapping consumer (GORMAPPER) can travel
alongside a scene's own authored intent (style/energy/palette/brightness
hints) without the lightserver needing to know or validate that vocabulary.
"""
from __future__ import annotations

import math
import re

import numpy as np

from .layers import LAYER_TYPES, make_layer

_HEX_RE = re.compile(r"^#?[0-9a-fA-F]{6}$|^#?[0-9a-fA-F]{3}$")

BLENDS = ("add", "max", "alpha", "boost")
TONE_MAPS = ("clip", "reinhard")
STROBE_MODES = ("none", "pulse", "strobe")
GRAVITY_MODES = ("direction", "point")
FALLOFFS = ("gaussian", "hard")
ATTRACTOR_FALLOFFS = ("constant", "linear", "inverse_square")
COLOR_MODES = ("static", "velocity", "age", "index")


class SceneValidationError(ValueError):
    pass


def _err(path: str, msg: str):
    raise SceneValidationError(f"{path}: {msg}")


def _check_color(v, path):
    if not isinstance(v, str) or not _HEX_RE.match(v):
        _err(path, f"expected hex color like '#rrggbb', got {v!r}")


def _check_num(d, key, path, lo=None, hi=None, required=False):
    if key not in d or d[key] is None:
        if required:
            _err(path, f"missing required number '{key}'")
        return
    v = d[key]
    if isinstance(v, bool) or not isinstance(v, (int, float)):
        _err(f"{path}.{key}", f"expected number, got {v!r}")
    if lo is not None and v < lo:
        _err(f"{path}.{key}", f"must be >= {lo}, got {v}")
    if hi is not None and v > hi:
        _err(f"{path}.{key}", f"must be <= {hi}, got {v}")


def _check_enum(d, key, path, allowed):
    if key in d and d[key] is not None and d[key] not in allowed:
        _err(f"{path}.{key}", f"must be one of {allowed}, got {d[key]!r}")


def _check_palette(d, key, path):
    if key in d and d[key] is not None:
        if not isinstance(d[key], list) or not d[key]:
            _err(f"{path}.{key}", "must be a non-empty list of hex colors")
        for i, c in enumerate(d[key]):
            _check_color(c, f"{path}.{key}[{i}]")


def validate_scene(scene: dict) -> dict:
    """Validate a scene dict against the DSL. Raises SceneValidationError.
    Returns the scene unchanged on success."""
    if not isinstance(scene, dict):
        raise SceneValidationError("scene must be a JSON object")
    if "layers" not in scene or not isinstance(scene["layers"], list) or not scene["layers"]:
        raise SceneValidationError("scene.layers: must be a non-empty list")
    if "name" in scene and not isinstance(scene["name"], str):
        raise SceneValidationError("scene.name: must be a string")
    if "projector" in scene and scene["projector"] is not None:
        if not isinstance(scene["projector"], dict):
            raise SceneValidationError("scene.projector: must be an object")

    m = scene.get("master", {})
    if not isinstance(m, dict):
        raise SceneValidationError("scene.master: must be an object")
    _check_num(m, "brightness", "master", 0.0, 4.0)
    _check_num(m, "blur", "master", 0.0, 50.0)
    _check_enum(m, "tone_map", "master", TONE_MAPS)
    st = m.get("strobe")
    if st is not None:
        if not isinstance(st, dict):
            raise SceneValidationError("master.strobe: must be an object")
        _check_enum(st, "mode", "master.strobe", STROBE_MODES)
        _check_num(st, "rate_hz", "master.strobe", 0.0, 30.0)
        _check_num(st, "depth", "master.strobe", 0.0, 1.0)
        _check_num(st, "duty", "master.strobe", 0.01, 1.0)

    for li, layer in enumerate(scene["layers"]):
        p = f"layers[{li}]"
        if not isinstance(layer, dict):
            _err(p, "must be an object")
        lt = layer.get("type")
        if lt not in LAYER_TYPES:
            _err(f"{p}.type", f"must be one of {sorted(LAYER_TYPES)}, got {lt!r}")
        _check_enum(layer, "blend", p, BLENDS)
        _check_num(layer, "opacity", p, 0.0, 2.0)
        _check_num(layer, "decay", p, 0.0, 0.999)
        _check_trail(layer, p)
        if lt == "particles":
            _validate_particles(layer, p)
        elif lt == "wave":
            _check_palette(layer, "palette", p)
            _check_num(layer, "palette_speed", p)
            if "color" in layer and layer["color"] is not None:
                _check_color(layer["color"], f"{p}.color")
            for wi, w in enumerate(layer.get("waves") or []):
                wp = f"{p}.waves[{wi}]"
                _check_num(w, "wavelength", wp, 1.0)
                _check_num(w, "speed", wp)
                _check_num(w, "amplitude", wp, 0.0)
                _check_num(w, "phase", wp)
        elif lt in ("noise", "plasma", "gradient"):
            _check_palette(layer, "palette", p)
            _check_num(layer, "palette_speed", p)
            _check_num(layer, "scale", p, 0.5)
            _check_num(layer, "speed", p)
            _check_num(layer, "brightness", p, 0.0, 4.0)
            _check_num(layer, "scroll_speed", p)
            _check_num(layer, "octaves", p, 1, 6)
            _check_num(layer, "contrast", p, 0.0, 10.0)
            _check_num(layer, "threshold", p, 0.0, 1.0)
            _check_num(layer, "drift", p)
        elif lt == "solid":
            _check_color(layer.get("color", "#000000"), f"{p}.color")
        elif lt == "paint":
            px = layer.get("pixels", {})
            if not isinstance(px, dict):
                _err(f"{p}.pixels", "must be an object of index -> hex color")
            for k, v in px.items():
                try:
                    int(k)
                except (TypeError, ValueError):
                    _err(f"{p}.pixels", f"key {k!r} is not a pixel index")
                _check_color(v, f"{p}.pixels[{k}]")
            bg = layer.get("background", "#000000")
            if bg is not None:
                _check_color(bg, f"{p}.background")
    return scene


def _check_trail(d, path):
    trail = d.get("trail")
    if trail is None:
        return
    if not isinstance(trail, dict):
        _err(f"{path}.trail", "must be an object")
    _check_num(trail, "decay", f"{path}.trail", 0.0, 0.999)
    _check_num(trail, "blur", f"{path}.trail", 0.0, 50.0)


def _validate_particles(layer: dict, p: str):
    _check_palette(layer, "palette", p)
    _check_num(layer, "palette_speed", p)
    ph = layer.get("physics") or {}
    if not isinstance(ph, dict):
        _err(f"{p}.physics", "must be an object")
    g = ph.get("gravity")
    if g:
        _check_enum(g, "mode", f"{p}.physics.gravity", GRAVITY_MODES)
        _check_num(g, "accel", f"{p}.physics.gravity")
        _check_num(g, "point", f"{p}.physics.gravity", 0)
    _check_num(ph, "damping", f"{p}.physics", 0.0)
    b = ph.get("bounce")
    if isinstance(b, dict):
        _check_num(b, "restitution", f"{p}.physics.bounce", 0.0, 1.0)
    w = ph.get("wind")
    if w:
        _check_num(w, "accel", f"{p}.physics.wind")
        _check_num(w, "vary_hz", f"{p}.physics.wind", 0.0)
        _check_num(w, "vary_amount", f"{p}.physics.wind", 0.0, 1.0)
    tb = ph.get("turbulence")
    if tb:
        _check_num(tb, "accel", f"{p}.physics.turbulence")
        _check_num(tb, "scale", f"{p}.physics.turbulence", 1.0)
        _check_num(tb, "speed", f"{p}.physics.turbulence")
    for ai, a in enumerate(ph.get("attractors") or []):
        ap = f"{p}.physics.attractors[{ai}]"
        _check_num(a, "pos", ap, 0)
        _check_num(a, "strength", ap)
        _check_num(a, "radius", ap, 1.0)
        _check_enum(a, "falloff", ap, ATTRACTOR_FALLOFFS)
    for ei, e in enumerate(layer.get("emitters") or []):
        ep = f"{p}.emitters[{ei}]"
        pos = e.get("pos", 0)
        if isinstance(pos, list):
            if len(pos) != 2:
                _err(f"{ep}.pos", "range must be [min, max]")
        elif isinstance(pos, bool) or not isinstance(pos, (int, float)):
            _err(f"{ep}.pos", f"must be number or [min,max], got {pos!r}")
        _check_num(e, "rate", ep, 0.0)
        _check_num(e, "vel", ep)
        _check_num(e, "vel_jitter", ep, 0.0)
        _check_num(e, "lifetime", ep, 0.0)
        _check_num(e, "lifetime_jitter", ep, 0.0)
        _check_num(e, "size", ep, 0.1)
        _check_num(e, "size_jitter", ep, 0.0)
        _check_num(e, "mass", ep, 0.001)
        _check_num(e, "burst", ep, 0)
        if "color" in e and e["color"] is not None:
            _check_color(e["color"], f"{ep}.color")
        _check_palette(e, "colors", ep)
        _check_enum(e, "color_mode", ep, COLOR_MODES)
        _check_num(e, "hue_drift", ep)
    for pi, pc in enumerate(layer.get("particles") or []):
        pp = f"{p}.particles[{pi}]"
        _check_num(pc, "pos", pp, required=True)
        _check_num(pc, "vel", pp)
        _check_num(pc, "size", pp, 0.1)
        _check_num(pc, "mass", pp, 0.001)
        _check_num(pc, "lifetime", pp, 0.0)
        if "color" in pc and pc["color"] is not None:
            _check_color(pc["color"], f"{pp}.color")
        _check_enum(pc, "color_mode", pp, COLOR_MODES)
        _check_num(pc, "hue_drift", pp)
    for si, s in enumerate(layer.get("springs") or []):
        sp = f"{p}.springs[{si}]"
        _check_num(s, "a", sp, 0, required=True)
        _check_num(s, "b", sp, 0)
        _check_num(s, "anchor", sp, 0)
        _check_num(s, "k", sp, 0.0)
        _check_num(s, "damping", sp, 0.0)
        _check_num(s, "rest", sp)
    r = layer.get("render")
    if r:
        _check_enum(r, "falloff", f"{p}.render", FALLOFFS)


def _gaussian_kernel(sigma: float) -> np.ndarray:
    r = max(1, int(math.ceil(sigma * 3)))
    x = np.arange(-r, r + 1, dtype=np.float64)
    k = np.exp(-(x * x) / (2 * sigma * sigma))
    return k / k.sum()


class SceneInstance:
    """A validated scene, instantiated with live layer state."""

    def __init__(self, scene: dict, num_pixels: int, seed: int | None = None):
        validate_scene(scene)
        self.scene = scene
        self.n = num_pixels
        self.name = scene.get("name", "untitled")
        m = scene.get("master", {})
        self.brightness = float(m.get("brightness", 1.0))
        self.blur = float(m.get("blur", 0.0))
        self.tone_map = m.get("tone_map", "clip")
        st = m.get("strobe") or {}
        self.strobe_mode = st.get("mode", "none")
        self.strobe_rate = float(st.get("rate_hz", 1.0))
        self.strobe_depth = float(st.get("depth", 1.0))
        self.strobe_duty = float(st.get("duty", 0.5))
        self._kernel = _gaussian_kernel(self.blur) if self.blur > 0 else None
        self.layers = [make_layer(l, num_pixels, seed=seed) for l in scene["layers"]]
        self.t = 0.0

    def render(self, dt: float) -> np.ndarray:
        """Advance the sim by dt and return the linear HDR frame (n,3)."""
        self.t += dt
        hdr = np.zeros((self.n, 3), dtype=np.float32)
        for layer in self.layers:
            layer.composite(hdr, self.t, dt)
        # master post
        gain = self.brightness
        if self.strobe_mode == "pulse":
            wave = 0.5 + 0.5 * math.sin(2 * math.pi * self.strobe_rate * self.t)
            gain *= (1.0 - self.strobe_depth) + self.strobe_depth * wave
        elif self.strobe_mode == "strobe":
            phase = (self.t * self.strobe_rate) % 1.0
            on = 1.0 if phase < self.strobe_duty else 0.0
            gain *= (1.0 - self.strobe_depth) + self.strobe_depth * on
        if gain != 1.0:
            hdr *= gain
        if self._kernel is not None:
            for c in range(3):
                hdr[:, c] = np.convolve(hdr[:, c], self._kernel, mode="same")
        return hdr
