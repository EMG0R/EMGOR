"""sinks — one file per consumer, all behind the same tiny interface:

    send(target: dict, value: float) -> None

`target` is the map's "target" dict from sensors.json, e.g.
    {"sink": "csound", "kind": "expr", "id": 30}
    {"sink": "csound", "kind": "cc", "ch": 1, "cc": 20}
    {"sink": "osc", "host": "127.0.0.1", "port": 9002, "address": "/cutoff"}
    {"sink": "midi", "ch": 1, "cc": 20}
    {"param": "/csound/gkMasterVol"}   # resolved to one of the above by pool.py

Keeping every sink to this one method is what lets router.py stay ignorant
of transport entirely, and is what lets a future video.py/game.py sink drop
in without touching router.py or daemon.py's dispatch loop — only the
{"sources": [...], "sink registry"} wiring in daemon.py needs to grow a line.
"""

from typing import Protocol


class Sink(Protocol):
    def send(self, target: dict, value: float) -> None: ...


class Coalescer:
    """Per-key rate cap that keeps the LATEST value (parameters.md §3.1: "per-
    parameter rate cap, default 100 Hz, coalesce to latest").

    offer() says whether to send now; if not, the value is parked and drain()
    hands it back once the key's interval has elapsed. The daemon's main loop
    calls sink.flush() every tick, so a parked final value is never lost — a
    knob that stops moving still lands on its last position. Pure (time is
    passed in), so it is unit-testable without sleeping."""

    def __init__(self, hz: float = 100.0):
        self.min_dt = 1.0 / hz if hz > 0 else 0.0
        self._last: dict = {}
        self._pending: dict = {}

    def offer(self, key, value, now: float) -> bool:
        last = self._last.get(key)
        if last is None or (now - last) >= self.min_dt:
            self._last[key] = now
            self._pending.pop(key, None)
            return True
        self._pending[key] = value
        return False

    def drain(self, now: float) -> list:
        out = []
        for key in list(self._pending):
            if (now - self._last[key]) >= self.min_dt:
                self._last[key] = now
                out.append((key, self._pending.pop(key)))
        return out
