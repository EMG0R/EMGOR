"""sinks/midi.py — CC out to the ALSA MIDI pool (Midi Through, 14:0).

Same one-method interface as every sink. Target: {"sink": "midi", "ch": 1,
"cc": 20}; the value is the pool's 0..1 float, scaled here to 0..127 (MIDI is
an OUTPUT FORMAT only — parameters.md §3.2).

ALLOWLIST (parameters.md §3.1, _EMORY hard rule 7): never emits the clock CCs
(ch16 CC104-119 — clock.md) or ch1 CC29 (the DUODECIMUS expression pedal).
Anything else that is reserved goes in RESERVED. A blocked write is dropped
and logged once; the sink never raises into the dispatch loop.

mido is imported lazily and the port opened on first use, so importing this
module (tests, --dry-run) never touches ALSA. `opener` is injectable for tests.
"""

from __future__ import annotations

import sys
import time

from . import Coalescer

CLOCK_CH = 16
CLOCK_CC_RANGE = range(104, 120)       # 104..119
RESERVED = {(1, 29)}                    # (ch, cc): DUODECIMUS pedal
RETRY_S = 5.0


def is_blocked(ch: int, cc: int) -> bool:
    return (ch == CLOCK_CH and cc in CLOCK_CC_RANGE) or (ch, cc) in RESERVED


def _default_opener(name_hint: str):
    import mido
    names = mido.get_output_names()
    for n in names:
        if name_hint.lower() in n.lower():
            return mido.open_output(n)
    raise OSError(f"no MIDI output matching {name_hint!r} (have {names})")


class MidiSink:
    def __init__(self, port_hint: str = "Midi Through", max_hz: float = 100.0,
                 opener=None, clock=time.monotonic):
        self.port_hint = port_hint
        self._opener = opener or _default_opener
        self._port = None
        self._retry_at = 0.0
        self._co = Coalescer(max_hz)
        self._clock = clock
        self._warned: set = set()

    def _get_port(self):
        if self._port is None and self._clock() >= self._retry_at:
            try:
                self._port = self._opener(self.port_hint)
            except Exception as e:      # noqa: BLE001 — never raise into dispatch
                self._retry_at = self._clock() + RETRY_S
                print(f"[demiurge-io] midi sink: cannot open output ({e}); "
                      f"retry in {RETRY_S:.0f}s", file=sys.stderr, flush=True)
        return self._port

    def _emit(self, key, value) -> None:
        ch, cc = key
        port = self._get_port()
        if port is None:
            return
        v = max(0, min(127, int(round(float(value) * 127.0))))
        try:
            import mido
            port.send(mido.Message("control_change", channel=ch - 1, control=cc, value=v))
        except Exception as e:          # noqa: BLE001
            self._port = None
            print(f"[demiurge-io] midi sink send failed ({e})", file=sys.stderr, flush=True)

    def send(self, target: dict, value: float) -> None:
        ch = int(target.get("ch", 1))
        cc = int(target["cc"])
        if not (1 <= ch <= 16 and 0 <= cc <= 127):
            return
        if is_blocked(ch, cc):
            if (ch, cc) not in self._warned:
                self._warned.add((ch, cc))
                print(f"[demiurge-io] midi sink: ch{ch} CC{cc} is reserved; dropped",
                      file=sys.stderr, flush=True)
            return
        key = (ch, cc)
        if self._co.offer(key, value, self._clock()):
            self._emit(key, value)

    def flush(self) -> None:
        for key, value in self._co.drain(self._clock()):
            self._emit(key, value)
