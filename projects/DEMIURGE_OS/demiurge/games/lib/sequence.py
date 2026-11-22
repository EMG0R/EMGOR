"""
sequence.py — per-game button-CODE launcher (Konami-code style).

In SYNTH mode the supervisor watches for per-game button SEQUENCES on the PRESS
edges of the arcade + face buttons. Each registered code maps to a game; when the
most recent presses match a code's suffix, that game launches.

Tokens -> Teensy frame button fields (canonical map, see docs/BUTTONS.md):
    ARCADE = b1    A = b9    B = b8    X = b6    Y = b7

Example codes:
    Galaga  : ARCADE,Y,ARCADE,X,ARCADE,A,ARCADE
    Dig Dug : ARCADE,Y,ARCADE,B,ARCADE,X,ARCADE

Only run this in SYNTH mode — in a game these same buttons are gameplay (fire,
etc.), so matching there would false-trigger. Game EXIT is a separate gesture
(jb-hold), handled by the supervisor, not here.

Pure logic, no hardware -> unit-tested in tests/test_sequence.py.
"""

from __future__ import annotations

# launch-code token -> Teensy frame button field
TOKEN_FIELD = {"ARCADE": "b1", "A": "b9", "B": "b8", "X": "b6", "Y": "b7"}


class SequenceDetector:
    """Matches recent button-press sequences against registered codes.

    Feed every frame to `update(frame, now)`. Returns the matched game label
    exactly once when a code completes, else None. A gap longer than
    `gap_seconds` between presses resets the in-progress sequence.
    """

    def __init__(self, codes: dict[str, list[str]], gap_seconds: float = 2.5,
                 logger=None):
        # label -> normalized token list
        self.codes = {label: [t.upper() for t in toks]
                      for label, toks in codes.items() if toks}
        self.gap_seconds = gap_seconds
        # Optional debug hook: logger(token, buffer_list) called on each press.
        # Lets the supervisor show exactly what a launch code is detecting.
        self.logger = logger
        self._maxlen = max((len(c) for c in self.codes.values()), default=0)
        self._buf: list[str] = []
        self._last_t: float | None = None
        self._prev: dict[str, int] = {f: 0 for f in TOKEN_FIELD.values()}

    def update(self, frame: dict, now: float) -> str | None:
        fired = None
        # Deterministic order so simultaneous presses (rare) are reproducible.
        for token, field in TOKEN_FIELD.items():
            cur = 1 if frame.get(field, 0) else 0
            prev = self._prev[field]
            self._prev[field] = cur
            if cur and not prev:                 # rising edge = a press
                got = self._push(token, now)
                if got:
                    fired = got
        return fired

    def _push(self, token: str, now: float) -> str | None:
        if self._last_t is not None and (now - self._last_t) > self.gap_seconds:
            self._buf.clear()
        self._last_t = now
        self._buf.append(token)
        if self._maxlen and len(self._buf) > self._maxlen:
            self._buf = self._buf[-self._maxlen:]
        if self.logger:
            self.logger(token, list(self._buf))
        for label, code in self.codes.items():
            n = len(code)
            if n and self._buf[-n:] == code:
                self._buf.clear()                # consume on match
                return label
        return None

    def reset(self) -> None:
        self._buf.clear()
        self._last_t = None
        self._prev = {f: 0 for f in TOKEN_FIELD.values()}
