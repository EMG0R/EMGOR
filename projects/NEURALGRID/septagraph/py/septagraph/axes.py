"""Axis semantics v2 (spec §A / docs/AXIS_SEMANTICS_v2.md).

Nine coordinate indices, each a float in [0, 1], perceptually linear.

v2 makes space a point on a sphere. v1's `az` spanned only -90..+90, so
the back half of the room was unreachable, and there was no elevation at
all -- space was a flat half-plane, which ambisonics and VBAP cannot be
driven from. `el` is appended LAST rather than inserted beside `az`, so
indices 0..7 keep their meaning and existing catalog data stays valid.
`az` widening to a full circle DOES change what an old az value means:
see docs/AXIS_SEMANTICS_v2.md for the remap.
The default physical maps below are what a renderer uses when it declares
nothing else; renderers own the map to physical units.
"""
import math

AXES = ("freq", "loud", "harm", "rate", "scale", "az", "spread", "dist", "el")
K = len(AXES)


def clamp01(x: float) -> float:
    return 0.0 if x < 0.0 else 1.0 if x > 1.0 else float(x)


def to_physical(idx: int, k: float) -> float:
    """Default map from 0-1 coordinate to physical units for index ``idx``."""
    k = clamp01(k)
    if idx == 0:
        return 20.0 * (1000.0 ** k)  # Hz, 20 -> 20k
    if idx == 1:
        return -60.0 + 60.0 * k  # dBFS
    if idx == 3:
        return 0.5 * (128.0 ** k)  # Hz, 0.5 -> 64
    if idx == 4:
        return 0.25 * (32.0 ** k)  # cyc/oct, 0.25 -> 8
    if idx == 5:
        return -180.0 + 360.0 * k  # deg, full circle; 0.5 = straight ahead
    if idx == 8:
        return -90.0 + 180.0 * k  # deg, -90 below -> +90 above
    return k  # harm, spread, dist are already the physical fraction


def from_physical(idx: int, v: float) -> float:
    """Inverse of :func:`to_physical`, clamped to [0, 1]."""
    if idx == 0:
        return clamp01(math.log(v / 20.0, 1000.0))
    if idx == 1:
        return clamp01((v + 60.0) / 60.0)
    if idx == 3:
        return clamp01(math.log(v / 0.5, 128.0))
    if idx == 4:
        return clamp01(math.log(v / 0.25, 32.0))
    if idx == 5:
        return clamp01((v + 180.0) / 360.0)
    if idx == 8:
        return clamp01((v + 90.0) / 180.0)
    return clamp01(v)
