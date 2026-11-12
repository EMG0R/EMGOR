"""LidarGridLink — GridLink subclass that also dispatches LIDAR (0x13) frames
and the fw5 latency-work frames (ACK 0x15, TIMING 0x16).

ADDITIVE module: link.py is not modified. GridLink._dispatch silently drops
message types it does not know, so lidar-aware consumers (monitor.py,
test_lidar.py) use this subclass instead. All existing callbacks
(on_hello / on_key / on_analog / on_frame_error) behave exactly as before;
new callbacks:

    on_lidar(lidar: codec_lidar.Lidar)         # called from the reader thread
    on_lidardiag(diag: codec_lidar.LidarDiag)
    on_ack(ack: codec_lidar.LedAck)            # fw5+
    on_timing(t: codec_lidar.Timing)           # fw5+

Works unchanged against older firmware too — it simply never emits the
types it doesn't have, so the corresponding callback never fires.
send_local_echo() is a thin wrapper for the fw5 TYPE_LOCAL_ECHO down-frame.
"""

from __future__ import annotations

from typing import Callable, Optional

from . import codec, codec_lidar
from .link import GridLink


class LidarGridLink(GridLink):
    def __init__(
        self,
        port: str,
        baudrate: int = 115200,
        on_hello: Optional[Callable[[codec.Hello], None]] = None,
        on_key: Optional[Callable[[codec.Key], None]] = None,
        on_analog: Optional[Callable[[codec.Analog], None]] = None,
        on_lidar: Optional[Callable[[codec_lidar.Lidar], None]] = None,
        on_lidardiag: Optional[Callable[[codec_lidar.LidarDiag], None]] = None,
        on_ack: Optional[Callable[[codec_lidar.LedAck], None]] = None,
        on_timing: Optional[Callable[[codec_lidar.Timing], None]] = None,
        on_frame_error: Optional[Callable[[Exception, bytes], None]] = None,
        timeout: float = 0.1,
    ):
        super().__init__(
            port,
            baudrate=baudrate,
            on_hello=on_hello,
            on_key=on_key,
            on_analog=on_analog,
            on_frame_error=on_frame_error,
            timeout=timeout,
        )
        self.on_lidar = on_lidar
        self.on_lidardiag = on_lidardiag
        self.on_ack = on_ack
        self.on_timing = on_timing

    # -- reader loop (latency fix) --------------------------------------
    #
    # GridLink._reader_loop (link.py, FROZEN) does `self._serial.read(256)`
    # with timeout=0.1: pyserial's read(n) blocks until n bytes arrive OR
    # the timeout expires, so a short frame (a HELLO/KEY/ACK is well under
    # 256 bytes) sits in the OS driver buffer for up to the full 100ms
    # timeout before being handed back — mean ~50ms, matching the measured
    # ~49.5ms PING->HELLO RTT that fw5's on-device timing work could not
    # move (see HANDOFF.md LATENCY section). link.py is frozen, so the fix
    # lives here instead: read 1 byte (blocks at most `timeout` only when
    # the link is truly idle — the normal, expected case), then drain
    # whatever else is already sitting in the driver buffer via
    # `in_waiting` with a non-blocking follow-up read. A short frame that
    # has already fully arrived by the time we ask is returned in the same
    # loop iteration it arrived in, not up to 100ms later. Stop-event and
    # exception->break semantics are identical to the parent.
    def _reader_loop(self) -> None:
        while not self._stop_event.is_set():
            try:
                chunk = self._serial.read(1)
                if chunk:
                    n = self._serial.in_waiting
                    if n:
                        chunk += self._serial.read(n)
            except Exception:
                # Port likely closed/disconnected; stop quietly.
                break
            if chunk:
                self._feed(chunk)

    def send_local_echo(self, enabled: bool) -> None:
        """fw5+: toggle Pi-gated immediate key->LED echo (default OFF on the
        firmware side; this call is a no-op against older firmware, which
        silently ignores unknown down-frame types)."""
        self._write(codec_lidar.local_echo(enabled))

    def send_lidar_heal(self, enabled: bool) -> None:
        """fw7+: toggle the firmware's dead-lidar-channel self-heal
        round-robin (default ON on the firmware side; no-op against older
        firmware). See HANDOFF.md §12 / the firmware README's "fw7"
        section for why this exists."""
        self._write(codec_lidar.lidar_heal(enabled))

    def _dispatch(self, cobs_frame: bytes) -> None:
        # Peek at the type; on LIDAR handle it here, otherwise defer to the
        # parent implementation (which re-decodes — trivially cheap at the
        # sizes/rates this protocol runs at, and keeps link.py untouched).
        try:
            msg_type, body = codec.decode_frame(cobs_frame)
        except Exception as exc:
            if self.on_frame_error:
                self.on_frame_error(exc, cobs_frame)
            return

        if msg_type == codec_lidar.TYPE_LIDAR:
            if self.on_lidar:
                try:
                    self.on_lidar(codec_lidar.parse_lidar(body))
                except Exception as exc:
                    if self.on_frame_error:
                        self.on_frame_error(exc, cobs_frame)
            return

        if msg_type == codec_lidar.TYPE_LIDARDIAG:
            if self.on_lidardiag:
                try:
                    self.on_lidardiag(codec_lidar.parse_lidardiag(body))
                except Exception as exc:
                    if self.on_frame_error:
                        self.on_frame_error(exc, cobs_frame)
            return

        if msg_type == codec_lidar.TYPE_ACK:
            if self.on_ack:
                try:
                    self.on_ack(codec_lidar.parse_ack(body))
                except Exception as exc:
                    if self.on_frame_error:
                        self.on_frame_error(exc, cobs_frame)
            return

        if msg_type == codec_lidar.TYPE_TIMING:
            if self.on_timing:
                try:
                    self.on_timing(codec_lidar.parse_timing(body))
                except Exception as exc:
                    if self.on_frame_error:
                        self.on_frame_error(exc, cobs_frame)
            return

        super()._dispatch(cobs_frame)
