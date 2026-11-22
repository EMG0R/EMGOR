#!/usr/bin/env python3
"""Unit test for LidarGridLink._reader_loop's minimal-latency read strategy.

    python3 test_link_lidar.py             # run all tests
    python3 -m unittest test_link_lidar    # equivalent

ROOT CAUSE this guards against: GridLink._reader_loop (link.py, FROZEN) does
`self._serial.read(256)` with timeout=0.1 — pyserial's read(n) blocks until
n bytes arrive OR the timeout expires, so a short frame (a HELLO/KEY/ACK is
well under 256 bytes) can sit for up to the full 100ms timeout before being
handed back. This matched the measured ~49.5ms PING->HELLO link RTT that
fw5's on-device timing work could not move (see HANDOFF.md's LATENCY
section). link.py cannot be edited, so LidarGridLink overrides
_reader_loop with a read(1) + drain-in_waiting strategy instead.

This test proves that override dispatches a short (well under 256 byte)
frame using exactly two calls to the fake serial's read() -- one blocking
read(1), one non-blocking drain of whatever was already sitting in
in_waiting -- never a read(256) that would wait for a full buffer's worth
of data or the timeout to elapse.
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from neuralgrid import codec  # noqa: E402
from neuralgrid.link_lidar import LidarGridLink  # noqa: E402


class FakeSerial:
    """Minimal stand-in for a pyserial Serial object: exposes exactly the
    surface _reader_loop touches (.read(n), .in_waiting, .timeout) plus a
    read-call log so the test can assert *how* the frame was read, not just
    that it eventually arrived.

    Simulates data that has ALREADY fully arrived in the OS driver's buffer
    by the time the reader loop asks -- i.e. exactly the case where the old
    read(256, timeout=0.1) behavior would still block until either 256
    bytes accumulated or the 100ms timeout expired, even though every byte
    of the frame was sitting there the whole time.
    """

    def __init__(self, data: bytes):
        self._data = data
        self._pos = 0
        self.timeout = 0.1
        self.read_calls: list[int] = []

    @property
    def in_waiting(self) -> int:
        return len(self._data) - self._pos

    def read(self, n: int = 1) -> bytes:
        self.read_calls.append(n)
        chunk = self._data[self._pos:self._pos + n]
        self._pos += len(chunk)
        return chunk


class TestReaderLoopLatency(unittest.TestCase):
    def _make_link(self, fake_serial: FakeSerial) -> LidarGridLink:
        # Bypass GridLink.__init__ (it lazily imports pyserial and opens a
        # real port) -- construct a bare instance and wire up only what
        # _reader_loop / _feed / _dispatch actually touch, matching what
        # __init__ would have set.
        link = object.__new__(LidarGridLink)
        link._serial = fake_serial
        link._rx_buf = bytearray()
        import threading
        link._stop_event = threading.Event()
        link.on_hello = None
        link.on_key = None
        link.on_analog = None
        link.on_lidar = None
        link.on_lidardiag = None
        link.on_ack = None
        link.on_timing = None
        link.on_frame_error = None
        return link

    def test_short_frame_dispatched_without_reading_256_bytes(self):
        # A KEY frame (press, x=3, y=5) -- a handful of bytes on the wire,
        # nowhere near the old code's read(256) threshold.
        body = bytes([3, 5, 1])
        frame = codec.encode_frame(codec.TYPE_KEY, body)  # COBS + trailing 0x00
        self.assertLess(len(frame), 32, "test frame should be short, like a real KEY")

        fake = FakeSerial(frame)
        link = self._make_link(fake)

        received = []

        def on_key(k):
            received.append(k)
            # Stop the loop right after the frame we care about is
            # dispatched -- proves it happened within the very first
            # read(1)+drain cycle, not after further polling.
            link._stop_event.set()

        link.on_key = on_key

        link._reader_loop()

        self.assertEqual(len(received), 1)
        key = received[0]
        self.assertEqual((key.x, key.y, key.pressed), (3, 5, True))

        # The crux of the fix: dispatched via one blocking read(1) plus one
        # drain of in_waiting -- never a read(256) waiting on a full buffer
        # or the 100ms timeout.
        self.assertEqual(fake.read_calls, [1, len(frame) - 1])
        self.assertNotIn(256, fake.read_calls)

    def test_stop_event_honored_when_idle(self):
        # No data at all: read(1) returns b"" (as pyserial does on timeout
        # with nothing to read), loop must still respect the stop event
        # rather than spinning forever.
        fake = FakeSerial(b"")
        link = self._make_link(fake)
        link._stop_event.set()  # already stopped

        link._reader_loop()  # must return promptly, not hang

        self.assertEqual(fake.read_calls, [])

    def test_exception_breaks_loop_quietly(self):
        class RaisingSerial(FakeSerial):
            def read(self, n: int = 1) -> bytes:
                raise OSError("port closed")

        link = self._make_link(RaisingSerial(b""))
        # Should return (not raise) -- matches GridLink._reader_loop's
        # except Exception: break semantics.
        link._reader_loop()


if __name__ == "__main__":
    unittest.main()
