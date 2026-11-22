"""MIDI clock IN for the neuralGrid live layer, over python-rtmidi.

Listens for MIDI beat clock (0xF8, "Timing Clock", 24 pulses per quarter
note -- PPQN) from an external source, by default the Pi's own
"demiurge_clock" ALSA sequencer client, falling back to "Midi Through" if
that's not present. From the pulse stream it derives:

  - a "step" callback every `pulses_per_step` pulses (default 6 -> 16th
    notes at 24 PPQN), which is what menu_sequencer.py advances its
    playhead on.
  - a "downbeat" callback every `pulses_per_downbeat` pulses (default 24,
    i.e. once per quarter note at 24 PPQN -- the literal pulse count the
    build spec asked for).
  - a running `bpm` estimate, read straight from channel 16 (index 15)
    CC119. The neuralGrid clock's CC119 encoding isn't otherwise
    documented, so this module takes the simplest reading: the raw 0..127
    CC value *is* the bpm. That's a resolved ambiguity -- see README.

If no 0xF8 pulses have arrived within `fallback_timeout` seconds (default
1.0s) -- including at startup, before anything external has ever been
seen -- a background thread free-runs an internal clock at `fallback_bpm`
(default 120) so steps/downbeats keep firing and the grid stays alive with
no external clock attached at all. As soon as a real external pulse shows
up again, the fallback thread stands down.
"""

from __future__ import annotations

import logging
import threading
import time
from typing import Callable, Optional, Sequence

log = logging.getLogger("neuralgrid.clock")

MSG_TIMING_CLOCK = 0xF8
MSG_START = 0xFA
MSG_CONTINUE = 0xFB
MSG_STOP = 0xFC

CC_BPM = 119
CC_CHANNEL = 15  # "ch16" 1-indexed == index 15


class Clock:
    def __init__(
        self,
        port_name_substrings: Sequence[str] = ("demiurge_clock", "Midi Through"),
        pulses_per_step: int = 6,
        pulses_per_downbeat: int = 24,
        steps_per_cycle: int = 8,
        fallback_bpm: float = 120.0,
        fallback_timeout: float = 1.0,
        on_step: Optional[Callable[[int], None]] = None,
        on_downbeat: Optional[Callable[[], None]] = None,
    ):
        self.port_name_substrings = tuple(port_name_substrings)
        self.pulses_per_step = pulses_per_step
        self.pulses_per_downbeat = pulses_per_downbeat
        self.steps_per_cycle = steps_per_cycle
        self.fallback_bpm = fallback_bpm
        self.fallback_timeout = fallback_timeout
        self.on_step = on_step
        self.on_downbeat = on_downbeat

        self.pulse_count = 0
        self.step = 0
        self.bpm = fallback_bpm
        self.using_fallback = True

        self._lock = threading.Lock()
        self._last_external_pulse_time = 0.0
        self._seen_external = False
        self._midiin = None
        self._stop_event = threading.Event()
        self._fallback_thread: Optional[threading.Thread] = None

        self._connect()

    # -- setup ---------------------------------------------------------

    def _connect(self) -> None:
        try:
            import rtmidi  # type: ignore
        except ImportError:
            log.warning(
                "python-rtmidi not installed -- clock IN disabled, "
                "internal fallback tempo will drive steps/downbeats"
            )
            return

        try:
            midiin = rtmidi.MidiIn()
            ports = midiin.get_ports()
            idx = None
            for substr in self.port_name_substrings:
                for i, name in enumerate(ports):
                    if substr.lower() in name.lower():
                        idx = i
                        break
                if idx is not None:
                    break
            if idx is None:
                log.warning(
                    "no MIDI clock source matching %s found among ports %s "
                    "-- internal fallback tempo will drive steps/downbeats",
                    self.port_name_substrings,
                    ports,
                )
                return

            # Don't ignore realtime messages (0xF8 timing clock) -- rtmidi
            # ignores sysex/timing/sensing by default.
            midiin.ignore_types(sysex=True, timing=False, active_sense=True)
            midiin.open_port(idx)
            midiin.set_callback(self._on_midi_in)
            self._midiin = midiin
            log.info("clock IN connected to %r", ports[idx])
        except Exception as exc:  # noqa: BLE001
            log.warning("clock IN init failed (%s) -- using internal fallback tempo", exc)
            self._midiin = None

    # -- lifecycle -------------------------------------------------------

    def start(self) -> None:
        self._stop_event.clear()
        self._fallback_thread = threading.Thread(
            target=self._fallback_loop, name="Clock-fallback", daemon=True
        )
        self._fallback_thread.start()

    def stop(self) -> None:
        self._stop_event.set()
        if self._fallback_thread is not None:
            self._fallback_thread.join(timeout=1.0)
            self._fallback_thread = None
        if self._midiin is not None:
            try:
                self._midiin.close_port()
            except Exception:  # noqa: BLE001
                pass

    # -- MIDI IN callback (runs on rtmidi's own thread) -------------------

    def _on_midi_in(self, event, data=None) -> None:
        message, _deltatime = event
        if not message:
            return
        status = message[0]

        if status == MSG_TIMING_CLOCK:
            with self._lock:
                self._last_external_pulse_time = time.monotonic()
                self._seen_external = True
                self.using_fallback = False
            self._pulse()
        elif status in (MSG_START, MSG_CONTINUE):
            with self._lock:
                self.pulse_count = 0
                self.step = 0
        elif (status & 0xF0) == 0xB0 and len(message) >= 3:
            channel = status & 0x0F
            cc, value = message[1], message[2]
            if channel == CC_CHANNEL and cc == CC_BPM:
                with self._lock:
                    self.bpm = float(value)

    # -- pulse handling (shared by real MIDI IN and fallback thread) -----

    def _pulse(self) -> None:
        with self._lock:
            self.pulse_count += 1
            fire_step = (self.pulse_count % self.pulses_per_step) == 0
            fire_downbeat = (self.pulse_count % self.pulses_per_downbeat) == 0
            if fire_step:
                self.step = (self.step + 1) % self.steps_per_cycle
            step = self.step
        if fire_step and self.on_step:
            self.on_step(step)
        if fire_downbeat and self.on_downbeat:
            self.on_downbeat()

    # -- fallback internal tempo ------------------------------------------

    def _fallback_loop(self) -> None:
        while not self._stop_event.is_set():
            with self._lock:
                elapsed = time.monotonic() - self._last_external_pulse_time
                external_alive = self._seen_external and elapsed < self.fallback_timeout

            if external_alive:
                with self._lock:
                    self.using_fallback = False
                # Real clock is driving us; just poll for it going stale.
                self._stop_event.wait(0.05)
                continue

            with self._lock:
                self.using_fallback = True
                bpm = self.fallback_bpm
                self.bpm = bpm if not self._seen_external else self.bpm

            interval = 60.0 / max(bpm, 1.0) / 24.0
            self._pulse()
            self._stop_event.wait(interval)
