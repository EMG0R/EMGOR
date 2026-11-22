"""Lidar calibration by arcs.

Measured on hardware 2026-09-16: three of the four sensors sit pinned on
something 15-42mm away, dead steady to +/-1mm. They are staring at the housing.
So the wall reading is not noise to be filtered out -- **it is the reference**,
and a hand is whatever departs from it.

Method: two slow arcs over each perceived edge, recording all four sensors
throughout. While you arc over one edge the other three see only wall, so every
arc doubles as a control for the others and the baseline falls out for free.
One pass yields, per sensor:

    baseline   what it reads with no hand (the housing)
    owner      which sensor actually answered for that edge
    bounds     the usable range of hand readings on that sensor

Run once, bake the numbers in with :meth:`ArcResult.as_python`.

The most useful output may be ``usable_mm``: if a sensor has only a couple of
centimetres between wall and hand, that is a mounting problem, and it is better
to read it as a number than to discover it as bad feel later.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Optional

from .encoders.neuralgrid import LIDAR_INVALID, LIDAR_POSITIONS, Calibration

#: Positions a human can see, in the order they are asked about, and the
#: compass key each becomes. Defined here (not in calibrate) so the analysis
#: has no dependency on the step machine that feeds it.
POSITIONS = ("top", "right", "bottom", "left")
POSITION_TO_LIDAR = {"top": "n", "right": "e", "bottom": "s", "left": "w"}

N_LIDAR = 4

#: A reading must depart from the sensor's baseline by at least this much to
#: count as a hand rather than wall jitter (measured jitter is ~1mm).
DEPART_MM = 6

#: Below this much room between the nearest and furthest hand reading, an edge
#: is not playable and the mount needs moving.
MIN_USABLE_MM = 15


def _median(xs):
    s = sorted(xs)
    n = len(s)
    if not n:
        return None
    return s[n // 2] if n % 2 else (s[n // 2 - 1] + s[n // 2]) / 2.0


class ArcRecorder:
    """Collects raw lidar samples per edge, between begin() and end()."""

    def __init__(self):
        self.samples: dict[str, list[tuple[int, ...]]] = {}
        self._active: Optional[str] = None

    def begin(self, position: str) -> None:
        self._active = position
        self.samples.setdefault(position, [])

    def end(self) -> None:
        self._active = None

    @property
    def recording(self) -> bool:
        return self._active is not None

    @property
    def position(self) -> Optional[str]:
        return self._active

    def observe(self, ranges) -> None:
        if self._active is None:
            return
        self.samples[self._active].append(tuple(int(v) for v in ranges))

    def response(self, position: str) -> float:
        """How much any sensor moved during this edge's arcs.

        Live feedback while recording: zero means nothing has answered yet.
        """
        rows = self.samples.get(position) or []
        best = 0.0
        for i in range(N_LIDAR):
            vals = [r[i] for r in rows if i < len(r) and r[i] != LIDAR_INVALID]
            if len(vals) < 2:
                continue
            best = max(best, float(max(vals) - min(vals)))
        return best


@dataclass
class ArcResult:
    baseline: dict = field(default_factory=dict)      # sensor index -> mm
    owner: dict = field(default_factory=dict)         # position -> sensor index
    bounds: dict = field(default_factory=dict)        # position -> (near, far)
    usable_mm: dict = field(default_factory=dict)     # position -> mm of depth
    too_shallow: list = field(default_factory=list)   # positions needing a remount

    def to_calibration(self, base: Optional[Calibration] = None) -> Calibration:
        lidar_map, lidar_bounds = {}, {}
        for position, idx in self.owner.items():
            key = POSITION_TO_LIDAR[position]
            lidar_map[key] = idx
            if position in self.bounds:
                lidar_bounds[key] = self.bounds[position]
        far = max((b[1] for b in lidar_bounds.values()), default=0)

        kwargs = dict(
            softpot=dict(base.softpot) if base else {},
            softpot_map=dict(base.softpot_map) if base else {},
            fsr_rest=tuple(base.fsr_rest) if base else (),
            lidar_map=lidar_map,
            lidar_bounds=lidar_bounds,
        )
        if far:
            kwargs["lidar_max_mm"] = int(far)
        elif base:
            kwargs["lidar_max_mm"] = base.lidar_max_mm
        return Calibration(**kwargs)

    def as_python(self) -> str:
        """Constants to paste into the encoder -- this is a run-once job."""
        lines = [
            "# Lidar calibration, measured by arcs. Re-run only if the sensors move.",
            f"LIDAR_BASELINE = {({k: round(v) for k, v in self.baseline.items()})!r}",
            f"LIDAR_MAP = {({POSITION_TO_LIDAR[p]: i for p, i in self.owner.items()})!r}",
            f"LIDAR_BOUNDS = {({POSITION_TO_LIDAR[p]: b for p, b in self.bounds.items()})!r}",
        ]
        if self.too_shallow:
            lines.append(
                f"# WARNING: too little room to play on {self.too_shallow} "
                f"-- the sensor is too close to the housing; re-aim it."
            )
        return "\n".join(lines) + "\n"


def analyze_arcs(rec: ArcRecorder) -> ArcResult:
    positions = [p for p in POSITIONS if p in rec.samples]
    result = ArcResult()

    # Baseline per sensor, pooled across EVERY arc. A given sensor is only
    # obscured during its own edge's arc -- a minority of all samples -- so the
    # median lands on the wall rather than on the hand.
    for i in range(N_LIDAR):
        vals = [
            r[i]
            for p in positions
            for r in rec.samples[p]
            if i < len(r) and r[i] != LIDAR_INVALID
        ]
        if vals:
            result.baseline[i] = _median(vals)

    # Ownership: for each edge, whichever sensor departed furthest from its own
    # baseline answered. Claimed sensors are removed so one cannot own two.
    claimed: set[int] = set()
    scores = []
    for p in positions:
        for i in range(N_LIDAR):
            if i not in result.baseline:
                continue
            departures = [
                abs(r[i] - result.baseline[i])
                for r in rec.samples[p]
                if i < len(r) and r[i] != LIDAR_INVALID
                and abs(r[i] - result.baseline[i]) >= DEPART_MM
            ]
            if departures:
                scores.append((max(departures), p, i))

    for _, p, i in sorted(scores, reverse=True):
        if p in result.owner or i in claimed:
            continue
        result.owner[p] = i
        claimed.add(i)

    # Bounds from the owner's hand readings only -- the wall is excluded.
    for p, i in result.owner.items():
        hand = [
            r[i]
            for r in rec.samples[p]
            if i < len(r) and r[i] != LIDAR_INVALID
            and abs(r[i] - result.baseline[i]) >= DEPART_MM
        ]
        if not hand:
            continue
        near, far = int(min(hand)), int(max(hand))
        result.bounds[p] = (near, far)
        result.usable_mm[p] = far - near
        if (far - near) < MIN_USABLE_MM:
            result.too_shallow.append(p)

    return result


__all__ = [
    "ArcRecorder", "ArcResult", "analyze_arcs",
    "DEPART_MM", "MIN_USABLE_MM", "POSITIONS", "POSITION_TO_LIDAR",
]
