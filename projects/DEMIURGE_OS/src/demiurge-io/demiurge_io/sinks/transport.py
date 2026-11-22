"""sinks/transport.py — a tempo TAP toward demiurge-clock.

The clock already owns tap analysis (demiurge-transport.h: one analyzer, shared
with PMOR). It accepts a tap two ways: ch16 CC113 on the MIDI bus (what
serial_bridge.py sent for the arcade button) and OSC `/transport/tap ,if`
(source, age_ms) on UDP 127.0.0.1:9100 ("the same event from any other pool
source", demiurge-clock.cpp). The pool uses the OSC form: no MIDI port, and the
age is a real float. NEPTR is source 1.

AGE. A press seen in a 100 Hz frame happened somewhere in the preceding 10 ms,
~5 ms ago on average, plus however long we held the frame before sending. This
is serial_bridge.py's tap_age_ms() to the letter (5 ms + dwell, whole ms,
clipped 0..127) so a tap measures the same through either path.

`wants_t` tells Pool.send_target to hand us the press time (the reading's
monotonic timestamp). Nothing here loops back: the clock republishes taps on
9101 and the pool does not listen there.
"""

from __future__ import annotations

import socket
import time

from ..osc import encode_osc

CLOCK_OSC = ("127.0.0.1", 9100)       # DT_OSC_IN_PORT in demiurge-transport.h
TAP_SRC_NEPTR = 1
TAP_FRAME_AGE_MS = 5.0                # serial_bridge.py TAP_FRAME_AGE_MS


def tap_age_ms(t_press: float, now: float) -> int:
    age = TAP_FRAME_AGE_MS + (now - t_press) * 1000.0
    return max(0, min(127, int(round(age))))


class TransportSink:
    wants_t = True

    def __init__(self, addr=CLOCK_OSC, clock=time.monotonic):
        self.addr = addr
        self.clock = clock
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def packet(self, t_press) -> bytes:
        age = tap_age_ms(t_press, self.clock()) if t_press is not None else 0
        return encode_osc("/transport/tap", TAP_SRC_NEPTR, float(age))

    def send(self, target: dict, value: float, t0=None) -> None:
        if target.get("kind") != "tap":
            raise ValueError(f"unknown transport kind: {target.get('kind')!r}")
        if value < 0.5:                  # a tap is the 0 -> 1 edge; ignore the 0
            return
        self.sock.sendto(self.packet(t0), self.addr)
