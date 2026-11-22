"""sources/osc_in.py — the pool's OSC ingress: `/p <path:string> <val:float>`.

Anyone (web UI, phone, a script, an engine reporting its own value) publishes
a PARAMETER with one datagram to DEMIURGE_POOL_PORT (default 9102, outside the
9000-9099 stage range and clear of 7770/8000-8004/1234):

    /p "/sc-pad/cutoff" 0.42

This is a direct parameter write, not a sensor reading: it skips conditioning
and the mapping matrix (docs/parameters.md §1.4) and goes to Pool.publish via
the daemon's queue — so the single consumer thread stays the single writer
toward engines, exactly like Reading items. Hence the third callback: this
source sets `accepts_param = True` and daemon._run_source hands it on_param.

Note the verb collision is deliberate-but-harmless: csound's own 7770 `/p`
takes an INT id; this one takes a STRING path. A datagram whose first arg is
not a string is ignored here, never forwarded.

Loopback bind by default. Malformed / non-finite / wrong-shape packets are
dropped silently — an untrusted sender must not be able to stop the thread.
"""

from __future__ import annotations

import math
import socket

from ..osc import decode_packet

HOST_DEFAULT = "127.0.0.1"
PORT_DEFAULT = 9102


def parse_pool_packet(data: bytes, verb: str = "/p") -> list[tuple[str, float]]:
    """Datagram -> [(path, value)] for every well-formed `<verb> <str> <num>`.
    verb "/p" = a write toward engines; "/pout" = an engine REPORTING its own
    value (docs/parameters.md §11): echoed to the UI, never sent back."""
    out = []
    for addr, args in decode_packet(data):
        if addr != verb or len(args) != 2:
            continue
        path, val = args
        if not isinstance(path, str) or not path.startswith("/"):
            continue
        if isinstance(val, str):
            continue
        val = float(val)
        if math.isnan(val) or math.isinf(val):
            continue
        out.append((path, val))
    return out


class OscInSource:
    id = "osc_in"
    descriptors: dict = {}
    accepts_param = True

    def __init__(self, host: str = HOST_DEFAULT, port: int = PORT_DEFAULT):
        self.host = host
        self.port = port
        self.sock: socket.socket | None = None

    def calibration(self, source_id: str):
        return None

    def _open(self) -> socket.socket:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.bind((self.host, self.port))
        return s

    def run(self, on_reading, on_control=None, on_param=None) -> None:
        del on_reading, on_control
        if self.sock is None:
            self.sock = self._open()
        while True:
            data, _addr = self.sock.recvfrom(4096)
            if on_param is None:
                continue
            for path, val in parse_pool_packet(data):
                on_param(path, val)
            for path, val in parse_pool_packet(data, "/pout"):
                on_param(path, val, True)
