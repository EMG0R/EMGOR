"""Aftertouch and strike velocity, both referenced to the note's own onset.

Measuring pressure against a long-lived "resting plate" level was a workaround
for sensor drift. Referencing each note to its OWN onset removes the problem
instead: the reference is captured milliseconds before it is used, so drift over
minutes -- 2133 counts to 240 on this hardware -- cannot reach it. It also
cancels whatever the plate was already carrying, so a hand leaning on the grid
or another pad held simply does not appear.

One continuous value: pressure above the level the note settled at. "Velocity"
and "aftertouch" are the same quantity here, not two measurements.

The window matters. The dome switch closes partway through the travel and the
plate is still settling when the key event arrives, so the hold reference is
the QUIETEST moment shortly after onset, not a single sample: a single sample
lands somewhere random on the strike transient.
"""
from __future__ import annotations

from typing import Optional

N_FSR = 4

#: How far back to look for the plate's level just before a strike.
PRE_S = 0.06

#: The strike transient lives in this window after the key event.
STRIKE_S = 0.05

#: The hold reference is the quietest moment in this window after onset --
#: past the strike, before any deliberate lean-in.
SETTLE_FROM_S = 0.05
SETTLE_TO_S = 0.18


class OnsetForce:
    """Per-press references, fed the same analog stream as everything else."""

    def __init__(self):
        self._history: list = []          # (t, (r0..r3))
        self._onset_t: Optional[float] = None
        self._pre: tuple = ()
        self._peak: tuple = ()
        self._hold: Optional[tuple] = None

    # ---- stream ------------------------------------------------------
    def observe(self, fsr, t: float) -> None:
        row = tuple(int(v) for v in fsr[:N_FSR])
        self._history.append((t, row))
        cutoff = t - max(PRE_S, SETTLE_TO_S) * 3
        while self._history and self._history[0][0] < cutoff:
            self._history.pop(0)

        if self._onset_t is None:
            return
        since = t - self._onset_t
        if since <= STRIKE_S:
            self._peak = tuple(
                max(a, b) for a, b in zip(self._peak or row, row)
            )
        if SETTLE_FROM_S <= since <= SETTLE_TO_S:
            self._hold = tuple(
                min(a, b) for a, b in zip(self._hold or row, row)
            )

    # ---- press boundaries --------------------------------------------
    def begin(self, t: float) -> None:
        """A note started. Freeze what the plate was doing just before it."""
        self._onset_t = t
        self._pre = self._level_before(t)
        self._peak = ()
        self._hold = None

    def end(self) -> None:
        self._onset_t = None
        self._pre = ()
        self._peak = ()
        self._hold = None

    @property
    def active(self) -> bool:
        return self._onset_t is not None

    def _level_before(self, t: float) -> tuple:
        """The quietest the plate was in the moments before the strike."""
        window = [row for ts, row in self._history if t - PRE_S <= ts <= t]
        if not window:
            return (0,) * N_FSR
        return tuple(min(r[i] for r in window) for i in range(N_FSR))

    # ---- outputs -----------------------------------------------------
    def strike(self) -> Optional[tuple]:
        """Peak above the pre-strike level, per sensor, once the window has
        passed. None until there is something to report."""
        if not self._peak or not self._pre:
            return None
        return tuple(max(0, p - q) for p, q in zip(self._peak, self._pre))

    def aftertouch(self, fsr) -> Optional[tuple]:
        """Pressure above the settled hold, per sensor."""
        if self._onset_t is None:
            return None
        ref = self._hold or self._peak or self._pre
        if not ref:
            return None
        return tuple(
            max(0, int(v) - r) for v, r in zip(tuple(fsr)[:N_FSR], ref)
        )


__all__ = ["OnsetForce", "PRE_S", "STRIKE_S", "SETTLE_FROM_S", "SETTLE_TO_S"]
