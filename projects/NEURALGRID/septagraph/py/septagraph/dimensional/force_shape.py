"""Shaping the force value: a compressive curve, then smoothing.

Two distinct jobs.

**Curve.** Raw force is roughly linear in sensor counts, which is not how
pressing feels. The light end deserves most of the resolution: you can press
meaningfully lighter than "medium", but you cannot press meaningfully harder
than "hard". So the map is ``x ** exponent`` with exponent < 1 -- steep where
it matters, flattening where it does not, and the hardest presses change least.

**Smoothing.** Asymmetric on purpose: fast on the way up so a hit still feels
immediate, slow on the way down so sustained pressure does not shimmer. This is
monophonic aftertouch -- a value ridden continuously -- and the sensors on this
instrument wander several hundred counts even at rest, so some damping is not
a nicety.

Order matters: smooth in the LINEAR force domain, then curve. Curving first
would make the smoother's time constant depend on how hard you are pressing.
"""
from __future__ import annotations

from typing import Optional

#: Default compression. 1.0 is a straight line; lower lifts the light end and
#: flattens the hardest presses. 0.35 was too eager -- a light press leapt
#: straight to bright -- so this is only gently compressive now. Tunable per
#: instrument via Calibration.force_curve; a by-feel number, not a derived one.
DEFAULT_CURVE = 0.7

#: EMA coefficients at the ~42fps analog rate.
#:
#: ATTACK is deliberately near-transparent: 0.75 reaches 90% of a step in about
#: two frames (~32ms), which is below the threshold where added smoothing reads
#: as lag. Smoothing the RISE is what you would feel; smoothing the fall is
#: what actually removes jitter, so the asymmetry is the whole trick.
ATTACK = 0.75
RELEASE = 0.12

#: What a calibration-strength press reads. Deliberately short of 1.0: you
#: calibrate at a comfortable press, so without headroom normal playing pins at
#: full scale and there is nothing left above it.
HEADROOM = 0.72


def clamp01(x: float) -> float:
    return 0.0 if x < 0.0 else 1.0 if x > 1.0 else float(x)


def apply_curve(x: float, exponent: float = DEFAULT_CURVE) -> float:
    """Compressive response. Endpoints are fixed at 0 and 1."""
    x = clamp01(x)
    e = float(exponent)
    if e <= 0:
        return x
    return x ** e


class ForceSmoother:
    """Asymmetric exponential smoothing, one value."""

    def __init__(self, attack: float = ATTACK, release: float = RELEASE):
        self.attack = float(attack)
        self.release = float(release)
        self._v: Optional[float] = None

    def reset(self) -> None:
        self._v = None

    @property
    def value(self) -> float:
        return self._v or 0.0

    def push(self, x: float) -> float:
        x = clamp01(x)
        if self._v is None:
            self._v = x          # never lag the first reading of a new press
            return self._v
        a = self.attack if x > self._v else self.release
        self._v += (x - self._v) * a
        return self._v


__all__ = ["apply_curve", "ForceSmoother", "DEFAULT_CURVE", "ATTACK", "RELEASE"]
