"""Plain step-sequencer engine for the neuralGrid Pi app.

Deliberately hardware/renderer agnostic: it knows nothing about catalog
points, MIDI notes, or LEDs. A mode (modes.py) wraps ``on_step`` to do
whatever that mode needs (Machine Lab strike, MIDI note-on, rate limiting
+ skip counting, etc).

Timing: a dedicated background thread (``start``/``stop``) calls
``tick(time.time())`` at ~1ms resolution while ``playing`` is True.
``tick`` is also callable directly with synthetic timestamps, which is how
the tests drive it deterministically without real sleeps.

Scheduling uses absolute accumulation (``_next_time += gap``, never
``_next_time = now + gap``) so drift doesn't accumulate across many steps.
"""
from __future__ import annotations

import json
import threading
import time
from typing import Callable, List, Optional

# 3 arm levels (1..3) map to these velocities; level 0 = step is off.
LEVEL_VELOCITY = {1: 0.4, 2: 0.7, 3: 1.0}

MIN_BPM = 40
MAX_BPM = 240
MIN_SWING = 0
MAX_SWING = 60
VALID_STEPS_PER_BEAT = (1, 2, 4)

OnStep = Callable[[int, int, float], None]
OnPlayhead = Callable[[int], None]


class Sequencer:
    def __init__(
        self,
        n_tracks: int = 4,
        n_steps: int = 8,
        tempo_bpm: float = 120,
        swing_pct: float = 0,
        steps_per_beat: int = 1,
        on_step: Optional[OnStep] = None,
        on_playhead: Optional[OnPlayhead] = None,
    ):
        self.n_tracks = n_tracks
        self.n_steps = n_steps
        self.pattern: List[List[int]] = [[0] * n_steps for _ in range(n_tracks)]
        self._tempo_bpm = MIN_BPM
        self.tempo_bpm = tempo_bpm  # through the clamping property
        self._swing_pct = 0
        self.swing_pct = swing_pct
        self._steps_per_beat = 1
        self.steps_per_beat = steps_per_beat
        self.on_step = on_step
        self.on_playhead = on_playhead
        self.skip_count = 0  # informational; modes bump this on rate-limit skip

        self.playing = False
        self.playhead = 0
        self._pos = 0
        self._next_time = 0.0
        self._now = 0.0

        self._lock = threading.RLock()
        self._thread: Optional[threading.Thread] = None
        self._stop_event = threading.Event()

    # -- clamped transport params ---------------------------------------

    @property
    def tempo_bpm(self) -> float:
        return self._tempo_bpm

    @tempo_bpm.setter
    def tempo_bpm(self, v: float) -> None:
        self._tempo_bpm = max(MIN_BPM, min(MAX_BPM, float(v)))

    @property
    def swing_pct(self) -> float:
        return self._swing_pct

    @swing_pct.setter
    def swing_pct(self, v: float) -> None:
        self._swing_pct = max(MIN_SWING, min(MAX_SWING, float(v)))

    @property
    def steps_per_beat(self) -> int:
        return self._steps_per_beat

    @steps_per_beat.setter
    def steps_per_beat(self, v: int) -> None:
        v = int(v)
        self._steps_per_beat = v if v in VALID_STEPS_PER_BEAT else 1

    # -- editing ----------------------------------------------------------

    def toggle_step(self, track: int, step: int) -> None:
        with self._lock:
            self.pattern[track][step] = 0 if self.pattern[track][step] else 1

    def cycle_step_velocity(self, track: int, step: int) -> None:
        with self._lock:
            self.pattern[track][step] = (self.pattern[track][step] + 1) % 4

    def resize(self, n_tracks: int, n_steps: int) -> None:
        with self._lock:
            new_pattern = [[0] * n_steps for _ in range(n_tracks)]
            for t in range(min(n_tracks, self.n_tracks)):
                for s in range(min(n_steps, self.n_steps)):
                    new_pattern[t][s] = self.pattern[t][s]
            self.pattern = new_pattern
            self.n_tracks = n_tracks
            self.n_steps = n_steps
            self._pos = 0
            self.playhead = 0

    # -- transport ----------------------------------------------------------

    def _step_gap(self, next_index: int) -> float:
        base = 60.0 / self._tempo_bpm / self._steps_per_beat
        if next_index % 2 == 1 and self._swing_pct:
            base *= 1.0 + (self._swing_pct / 100.0)
        return base

    def _fire_step(self, step: int) -> None:
        self.playhead = step
        if self.on_playhead:
            self.on_playhead(step)
        for track in range(self.n_tracks):
            level = self.pattern[track][step]
            if level:
                vel = LEVEL_VELOCITY.get(level, 1.0)
                if self.on_step:
                    self.on_step(track, step, vel)

    def play(self, now: Optional[float] = None) -> None:
        if now is None:
            now = time.time()
        with self._lock:
            self.playing = True
            self._pos = 0
            self._next_time = now
        self.tick(now)

    def stop_transport(self) -> None:
        with self._lock:
            self.playing = False

    def tick(self, now: float) -> None:
        self._now = now
        if not self.playing:
            return
        with self._lock:
            guard = 0
            while self.playing and now >= self._next_time:
                step = self._pos
                self._fire_step(step)
                nxt = (step + 1) % self.n_steps
                self._next_time += self._step_gap(nxt)
                self._pos = nxt
                guard += 1
                if guard > 10000:  # pathological input safety valve
                    break

    # -- background thread ----------------------------------------------

    def start(self) -> None:
        if self._thread is not None:
            return
        self._stop_event.clear()
        self._thread = threading.Thread(target=self._run, name="Sequencer", daemon=True)
        self._thread.start()

    def stop(self) -> None:
        self._stop_event.set()
        if self._thread is not None:
            self._thread.join(timeout=2.0)
            self._thread = None

    def _run(self) -> None:
        while not self._stop_event.is_set():
            self.tick(time.time())
            time.sleep(0.001)

    # -- persistence ----------------------------------------------------

    def to_json(self) -> dict:
        return {
            "n_tracks": self.n_tracks,
            "n_steps": self.n_steps,
            "pattern": [row[:] for row in self.pattern],
            "tempo_bpm": self.tempo_bpm,
            "swing_pct": self.swing_pct,
            "steps_per_beat": self.steps_per_beat,
        }

    def from_json(self, data: dict) -> None:
        with self._lock:
            n_tracks = int(data.get("n_tracks", self.n_tracks))
            n_steps = int(data.get("n_steps", self.n_steps))
            self.n_tracks = n_tracks
            self.n_steps = n_steps
            pattern = data.get("pattern")
            if pattern is not None:
                self.pattern = [list(row) for row in pattern]
            else:
                self.pattern = [[0] * n_steps for _ in range(n_tracks)]
            if "tempo_bpm" in data:
                self.tempo_bpm = data["tempo_bpm"]
            if "swing_pct" in data:
                self.swing_pct = data["swing_pct"]
            if "steps_per_beat" in data:
                self.steps_per_beat = data["steps_per_beat"]
            self._pos = 0
            self.playhead = 0

    def to_json_str(self) -> str:
        return json.dumps(self.to_json())

    def from_json_str(self, s: str) -> None:
        self.from_json(json.loads(s))
