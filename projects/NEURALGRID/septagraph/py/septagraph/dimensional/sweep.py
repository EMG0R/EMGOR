"""What the soft -> hard -> soft sweep is for.

The user performs a full pressure sweep on each pad, deliberately. Keeping only
its peak throws that away: one sample per pad, taken at the single noisiest
moment (maximum flex), with no way to tell whether the sensors' ratio to each
other even holds across the force range.

This matters more than it would for a plain hit velocity, because the force
channel is **monophonic aftertouch** -- a continuous pressure stream the player
rides while holding, not a value sampled once at onset. The calibration has to
describe the whole range that gets traversed, not just its top.

So the sweep yields, per pad:

    column      the sensor mixing ratio, FITTED across every sample
    span        how much force range was actually covered
    stability   whether that ratio holds from soft to hard
    hysteresis  how far the release branch sits from the press branch

``column`` is what the decomposition consumes. Fitting it over the whole sweep
rather than reading it off the peak is the difference between one noisy sample
and a line through twenty.
"""
from __future__ import annotations

from dataclasses import dataclass

N_SENSORS = 4

#: A sample has to carry at least this much total load to say anything about
#: the mixing ratio; below it the shares are mostly noise.
FLOOR = 25

#: A sweep must cover at least this FRACTION of its own peak load. Relative,
#: not absolute: a pad far from every FSR produces a small but perfectly real
#: sweep, and an absolute threshold rejects it for being quiet -- a different
#: thing from being a stab. Observed on hardware as a contiguous block of ten
#: pads failing together, which is a coupling pattern, not a technique problem.
MIN_SPAN_FRAC = 0.45

#: Below this peak load nothing meaningful was felt at all.
MIN_PEAK = 20


@dataclass(frozen=True)
class SweepResult:
    column: tuple
    span: int
    peak: int
    stability: float
    hysteresis: float
    is_sweep: bool
    n_samples: int
    #: None when good; otherwise "too_faint" or "stab" -- so a rejected pad can
    #: be diagnosed instead of just going red.
    reason: str | None = None


def _totals(rows):
    return [sum(max(0, int(v)) for v in r) for r in rows]


def _median(xs):
    o = sorted(xs)
    n = len(o)
    return o[n // 2] if n % 2 else (o[n // 2 - 1] + o[n // 2]) / 2.0


def _reference_load(totals) -> float:
    """The load the column is scaled to: a high percentile, not the max.

    The single hardest instant is also the likeliest to be a bad sample, and
    letting it define "force 1.0" hands that sample control of the pad's whole
    feel. The 90th percentile sits just below the top of a real sweep and does
    not move when one sample spikes.
    """
    if not totals:
        return 0.0
    o = sorted(totals)
    return float(o[min(len(o) - 1, int(0.9 * (len(o) - 1)))])


def _usable(rows, totals):
    """Samples worth fitting: above the noise floor and not in the mushy
    bottom fifth of the sweep, where the shares are mostly quantization."""
    peak = max(totals) if totals else 0
    cut = max(FLOOR, peak * 0.2)
    return [(r, t) for r, t in zip(rows, totals) if t >= cut]


def _fit_column(rows, totals) -> tuple:
    """Sensor shares, taken as the MEDIAN across the sweep.

    Least squares against the total looked natural but weights by force
    squared, so a single bad sample AT PEAK -- the likeliest place for one,
    since that is maximum flex -- dominates the fit. A median over many
    samples ignores it outright, which is the entire reason for sweeping
    instead of reading one press.

    Shares sum to 1, so scaling by the reference load preserves the meaning of
    the column: force 1.0 == as hard as this pad was calibrated.
    """
    usable = _usable(rows, totals)
    if not usable:
        return (0,) * N_SENSORS
    peak = _reference_load(totals)
    shares = [
        _median([(int(r[i]) if i < len(r) else 0) / t for r, t in usable])
        for i in range(N_SENSORS)
    ]
    ssum = sum(shares) or 1.0
    return tuple(max(0, int(round((sh / ssum) * peak))) for sh in shares)


def _stability(rows, totals) -> float:
    """How constant each sensor's share of the total is across the sweep.

    A sensor that saturates, or a pad that shifts which sensor carries it as
    load increases, shows up here -- and means one scalar column cannot fully
    describe that pad.
    """
    usable = _usable(rows, totals)
    if len(usable) < 3:
        return 0.0
    drifts = []
    for i in range(N_SENSORS):
        shares = sorted((int(r[i]) if i < len(r) else 0) / t for r, t in usable)
        mean = sum(shares) / len(shares)
        if mean <= 1e-6:
            continue  # a sensor that feels nothing says nothing about stability
        # Trimmed RANGE, not standard deviation: a share that drifts steadily
        # with force has an unremarkable std but an obvious range, and drift is
        # exactly what breaks a one-scalar-per-sensor column. Trimming the
        # extremes keeps a single noisy sample from reading as drift.
        trimmed = shares[1:-1] if len(shares) >= 5 else shares
        drifts.append((trimmed[-1] - trimmed[0]) / mean)
    if not drifts:
        return 0.0
    # WORST sensor, not the average: one drifting sensor already means a single
    # scalar column cannot describe this pad, and averaging would let three
    # well-behaved sensors hide it.
    return max(0.0, 1.0 - max(drifts))


def _hysteresis(totals) -> float:
    """How far the release branch sits from the press branch.

    Matched by load level, not by time: the two branches visit the same forces
    in opposite order, so comparing them sorted is comparing like with like.
    """
    if len(totals) < 4:
        return 0.0
    peak_at = totals.index(max(totals))
    up = sorted(totals[: peak_at + 1])
    down = sorted(totals[peak_at + 1 :])
    n = min(len(up), len(down))
    if n == 0:
        return 0.0
    up, down = up[:n], down[:n]
    base = sum(up) / n
    if base <= 0:
        return 0.0
    return sum(abs(d - u) for u, d in zip(up, down)) / n / base


def analyze_sweep(rows) -> SweepResult:
    rows = [tuple(r) for r in rows]
    if not rows:
        return SweepResult((0,) * N_SENSORS, 0, 0, 0.0, 0.0, False, 0, "too_faint")

    totals = _totals(rows)
    peak = max(totals)
    span = peak - min(totals)

    if peak < MIN_PEAK:
        reason = "too_faint"
    elif span < peak * MIN_SPAN_FRAC:
        reason = "stab"
    else:
        reason = None

    return SweepResult(
        column=_fit_column(rows, totals),
        span=span,
        peak=peak,
        stability=_stability(rows, totals),
        hysteresis=_hysteresis(totals),
        is_sweep=reason is None,
        n_samples=len(rows),
        reason=reason,
    )


__all__ = [
    "analyze_sweep", "SweepResult", "FLOOR", "MIN_SPAN_FRAC", "MIN_PEAK", "N_SENSORS",
]
