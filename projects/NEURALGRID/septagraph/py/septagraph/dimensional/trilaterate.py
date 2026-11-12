"""Hand position from however many rangefinders can see it.

Each sensor sits at a known point on the rim and reports a distance, which
constrains the hand to a circle around it. Three circles meet at a point. Using
opposing PAIRS instead -- two sensors per axis -- throws the other two readings
away and gets the geometry only roughly right, because a pair's ratio is only
the true position when the hand is on the line between them.

Degrades on purpose: three or more solves, two falls back to the pair, one
reports that it barely knows anything rather than inventing a position.
"""
from __future__ import annotations

from typing import Optional

#: Where each rangefinder sits on the rim, in floor coordinates. They are at the
#: CENTRE of each edge, not at the corners.
SENSOR_AT = {
    "n": (0.5, 0.0),
    "s": (0.5, 1.0),
    "w": (0.0, 0.5),
    "e": (1.0, 0.5),
}

#: Gauss-Newton steps. The geometry is small and well behaved; more is wasted.
ITERATIONS = 12
STEP = 0.5

SIGMA_TRILATERATED = 0.07
SIGMA_PAIR = 0.22
SIGMA_SINGLE = 0.55

#: Above this mean mismatch the distances describe no single point, so the
#: solve is fiction and the pair estimate is the honest answer.
MAX_RESIDUAL = 0.18


def _clamp01(v: float) -> float:
    return 0.0 if v < 0.0 else 1.0 if v > 1.0 else v


#: Inward direction from each sensor, toward the far side of the floor.
_INWARD = {"n": (0.0, 1.0), "s": (0.0, -1.0), "w": (1.0, 0.0), "e": (-1.0, 0.0)}


def _single(pos: str, d: float) -> tuple:
    """One reading: the hand is out in front of that sensor, at about that
    distance. Weak, but it is not NO information -- returning the centre made
    sweeping over one sensor park the point in the middle."""
    sx, sy = SENSOR_AT[pos]
    ix, iy = _INWARD[pos]
    return (_clamp01(sx + ix * d), _clamp01(sy + iy * d))


def _pair_fallback(dist: dict) -> tuple:
    """Opposing-pair ratio where a pair exists, straight out where one does."""
    if len(dist) == 1:
        pos, d = next(iter(dist.items()))
        return _single(pos, d)

    def axis(a, b):
        da, db = dist.get(a), dist.get(b)
        if da is not None and db is not None:
            total = da + db
            return 0.5 if total <= 0 else _clamp01(da / total)
        return None

    y, x = axis("n", "s"), axis("w", "e")
    # a lone sensor on an axis still says roughly where along it the hand is
    if x is None:
        for p in ("w", "e"):
            if p in dist:
                x = _single(p, dist[p])[0]
                break
    if y is None:
        for p in ("n", "s"):
            if p in dist:
                y = _single(p, dist[p])[1]
                break
    return (0.5 if x is None else x, 0.5 if y is None else y)


def in_plane(dist: dict, height: float) -> dict:
    """Distances with the height leg removed.

    A rangefinder on the rim measures the hypotenuse from itself to the hand;
    height is one leg of that triangle. Solving the plane with the hypotenuse
    is what made a rising hand appear to move inward -- the solver had nowhere
    to put the extra length except sideways.
    """
    z = max(0.0, float(height))
    out = {}
    for pos, d in dist.items():
        if d is None:
            continue
        flat = float(d) ** 2 - z * z
        out[pos] = flat ** 0.5 if flat > 0.0 else 0.0
    return out


def residual(point, dist: dict) -> float:
    """Mean mismatch between the solved point and the measured distances.

    A large residual means the readings do not describe any single point -- the
    sensors are seeing different things, or the scale is wrong -- and the
    solution is then worse than not solving at all.
    """
    x, y = point
    out = 0.0
    for pos, d in dist.items():
        sx, sy = SENSOR_AT[pos]
        out += abs(((x - sx) ** 2 + (y - sy) ** 2) ** 0.5 - d)
    return out / max(len(dist), 1)


def solve_xyz(dist: dict) -> tuple:
    """({pos: distance in floor units}) -> ((x, y, z), sigma) or (None, 1.0).

    Solves in THREE dimensions. The rangefinders sit on the rim and measure
    straight-line distance, so a raised hand is further from ALL of them --
    and a solver with nowhere to put height can only explain that as moving
    away across the floor, which is exactly what walked a hand on the right
    side leftward as it lifted.
    """
    usable = {p: float(d) for p, d in dist.items() if p in SENSOR_AT and d is not None}
    if not usable:
        return None, 1.0
    if len(usable) < 3:
        # Two spheres intersect in a CIRCLE, not a point: height is then
        # unconstrained and simply absorbs whatever noise is going, which
        # measured as roughly 3x the jitter of the flat answer. Below three
        # sensors the flat solve is the honest one.
        x, y = _pair_fallback(usable)
        return (
            (x, y, 0.0),
            SIGMA_SINGLE if len(usable) == 1 else SIGMA_PAIR,
        )

    # Seed from the flat solve, then let height take up the slack.
    seed, _ = solve_xy(usable)
    x, y = seed if seed else (0.5, 0.5)
    z = 0.1
    for _ in range(ITERATIONS * 2):
        gx = gy = gz = 0.0
        for pos, d in usable.items():
            sx, sy = SENSOR_AT[pos]
            dx, dy, dz = x - sx, y - sy, z
            r = (dx * dx + dy * dy + dz * dz) ** 0.5
            if r < 1e-6:
                continue
            res = r - d
            gx += res * dx / r
            gy += res * dy / r
            gz += res * dz / r
        n = len(usable)
        x -= STEP * gx / n
        y -= STEP * gy / n
        z -= STEP * gz / n
        x, y = _clamp01(x), _clamp01(y)
        z = max(0.0, min(1.0, z))     # the hand is above the plane, not below

    if len(usable) == 3:
        return (x, y, z), SIGMA_TRILATERATED * 1.6
    return (x, y, z), SIGMA_TRILATERATED


def solve_xy(dist: dict) -> tuple:
    """({pos: distance in floor units}) -> ((x, y), sigma) or (None, 1.0)."""
    usable = {p: float(d) for p, d in dist.items() if p in SENSOR_AT and d is not None}
    if not usable:
        return None, 1.0
    if len(usable) == 1:
        return _pair_fallback(usable), SIGMA_SINGLE

    # Two ADJACENT sensors pin a point (two circles intersect); only an
    # opposing pair leaves an axis genuinely unconstrained.
    opposing = {"n", "s"} <= set(usable) or {"w", "e"} <= set(usable)
    if len(usable) == 2 and opposing:
        return _pair_fallback(usable), SIGMA_PAIR

    # Gauss-Newton from the pair estimate, which is already close enough that
    # this converges in a handful of steps.
    x, y = _pair_fallback(usable)
    for _ in range(ITERATIONS):
        gx = gy = 0.0
        for pos, d in usable.items():
            sx, sy = SENSOR_AT[pos]
            dx, dy = x - sx, y - sy
            r = (dx * dx + dy * dy) ** 0.5
            if r < 1e-6:
                continue
            # residual: how much further the hand is than this sensor says
            res = r - d
            gx += res * dx / r
            gy += res * dy / r
        n = len(usable)
        x -= STEP * gx / n
        y -= STEP * gy / n
        x, y = _clamp01(x), _clamp01(y)

    # If the readings do not agree on any single point, the fit is fiction.
    # Fall back rather than reporting a confident invention.
    if residual((x, y), usable) > MAX_RESIDUAL:
        return _pair_fallback(usable), SIGMA_PAIR
    if len(usable) >= 4:
        return (x, y), SIGMA_TRILATERATED
    if len(usable) == 3:
        return (x, y), SIGMA_TRILATERATED * 1.6
    return (x, y), SIGMA_PAIR


__all__ = [
    "solve_xy", "solve_xyz", "in_plane", "residual", "SENSOR_AT", "MAX_RESIDUAL",
    "SIGMA_TRILATERATED", "SIGMA_PAIR", "SIGMA_SINGLE",
]
