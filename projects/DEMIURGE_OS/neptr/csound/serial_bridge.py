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
import sys
import termios
import time

import mido

PORT_DEFAULT    = "/dev/ttyACM0"
BAUD            = 115200          # informational; set via termios B115200 below
MIDI_PORT_NAME  = "NEPTR Serial Bridge"
CHANNEL         = 0   # MIDI channel 1, 0-indexed

# ── Field → CC mapping (mirrors the MIDI firmware) ──────────────────────────
ENCODER_CCS = {f"e{i+1}":  20 + i for i in range(7)}   # e1=20 ... e7=26
BUTTON_CCS  = {f"b{i+1}":  60 + i for i in range(9)}   # b1=60 ... b9=68
ENC_BTN_CCS = {f"eb{i+1}": 70 + i for i in range(7)}   # eb1=70 ... eb7=76
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
# The Teensy streams frames at 100 Hz and reports button STATE, so a press edge seen in a
# frame happened somewhere in the preceding 10 ms: on average 5 ms ago. Plus however long
# this process held the frame before getting to the send. (A Teensy-side timestamp would
# remove the 5 ms guess; the Pi-side stamp is the part that needs no firmware flash.)
TAP_FRAME_AGE_MS = 5.0


def _clip127(v: int) -> int:
    return max(0, min(127, int(v)))


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
                # CDC readable-but-empty: normal for Teensy, not a disconnect.
                continue
            self._buf += chunk
            while b"\n" in self._buf:
                raw, self._buf = self._buf.split(b"\n", 1)
                yield raw.decode("ascii", errors="ignore").strip()
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

    out = mido.open_output(MIDI_PORT_NAME, virtual=True)
    print(f"[bridge] {port_path} @ {BAUD} → ALSA-seq port '{MIDI_PORT_NAME}'",
          flush=True)

    # State carried across frames so we only emit CCs on actual change — same
    # discipline the MIDI firmware used. Initial values are "unknown sentinel"
    # so the first frame always emits its scaled values once.
    prev_btn:    dict[str, int] = {**{k: 0 for k in BUTTON_CCS},
                                   **{k: 0 for k in ENC_BTN_CCS},
                                   "jb": 0}
    prev_analog: dict[str, int] = {}

    for line in reader.lines():
        if not line:
            continue
        t_frame = time.monotonic()   # the instant this process took the frame (tap age is measured from here)

        frame = parse_frame(line)
        if frame is None:
            continue

        # ── Encoder deltas → repeated CC events (one per click) ──
        for field, cc in ENCODER_CCS.items():
            d = frame.get(field, 0)
            if not d:
                continue
            val = 127 if d > 0 else 0
            for _ in range(abs(d)):
                out.send(mido.Message("control_change",
                                       channel=CHANNEL, control=cc, value=val))

        # ── Buttons → CC on state edge (press / release) ──
        for field, cc in BUTTON_CCS.items():
            cur = frame.get(field, 0)
            if cur != prev_btn[field]:
                out.send(mido.Message("control_change",
                                       channel=CHANNEL, control=cc,
                                       value=127 if cur else 0))
                if cur and field == TAP_BUTTON:
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

        # ── Analogs → 7-bit scaled, emitted only on change ──
        for field, (cc, scaler) in ANALOG_MAP.items():
            raw_val = frame.get(field)
            if raw_val is None:
                continue
            scaled = scaler(raw_val)
            if scaled is None:                  # skip flag from the scaler
                continue
            if prev_analog.get(field) != scaled:
                out.send(mido.Message("control_change",
                                       channel=CHANNEL, control=cc, value=scaled))
                prev_analog[field] = scaled

    return 0


if __name__ == "__main__":
    sys.exit(main())
