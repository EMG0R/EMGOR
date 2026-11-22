"""sources/web.py — accepts pushed values from a browser/phone (gyro, on-
screen faders) over local UDP, as a plain little text protocol.

Kept intentionally small: this is not a general RPC surface, just a way for
demiurge_web.py (or a future touch-UI page) to shove a named 0..1-ish value
into the same router any hardware sensor feeds. One UDP datagram = one
reading:

    <source_id> <raw_float>\n

e.g. b"phone_gyro_x 0.42\n". Multiple readings may be newline-separated in
one datagram. `source_id` is free-form — the operator declares it as a
"sources[]" entry in sensors.json (in_lo=0, in_hi=1 is the natural default
for anything already 0..1, but raw values of any range are fine as long as
the config's in_lo/in_hi matches what this sends).

No calibration, descriptors, or units are known ahead of time for a web
source (unlike the Teensy's fixed sensor set) — the config supplies
in_lo/in_hi directly and this driver's `descriptors` dict is intentionally
empty. conditioning.py already handles a missing SourceDescriptor (dropout
detection is simply skipped, which is correct: there is no hardware
sentinel for "phone disconnected" over UDP — a stalled source just stops
producing readings, which conditioning's "hold" behaviour via the router's
own outside="hold" zone policy can approximate if desired).
"""

from __future__ import annotations

import socket
import time
from typing import Iterator

from ..model import Reading

HOST_DEFAULT = "127.0.0.1"
# 8003, NOT 8001: the on-device touch UI (neptrPhase4_UI.py) already sends
# chunked wavetable OSC to 127.0.0.1:8001, and squatting that port would eat
# its packets. Port map — 8000 engine state out, 8001 wavetable, 8002 sensor
# telemetry out, 8003 this (pushed sensor values in), 7770 control into csound.
PORT_DEFAULT = 8003


def parse_datagram(data: bytes, t: float) -> list[Reading]:
    """Parse one UDP payload into zero or more Readings. Malformed lines are
    skipped rather than dropping the whole datagram."""
    out: list[Reading] = []
    text = data.decode("ascii", errors="ignore")
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        parts = line.split()
        if len(parts) != 2:
            continue
        source_id, raw_s = parts
        try:
            raw = float(raw_s)
        except ValueError:
            continue
        out.append(Reading(source_id, raw, t))
    return out


class WebSource:
    id = "web"
    descriptors: dict = {}   # no fixed sensor set; see module docstring

    def __init__(self, host: str = HOST_DEFAULT, port: int = PORT_DEFAULT):
        self.host = host
        self.port = port
        self.sock: socket.socket | None = None

    def _open(self) -> socket.socket:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.bind((self.host, self.port))
        return s

    def readings(self) -> Iterator[list[Reading]]:
        if self.sock is None:
            self.sock = self._open()
        while True:
            data, _addr = self.sock.recvfrom(4096)
            yield parse_datagram(data, time.monotonic())

    def run(self, on_reading, on_control=None) -> None:
        del on_control  # web source has no control-plane bypass events
        for readings in self.readings():
            for r in readings:
                on_reading(r)
