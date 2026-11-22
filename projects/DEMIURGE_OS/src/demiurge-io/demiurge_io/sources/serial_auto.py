"""sources/serial_auto.py — generic serial -> pool sources. SCAFFOLD.

*** FIRMWARE HANDSHAKE UNVERIFIED. *** Nothing in the repo emits `@hello` yet;
only the Teensy NEPTR2 frame (sources/teensy.py) is known to exist. This
module implements docs/parameters.md §3.2 as written, so a future microcontroller
can be added by firmware alone, but it has never talked to real hardware.

NEVER AUTO-DISCOVERED. Unlike teensy.resolve_port this takes an EXPLICIT port
(--serial-auto PATH, repeatable): scanning /dev/ttyACM* would grab the NEPTR
body, the DUODECIMUS or the Bela — the exact failure resolve_port documents.

Protocol (line oriented, 115200 8N1, same SerialReader as the Teensy):
    @hello <device> <name>:<kind>:<lo>:<hi> ...     once, on (re)connect
    <name> <raw>                                     then streamed
  kind: knob -> continuous, button -> gate, enc -> step (informational here;
  the map's `mode` decides routing — kind is exposed via .kinds for a UI).
Source ids are `/hw/<device>/<name>`; lo/hi seed calibration (in_lo/in_hi).

Unknown / no handshake: `<name> <raw>` lines still work as `/hw/<basename of
port>/<name>`, auto-ranged from the min/max observed so far, and reported by
.uncalibrated() so a UI can flag them until someone confirms.
"""

from __future__ import annotations

import os
from typing import Optional

from ..model import Reading
from .teensy import SerialReader


def parse_hello(line: str):
    """'@hello dev a:knob:0:1023 b:button:0:1' -> (device, {name: (kind, lo, hi)})
    or None if not a well-formed hello. Bad channel specs are skipped."""
    parts = line.split()
    if len(parts) < 2 or parts[0] != "@hello":
        return None
    device = parts[1]
    chans = {}
    for spec in parts[2:]:
        f = spec.split(":")
        if len(f) != 4:
            continue
        try:
            chans[f[0]] = (f[1], float(f[2]), float(f[3]))
        except ValueError:
            continue
    return device, chans


class SerialAutoSource:
    descriptors: dict = {}

    def __init__(self, path: str):
        self.path = path
        self.id = "serial_auto:" + os.path.basename(path)
        self.device = os.path.basename(path)
        self.reader: Optional[SerialReader] = None
        self._cal: dict = {}          # source_id -> (lo, hi)
        self.kinds: dict = {}         # source_id -> kind
        self._auto: dict = {}         # source_id -> [min, max]  (no handshake)

    def _sid(self, name: str) -> str:
        return f"/hw/{self.device}/{name}"

    def handle_line(self, line: str, t: float) -> Optional[Reading]:
        """Pure per-line logic (testable without a port)."""
        if not line:
            return None
        if line.startswith("@hello"):
            h = parse_hello(line)
            if h:
                self.device, chans = h
                for name, (kind, lo, hi) in chans.items():
                    sid = self._sid(name)
                    self._cal[sid] = (lo, hi)
                    self.kinds[sid] = kind
                    self._auto.pop(sid, None)
            return None
        parts = line.split()
        if len(parts) != 2:
            return None
        try:
            raw = float(parts[1])
        except ValueError:
            return None
        sid = self._sid(parts[0])
        if sid not in self._cal:
            lohi = self._auto.setdefault(sid, [raw, raw])
            lohi[0], lohi[1] = min(lohi[0], raw), max(lohi[1], raw)
        return Reading(sid, raw, t)

    def calibration(self, source_id: str):
        lohi = self._cal.get(source_id) or self._auto.get(source_id)
        if lohi is None:
            return None
        lo, hi = lohi
        return {"in_lo": lo, "in_hi": hi if hi > lo else lo + 1.0}

    def uncalibrated(self) -> list:
        return sorted(self._auto)

    def run(self, on_reading, on_control=None) -> None:
        import time
        del on_control
        self.reader = SerialReader(self.path)
        for line in self.reader.lines():
            r = self.handle_line(line, time.monotonic())
            if r:
                on_reading(r)
