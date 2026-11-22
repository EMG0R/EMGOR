from __future__ import annotations
import random
import re
import threading
import time
from typing import Callable

from engine.presence import Presence, PresenceState


class IdleLoop:
    """Background daemon that emits NPC sounds and sung verses while IDLE.

    Two independent jittered timers — one for short NPC vocalizations, one
    for short sung verses on cosmology topics. Both suppressed during ACTIVE.
    """

    def __init__(
        self,
        presence: Presence,
        speak_fn: Callable[[str], None],
        sing_fn: Callable[[list[str]], None],
        verse_generator: Callable[[str], str],
        sounds: list[str],
        topics: list[str],
        npc_cadence_seconds: float = 180.0,
        npc_jitter_seconds: float = 30.0,
        verse_cadence_seconds: float = 1200.0,
        verse_jitter_seconds: float = 300.0,
    ) -> None:
        self._presence = presence
        self._speak_fn = speak_fn
        self._sing_fn = sing_fn
        self._verse_generator = verse_generator
        self._sounds = sounds
        self._topics = topics
        self._npc_cadence = npc_cadence_seconds
        self._npc_jitter = npc_jitter_seconds
        self._verse_cadence = verse_cadence_seconds
        self._verse_jitter = verse_jitter_seconds
        now = time.monotonic()
        self._next_npc_at = now + self._npc_delay()
        self._next_verse_at = now + self._verse_delay()
        self._stop_event = threading.Event()
        self._thread: threading.Thread | None = None

    def _npc_delay(self) -> float:
        return self._npc_cadence + random.uniform(-self._npc_jitter, self._npc_jitter)

    def _verse_delay(self) -> float:
        return self._verse_cadence + random.uniform(-self._verse_jitter, self._verse_jitter)

    def _decide(self, now: float) -> tuple[str | None, str | None]:
        """Return (sound, topic) to fire this tick. Either may be None."""
        if self._presence.state == PresenceState.ACTIVE:
            # Reset timers so we don't burst the moment we return to IDLE
            self._next_npc_at = now + self._npc_delay()
            self._next_verse_at = now + self._verse_delay()
            return (None, None)
        sound = None
        topic = None
        if now >= self._next_npc_at:
            sound = random.choice(self._sounds)
            self._next_npc_at = now + self._npc_delay()
        if now >= self._next_verse_at:
            topic = random.choice(self._topics)
            self._next_verse_at = now + self._verse_delay()
        return (sound, topic)

    def _apply(self, sound: str | None, topic: str | None) -> None:
        if sound is not None:
            self._speak_fn(sound)
        if topic is not None:
            try:
                verse = self._verse_generator(topic)
            except Exception as exc:
                print(f"[idle] verse generation failed: {exc}")
                return
            if not verse:
                return
            cleaned = re.sub(r"[^a-zA-Z\s]", " ", verse)
            lyrics = cleaned.upper().split()
            if lyrics:
                self._sing_fn(lyrics)

    def start(self) -> None:
        if self._thread is not None:
            return
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def stop(self) -> None:
        self._stop_event.set()
        if self._thread is not None:
            self._thread.join(timeout=2.0)
            self._thread = None

    def _run(self) -> None:
        while not self._stop_event.is_set():
            time.sleep(1.0)
            self._presence.tick()
            sound, topic = self._decide(time.monotonic())
            self._apply(sound, topic)
