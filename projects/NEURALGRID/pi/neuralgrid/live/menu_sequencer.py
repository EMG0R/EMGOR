"""Menu B -- "Sequencer": an 8-track x 8-step step sequencer demo.

Grid layout reuses the 8x8 pad grid directly: column x = step 0..7, row y =
track 0..7. Pressing a pad toggles that cell (press-edge only; release is
ignored so you can't un-toggle by lifting your finger off a still-held
pad). The grid starts completely empty -- silence until the human programs
a pattern.

The playhead column advances by one step every time live/clock.py fires an
on_clock_step() callback (see clock.py: that's every `pulses_per_step`
MIDI clock pulses, 16th notes at 24 PPQN by default, with an internal
fallback tempo if no external MIDI clock is present). Entering a new
playhead column fires a MIDI note_on for every lit cell in that column
(one note per row-track) with a short, fixed gate; note_off follows a
fixed number of milliseconds later, checked in tick() against
time.monotonic() rather than blocking the engine loop.

Track -> note mapping: a two-octave minor pentatonic scale, one note per
row, so an untrained demo pattern still sounds musical instead of atonal.
Row 7 (bottom) is the lowest note, row 0 (top) the highest, matching the
"bottom = low pitch" convention used in menu_instrument.py.

Rendering: lit-but-not-under-playhead cells show a dim per-row color;
lit-and-under-playhead cells show the same hue at full brightness so the
"hit" is obviously brighter; the playhead column itself is dimly washed
across every cell (even empty ones) so its motion across the grid is
visible even on an empty pattern. Phase 3: colors come from the ACTIVE
config's led_palette + led_brightness (config.palette_color), same as
menu_instrument, so a config swap re-colors both menus consistently.
"""

from __future__ import annotations

import time
from typing import List, Tuple

from .. import config as config_mod
from .menu import GRID_H, GRID_W, Menu, RGB

# Two-octave A minor pentatonic, row 7 (bottom) lowest -> row 0 (top) highest.
SCALE = [69, 65, 62, 60, 57, 53, 50, 45]  # index = row 0..7 (top..bottom)... see note_for
GATE_SECONDS = 0.12
MIDI_CHANNEL = 1

PLAYHEAD_WASH = 45  # background level on the playhead column -- bright enough
                    # that the sweep is clearly visible on an empty pattern


def note_for_row(y: int) -> int:
    # SCALE is authored top(index0)->bottom low to keep the literal list
    # readable low->high top-to-bottom would be backwards, so index from
    # the bottom: row 7 (bottom) -> SCALE[0] (lowest), row 0 (top) -> SCALE[7].
    return SCALE[GRID_H - 1 - y]


def _row_color(y: int, bright: float, cfg) -> RGB:
    return config_mod.palette_color(cfg["led_palette"], y / GRID_H, bright)


class MenuSequencer(Menu):
    name = "sequencer"

    def __init__(self, engine):
        super().__init__(engine)
        self.grid = [[False] * GRID_W for _ in range(GRID_H)]  # grid[y][x]
        self.playhead = 0
        # row -> (note, off_at monotonic time)
        self._gated: dict = {}

    def on_exit(self) -> None:
        for note in list(v[0] for v in self._gated.values()):
            self.engine.midi_out.note_off(note, ch=MIDI_CHANNEL)
        self._gated.clear()

    def on_key(self, x: int, y: int, pressed: bool) -> None:
        if not (0 <= x < GRID_W and 0 <= y < GRID_H):
            return
        if not pressed:
            return
        self.grid[y][x] = not self.grid[y][x]

    def on_clock_step(self, step: int) -> None:
        self.playhead = step % GRID_W
        note_out = self.engine.midi_out
        now = time.monotonic()
        for y in range(GRID_H):
            if self.grid[y][self.playhead]:
                note = note_for_row(y)
                # Re-trigger cleanly if this row was already gated (e.g.
                # very fast tempo / short cycle).
                if y in self._gated:
                    note_out.note_off(self._gated[y][0], ch=MIDI_CHANNEL)
                note_out.note_on(note, 105, ch=MIDI_CHANNEL)
                self._gated[y] = (note, now + GATE_SECONDS)

    def tick(self, now: float) -> None:
        done = [y for y, (note, off_at) in self._gated.items() if now >= off_at]
        for y in done:
            note, _ = self._gated.pop(y)
            self.engine.midi_out.note_off(note, ch=MIDI_CHANNEL)

    def render(self, fb: List[RGB]) -> None:
        self.clear(fb)
        cfg = self.engine.config
        global_bright = cfg["led_brightness"]
        for y in range(GRID_H):
            for x in range(GRID_W):
                lit = self.grid[y][x]
                on_playhead = x == self.playhead
                if lit and on_playhead:
                    color = _row_color(y, global_bright, cfg)
                elif lit:
                    color = _row_color(y, global_bright * 0.35, cfg)
                elif on_playhead:
                    wash = int(PLAYHEAD_WASH * global_bright)
                    color = (wash, wash, wash)
                else:
                    continue
                fb[y * GRID_W + x] = color
