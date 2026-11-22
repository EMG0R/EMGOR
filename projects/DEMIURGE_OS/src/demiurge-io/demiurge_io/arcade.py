"""arcade.py — what the NEPTR arcade button (b1) means right now.

TWO ROLES, ONE BUTTON. On NEPTR the arcade button is TAP TEMPO (default: every
press is a tempo tap for demiurge-clock, exactly what serial_bridge.py's
ch16 CC113 did). When a USS instrument is active its manifest's hard map
makes the same button its TRIGGER (press = trig gate). This module is the
tiny state machine that decides which role is live. It lives in the pool, not
in Csound: the engine never sees the button either way.

  mode "tap"   b1 maps tagged  "arcade":"tap"  are evaluated; "trig" maps are not.
  mode "trig"  the reverse.

Rules (all measured by tests/test_neptr.py):
  * No USS active (no live map tagged "arcade":"trig") -> mode is "tap".
  * A USS instrument becomes active -> mode "trig". It goes inactive -> "tap".
  * LONG-HOLD (default 2.0 s, still down) flips the role while a USS is active:
    trig -> tap (so the player can tap a tempo without leaving the instrument),
    and a second long hold flips it back. One flip per hold; the release after
    a flip does nothing. A hold with no USS active is just a long tap.
  * Hold is measured from the press edge on the same monotonic clock as the
    readings, polled by the daemon loop (tick), so it fires WHILE held.

Pure (time is passed in), no I/O.
"""

from __future__ import annotations

from typing import Optional

ARCADE_SOURCE = "/hw/teensy/b1"
HOLD_S = 2.0
TAP, TRIG = "tap", "trig"


class ArcadeRouter:
    def __init__(self, hold_s: float = HOLD_S):
        self.hold_s = hold_s
        self.mode = TAP
        self.uss_active = False
        self._down_t: Optional[float] = None
        self._flipped = False

    def set_uss_active(self, active: bool) -> Optional[str]:
        """Called when the effective map set changes. Returns the new mode if it
        changed, else None."""
        active = bool(active)
        if active == self.uss_active:
            return None
        self.uss_active = active
        new = TRIG if active else TAP
        if new == self.mode:
            return None
        self.mode = new
        return new

    def on_edge(self, down: bool, t: float) -> None:
        if down:
            self._down_t = t
            self._flipped = False
        else:
            self._down_t = None
            self._flipped = False

    def tick(self, now: float) -> Optional[str]:
        """Poll while held. Returns the new mode on the tick the hold completes."""
        if (self._down_t is None or self._flipped or not self.uss_active
                or now - self._down_t < self.hold_s):
            return None
        self._flipped = True
        self.mode = TAP if self.mode == TRIG else TRIG
        return self.mode

    def allows(self, map_cfg: dict) -> bool:
        """May this map run in the current mode? Maps without "arcade" always may."""
        want = map_cfg.get("arcade")
        return want is None or want == self.mode
