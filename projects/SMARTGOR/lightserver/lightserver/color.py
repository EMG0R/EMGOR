"""Color utilities: hex <-> linear float, palettes, gamma.

The engine works in *linear* light (float32 HDR). DSL colors are sRGB-ish
hex; we decode with gamma 2.2 and re-encode on output, so a paint color
round-trips to the exact same uint8 value.
"""
from __future__ import annotations

import numpy as np

GAMMA = 2.2


def hex_to_linear(hex_str: str) -> np.ndarray:
    """'#rrggbb' (or 'rrggbb') -> linear float32 rgb in [0,1]."""
    s = hex_str.strip().lstrip("#")
    if len(s) == 3:
        s = "".join(c * 2 for c in s)
    if len(s) != 6:
        raise ValueError(f"bad hex color: {hex_str!r}")
    v = np.array([int(s[i:i + 2], 16) for i in (0, 2, 4)], dtype=np.float64) / 255.0
    return (v ** GAMMA).astype(np.float32)


def linear_frame_to_uint8(hdr: np.ndarray, tone_map: str = "clip") -> np.ndarray:
    """(N,3) linear HDR float -> gamma-corrected uint8 RGB.

    tone_map 'clip': hard clip at 1.0 (identity below 1 -> exact paint colors).
    tone_map 'reinhard': x/(1+x) soft roll-off for very hot scenes.
    """
    x = np.asarray(hdr, dtype=np.float64)
    x = np.maximum(x, 0.0)
    if tone_map == "reinhard":
        x = x / (1.0 + x)
    else:
        x = np.minimum(x, 1.0)
    out = np.rint(255.0 * (x ** (1.0 / GAMMA)))
    return out.astype(np.uint8)


class Palette:
    """Evenly spaced color stops, linearly interpolated, wrapping."""

    def __init__(self, hex_colors: list[str]):
        if not hex_colors:
            hex_colors = ["#ffffff"]
        self.stops = np.stack([hex_to_linear(h) for h in hex_colors])  # (K,3)

    def lookup(self, t: np.ndarray) -> np.ndarray:
        """t: array of positions (any float, wraps mod 1) -> (len(t),3) linear rgb."""
        k = len(self.stops)
        if k == 1:
            return np.broadcast_to(self.stops[0], (len(t), 3)).copy()
        u = (np.asarray(t, dtype=np.float64) % 1.0) * k
        i0 = np.floor(u).astype(int) % k
        i1 = (i0 + 1) % k
        f = (u - np.floor(u)).astype(np.float32)[:, None]
        return (self.stops[i0] * (1 - f) + self.stops[i1] * f).astype(np.float32)

    def sample_scalar(self, t: float) -> np.ndarray:
        return self.lookup(np.array([t]))[0]
