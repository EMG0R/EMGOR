"""
combo.py — the secret-handshake detector.

The arcade console is toggled by holding the **arcade button (b1)** and the
**joystick click (jb)** down together for ~1 second. That single gesture both
*opens* the console (from synth mode) and *closes* it (from console mode), so
the detector runs on the raw Teensy frame at all times, independent of where
input is currently routed.

Why b1 + jb specifically: during gameplay only the joystick *axes* and the
arcade button are used; the joystick *click* is otherwise unused, so combining
it with the arcade button is a gesture that never happens by accident while
playing. See ../docs/ARCHITECTURE.md.

This module is pure logic (no hardware, no I/O) so it is unit-testable — see
../tests/test_combo.py.
"""

from __future__ import annotations


class ComboDetector:
    """Edge-detects 'both buttons held for >= hold_seconds'.

    Feed it `update(both_pressed, now)` every frame. It returns True exactly
    once per qualifying hold — on the frame the hold threshold is first crossed
    — then stays False until both buttons are released and re-held. That
    one-shot behaviour is what makes a single sustained press a clean toggle
    rather than a rapid-fire flip-flop.
    """

    def __init__(self, hold_seconds: float = 1.0):
        self.hold_seconds = hold_seconds
        self._down_since: float | None = None
        self._fired = False
        # Must observe a "both released" frame before the FIRST toggle can fire.
        # On serial port-open / supervisor startup the Teensy can deliver a stale
        # frame with b1+jb already set; without this guard that would auto-enter
        # the console (stopping the face UI) right after a restart. Arming on a
        # release makes the combo require a genuine fresh press.
        self._armed = False

    def update(self, both_pressed: bool, now: float) -> bool:
        if not both_pressed:
            # either button released — arm, and reset; next hold must start fresh
            self._armed = True
            self._down_since = None
            self._fired = False
            return False

        # both pressed
        if not self._armed:
            return False  # ignore a held state we never saw released (stale/startup)
        if self._down_since is None:
            # rising edge — start the clock
            self._down_since = now
            self._fired = False
        elif not self._fired and (now - self._down_since) >= self.hold_seconds:
            self._fired = True
            return True
        return False

    def reset(self) -> None:
        self._down_since = None
        self._fired = False
        self._armed = False
