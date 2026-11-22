"""Be a monome grid, the way a monome grid actually is one.

A real grid is a USB device; `serialoscd` on the host enumerates it,
advertises it over Bonjour as `_monome-osc._udp`, and exposes it as an
OSC endpoint. Applications never touch the hardware -- they ask serialosc
what exists, then trade `/monome/grid/led/*` in and `/monome/grid/key`
out. So "look like a grid" means implementing that endpoint plus the
discovery service in front of it, which is what this module is.

We are both halves at once (the device AND its serialosc), because the
device here is on the far side of ethernet rather than a USB port.

Protocol: monome.org/docs/osc/ (serialosc + grid).
"""
from __future__ import annotations

import socket
import struct
import threading
from typing import Callable, List, Optional, Sequence, Tuple

from . import leds
from .modes import Mode

DEFAULT_PREFIX = "/monome"
GRID_W = 8
GRID_H = 8

#: Varibright depth. A press-lit grid uses only 0 and 15, but norns and
#: most scripts drive levels, so the full range is kept.
GRID_LEVELS = 16

DEFAULT_SERIALOSC_PORT = 12002
DEFAULT_DEVICE_PORT = 15000
DEFAULT_DEVICE_ID = "neuralgrid"
DEFAULT_DEVICE_TYPE = "monome 64"

#: The monome white/yellow. Lit pads are this, scaled by level.
MONOME_COLOR: leds.RGB = (255, 225, 90)

Addr = Tuple[str, int]


# -- a small OSC codec ---------------------------------------------------
# Only what the grid protocol uses: ints, floats and strings, no bundles.

def _pad(b: bytes) -> bytes:
    return b + b"\x00" * ((4 - len(b) % 4) % 4)


def _pad_str(s: str) -> bytes:
    return _pad(s.encode("utf-8") + b"\x00")


def osc_encode(address: str, *args) -> bytes:
    tags = ","
    body = b""
    for a in args:
        if isinstance(a, bool):
            tags += "i"
            body += struct.pack(">i", 1 if a else 0)
        elif isinstance(a, int):
            tags += "i"
            body += struct.pack(">i", a)
        elif isinstance(a, float):
            tags += "f"
            body += struct.pack(">f", a)
        else:
            tags += "s"
            body += _pad_str(str(a))
    return _pad_str(address) + _pad_str(tags) + body


def osc_decode(data: bytes) -> Optional[Tuple[str, List]]:
    """Return (address, args), or None if this isn't an OSC message."""
    try:
        if not data or data[0:1] != b"/":
            return None
        end = data.index(b"\x00")
        address = data[:end].decode("utf-8")
        i = (end + 4) & ~3
        if i >= len(data) or data[i:i + 1] != b",":
            return (address, [])
        tend = data.index(b"\x00", i)
        tags = data[i + 1:tend].decode("ascii")
        i = (tend + 4) & ~3
        args: List = []
        for t in tags:
            if t == "i":
                args.append(struct.unpack(">i", data[i:i + 4])[0])
                i += 4
            elif t == "f":
                args.append(struct.unpack(">f", data[i:i + 4])[0])
                i += 4
            elif t == "s":
                send = data.index(b"\x00", i)
                args.append(data[i:send].decode("utf-8", "replace"))
                i = (send + 4) & ~3
            else:
                return (address, args)   # unknown tag: keep what we have
        return (address, args)
    except (ValueError, struct.error, IndexError):
        return None


# -- the grid itself -----------------------------------------------------

class MonomeGrid:
    """LED state, driven entirely by whatever is on the other end."""

    def __init__(self, width: int = GRID_W, height: int = GRID_H):
        self.width = width
        self.height = height
        self._level = [[0] * width for _ in range(height)]
        self.intensity = GRID_LEVELS - 1

    def level(self, x: int, y: int) -> int:
        if 0 <= x < self.width and 0 <= y < self.height:
            return self._level[y][x]
        return 0

    def _set(self, x: int, y: int, lvl: int) -> None:
        if 0 <= x < self.width and 0 <= y < self.height:
            self._level[y][x] = max(0, min(GRID_LEVELS - 1, int(lvl)))

    def _on_off(self, s) -> int:
        return GRID_LEVELS - 1 if int(s) else 0

    def handle(self, address: str, args: Sequence) -> None:
        """Apply one /grid/led/* message. Unknown addresses are ignored --
        a grid that crashes on an unexpected message is not a grid."""
        try:
            leaf = address.split("/grid/led/", 1)[1] if "/grid/led/" in address else None
            if leaf is None:
                return
            if leaf == "set":
                x, y, s = args[:3]
                self._set(int(x), int(y), self._on_off(s))
            elif leaf == "all":
                lvl = self._on_off(args[0])
                for y in range(self.height):
                    for x in range(self.width):
                        self._set(x, y, lvl)
            elif leaf == "row":
                self._row(int(args[0]), int(args[1]), args[2:], self._on_off)
            elif leaf == "col":
                self._col(int(args[0]), int(args[1]), args[2:], self._on_off)
            elif leaf == "map":
                self._map(int(args[0]), int(args[1]), args[2:])
            elif leaf == "level/set":
                x, y, lvl = args[:3]
                self._set(int(x), int(y), int(lvl))
            elif leaf == "level/all":
                lvl = int(args[0])
                for y in range(self.height):
                    for x in range(self.width):
                        self._set(x, y, lvl)
            elif leaf == "level/row":
                self._row(int(args[0]), int(args[1]), args[2:], None)
            elif leaf == "level/col":
                self._col(int(args[0]), int(args[1]), args[2:], None)
            elif leaf == "level/map":
                self._level_map(int(args[0]), int(args[1]), args[2:])
            elif leaf == "intensity":
                self.intensity = max(0, min(GRID_LEVELS - 1, int(args[0])))
        except (IndexError, ValueError, TypeError):
            return

    def _row(self, x_off: int, y: int, data, conv) -> None:
        if conv is None:                      # level/row: one level per pad
            for i, lvl in enumerate(data):
                self._set(x_off + i, y, int(lvl))
            return
        for byte_i, mask in enumerate(data):  # led/row: a bitmask per byte
            for bit in range(8):
                self._set(x_off + byte_i * 8 + bit, y,
                          conv((int(mask) >> bit) & 1))

    def _col(self, x: int, y_off: int, data, conv) -> None:
        if conv is None:
            for i, lvl in enumerate(data):
                self._set(x, y_off + i, int(lvl))
            return
        for byte_i, mask in enumerate(data):
            for bit in range(8):
                self._set(x, y_off + byte_i * 8 + bit,
                          conv((int(mask) >> bit) & 1))

    def _map(self, x_off: int, y_off: int, rows) -> None:
        for dy, mask in enumerate(list(rows)[:8]):
            for bit in range(8):
                self._set(x_off + bit, y_off + dy,
                          self._on_off((int(mask) >> bit) & 1))

    def _level_map(self, x_off: int, y_off: int, levels) -> None:
        for i, lvl in enumerate(list(levels)[:64]):
            self._set(x_off + (i % 8), y_off + (i // 8), int(lvl))

    def led_frame(self) -> List[leds.RGB]:
        frame = leds.blank()
        scale = (self.intensity + 1) / GRID_LEVELS
        for y in range(min(self.height, 8)):
            for x in range(min(self.width, 8)):
                lvl = self._level[y][x]
                if lvl:
                    frame[leds.idx(x, y)] = leds.dim(
                        MONOME_COLOR, (lvl / (GRID_LEVELS - 1)) * scale
                    )
        return frame


# -- discovery -----------------------------------------------------------

class SerialoscServer:
    """The `/serialosc/*` half: what exists, and tell me when that changes."""

    def __init__(self, device_id: str = DEFAULT_DEVICE_ID,
                 device_type: str = DEFAULT_DEVICE_TYPE,
                 device_port: int = DEFAULT_DEVICE_PORT,
                 _send: Optional[Callable[[str, list, Addr], None]] = None):
        self.device_id = device_id
        self.device_type = device_type
        self.device_port = device_port
        self._send = _send or (lambda a, args, addr: None)
        #: Clients waiting to hear about the NEXT device change. One-shot,
        #: exactly like serialosc: it sends a single message and forgets you.
        self.watchers: set = set()

    def handle(self, address: str, args: Sequence, addr: Addr) -> None:
        if address not in ("/serialosc/list", "/serialosc/notify"):
            return
        try:
            host, port = str(args[0]), int(args[1])
        except (IndexError, ValueError, TypeError):
            return
        to = (host, port)
        if address == "/serialosc/list":
            self._send("/serialosc/device",
                       [self.device_id, self.device_type, self.device_port], to)
        else:
            # notify means "tell me when this CHANGES", not "tell me now".
            # Answering immediately invites a client that re-lists on every
            # add -- matron does -- into a list -> notify -> add -> list
            # spin, which is a busy loop over the network.
            self.watchers.add(to)

    def announce_added(self) -> None:
        """Tell everyone waiting that the device appeared, once each."""
        for to in list(self.watchers):
            self._send("/serialosc/add", [self.device_id], to)
        self.watchers.clear()

    def announce_removed(self) -> None:
        for to in list(self.watchers):
            self._send("/serialosc/remove", [self.device_id], to)
        self.watchers.clear()


# -- the mode ------------------------------------------------------------

class MonomeMode(Mode):
    """The grid, as seen by whatever is driving it. The neuralGrid's own
    sensors stay out of it -- a monome has pads and nothing else, and
    inventing extra messages would make us a grid that lies."""

    name = "monome"

    def __init__(self, send: Callable[[str, list], None],
                 _reply: Optional[Callable[[str, list, Addr], None]] = None,
                 device_id: str = DEFAULT_DEVICE_ID,
                 device_port: int = DEFAULT_DEVICE_PORT):
        self.grid = MonomeGrid()
        self.prefix = DEFAULT_PREFIX
        self.device_id = device_id
        self.device_port = device_port
        self._send = send
        self._reply = _reply or (lambda a, args, addr: None)
        self.host: Optional[str] = None
        self.port: Optional[int] = None

    # -- from the grid to the app --------------------------------------
    def on_key(self, x: int, y: int, edge: int) -> None:
        self._send(f"{self.prefix}/grid/key", [int(x), int(y), 1 if edge else 0])

    def on_analog(self, raw) -> None:
        return

    def on_lidar(self, mm4) -> None:
        return

    # -- from the app to the grid --------------------------------------
    def handle(self, address: str, args: Sequence, addr: Optional[Addr] = None) -> None:
        if address.startswith("/sys/"):
            self._handle_sys(address, args, addr)
            return
        if "/grid/led/" in address:
            self.grid.handle(address, args)

    def _handle_sys(self, address: str, args: Sequence, addr: Optional[Addr]) -> None:
        leaf = address[len("/sys/"):]
        if leaf == "prefix" and args:
            p = str(args[0])
            self.prefix = p if p.startswith("/") else "/" + p
        elif leaf == "host" and args:
            self.host = str(args[0])
        elif leaf == "port" and args:
            self.port = int(args[1] if len(args) > 1 else args[0])
        elif leaf == "info":
            self._send_info(args, addr)

    def _send_info(self, args: Sequence, addr: Optional[Addr]) -> None:
        to: Optional[Addr] = None
        if len(args) >= 2:
            to = (str(args[0]), int(args[1]))
        elif len(args) == 1 and self.host:
            to = (self.host, int(args[0]))
        elif addr is not None:
            to = addr
        if to is None:
            return
        for address, vals in (
            ("/sys/id", [self.device_id]),
            ("/sys/size", [self.grid.width, self.grid.height]),
            ("/sys/host", [self.host or to[0]]),
            ("/sys/port", [self.port or to[1]]),
            ("/sys/prefix", [self.prefix]),
            ("/sys/rotation", [0]),
        ):
            self._reply(address, vals, to)

    def led_frame(self):
        return self.grid.led_frame()

    def panic(self) -> None:
        return

    def state(self) -> dict:
        return {
            "device_id": self.device_id,
            "prefix": self.prefix,
            "size": [self.grid.width, self.grid.height],
            "host": self.host,
            "port": self.port,
            "device_port": self.device_port,
        }


class MonomeServer:
    """Sockets + Bonjour: the serialosc port, the device port, and the
    `_monome-osc._udp` advertisement that makes it discoverable."""

    def __init__(self, mode: MonomeMode,
                 serialosc_port: int = DEFAULT_SERIALOSC_PORT,
                 device_port: int = DEFAULT_DEVICE_PORT):
        self.mode = mode
        self.serialosc_port = serialosc_port
        self.device_port = device_port
        self._disc = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._dev = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.serialosc = SerialoscServer(
            device_id=mode.device_id, device_port=device_port, _send=self._send_to
        )
        # The device has to be able to ANSWER -- /sys/info is how anything
        # asks "what are you?", and a grid that stays silent there is not
        # discoverable however well it is advertised. Late-bound so the
        # send path can be swapped out (tests, or a different transport).
        mode._reply = lambda a, args, addr: self._send_to(a, args, addr)
        self._stop = threading.Event()
        self._avahi = None
        self.last_error: Optional[str] = None

    def _send_to(self, address: str, args: list, addr: Addr) -> None:
        try:
            self._dev.sendto(osc_encode(address, *args), addr)
        except OSError as e:
            self.last_error = str(e)

    def send_key(self, address: str, args: list) -> None:
        if self.mode.host and self.mode.port:
            self._send_to(address, args, (self.mode.host, self.mode.port))

    def start(self) -> None:
        try:
            self._disc.bind(("0.0.0.0", self.serialosc_port))
            self._dev.bind(("0.0.0.0", self.device_port))
        except OSError as e:
            self.last_error = f"bind failed: {e}"
            return
        self._stop.clear()
        threading.Thread(target=self._pump, args=(self._disc, True), daemon=True).start()
        threading.Thread(target=self._pump, args=(self._dev, False), daemon=True).start()
        self._advertise()

    def _pump(self, sock: socket.socket, discovery: bool) -> None:
        sock.settimeout(0.5)
        while not self._stop.is_set():
            try:
                data, addr = sock.recvfrom(4096)
            except socket.timeout:
                continue
            except OSError:
                return
            msg = osc_decode(data)
            if msg is None:
                continue
            address, args = msg
            if discovery:
                self.serialosc.handle(address, args, addr)
            else:
                self.mode.handle(address, args, addr)

    def _advertise(self) -> None:
        import shutil
        import subprocess
        if not shutil.which("avahi-publish-service"):
            self.last_error = "avahi-publish-service missing; grid is not advertised"
            return
        try:
            self._avahi = subprocess.Popen(
                ["avahi-publish-service", self.mode.device_id, "_monome-osc._udp",
                 str(self.device_port)],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            )
        except OSError as e:
            self.last_error = f"advertise failed: {e}"

    def stop(self) -> None:
        self._stop.set()
        if self._avahi is not None:
            self._avahi.terminate()
            self._avahi = None
        for s in (self._disc, self._dev):
            try:
                s.close()
            except OSError:
                pass

    def state(self) -> dict:
        d = dict(self.mode.state())
        d["serialosc_port"] = self.serialosc_port
        d["error"] = self.last_error
        return d


__all__ = [
    "DEFAULT_PREFIX", "GRID_LEVELS", "MONOME_COLOR", "DEFAULT_SERIALOSC_PORT",
    "DEFAULT_DEVICE_PORT", "DEFAULT_DEVICE_ID", "DEFAULT_DEVICE_TYPE",
    "osc_encode", "osc_decode", "MonomeGrid", "SerialoscServer", "MonomeMode",
    "MonomeServer",
]
