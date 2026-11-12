"""Core value types (all frozen dataclasses).

``Vec`` is a tuple of exactly ``K`` (= 8) floats in [0, 1]. A *septagraph*
is not a point: it is a trajectory of (mu, sigma) knots over normalized
time plus a duration. See spec §A / §B.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Optional

from .axes import K, clamp01

Vec = tuple[float, ...]

MAX_KNOTS = 32


def vec(*xs: float) -> Vec:
    """Build a Vec: validates length == K, clamps every value to [0, 1]."""
    if len(xs) == 1 and not isinstance(xs[0], (int, float)):
        xs = tuple(xs[0])  # allow vec(iterable)
    if len(xs) != K:
        raise ValueError(f"Vec needs {K} values, got {len(xs)}")
    return tuple(clamp01(float(x)) for x in xs)


ZERO: Vec = tuple([0.0] * K)
HALF: Vec = tuple([0.5] * K)
ONE: Vec = tuple([1.0] * K)


def _lerp_vec(a: Vec, b: Vec, f: float) -> Vec:
    return tuple(x + (y - x) * f for x, y in zip(a, b))


@dataclass(frozen=True)
class Knot:
    t: float
    mu: Vec
    sigma: Vec

    def __post_init__(self):
        object.__setattr__(self, "t", clamp01(float(self.t)))
        object.__setattr__(self, "mu", vec(*self.mu))
        object.__setattr__(self, "sigma", vec(*self.sigma))


@dataclass(frozen=True)
class Septagraph:
    shape_id: str
    layer: int
    duration_s: float
    knots: tuple[Knot, ...]

    def __post_init__(self):
        knots = tuple(self.knots)
        object.__setattr__(self, "knots", knots)
        object.__setattr__(self, "layer", int(self.layer))
        object.__setattr__(self, "duration_s", max(0.0, float(self.duration_s)))
        n = len(knots)
        if not (1 <= n <= MAX_KNOTS):
            raise ValueError(f"septagraph needs 1..{MAX_KNOTS} knots, got {n}")
        if knots[0].t != 0.0:
            raise ValueError("first knot must have t == 0.0")
        if n > 1 and knots[-1].t != 1.0:
            raise ValueError("last knot must have t == 1.0")
        for a, b in zip(knots, knots[1:]):
            if b.t < a.t:
                raise ValueError("knots must be sorted by t ascending")

    def at(self, t: float) -> tuple[Vec, Vec]:
        """Linear interpolation of (mu, sigma) at normalized time ``t``."""
        t = clamp01(float(t))
        ks = self.knots
        if len(ks) == 1 or t <= ks[0].t:
            return ks[0].mu, ks[0].sigma
        for a, b in zip(ks, ks[1:]):
            if t <= b.t:
                span = b.t - a.t
                f = 0.0 if span <= 0.0 else (t - a.t) / span
                return _lerp_vec(a.mu, b.mu, f), _lerp_vec(a.sigma, b.sigma, f)
        return ks[-1].mu, ks[-1].sigma


@dataclass(frozen=True)
class Hit:
    voice_id: str
    layer: int
    onset: bool
    velocity: float
    mu: Vec
    sigma: Vec

    def __post_init__(self):
        object.__setattr__(self, "layer", int(self.layer))
        object.__setattr__(self, "onset", bool(self.onset))
        object.__setattr__(self, "velocity", clamp01(float(self.velocity)))
        object.__setattr__(self, "mu", vec(*self.mu))
        object.__setattr__(self, "sigma", vec(*self.sigma))


@dataclass(frozen=True)
class Move:
    voice_id: str  # "" = global
    mu: Vec

    def __post_init__(self):
        object.__setattr__(self, "mu", vec(*self.mu))


@dataclass(frozen=True)
class CatalogPoint:
    point_id: int
    mu: Vec
    sigma: Vec
    instrument: str
    max_rate_hz: float
    vel_min: float
    vel_max: float

    def __post_init__(self):
        object.__setattr__(self, "point_id", int(self.point_id))
        object.__setattr__(self, "mu", vec(*self.mu))
        object.__setattr__(self, "sigma", vec(*self.sigma))
        object.__setattr__(self, "max_rate_hz", float(self.max_rate_hz))
        object.__setattr__(self, "vel_min", clamp01(float(self.vel_min)))
        object.__setattr__(self, "vel_max", clamp01(float(self.vel_max)))


@dataclass(frozen=True)
class Manifest:
    renderer_id: str
    kind: str  # "continuous" | "catalog"
    name: str
    lo: Optional[Vec]
    hi: Optional[Vec]
    points: tuple[CatalogPoint, ...]

    def __post_init__(self):
        if self.kind not in ("continuous", "catalog"):
            raise ValueError(f"manifest kind must be 'continuous' or 'catalog', got {self.kind!r}")
        object.__setattr__(self, "lo", None if self.lo is None else vec(*self.lo))
        object.__setattr__(self, "hi", None if self.hi is None else vec(*self.hi))
        object.__setattr__(self, "points", tuple(self.points))


@dataclass(frozen=True)
class ManifestRequest:
    """`/sept/v1/manifest/request` — controller asks renderers to re-announce."""


__all__ = [
    "Vec", "vec", "ZERO", "HALF", "ONE", "MAX_KNOTS",
    "Knot", "Septagraph", "Hit", "Move", "CatalogPoint", "Manifest", "ManifestRequest",
]
