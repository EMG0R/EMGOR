"""Per-pad velocity when several pads are held at once.

Four force sensors give four equations. Once the response surface is known,
each held pad contributes a *known column* to that system:

    A f = b        A[i][j] = response of sensor i to a press at pad j
                   f[j]    = force at pad j          (what we want)
                   b[i]    = what sensor i reads now

So for up to four simultaneous pads the individual forces can be **solved for**
rather than interpolated. Beyond four the system is underdetermined; the solver
still answers, distributing the load, but says so in sigma instead of inventing
per-pad numbers it cannot support.

This overturns an earlier claim in this package, and the distinction is worth
keeping straight: *uncalibrated*, the hardware genuinely cannot tell which of
two held pads is pressed harder -- the four FSRs are unevenly placed and all
you have is a total. *Calibrated*, it can, because the profile supplies the
mixing matrix that makes the total decomposable.

Forces come out relative to the calibration press: 1.0 means "as hard as the
press this pad was calibrated with".
"""
from __future__ import annotations

from typing import Optional

from .response_surface import _solve

N_SENSORS = 4

#: Ridge term on the normal equations. Two pads with near-identical response
#: make the system ill-conditioned; without this the solve swings wildly
#: between them (a huge positive force on one, negative on the other).
RIDGE = 1e-3

#: Sigma reported for a clean, well-conditioned single-pad solve.
SIGMA_GOOD = 0.06

#: Sigma once there are more pads than sensors -- genuinely underdetermined.
SIGMA_UNDERDETERMINED = 0.6


def _normalized_columns(held, profile) -> tuple[list[int], list[list[float]]]:
    pads, cols = [], []
    for pad in held:
        ref = profile.get(pad)
        if not ref:
            continue  # a pad with no profile contributes nothing knowable
        pads.append(pad)
        cols.append([float(ref[i]) if i < len(ref) else 0.0 for i in range(N_SENSORS)])
    return pads, cols


def _conditioning_sigma(cols: list[list[float]]) -> float:
    """How separable the held pads actually are.

    Two pads whose response vectors point the same way cannot be told apart by
    four sensors, however clean the arithmetic looks.
    """
    worst = 0.0
    for a in range(len(cols)):
        for b in range(a + 1, len(cols)):
            va, vb = cols[a], cols[b]
            na = sum(v * v for v in va) ** 0.5
            nb = sum(v * v for v in vb) ** 0.5
            if na <= 0 or nb <= 0:
                continue
            cos = sum(x * y for x, y in zip(va, vb)) / (na * nb)
            worst = max(worst, cos)
    return SIGMA_GOOD + (1.0 - SIGMA_GOOD) * max(0.0, worst) ** 8


#: Below this expected response the held pads barely register at all, and the
#: projection is dividing by noise.
WEAK_EXPECTED = 400.0

#: A held pad counts as being PLAYED once its direction matches the measurement
#: at least this well, relative to the best-matching held pad. Below it, the
#: finger is resting rather than playing and must not drag the overall down.
PLAYED_FRAC = 0.45


def overall_force(above_rest, held, profile) -> tuple[Optional[float], float]:
    """ONE overall force across however many pads are held.

    Per-pad decomposition needs as many good sensors as pads; with two of four
    FSRs unreliable, a four-pad chord makes that system rank-deficient and the
    answer is noise. But the individual forces were never what was wanted.

    Sum the held pads' calibrated columns into E -- what the sensors would read
    if every held pad were pressed at its reference force -- and project the
    measurement onto it:

        overall = (b . E) / (E . E)

    One unknown, four equations. Massively overdetermined, so it survives a
    dead sensor, is unaffected by how many pads are down, and answers exactly
    the question asked: given these pads are held, how hard overall?

    Returns (None, 1.0) when no held pad has a profile -- better to decline
    than to invent.
    """
    pads, cols = _normalized_columns(held, profile)
    if not pads:
        return None, 1.0

    b = [float(above_rest[i]) if i < len(above_rest) else 0.0 for i in range(N_SENSORS)]

    # Decide which held pads are actually being PLAYED before summing.
    # Projecting onto the plain sum treats a finger merely resting on a pad as
    # if it were playing -- measured at a 59% drop on a hard press with two
    # light fingers down, which is unusable since resting on the grid is
    # normal. Weighting by apparent load instead over-read by ~20%, because
    # scaling the weights shrinks the expectation. So: a GATE, not a weight.
    # A pad counts fully or not at all, which keeps the projection exact when
    # every held pad really is being pressed.
    # Gate on DIRECTION, not magnitude. A projection magnitude explodes for a
    # pad whose column is tiny on the sensors that still work -- measured at 8x
    # with two FSRs dead -- and crosstalk from a hard neighbour makes a resting
    # pad look 31% involved. Cosine is scale-free and asks the right question:
    # does the measurement point the way this pad would push it?
    nb = sum(x * x for x in b) ** 0.5
    involvement = []
    for c in cols:
        nc = sum(v * v for v in c) ** 0.5
        involvement.append(
            max(0.0, sum(x * v for x, v in zip(b, c)) / (nb * nc))
            if nb > 0 and nc > 0 else 0.0
        )
    peak = max(involvement) if involvement else 0.0
    if peak > 0:
        played = [c for c, w in zip(cols, involvement) if w >= peak * PLAYED_FRAC]
    else:
        played = cols   # nothing registers yet; assume they all count

    expected = [sum(c[i] for c in played) for i in range(N_SENSORS)]
    denom = sum(e * e for e in expected)
    if denom <= 0:
        return None, 1.0

    value = max(0.0, sum(x * e for x, e in zip(b, expected)) / denom)

    # Confidence follows how strongly the held pads are expected to register:
    # projecting onto a weak expectation is dividing by noise.
    strength = denom ** 0.5
    sigma = SIGMA_GOOD if strength >= WEAK_EXPECTED else min(
        1.0, SIGMA_GOOD + (1.0 - strength / WEAK_EXPECTED) * 0.5
    )
    return value, sigma


def solve_pad_forces(above_rest, held, profile) -> tuple[dict, float]:
    """Return ({pad: force}, sigma) for the currently held pads."""
    pads, cols = _normalized_columns(held, profile)
    if not pads:
        return {}, 1.0

    b = [float(above_rest[i]) if i < len(above_rest) else 0.0 for i in range(N_SENSORS)]
    n = len(pads)

    if n > N_SENSORS:
        # Underdetermined: four equations cannot resolve five or more unknowns.
        # Split the measured total in proportion to how strongly each pad reads,
        # which is the best available guess, and flag it plainly.
        weights = [sum(c) for c in cols]
        wsum = sum(weights) or 1.0
        total = sum(b)
        return (
            {
                pad: max(0.0, (total * (w / wsum)) / (sum(c) or 1.0))
                for pad, w, c in zip(pads, weights, cols)
            },
            SIGMA_UNDERDETERMINED,
        )

    # Normal equations with a ridge term, then clamp: a negative force is not a
    # thing, and the solve can produce one when two pads nearly overlap.
    ata = [[sum(cols[i][k] * cols[j][k] for k in range(N_SENSORS)) for j in range(n)]
           for i in range(n)]
    scale = max((ata[i][i] for i in range(n)), default=1.0) or 1.0
    for i in range(n):
        ata[i][i] += RIDGE * scale
    atb = [sum(cols[i][k] * b[k] for k in range(N_SENSORS)) for i in range(n)]

    f = _solve(ata, atb)
    if f is None:
        return {pad: 0.0 for pad in pads}, 1.0

    return {pad: max(0.0, v) for pad, v in zip(pads, f)}, _conditioning_sigma(cols)


__all__ = [
    "solve_pad_forces", "overall_force", "N_SENSORS", "WEAK_EXPECTED", "PLAYED_FRAC",
    "SIGMA_GOOD", "SIGMA_UNDERDETERMINED", "RIDGE",
]
