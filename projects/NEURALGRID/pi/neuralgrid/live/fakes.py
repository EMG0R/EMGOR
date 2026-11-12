"""Fakes for headless testing of LiveEngine (no serial, no real MIDI).

Duck-type the same interfaces LiveEngine expects from GridLink, Clock, and
MidiOut, plus test-only injection helpers (inject_key/inject_analog/
fire_step/fire_downbeat) so a selftest can drive the engine deterministically.
"""

from __future__ import annotations

from typing import List, Optional, Tuple

from .. import codec


class FakeLink:
    def __init__(self, hello: Optional[codec.Hello] = None):
        self.on_hello = None
        self.on_key = None
        self.on_analog = None
        self.on_frame_error = None
        self.started = False
        self.sent_frames: List[tuple] = []
        self._hello = hello or codec.Hello(proto_ver=1, grid_w=8, grid_h=8, n_analog=4, fw_ver=1)

    def start(self) -> None:
        self.started = True

    def stop(self) -> None:
        self.started = False

    def send_leds_full(self, rgb_list) -> None:
        self.sent_frames.append(("full", list(rgb_list)))

    def send_leds_partial(self, updates) -> None:
        self.sent_frames.append(("partial", list(updates)))

    def send_brightness(self, level: int) -> None:
        self.sent_frames.append(("brightness", level))

    def send_clear(self) -> None:
        self.sent_frames.append(("clear",))

    def send_ping(self) -> None:
        self.sent_frames.append(("ping",))
        if self.on_hello:
            self.on_hello(self._hello)

    # -- test-only injection helpers ---------------------------------------

    def inject_key(self, x: int, y: int, pressed: bool) -> None:
        if self.on_key:
            self.on_key(codec.Key(x, y, 1 if pressed else 0))

    def inject_analog(self, values) -> None:
        if self.on_analog:
            self.on_analog(codec.Analog(tuple(values)))


class FakeClock:
    def __init__(self):
        self.on_step = None
        self.on_downbeat = None
        self.step = 0
        self.started = False
        self.bpm = 120.0
        self.using_fallback = True

    def start(self) -> None:
        self.started = True

    def stop(self) -> None:
        self.started = False

    def fire_step(self, step: int) -> None:
        self.step = step
        if self.on_step:
            self.on_step(step)

    def fire_downbeat(self) -> None:
        if self.on_downbeat:
            self.on_downbeat()


class StubMidiOut:
    def __init__(self):
        self.events: List[tuple] = []
        self.active: set = set()

    @property
    def available(self) -> bool:
        return True

    def note_on(self, note: int, vel: int = 100, ch: int = 0) -> None:
        self.events.append(("on", note, vel, ch))
        self.active.add((note, ch))

    def note_off(self, note: int, ch: int = 0) -> None:
        self.events.append(("off", note, ch))
        self.active.discard((note, ch))

    def all_notes_off(self) -> None:
        self.events.append(("all_off",))
        self.active.clear()

    def close(self) -> None:
        pass
