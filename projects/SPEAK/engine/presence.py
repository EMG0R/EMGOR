from __future__ import annotations
import threading
import time
from enum import Enum


class PresenceState(Enum):
    IDLE = "idle"
    ACTIVE = "active"


class Presence:
    """Tracks whether neptr is in active conversation or idle.

    Wake transitions IDLE -> ACTIVE. Active stays until `active_window_seconds`
    have elapsed since the last user utterance, then drops back to IDLE on the
    next `tick()`. Thread-safe.
    """

    def __init__(self, active_window_seconds: float = 900.0) -> None:
        self._state = PresenceState.IDLE
        self._last_activity_at = 0.0
        self._window = active_window_seconds
        self._lock = threading.Lock()

    @property
    def state(self) -> PresenceState:
        return self._state

    def wake(self) -> None:
        with self._lock:
            self._state = PresenceState.ACTIVE
            self._last_activity_at = time.monotonic()

    def mark_utterance(self) -> None:
        with self._lock:
            if self._state == PresenceState.ACTIVE:
                self._last_activity_at = time.monotonic()

    def tick(self) -> None:
        with self._lock:
            if self._state == PresenceState.ACTIVE:
                if time.monotonic() - self._last_activity_at > self._window:
                    self._state = PresenceState.IDLE
