"""model.py — the tiny shared vocabulary every layer speaks.

Kept deliberately separate from any one source/sink so conditioning.py and
router.py can be imported (and unit-tested) without pulling in termios,
sockets, or anything else that touches hardware.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import NamedTuple, Optional


class Reading(NamedTuple):
    """One sample from a source, in the source's native (raw) units.

    `t` is a monotonic timestamp (seconds, float) — used by conditioning's
    one-pole smoothing to compute dt, and NOT wall-clock time (which can
    jump on NTP sync and would corrupt the filter).
    """
    source_id: str
    raw: float
    t: float


@dataclass(frozen=True)
class SourceDescriptor:
    """Static facts about a source, supplied by the driver (sources/*.py).

    This is hardware truth (what the ADC/sensor physically produces), as
    opposed to the user-editable calibration in sensors.json (what range the
    player actually wants to use). `dropout_value` is the raw sentinel the
    driver emits when a sample is known-bad (e.g. VL53L1X timeout -> -1);
    None means the source has no such concept and every sample is trusted.
    """
    source_id: str
    label: str
    units: str
    raw_min: float
    raw_max: float
    rate_hz: float
    dropout_value: Optional[float] = None

    # USABLE band, as opposed to the full electrical range above. These differ
    # more often than you'd guess, and defaulting calibration to raw_min/raw_max
    # quietly ruins a sensor: the VL53L1X reports 0..4000 mm but the firmware
    # only treats 20..460 mm as meaningful (TEENSYY.ino tofScale() gates cm to
    # 2..46), so calibrating 0..4000 squeezes the LiDAR's entire real travel
    # into the bottom 11% of its output and the thing feels dead. Same story for
    # the FSRs, where contact doesn't even register below ~2500 counts
    # (FSR_MIN_RAW). None means "same as raw" — most sensors genuinely use their
    # whole range.
    use_min: Optional[float] = None
    use_max: Optional[float] = None

    @property
    def usable(self):
        """(lo, hi) to seed sensors.json calibration with."""
        return (self.raw_min if self.use_min is None else self.use_min,
                self.raw_max if self.use_max is None else self.use_max)
