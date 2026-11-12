"""Menu A -- "Instrument": press a pad, get a lit cell + a MIDI note.

Phase 3: note mapping, velocity, MIDI channel, and LED coloring are all
driven by the ACTIVE config (see neuralgrid/config.py and
docs/CONFIG_SCHEMA.md) instead of hard-coded constants, so the model layer
(Phase 4+) -- or, for now, live/stager_manual.py -- can reconfigure this
menu on the downbeat. `note_for`/`velocity_for` take an optional `cfg`
(defaulting to config.default_config()) so this module stays independently
testable/callable with no engine at all.

Four note_layouts (config.NOTE_LAYOUTS), all built from a common base note
`12*(octave+1) + root_note`:

  - 0 chromatic: whole grid walks chromatically, row-major (bottom-left
    lowest, wrapping up-and-right one semitone per pad).
  - 1 rows_in_fourths (Phase 2's original/default formula):
    note = base + x + 5*(7-y) -- each row up is a perfect fourth (5
    semitones) above the row below, isomorphic Launchpad-"Fourths" style.
  - 2 scale_degrees: x indexes a degree into the selected scale
    (config.SCALES[scale_id]), y indexes octave (row 7 lowest).
  - 3 isomorphic: a second isomorphic layout (whole-tone right, fifth up)
    distinct from rows_in_fourths, for contrast when the config swaps.

Color by row uses config.palette_color(led_palette, y/8, led_brightness),
full brightness while held, off otherwise -- so led_palette/led_brightness
swaps are visible here too.
"""

from __future__ import annotations

from typing import Dict, List, Tuple

from .. import config as config_mod
from .menu import GRID_H, GRID_W, Menu, RGB

# Kept only as the *fallback* module-level defaults baked into
# config.default_config() (octave=2, root_note=0 -> base 36 == old
# BASE_NOTE; velocity_fixed=100/127 -> old VELOCITY=100); no longer used
# directly by on_key/render, which always read the active config.


def _base_note(cfg: Dict) -> int:
    return 12 * (cfg["octave"] + 1) + cfg["root_note"]


def note_for(x: int, y: int, cfg: Dict = None) -> int:
    if cfg is None:
        cfg = config_mod.default_config()
    base = _base_note(cfg)
    layout = cfg["note_layout"]

    if layout == 0:  # chromatic, row-major across the whole grid
        note = base + (GRID_H - 1 - y) * GRID_W + x
    elif layout == 2:  # scale_degrees: x = degree, y = octave (row 7 lowest)
        intervals = config_mod.SCALES[cfg["scale_id"]]["intervals"]
        degree = x % len(intervals)
        octave_row = GRID_H - 1 - y
        note = base + octave_row * 12 + intervals[degree]
    elif layout == 3:  # isomorphic: whole-tone right, fifth up
        note = base + x * 2 + (GRID_H - 1 - y) * 7
    else:  # 1, or any out-of-range fallback: rows_in_fourths (Phase 2 default)
        note = base + x + 5 * (GRID_H - 1 - y)

    return max(0, min(127, note))


def velocity_for(x: int, y: int, cfg: Dict = None) -> int:
    if cfg is None:
        cfg = config_mod.default_config()
    curve = cfg["velocity_curve"]
    ceiling = cfg["velocity_fixed"] * 127.0

    if curve == 1:  # row-scaled: top (y=0) loudest
        v = ceiling * ((GRID_H - y) / GRID_H)
    elif curve == 2:  # col-scaled: right (x=7) loudest
        v = ceiling * ((x + 1) / GRID_W)
    elif curve == 3:  # accent every 4th column
        v = ceiling if x % 4 == 0 else ceiling * 0.7
    else:  # 0, or any out-of-range fallback: fixed
        v = ceiling

    return max(1, min(127, int(round(v))))


def _row_color(y: int, cfg: Dict) -> RGB:
    return config_mod.palette_color(cfg["led_palette"], y / GRID_H, cfg["led_brightness"])


class MenuInstrument(Menu):
    name = "instrument"

    def __init__(self, engine):
        super().__init__(engine)
        # (x, y) -> (note, channel), for cells currently held down -- keep
        # both so a config swap mid-hold still releases the *original*
        # note/channel it was struck with, never a note-off on the wrong
        # note if the active config changes before the key is released.
        self._held: Dict[Tuple[int, int], Tuple[int, int]] = {}

    def on_exit(self) -> None:
        for note, channel in list(self._held.values()):
            self.engine.midi_out.note_off(note, ch=channel)
        self._held.clear()

    def on_key(self, x: int, y: int, pressed: bool) -> None:
        if not (0 <= x < GRID_W and 0 <= y < GRID_H):
            return
        cfg = self.engine.config
        if pressed:
            note = note_for(x, y, cfg)
            channel = cfg["channel"]
            vel = velocity_for(x, y, cfg)
            self._held[(x, y)] = (note, channel)
            self.engine.midi_out.note_on(note, vel, ch=channel)
        else:
            held = self._held.pop((x, y), None)
            if held is not None:
                note, channel = held
            else:
                note, channel = note_for(x, y, cfg), cfg["channel"]
            self.engine.midi_out.note_off(note, ch=channel)

    # Idle pads glow faintly so the grid always looks like a live, playable
    # surface (never a dead black screen); held pads jump to full brightness.
    IDLE_LEVEL = 0.14

    def render(self, fb: List[RGB]) -> None:
        self.clear(fb)
        cfg = self.engine.config
        bright = cfg["led_brightness"]
        # faint per-row wash across the whole grid = visible playable layout
        for y in range(GRID_H):
            idle = config_mod.palette_color(cfg["led_palette"], y / GRID_H, bright * self.IDLE_LEVEL)
            if idle != (0, 0, 0):
                base = y * GRID_W
                for x in range(GRID_W):
                    fb[base + x] = idle
        # held pads full brightness on top
        for (x, y) in self._held:
            fb[y * GRID_W + x] = _row_color(y, cfg)
