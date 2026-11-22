"""Per-pad FSR calibration -- the program that gets re-run every housing open.

It lights one pad, you hit it, it records how all four force sensors answered,
and it moves to the next by itself. Sixty-four pads, no button pressing: the
release advances it, so the whole pass is a couple of minutes of playing rather
than a chore you talk yourself out of.

The result is a 64x4 **response profile**: what a press at that spot does to
each sensor. The four FSRs are unevenly placed under the plate, so the same
force at the top-left and the bottom-right produce wildly different raw totals.
With the profile, force stops depending on where you hit.

This is measured, not modelled. It absorbs uneven placement, backer flex and
stiffness variation for free, which is why it beats deriving anything from
sensor positions -- and why it has to be re-run whenever the assembly changes.

Three separate things, easy to confuse:

    resting offsets   the zero point                  (calibrate.py, step 1)
    sensor map        which strip/sensor is which     (run once, hardcoded)
    pad profile       THIS -- every time it is opened
"""
from __future__ import annotations

import json
import os
from typing import Optional

from .sweep import analyze_sweep

GRID_W = 8
GRID_H = 8
N_PADS = GRID_W * GRID_H
N_FSR = 4

#: A press has to move at least one sensor this far above rest to count. Below
#: it the pad is recorded as "unfelt" rather than silently calibrated to noise.
MIN_RESPONSE = 25

#: Steady samples watched before the resting load is believed.
REST_SAMPLES = 8

CUE = (150, 150, 255)


def _median(xs):
    o = sorted(xs)
    return o[len(o) // 2]


class FsrPadSession:
    """Walks the grid pad by pad. Fed by on_key / on_analog, read by result()."""

    def __init__(self, rest=(), order=None):
        self.rest = tuple(int(v) for v in rest)
        self._rest_samples: list[tuple[int, ...]] = []
        self.order = list(order) if order is not None else list(range(N_PADS))
        self._i = 0
        self._profile: dict[int, tuple[int, ...]] = {}
        self.unfelt: list[int] = []
        self._holding: Optional[int] = None
        #: every sample of the current hold, not just its peak -- the sweep is
        #: the measurement, and one sample at maximum flex is the worst of it.
        self._sweep: list[tuple[int, ...]] = []
        self.quality: dict[int, dict] = {}
        #: pad -> every sample of its sweep. The raw evidence: without it a
        #: rejected pad cannot be diagnosed, and the response can never be
        #: plotted or refitted with a better model.
        self.curves: dict[int, list] = {}

    # ---- position --------------------------------------------------------
    @property
    def phase(self) -> str:
        """"rest" while learning the housing's pre-load, then "pads"."""
        return "pads" if self.rest else "rest"

    @property
    def current_pad(self) -> Optional[int]:
        if self.phase == "rest":
            return None
        return self.order[self._i] if self._i < len(self.order) else None

    @property
    def complete(self) -> bool:
        return self.phase == "pads" and self._i >= len(self.order)

    @property
    def total(self) -> int:
        return len(self.order)

    @property
    def done(self) -> int:
        return len(self._profile)

    def skip(self) -> None:
        self._holding = None
        self._i = min(self._i + 1, len(self.order))

    def goto(self, pad: int) -> None:
        """Back to a pad, dropping whatever it had. A bad hit must not stick."""
        if pad not in self.order:
            raise KeyError(pad)
        self._profile.pop(pad, None)
        self.quality.pop(pad, None)
        self.curves.pop(pad, None)
        if pad in self.unfelt:
            self.unfelt.remove(pad)
        self._holding = None
        self._i = self.order.index(pad)

    # ---- input -----------------------------------------------------------
    def _rest_at(self, i: int) -> int:
        return self.rest[i] if i < len(self.rest) else 0

    def on_key(self, x: int, y: int, pressed: bool) -> None:
        if self.phase == "rest":
            return  # nothing measurable until the zero point is known
        pad = int(y) * GRID_W + int(x)
        if pad != self.current_pad:
            return  # follow the light; stray hits are not measurements
        if pressed:
            self._holding = pad
            self._sweep = []
            return
        if self._holding != pad:
            return
        self._holding = None
        self.curves[pad] = list(self._sweep)
        r = analyze_sweep(self._sweep)
        if r.is_sweep and max(r.column) >= MIN_RESPONSE:
            self._profile[pad] = r.column
            self.quality[pad] = {
                "span": r.span,
                "peak": r.peak,
                "stability": round(r.stability, 3),
                "hysteresis": round(r.hysteresis, 3),
                "samples": r.n_samples,
                "reason": None,
            }
            if pad in self.unfelt:
                self.unfelt.remove(pad)
        else:
            self.quality[pad] = {
                "span": r.span,
                "peak": r.peak,
                "stability": round(r.stability, 3),
                "hysteresis": round(r.hysteresis, 3),
                "samples": r.n_samples,
                "reason": r.reason,
            }
            # Either no sensor felt it (dead pad, or a spot the plate does not
            # transmit) or it was a stab rather than a sweep, which says
            # nothing about the force curve. Both need re-doing, not silently
            # calibrating.
            if pad not in self.unfelt:
                self.unfelt.append(pad)
        self._i = min(self._i + 1, len(self.order))

    def on_analog(self, values) -> None:
        fsr_now = list(values)[-N_FSR:] if len(values) >= N_FSR else list(values)
        if self.phase == "rest":
            self._rest_samples.append(tuple(int(v) for v in fsr_now[:N_FSR]))
            if len(self._rest_samples) >= REST_SAMPLES:
                self.rest = tuple(
                    _median([s[i] for s in self._rest_samples])
                    for i in range(N_FSR)
                )
            return
        if self._holding is None:
            return
        self._sweep.append(
            tuple(
                max(0, int(raw) - self._rest_at(i))
                for i, raw in enumerate(fsr_now[:N_FSR])
            )
        )

    # ---- output ----------------------------------------------------------
    def result(self) -> dict:
        return dict(self._profile)

    def state(self) -> dict:
        return {
            "phase": self.phase,
            "rest": list(self.rest),
            "rest_samples": len(self._rest_samples),
            "rest_needed": REST_SAMPLES,
            "pad": self.current_pad,
            "x": None if self.current_pad is None else self.current_pad % GRID_W,
            "y": None if self.current_pad is None else self.current_pad // GRID_W,
            "done": self.done,
            "total": self.total,
            "complete": self.complete,
            "holding": self._holding is not None,
            "peak": list(self._sweep[-1]) if self._sweep else [0] * N_FSR,
            "span": (
                max(sum(r) for r in self._sweep) - min(sum(r) for r in self._sweep)
                if self._sweep else 0
            ),
            "quality": self.quality.get(self.current_pad or -1),
            "unfelt": list(self.unfelt),
            "calibrated": sorted(self._profile),
            "quality_all": self.quality,
            "curve": list(self._sweep) if self._holding is not None
                     else self.curves.get(self.current_pad or -1, []),
        }

    def led_frame(self) -> list[tuple[int, int, int]]:
        frame = [(0, 0, 0)] * N_PADS
        pad = self.current_pad
        if pad is not None:
            frame[pad] = CUE
        return frame

    # ---- persistence -----------------------------------------------------
    def to_dict(self) -> dict:
        return {
            "v": 1,
            "rest": list(self.rest),
            "pads": {str(k): list(v) for k, v in self._profile.items()},
            "quality": {str(k): v for k, v in self.quality.items()},
            "curves": {str(k): [list(r) for r in v] for k, v in self.curves.items()},
            "unfelt": list(self.unfelt),
        }

    def save(self, path) -> str:
        path = str(path)
        tmp = path + ".tmp"
        with open(tmp, "w") as fh:
            json.dump(self.to_dict(), fh, indent=2)
            fh.flush()
            os.fsync(fh.fileno())
        os.replace(tmp, path)
        return path

    @staticmethod
    def _read(path) -> dict:
        try:
            with open(str(path)) as fh:
                return json.load(fh)
        except (OSError, ValueError):
            return {}

    @classmethod
    def load_profile(cls, path) -> dict:
        d = cls._read(path)
        return {int(k): tuple(int(x) for x in v) for k, v in d.get("pads", {}).items()}

    @classmethod
    def load_curves(cls, path) -> dict:
        d = cls._read(path)
        return {
            int(k): [tuple(int(x) for x in row) for row in v]
            for k, v in d.get("curves", {}).items()
        }

    @classmethod
    def load_rest(cls, path) -> tuple:
        return tuple(int(v) for v in cls._read(path).get("rest", ()))


def force_from_profile(above_rest, pad: int, profile: dict) -> Optional[float]:
    """Force at ``pad``, relative to the reference press recorded there.

    1.0 means "as hard as the press this pad was calibrated with". Returns None
    for a pad with no profile, so the caller can fall back to the plain sum
    rather than silently inventing a number.
    """
    ref = profile.get(pad)
    if not ref:
        return None
    ref_total = sum(ref)
    if ref_total <= 0:
        return None
    return sum(max(0, int(v)) for v in above_rest) / float(ref_total)


__all__ = [
    "FsrPadSession", "force_from_profile",
    "GRID_W", "GRID_H", "N_PADS", "N_FSR", "MIN_RESPONSE",
]
