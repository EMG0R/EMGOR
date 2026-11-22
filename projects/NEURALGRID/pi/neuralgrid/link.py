"""Serial transport for the neuralGrid protocol (uses pyserial).

pyserial is imported lazily (inside methods that need it) so that
`neuralgrid.codec` — and even this module, at import time — stay usable on a
machine that has no pyserial installed and no hardware attached. Only
constructing/starting a GridLink actually requires pyserial.
"""

from __future__ import annotations

import threading
from typing import Callable, Optional

from . import codec

DELIMITER = codec.DELIMITER


class GridLink:
    """Owns a serial connection to the ESP32 and a background reader thread
    that frames, decodes, and dispatches incoming messages to callbacks.

    Writes are serialized with a lock so multiple threads can safely call
    send_*() concurrently.
    """

    def __init__(
        self,
        port: str,
        baudrate: int = 115200,
        on_hello: Optional[Callable[[codec.Hello], None]] = None,
        on_key: Optional[Callable[[codec.Key], None]] = None,
        on_analog: Optional[Callable[[codec.Analog], None]] = None,
        on_frame_error: Optional[Callable[[Exception, bytes], None]] = None,
        timeout: float = 0.1,
    ):
        # Lazy import: keep pyserial optional for callers that only need codec.
        import serial  # type: ignore

        self.port = port
        self.baudrate = baudrate
        self.on_hello = on_hello
        self.on_key = on_key
        self.on_analog = on_analog
        self.on_frame_error = on_frame_error

        self._serial = serial.Serial(port, baudrate, timeout=timeout)
        self._write_lock = threading.Lock()
        self._rx_buf = bytearray()
        self._stop_event = threading.Event()
        self._reader_thread: Optional[threading.Thread] = None

    # -- lifecycle ---------------------------------------------------

    def start(self) -> None:
        """Start the background reader thread."""
        if self._reader_thread is not None:
            return
        self._stop_event.clear()
        self._reader_thread = threading.Thread(
            target=self._reader_loop, name="GridLink-reader", daemon=True
        )
        self._reader_thread.start()

    def stop(self) -> None:
        """Stop the reader thread and close the serial port."""
        self._stop_event.set()
        if self._reader_thread is not None:
            self._reader_thread.join(timeout=2.0)
            self._reader_thread = None
        try:
            self._serial.close()
        except Exception:
            pass

    def __enter__(self) -> "GridLink":
        self.start()
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.stop()

    # -- reader loop ---------------------------------------------------

    def _reader_loop(self) -> None:
        while not self._stop_event.is_set():
            try:
                chunk = self._serial.read(256)
            except Exception:
                # Port likely closed/disconnected; stop quietly.
                break
            if chunk:
                self._feed(chunk)

    def _feed(self, chunk: bytes) -> None:
        self._rx_buf.extend(chunk)
        while True:
            idx = self._rx_buf.find(DELIMITER)
            if idx == -1:
                break
            frame = bytes(self._rx_buf[:idx])
            del self._rx_buf[: idx + 1]
            if frame:
                self._dispatch(frame)

    def _dispatch(self, cobs_frame: bytes) -> None:
        try:
            msg_type, body = codec.decode_frame(cobs_frame)
        except Exception as exc:
            if self.on_frame_error:
                self.on_frame_error(exc, cobs_frame)
            return

        try:
            if msg_type == codec.TYPE_HELLO and self.on_hello:
                self.on_hello(codec.parse_hello(body))
            elif msg_type == codec.TYPE_KEY and self.on_key:
                self.on_key(codec.parse_key(body))
            elif msg_type == codec.TYPE_ANALOG and self.on_analog:
                self.on_analog(codec.parse_analog(body))
        except Exception as exc:
            if self.on_frame_error:
                self.on_frame_error(exc, cobs_frame)

    # -- writes ---------------------------------------------------

    def _write(self, frame: bytes) -> None:
        with self._write_lock:
            self._serial.write(frame)
            self._serial.flush()

    def send_leds_full(self, rgb_list) -> None:
        self._write(codec.leds_full(rgb_list))

    def send_leds_partial(self, updates) -> None:
        self._write(codec.leds_partial(updates))

    def send_brightness(self, level: int) -> None:
        self._write(codec.brightness(level))

    def send_clear(self) -> None:
        self._write(codec.clear())

    def send_ping(self) -> None:
        self._write(codec.ping())
