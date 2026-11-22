"""WebSocket <-> UDP bridge for browser Septagraph controllers (Task 2.5).

A browser page can't send raw UDP. This bridge is a stdlib-only WebSocket
server (hand-rolled RFC 6455 handshake + binary frames -- no new
dependency, in particular no ``websockets`` package) that:

- relays every binary WS frame from any connected browser client, verbatim,
  to one or more UDP targets (the fast/slow planes, port 7700 by default);
- relays every UDP packet arriving on the control port (7701, manifests)
  back out to all connected WS clients, verbatim;
- with ``--tap``, also opens a second UDP listener on the render port
  (7700) and forwards every packet it sees there to WS clients too, so a
  browser visualizer can watch what a *different* local controller (e.g.
  the neuralGrid Pi, or another renderer's own traffic) is sending, without
  a renderer of its own running on this host.

All relaying is byte-for-byte: the bridge never decodes OSC. This keeps it
protocol-agnostic and keeps golden-byte tests meaningful end-to-end.

CLI::

    python -m septagraph.tools.sept_bridge --ws 8766 \\
        --to 127.0.0.1:7700 [--to 192.168.0.103:7700] [--broadcast] [--tap]
"""
from __future__ import annotations

import argparse
import base64
import hashlib
import socket
import struct
import threading
from dataclasses import dataclass, field
from typing import Optional

from ..net import CTL_PORT, RENDER_PORT

WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

OP_CONT = 0x0
OP_TEXT = 0x1
OP_BINARY = 0x2
OP_CLOSE = 0x8
OP_PING = 0x9
OP_PONG = 0xA


def accept_key(sec_websocket_key: str) -> str:
    """RFC 6455 SS4.2.2: Sec-WebSocket-Accept = base64(sha1(key + GUID))."""
    digest = hashlib.sha1((sec_websocket_key + WS_GUID).encode("ascii")).digest()
    return base64.b64encode(digest).decode("ascii")


def _recv_exact(sock: socket.socket, n: int) -> bytes:
    buf = bytearray()
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("socket closed")
        buf.extend(chunk)
    return bytes(buf)


def read_handshake_request(sock: socket.socket) -> dict[str, str]:
    """Read an HTTP/1.1 upgrade request (headers only) and return headers
    as a case-insensitive dict (keys lowercased)."""
    data = b""
    while b"\r\n\r\n" not in data:
        chunk = sock.recv(4096)
        if not chunk:
            raise ConnectionError("socket closed during handshake")
        data += chunk
        if len(data) > 65536:
            raise ConnectionError("handshake request too large")
    head, _, _rest = data.partition(b"\r\n\r\n")
    lines = head.decode("iso-8859-1").split("\r\n")
    headers: dict[str, str] = {}
    for line in lines[1:]:
        if ":" not in line:
            continue
        k, v = line.split(":", 1)
        headers[k.strip().lower()] = v.strip()
    return headers


def do_server_handshake(sock: socket.socket) -> bool:
    """Perform the server side of the WS opening handshake. Returns True on
    success (101 sent), False if this wasn't a valid WS upgrade request."""
    headers = read_handshake_request(sock)
    key = headers.get("sec-websocket-key")
    if not key or headers.get("upgrade", "").lower() != "websocket":
        sock.sendall(b"HTTP/1.1 400 Bad Request\r\n\r\n")
        return False
    accept = accept_key(key)
    response = (
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        f"Sec-WebSocket-Accept: {accept}\r\n"
        "\r\n"
    )
    sock.sendall(response.encode("ascii"))
    return True


def encode_frame(payload: bytes, opcode: int = OP_BINARY) -> bytes:
    """Encode one unmasked (server->client) WS frame, FIN=1."""
    length = len(payload)
    header = bytearray()
    header.append(0x80 | (opcode & 0x0F))  # FIN=1
    if length < 126:
        header.append(length)
    elif length < 65536:
        header.append(126)
        header += struct.pack(">H", length)
    else:
        header.append(127)
        header += struct.pack(">Q", length)
    return bytes(header) + payload


def read_frame(sock: socket.socket) -> Optional[tuple[int, bytes]]:
    """Read one WS frame from a client (must be masked per RFC 6455).
    Returns (opcode, payload), or None on a clean close / closed socket.
    Only single-frame messages are supported (FIN must be 1) -- adequate
    for the small OSC packets this bridge relays."""
    hdr = sock.recv(2)
    if len(hdr) < 2:
        return None
    if len(hdr) == 1:
        hdr += _recv_exact(sock, 1)
    b0, b1 = hdr[0], hdr[1]
    fin = (b0 & 0x80) != 0
    opcode = b0 & 0x0F
    masked = (b1 & 0x80) != 0
    length = b1 & 0x7F
    if length == 126:
        length = struct.unpack(">H", _recv_exact(sock, 2))[0]
    elif length == 127:
        length = struct.unpack(">Q", _recv_exact(sock, 8))[0]
    mask_key = _recv_exact(sock, 4) if masked else b"\x00\x00\x00\x00"
    payload = _recv_exact(sock, length) if length else b""
    if masked:
        payload = bytes(b ^ mask_key[i % 4] for i, b in enumerate(payload))
    if not fin:
        # fragmented message: not supported, but don't crash the connection
        raise ConnectionError("fragmented WS frames are not supported")
    return opcode, payload


@dataclass
class _Client:
    sock: socket.socket
    lock: threading.Lock = field(default_factory=threading.Lock)

    def send(self, payload: bytes) -> bool:
        try:
            with self.lock:
                self.sock.sendall(encode_frame(payload))
            return True
        except OSError:
            return False


class Bridge:
    """WebSocket <-> UDP relay. See module docstring."""

    def __init__(
        self,
        ws_port: int,
        to: list[tuple[str, int]],
        broadcast: bool = False,
        tap: bool = False,
        ctl_port: int = CTL_PORT,
        render_port: int = RENDER_PORT,
        ws_host: str = "0.0.0.0",
    ) -> None:
        self.ws_port = ws_port
        self.ws_host = ws_host
        self.to = list(to)
        self.broadcast = broadcast
        self.tap = tap
        self.ctl_port = ctl_port
        self.render_port = render_port

        self._clients: list[_Client] = []
        self._clients_lock = threading.Lock()

        self._udp_out = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        if self.broadcast:
            self._udp_out.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

        self._ws_server: Optional[socket.socket] = None
        self._ctl_sock: Optional[socket.socket] = None
        self._tap_sock: Optional[socket.socket] = None
        self._threads: list[threading.Thread] = []
        self._stop = threading.Event()

    # -- lifecycle -----------------------------------------------------
    def start(self) -> None:
        self._stop.clear()

        self._ws_server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._ws_server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._ws_server.bind((self.ws_host, self.ws_port))
        self._ws_server.listen(8)
        self._ws_server.settimeout(0.2)
        t = threading.Thread(target=self._accept_loop, daemon=True)
        t.start()
        self._threads.append(t)

        self._ctl_sock = self._bind_udp(self.ctl_port)
        t = threading.Thread(
            target=self._udp_relay_loop, args=(self._ctl_sock,), daemon=True
        )
        t.start()
        self._threads.append(t)

        if self.tap:
            self._tap_sock = self._bind_udp(self.render_port)
            t = threading.Thread(
                target=self._udp_relay_loop, args=(self._tap_sock,), daemon=True
            )
            t.start()
            self._threads.append(t)

    def stop(self) -> None:
        self._stop.set()
        for s in (self._ws_server, self._ctl_sock, self._tap_sock):
            if s is not None:
                try:
                    s.close()
                except OSError:
                    pass
        with self._clients_lock:
            clients, self._clients = self._clients, []
        for c in clients:
            try:
                c.sock.close()
            except OSError:
                pass
        for t in self._threads:
            t.join(timeout=1.0)
        self._threads.clear()
        self._udp_out.close()

    def actual_ws_port(self) -> int:
        assert self._ws_server is not None
        return self._ws_server.getsockname()[1]

    # -- WS accept / per-connection --------------------------------------
    def _accept_loop(self) -> None:
        assert self._ws_server is not None
        while not self._stop.is_set():
            try:
                conn, _addr = self._ws_server.accept()
            except socket.timeout:
                continue
            except OSError:
                break
            t = threading.Thread(target=self._serve_client, args=(conn,), daemon=True)
            t.start()

    def _serve_client(self, sock: socket.socket) -> None:
        try:
            if not do_server_handshake(sock):
                sock.close()
                return
        except (ConnectionError, OSError):
            sock.close()
            return

        client = _Client(sock)
        with self._clients_lock:
            self._clients.append(client)
        try:
            sock.settimeout(0.5)
            while not self._stop.is_set():
                try:
                    frame = read_frame(sock)
                except socket.timeout:
                    continue
                except (ConnectionError, OSError):
                    break
                if frame is None:
                    break
                opcode, payload = frame
                if opcode == OP_CLOSE:
                    break
                if opcode == OP_PING:
                    # Best-effort pong; browsers rarely ping an app socket
                    # and a missed pong just risks an idle-timeout close.
                    try:
                        with client.lock:
                            client.sock.sendall(encode_frame(payload, OP_PONG))
                    except OSError:
                        break
                    continue
                if opcode == OP_BINARY and payload:
                    self._relay_to_udp(payload)
        finally:
            with self._clients_lock:
                if client in self._clients:
                    self._clients.remove(client)
            try:
                sock.close()
            except OSError:
                pass

    # -- relaying --------------------------------------------------------
    def _relay_to_udp(self, payload: bytes) -> None:
        for host, port in self.to:
            try:
                self._udp_out.sendto(payload, (host, port))
            except OSError:
                pass

    def _bind_udp(self, port: int) -> socket.socket:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind(("0.0.0.0", port))
        s.settimeout(0.2)
        return s

    def _udp_relay_loop(self, sock: socket.socket) -> None:
        while not self._stop.is_set():
            try:
                data, _addr = sock.recvfrom(65536)
            except socket.timeout:
                continue
            except OSError:
                break
            self._broadcast_to_ws(data)

    def _broadcast_to_ws(self, payload: bytes) -> None:
        with self._clients_lock:
            clients = list(self._clients)
        dead = []
        for c in clients:
            if not c.send(payload):
                dead.append(c)
        if dead:
            with self._clients_lock:
                for d in dead:
                    if d in self._clients:
                        self._clients.remove(d)


def _parse_hostport(s: str) -> tuple[str, int]:
    host, _, port = s.rpartition(":")
    if not host:
        raise argparse.ArgumentTypeError(f"expected host:port, got {s!r}")
    return host, int(port)


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--ws", type=int, default=8766, help="WebSocket listen port")
    p.add_argument(
        "--to",
        action="append",
        default=[],
        type=_parse_hostport,
        metavar="host:port",
        help="UDP relay target (repeatable)",
    )
    p.add_argument("--broadcast", action="store_true")
    p.add_argument(
        "--tap",
        action="store_true",
        help="also relay UDP arriving on the render port (7700) to WS clients",
    )
    p.add_argument("--ctl-port", type=int, default=CTL_PORT)
    p.add_argument("--render-port", type=int, default=RENDER_PORT)
    args = p.parse_args(argv)

    if not args.to:
        p.error("at least one --to host:port is required")

    bridge = Bridge(
        ws_port=args.ws,
        to=args.to,
        broadcast=args.broadcast,
        tap=args.tap,
        ctl_port=args.ctl_port,
        render_port=args.render_port,
    )
    bridge.start()
    print(
        f"[sept_bridge] WS :{bridge.actual_ws_port()} <-> UDP "
        f"{', '.join(f'{h}:{p}' for h, p in args.to)}"
        + (" [broadcast]" if args.broadcast else "")
        + (f" | tap :{args.render_port}" if args.tap else "")
        + f" | manifests <- :{args.ctl_port}"
    )
    try:
        while True:
            threading.Event().wait(1.0)
    except KeyboardInterrupt:
        pass
    finally:
        bridge.stop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
