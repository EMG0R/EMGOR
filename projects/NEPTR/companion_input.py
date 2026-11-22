"""Teensy MIDI bus -> companion input state.

The companion subscribes to the same ALSA-seq MIDI the serial bridge feeds to
Csound (it does NOT open the serial port — Csound owns that). Csound keeps
reacting to every control; the companion only reads the bus, so audio control
is never disturbed.

InputState holds the latest value of every CC and exposes:
  * the four navigation controls (joystick button/X/Y, arcade button) as a
    snapshot() dict matching CompanionController.update()'s kwargs, and
  * named sensor telemetry for the dashboard via sensors().

read_loop() is the thin mido reader thread; it has no logic beyond feeding CCs
into an InputState and notifying the UI when a control changes.

EngineOut (2026-08-09) is the write side: the touch UI speaks to the Csound
engine over the SAME shared ALSA-seq pool (Midi Through) the Teensy feeds,
emitting the very CCs the hardware emits — physical encoder-button presses
(127) / releases (0) on ch1 for slot toggles and looper transport, encoder
rotation pulses (CW=127/CCW=0) for parameter edits, plus one utility message
of its own: ch16 CC102 = "UI sync request" (the engine answers by
rebroadcasting its full OSC state so a late-joining UI syncs — see
DEMIURGE_OS demiurge/docs/engine-ui.md).
"""

import os
import threading
import time

# Control CCs (TEENSYY/TEENSYY.ino MIDI mirror).
CC_JOY_BTN = 77
CC_JOY_X = 27
CC_JOY_Y = 28
CC_ARCADE = 60          # arcade button b1 (BTN_CC[0])
BUTTON_ON = 64          # >= this is "pressed" for 0/127 button CCs

# Sensor CCs shown on the dashboard (legacy keys, kept for back-compat).
_SENSOR_CC = {
    30: "tof1", 36: "tof2",
    31: "accel_x", 32: "accel_y", 33: "accel_z",
    34: "fsr1", 35: "fsr2",
    29: "exp",
    78: "mic_l", 79: "mic_r",
}

# Ordered telemetry for the dense dashboard readout: (short label, CC). Short
# labels render as `jstX:70` style cells. Covers everything on the MIDI bus.
_TELEMETRY = (
    ("jstX", 27), ("jstY", 28), ("joyB", 77), ("exp", 29),
    ("ldr1", 30), ("ldr2", 36),
    ("accX", 31), ("accY", 32), ("accZ", 33),
    ("fsr1", 34), ("fsr2", 35), ("micL", 78), ("micR", 79),
    ("btn1", 60), ("btn2", 61), ("btn3", 62), ("btn4", 63), ("btn5", 64),
    ("btn6", 65), ("btn7", 66), ("btn8", 67), ("btn9", 68),
    ("enc1", 20), ("enc2", 21), ("enc3", 22), ("enc4", 23),
    ("enc5", 24), ("enc6", 25), ("enc7", 26),
    ("eb1", 70), ("eb2", 71), ("eb3", 72), ("eb4", 73),
    ("eb5", 74), ("eb6", 75), ("eb7", 76),
)

# Which CCs change a navigation control (so the reader knows when to wake the UI).
CONTROL_CCS = frozenset({CC_JOY_BTN, CC_JOY_X, CC_JOY_Y, CC_ARCADE})

# UI -> engine sync request: ch16 (the demiurge utility channel — touch CC80-95,
# clock CC116-119 live there) CC102, press-style 127. The Csound engine answers
# by pulsing gkOscRefresh, which rebroadcasts the FULL OSC state/param snapshot
# to the UI's listener on 127.0.0.1:8000. Collision-checked 2026-08-09: CC96-115
# are unused across DEMIURGE_OS + this repo. Documented in
# DEMIURGE_OS demiurge/docs/engine-ui.md (+ the clock.md channel-16 table).
CC_UI_SYNC = 102
UTILITY_CHANNEL = 15          # 0-based mido channel == MIDI ch16


class EngineOut:
    """Write side of the engine link: emits the exact CCs the Teensy emits,
    onto the shared pool Csound reads (-M on Midi Through). Lazy-opens the
    port on first send; every failure is non-fatal (the UI keeps running,
    touch control of the engine is then simply unreachable — mirroring how
    read_loop degrades). Thread-safe (sends can come from the UI loop and
    the param-chase timer)."""

    # A failed open is TEMPORARY, never terminal: on a cold boot the UI can
    # easily be up before the ALSA Through pool exists (and the pool also
    # disappears/reappears around a snd-seq reload). The old code latched
    # _failed on the first miss and disabled touch->engine control — plus the
    # CC102 sync request — for the whole session. Now it just backs off.
    RETRY_SECS = 2.0
    RETRY_MAX = 30.0

    def __init__(self, port_name=None, clock=None):
        self._want = port_name or os.environ.get("NEPTR_MIDI_OUT")
        self._port = None
        self._clock = clock or time.monotonic
        self._retry_at = 0.0        # next allowed open attempt
        self._retry_dt = self.RETRY_SECS
        self._warned = False
        self._lock = threading.Lock()

    def _resolve(self, names):
        if self._want:
            for n in names:
                if self._want in n:
                    return n
            return self._want                       # try verbatim
        # The Through pool is the ONLY port Csound is guaranteed to read
        # (-M 14:0); never write blindly to e.g. the Teensy's own port.
        for n in names:
            if "through" in n.lower():
                return n
        return None

    def _open(self):
        """Open the port (or raise). Split out so the retry policy above can
        be tested without a MIDI backend."""
        import mido
        name = self._resolve(mido.get_output_names())
        if name is None:
            raise RuntimeError("no Through-pool MIDI output found")
        return mido.open_output(name)

    def _out(self):
        if self._port is not None:
            return self._port
        now = self._clock()
        if now < self._retry_at:
            return None                             # backing off; try later
        try:
            self._port = self._open()
            self._retry_dt = self.RETRY_SECS
            self._warned = False
        except Exception as exc:                    # mido/backend/port missing
            self._retry_at = now + self._retry_dt
            self._retry_dt = min(self._retry_dt * 2, self.RETRY_MAX)
            if not self._warned:
                self._warned = True
                print(f"engine-out: MIDI output unavailable ({exc}); "
                      "retrying in the background")
            return None
        return self._port

    def cc(self, control, value, channel=0):
        """Send one control_change. Returns True if it went out."""
        with self._lock:
            port = self._out()
            if port is None:
                return False
            try:
                import mido
                port.send(mido.Message("control_change", channel=channel,
                                       control=control, value=value))
                return True
            except Exception:
                # Port went away mid-session (pool reload, seq restart): drop
                # it and let _out() re-open on the backoff schedule.
                try:
                    self._port.close()
                except Exception:
                    pass
                self._port = None
                self._retry_at = self._clock() + self.RETRY_SECS
                return False

    # Button-style press/release (the Teensy sends 127 then 0).
    def press(self, control):
        return self.cc(control, 127)

    def release(self, control):
        return self.cc(control, 0)

    def tap(self, control):
        ok = self.press(control)
        self.release(control)
        return ok

    # Encoder-style relative rotation pulse: CW=127, CCW=0.
    def rotate(self, control, up):
        return self.cc(control, 127 if up else 0)

    def sync_request(self):
        """Ask the engine to rebroadcast its full OSC state (late-join sync)."""
        ok = self.cc(CC_UI_SYNC, 127, channel=UTILITY_CHANNEL)
        self.cc(CC_UI_SYNC, 0, channel=UTILITY_CHANNEL)
        return ok

    def close(self):
        with self._lock:
            if self._port is not None:
                try:
                    self._port.close()
                except Exception:
                    pass
                self._port = None


class InputState:
    def __init__(self):
        self._cc = {CC_JOY_X: 64, CC_JOY_Y: 64}

    def feed(self, cc, value):
        self._cc[cc] = value

    @property
    def joy_btn(self):
        return self._cc.get(CC_JOY_BTN, 0) >= BUTTON_ON

    @property
    def arcade(self):
        return self._cc.get(CC_ARCADE, 0) >= BUTTON_ON

    @property
    def joy_x(self):
        return self._cc.get(CC_JOY_X, 64)

    @property
    def joy_y(self):
        return self._cc.get(CC_JOY_Y, 64)

    def snapshot(self):
        return dict(joy_btn=self.joy_btn, joy_x=self.joy_x,
                    joy_y=self.joy_y, arcade=self.arcade)

    def sensors(self):
        return {name: self._cc.get(cc) for cc, name in _SENSOR_CC.items()}

    def telemetry(self):
        """Ordered [(label, value|None)] for the dashboard. Value is None until
        the channel's first CC arrives."""
        return [(label, self._cc.get(cc)) for label, cc in _TELEMETRY]


def read_loop(state, port_name, on_control, stop):
    """Blocking mido reader. Feeds every CC into `state`; calls on_control()
    when a navigation control changes so the UI can re-evaluate. `stop` is a
    threading.Event. Imports mido lazily so the UI still starts if MIDI is
    unavailable (the companion is then simply unreachable, normal UI works).

    NOTE: telemetry publishing for the terminal companion is NO LONGER done
    here — it lives in the UI-independent `demiurge-monitor` system service,
    which taps the same MIDI bus and publishes regardless of whether any UI is
    running. The python UI is purely a consumer of MIDI now."""
    import mido  # lazy: keeps the hard dependency off the import path

    with mido.open_input(port_name) as inport:
        while not stop.is_set():
            for msg in inport.iter_pending():
                if msg.type == "control_change":
                    state.feed(msg.control, msg.value)
                    if msg.control in CONTROL_CCS:
                        on_control()
            stop.wait(0.005)
