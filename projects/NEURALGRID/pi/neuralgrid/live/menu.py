"""Menu base class for the neuralGrid live layer.

A "menu" is one screen/mode of the instrument, selected by which SoftPot
was last touched (see live/engine.py). Each menu is self-contained and
independently testable: it only ever touches the grid through the 64-cell
`fb` list passed to render(), and reaches MIDI/clock/link through
`self.engine` (a LiveEngine, or anything duck-typing its `midi_out`,
`link`, `clock` attributes -- the headless selftest injects fakes there).

Subclasses override whichever hooks they care about; every hook has a
harmless no-op default so a minimal menu (see menu_placeholder.py) can't
crash the engine.
"""

from __future__ import annotations

from typing import List, Tuple

RGB = Tuple[int, int, int]

GRID_W = 8
GRID_H = 8


class Menu:
    name = "menu"

    def __init__(self, engine):
        self.engine = engine

    # -- lifecycle ---------------------------------------------------

    def on_enter(self) -> None:
        """Called when this menu becomes active (its softpot was touched)."""

    def on_exit(self) -> None:
        """Called when a different menu becomes active. Subclasses that
        hold MIDI notes/gates should release them here so switching menus
        never leaves a stuck note."""

    # -- input ---------------------------------------------------------

    def on_key(self, x: int, y: int, pressed: bool) -> None:
        """A KEY edge on the grid while this menu is active."""

    def on_analog(self, channel: int, reading) -> None:
        """A TouchDetector ChannelReading for `channel`, on every ANALOG
        tick, regardless of which channel is currently "owning" menu
        selection. Most menus ignore this."""

    # -- clock -----------------------------------------------------------

    def on_clock_step(self, step: int) -> None:
        """Fired by Clock every `pulses_per_step` pulses (16th notes)."""

    def on_clock_downbeat(self) -> None:
        """Fired by Clock every `pulses_per_downbeat` pulses."""

    # -- per-frame ---------------------------------------------------------

    def tick(self, now: float) -> None:
        """Called once per engine loop iteration (up to ~60fps) with a
        time.monotonic() timestamp. Use this for anything time-based that
        isn't driven by clock steps (e.g. sequencer note-gate timeouts)."""

    def render(self, fb: List[RGB]) -> None:
        """Write this menu's full visual state into `fb` (list of 64
        (r,g,b) logical-RGB tuples, index i -> x=i%8, y=i//8). Menus should
        write every cell they own each call (not just deltas) so switching
        menus never leaves stale pixels from a previous menu."""

    @staticmethod
    def clear(fb: List[RGB]) -> None:
        for i in range(GRID_W * GRID_H):
            fb[i] = (0, 0, 0)
