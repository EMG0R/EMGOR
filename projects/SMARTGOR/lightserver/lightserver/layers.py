"""Composable render layers. Each layer renders (rgb, alpha) into linear HDR
and is composited with a blend mode: add | max | alpha.

Per-layer trail support: with decay > 0 the layer keeps a persistence buffer
(persist = max(persist * decay, fresh)) — motion leaves fading trails. The
richer form is `"trail": {"decay": 0..0.999, "blur": 0..50}` — same decay
semantics, plus an optional per-frame gaussian blur of the persistence
buffer for a soft smear instead of a hard-edged tail. The legacy top-level
`"decay"` key still works and is exactly `"trail": {"decay": ...}`. Trail
blur is circular (wraps at index 0/n) so it looks correct on ring scenes
where n-1 is physically adjacent to 0 — unlike master.blur, which does not
wrap (see color-note in scene.py).
"""
from __future__ import annotations

import math

import numpy as np

from .color import Palette, hex_to_linear
from .noise import fbm_2d
from .physics import ParticleSystem


def _gaussian_kernel(sigma: float) -> np.ndarray:
    r = max(1, int(math.ceil(sigma * 3)))
    x = np.arange(-r, r + 1, dtype=np.float64)
    k = np.exp(-(x * x) / (2 * sigma * sigma))
    return (k / k.sum()).astype(np.float32)


def _circular_blur(arr: np.ndarray, kernel: np.ndarray) -> np.ndarray:
    """1-D convolution of an (n,3) buffer with wraparound edges."""
    r = len(kernel) // 2
    n = arr.shape[0]
    if r >= n:
        r = n - 1
    padded = np.concatenate([arr[-r:], arr, arr[:r]], axis=0) if r > 0 else arr
    out = np.empty_like(arr)
    for c in range(arr.shape[1]):
        out[:, c] = np.convolve(padded[:, c], kernel, mode="valid" if r > 0 else "same")
    return out


class Layer:
    def __init__(self, cfg: dict, n: int):
        self.cfg = cfg
        self.n = n
        self.blend = cfg.get("blend", "add")
        self.opacity = float(cfg.get("opacity", 1.0))
        trail = cfg.get("trail") or {}
        self.decay = float(trail.get("decay", cfg.get("decay", 0.0)))
        self.trail_blur = float(trail.get("blur", 0.0))
        self._persist = np.zeros((n, 3), dtype=np.float32) if self.decay > 0 else None
        self._trail_kernel = (_gaussian_kernel(self.trail_blur)
                              if self._persist is not None and self.trail_blur > 0 else None)

    def render(self, t: float, dt: float):
        """Return (rgb (n,3) float32 linear, alpha (n,) float32 or None)."""
        raise NotImplementedError

    def composite(self, dst: np.ndarray, t: float, dt: float):
        rgb, alpha = self.render(t, dt)
        if self._persist is not None:
            self._persist *= self.decay
            if self._trail_kernel is not None:
                self._persist[:] = _circular_blur(self._persist, self._trail_kernel)
            np.maximum(self._persist, rgb, out=self._persist)
            rgb = self._persist
        op = self.opacity
        if self.blend == "boost":
            # Relative brightening lens: dst *= 1 + rgb*opacity. A white
            # gaussian splat at opacity 0.5 makes whatever is underneath 50%
            # brighter at its center (taking on the local hue) and fades to
            # no-op at its edges. Pure black stays black.
            dst *= 1.0 + rgb * op
        elif self.blend == "max":
            np.maximum(dst, rgb * op, out=dst)
        elif self.blend == "alpha":
            if alpha is None:
                alpha = np.ones(self.n, dtype=np.float32)
            a = np.clip(alpha * op, 0.0, 1.0)[:, None]
            dst[:] = rgb * a + dst * (1.0 - a)
        else:  # add
            dst += rgb * op


class ParticleLayer(Layer):
    def __init__(self, cfg: dict, n: int, seed: int | None = None):
        super().__init__(cfg, n)
        self.system = ParticleSystem(
            n,
            physics=cfg.get("physics"),
            emitters=cfg.get("emitters"),
            initial=cfg.get("particles"),
            springs=cfg.get("springs"),
            seed=seed,
            palette=cfg.get("palette"),
            palette_speed=cfg.get("palette_speed", 0.0),
        )
        r = cfg.get("render") or {}
        self.falloff = r.get("falloff", "gaussian")
        self.fade_out = bool(r.get("fade_out", True))

    def render(self, t: float, dt: float):
        self.system.step(dt, t)
        rgb = np.zeros((self.n, 3), dtype=np.float32)
        self.system.render(rgb, falloff=self.falloff, fade_out=self.fade_out, t=t)
        alpha = np.clip(rgb.max(axis=1), 0.0, 1.0)
        return rgb, alpha


class WaveLayer(Layer):
    """Sum of traveling sinusoids. Intensity maps a color or scrolls a palette."""

    def __init__(self, cfg: dict, n: int, **_):
        super().__init__(cfg, n)
        self.waves = cfg.get("waves") or [{"wavelength": 200, "speed": 60, "amplitude": 1.0}]
        self.palette = Palette(cfg["palette"]) if cfg.get("palette") else None
        self.palette_speed = float(cfg.get("palette_speed", 0.0))
        self.color = hex_to_linear(cfg.get("color", "#4060ff"))
        self.bias = float(cfg.get("bias", 0.0))
        self.xs = np.arange(n, dtype=np.float64)

    def render(self, t: float, dt: float):
        v = np.full(self.n, self.bias)
        for w in self.waves:
            wl = max(1.0, float(w.get("wavelength", 200)))
            sp = float(w.get("speed", 60))
            amp = float(w.get("amplitude", 1.0))
            ph = float(w.get("phase", 0.0))
            v += amp * np.sin(2 * math.pi * ((self.xs - sp * t) / wl) + ph)
        inten = np.clip((v + 1.0) * 0.5, 0.0, None)  # remap to >= 0
        if self.palette:
            u = inten * 0.999 + self.palette_speed * t
            rgb = self.palette.lookup(u) * np.clip(inten, 0, 1)[:, None]
        else:
            rgb = self.color[None, :] * inten[:, None]
        return rgb.astype(np.float32), np.clip(inten, 0, 1).astype(np.float32)


class NoiseLayer(Layer):
    """Scrolling 1-D fBm noise field mapped through a palette (lava, embers...)."""

    def __init__(self, cfg: dict, n: int, **_):
        super().__init__(cfg, n)
        self.scale = max(1.0, float(cfg.get("scale", 80.0)))
        self.speed = float(cfg.get("speed", 0.3))
        self.drift = float(cfg.get("drift", 0.0))  # px/s spatial scroll
        self.octaves = int(cfg.get("octaves", 3))
        self.palette = Palette(cfg.get("palette") or ["#000000", "#ff4000", "#ffc040"])
        self.palette_speed = float(cfg.get("palette_speed", 0.0))
        self.brightness = float(cfg.get("brightness", 1.0))
        self.contrast = float(cfg.get("contrast", 1.0))
        self.threshold = float(cfg.get("threshold", 0.0))
        self.seed = float(cfg.get("seed", 7.0))
        self.xs = np.arange(n, dtype=np.float64)

    def render(self, t: float, dt: float):
        x = (self.xs + self.drift * t) / self.scale
        v = fbm_2d(x, t * self.speed, octaves=self.octaves, seed=self.seed)
        v = np.clip((v - 0.5) * self.contrast + 0.5, 0.0, 1.0)
        if self.threshold > 0:
            v = np.where(v >= self.threshold, (v - self.threshold) / max(1e-6, 1 - self.threshold), 0.0)
        u = v * 0.999 + self.palette_speed * t
        rgb = self.palette.lookup(u) * (v[:, None] * self.brightness)
        return rgb.astype(np.float32), np.clip(v, 0, 1).astype(np.float32)


class PlasmaLayer(Layer):
    def __init__(self, cfg: dict, n: int, **_):
        super().__init__(cfg, n)
        self.scale = max(1.0, float(cfg.get("scale", 100.0)))
        self.speed = float(cfg.get("speed", 1.0))
        self.palette = Palette(cfg.get("palette") or ["#ff00ff", "#00ffff", "#4000ff"])
        self.palette_speed = float(cfg.get("palette_speed", 0.0))
        self.brightness = float(cfg.get("brightness", 1.0))
        self.xs = np.arange(n, dtype=np.float64)

    def render(self, t: float, dt: float):
        s, sp = self.scale, self.speed * t
        v = (np.sin(self.xs / s + sp)
             + np.sin(self.xs / (s * 0.53) - sp * 1.3)
             + np.sin((self.xs + 40 * math.sin(sp * 0.7)) / (s * 1.71)))
        u = (v / 3.0 + 1.0) * 0.5 + self.palette_speed * t
        rgb = self.palette.lookup(u) * self.brightness
        return rgb.astype(np.float32), np.ones(self.n, dtype=np.float32)


class GradientLayer(Layer):
    """Scrollable palette gradient across the strip."""

    def __init__(self, cfg: dict, n: int, **_):
        super().__init__(cfg, n)
        self.palette = Palette(cfg.get("palette") or ["#000030", "#8020ff", "#000030"])
        self.palette_speed = float(cfg.get("palette_speed", 0.0))
        self.scroll_speed = float(cfg.get("scroll_speed", 0.0))  # px/s
        self.scale = max(1.0, float(cfg.get("scale", n)))        # px per palette cycle
        self.brightness = float(cfg.get("brightness", 1.0))
        self.xs = np.arange(n, dtype=np.float64)

    def render(self, t: float, dt: float):
        u = (self.xs + self.scroll_speed * t) / self.scale + self.palette_speed * t
        rgb = self.palette.lookup(u) * self.brightness
        return rgb.astype(np.float32), np.ones(self.n, dtype=np.float32)


class SolidLayer(Layer):
    def __init__(self, cfg: dict, n: int, **_):
        super().__init__(cfg, n)
        self.color = hex_to_linear(cfg.get("color", "#000000"))

    def render(self, t: float, dt: float):
        rgb = np.broadcast_to(self.color, (self.n, 3)).astype(np.float32).copy()
        return rgb, np.ones(self.n, dtype=np.float32)


class PaintLayer(Layer):
    """Explicit per-pixel assignment: {"index": "#rrggbb"}. The 'any LED any
    color' guarantee. background=null -> only listed pixels are touched."""

    def __init__(self, cfg: dict, n: int, **_):
        cfg.setdefault("blend", "alpha")
        super().__init__(cfg, n)
        bg = cfg.get("background", "#000000")
        self.rgb = np.zeros((n, 3), dtype=np.float32)
        self.alpha = np.zeros(n, dtype=np.float32)
        if bg is not None:
            self.rgb[:] = hex_to_linear(bg)
            self.alpha[:] = 1.0
        for k, v in (cfg.get("pixels") or {}).items():
            i = int(k)
            if 0 <= i < n:
                self.rgb[i] = hex_to_linear(v)
                self.alpha[i] = 1.0

    def render(self, t: float, dt: float):
        return self.rgb, self.alpha


LAYER_TYPES = {
    "particles": ParticleLayer,
    "wave": WaveLayer,
    "noise": NoiseLayer,
    "plasma": PlasmaLayer,
    "gradient": GradientLayer,
    "solid": SolidLayer,
    "paint": PaintLayer,
}


def make_layer(cfg: dict, n: int, seed: int | None = None) -> Layer:
    ltype = cfg.get("type")
    cls = LAYER_TYPES.get(ltype)
    if cls is None:
        raise ValueError(f"unknown layer type: {ltype!r}")
    if cls is ParticleLayer:
        return cls(cfg, n, seed=seed)
    return cls(cfg, n)
