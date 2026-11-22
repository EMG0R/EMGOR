"""preset_gesture.py — hold-timing state machine for NEPTR preset capture/delete.

Shared, side-effect-free logic used by the on-device PRESETS page (touch-hold)
and available to any surface that can measure a real press duration:

    hold  <5s, release      -> SELECT (normal tap: recall that preset)
    hold >=5s and <8s, rel. -> CAPTURE (save current full state into the slot)
    hold >=8s               -> DELETE  (fires at 8s; capture is NOT also fired)

Unambiguous by construction: DELETE is terminal at 8s, so holding through the
capture window to 8s deletes without first capturing. The tracker also exposes a
0..1 progress fraction toward the next threshold so the UI can draw a ring.

This module does NO I/O and imports nothing heavy, so it unit-tests cleanly and
can be imported by the pygame UI without side effects. The occupancy helper
(which shells out to demiurge-preset) is separate and optional.
"""
from __future__ import annotations
from dataclasses import dataclass

CAPTURE_S = 5.0
DELETE_S  = 8.0


def classify(held: float, capture_s: float = CAPTURE_S,
             delete_s: float = DELETE_S) -> str:
    """Resolve an outcome purely from a hold DURATION (seconds). Lets a surface
    that can't tick every frame (e.g. the UI main loop blocks on OSC) decide the
    whole gesture at finger-up: <5s SELECT, 5..8s CAPTURE, >=8s DELETE."""
    if held >= delete_s:
        return "delete"
    if held >= capture_s:
        return "capture"
    return "select"


@dataclass
class HoldState:
    slot: int
    start: float
    fired_delete: bool = False


class PresetHold:
    """Track a single active hold. Feed monotonic timestamps."""

    def __init__(self, capture_s: float = CAPTURE_S, delete_s: float = DELETE_S):
        self.capture_s = capture_s
        self.delete_s = delete_s
        self._st: HoldState | None = None

    @property
    def active(self) -> bool:
        return self._st is not None

    @property
    def slot(self) -> int:
        return self._st.slot if self._st else 0

    def start(self, slot: int, t: float) -> None:
        self._st = HoldState(slot=slot, start=t)

    def update(self, t: float) -> str | None:
        """Call each frame while held. Returns 'delete' once at 8s, else None."""
        st = self._st
        if st is None or st.fired_delete:
            return None
        if t - st.start >= self.delete_s:
            st.fired_delete = True
            return "delete"
        return None

    def release(self, t: float) -> str | None:
        """Returns 'select' | 'capture' | None (None if delete already fired)."""
        st = self._st
        self._st = None
        if st is None:
            return None
        if st.fired_delete:
            return None
        held = t - st.start
        if held >= self.capture_s:
            return "capture"
        return "select"

    def cancel(self) -> None:
        self._st = None

    def progress(self, t: float) -> tuple[str, float]:
        """(stage, frac) for UI. stage in {'idle','arming','capture_ready',
        'delete_ready'}; frac is 0..1 toward the next threshold."""
        st = self._st
        if st is None:
            return ("idle", 0.0)
        held = t - st.start
        if held < self.capture_s:
            return ("arming", held / self.capture_s)
        if held < self.delete_s:
            span = self.delete_s - self.capture_s
            return ("capture_ready", (held - self.capture_s) / span)
        return ("delete_ready", 1.0)


# --------------------------------------------------------- occupancy (optional)
def load_occupancy(context: str, cli: str = "demiurge-preset") -> dict[int, dict]:
    """Return {slot: {'used': bool, 'label': str}} by asking demiurge-preset.
    Returns {} on any failure so the UI degrades gracefully."""
    import json
    import subprocess
    try:
        out = subprocess.run([cli, "-c", context, "list", "--json"],
                             capture_output=True, text=True, timeout=2)
        data = json.loads(out.stdout)
        return {r["slot"]: {"used": r["used"], "label": r.get("label", "")}
                for r in data.get("slots", [])}
    except Exception:
        return {}
