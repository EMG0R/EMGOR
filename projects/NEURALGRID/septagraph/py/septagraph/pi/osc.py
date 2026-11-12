"""Generic OSC -- the whole instrument, for anything that speaks OSC.

Monome mode speaks monome's protocol because that is the point of it,
and a monome has pads and nothing else. This is the other half: every
sensor out and the lights in, under one obvious address space, so Max,
SuperCollider, Pd or TouchDesigner can use the force sensors and the
lidar too without pretending to be a grid.

Finding each other is the same trick the monome shim uses -- anything
that sends us a message is registered as a listener, so a patch that
pokes the grid once starts receiving. That is not enough on its own
(Max's udpsend and udpreceive are separate objects on separate ports),
so `/neuralgrid/connect <port>` names where to reply.

    OUT                                   IN
    /neuralgrid/pad     x y state         /neuralgrid/led/set   x y r g b
    /neuralgrid/ribbon  i value           /neuralgrid/led/all   r g b
    /neuralgrid/force   i value           /neuralgrid/led/frame r g b * 64
    /neuralgrid/lidar   i mm near         /neuralgrid/led/clear
                                          /neuralgrid/connect    [host] port
                                          /neuralgrid/disconnect [host] port
"""
from __future__ import annotations

from typing import Callable, List, Optional, Sequence, Tuple

from . import leds
from .modes import Mode
from .monome import osc_decode, osc_encode

DEFAULT_PREFIX = "/neuralgrid"
DEFAULT_PORT = 7710          # 7700/7701 are the septagraph render/ctl pair

PAD = "/pad"
RIBBON = "/ribbon"
FORCE = "/force"
LIDAR = "/lidar"

N_RIBBON = 4
N_FORCE = 4
ANALOG_MAX = 4095.0
LIDAR_RANGE_MM = 400.0

#: How much an analog channel must move before it is worth a packet.
#: The analog stream runs at ~42-50Hz across 8 channels; forwarding all
#: of it unconditionally is 400 messages a second of mostly noise.
EPSILON = 1.0 / 127.0

#: What an undriven grid shows. Dark would be indistinguishable from a
#: crash, and unlike the encoder there is nothing else drawing here.
IDLE_COLOR: leds.RGB = (0, 60, 60)
IDLE_LEVEL = 0.5

Addr = Tuple[str, int]


def _clamp8(v) -> int:
    try:
        v = int(v)
    except (TypeError, ValueError):
        return 0
    return 0 if v < 0 else 255 if v > 255 else v


class OscMode(Mode):
    name = "osc"

    def __init__(self, send: Callable[[str, list], None], prefix: str = DEFAULT_PREFIX):
        self.prefix = prefix
        self._send = send
        self.targets: set = set()
        self._frame: Optional[List[leds.RGB]] = None   # None = nothing driving us
        self._last: dict = {}

    # -- out -----------------------------------------------------------
    def _emit(self, leaf: str, args: list) -> None:
        self._send(f"{self.prefix}{leaf}", args)

    def _emit_changed(self, key, leaf: str, args: list, value: float) -> None:
        prev = self._last.get(key)
        if prev is not None and abs(prev - value) < EPSILON:
            return
        self._last[key] = value
        self._emit(leaf, args)

    def on_key(self, x: int, y: int, edge: int) -> None:
        self._emit(PAD, [int(x), int(y), 1 if edge else 0])

    def on_analog(self, raw) -> None:
        raw = list(raw)
        for i in range(N_RIBBON):
            if i < len(raw):
                v = max(0.0, min(1.0, float(raw[i]) / ANALOG_MAX))
                self._emit_changed(("rib", i), RIBBON, [i, v], v)
        for i in range(N_FORCE):
            j = 4 + i
            if j < len(raw):
                v = max(0.0, min(1.0, float(raw[j]) / ANALOG_MAX))
                self._emit_changed(("frc", i), FORCE, [i, v], v)

    def on_lidar(self, mm4) -> None:
        for i, mm in enumerate(list(mm4)[:4]):
            if mm is None:
                continue    # no target: say nothing rather than send a lie
            near = 1.0 - max(0.0, min(1.0, float(mm) / LIDAR_RANGE_MM))
            self._emit_changed(("lid", i), LIDAR, [i, int(mm), near], near)

    # -- in ------------------------------------------------------------
    def on_packet(self, data: bytes, addr: Addr) -> None:
        msg = osc_decode(data)
        if msg is None:
            return
        # Anything that talks to us is a listener, so a patch that pokes
        # the grid once starts receiving without being told to.
        self.targets.add(addr)
        self.handle(msg[0], msg[1], addr=addr)

    def handle(self, address: str, args: Sequence, addr: Optional[Addr] = None) -> None:
        if not address.startswith(self.prefix):
            return
        leaf = address[len(self.prefix):]
        try:
            if leaf == "/led/set":
                x, y, r, g, b = (list(args) + [0] * 5)[:5]
                x, y = int(x), int(y)
                if 0 <= x < 8 and 0 <= y < 8:
                    self._ensure_frame()[leds.idx(x, y)] = (
                        _clamp8(r), _clamp8(g), _clamp8(b))
            elif leaf == "/led/all":
                r, g, b = (list(args) + [0] * 3)[:3]
                self._frame = [(_clamp8(r), _clamp8(g), _clamp8(b))] * leds.N_LEDS
            elif leaf == "/led/frame":
                vals = list(args)
                frame = self._ensure_frame()
                for i in range(min(leds.N_LEDS, len(vals) // 3)):
                    frame[i] = (_clamp8(vals[i * 3]), _clamp8(vals[i * 3 + 1]),
                                _clamp8(vals[i * 3 + 2]))
            elif leaf == "/led/clear":
                self._frame = leds.blank()
            elif leaf == "/connect":
                t = self._target_from(args, addr)
                if t:
                    self.targets.add(t)
            elif leaf == "/disconnect":
                t = self._target_from(args, addr)
                if t:
                    self.targets.discard(t)
        except (TypeError, ValueError, IndexError):
            return   # a malformed message must never take the grid down

    def _target_from(self, args: Sequence, addr: Optional[Addr]) -> Optional[Addr]:
        args = list(args)
        if len(args) >= 2:
            return (str(args[0]), int(args[1]))
        if len(args) == 1 and addr is not None:
            return (addr[0], int(args[0]))
        return addr

    def _ensure_frame(self) -> List[leds.RGB]:
        if self._frame is None:
            self._frame = leds.blank()
        return self._frame

    # -- looks ---------------------------------------------------------
    def led_frame(self):
        if self._frame is not None:
            return list(self._frame)
        return [leds.dim(IDLE_COLOR, IDLE_LEVEL)] * leds.N_LEDS

    def panic(self) -> None:
        return

    def state(self) -> dict:
        return {
            "prefix": self.prefix,
            "port": DEFAULT_PORT,
            "listeners": [f"{h}:{p}" for h, p in sorted(self.targets)],
            "driven": self._frame is not None,
        }


__all__ = [
    "OscMode", "DEFAULT_PREFIX", "DEFAULT_PORT",
    "PAD", "RIBBON", "FORCE", "LIDAR", "LIDAR_RANGE_MM", "IDLE_COLOR",
]


class OscServer:
    """The socket half: one UDP port in, every listener out, advertised
    over Bonjour as `_osc._udp` so patches that browse can find it."""

    def __init__(self, mode: "OscMode", port: int = DEFAULT_PORT,
                 name: str = "neuralGrid"):
        import socket
        import threading
        self.mode = mode
        self.port = port
        self.name = name
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._stop = threading.Event()
        self._avahi = None
        self.last_error: Optional[str] = None
        mode._send = self.send

    def send(self, address: str, args: list) -> None:
        if not self.mode.targets:
            return      # nobody listening; this is live control, not a queue
        pkt = osc_encode(address, *args)
        for target in list(self.mode.targets):
            try:
                self._sock.sendto(pkt, target)
            except OSError as e:
                self.last_error = str(e)
                self.mode.targets.discard(target)

    def start(self) -> None:
        import threading
        try:
            self._sock.bind(("0.0.0.0", self.port))
        except OSError as e:
            self.last_error = f"bind failed: {e}"
            return
        self._stop.clear()
        threading.Thread(target=self._pump, daemon=True).start()
        self._advertise()

    def _pump(self) -> None:
        import socket
        self._sock.settimeout(0.5)
        while not self._stop.is_set():
            try:
                data, addr = self._sock.recvfrom(8192)
            except socket.timeout:
                continue
            except OSError:
                return
            self.mode.on_packet(data, addr)

    def _advertise(self) -> None:
        import shutil
        import subprocess
        if not shutil.which("avahi-publish-service"):
            self.last_error = "avahi-publish-service missing; OSC is not advertised"
            return
        try:
            self._avahi = subprocess.Popen(
                ["avahi-publish-service", self.name, "_osc._udp", str(self.port)],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            )
        except OSError as e:
            self.last_error = f"advertise failed: {e}"

    def stop(self) -> None:
        self._stop.set()
        if self._avahi is not None:
            self._avahi.terminate()
            self._avahi = None
        try:
            self._sock.close()
        except OSError:
            pass

    def state(self) -> dict:
        d = dict(self.mode.state())
        d["port"] = self.port
        d["error"] = self.last_error
        return d


__all__.append("OscServer")
