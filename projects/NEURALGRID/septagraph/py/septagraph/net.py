"""UDP transport for the septagraph wire (spec §B ports).

Renderers listen on :data:`RENDER_PORT` (7700); controllers listen on
:data:`CTL_PORT` (7701) for manifests. :class:`Sender` is a thin
non-blocking fire-and-forget UDP sender; :class:`Receiver` runs a
background thread that decodes every incoming packet with a
:class:`~septagraph.codec.Decoder` and calls ``on_message`` per decoded
object, on the receiver thread.
"""
from __future__ import annotations

import socket
import threading
from typing import Callable

from .codec import Decoder

RENDER_PORT = 7700
CTL_PORT = 7701


class Sender:
    """Non-blocking UDP sender to one or more (host, port) targets."""

    def __init__(self, targets: list[tuple[str, int]], broadcast: bool = False) -> None:
        self.targets = list(targets)
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        if broadcast:
            self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        self._sock.setblocking(False)

    def send(self, packet: bytes) -> None:
        for host, port in self.targets:
            try:
                self._sock.sendto(packet, (host, port))
            except OSError:
                pass  # fast plane: never block or raise on a dropped send

    def try_recv(self, bufsize: int = 4096) -> tuple[bytes, tuple[str, int]] | None:
        """Non-blocking receive on this same (send-side) socket, or ``None``.

        The socket is non-blocking already; once :meth:`send` has fired at
        least one ``sendto``, the OS has bound an ephemeral local port to
        it, so a reply sent back to that ip:port (e.g. a lab server's
        ``/pong``) arrives here too -- no separate bound receiver needed.
        """
        try:
            return self._sock.recvfrom(bufsize)
        except (BlockingIOError, OSError):
            return None

    def close(self) -> None:
        self._sock.close()


class Receiver:
    """Background-thread UDP receiver that decodes and dispatches messages."""

    def __init__(self, port: int, on_message: Callable[[object], None], host: str = "0.0.0.0") -> None:
        self.port = port
        self.host = host
        self._on_message = on_message
        self._decoder = Decoder()
        self._sock: socket.socket | None = None
        self._thread: threading.Thread | None = None
        self._stop = threading.Event()

    def start(self) -> None:
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._sock.bind((self.host, self.port))
        self._sock.settimeout(0.2)
        self._stop.clear()
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def _run(self) -> None:
        assert self._sock is not None
        while not self._stop.is_set():
            try:
                data, _addr = self._sock.recvfrom(65536)
            except socket.timeout:
                continue
            except OSError:
                break
            for obj in self._decoder.feed(data):
                self._on_message(obj)

    def stop(self) -> None:
        self._stop.set()
        if self._thread is not None:
            self._thread.join(timeout=1.0)
        if self._sock is not None:
            self._sock.close()
            self._sock = None
