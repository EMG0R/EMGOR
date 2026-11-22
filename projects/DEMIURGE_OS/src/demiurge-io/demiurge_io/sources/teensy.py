"""sources/teensy.py — parses the Teensy's 100 Hz "NEPTR2" serial frame.

Frame format and field meanings are documented in
    ../TEENSYY phase4/TEENSYY.ino  (lines ~1-60, sensor scalers ~175-280)
and this module is a deliberate sibling of
    ../TEENSYY phase4/csound/serial_bridge.py
reusing its exact raw-fd/termios reading approach (see SerialReader below,
copied near-verbatim) because of a documented pyserial bug on this hardware:
the Teensy enumerates as USB "Serial + MIDI", so /dev/ttyACM0 is a CDC ACM
port, and pyserial's POSIX backend misreads its intermittent zero-length CDC
URBs as a fatal disconnect. Opening the fd the way `cat` does (raw termios,
select + os.read, tolerate empty/short reads) sidesteps the bug entirely.
See serial_bridge.py's own "Serial I/O note" docstring for the full story.

TWO KINDS OF FIELD IN ONE FRAME, TREATED DIFFERENTLY:

  SENSORS (jx, jy, exp, fsr1, fsr2, tof1, tof2, ax, ay, az, ml, mr) are
  continuous data. They are emitted here at FULL RAW RESOLUTION — no 7-bit
  squashing — because that resolution is the whole point of this package
  (a LiDAR at 2-46cm is 3.5mm/step at 7 bits vs ~1mm/step raw; recovering
  that is a primary goal, see the package docstring). These go through
  conditioning.py -> router.py before anything is sent anywhere.

  CONTROLS (encoders e1-7, buttons b1-9, encoder-buttons eb1-7, joystick
  button jb) are discrete UI events — menu navigation, mode toggles — with
  an existing, working CC mapping (serial_bridge.py's ENCODER_CCS/BUTTON_CCS/
  ENC_BTN_CCS/JOY_BTN_CC). They BYPASS conditioning and the router entirely
  and are translated straight to `/cc` messages here, byte-compatible with
  serial_bridge.py's mapping, so csound's handler_remap/handler_looper/
  handler_menus chain sees identical traffic whether serial_bridge.py or
  this daemon is the one running.
"""

from __future__ import annotations

import errno
import os
import select
import sys
import time
from typing import Iterator, NamedTuple, Optional

from ..model import Reading, SourceDescriptor

# ---------------------------------------------------------------------------
# WHICH SERIAL PORT IS THE NEPTR BODY?
#
# NOT /dev/ttyACM0. That was the default until 2026-09-15 and it is an INDEX,
# assigned in USB enumeration order, which is a race between every CDC device
# on the bus. On this rig there are up to three: the NEPTR body's Teensy, the
# DUODECIMUS foot controller (also a Teensy — same 16c0:0485 vendor/product),
# and the Bela. Measured after one reboot:
#
#   /dev/serial/by-id/usb-Teensyduino_Teensy_MIDI_19910760-if00  -> ttyACM1
#   /dev/serial/by-id/usb-Augmented_Instruments_Ltd_Bela-...-if04 -> ttyACM2
#   /dev/ttyACM0                                                  DID NOT EXIST
#
# so the daemon was opening a path that was not there, and on a different boot
# order it would have opened the Bela or the foot controller instead and sat
# waiting forever for NEPTR2 frames that were never coming. Either way the
# body's sensors are silently dead, with no error that names the cause.
#
# /dev/serial/by-id/ names carry the USB product string and serial number, so
# they are stable across reboots and across which port came up first. We prefer
# those, exclude the two devices we know are NOT the body, and only fall back
# to scanning ttyACM* when the by-id directory is missing entirely.
#
# The body identifies itself as "Teensy_MIDI" (the stock Teensyduino product
# string); DUODECIMUS's firmware overrides its USB product name, which is why
# it shows up as its own ALSA client and does not collide with this match.
_BYID_DIR = "/dev/serial/by-id"
_BODY_HINTS = ("Teensy",)
_NOT_BODY = ("Bela", "DUODECIMUS")


def resolve_port(explicit: Optional[str] = None) -> str:
    """Best guess at the NEPTR body's serial port, stable across reboots.

    An explicit path always wins — this only supplies the default, and a
    caller who knows better should not be second-guessed.
    """
    if explicit:
        return explicit

    try:
        names = sorted(os.listdir(_BYID_DIR))
    except OSError:
        names = []

    for name in names:
        if any(bad.lower() in name.lower() for bad in _NOT_BODY):
            continue
        if any(hint.lower() in name.lower() for hint in _BODY_HINTS):
            return os.path.join(_BYID_DIR, name)

    # No by-id entry matched. Fall back to the lowest ttyACM that is NOT one of
    # the devices we can positively identify as something else — better than
    # hardcoding ttyACM0, which is how this broke in the first place.
    excluded = set()
    for name in names:
        if any(bad.lower() in name.lower() for bad in _NOT_BODY):
            try:
                excluded.add(os.path.realpath(os.path.join(_BYID_DIR, name)))
            except OSError:
                pass
    import glob as _glob
    for cand in sorted(_glob.glob("/dev/ttyACM*")):
        if os.path.realpath(cand) not in excluded:
            return cand

    # Nothing plugged in. Return the historical default so the error message
    # the caller produces is the familiar one.
    return "/dev/ttyACM0"


PORT_DEFAULT = resolve_port()
BAUD = 115200

# MIDI channel these CCs are sent on. The Teensy's own USB-MIDI mirror uses
# MIDI_CHANNEL=1 (TEENSYY.ino) and csound's OSC control-input loop compares
# kCh directly against Csound-convention 1-16 channel numbers (see e.g.
# `kClockRole ctrl7 16, 116, ...` and `if (kCh == 16 && kCC == 102...)` in
# midi_handler.orc) — NOT the 0-indexed wire nibble mido uses internally.
# So unlike serial_bridge.py's CHANNEL=0 (a mido-internal value that still
# lands on physical channel 1 once on the wire), our OSC /cc sends CHANNEL=1
# directly, to match what csound's kCh actually compares against.
CHANNEL = 1

# ── Control-field -> CC mapping (mirrors serial_bridge.py exactly) ─────────
ENCODER_CCS = {f"e{i + 1}": 20 + i for i in range(7)}    # e1=20 ... e7=26
BUTTON_CCS = {f"b{i + 1}": 60 + i for i in range(9)}     # b1=60 ... b9=68
ENC_BTN_CCS = {f"eb{i + 1}": 70 + i for i in range(7)}   # eb1=70 ... eb7=76
JOY_BTN_CC = 77

# ── Sensor descriptors — hardware truth, not user calibration ──────────────
# raw_min/raw_max are the ADC/sensor's native range; dropout_value is the
# sentinel the firmware emits for "no reading" (LiDAR timeout only).
# ---------------------------------------------------------------------------
# ONBOARD EXPRESSION PEDAL — ch1 CC29 IS NOT AVAILABLE TO IT.
#
# CC29 belongs to the DUODECIMUS foot controller's treadle, which reaches the
# engine as ch7 CC14/15 and is rewritten to ch1 CC29 by midi_handler.orc. That
# pedal started working correctly on 2026-09-15 for the first time, and the
# reason there is no contention is that the onboard `exp` sensor below is
# currently routed NOWHERE (~/demiurge/sensors.json carries `maps: []`).
#
# Two sources writing the same CC do not merge, they FIGHT: expr_cc29.orc's
# soft-takeover disarms on every owner change and only re-arms once the pedal
# physically moves >0.03, so a second writer produces a pedal that
# intermittently ignores you — a symptom that looks like flaky hardware and is
# miserable to trace back to a routing table.
#
# So: if the onboard pedal is ever given a destination, it goes HERE, to CC30.
# ch1's map is 20-26 rotations, 29 expression, 70-77 buttons, 110/111 poly
# delay — 30 is free.
ONBOARD_EXPR_CC = 30
DUODECIMUS_EXPR_CC = 29        # reserved; never emit this from this module

SENSOR_DESCRIPTORS: dict[str, SourceDescriptor] = {
    "jx":   SourceDescriptor("jx",   "Joystick X",       "raw",   0,     4095, 100.0),
    "jy":   SourceDescriptor("jy",   "Joystick Y",       "raw",   0,     4095, 100.0),
    "exp":  SourceDescriptor("exp",  "Expression pedal", "raw",   128,   3992, 100.0),
    "fsr1": SourceDescriptor("fsr1", "FSR 1",            "raw",   0,     4095, 100.0,
                             use_min=2500, use_max=4000),   # FSR_MIN/MAX_RAW
    "fsr2": SourceDescriptor("fsr2", "FSR 2",            "raw",   0,     4095, 100.0,
                             use_min=2500, use_max=4000),
    "tof1": SourceDescriptor("tof1", "LiDAR 1",          "mm",    0,     4000, 100.0, dropout_value=-1,
                             use_min=20, use_max=460),     # tofScale() gates cm 2..46
    "tof2": SourceDescriptor("tof2", "LiDAR 2",          "mm",    0,     4000, 100.0, dropout_value=-1,
                             use_min=20, use_max=460),
    "ax":   SourceDescriptor("ax",   "Accel X",          "raw",   -17000, 17000, 100.0),
    "ay":   SourceDescriptor("ay",   "Accel Y",          "raw",   -17000, 17000, 100.0),
    "az":   SourceDescriptor("az",   "Accel Z",          "raw",   -17000, 17000, 100.0),
    "ml":   SourceDescriptor("ml",   "Mic L envelope",   "env",   0,     800,  100.0),
    "mr":   SourceDescriptor("mr",   "Mic R envelope",   "env",   0,     800,  100.0),
}
SENSOR_FIELDS = tuple(SENSOR_DESCRIPTORS.keys())


def parse_frame(line: str) -> Optional[dict]:
    """Parse one 'NEPTR2 key=value key=value ...' line. Returns dict or None."""
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


# ── Pool source ids for the body (docs/parameters.md §2: `/hw/<device>/<name>`) ──
# Sensors keep their bare ids on the wire and in sensors.json (that file is
# hardware truth and hand-tuned); the daemon ALIASES them to `/hw/teensy/<id>`
# so a map may name either (see daemon.source_aliases). Discrete body buttons
# had no source at all (they bypassed the router as /cc); they are published
# here as `/hw/teensy/<id>` Readings 0/1 so they can be mapped. Encoders and
# encoder buttons are NOT sources: they keep their fixed menu roles on MIDI.
POOL_PREFIX = "/hw/teensy/"
POOL_BUTTONS = tuple(BUTTON_CCS) + ("jb",)       # b1..b9 + joystick click


def pool_source_id(field: str) -> str:
    return POOL_PREFIX + field


class ControlCC(NamedTuple):
    channel: int
    cc: int
    value: int


def frame_to_readings(frame: dict, t: float) -> list[Reading]:
    """Extract the continuous-sensor fields present in `frame` as Readings."""
    return [Reading(field, frame[field], t)
            for field in SENSOR_FIELDS if field in frame]


class ControlState:
    """Carries the previous button/encoder-button/joybutton levels so we
    only emit a CC on an actual edge — same discipline serial_bridge.py
    uses. Encoders need no state: a delta of 0 is simply skipped."""

    def __init__(self) -> None:
        self.prev_btn: dict[str, int] = {**{k: 0 for k in BUTTON_CCS},
                                          **{k: 0 for k in ENC_BTN_CCS},
                                          "jb": 0}
        # THE FIRST FRAME ESTABLISHES A BASELINE AND EMITS NOTHING.
        #
        # prev_btn starts all-zero, which is an ASSUMPTION that every button is
        # released when we connect. Any button the first frame reports as 1 then
        # reads as a fresh 0->1 edge and fires a press that the player never
        # made. eb7 is CC76, which on the DIGIVERB menu is FREEZE — and FREEZE
        # engages by gesture, so a phantom press silently switches it on.
        # Reported 2026-09-15: "FREEZE KEEPS GETTING TOGGLED ON IDK HOW ... i
        # dont hear it but ui shows it sometimes".
        #
        # It fires on every (re)connect, not just at boot, and the serial stall
        # watchdog added the same day reconnects far more readily than before,
        # which is why it went from rare to frequent.
        #
        # Priming costs one frame — 10 ms at 100 Hz — and removes the entire
        # class of phantom press.
        self.primed = False
        # Same baseline rule for the pool's button Readings (see
        # frame_to_button_readings): a button already down when we connect is
        # BLOCKED until it is seen released, so a held arcade button can never
        # read as a fresh tap/trigger on (re)connect.
        self._btn_primed = False
        self._btn_blocked: set = set()
        self._btn_last: dict = {}

    def frame_to_button_readings(self, frame: dict, t: float) -> list:
        """Body buttons -> `/hw/teensy/<id>` Readings (raw 0/1), EDGE-ONLY.

        Edge-only on purpose: 10 buttons x 100 Hz of identical values would be
        1000 queue items/s for nothing. The first frame emits each released
        button once as 0 (initialises the router's gate state) and withholds any
        button that is down until it is released."""
        out: list = []
        if not self._btn_primed:
            self._btn_primed = True
            for f in POOL_BUTTONS:
                if frame.get(f, 0):
                    self._btn_blocked.add(f)
                else:
                    self._btn_last[f] = 0
                    out.append(Reading(pool_source_id(f), 0, t))
            return out
        for f in POOL_BUTTONS:
            v = 1 if frame.get(f, 0) else 0
            if f in self._btn_blocked:
                if v:
                    continue
                self._btn_blocked.discard(f)
            if self._btn_last.get(f) != v:
                self._btn_last[f] = v
                out.append(Reading(pool_source_id(f), v, t))
        return out

    def frame_to_ccs(self, frame: dict) -> list[ControlCC]:
        out: list[ControlCC] = []

        if not self.primed:
            for field in list(BUTTON_CCS) + list(ENC_BTN_CCS):
                self.prev_btn[field] = frame.get(field, 0)
            self.prev_btn["jb"] = frame.get("jb", 0)
            self.primed = True
            # Encoder deltas in this frame are still real movement, but a frame
            # that arrives mid-turn is not worth the risk of a stray step on
            # reconnect; drop the whole first frame and start clean.
            return out

        for field, cc in ENCODER_CCS.items():
            d = frame.get(field, 0)
            if not d:
                continue
            val = 127 if d > 0 else 0
            for _ in range(abs(d)):
                out.append(ControlCC(CHANNEL, cc, val))

        for field, cc in BUTTON_CCS.items():
            cur = frame.get(field, 0)
            if cur != self.prev_btn[field]:
                out.append(ControlCC(CHANNEL, cc, 127 if cur else 0))
                self.prev_btn[field] = cur

        for field, cc in ENC_BTN_CCS.items():
            cur = frame.get(field, 0)
            if cur != self.prev_btn[field]:
                out.append(ControlCC(CHANNEL, cc, 127 if cur else 0))
                self.prev_btn[field] = cur

        jb = frame.get("jb", 0)
        if jb != self.prev_btn["jb"]:
            out.append(ControlCC(CHANNEL, JOY_BTN_CC, 127 if jb else 0))
            self.prev_btn["jb"] = jb

        return out


# ── Raw-fd serial reader — copied near-verbatim from serial_bridge.py ──────
# See module docstring for why this exists instead of pyserial.

class SerialReader:
    """Line-oriented reader for a Teensy USB-CDC port using raw fd + termios."""

    RETRY_MIN = 1.0
    RETRY_MAX = 30.0

    def __init__(self, path: str):
        self.path = path
        self.fd = -1
        self._buf = b""
        # Whether we are currently in the waiting state, and what we last
        # complained about. Both exist so the log records TRANSITIONS rather
        # than the steady state.
        self._waiting = False
        self._last_err = ""

    @staticmethod
    def _assert_dtr(fd: int) -> None:
        """Raise DTR and RTS so the Teensy starts talking.

        Best-effort: a device that does not implement modem control lines
        raises here, and that is not a reason to refuse to read it.
        """
        import fcntl
        import struct
        import termios
        try:
            # TIOCMBIS = bit-set on the modem control lines
            bits = struct.pack("I", termios.TIOCM_DTR | termios.TIOCM_RTS)
            fcntl.ioctl(fd, termios.TIOCMBIS, bits)
        except (OSError, AttributeError):
            pass

    def _open(self) -> None:
        import termios
        # Backs off, and says so only when something changes. On a rig with no
        # board attached this loop never exits: at one line per second it wrote
        # ~86k lines a day into a now-persistent journal, which is a real cost
        # for a message that says the same thing every time. Retrying once a
        # second forever is also pointless -- a Teensy that is going to appear
        # appears when it is plugged in, and the 30s ceiling still catches that
        # within half a minute.
        delay = self.RETRY_MIN
        while True:
            try:
                # O_RDWR, not O_RDONLY, and DTR is asserted explicitly below.
                #
                # DTR IS NOT DECORATION ON A TEENSY. Its USB serial stack only
                # transmits while the host has the CDC port open AND DTR raised;
                # with DTR low, writes in the sketch are buffered and then
                # dropped. A read-only open does not reliably raise it, so the
                # board can sit there enumerated, healthy, and silent.
                #
                # That is exactly the failure seen on 2026-09-15: after a USB
                # re-enumeration the body stopped emitting NEPTR2 frames, a root
                # `head -c 300 /dev/ttyACM*` returned NOTHING, a software
                # unbind/bind did NOT fix it — and a PHYSICAL unplug/replug did.
                # The difference between those two is VBUS: only the physical
                # one power-cycles the MCU and restarts the sketch. A host that
                # raises DTR properly is the software equivalent, and it means
                # the player does not have to reach for the cable.
                fd = os.open(self.path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
                self._configure(fd)
                self._assert_dtr(fd)
                self.fd = fd
                self._buf = b""
                if self._waiting:
                    print(f"[demiurge-io] {self.path} appeared", file=sys.stderr,
                          flush=True)
                    self._waiting = False
                return
            except OSError as e:
                # First failure, or a DIFFERENT failure: a port that changes
                # from ENOENT to EACCES is a permissions problem wearing the
                # same costume, and silence would hide it.
                msg = str(e)
                if not self._waiting or msg != self._last_err:
                    print(f"[demiurge-io] waiting for {self.path}: {msg}",
                          file=sys.stderr, flush=True)
                    self._last_err = msg
                    self._waiting = True
                time.sleep(delay)
                delay = min(delay * 2.0, self.RETRY_MAX)

    @staticmethod
    def _configure(fd: int) -> None:
        import termios
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
        print(f"[demiurge-io] serial drop ({why}); reconnecting",
              file=sys.stderr, flush=True)
        try:
            if self.fd >= 0:
                os.close(self.fd)
        except OSError:
            pass
        self.fd = -1
        time.sleep(0.5)
        self._open()

    def lines(self) -> Iterator[str]:
        """Yield decoded, stripped text lines forever, reconnecting as needed."""
        if self.fd < 0:
            self._open()
        # STALL WATCHDOG — the board going away must not be silent.
        #
        # On a USB disconnect os.read() returns b"" (EOF) and select() reports
        # the fd readable forever, so the `if not chunk: continue` below span in
        # a tight loop and NEVER reopened. Combined with the device coming back
        # on a different node (ttyACM1 -> ttyACM2 after a re-enumeration), the
        # daemon sat holding a dead descriptor while the body was alive on
        # another one — which is why only a physical replug appeared to fix it.
        #
        # A single empty read cannot be the trigger: this reader deliberately
        # tolerates those, because the Teensy's CDC stack emits zero-length URBs
        # in normal operation (see the module docstring). So we watch for an
        # absence of DATA instead. The frame rate is 100 Hz, so two seconds of
        # nothing is ~200 missed frames and unambiguous.
        STALL_SECONDS = 2.0
        last_data = time.monotonic()
        while True:
            try:
                r, _, _ = select.select([self.fd], [], [], 0.5)
                if not r:
                    if time.monotonic() - last_data > STALL_SECONDS:
                        self._reopen("no data for %.1fs" % STALL_SECONDS)
                        last_data = time.monotonic()
                    continue
                chunk = os.read(self.fd, 512)
            except OSError as e:
                if e.errno in (errno.EAGAIN, errno.EWOULDBLOCK, errno.EINTR):
                    continue
                self._reopen(str(e))
                last_data = time.monotonic()
                continue
            if not chunk:
                # EOF or a zero-length URB — indistinguishable here, so let the
                # watchdog decide. Sleeping briefly keeps an EOF fd from pinning
                # a core while it counts down.
                if time.monotonic() - last_data > STALL_SECONDS:
                    self._reopen("eof / no data for %.1fs" % STALL_SECONDS)
                    last_data = time.monotonic()
                else:
                    time.sleep(0.01)
                continue
            last_data = time.monotonic()
            self._buf += chunk
            while b"\n" in self._buf:
                raw, self._buf = self._buf.split(b"\n", 1)
                yield raw.decode("ascii", errors="ignore").strip()
            if len(self._buf) > 4096:
                self._buf = self._buf[-1024:]


class TeensySource:
    """The live driver: wraps SerialReader, yields (readings, control_ccs)
    per frame. `run()` blocks forever — callers use it from a dedicated
    thread (see daemon.py)."""

    id = "teensy"
    descriptors = SENSOR_DESCRIPTORS

    def __init__(self, path: str = PORT_DEFAULT):
        self.reader = SerialReader(path)
        self.control = ControlState()

    def frames(self) -> Iterator[tuple[list[Reading], list[ControlCC]]]:
        """Yield (readings, control_ccs) for every valid frame, forever."""
        for line in self.reader.lines():
            if not line:
                continue
            frame = parse_frame(line)
            if frame is None:
                continue
            t = time.monotonic()
            readings = (frame_to_readings(frame, t)
                        + self.control.frame_to_button_readings(frame, t))
            yield readings, self.control.frame_to_ccs(frame)

    def run(self, on_reading, on_control) -> None:
        for readings, ccs in self.frames():
            for r in readings:
                on_reading(r)
            if ccs:
                on_control(ccs)
