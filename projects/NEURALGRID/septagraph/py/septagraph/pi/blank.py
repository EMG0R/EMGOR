"""BLANK -- the controller the neuralGrid is before you tell it otherwise.

No instrument names, no scale, no catalog: 64 chromatic notes and one
control per sensor, sent as plain MIDI. This is the boot mode, because
booting straight into the Machine Lab meant the grid always came up
committed to one room's worth of robots.

Everything is lit one light blue, so at a glance the instrument reads as
"blank" rather than as whatever it was last used for.
"""
from __future__ import annotations

from typing import Dict, Optional, Tuple

from . import leds
from .modes import Mode

#: Bottom of the chromatic run. 36 = C2, the usual pad-controller home.
BASE_NOTE = 36

#: One control per sensor, none of them sharing. Softpots keep the CCs
#: the old MIDI mode used; force and lidar get their own blocks rather
#: than everything piling onto CC74.
SOFTPOT_CC: Tuple[int, ...] = (1, 2, 3, 4)
FSR_CC: Tuple[int, ...] = (16, 17, 18, 19)
LIDAR_CC: Tuple[int, ...] = (20, 21, 22, 23)

N_FSR = 4
ANALOG_MAX = 4095.0

#: How far a lidar sees before the reading stops meaning anything.
LIDAR_RANGE_MM = 400.0

#: The field the grid sits in when nothing is happening. 64 pads lit
#: together read as one glow rather than 64 colours, and at the level
#: this started at that glow looked white -- so the field is pulled right
#: down: present enough to see the instrument is alive, nowhere near
#: bright enough to be the thing you look at.
#: SATURATED violet, not a pale lavender. A pale colour dimmed down is
#: not a dim colour, it is grey -- (180,140,255) at 0.30 reached the grid
#: as (54,42,76) and read as dim white. Keep green near zero: saturation
#: is what makes this read as purple, brightness only makes it brighter.
FIELD_COLOR: Tuple[int, int, int] = (110, 0, 255)

#: What a press looks like: a dark purple light against the dark field.
PRESS_COLOR: Tuple[int, int, int] = (130, 0, 230)

#: How far the field is pulled down from FIELD_COLOR. Not as low as it
#: looks: frames are gamma-corrected (leds.GAMMA = 2.2) just before send,
#: so a value that reads "dim" here lands near black on the wire. 0.30
#: pre-gamma is about 7% of full once sent -- (8,5,18) on the wire.
FIELD_LEVEL = 0.30


def _cc_value(x: float) -> int:
    return max(0, min(127, int(round(x * 127))))


class BlankMode(Mode):
    name = "blank"

    def __init__(self, midiout, channel: int = 0):
        self.midiout = midiout
        self.channel = int(channel)
        self.held_notes: Dict[Tuple[int, int], int] = {}
        # Last value SENT per control, so a still instrument stays quiet
        # on the wire instead of restating itself at the analog rate.
        self._last_cc: Dict[int, int] = {}

    # -- pads ----------------------------------------------------------
    def note_for_pad(self, x: int, y: int) -> int:
        return BASE_NOTE + int(y) * 8 + int(x)

    def on_key(self, x: int, y: int, edge: int) -> None:
        key = (int(x), int(y))
        if edge:
            note = self.note_for_pad(x, y)
            self.held_notes[key] = note
            self.midiout.note_on(note, vel=100, ch=self.channel)
        else:
            note = self.held_notes.pop(key, None)
            if note is not None:
                self.midiout.note_off(note, ch=self.channel)

    # -- everything else is a control ----------------------------------
    def _send_cc(self, cc: int, value: float) -> None:
        v = _cc_value(value)
        if self._last_cc.get(cc) == v:
            return
        self._last_cc[cc] = v
        self.midiout.cc(cc, v, ch=self.channel)

    def on_analog(self, raw) -> None:
        raw = list(raw)
        for i, cc in enumerate(SOFTPOT_CC):
            if i < len(raw):
                self._send_cc(cc, raw[i] / ANALOG_MAX)
        # The force sensors are controls here, never notes -- the same
        # rule the other modes hold to (a pad press is the note; the FSRs
        # underneath it say how hard, which is a continuous thing).
        for i, cc in enumerate(FSR_CC):
            j = 4 + i
            if j < len(raw):
                self._send_cc(cc, raw[j] / ANALOG_MAX)

    def on_lidar(self, mm4) -> None:
        # Near = 127. A hand approaching should open a control, not close
        # it, which is what every hand-over-sensor instrument does.
        for i, cc in enumerate(LIDAR_CC):
            if i >= len(mm4):
                continue
            mm = mm4[i]
            if mm is None:
                continue
            near = 1.0 - min(1.0, max(0.0, float(mm) / LIDAR_RANGE_MM))
            self._send_cc(cc, near)

    # -- looks ---------------------------------------------------------
    def led_frame(self):
        frame = [leds.dim(FIELD_COLOR, FIELD_LEVEL)] * leds.N_LEDS
        for (x, y) in self.held_notes:
            frame[leds.idx(x, y)] = PRESS_COLOR
        return frame

    def panic(self) -> None:
        for note in list(self.held_notes.values()):
            self.midiout.note_off(note, ch=self.channel)
        self.held_notes.clear()

    def state(self) -> dict:
        return {
            "base_note": BASE_NOTE,
            "channel": self.channel,
            "softpot_cc": list(SOFTPOT_CC),
            "fsr_cc": list(FSR_CC),
            "lidar_cc": list(LIDAR_CC),
            "held": [f"{x},{y}" for (x, y) in self.held_notes],
        }


__all__ = ["BlankMode", "BASE_NOTE", "SOFTPOT_CC", "FSR_CC", "LIDAR_CC"]
