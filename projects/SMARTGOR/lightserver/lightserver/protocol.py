"""Frame Protocol v1 packet builder + UDP sender.

Packet layout (little-endian):
  bytes 0-1 : magic 'F','C'
  byte  2   : version = 1
  byte  3   : flags   (bit0 = SHOW: latch buffer to strip after this packet)
  bytes 4-5 : uint16  pixel offset
  bytes 6-7 : uint16  pixel count N
  then 3*N bytes RGB
"""
from __future__ import annotations

import socket
import struct
import time

MAGIC = b"FC"
VERSION = 1
FLAG_SHOW = 0x01
HEADER = struct.Struct("<2sBBHH")
MAX_PIXELS_PER_PACKET = 400


def build_packet(offset: int, rgb: bytes, show: bool) -> bytes:
    """Build a single protocol-v1 packet. rgb is raw 3*N bytes."""
    n = len(rgb) // 3
    if len(rgb) != n * 3:
        raise ValueError("rgb length must be a multiple of 3")
    if not (0 <= offset <= 0xFFFF and 0 <= n <= 0xFFFF):
        raise ValueError("offset/count out of uint16 range")
    flags = FLAG_SHOW if show else 0
    return HEADER.pack(MAGIC, VERSION, flags, offset, n) + rgb


def build_frame_packets(frame_bytes: bytes, chunk: int = MAX_PIXELS_PER_PACKET) -> list[bytes]:
    """Split a full frame (3*num_pixels bytes) into packets.

    All packets have flags=0 except the last, which carries SHOW so the
    firmware latches the completed buffer to the strip.
    """
    n = len(frame_bytes) // 3
    if len(frame_bytes) != n * 3:
        raise ValueError("frame length must be a multiple of 3")
    packets = []
    off = 0
    while off < n:
        cnt = min(chunk, n - off)
        last = off + cnt >= n
        packets.append(build_packet(off, frame_bytes[off * 3:(off + cnt) * 3], show=last))
        off += cnt
    return packets


def parse_packet(pkt: bytes):
    """Parse a packet back into (version, flags, offset, count, rgb). For tests."""
    magic, version, flags, offset, count = HEADER.unpack(pkt[:HEADER.size])
    if magic != MAGIC:
        raise ValueError("bad magic")
    rgb = pkt[HEADER.size:HEADER.size + 3 * count]
    if len(rgb) != 3 * count:
        raise ValueError("truncated payload")
    return version, flags, offset, count, rgb


RESOLVE_RETRY_SECS = 20.0    # retry hostname resolution this often while failing
RESOLVE_REFRESH_SECS = 300.0  # re-resolve a working hostname this often (IP churn)


class UdpSender:
    """Sends frames to a strip. Non-blocking best-effort UDP.

    Targeting: if `host` is given (mDNS hostname preferred) it is resolved
    lazily and re-resolved periodically; a resolved address becomes the
    last-known `ip`. If resolution fails, falls back to the last-known `ip`.
    With no usable address the frame is silently dropped (node offline) —
    network trouble never raises out of send_frame.
    """

    def __init__(self, ip: str | None, port: int, host: str | None = None):
        self.ip = ip or None          # last-known / fallback IP
        self.host = host or None      # preferred hostname (e.g. foo.local)
        self.port = port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.frames_sent = 0
        self.packets_sent = 0
        self.bytes_sent = 0
        self.last_error: str | None = None
        self.last_send_ts: float | None = None
        self.resolved_ip: str | None = None
        self._next_resolve = 0.0

    def target_ip(self) -> str | None:
        """Best current target IP; periodically (re-)resolves self.host."""
        if not self.host:
            return self.ip
        now = time.time()
        if now >= self._next_resolve:
            try:
                self.resolved_ip = socket.gethostbyname(self.host)
                self.ip = self.resolved_ip  # remember as last-known
                self._next_resolve = now + RESOLVE_REFRESH_SECS
                if self.last_error and self.last_error.startswith("resolve "):
                    self.last_error = None
            except OSError as e:
                self.last_error = f"resolve {self.host}: {e}"
                self._next_resolve = now + RESOLVE_RETRY_SECS
        return self.resolved_ip or self.ip

    def send_frame(self, frame_bytes: bytes) -> None:
        try:
            ip = self.target_ip()
            if ip is None:  # node offline / never resolved: drop frame
                return
            for pkt in build_frame_packets(frame_bytes):
                self.sock.sendto(pkt, (ip, self.port))
                self.packets_sent += 1
                self.bytes_sent += len(pkt)
            self.frames_sent += 1
            self.last_send_ts = time.time()
        except OSError as e:  # never let network trouble kill the engine
            self.last_error = str(e)

    def close(self) -> None:
        try:
            self.sock.close()
        except OSError:
            pass
