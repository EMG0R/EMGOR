"""MIDI out over ethernet, with nothing to install on the laptop.

The neuralGrid has no sound card, no ALSA sequencer and no USB gadget,
so "send MIDI" had to mean the network. RTP-MIDI (AppleMIDI) is the one
option a Mac speaks out of the box: Audio MIDI Setup -> Network, click
Connect, and Ableton lists the neuralGrid like any other port.

Two UDP ports: control (invitations, goodbyes) and data = control + 1
(clock sync and the MIDI itself). We are always the RESPONDER -- the
laptop invites us -- which keeps this to a handshake, a clock reply, and
a stream of RTP packets.

Everything that touches the wire format is a pure function here, because
a single wrong byte means a silent port and no error anywhere.

References: RFC 6295, Apple's session protocol as documented by the
rtpmidi/pymidi implementations.
"""
from __future__ import annotations

import socket
import struct
import threading
import time
from dataclasses import dataclass
from typing import Callable, Dict, List, Optional, Tuple

APPLEMIDI_SIGNATURE = b"\xff\xff"
PROTOCOL_VERSION = 2

#: RTP dynamic payload type for a MIDI stream.
PAYLOAD_TYPE = 0x61

#: AppleMIDI's clock is 10kHz -- one tick per 100 microseconds.
CLOCK_HZ = 10_000

DEFAULT_CONTROL_PORT = 5004
DEFAULT_NAME = "neuralGrid"

EXCHANGE_COMMANDS = (b"IN", b"NO", b"OK", b"BY", b"CK", b"RS")

Addr = Tuple[str, int]


def timestamp_units(t_seconds: float) -> int:
    """Seconds -> AppleMIDI clock ticks."""
    return int(t_seconds * CLOCK_HZ)


@dataclass(frozen=True)
class Exchange:
    command: str
    token: int
    ssrc: int
    name: Optional[str]


def build_exchange(command: str, token: int, ssrc: int, name: Optional[str]) -> bytes:
    pkt = (
        APPLEMIDI_SIGNATURE
        + command.encode("ascii")
        + struct.pack(">III", PROTOCOL_VERSION, token & 0xFFFFFFFF, ssrc & 0xFFFFFFFF)
    )
    if name is not None:
        pkt += name.encode("utf-8") + b"\x00"
    return pkt


def parse_exchange(data: bytes) -> Optional[Exchange]:
    """Return the session message, or None if this isn't one.

    The data port carries both these and RTP audio-style packets, so
    "not ours" is a normal answer, not an error.
    """
    if len(data) < 16 or data[:2] != APPLEMIDI_SIGNATURE:
        return None
    command = data[2:4]
    if command not in EXCHANGE_COMMANDS:
        return None
    _version, token, ssrc = struct.unpack(">III", data[4:16])
    name: Optional[str] = None
    rest = data[16:]
    if rest:
        name = rest.split(b"\x00", 1)[0].decode("utf-8", "replace")
    return Exchange(command.decode("ascii"), token, ssrc, name)


def build_rtp_midi(payload: bytes, seq: int, ts: int, ssrc: int) -> bytes:
    """One RTP packet carrying a MIDI command section.

    No recovery journal (J=0): we are on a wired LAN and a dropped note
    is better than a protocol we cannot verify by ear.
    """
    header = struct.pack(
        ">BBHII",
        0x80,                       # V=2, no padding, no extension, CC=0
        0x80 | PAYLOAD_TYPE,        # M=1, PT=97
        seq & 0xFFFF,
        ts & 0xFFFFFFFF,
        ssrc & 0xFFFFFFFF,
    )
    n = len(payload)
    if n <= 0x0F:
        # short form: B=0, one length nibble
        command_section = bytes([n]) + payload
    else:
        # long form: B=1, 12-bit length across two bytes
        command_section = bytes([0x80 | ((n >> 8) & 0x0F), n & 0xFF]) + payload
    return header + command_section


def _clamp7(v: int) -> int:
    return 0 if v < 0 else 127 if v > 127 else int(v)


class NetworkMidiOut:
    """Shaped like `pi/neuralgrid/midiout.py` so modes cannot tell the
    difference between a local port and a laptop across the room."""

    def __init__(self, _send: Callable[[bytes], None], name: str = DEFAULT_NAME,
                 ssrc: int = 0x53455054):
        self._send = _send
        self.name = name
        self.ssrc = ssrc
        self._seq = 0
        self._connected = False
        self._t0 = time.monotonic()

    # -- connection state ----------------------------------------------
    def connect(self) -> None:
        self._connected = True

    def disconnect(self) -> None:
        self._connected = False

    @property
    def connected(self) -> bool:
        return self._connected

    def list_ports(self) -> List[str]:
        return [f"{self.name} (network session)"]

    def select_port(self, name: str) -> bool:
        return True

    # -- sending -------------------------------------------------------
    def _emit(self, midi: bytes) -> None:
        if not self._connected:
            return  # nobody is listening; do not queue, this is live control
        pkt = build_rtp_midi(
            midi, seq=self._seq,
            ts=timestamp_units(time.monotonic() - self._t0),
            ssrc=self.ssrc,
        )
        self._seq = (self._seq + 1) & 0xFFFF
        self._send(pkt)

    def note_on(self, note: int, vel: int = 100, ch: int = 0) -> None:
        self._emit(bytes([0x90 | (ch & 0x0F), _clamp7(note), _clamp7(vel)]))

    def note_off(self, note: int, ch: int = 0) -> None:
        self._emit(bytes([0x80 | (ch & 0x0F), _clamp7(note), 0]))

    def cc(self, cc: int, val: int, ch: int = 0) -> None:
        self._emit(bytes([0xB0 | (ch & 0x0F), _clamp7(cc), _clamp7(val)]))

    def aftertouch(self, val: int, ch: int = 0) -> None:
        self._emit(bytes([0xD0 | (ch & 0x0F), _clamp7(val)]))

    def panic(self) -> None:
        for ch in range(16):
            self.cc(123, 0, ch=ch)   # all notes off


class RtpMidiSession:
    """The responder half: accept invitations, answer clock syncs, keep
    track of who is listening."""

    def __init__(self, name: str = DEFAULT_NAME,
                 _send: Optional[Callable[[bytes, Addr], None]] = None,
                 ssrc: int = 0x53455054):
        self.name = name
        self.ssrc = ssrc
        self._send = _send or (lambda data, addr: None)
        self.peers: Dict[int, Addr] = {}
        self._t0 = time.monotonic()
        self.on_change: Optional[Callable[[], None]] = None

    def _now(self) -> int:
        return timestamp_units(time.monotonic() - self._t0)

    def handle(self, data: bytes, addr: Addr) -> None:
        msg = parse_exchange(data)
        if msg is None:
            return
        if msg.command == "IN":
            self._send(
                build_exchange("OK", token=msg.token, ssrc=self.ssrc, name=self.name),
                addr,
            )
            self.peers[msg.ssrc] = addr
            self._changed()
        elif msg.command == "BY":
            if self.peers.pop(msg.ssrc, None) is not None:
                self._changed()
        elif msg.command == "CK":
            self._handle_clock(data, addr)

    def _handle_clock(self, data: bytes, addr: Addr) -> None:
        if len(data) < 36:
            return
        ssrc, count = struct.unpack(">IB", data[4:9])
        ts1 = struct.unpack(">Q", data[12:20])[0]
        if count != 0:
            return  # count 2 closes the exchange; nothing for us to say
        reply = (
            APPLEMIDI_SIGNATURE + b"CK"
            + struct.pack(">IBxxx", self.ssrc, 1)
            + struct.pack(">QQQ", ts1, self._now(), 0)
        )
        self._send(reply, addr)

    def _changed(self) -> None:
        if self.on_change:
            self.on_change()


class NetworkMidiServer:
    """Sockets, threads and the Bonjour advertisement.

    Kept apart from the protocol above so the fussy part stays testable
    without binding a port.
    """

    def __init__(self, name: str = DEFAULT_NAME, control_port: int = DEFAULT_CONTROL_PORT):
        self.name = name
        self.control_port = control_port
        self.data_port = control_port + 1
        self._control = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._data = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.session = RtpMidiSession(name=name, _send=self._reply)
        self.session.on_change = self._peers_changed
        self.out = NetworkMidiOut(_send=self._broadcast, name=name)
        self._stop = threading.Event()
        self._threads: List[threading.Thread] = []
        self._avahi = None
        self.last_error: Optional[str] = None

    # -- wiring --------------------------------------------------------
    def _reply(self, data: bytes, addr: Addr) -> None:
        sock = self._control if addr[1] == self.control_port else self._data
        try:
            sock.sendto(data, addr)
        except OSError as e:
            self.last_error = str(e)

    def _broadcast(self, pkt: bytes) -> None:
        # MIDI always goes to the DATA port of every connected peer.
        for ssrc, addr in list(self.session.peers.items()):
            try:
                self._data.sendto(pkt, (addr[0], self.data_port))
            except OSError as e:
                self.last_error = str(e)

    def _peers_changed(self) -> None:
        if self.session.peers:
            self.out.connect()
        else:
            self.out.disconnect()

    # -- lifecycle -----------------------------------------------------
    def start(self) -> None:
        try:
            self._control.bind(("0.0.0.0", self.control_port))
            self._data.bind(("0.0.0.0", self.data_port))
        except OSError as e:
            self.last_error = f"bind failed: {e}"
            return
        self._stop.clear()
        for sock in (self._control, self._data):
            t = threading.Thread(target=self._pump, args=(sock,), daemon=True)
            t.start()
            self._threads.append(t)
        self._advertise()

    def _pump(self, sock: socket.socket) -> None:
        sock.settimeout(0.5)
        while not self._stop.is_set():
            try:
                data, addr = sock.recvfrom(2048)
            except socket.timeout:
                continue
            except OSError:
                return
            self.session.handle(data, addr)

    def _advertise(self) -> None:
        """Publish _apple-midi._udp so the Mac's Network pane lists us
        without anyone typing an IP address."""
        import shutil
        import subprocess
        if not shutil.which("avahi-publish-service"):
            self.last_error = "avahi-publish-service missing; session is not advertised"
            return
        try:
            self._avahi = subprocess.Popen(
                ["avahi-publish-service", self.name, "_apple-midi._udp",
                 str(self.control_port)],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            )
        except OSError as e:
            self.last_error = f"advertise failed: {e}"

    def stop(self) -> None:
        self._stop.set()
        if self._avahi is not None:
            self._avahi.terminate()
            self._avahi = None
        for sock in (self._control, self._data):
            try:
                sock.close()
            except OSError:
                pass

    def state(self) -> dict:
        return {
            "name": self.name,
            "control_port": self.control_port,
            "peers": [f"{a[0]}:{a[1]}" for a in self.session.peers.values()],
            "connected": self.out.connected,
            "error": self.last_error,
        }


__all__ = [
    "APPLEMIDI_SIGNATURE", "PAYLOAD_TYPE", "CLOCK_HZ", "DEFAULT_CONTROL_PORT",
    "Exchange", "build_exchange", "parse_exchange", "build_rtp_midi",
    "timestamp_units", "NetworkMidiOut", "RtpMidiSession", "NetworkMidiServer",
]
