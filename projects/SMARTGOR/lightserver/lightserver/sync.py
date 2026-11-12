"""Output-delay AV sync: the physical LED outputs (strips + satellites)
are delayed to match a slow external projection consumer (GORMAPPER),
while canvas taps (pixel + semantic) keep sending the freshest frame
immediately.

Why: the tap -> GORMAPPER render -> projector-glass chain has ~100-200ms
of unavoidable latency the LEDs don't have. Rather than trying to predict
or speed up the projector path, we just make the LEDs wait: buffer the
last N rendered ring frames and, once a delay is set, output the frame
from `output_delay_ms` ago on the PHYSICAL outputs while the taps
(GORMAPPER's own feed) still get the current frame. The result: the wall
and the LEDs show the same content at the same wall-clock moment, for any
content, with zero prediction.

`SyncManager` owns:
  - `output_delay_ms` (0..MAX_DELAY_MS, default 0), live-settable.
  - a ring buffer of recent uint8 (n,3) frames, sized for the worst case
    (MAX_DELAY_MS at the engine's fps) so raising the delay never needs a
    reallocation.

RingEngine._emit calls `push(frame)` once per rendered frame (the
CURRENT frame, before any calibration overlay substitution) and
`delayed_frame(frame)` to get what strips/satellites should actually
send. Taps always get the current frame, unmodified -- see ring.py.

Calibration bypass: while calibration mode is active, RingEngine skips
`push`/`delayed_frame` entirely and sends the calibration overlay frame
everywhere undelayed (markers must respond to slider drags instantly) --
see ring.py's `_emit`, not this module.

Persistence: {"output_delay_ms": int} under the "sync" top-level key in
state.json (see persist.py), independent of nodes/satellites/taps/
calibration.
"""
from __future__ import annotations

import math
from collections import deque

import numpy as np

MIN_DELAY_MS = 0
MAX_DELAY_MS = 500
DEFAULT_DELAY_MS = 0


def frames_for_delay(delay_ms: int, fps: int) -> int:
    """How many past frames must be retained to satisfy `delay_ms` at
    `fps`: ceil(delay_ms / frame_period_ms) + 1 (the +1 gives a little
    slack so the nearest-frame lookup always has a frame to land on)."""
    frame_period_ms = 1000.0 / fps
    return int(math.ceil(delay_ms / frame_period_ms)) + 1


class SyncManager:
    """Per-RingEngine delay buffer + live delay setting."""

    def __init__(self, fps: int):
        self.fps = fps
        self.output_delay_ms = DEFAULT_DELAY_MS
        self._maxlen = frames_for_delay(MAX_DELAY_MS, fps)
        self._buffer: deque[np.ndarray] = deque(maxlen=self._maxlen)

    # -- live control --------------------------------------------------
    def set_delay_ms(self, ms: int) -> None:
        """Raises ValueError if out of range. Applies live, within a
        frame or two -- no buffer resize needed (the deque is always
        sized for MAX_DELAY_MS)."""
        if not (MIN_DELAY_MS <= ms <= MAX_DELAY_MS):
            raise ValueError(
                f"output_delay_ms {ms} out of range {MIN_DELAY_MS}..{MAX_DELAY_MS}")
        self.output_delay_ms = int(ms)

    def status(self) -> dict:
        return {"output_delay_ms": self.output_delay_ms}

    # -- per-frame ------------------------------------------------------
    def push(self, frame: np.ndarray) -> None:
        """Record the just-rendered CURRENT frame. Call once per emitted
        frame (before any calibration overlay substitution)."""
        self._buffer.append(frame.copy())

    def delayed_frame(self, current: np.ndarray) -> np.ndarray:
        """The frame that should go out on strips/satellites right now:
        the buffered frame from `output_delay_ms` ago (nearest-frame,
        no interpolation), or `current` itself when delay is 0 or the
        buffer doesn't have enough history yet (startup ramp-up)."""
        if self.output_delay_ms <= 0 or not self._buffer:
            return current
        frame_period_ms = 1000.0 / self.fps
        frames_ago = int(round(self.output_delay_ms / frame_period_ms))
        frames_ago = min(frames_ago, len(self._buffer) - 1)
        idx = len(self._buffer) - 1 - frames_ago
        return self._buffer[idx]

    # -- persistence ------------------------------------------------------
    def for_persist(self) -> dict:
        return {"output_delay_ms": self.output_delay_ms}

    def restore(self, saved: dict) -> None:
        """saved: {"output_delay_ms": int}. Missing/corrupt/out-of-range
        -> leaves the default (0) -- never blocks startup."""
        if not isinstance(saved, dict):
            return
        ms = saved.get("output_delay_ms")
        try:
            ms = int(ms)
        except (TypeError, ValueError):
            return
        if MIN_DELAY_MS <= ms <= MAX_DELAY_MS:
            self.output_delay_ms = ms
