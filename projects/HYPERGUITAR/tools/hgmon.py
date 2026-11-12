#!/usr/bin/env python3
"""
hgmon.py -- real-time terminal monitor for the hyperGuitar Bela rig.

Listens for HGMON telemetry lines on UDP (default 0.0.0.0:9000) and renders a
live curses dashboard: 10 audio input meters, 2 output meters, and a sensor
panel (accel / gyro / ToF / expression / pot / joystick / encoders).

Design rules baked in:
  * MISSING is not ZERO.  A field absent from the packet renders as a dim
    em-dash, never as 0.00.  A ToF of -1 renders as "no reading", which is
    again distinct from both.
  * STALE IS LOUD.  If nothing has arrived for >1 s the whole screen dims and
    a reverse-video banner reports how long the link has been dead.
  * Peak-hold on every audio meter, so transients are visible.

Keys:  q / Esc quit      p reset peak holds      c clear counters

Stdlib only.  Python 3.8+.  Tested on macOS.
"""

from __future__ import annotations

import argparse
import curses
import errno
import locale
import math
import socket
import sys
import time
from collections import deque

# ---------------------------------------------------------------------------
# configuration
# ---------------------------------------------------------------------------

DEFAULT_HOST = "0.0.0.0"
DEFAULT_PORT = 9000

STALE_AFTER = 1.0          # seconds with no packet before we scream
DB_FLOOR = -80.0           # bottom of the audio meter scale
DB_CEIL = 0.0              # top of the audio meter scale
PEAK_HOLD = 1.0            # seconds a peak marker sits still
PEAK_FALL = 24.0           # dB per second the peak marker falls after that
ACC_FS = 2.0               # accelerometer full scale, g
GYR_FS = 250.0             # gyro full scale, deg/s
TOF_FS = 2000.0            # ToF bar full scale, mm
FRAME = 0.033              # redraw period, seconds

IN_LABELS = [
    "0 gtr tip",
    "1 gtr ring",
    "2 piezo A",
    "3 piezo B",
    "4 hex E-lo",
    "5 hex A",
    "6 hex D",
    "7 hex G",
    "8 hex B",
    "9 hex E-hi",
]
OUT_LABELS = ["L out", "R out"]

MISSING = "—"  # em-dash
DOT = "·"      # empty meter cell
DEG = "°"      # degree sign


def setup_glyphs():
    """ncurses only renders non-ASCII once the locale is set, and some
    locales/terminals still cannot.  Fall back to ASCII rather than paint
    mojibake into a meter."""
    global MISSING, DOT, DEG
    try:
        locale.setlocale(locale.LC_ALL, "")
    except locale.Error:
        pass
    enc = (locale.getpreferredencoding(False) or "").lower()
    if "utf" not in enc:
        MISSING, DOT, DEG = "--", ".", "d"

# colour pair ids
C_OK = 1
C_WARN = 2
C_HOT = 3
C_DIM = 4
C_HEAD = 5
C_ALERT = 6


# ---------------------------------------------------------------------------
# packet parsing
# ---------------------------------------------------------------------------

class Telemetry:
    """Latest decoded state.  Any attribute may be None == 'not in the packet'."""

    __slots__ = (
        "ins", "outs", "acc", "gyr", "tof", "exp", "jx", "jy",
        "jsw", "pot", "enc1", "enc2",
    )

    def __init__(self) -> None:
        self.ins = [None] * 10
        self.outs = [None] * 2
        self.acc = [None] * 3
        self.gyr = [None] * 3
        self.tof = None
        self.exp = None
        self.jx = None
        self.jy = None
        self.jsw = None
        self.pot = None
        self.enc1 = None
        self.enc2 = None


def _floats(raw: str, n: int):
    """Parse up to n comma-separated floats; short/garbage entries become None."""
    out = [None] * n
    for i, part in enumerate(raw.split(",")):
        if i >= n:
            break
        part = part.strip()
        if not part:
            continue
        try:
            v = float(part)
        except ValueError:
            continue
        if math.isnan(v) or math.isinf(v):
            continue
        out[i] = v
    return out


def _one_float(raw: str):
    try:
        v = float(raw.strip())
    except (ValueError, AttributeError):
        return None
    if math.isnan(v) or math.isinf(v):
        return None
    return v


def _one_int(raw: str):
    try:
        return int(float(raw.strip()))
    except (ValueError, AttributeError):
        return None


def parse_line(line: str):
    """Decode one HGMON line.  Returns a fresh Telemetry, or None if not ours.

    Absent fields stay None -- that is the whole point.  Never raises.
    """
    line = line.strip()
    if not line:
        return None
    toks = line.split()
    if toks[0] != "HGMON":
        # be forgiving: accept a line with no tag if it still has key:val pairs
        if ":" not in line:
            return None
    else:
        toks = toks[1:]

    t = Telemetry()
    got_any = False
    for tok in toks:
        if ":" not in tok:
            continue
        key, _, val = tok.partition(":")
        key = key.lower()
        try:
            if key == "in":
                t.ins = _floats(val, 10)
            elif key == "out":
                t.outs = _floats(val, 2)
            elif key == "acc":
                t.acc = _floats(val, 3)
            elif key == "gyr":
                t.gyr = _floats(val, 3)
            elif key == "tof":
                t.tof = _one_float(val)
            elif key == "exp":
                t.exp = _one_float(val)
            elif key == "jx":
                t.jx = _one_float(val)
            elif key == "jy":
                t.jy = _one_float(val)
            elif key == "jsw":
                t.jsw = _one_int(val)
            elif key == "pot":
                t.pot = _one_float(val)
            elif key == "enc1":
                t.enc1 = _one_int(val)
            elif key == "enc2":
                t.enc2 = _one_int(val)
            else:
                continue
        except Exception:
            continue
        got_any = True
    return t if got_any else None


# ---------------------------------------------------------------------------
# peak hold
# ---------------------------------------------------------------------------

class PeakHold:
    def __init__(self) -> None:
        self._peaks = {}   # key -> [value, set_time]

    def update(self, key, value, now):
        if value is None:
            return
        entry = self._peaks.get(key)
        if entry is None or value >= entry[0]:
            self._peaks[key] = [value, now]

    def get(self, key, now):
        entry = self._peaks.get(key)
        if entry is None:
            return None
        val, t0 = entry
        age = now - t0
        if age > PEAK_HOLD:
            val = val - (age - PEAK_HOLD) * PEAK_FALL
            if val <= DB_FLOOR:
                return None
        return val

    def reset(self):
        self._peaks.clear()


# ---------------------------------------------------------------------------
# curses helpers
# ---------------------------------------------------------------------------

class Screen:
    def __init__(self, stdscr):
        self.s = stdscr
        self.colour = False
        try:
            if curses.has_colors():
                curses.start_color()
                try:
                    curses.use_default_colors()
                    bg = -1
                except curses.error:
                    bg = curses.COLOR_BLACK
                curses.init_pair(C_OK, curses.COLOR_GREEN, bg)
                curses.init_pair(C_WARN, curses.COLOR_YELLOW, bg)
                curses.init_pair(C_HOT, curses.COLOR_RED, bg)
                curses.init_pair(C_DIM, curses.COLOR_BLUE, bg)
                curses.init_pair(C_HEAD, curses.COLOR_CYAN, bg)
                curses.init_pair(C_ALERT, curses.COLOR_RED, bg)
                self.colour = True
        except curses.error:
            self.colour = False
        self.h, self.w = self.s.getmaxyx()
        self.stale = False

    def attr(self, pair=0, bold=False, dim=False, reverse=False):
        a = curses.A_NORMAL
        if self.colour and pair:
            a |= curses.color_pair(pair)
        if bold:
            a |= curses.A_BOLD
        if dim or self.stale:
            a |= curses.A_DIM
            a &= ~curses.A_BOLD
        if reverse:
            a |= curses.A_REVERSE
        return a

    def put(self, y, x, text, attr=curses.A_NORMAL):
        """Bounds-safe addstr.  Silently clips; never raises."""
        if y < 0 or y >= self.h or x >= self.w:
            return
        if x < 0:
            text = text[-x:]
            x = 0
        room = self.w - x
        if room <= 0:
            return
        text = text[:room]
        # writing the very last cell of the screen raises on some terms
        if y == self.h - 1 and x + len(text) >= self.w:
            text = text[: self.w - x - 1]
        if not text:
            return
        try:
            self.s.addstr(y, x, text, attr)
        except curses.error:
            pass


def db_attr(scr: Screen, db):
    if db is None:
        return scr.attr(C_DIM, dim=True)
    if db >= -3.0:
        return scr.attr(C_HOT, bold=True)
    if db >= -12.0:
        return scr.attr(C_WARN)
    return scr.attr(C_OK)


def bar_cells(value, lo, hi, width):
    """How many cells of `width` a value fills.  Clamped."""
    if value is None or width <= 0 or hi <= lo:
        return 0
    frac = (value - lo) / (hi - lo)
    frac = max(0.0, min(1.0, frac))
    return int(round(frac * width))


def draw_db_meter(scr: Screen, y, x, width, label, db, peak):
    """One audio meter row: label, numeric dB, bar with peak marker."""
    lab_w = 10
    num_w = 7
    scr.put(y, x, label[:lab_w].ljust(lab_w), scr.attr(0))
    nx = x + lab_w + 1

    if db is None:
        scr.put(y, nx, MISSING.rjust(num_w), scr.attr(C_DIM, dim=True))
    else:
        txt = ("%.1f" % db) if db > DB_FLOOR else "-inf"
        scr.put(y, nx, txt.rjust(num_w), db_attr(scr, db))

    bx = nx + num_w + 1
    bw = scr.w - bx - 2
    if bw < 4:
        return
    scr.put(y, bx, "[", scr.attr(C_DIM, dim=True))
    scr.put(y, bx + bw + 1, "]", scr.attr(C_DIM, dim=True))
    inner = bx + 1

    if db is None:
        scr.put(y, inner, (DOT * bw), scr.attr(C_DIM, dim=True))
        scr.put(y, inner + 1, " no data ", scr.attr(C_DIM, dim=True))
        return

    filled = bar_cells(db, DB_FLOOR, DB_CEIL, bw)
    # draw the fill in segments so the hot end reads red even in mono via '#'
    warn_at = bar_cells(-12.0, DB_FLOOR, DB_CEIL, bw)
    hot_at = bar_cells(-3.0, DB_FLOOR, DB_CEIL, bw)
    for i in range(bw):
        if i < filled:
            if i >= hot_at:
                ch, a = "#", scr.attr(C_HOT, bold=True)
            elif i >= warn_at:
                ch, a = "=", scr.attr(C_WARN)
            else:
                ch, a = "=", scr.attr(C_OK)
        else:
            ch, a = DOT, scr.attr(C_DIM, dim=True)
        scr.put(y, inner + i, ch, a)

    if peak is not None:
        p = bar_cells(peak, DB_FLOOR, DB_CEIL, bw)
        p = max(0, min(bw - 1, p - 1))
        scr.put(y, inner + p, "|", scr.attr(C_HOT, bold=True))


def draw_unit_bar(scr: Screen, y, x, width, value, centre=False):
    """A 0..1 bar.  centre=True draws a centre tick for joystick axes."""
    if width < 3:
        return
    if value is None:
        scr.put(y, x, DOT * width, scr.attr(C_DIM, dim=True))
        return
    mid = width // 2
    pos = bar_cells(value, 0.0, 1.0, width)
    pos = max(0, min(width - 1, pos - 1 if pos > 0 else 0))
    for i in range(width):
        if centre:
            if i == pos:
                ch, a = "O", scr.attr(C_HEAD, bold=True)
            elif i == mid:
                ch, a = "+", scr.attr(C_DIM, dim=True)
            else:
                ch, a = DOT, scr.attr(C_DIM, dim=True)
        else:
            if i < pos:
                ch, a = "=", scr.attr(C_OK)
            else:
                ch, a = DOT, scr.attr(C_DIM, dim=True)
        scr.put(y, x + i, ch, a)


def draw_bipolar_bar(scr: Screen, y, x, width, value, fs):
    """A -fs..+fs bar with a zero tick, for accel/gyro axes."""
    if width < 3:
        return
    mid = width // 2
    if value is None:
        scr.put(y, x, DOT * width, scr.attr(C_DIM, dim=True))
        return
    frac = max(-1.0, min(1.0, value / fs)) if fs else 0.0
    pos = int(round(mid + frac * (mid - 1)))
    pos = max(0, min(width - 1, pos))
    for i in range(width):
        if i == pos:
            ch, a = "O", scr.attr(C_HEAD, bold=True)
        elif i == mid:
            ch, a = "|", scr.attr(C_DIM, dim=True)
        elif (i > mid and i < pos) or (i < mid and i > pos):
            ch, a = "-", scr.attr(C_OK)
        else:
            ch, a = DOT, scr.attr(C_DIM, dim=True)
        scr.put(y, x + i, ch, a)


def fmt(value, spec="%.3f", width=7):
    if value is None:
        return MISSING.rjust(width)
    try:
        return (spec % value).rjust(width)
    except (TypeError, ValueError):
        return MISSING.rjust(width)


# ---------------------------------------------------------------------------
# main render
# ---------------------------------------------------------------------------

def render(scr: Screen, tel: Telemetry, peaks: PeakHold, now,
           last_rx, pps, total, bad, bind_desc, ever):
    s = scr.s
    s.erase()
    scr.h, scr.w = s.getmaxyx()
    age = (now - last_rx) if last_rx else None
    scr.stale = (not ever) or (age is not None and age > STALE_AFTER)

    w = scr.w
    y = 0
    # The full layout wants 27 rows.  On a shorter terminal (the 80x24
    # minimum) drop the blank separator rows so the joystick, button and
    # encoders still make it onto the screen.
    gap = 1 if scr.h >= 27 else 0

    # ---- header -----------------------------------------------------------
    title = " hyperGuitar monitor "
    right = "%s  %5.1f pkt/s  rx %d" % (bind_desc, pps, total)
    if bad:
        right += "  bad %d" % bad
    scr.put(y, 0, title, scr.attr(C_HEAD, bold=True) if not scr.stale
            else scr.attr(C_HEAD, dim=True))
    scr.put(y, max(len(title) + 1, w - len(right) - 1), right,
            scr.attr(C_DIM, dim=True))
    y += 1

    # ---- staleness banner -------------------------------------------------
    if scr.stale:
        if not ever:
            msg = ("  WAITING FOR DATA -- no packet has ever arrived "
                   "on %s  " % bind_desc)
        else:
            msg = "  *** NO DATA FOR %.1fs -- LINK DOWN ***  " % age
        pad = msg.center(w - 1)
        a = curses.A_REVERSE | curses.A_BOLD
        if scr.colour:
            a |= curses.color_pair(C_ALERT)
        try:
            s.addstr(y, 0, pad[: w - 1], a)
        except curses.error:
            pass
    else:
        scr.put(y, 0, " link up  (last packet %.0f ms ago)" % (age * 1000.0),
                scr.attr(C_OK))
    y += 1 + gap

    # ---- audio inputs -----------------------------------------------------
    scr.put(y, 0, "AUDIO IN  (dB, | = peak hold)", scr.attr(C_HEAD, bold=True))
    y += 1
    for i in range(10):
        if y >= scr.h - 1:
            break
        db = tel.ins[i]
        draw_db_meter(scr, y, 0, w, IN_LABELS[i], db, peaks.get(("in", i), now))
        y += 1

    y += gap
    if y < scr.h - 1:
        scr.put(y, 0, "AUDIO OUT", scr.attr(C_HEAD, bold=True))
        y += 1
    for i in range(2):
        if y >= scr.h - 1:
            break
        draw_db_meter(scr, y, 0, w, OUT_LABELS[i], tel.outs[i],
                      peaks.get(("out", i), now))
        y += 1

    y += gap

    # ---- sensors ----------------------------------------------------------
    if y >= scr.h - 1:
        return
    scr.put(y, 0, "SENSORS", scr.attr(C_HEAD, bold=True))
    y += 1

    barw = max(6, min(18, w - 58))
    col2 = min(w - 1, 40)

    def row(text, attr=None):
        nonlocal y
        if y < scr.h:
            scr.put(y, 0, text, attr if attr is not None else scr.attr(0))
        y += 1

    # accel
    if y < scr.h:
        scr.put(y, 0, "ACC  g ", scr.attr(0))
        x = 7
        for ax, v in zip("xyz", tel.acc):
            scr.put(y, x, "%s%s " % (ax, fmt(v, "%+.3f", 7)),
                    scr.attr(C_DIM, dim=True) if v is None else scr.attr(0))
            x += 10
        if x + barw < w:
            draw_bipolar_bar(scr, y, x, barw, tel.acc[2], ACC_FS)
            scr.put(y, x + barw + 1, "z", scr.attr(C_DIM, dim=True))
    y += 1

    # gyro
    if y < scr.h:
        scr.put(y, 0, "GYR  " + DEG + "/s", scr.attr(0))
        x = 9
        for ax, v in zip("xyz", tel.gyr):
            scr.put(y, x, "%s%s " % (ax, fmt(v, "%+.2f", 8)),
                    scr.attr(C_DIM, dim=True) if v is None else scr.attr(0))
            x += 11
    y += 1

    # ToF -- three distinct states: missing / no reading / value
    if y < scr.h:
        scr.put(y, 0, "ToF  ", scr.attr(0))
        if tel.tof is None:
            scr.put(y, 5, "%s  (sensor absent)" % MISSING,
                    scr.attr(C_DIM, dim=True))
        elif tel.tof < 0:
            scr.put(y, 5, "no reading", scr.attr(C_WARN, bold=True))
        else:
            scr.put(y, 5, "%6.0f mm" % tel.tof, scr.attr(C_OK, bold=True))
            if 20 + barw < w:
                draw_unit_bar(scr, y, 20, barw,
                              max(0.0, min(1.0, tel.tof / TOF_FS)))
    y += 1

    # expression + pot on one line when there is room
    if y < scr.h:
        scr.put(y, 0, "EXP  " + fmt(tel.exp),
                scr.attr(C_DIM, dim=True) if tel.exp is None else scr.attr(0))
        draw_unit_bar(scr, y, 14, barw, tel.exp)
        if col2 + 24 < w:
            scr.put(y, col2, "POT  " + fmt(tel.pot),
                    scr.attr(C_DIM, dim=True) if tel.pot is None else scr.attr(0))
            draw_unit_bar(scr, y, col2 + 14, barw, tel.pot)
            y += 1
        else:
            y += 1
            if y < scr.h:
                scr.put(y, 0, "POT  " + fmt(tel.pot),
                        scr.attr(C_DIM, dim=True) if tel.pot is None
                        else scr.attr(0))
                draw_unit_bar(scr, y, 14, barw, tel.pot)
            y += 1

    # joystick axes
    if y < scr.h:
        scr.put(y, 0, "JOY x" + fmt(tel.jx),
                scr.attr(C_DIM, dim=True) if tel.jx is None else scr.attr(0))
        draw_unit_bar(scr, y, 14, barw, tel.jx, centre=True)
        if col2 + 24 < w:
            scr.put(y, col2, "   y" + fmt(tel.jy),
                    scr.attr(C_DIM, dim=True) if tel.jy is None else scr.attr(0))
            draw_unit_bar(scr, y, col2 + 14, barw, tel.jy, centre=True)
            y += 1
        else:
            y += 1
            if y < scr.h:
                scr.put(y, 0, "JOY y" + fmt(tel.jy),
                        scr.attr(C_DIM, dim=True) if tel.jy is None
                        else scr.attr(0))
                draw_unit_bar(scr, y, 14, barw, tel.jy, centre=True)
            y += 1

    # joystick button + encoders
    if y < scr.h:
        scr.put(y, 0, "JSW  ", scr.attr(0))
        if tel.jsw is None:
            scr.put(y, 5, "%s      " % MISSING, scr.attr(C_DIM, dim=True))
        elif tel.jsw:
            scr.put(y, 5, "[ DOWN ]", scr.attr(C_HOT, bold=True, reverse=True))
        else:
            scr.put(y, 5, "  up    ", scr.attr(C_DIM, dim=True))
        e1 = MISSING if tel.enc1 is None else "%+d" % tel.enc1
        e2 = MISSING if tel.enc2 is None else "%+d" % tel.enc2
        scr.put(y, 18, "ENC1 %8s" % e1,
                scr.attr(C_DIM, dim=True) if tel.enc1 is None else scr.attr(0))
        scr.put(y, 38, "ENC2 %8s" % e2,
                scr.attr(C_DIM, dim=True) if tel.enc2 is None else scr.attr(0))
    y += 1

    # ---- footer -----------------------------------------------------------
    if scr.h - 1 >= y:
        scr.put(scr.h - 1, 0,
                " q quit   p reset peaks   c clear counters   "
                + MISSING + " = field absent from packet ",
                scr.attr(C_DIM, dim=True))
    s.noutrefresh()
    curses.doupdate()


# ---------------------------------------------------------------------------
# socket + loop
# ---------------------------------------------------------------------------

def make_socket(host, port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1 << 20)
    except OSError:
        pass
    sock.bind((host, port))
    sock.setblocking(False)
    return sock


def run(stdscr, sock, bind_desc):
    curses.curs_set(0)
    stdscr.nodelay(True)
    try:
        curses.cbreak()
    except curses.error:
        pass

    scr = Screen(stdscr)
    tel = Telemetry()
    peaks = PeakHold()
    arrivals = deque()
    last_rx = None
    total = 0
    bad = 0
    ever = False
    partial = b""
    next_draw = 0.0

    while True:
        now = time.monotonic()

        # drain the socket completely each pass so we never fall behind
        while True:
            try:
                data, _addr = sock.recvfrom(65535)
            except BlockingIOError:
                break
            except OSError as e:
                if e.errno in (errno.EAGAIN, errno.EWOULDBLOCK):
                    break
                break
            if not data:
                break
            buf = partial + data
            partial = b""
            # a datagram usually holds exactly one line, but tolerate
            # several lines or a split line
            if not buf.endswith(b"\n"):
                lines = buf.split(b"\n")
                partial = lines.pop()
                if len(partial) > 4096:      # runaway garbage, drop it
                    partial = b""
            else:
                lines = buf.split(b"\n")
            for raw in lines:
                if not raw.strip():
                    continue
                try:
                    text = raw.decode("ascii", "replace")
                except Exception:
                    bad += 1
                    continue
                parsed = parse_line(text)
                if parsed is None:
                    bad += 1
                    continue
                tel = parsed
                total += 1
                ever = True
                last_rx = now
                arrivals.append(now)
                for i in range(10):
                    peaks.update(("in", i), tel.ins[i], now)
                for i in range(2):
                    peaks.update(("out", i), tel.outs[i], now)

        while arrivals and now - arrivals[0] > 2.0:
            arrivals.popleft()
        span = 2.0 if (arrivals and now - arrivals[0] > 1.5) else max(
            0.5, (now - arrivals[0]) if arrivals else 1.0)
        pps = len(arrivals) / span if arrivals else 0.0

        # keys
        try:
            ch = stdscr.getch()
        except curses.error:
            ch = -1
        while ch != -1:
            if ch in (ord("q"), ord("Q"), 27):
                return
            if ch in (ord("p"), ord("P")):
                peaks.reset()
            if ch in (ord("c"), ord("C")):
                total = 0
                bad = 0
                arrivals.clear()
            if ch == curses.KEY_RESIZE:
                stdscr.erase()
            try:
                ch = stdscr.getch()
            except curses.error:
                ch = -1

        if now >= next_draw:
            next_draw = now + FRAME
            try:
                render(scr, tel, peaks, now, last_rx, pps, total, bad,
                       bind_desc, ever)
            except curses.error:
                pass

        time.sleep(0.008)


def main(argv=None):
    ap = argparse.ArgumentParser(description="hyperGuitar UDP telemetry monitor")
    ap.add_argument("--host", default=DEFAULT_HOST,
                    help="address to bind (default %s = all interfaces, which "
                         "covers both the Bela USB net and localhost)"
                         % DEFAULT_HOST)
    ap.add_argument("--port", type=int, default=DEFAULT_PORT,
                    help="UDP port (default %d)" % DEFAULT_PORT)
    ap.add_argument("--dump", action="store_true",
                    help="no UI: print parsed packets to stdout (debug aid)")
    args = ap.parse_args(argv)

    setup_glyphs()

    try:
        sock = make_socket(args.host, args.port)
    except OSError as e:
        sys.stderr.write("hgmon: cannot bind %s:%d -- %s\n"
                         % (args.host, args.port, e))
        return 2

    bind_desc = "%s:%d" % (args.host, args.port)

    if args.dump:
        sock.setblocking(True)
        try:
            while True:
                data, addr = sock.recvfrom(65535)
                for raw in data.split(b"\n"):
                    if not raw.strip():
                        continue
                    t = parse_line(raw.decode("ascii", "replace"))
                    if t is None:
                        print("BAD  %r" % raw[:120])
                    else:
                        print("OK   from %s in=%s out=%s tof=%s jsw=%s "
                              "enc=%s/%s"
                              % (addr[0], t.ins[:3], t.outs, t.tof, t.jsw,
                                 t.enc1, t.enc2))
        except KeyboardInterrupt:
            return 0
        finally:
            sock.close()

    try:
        curses.wrapper(run, sock, bind_desc)
    except KeyboardInterrupt:
        pass
    finally:
        sock.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
