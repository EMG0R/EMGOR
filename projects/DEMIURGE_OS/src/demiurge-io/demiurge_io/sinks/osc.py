"""sinks/osc.py — generic stage sink: one float to (host, port, address).

The pool's egress toward every non-Csound engine (SC, Pd, ChucK, Faust, RNBO,
Strudel ... see docs/parameters.md §6). Same single-method interface as
CsoundSink — `send(target, value)` — so router.py and the daemon's dispatch
never learn it exists. Target dict:

    {"sink": "osc", "host": "127.0.0.1", "port": 9002, "address": "/cutoff",
     "native": {"min": 40, "max": 12000, "curve": "exp"}}      # native optional

Hook mode (docs/parameters.md §11): {"sink": "osc", "host", "port", "proto": "p",
"path": "/stage/name"} sends `/p <path:s> <val:f>` with the raw 0..1 value (no
`native`, no `address`) to engines using the demiurge_params hooks.

Value in is the pool's 0..1 float. `native`, when present, denormalizes it at
the last inch (the adapter's job, never the pool's) — engines that take 0..1
omit it.

SAFETY (parameters.md §3.1): loopback destinations only unless the host is on
an explicit allowlist; nothing is ever sent to a remote host by default, which
is also what keeps this from ever aiming at Machine Lab robots. Per-address
rate cap, coalesce-to-latest (Coalescer); the daemon calls flush() each tick.
"""

from __future__ import annotations

import socket
import sys
import time

from ..osc import encode_osc
from . import Coalescer

LOOPBACK = ("127.0.0.1", "localhost", "::1")


def denormalize(v: float, native: dict | None) -> float:
    """0..1 -> native units. `curve`: lin (default) or exp (needs min > 0)."""
    v = 0.0 if v < 0.0 else (1.0 if v > 1.0 else float(v))
    if not native:
        return v
    lo = float(native.get("min", 0.0))
    hi = float(native.get("max", 1.0))
    if native.get("curve") == "exp" and lo > 0.0 and hi > 0.0:
        return lo * (hi / lo) ** v
    return lo + v * (hi - lo)


class OscSink:
    def __init__(self, allow_hosts=(), max_hz: float = 100.0, clock=time.monotonic):
        self.allow_hosts = set(allow_hosts)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._co = Coalescer(max_hz)
        self._clock = clock
        self._warned: set = set()

    def _allowed(self, host: str) -> bool:
        return host in LOOPBACK or host in self.allow_hosts

    def _emit(self, key, value) -> None:
        host, port, address, native = key
        if isinstance(address, tuple):      # hook mode: ("/p", path) -> /p <path:s> <0..1:f>
            v = 0.0 if value < 0.0 else (1.0 if value > 1.0 else float(value))
            packet = encode_osc(address[0], address[1], v)
        else:
            packet = encode_osc(address, float(denormalize(value, dict(native) if native else None)))
        try:
            self.sock.sendto(packet, (host, port))
        except OSError:
            pass   # fire-and-forget, like every other sink

    def send(self, target: dict, value: float) -> None:
        host = target.get("host", "127.0.0.1")
        if not self._allowed(host):
            if host not in self._warned:
                self._warned.add(host)
                print(f"[demiurge-io] osc sink: refusing non-loopback host {host!r} "
                      f"(not on the allowlist)", file=sys.stderr, flush=True)
            return
        native = target.get("native")
        if target.get("proto") == "p":       # hook engines: /p <path> <0..1>, never denormalized
            address, native = ("/p", target["path"]), None
        else:
            address = target["address"]
        key = (host, int(target["port"]), address,
               tuple(sorted(native.items())) if native else None)
        if self._co.offer(key, value, self._clock()):
            self._emit(key, value)

    def flush(self) -> None:
        for key, value in self._co.drain(self._clock()):
            self._emit(key, value)
