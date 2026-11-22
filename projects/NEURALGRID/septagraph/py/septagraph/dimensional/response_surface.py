"""Smooth the per-pad FSR profile into a response surface.

A rigid plate resting on four point sensors has a response that varies
*smoothly* with position. It is a physical field, not sixty-four independent
numbers. But the measurement comes from sixty-four hand presses, and a hand is
not a consistent actuator -- press pad 12 a little harder than pad 13 and,
without this, that difference becomes permanent calibration.

So: fit a low-order 2D surface through all the samples, per sensor. Six
coefficients per sensor, twenty-four in total, against up to 256 measurements.
Heavily overdetermined, which is precisely why it rejects press-to-press
variation while keeping the real spatial trend -- and why it can fill in pads
that were skipped or that no sensor felt.

Pure Python on purpose (a 6x6 normal-equation solve): this runs on the Pi
inside the calibration service, and adding numpy to that path would be a
dependency for thirty lines of arithmetic.
"""
from __future__ import annotations

GRID_W = 8
GRID_H = 8
N_PADS = GRID_W * GRID_H

#: Quadratic in x and y. Enough to follow a plate's real falloff toward each
#: sensor; too stiff to chase an individual bad press.
N_TERMS = 6

#: Below this many measured pads the fit is not meaningfully overdetermined,
#: and smoothing would invent structure rather than reveal it.
MIN_SAMPLES = 12


def _basis(x: float, y: float) -> list[float]:
    return [1.0, x, y, x * y, x * x, y * y]


def _solve(a: list[list[float]], b: list[float]) -> list[float] | None:
    """Gaussian elimination with partial pivoting. None if singular."""
    n = len(b)
    m = [row[:] + [b[i]] for i, row in enumerate(a)]
    for col in range(n):
        piv = max(range(col, n), key=lambda r: abs(m[r][col]))
        if abs(m[piv][col]) < 1e-12:
            return None
        m[col], m[piv] = m[piv], m[col]
        pv = m[col][col]
        for r in range(n):
            if r == col:
                continue
            f = m[r][col] / pv
            if f:
                for c in range(col, n + 1):
                    m[r][c] -= f * m[col][c]
    return [m[i][n] / m[i][i] for i in range(n)]


def _fit_one(samples: list[tuple[float, float, float]]) -> list[float] | None:
    """Least squares over the basis, via the normal equations."""
    ata = [[0.0] * N_TERMS for _ in range(N_TERMS)]
    atb = [0.0] * N_TERMS
    for x, y, v in samples:
        phi = _basis(x, y)
        for i in range(N_TERMS):
            atb[i] += phi[i] * v
            for j in range(N_TERMS):
                ata[i][j] += phi[i] * phi[j]
    return _solve(ata, atb)


def fit_response_surface(profile: dict, n_sensors: int = 4) -> dict:
    """Measured pad -> 4-vector, smoothed and completed across the whole grid.

    Returns the input untouched when there is too little to fit: better an
    honest sparse profile than a confident invented one.
    """
    if not profile:
        return {}
    if len(profile) < MIN_SAMPLES:
        return dict(profile)

    coeffs = []
    for i in range(n_sensors):
        samples = [
            ((pad % GRID_W) / (GRID_W - 1), (pad // GRID_W) / (GRID_H - 1), float(v[i]))
            for pad, v in sorted(profile.items())
            if i < len(v)
        ]
        c = _fit_one(samples) if samples else None
        if c is None:
            return dict(profile)  # degenerate -- do not guess
        coeffs.append(c)

    out = {}
    for pad in range(N_PADS):
        x = (pad % GRID_W) / (GRID_W - 1)
        y = (pad // GRID_W) / (GRID_H - 1)
        phi = _basis(x, y)
        out[pad] = tuple(
            # A fitted surface can dip below zero at the edges where it is
            # extrapolating; a negative force response is not a thing.
            max(0, int(round(sum(c[k] * phi[k] for k in range(N_TERMS)))))
            for c in coeffs
        )
    return out


__all__ = ["fit_response_surface", "GRID_W", "GRID_H", "N_PADS", "MIN_SAMPLES"]
