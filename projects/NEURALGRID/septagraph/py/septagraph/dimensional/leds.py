"""LED frame helpers for the dimensional service."""
from __future__ import annotations

from typing import Optional

N_LEDS = 64
GRID_W = 8

#: Above this many changed pixels a full frame is cheaper than listing them.
PARTIAL_MAX = 24


def clamp_frame(frame) -> Optional[list]:
    """Validate anything handed in as an LED frame.

    Anything can drive the grid, so anything can hand it nonsense. A bad frame
    must be refused, never forwarded -- the link is shared with the sensors.
    """
    try:
        rows = list(frame)
    except TypeError:
        return None
    if len(rows) != N_LEDS:
        return None
    out = []
    for px in rows:
        try:
            r, g, b = px
        except (TypeError, ValueError):
            return None
        out.append(tuple(max(0, min(255, int(v))) for v in (r, g, b)))
    return out


def led_diff(old, new) -> Optional[list]:
    """Changed pixels as (x, y, r, g, b), or None when a full send is needed."""
    if old is None or len(old) != len(new):
        return None
    changed = [
        (i % GRID_W, i // GRID_W, *new[i])
        for i in range(len(new))
        if old[i] != new[i]
    ]
    return None if len(changed) > PARTIAL_MAX else changed


__all__ = ["clamp_frame", "led_diff", "N_LEDS", "PARTIAL_MAX"]
