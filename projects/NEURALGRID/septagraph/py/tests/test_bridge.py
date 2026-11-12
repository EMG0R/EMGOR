"""Tests for the WebSocket<->UDP bridge (Task 2.5).

The test client is a ~20-line hand-rolled WebSocket client using raw
``socket`` -- the ``websockets`` package is explicitly not allowed (spec:
"NOT allowed -- write the 20-line client in the test").
"""
from __future__ import annotations

import base64
import os
import socket
import struct
import threading
import time

import pytest

from septagraph.codec import encode_hit, encode_manifest
from septagraph.tools.sept_bridge import Bridge
from septagraph.types import ZERO, Hit, Manifest, vec
from septagraph.axes import K


def _free_port() -> int:
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


class RawWSClient:
    """Minimal RFC 6455 client: opening handshake + masked binary frames."""

    def __init__(self, host: str, port: int) -> None:
        self.sock = socket.create_connection((host, port), timeout=2.0)
        key = base64.b64encode(os.urandom(16)).decode("ascii")
        req = (
            f"GET / HTTP/1.1\r\nHost: {host}:{port}\r\n"
            "Upgrade: websocket\r\nConnection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n"
        )
        self.sock.sendall(req.encode("ascii"))
        resp = self.sock.recv(4096)
        assert b"101" in resp.split(b"\r\n", 1)[0], resp

    def send_binary(self, payload: bytes) -> None:
        mask = os.urandom(4)
        masked = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
        n = len(payload)
        header = bytearray([0x82])  # FIN + binary
        if n < 126:
            header.append(0x80 | n)
        elif n < 65536:
            header.append(0x80 | 126)
            header += struct.pack(">H", n)
        else:
            header.append(0x80 | 127)
            header += struct.pack(">Q", n)
        self.sock.sendall(bytes(header) + mask + masked)

    def recv_binary(self, timeout: float = 2.0) -> bytes:
        self.sock.settimeout(timeout)
        hdr = self.sock.recv(2)
        b0, b1 = hdr[0], hdr[1]
        length = b1 & 0x7F
        if length == 126:
            length = struct.unpack(">H", self._recv_exact(2))[0]
        elif length == 127:
            length = struct.unpack(">Q", self._recv_exact(8))[0]
        return self._recv_exact(length)

    def _recv_exact(self, n: int) -> bytes:
        buf = b""
        while len(buf) < n:
            chunk = self.sock.recv(n - len(buf))
            if not chunk:
                raise ConnectionError("closed")
            buf += chunk
        return buf

    def close(self) -> None:
        self.sock.close()


@pytest.fixture
def target_receiver():
    port = _free_port()
    received: list[bytes] = []
    event = threading.Event()
    # a bare UDP socket (not septagraph.net.Receiver) so we assert on the
    # exact bytes the bridge relayed, with no decode step in between.
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("127.0.0.1", port))
    sock.settimeout(0.2)
    stop = threading.Event()

    def _run():
        while not stop.is_set():
            try:
                data, _ = sock.recvfrom(65536)
            except socket.timeout:
                continue
            except OSError:
                break
            received.append(data)
            event.set()

    t = threading.Thread(target=_run, daemon=True)
    t.start()
    yield port, received, event
    stop.set()
    t.join(timeout=1.0)
    sock.close()


def test_ws_frame_relays_to_udp_target(target_receiver):
    udp_port, received, event = target_receiver
    ws_port = _free_port()
    bridge = Bridge(ws_port=ws_port, to=[("127.0.0.1", udp_port)])
    bridge.start()
    try:
        time.sleep(0.05)
        client = RawWSClient("127.0.0.1", ws_port)
        try:
            h = Hit("grid:7", 0, True, 0.8, vec(0.42,0.7,0.9,0.02,0.35,0.5,0.1,0.2,0.5), ZERO)
            packet = encode_hit(h)
            client.send_binary(packet)
            assert event.wait(2.0)
            assert received[-1] == packet
        finally:
            client.close()
    finally:
        bridge.stop()


def test_ctl_port_udp_relays_to_ws_clients():
    ws_port = _free_port()
    ctl_port = _free_port()
    bridge = Bridge(ws_port=ws_port, to=[("127.0.0.1", _free_port())], ctl_port=ctl_port)
    bridge.start()
    try:
        time.sleep(0.05)
        client = RawWSClient("127.0.0.1", ws_port)
        try:
            time.sleep(0.05)  # let the accept thread register the client
            m = Manifest(
                "lab", "catalog", "Machine Lab", None, None,
                (),
            )
            packet = encode_manifest(m)
            sender_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sender_sock.sendto(packet, ("127.0.0.1", ctl_port))
            sender_sock.close()

            got = client.recv_binary(timeout=2.0)
            assert got == packet
        finally:
            client.close()
    finally:
        bridge.stop()


def test_tap_relays_render_port_traffic_to_ws_clients():
    ws_port = _free_port()
    render_port = _free_port()
    bridge = Bridge(
        ws_port=ws_port, to=[("127.0.0.1", _free_port())],
        tap=True, render_port=render_port,
    )
    bridge.start()
    try:
        time.sleep(0.05)
        client = RawWSClient("127.0.0.1", ws_port)
        try:
            time.sleep(0.05)
            h = Hit("grid:9", 0, True, 0.5, vec(*[0.5] * K), ZERO)
            packet = encode_hit(h)
            sender_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sender_sock.sendto(packet, ("127.0.0.1", render_port))
            sender_sock.close()

            got = client.recv_binary(timeout=2.0)
            assert got == packet
        finally:
            client.close()
    finally:
        bridge.stop()


def test_no_tap_by_default_render_port_not_bound():
    ws_port = _free_port()
    render_port = _free_port()
    bridge = Bridge(ws_port=ws_port, to=[("127.0.0.1", _free_port())], render_port=render_port)
    bridge.start()
    try:
        # render_port should be free -- bridge without --tap doesn't bind it
        probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        probe.bind(("0.0.0.0", render_port))
        probe.close()
    finally:
        bridge.stop()
