"""Raw evdev touch reader for the NEPTR UI.

WHY THIS EXISTS (2026-08-09, gesture round 4): SDL2's KMSDRM/evdev path on
the single-touch resistive panel (ADS7846) drops ALL touch motion — an
on-device event dump showed every physical swipe delivered as a
FingerDown/FingerUp pair at the SAME stale coordinate, zero FingerMotion,
zero MouseMotion. Gestures are impossible at the SDL layer on this
hardware. The kernel stream itself is fine (the demiurge-touch daemon
reads it directly and gets full smooth motion), so the UI bypasses SDL for
touch entirely and reads /dev/input the same way the daemon does
(DEMIURGE_OS src/wrappers/demiurge-touch: struct-unpacked input_event,
EV_ABS ABS_X/ABS_Y/ABS_PRESSURE + EV_KEY BTN_TOUCH, EV_SYN framing).
Multiple readers on one evdev node are fine; the daemon keeps running.

The decoder core is pure (feed events, get ("down"|"move"|"up", x, y)
tuples on SYN boundaries) so it is unit-testable with synthesized byte
streams. The reader thread pushes decoded events into a deque and nudges
the UI's main-loop queue when the deque goes non-empty.

On Mac / machines without /proc/bus/input this module reports no device
and the UI falls back to SDL mouse events (dev mode unchanged).
"""

import os
import re
import struct
import threading

EV_FMT = "qqHHi"          # struct input_event, 64-bit
EV_SIZE = struct.calcsize(EV_FMT)

EV_SYN, EV_KEY, EV_ABS = 0x00, 0x01, 0x03
SYN_REPORT = 0x00
BTN_TOUCH = 0x14A
ABS_X, ABS_Y, ABS_PRESSURE = 0x00, 0x01, 0x18
ABS_MT_POSITION_X = 0x35


def find_touch_device():
    """(/dev/input/eventN, name) for the first absolute-XY touch panel —
    same scan/scoring as demiurge-touch. None off-Pi."""
    try:
        with open("/proc/bus/input/devices") as f:
            text = f.read()
    except OSError:
        return None
    candidates = []
    for chunk in text.strip().split("\n\n"):
        name, handlers, ev, absbits = "", "", 0, 0
        for line in chunk.splitlines():
            if line.startswith("N:"):
                m = re.search(r'Name="(.*)"', line)
                name = m.group(1) if m else ""
            elif line.startswith("H:"):
                handlers = line
            elif line.startswith("B: EV="):
                ev = int(line.split("=", 1)[1], 16)
            elif line.startswith("B: ABS="):
                for w in line.split("=", 1)[1].split():
                    absbits = (absbits << 64) | int(w, 16)
        m = re.search(r"(event\d+)", handlers)
        if not m:
            continue
        has_abs = bool(ev & (1 << EV_ABS))
        has_xy = bool(absbits & (1 << ABS_X)) \
            or bool(absbits & (1 << ABS_MT_POSITION_X))
        if has_abs and has_xy:
            score = 2 if re.search(r"touch", name, re.I) else 1
            candidates.append((score, "/dev/input/" + m.group(1), name))
    if not candidates:
        return None
    candidates.sort(reverse=True)
    return candidates[0][1], candidates[0][2]


def read_absinfo(fd, code):
    """EVIOCGABS -> (min, max) for an ABS axis, or None."""
    import fcntl
    buf = bytearray(24)
    req = (2 << 30) | (24 << 16) | (0x45 << 8) | (0x40 + code)
    try:
        fcntl.ioctl(fd, req, buf)
    except OSError:
        return None
    _v, mn, mx, _f, _fl, _r = struct.unpack("iiiiii", buf)
    return (mn, mx) if mx > mn else None


class Decoder:
    """Pure single-touch protocol decoder. feed() one kernel event at a
    time; SYN_REPORT frame boundaries emit ("down"|"move"|"up", x, y).
    "move" only while touching and only when the position changed."""

    def __init__(self):
        self.x = 0
        self.y = 0
        self.touching = False
        self._pend_touch = None       # BTN_TOUCH value seen this frame
        self._pend_pos = False        # X/Y updated this frame
        self.out = []

    def feed(self, etype, code, value):
        if etype == EV_ABS:
            if code == ABS_X:
                self.x = value
                self._pend_pos = True
            elif code == ABS_Y:
                self.y = value
                self._pend_pos = True
        elif etype == EV_KEY and code == BTN_TOUCH:
            self._pend_touch = bool(value)
        elif etype == EV_SYN and code == SYN_REPORT:
            if self._pend_touch is not None \
                    and self._pend_touch != self.touching:
                self.touching = self._pend_touch
                self.out.append(("down" if self.touching else "up",
                                 self.x, self.y))
            elif self.touching and self._pend_pos:
                self.out.append(("move", self.x, self.y))
            self._pend_touch = None
            self._pend_pos = False

    def feed_bytes(self, data):
        """Decode a raw byte stream of input_event structs (tests + reader)."""
        for off in range(0, len(data) - EV_SIZE + 1, EV_SIZE):
            _s, _u, etype, code, value = struct.unpack_from(EV_FMT, data, off)
            self.feed(etype, code, value)

    def take(self):
        out, self.out = self.out, []
        return out


class TouchReader:
    """Daemon-thread reader: raw device -> Decoder -> thread-safe deque.
    `wake` is called (once per empty->non-empty transition) so the UI's
    blocking main loop notices a touch immediately."""

    def __init__(self, wake=None):
        self.dev = None
        self.name = None
        self.ranges = {}
        self._wake = wake or (lambda: None)
        self._events = []
        self._lock = threading.Lock()
        self._stop = threading.Event()
        if os.environ.get("NEPTR_NO_EVDEV", "0") == "1":
            return
        found = find_touch_device()
        if found:
            self.dev, self.name = found

    @property
    def available(self):
        return self.dev is not None

    def start(self):
        if not self.available:
            return False
        try:
            fd = open(self.dev, "rb", buffering=0)
        except OSError:
            self.dev = None
            return False
        for code in (ABS_X, ABS_Y, ABS_PRESSURE):
            r = read_absinfo(fd, code)
            if r:
                self.ranges[code] = r
        self._fd = fd
        threading.Thread(target=self._run, daemon=True).start()
        return True

    def stop(self):
        self._stop.set()

    def _run(self):
        import select
        dec = Decoder()
        try:
            while not self._stop.is_set():
                r, _, _ = select.select([self._fd], [], [], 0.5)
                if not r:
                    continue
                data = self._fd.read(EV_SIZE)
                if not data or len(data) < EV_SIZE:
                    break
                dec.feed_bytes(data)
                evs = dec.take()
                if evs:
                    with self._lock:
                        was_empty = not self._events
                        self._events.extend(evs)
                    if was_empty:
                        self._wake()
        except OSError:
            pass
        finally:
            try:
                self._fd.close()
            except OSError:
                pass

    def drain(self):
        """All queued (kind, rawx, rawy) events, oldest first."""
        with self._lock:
            out, self._events = self._events, []
        return out


class GapBridger:
    """Contact-bounce bridge for the resistive panel. A light or fast swipe
    makes the panel's pressure dip below threshold mid-stroke: BTN_TOUCH
    cycles 0/1 and one physical swipe arrives as several down/up fragments —
    observed on-device as one user swipe classified into FOUR taps. A real
    finger cannot lift and re-press within GRACE seconds, so an 'up'
    followed by a 'down' inside GRACE is rewritten into a 'move' (stroke
    continues); the 'up' is only released after GRACE with no re-press.
    Cost: tap release-fire is delayed by GRACE (~70 ms) — still snappy."""

    GRACE = 0.07

    def __init__(self, grace=None):
        if grace is not None:
            self.GRACE = grace
        self._pending = None          # (x, y, t) of a held 'up'

    def apply(self, kind, x, y, now):
        """-> list of events to dispatch now."""
        if kind == "down":
            if self._pending is not None:
                self._pending = None
                return [("move", x, y)]          # bounce: bridge the gap
            return [("down", x, y)]
        if kind == "up":
            self._pending = (x, y, now)
            return []
        return [("move", x, y)]

    def flush(self, now):
        """-> the held 'up' once GRACE passed with no re-press."""
        if self._pending is not None and now - self._pending[2] >= self.GRACE:
            x, y, _ = self._pending
            self._pending = None
            return [("up", x, y)]
        return []


class Smoother:
    """Light touch-stream conditioner (canvas coords), for the resistive
    panel's contact jitter — "needs some smoothing vibe just a bit":
      * moves are EMA-filtered (alpha ~0.45: perceptible smoothing, ~1-2
        frame lag at the ADS7846's report rate);
      * sub-min_px post-filter moves are dropped (micro-jitter deadband);
      * 'up' reuses the last smoothed position — ADS7846 release frames
        can carry stale/laggard coordinates (the SDL bug's other half).
    Reset on every 'down' so strokes never bleed into each other."""

    def __init__(self, alpha=0.45, min_px=2):
        self.alpha = alpha
        self.min_px = min_px
        self.sx = self.sy = None
        self.last = None

    def apply(self, kind, x, y):
        """-> (kind, x, y) conditioned, or None to drop the event."""
        if kind == "down":
            self.sx, self.sy = float(x), float(y)
            self.last = (x, y)
            return (kind, x, y)
        if kind == "move":
            if self.sx is None:               # move without a down: as-is
                self.sx, self.sy = float(x), float(y)
            self.sx += self.alpha * (x - self.sx)
            self.sy += self.alpha * (y - self.sy)
            nx, ny = int(round(self.sx)), int(round(self.sy))
            if self.last is not None \
                    and abs(nx - self.last[0]) < self.min_px \
                    and abs(ny - self.last[1]) < self.min_px:
                return None
            self.last = (nx, ny)
            return ("move", nx, ny)
        # 'up': trust the smoothed track, not the release frame's coords.
        pos = self.last if self.last is not None else (x, y)
        self.sx = self.sy = None
        self.last = None
        return ("up", pos[0], pos[1])


def coalesce(events):
    """Collapse runs of consecutive 'move' events to their last position —
    one position update per drained batch segment — while preserving every
    'down'/'up' and their ordering (the classifier needs the sequence)."""
    out = []
    for ev in events:
        if ev[0] == "move" and out and out[-1][0] == "move":
            out[-1] = ev
        else:
            out.append(ev)
    return out
