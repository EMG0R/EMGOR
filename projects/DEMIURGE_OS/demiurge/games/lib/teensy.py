"""
teensy.py — Teensy serial reader + frame parser + MIDI emitter.

VENDORED from NEPTR phase4/csound/serial_bridge.py and kept byte-faithful on
purpose: the arcade supervisor is the SINGLE owner of /dev/ttyACM0 (only one
process can read a serial port), so in synth mode it must reproduce exactly what
serial_bridge.py emits — same CCs, same change-only discipline, on the same
'NEPTR Serial Bridge' ALSA-seq virtual port csound subscribes to. csound needs
no change.

The CDC quirk handling (raw fd + termios, tolerate empty/short reads, transparent
reconnect) is copied verbatim from the proven bridge. See that file's header and
../docs/ARCHITECTURE.md for why pyserial is not used.

If serial_bridge.py changes, mirror the change here. The frame/CC contract is
documented in NEPTR phase4/TEENSYY/midi_reference/SENSOR_MAP.md.
"""

from __future__ import annotations

import errno
import os
import select
import sys
import termios
import time

BAUD = 115200  # informational; set via termios B115200 below
MIDI_PORT_NAME = "NEPTR Serial Bridge"
CHANNEL = 0  # MIDI channel 1, 0-indexed

# ── Field → CC mapping (mirrors the MIDI firmware) ──────────────────────────
ENCODER_CCS = {f"e{i + 1}": 20 + i for i in range(7)}   # e1=20 ... e7=26
BUTTON_CCS = {f"b{i + 1}": 60 + i for i in range(9)}    # b1=60 ... b9=68
ENC_BTN_CCS = {f"eb{i + 1}": 70 + i for i in range(7)}  # eb1=70 ... eb7=76
JOY_BTN_CC = 77


def _clip127(v: int) -> int:
    return max(0, min(127, int(v)))


def joy_scale(raw: int) -> int:
    return _clip127(raw * 127 // 4095)


def exp_scale(raw: int) -> int:
    span = 3992 - 128
    return _clip127((raw - 128) * 127 // span) if raw >= 128 else 0


def fsr_scale(raw: int) -> int:
    norm = max(0.0, min(1.0, raw / 4095.0))
    return _clip127(int(127 * norm * norm))


def tof_scale(mm: int):
    if mm < 0:
        return None
    cm = mm // 10
    if cm < 2 or cm > 46:
        return None
    return _clip127(int((cm - 2) * 127 / (46 - 2)))


def accel_scale(raw: int) -> int:
    v = max(-17000, min(17000, raw))
    return _clip127(int((v + 17000) * 127 / 34000))


def mic_scale(env: int) -> int:
    return _clip127(int(env * 127 / 800))


ANALOG_MAP = {
    "jx": (27, joy_scale),
    "jy": (28, joy_scale),
    "exp": (29, exp_scale),
    "fsr1": (34, fsr_scale),
    "fsr2": (35, fsr_scale),
    "tof1": (30, tof_scale),
    "tof2": (36, tof_scale),
    "ax": (31, accel_scale),
    "ay": (32, accel_scale),
    "az": (33, accel_scale),
    "ml": (78, mic_scale),
    "mr": (79, mic_scale),
}


def parse_frame(line: str):
    """Parse one 'NEPTR2 key=value ...' line. Returns dict[str,int] or None."""
    if not line.startswith("NEPTR2 "):
        return None
    out: dict[str, int] = {}
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
    """Line-oriented reader for the Teensy USB-CDC port (raw fd + termios).

    Copied from serial_bridge.py. Opens like `cat`, raw 115200 8N1, tolerates
    empty/short reads, transparently reopens on USB bounce / re-flash.
    """

    def __init__(self, path: str):
        self.path = path
        self.fd = -1
        self._buf = b""

    def _open(self) -> None:
        while True:
            try:
                fd = os.open(self.path, os.O_RDONLY | os.O_NOCTTY)
                self._configure(fd)
                self.fd = fd
                self._buf = b""
                return
            except OSError as e:
                print(f"[arcade] waiting for {self.path}: {e}",
                      file=sys.stderr, flush=True)
                time.sleep(1.0)

    @staticmethod
    def _configure(fd: int) -> None:
        attrs = termios.tcgetattr(fd)
        iflag, oflag, cflag, lflag, ispeed, ospeed, cc = attrs
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
        cc[termios.VTIME] = 1
        termios.tcsetattr(fd, termios.TCSANOW,
                          [iflag, oflag, cflag, lflag, ispeed, ospeed, cc])
        try:
            termios.tcflush(fd, termios.TCIFLUSH)
        except OSError:
            pass

    def _reopen(self, why: str) -> None:
        print(f"[arcade] serial drop ({why}); reconnecting",
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
                    continue
                self._reopen(str(e))
                continue
            if not chunk:
                continue
            self._buf += chunk
            while b"\n" in self._buf:
                raw, self._buf = self._buf.split(b"\n", 1)
                yield raw.decode("ascii", errors="ignore").strip()
            if len(self._buf) > 4096:
                self._buf = self._buf[-1024:]


class MidiEmitter:
    """Synth-mode output: re-emits the exact CC traffic serial_bridge.py does.

    Holds the change-detection state (only emit a CC when its value actually
    changes) so csound sees identical sparse CC traffic. Open lazily so importing
    this module never requires mido/ALSA to be present (the tests don't need it).
    """

    def __init__(self):
        import mido  # lazy: only synth mode needs the MIDI stack
        self._mido = mido
        self.out = mido.open_output(MIDI_PORT_NAME, virtual=True)
        self._prev_btn: dict[str, int] = {
            **{k: 0 for k in BUTTON_CCS},
            **{k: 0 for k in ENC_BTN_CCS},
            "jb": 0,
        }
        self._prev_analog: dict[str, int] = {}
        print(f"[arcade] synth MIDI → ALSA-seq port '{MIDI_PORT_NAME}'",
              flush=True)

    def _cc(self, control: int, value: int) -> None:
        self.out.send(self._mido.Message(
            "control_change", channel=CHANNEL, control=control, value=value))

    def emit(self, frame: dict[str, int]) -> None:
        for field, cc in ENCODER_CCS.items():
            d = frame.get(field, 0)
            if not d:
                continue
            val = 127 if d > 0 else 0
            for _ in range(abs(d)):
                self._cc(cc, val)

        for field, cc in BUTTON_CCS.items():
            cur = frame.get(field, 0)
            if cur != self._prev_btn[field]:
                self._cc(cc, 127 if cur else 0)
                self._prev_btn[field] = cur
        for field, cc in ENC_BTN_CCS.items():
            cur = frame.get(field, 0)
            if cur != self._prev_btn[field]:
                self._cc(cc, 127 if cur else 0)
                self._prev_btn[field] = cur
        jb = frame.get("jb", 0)
        if jb != self._prev_btn["jb"]:
            self._cc(JOY_BTN_CC, 127 if jb else 0)
            self._prev_btn["jb"] = jb

        for field, (cc, scaler) in ANALOG_MAP.items():
            raw_val = frame.get(field)
            if raw_val is None:
                continue
            scaled = scaler(raw_val)
            if scaled is None:
                continue
            if self._prev_analog.get(field) != scaled:
                self._cc(cc, scaled)
                self._prev_analog[field] = scaled

    def close(self) -> None:
        try:
            self.out.close()
        except Exception:
            pass
