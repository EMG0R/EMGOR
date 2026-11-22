"""Deterministic 1-D + 2-D value noise (fBm), numpy-vectorized, no deps.

Used for noise fields, turbulence and firefly random-walk drift.
"""
from __future__ import annotations

import numpy as np


def _hash2(ix: np.ndarray, iy: np.ndarray, seed: float = 0.0) -> np.ndarray:
    """Lattice hash -> [0,1). Deterministic across runs/platforms enough for art."""
    h = np.sin(ix * 127.1 + iy * 311.7 + seed * 74.7) * 43758.5453123
    return h - np.floor(h)


def _smooth(f: np.ndarray) -> np.ndarray:
    return f * f * (3.0 - 2.0 * f)


def value_noise_2d(x: np.ndarray, y, seed: float = 0.0) -> np.ndarray:
    """Value noise at (x, y). x is an array; y may be scalar or array. Returns [0,1)."""
    x = np.asarray(x, dtype=np.float64)
    y = np.broadcast_to(np.asarray(y, dtype=np.float64), x.shape)
    ix, fx = np.floor(x), x - np.floor(x)
    iy, fy = np.floor(y), y - np.floor(y)
    ux, uy = _smooth(fx), _smooth(fy)
    a = _hash2(ix, iy, seed)
    b = _hash2(ix + 1, iy, seed)
    c = _hash2(ix, iy + 1, seed)
    d = _hash2(ix + 1, iy + 1, seed)
    return (a * (1 - ux) + b * ux) * (1 - uy) + (c * (1 - ux) + d * ux) * uy


def fbm_2d(x: np.ndarray, y, octaves: int = 3, seed: float = 0.0) -> np.ndarray:
    """Fractal brownian motion over value noise, normalized to ~[0,1]."""
    total = np.zeros_like(np.asarray(x, dtype=np.float64))
    amp, freq, norm = 1.0, 1.0, 0.0
    for o in range(max(1, octaves)):
        total += amp * value_noise_2d(x * freq, np.asarray(y) * freq, seed + o * 13.7)
        norm += amp
        amp *= 0.5
        freq *= 2.0
    return total / norm


def signed_noise(x: np.ndarray, y, octaves: int = 2, seed: float = 0.0) -> np.ndarray:
    """fBm remapped to [-1, 1] — handy for turbulence forces."""
    return fbm_2d(x, y, octaves=octaves, seed=seed) * 2.0 - 1.0
