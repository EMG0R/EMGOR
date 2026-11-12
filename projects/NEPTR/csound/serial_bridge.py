#!/usr/bin/env python3
"""
serial_bridge.py — NEPTR Phase 4 Teensy serial → MIDI translator.

Reads "NEPTR2 ..." frames from a Teensy serial port at 100 Hz and emits
the equivalent MIDI CC messages the original MIDI firmware produced, on
an ALSA-seq virtual MIDI port that csound (and anything else listening
on Midi Through) can subscribe to.

Csound's existing midi_handler.orc dispatcher needs no changes — the
serial format is parsed here and the same CC traffic appears on the bus.

The serial → CC mapping mirrors the old firmware exactly. See
../TEENSYY/midi_reference/SENSOR_MAP.md for the table.

Usage:
    python3 serial_bridge.py [/dev/ttyACM0]

Designed to run as a long-lived process under systemd (see SERIAL_BRIDGE.md).

Serial I/O note
---------------
The Teensy enumerates as USB type "Serial + MIDI", so /dev/ttyACM0 is a CDC
ACM port. pyserial's POSIX backend misreads the Teensy's intermittent
zero-length CDC URBs as a fatal "device disconnected" and raises on every
read — delivering zero frames even though `cat /dev/ttyACM0` streams fine.
We therefore open the port the way `cat` does: a raw file descriptor put
into raw 115200 8N1 mode via termios, read with select + os.read, tolerating
empty/short reads. No pyserial dependency. See SERIAL_BRIDGE.md.
"""

from __future__ import annotations
import errno
import os
import select
import socket
import struct
import subprocess
import sys
import termios
import time

import mido

PORT_DEFAULT    = "/dev/ttyACM0"
BAUD            = 115200          # informational; set via termios B115200 below
MIDI_PORT_NAME  = "NEPTR Serial Bridge"
CHANNEL         = 0   # MIDI channel 1, 0-indexed

# ── Field → CC mapping (body remap, measured 2026-10-22) ────────────────────
# The tables are indexed by the Teensy's serial channel (e1..e7 / b1..b9 /
# eb1..eb7) and hold the CC the ENGINE's remap layer (handler/handler_remap.orc)
# expects for that physical POSITION. They are the same four tables as
# TEENSYY.ino (ENC_CC / ENC_FLIP / BTN_CC / ENCBTN_CC), so this process and the
# Teensy's own USB-MIDI emit identical numbering. Full table: docs/body_control_map.md.
#   TL = E7 (menu PREV)   TR = E5 (menu NEXT)
#   bottom left->right = E6, E2, E3, E4, E1  (= slot4..slot0)
# Serial deltas are RAW (unflipped); ENC_FLIP corrects the three encoders wired
# backwards (e1, e3, e5) so CW = 127 for every encoder.
ENC_CC      = (21, 26, 22, 24, 25, 23, 20)
ENC_FLIP    = (True, False, True, False, True, False, False)
# b6/b7 swapped 2026-10-27 (EMGOR press test: the UP button arrives on b7, so
# b7 -> 61 UP; RIGHT gave no edge at all on any button -> wiring/firmware, b6 -> 64).
BTN_CC      = (60, 65, 68, 67, 66, 64, 61, 63, 62)
ENCBTN_CC   = (71, 76, 72, 74, 75, 73, 70)
ENCODER_CCS = {f"e{i+1}":  ENC_CC[i]    for i in range(7)}
BUTTON_CCS  = {f"b{i+1}":  BTN_CC[i]    for i in range(9)}
ENC_BTN_CCS = {f"eb{i+1}": ENCBTN_CC[i] for i in range(7)}
ENCODER_FLIP = {f"e{i+1}": ENC_FLIP[i]  for i in range(7)}
# TODO(RIGHT): capture 2026-10-22 recorded UP and RIGHT as the same CC (usb 64).
# Firmware says UP=b6(61) RIGHT=b7(64); the UP step was most likely a mis-press.
# Re-capture UP and RIGHT before trusting either.
JOY_BTN_CC  = 77

# ── Tap tempo (2026-10-05, bathtub realizations §2) ─────────────────────────
# The arcade button (b1) is also a tempo TAP. Every press edge emits a timestamped
# tap event on the shared bus: ch16 CC113, value = AGE in ms (how long ago the press
# actually happened). The Pi clock daemon (demiurge-clock) reconstructs the true press
# time as its own receive time minus that age, and does the tap analysis ONCE (the same
# analyzer PMOR compiles for standalone use) -- NEPTR never guesses a tempo of its own.
# b1's existing CC60 press/release is unchanged; the tap is additional.
TAP_BUTTON      = "b1"
TAP_CC          = 113            # ch16 (utility channel) -- PMOR's own taps are CC112
TAP_CHANNEL     = 15             # MIDI channel 16, 0-indexed
# Since body fw ab8470f (2026-11-05) the Teensy sends a frame the moment a button edge
# happens (no 10 ms timer), so a press edge is ~USB-frame old (<0.5 ms) when it arrives.
# Was 5.0 under the old fixed 100 Hz frames (edge somewhere in the preceding 10 ms).
TAP_FRAME_AGE_MS = 0.5


def _clip127(v: int) -> int:
    return max(0, min(127, int(v)))


def _clipf(v: float) -> float:
    return max(0.0, min(127.0, float(v)))


# ── High-resolution analog feed (2026-11-05, EMGOR: >= 1024 steps) ─────────
# Every analog is ALSO computed as a float 0..127 on the SAME curve as its 7-bit
# scaler below and sent to the engine as OSC `/ccf <ch> <cc> <val>` ("iif") on
# 127.0.0.1:7770 (midi_handler.orc). The 7-bit MIDI CC is still emitted for other
# listeners, but ONLY on the frames where the fine value changed, derived from that
# same fine value (so the two never disagree), and the engine drops the 7-bit copy
# of a CC while its /ccf is live. DEADBAND is in raw ADC units: ADC noise inside it
# sends nothing. Native source resolution: 12-bit (4096) for jx/jy/exp/fsr, 16-bit
# for accel; ToF is 1 mm over 20..470 mm and mic is 0..800 counts, so those two
# cannot exceed ~450 / ~800 steps whatever the transport does.
CTL_OSC_ADDR = ("127.0.0.1", 7770)
OSC_CH       = 1                    # ch1 as the engine's kCh sees it (1-based)


def osc_ccf(ch: int, cc: int, val: float) -> bytes:
    """OSC message /ccf ,iif ch cc val (big-endian, 4-byte aligned)."""
    return b"/ccf\x00\x00\x00\x00,iif\x00\x00\x00\x00" + struct.pack(">iif", ch, cc, val)


# Analog scalers — each maps raw 12-bit Teensy reading (or special raw) to a
# 7-bit MIDI value matching exactly what the MIDI firmware emitted.
#
# A scaler may return None to mean "skip this CC for this frame" (e.g. LiDAR
# timeout, out-of-range distance).

def joy_scale(raw: int) -> int:
    """Joystick 12-bit 0..4095 → 0..127 linear."""
    return _clip127(raw * 127 // 4095)


def exp_scale(raw: int) -> int:
    """Expression pedal — calibrated 12-bit range 128..3992 → 0..127."""
    span = 3992 - 128
    return _clip127((raw - 128) * 127 // span) if raw >= 128 else 0


class PedalPresence:
    """Is a real pedal in the body's expression jack, or is the input floating?

    Measured 2026-10-23 with nothing plugged in: CC29 random-walks 63..86 at ~75
    msgs/s, which overwrote the touch UI's expression slider (also ch1 CC29) many
    times a second. A real pedal at rest holds still (spread <= 2) and a moving
    pedal travels mostly one way. So over a 0.5 s window: spread >= FLOAT_SPREAD
    with >= FLOAT_FLIPS direction reversals = floating -> absent (stay silent);
    spread <= STILL_SPREAD held for STILL_S = present. Starts absent so an empty
    jack never fights the slider at boot.
    """
    WINDOW_S, FLOAT_SPREAD, FLOAT_FLIPS = 0.5, 6, 6
    STILL_SPREAD, STILL_S = 2, 0.3
    # 2026-10-24: a REAL pedal being rocked (or swept slowly with +-1 sensor
    # jitter) matched the float signature within 0.5 s, so the bridge went silent
    # until the pedal held still -> the expression "cut out for a second" while
    # the body's own LEDs kept tracking it. Now: reversals only count for steps
    # >= MIN_STEP, and once present the pedal is only given up after the float
    # signature has held continuously for DROP_S with every value inside the
    # band an empty jack wanders in (measured 63..86).
    MIN_STEP, DROP_S, FLOAT_LO, FLOAT_HI = 2, 5.0, 55, 95

    def __init__(self):
        self.hist: list[tuple[float, int]] = []
        self.present = False
        self.still_since: float | None = None
        self.float_since: float | None = None

    def update(self, t: float, v: int) -> bool:
        self.hist.append((t, v))
        while self.hist and t - self.hist[0][0] > self.WINDOW_S:
            self.hist.pop(0)
        vals = [x for _, x in self.hist]
        spread = max(vals) - min(vals)
        flips, last_dir, ref = 0, 0, vals[0]
        for b in vals[1:]:
            if abs(b - ref) < self.MIN_STEP:
                continue                      # sensor jitter, not a direction
            d = 1 if b > ref else -1
            if last_dir and d != last_dir:
                flips += 1
            last_dir, ref = d, b
        floating = (spread >= self.FLOAT_SPREAD and flips >= self.FLOAT_FLIPS
                    and min(vals) >= self.FLOAT_LO and max(vals) <= self.FLOAT_HI)
        if floating:
            self.still_since = None
            self.float_since = self.float_since if self.float_since is not None else t
            if not self.present or t - self.float_since >= self.DROP_S:
                self.present = False
        else:
            self.float_since = None
            recent = [x for tt, x in self.hist if t - tt <= self.STILL_S]
            if recent and max(recent) - min(recent) <= self.STILL_SPREAD:
                self.still_since = self.still_since if self.still_since is not None else t
                if t - self.still_since >= self.STILL_S or self.present:
                    self.present = True
            else:
                self.still_since = None
        return self.present


def fsr_scale(raw: int) -> int:
    """FSR — (raw/4095)^2 perceptual curve → 0..127."""
    norm = max(0.0, min(1.0, raw / 4095.0))
    return _clip127(int(127 * norm * norm))


def tof_scale(mm: int):
    """LiDAR mm → 0..127 mapped from 2..46 cm. -1/timeout/out-of-range → skip."""
    if mm < 0:
        return None
    cm = mm // 10
    if cm < 2 or cm > 46:
        return None
    return _clip127(int((cm - 2) * 127 / (46 - 2)))


def accel_scale(raw: int) -> int:
    """Accelerometer i16 ±17000 → 0..127 centered at 64."""
    v = max(-17000, min(17000, raw))
    return _clip127(int((v + 17000) * 127 / 34000))


def mic_scale(env: int) -> int:
    """Mic envelope → 0..127 mapped from 0..800 (12-bit scaled from old 0..200)."""
    return _clip127(int(env * 127 / 800))


def joy_hi(raw: int) -> float:
    return _clipf(raw * 127.0 / 4095.0)


def exp_hi(raw: int) -> float:
    return _clipf((raw - 128) * 127.0 / (3992 - 128)) if raw >= 128 else 0.0


def fsr_hi(raw: int) -> float:
    norm = max(0.0, min(1.0, raw / 4095.0))
    return _clipf(127.0 * norm * norm)


def tof_hi(mm: int):
    """Same window as tof_scale (20..469 mm) at the sensor's native 1 mm."""
    if mm < 20 or mm >= 470:
        return None
    return _clipf((mm - 20) * 127.0 / 440.0)


def accel_hi(raw: int) -> float:
    v = max(-17000, min(17000, raw))
    return _clipf((v + 17000) * 127.0 / 34000.0)


def mic_hi(env: int) -> float:
    return _clipf(env * 127.0 / 800.0)


# field -> (cc, hi-res scaler, deadband in raw units)
HIRES_MAP = {
    "jx":   (27, joy_hi,   2),
    "jy":   (28, joy_hi,   2),
    "exp":  (29, exp_hi,   2),
    "fsr1": (34, fsr_hi,   2),
    "fsr2": (35, fsr_hi,   2),
    "tof1": (30, tof_hi,   1),
    "tof2": (36, tof_hi,   1),
    "ax":   (31, accel_hi, 16),
    "ay":   (32, accel_hi, 16),
    "az":   (33, accel_hi, 16),
    "ml":   (78, mic_hi,   1),
    "mr":   (79, mic_hi,   1),
}


ANALOG_MAP = {
    "jx":   (27, joy_scale),
    "jy":   (28, joy_scale),
    "exp":  (29, exp_scale),
    "fsr1": (34, fsr_scale),
    "fsr2": (35, fsr_scale),
    "tof1": (30, tof_scale),
    "tof2": (36, tof_scale),
    "ax":   (31, accel_scale),
    "ay":   (32, accel_scale),
    "az":   (33, accel_scale),
    "ml":   (78, mic_scale),
    "mr":   (79, mic_scale),
}


def parse_frame(line: str):
    """Parse one 'NEPTR2 key=value key=value ...' line. Returns dict or None."""
    if not line.startswith("NEPTR2 "):
        return None
    out = {}
    for tok in line.split()[1:]:
        if "=" not in tok:
            continue
        k, v = tok.split("=", 1)
        try:
            out[k] = int(v)
        except ValueError:
            pass
    return out


def encoder_events(frame: dict):
    """Encoder deltas in a parsed frame -> [(cc, value)], one per click, CW = 127."""
    evs = []
    for field, cc in ENCODER_CCS.items():
        d = frame.get(field, 0)
        if not d:
            continue
        if ENCODER_FLIP[field]:
            d = -d
        evs += [(cc, 127 if d > 0 else 0)] * abs(d)
    return evs


class SerialReader:
    """Line-oriented reader for a Teensy USB-CDC port using raw fd + termios.

    Opens the port like `cat` does (no DTR/RTS dance, no pyserial), puts it
    into raw 115200 8N1, and yields decoded text lines. Empty/short reads are
    normal and ignored. On any OS error (Teensy re-flash, USB bounce) the
    port is closed and transparently reopened.
    """

    def __init__(self, path: str):
        self.path = path
        self.fd = -1
        self._buf = b""

    def _open(self) -> None:
        # Block here until the port exists and opens cleanly. The Teensy may
        # be mid-reboot (re-flash / power-up); retry quietly.
        while True:
            try:
                # Blocking open (no O_NONBLOCK): VMIN=0/VTIME below give the
                # read a short timeout, and we also gate on select(). Opening
                # non-blocking makes the CDC fd return EAGAIN on os.read, which
                # is not a disconnect — simplest to avoid it entirely.
                fd = os.open(self.path, os.O_RDONLY | os.O_NOCTTY)
                self._configure(fd)
                self.fd = fd
                self._buf = b""
                return
            except OSError as e:
                print(f"[bridge] waiting for {self.path}: {e}",
                      file=sys.stderr, flush=True)
                time.sleep(1.0)

    @staticmethod
    def _configure(fd: int) -> None:
        attrs = termios.tcgetattr(fd)
        iflag, oflag, cflag, lflag, ispeed, ospeed, cc = attrs
        # Raw mode: no input/output/line processing.
        iflag = 0
        oflag = 0
        lflag = 0
        cflag = (cflag | termios.CLOCAL | termios.CREAD) & ~termios.CSIZE | termios.CS8
        cflag &= ~(termios.PARENB | termios.CSTOPB)
        if hasattr(termios, "CRTSCTS"):
            cflag &= ~termios.CRTSCTS
        ispeed = ospeed = termios.B115200
        cc = list(cc)
        cc[termios.VMIN] = 0
        cc[termios.VTIME] = 1   # 0.1 s read timeout (we also gate on select)
        termios.tcsetattr(fd, termios.TCSANOW,
                          [iflag, oflag, cflag, lflag, ispeed, ospeed, cc])
        try:
            termios.tcflush(fd, termios.TCIFLUSH)
        except OSError:
            pass

    def _own_hub(self) -> None:
        """While serial frames flow, the bridge is the body's only path into the
        hub: drop the Teensy's own USB-MIDI -> Midi Through link (udev and the
        launcher re-add it on every USB event; both paths = every encoder step
        counted twice). If serial dies this stops running and MIDI stays as the
        fallback. Every 5 s at most."""
        now = time.monotonic()
        if now - getattr(self, "_hub_t", 0.0) < 5.0:
            return
        self._hub_t = now
        subprocess.run(["aconnect", "-d", "Teensy MIDI:0", "14:0"],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    def _gone(self) -> bool:
        try:
            st = os.fstat(self.fd)
            return st.st_nlink == 0 or os.stat(self.path).st_rdev != st.st_rdev
        except OSError:
            return True

    def _reopen(self, why: str) -> None:
        print(f"[bridge] serial drop ({why}); reconnecting",
              file=sys.stderr, flush=True)
        try:
            if self.fd >= 0:
                os.close(self.fd)
        except OSError:
            pass
        self.fd = -1
        time.sleep(0.5)
        self._open()

    def lines(self):
        """Yield decoded, stripped text lines forever, reconnecting as needed."""
        if self.fd < 0:
            self._open()
        while True:
            try:
                r, _, _ = select.select([self.fd], [], [], 0.5)
                if not r:
                    continue
                chunk = os.read(self.fd, 512)
            except OSError as e:
                if e.errno in (errno.EAGAIN, errno.EWOULDBLOCK, errno.EINTR):
                    # Transient: CDC signalled readable but had nothing yet, or
                    # the call was interrupted. Not a disconnect — keep going.
                    continue
                self._reopen(str(e))
                continue
            if not chunk:
                # CDC readable-but-empty: normal for Teensy, not a disconnect --
                # UNLESS the device node went away under us. The launcher bounces
                # USB on every engine start (2026-10-24): the Teensy re-enumerates,
                # the old /dev/ttyACMn is deleted, and this fd reads empty forever
                # (bridge spun at ~87% CPU, body silent). Reopen via self.path.
                if self._gone():
                    self._reopen("device node replaced")
                continue
            self._buf += chunk
            while b"\n" in self._buf:
                raw, self._buf = self._buf.split(b"\n", 1)
                yield raw.decode("ascii", errors="ignore").strip()
                self._own_hub()
            # Guard against an unbounded buffer if a line never terminates.
            if len(self._buf) > 4096:
                self._buf = self._buf[-1024:]


def tap_age_ms(t_frame: float) -> int:
    """Age of a button press, in whole ms, for the ch16 CC113 tap event (0..127)."""
    age = TAP_FRAME_AGE_MS + (time.monotonic() - t_frame) * 1000.0
    return max(0, min(127, int(round(age))))


def main() -> int:
    port_path = sys.argv[1] if len(sys.argv) > 1 else PORT_DEFAULT

    reader = SerialReader(port_path)

    # client_name matters: without it rtmidi names the ALSA CLIENT "RtMidiOut
    # Client" (only the port gets MIDI_PORT_NAME), and the unit's
    # `aconnect "NEPTR Serial Bridge":0 14:0` matches on client name -> fails.
    out = mido.open_output(MIDI_PORT_NAME, virtual=True,
                           client_name=MIDI_PORT_NAME)

    # 2026-11-02: one ALSA "error sending MIDI message to port" (seq queue full
    # during a USB/power wobble) raised out of send() and killed the bridge:
    # body controls dead until systemd restarted it. A failed send now drops
    # that one CC and logs (rate-limited) instead of exiting.
    class _SafeOut:
        def __init__(self, o):
            self.o, self.err = o, 0
        def send(self, msg):
            try:
                self.o.send(msg)
            except Exception as exc:
                self.err += 1
                if self.err in (1, 10, 100) or self.err % 1000 == 0:
                    print(f"[bridge] send failed x{self.err}: {exc}", flush=True)
    out = _SafeOut(out)
    print(f"[bridge] {port_path} @ {BAUD} → ALSA-seq port '{MIDI_PORT_NAME}'",
          flush=True)

    # State carried across frames so we only emit CCs on actual change — same
    # discipline the MIDI firmware used. Initial values are "unknown sentinel"
    # so the first frame always emits its scaled values once.
    prev_btn:    dict[str, int] = {**{k: 0 for k in BUTTON_CCS},
                                   **{k: 0 for k in ENC_BTN_CCS},
                                   "jb": 0}
    prev_analog: dict[str, int] = {}
    pedal = PedalPresence()
    prev_hi_raw: dict[str, int] = {}
    prev_hi_val: dict[str, float] = {}
    osc = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    osc.setblocking(False)
    osc_err = 0

    for line in reader.lines():
        if not line:
            continue
        t_frame = time.monotonic()   # the instant this process took the frame (tap age is measured from here)

        frame = parse_frame(line)
        if frame is None:
            continue

        # ── Encoder deltas → repeated CC events (one per click) ──
        for cc, val in encoder_events(frame):
            out.send(mido.Message("control_change",
                                   channel=CHANNEL, control=cc, value=val))

        # ── Buttons → CC on state edge (press / release) ──
        for field, cc in BUTTON_CCS.items():
            cur = frame.get(field, 0)
            if cur != prev_btn[field]:
                out.send(mido.Message("control_change",
                                       channel=CHANNEL, control=cc,
                                       value=127 if cur else 0))
                # TAP GATE (2026-10-23, Emory): the arcade is the VCV manual trigger;
                # it is a tempo tap only while the TOP-LEFT encoder click (eb2) is held.
                # Ungated, every trigger re-tapped the rig clock (it reached 300 BPM).
                if cur and field == TAP_BUTTON and prev_btn["eb2"]:
                    out.send(mido.Message("control_change",
                                           channel=TAP_CHANNEL, control=TAP_CC,
                                           value=tap_age_ms(t_frame)))
                prev_btn[field] = cur
        for field, cc in ENC_BTN_CCS.items():
            cur = frame.get(field, 0)
            if cur != prev_btn[field]:
                out.send(mido.Message("control_change",
                                       channel=CHANNEL, control=cc,
                                       value=127 if cur else 0))
                prev_btn[field] = cur
        jb = frame.get("jb", 0)
        if jb != prev_btn["jb"]:
            out.send(mido.Message("control_change",
                                   channel=CHANNEL, control=JOY_BTN_CC,
                                   value=127 if jb else 0))
            prev_btn["jb"] = jb

        # ── Analogs → high-res OSC /ccf (>= 1024 steps) + derived 7-bit CC ──
        for field, (cc, hi, band) in HIRES_MAP.items():
            raw_val = frame.get(field)
            if raw_val is None:
                continue
            fine = hi(raw_val)
            if fine is None:                    # skip flag (LiDAR out of range)
                continue
            # Empty expression jack: stay silent so the UI slider owns CC29.
            # Presence is judged on the same 7-bit value it always was.
            if field == "exp" and not pedal.update(t_frame, exp_scale(raw_val)):
                continue
            last_raw = prev_hi_raw.get(field)
            last_val = prev_hi_val.get(field)
            moved = (last_raw is None or abs(raw_val - last_raw) >= band
                     or (fine in (0.0, 127.0) and fine != last_val))
            if not moved:
                continue
            prev_hi_raw[field] = raw_val
            prev_hi_val[field] = fine
            try:
                osc.sendto(osc_ccf(OSC_CH, cc, fine), CTL_OSC_ADDR)
            except OSError as exc:
                osc_err += 1
                if osc_err in (1, 10, 100) or osc_err % 1000 == 0:
                    print(f"[bridge] /ccf send failed x{osc_err}: {exc}", flush=True)
            scaled = _clip127(int(fine))        # same floor the 7-bit scalers used
            if prev_analog.get(field) != scaled:
                out.send(mido.Message("control_change",
                                       channel=CHANNEL, control=cc, value=scaled))
                prev_analog[field] = scaled

    return 0


if __name__ == "__main__":
    sys.exit(main())
