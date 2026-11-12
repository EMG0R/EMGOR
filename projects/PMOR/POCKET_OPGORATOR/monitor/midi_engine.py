"""
MIDI clock + transport + CC monitoring for the Pocket OpGorator.

Protocol (from hardware spec):
  0xF8            MIDI clock, 24 PPQN
  0xFA            Start
  0xFC            Stop
  ch16 CC116      clock-role announce: 1 = OpGorator is master, 2 = other side is master
  ch16 CC119      BPM broadcast: bpm = 40 + value/127*360

Known hardware quirk (measured, not hypothetical): the device occasionally
transmits a DUPLICATE 0xF8 byte (~3% of ticks), arriving with a near-zero
gap after the real tick. A naive ticks/elapsed-time BPM readout reads ~3%
fast because of this. We filter it out explicitly:
  - any inter-tick gap < 2ms is treated as a duplicate/glitch, not a real
    tick (no legitimate tick gap can be under 6.25ms even at 400 BPM,
    24 PPQN => 60/(400*24)*1000 = 6.25ms), and is counted separately
  - displayed BPM is computed from the MEDIAN of the last N real
    inter-tick intervals, which is robust to occasional outliers in either
    direction (duplicates having already been removed, plus USB/serial
    jitter on individual ticks)
"""
import threading
import time
from collections import deque

import rtmidi

from device_match import matches as _device_name_matches

POLL_INTERVAL_S = 1.0

TICKS_PER_BEAT = 24
TICKS_PER_BAR = 96  # 4/4, 16th-note grid

MIN_REAL_TICK_GAP_S = 0.002  # 2ms; duplicates arrive near-zero gap
INTERVAL_HISTORY = 48        # 2 bars worth, for median filtering
RAW_INTERVAL_HISTORY = 48    # naive BPM window, includes duplicates on purpose
DUP_RATE_WINDOW_S = 5.0      # trailing window for duplicates/sec display


class MidiEngine:
    def __init__(self, state, port_name_override=None):
        self.state = state
        self.port_name_override = port_name_override

        self._midi_in = None
        self._midi_out = None
        self._connected_port = None
        self._connected_out_port = None
        self._out_lock = threading.Lock()
        self._stop = threading.Event()
        self._poll_thread = None

        self._intervals = deque(maxlen=INTERVAL_HISTORY)
        self._last_tick_time = None

        # Naive-BPM + duplicate-rate bookkeeping (see _handle_tick). These
        # are intentionally NOT filtered -- they exist specifically to show
        # the gap between "naive" and "median-filtered" readings.
        self._raw_intervals = deque(maxlen=RAW_INTERVAL_HISTORY)
        self._last_raw_tick_time = None
        self._dup_times = deque()

    def start(self):
        self._poll_thread = threading.Thread(target=self._poll_loop, daemon=True)
        self._poll_thread.start()

    def stop(self):
        self._stop.set()
        if self._poll_thread:
            self._poll_thread.join(timeout=2.0)
        self._close()
        self._close_out()

    def _poll_loop(self):
        while not self._stop.is_set():
            try:
                if self._midi_in is None:
                    self._try_connect()
                else:
                    self._check_still_present()
            except Exception as e:
                self.state.midi.last_error = f"poll error: {e}"
                self._close()
            self._stop.wait(POLL_INTERVAL_S)

    def _name_matches(self, name):
        if self.port_name_override:
            return self.port_name_override in name
        return _device_name_matches(name)

    def _find_port(self):
        probe = rtmidi.MidiIn()
        try:
            ports = probe.get_ports()
        finally:
            probe.close_port()
            del probe
        for i, name in enumerate(ports):
            if self._name_matches(name):
                return i, name
        return None, None

    def _find_out_port(self):
        probe = rtmidi.MidiOut()
        try:
            ports = probe.get_ports()
        finally:
            probe.close_port()
            del probe
        for i, name in enumerate(ports):
            if self._name_matches(name):
                return i, name
        return None, None

    def _try_connect(self):
        idx, name = self._find_port()
        if idx is None:
            self.state.midi.connected = False
            self._close_out()
            return
        midi_in = rtmidi.MidiIn()
        midi_in.ignore_types(sysex=True, timing=False, active_sense=True)
        midi_in.open_port(idx)
        midi_in.set_callback(self._on_message)
        self._midi_in = midi_in
        self._connected_port = name
        self.state.midi.connected = True
        self.state.midi.port_name = name
        self.state.midi.last_error = ""

        # Best-effort: also open the matching MIDI OUTPUT port so the UI can
        # send role-control (CC116) messages back to the device. Audio/clock
        # visualization must keep working even if this fails (e.g. no MIDI
        # output port exists, or it's already open elsewhere), so any
        # failure here is swallowed and just leaves role control disabled.
        try:
            out_idx, out_name = self._find_out_port()
            if out_idx is not None:
                midi_out = rtmidi.MidiOut()
                midi_out.open_port(out_idx)
                with self._out_lock:
                    self._close_out_locked()
                    self._midi_out = midi_out
                    self._connected_out_port = out_name
                self.state.midi.out_port_name = out_name
                self.state.midi.out_connected = True
        except Exception as e:
            self.state.midi.last_error = f"MIDI out open failed: {e}"

    def _check_still_present(self):
        idx, name = self._find_port()
        if idx is None:
            self.state.midi.last_error = "MIDI port disappeared"
            self._close()
            self.state.midi.connected = False
        out_idx, _ = self._find_out_port()
        if out_idx is None and self._midi_out is not None:
            self._close_out()
            self.state.midi.out_connected = False

    def _close_out_locked(self):
        if self._midi_out is not None:
            try:
                self._midi_out.close_port()
            except Exception:
                pass
        self._midi_out = None
        self._connected_out_port = None

    def _close_out(self):
        with self._out_lock:
            self._close_out_locked()

    def _close(self):
        if self._midi_in is not None:
            try:
                self._midi_in.close_port()
            except Exception:
                pass
        self._midi_in = None
        self._connected_port = None
        self._close_out()

    # ---------- role control: send CC116 to the device (any thread) ----------

    def send_role(self, role):
        """Send ch16 CC116 = role (1 = PMOR master, 2 = host master) to the
        device's MIDI input port, if we currently have an open output port.
        Returns True if a message was actually sent. Safe to call from the
        UI thread; never raises -- a failed send just leaves the on-screen
        role display unrepresentative until the next successful send or the
        device's own next CC116 broadcast corrects it."""
        with self._out_lock:
            out = self._midi_out
            if out is None:
                return False
            try:
                # status 0xBF = Control Change, channel 16 (0-indexed 15)
                out.send_message([0xBF, 116, int(role) & 0x7F])
                return True
            except Exception as e:
                self.state.midi.last_error = f"role send failed: {e}"
                return False

    # ---------- rtmidi callback: runs on rtmidi's own thread ----------

    def _on_message(self, event, data=None):
        message, _deltatime = event
        if not message:
            return
        status = message[0]
        now = time.time()
        st = self.state.midi
        st.last_message_at = now

        if status == 0xF8:
            self._handle_tick(now)
        elif status == 0xFA:  # Start
            st.running = True
            st.last_start_at = now
            st.tick_count = 0
            st.bar_tick = 0
            st.beat_tick = 0
        elif status == 0xFC:  # Stop
            st.running = False
            st.last_stop_at = now
        elif (status & 0xF0) == 0xB0:
            channel = status & 0x0F
            if channel == 15 and len(message) >= 3:
                cc, value = message[1], message[2]
                if cc == 116:
                    st.role = value
                elif cc == 119:
                    st.bpm_reported = 40.0 + (value / 127.0) * 360.0
                    self._update_disagreement()

    def _handle_tick(self, now):
        st = self.state.midi

        # Naive BPM + duplicate-rate bookkeeping runs on EVERY 0xF8, real or
        # duplicate -- that's the whole point: the naive reading is what a
        # dumb "ticks / elapsed time" implementation would show, duplicates
        # included, so its ~3% fast skew relative to bpm_measured is visible
        # side by side on screen rather than silently filtered away.
        if self._last_raw_tick_time is not None:
            raw_gap = now - self._last_raw_tick_time
            if raw_gap > 0:
                self._raw_intervals.append(raw_gap)
                if len(self._raw_intervals) >= 4:
                    avg = sum(self._raw_intervals) / len(self._raw_intervals)
                    if avg > 0:
                        st.bpm_naive = 60.0 / (avg * TICKS_PER_BEAT)
        self._last_raw_tick_time = now

        if self._last_tick_time is not None:
            gap = now - self._last_tick_time
            if gap < MIN_REAL_TICK_GAP_S:
                # duplicate 0xF8 -- do not advance tick count or feed the
                # interval filter, just count it
                st.duplicate_count += 1
                self._dup_times.append(now)
                self._prune_dup_window(now)
                st.duplicates_per_sec = len(self._dup_times) / DUP_RATE_WINDOW_S
                return
            self._intervals.append(gap)

        self._prune_dup_window(now)
        st.duplicates_per_sec = len(self._dup_times) / DUP_RATE_WINDOW_S

        self._last_tick_time = now
        st.tick_count += 1
        st.last_tick_at = now
        st.bar_tick = st.tick_count % TICKS_PER_BAR
        st.beat_tick = st.tick_count % TICKS_PER_BEAT

        if len(self._intervals) >= 4:
            ivals = sorted(self._intervals)
            mid = len(ivals) // 2
            if len(ivals) % 2:
                median_interval = ivals[mid]
            else:
                median_interval = (ivals[mid - 1] + ivals[mid]) / 2.0
            # seconds per tick -> BPM (24 ticks per quarter note)
            if median_interval > 0:
                st.bpm_measured = 60.0 / (median_interval * TICKS_PER_BEAT)
            self._update_disagreement()

    def _prune_dup_window(self, now):
        cutoff = now - DUP_RATE_WINDOW_S
        dq = self._dup_times
        while dq and dq[0] < cutoff:
            dq.popleft()

    def _update_disagreement(self):
        st = self.state.midi
        if st.bpm_measured > 0 and st.bpm_reported > 0:
            st.bpm_disagree = abs(st.bpm_measured - st.bpm_reported) > 2.0
