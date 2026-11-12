"""The menu -- how you change modes on an instrument with no screen.

Hold all four corners for MENU_HOLD_S and the grid becomes its own menu:
one lit pad per destination, each wearing that mode's colour, so the
legend IS the thing you press.

The gesture had to be chosen against one that already existed: holding
both corners of ONE edge rotates which edge is "top" (orientation.py).
Four corners contains four such pairs, so without arbitration every trip
to the menu would also spin the grid. CornerWatch owns that arbitration
and nothing else -- it is pure bookkeeping over press/release times so it
can be tested without a grid attached.
"""
from __future__ import annotations

from typing import Callable, Dict, Optional, Tuple

from . import leds
from .modes import Mode

#: Hold all four corners this long. Shorter than orientation's 0.8s so
#: that when both gestures could apply, the menu is the one that lands.
MENU_HOLD_S = 0.6

Coord = Tuple[int, int]

#: Where each mode lives, in LOGICAL coordinates (x left->right, y
#: top->bottom), exactly as the user laid them out.
MENU_SLOTS: Dict[Coord, str] = {
    (0, 0): "blank",        # top left -- back to the default controller
    (0, 1): "osc",          # below it -- generic OSC, the monome's opposite
    (7, 0): "direct_lab",   # top right -- the machine lab
    (7, 1): "modulettes",   # the square below it
    (7, 7): "encoder",      # bottom right -- the dimensional encoder
    (0, 7): "monome",       # bottom left -- a monome 64
}

#: One colour per mode, used both for its menu pad and as that mode's own
#: identity on the grid. Pressing a menu pad should look like the place
#: you are about to arrive at.
MODE_COLORS: Dict[str, leds.RGB] = {
    "blank":      (60, 170, 255),    # light blue -- the plain controller
    "direct_lab": (255, 120, 0),     # amber -- the machine lab's own hue
    "modulettes": (0, 230, 140),     # teal-green
    "encoder":    (255, 60, 200),    # magenta -- the dimensional encoder
    "monome":     (255, 225, 90),    # the monome white/yellow
    "osc":        (0, 220, 210),     # cyan -- anything that speaks OSC
}


class CornerWatch:
    """Which corners are down, since when, and what that permits.

    Deliberately not a gesture *detector* that fires callbacks: the app
    polls it from the LED loop (holding still generates no key events, so
    a purely event-driven check would never notice a completed hold).
    """

    CORNERS: frozenset = frozenset({(0, 0), (7, 0), (0, 7), (7, 7)})

    def __init__(self) -> None:
        self._down: Dict[Coord, float] = {}
        self._armed = True
        # The most corners held at once since the grid was last clean.
        # Rotation keys off this rather than off what is down RIGHT NOW,
        # because lifting off from four passes through two corners of an
        # edge that have by then been held well past the rotate
        # threshold -- so letting go of the menu span the grid.
        self._peak = 0

    def press(self, coord: Coord, t: float) -> None:
        if coord in self.CORNERS:
            self._down[coord] = float(t)
            self._peak = max(self._peak, len(self._down))

    def release(self, coord: Coord) -> None:
        if self._down.pop(coord, None) is not None:
            # The set is broken, so whatever fired last is spent.
            self._armed = True
        if not self._down:
            self._peak = 0   # every finger off: a clean slate

    @property
    def n_held(self) -> int:
        return len(self._down)

    def all_down(self) -> bool:
        return len(self._down) == len(self.CORNERS)

    def menu_ready(self, t: float) -> bool:
        """True once every corner has been down together for MENU_HOLD_S.

        The clock runs from the LAST corner pressed, not the first --
        otherwise a hand resting on one corner silently primes the
        gesture and the menu appears as soon as the other three land.
        """
        if not self._armed or not self.all_down():
            return False
        return (float(t) - max(self._down.values())) >= MENU_HOLD_S

    def consume(self) -> None:
        """Called once the menu has opened; re-arms only on a release."""
        self._armed = False

    def blocks_rotation(self) -> bool:
        """Whether the rotate-the-grid gesture must stand down.

        Three corners is already enough: any three contain an edge pair,
        so a hand on its way to the fourth would otherwise spin the grid
        out from under the menu it was reaching for.

        And once three have been down, rotation stays blocked until every
        finger is off -- not merely while three are still held. Lifting
        off from four goes 4 -> 3 -> 2, and that last pair is an edge
        pair whose press timestamps are long past the rotate threshold,
        so it would fire the instant the third finger left the grid.
        """
        return self._peak >= 3


class MenuMode(Mode):
    """The grid as its own mode picker. Inert apart from the five pads."""

    name = "menu"

    def __init__(self, choose: Callable[[str], None]) -> None:
        self.choose = choose
        self.last_choice: Optional[str] = None

    def on_key(self, x: int, y: int, edge: int) -> None:
        if not edge:
            return
        target = MENU_SLOTS.get((int(x), int(y)))
        if target is None:
            return
        self.last_choice = target
        self.choose(target)

    # The instrument's sensors say nothing while you are choosing: no
    # notes, no MIDI, no lab traffic escapes the menu.
    def on_analog(self, raw) -> None:
        return

    def on_lidar(self, mm4) -> None:
        return

    def led_frame(self):
        frame = leds.blank()
        for (x, y), mode_name in MENU_SLOTS.items():
            frame[leds.idx(x, y)] = MODE_COLORS[mode_name]
        return frame

    def panic(self) -> None:
        return

    def state(self) -> dict:
        return {
            "slots": {f"{x},{y}": name for (x, y), name in MENU_SLOTS.items()},
            "colors": {n: leds.to_hex(c) for n, c in MODE_COLORS.items()},
            "last_choice": self.last_choice,
        }


__all__ = ["MENU_SLOTS", "MODE_COLORS", "MENU_HOLD_S", "MenuMode", "CornerWatch"]
