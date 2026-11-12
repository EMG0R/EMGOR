#!/usr/bin/env python3
# NEPTR Phase 4 UI - Read-only display for guitar effects processor
# Receives OSC from Csound engine at 127.0.0.1:8000
#
# pygame rewrite (2026-06-12, spec docs/specs/2026-06-12-ui-squares-rewrite-design.md):
# Kivy is gone. Every pixel is drawn by hand on an SDL2 surface — no widget
# toolkit, no framework tick. The main loop BLOCKS on the OSC queue, so idle
# CPU is effectively zero; the screen repaints only when incoming OSC changes
# visible state (plus the brief glitch transition and the sleep faces).
#
#   - Menu view: 5 equidistant outlined slots, ON = inverted (white fill, purple text)
#   - Effect view: vertical Max/MSP-style fill sliders, one color per param
#   - Slice-displacement glitch transition on view changes (3 frames, ~200 ms)
#   - protoFACES sleep screen, same logic as before, timings 25% faster
#
# 2026-08-09 touch-native upgrade:
#   - Full touch support: every interactive element (companion tabs, patch /
#     power / audio tiles, menu slots) is tappable. Touch coords are mapped
#     through ~/demiurge/touch-cal.conf (same file the demiurge-touch daemon
#     and demiurge-touch-test consume) + the NEPTR_ROT transform — see
#     TouchMapper. Keyboard/MIDI paths unchanged (touch is additive).
#   - New WAVE screen (companion tab 5): finger-drawn single-cycle wavetable
#     with a log-spaced 64-harmonic bar editor (draw either view, both stay
#     consistent — DSP in wavetable.py). On edit-commit the table is sent
#     over OSC: /neptr/wavetable (blob, 2048 big-endian float32) and
#     /neptr/wavetable/harmonics (64 mags + 64 phases). Target from
#     live.conf wavetable_osc_host / wavetable_osc_port (127.0.0.1:8001).
#   - Visual pass: cached background gradient, rounded slots, glow accents,
#     redesigned touch tab bar with icons, slide transition between
#     companion tabs. All gradients/glows are pre-rendered once — per-frame
#     cost stays O(previous).
#
# 2026-08-09 engine link (see DEMIURGE_OS demiurge/docs/engine-ui.md):
#   - Touch now CONTROLS the engine, not just navigates: slot taps toggle
#     effects, looper rows carry the full press/release transport gesture,
#     and param sliders are finger-draggable. All of it goes out as the
#     EXACT ch1 CCs the Teensy encoders emit (companion_input.EngineOut on
#     the shared Midi Through pool) — no new protocol; the engine cannot
#     tell touch from hardware. Visual state stays engine-authoritative:
#     the existing OSC broadcasts are the only thing that flips a slot.
#   - Late-join sync: on startup the UI sends ch16 CC102 (sync request);
#     the engine rebroadcasts its full OSC state.
#   - Faster/subtler touch: pure-navigation taps (tabs, back, companion
#     open/close, ON-slot->params) fire on finger-DOWN; the press glow is
#     shorter and smaller; horizontal swipes switch companion tabs (axis-
#     dominant, so list scrolls and the WAVE canvas are never stolen).
#   - WAVE screen: UNDO/CLEAR buttons removed — the canvas and harmonic
#     strip grew instead. Dragging BELOW a surface writes 0 for every X
#     swept (bottom-edge sweep IS the clear), ABOVE writes max; X keeps
#     tracking the finger the whole stroke, in and out of the rect.
#
# Env:
#   NEPTR_ROT=0|90|180|270   panel rotation (default 270, same as before)
#   NEPTR_WINDOWED=1         windowed dev mode instead of fullscreen
#   NEPTR_SCALE=0.45         window scale factor in dev mode
#   NEPTR_WT_OSC_HOST/PORT   override the wavetable OSC target (else live.conf)
#   NEPTR_MIDI_OUT           override the engine-link MIDI output port

import os
os.chdir(os.path.dirname(os.path.abspath(__file__)))
os.environ.setdefault("PYGAME_HIDE_SUPPORT_PROMPT", "1")

import logging
import math
import queue
import random
import re
import signal
import struct
import subprocess
import sys
import threading
import time

import pygame
from pythonosc.dispatcher import Dispatcher
from pythonosc.osc_server import ThreadingOSCUDPServer
from pythonosc.udp_client import SimpleUDPClient

import touch_evdev
import wavetable

# Companion mode (joystick-hold dashboard / patch / power menu). These are
# local modules; see docs/companion.md. They have no hard runtime deps beyond
# mido (lazy-imported in companion_input), so the normal UI still starts if
# MIDI is unavailable — the companion is then simply unreachable.
import companion as companion_mod
import companion_input
import pi_status
import actions
import ui_state
import sm_page

logging.basicConfig(level=logging.INFO)
log = logging.getLogger("neptr-ui")

# ---------------------------------------------------------------------------
# Canvas geometry (logical, portrait; rotation applied at present time)
# ---------------------------------------------------------------------------

W, H = 720, 1280
# Canvas (W x H) is what present() scales into the area between the level
# meters / under the DSP bar, so the canvas centre IS the usable-area centre.
# Faces are all 720x720 images: ONE position for every face, the image centred
# in the usable area.
FACE_X = (W - 720) // 2
FACE_Y = (H - 720) // 2
TITLE_CENTER_Y = 44
DIVIDER_Y = 130
CONTENT_TOP = 148
CONTENT_BOTTOM = 1254
PAD_X = 20
SLOT_SPACING = 18
SLOT_INSET = 8

# ---------------------------------------------------------------------------
# Palette (0-255)
# ---------------------------------------------------------------------------

BG = (27, 1, 30)                  # background — EXACTLY the pixel colour baked into protoFACES/FACE*.png, so the sleep faces sit on it with no seam
FG = (255, 255, 255)
OFF_OUTLINE = (113, 64, 117)      # H=296°, S=43%, V=46%
OFF_TEXT = (155, 114, 158)        # H=296°, S=27%, V=61%
EMPTY_OUTLINE = (55, 25, 62)      # H=296°, S=60%, V=24%
TRACK_ACTIVE = (220, 186, 222)    # H=296°, S=16%, V=86% — near-white with purple warmth
TRACK_DIM = (92, 58, 94)          # H=296°, S=37%, V=37%

# Slider fills — analogous hues 260°-308°, all at high saturation and value
SLIDER_COLORS = [
    (202, 42, 235),    # H=290° violet-magenta (closest to BG hue)
    (138, 44, 245),    # H=272° electric violet
    (245, 44, 218),    # H=308° vivid magenta
    (145, 89, 255),    # H=260° bright lavender
]
# ---------------------------------------------------------------------------
# Touch + visual-upgrade constants
# ---------------------------------------------------------------------------

BG_TOP = BG                       # background 'gradient' endpoints: flat BG, identical to the face
BG_BOT = BG                       # image colour (a real gradient showed a seam around the faces)
ACCENT = (202, 42, 235)           # the violet-magenta glow accent
ACCENT_DIM = (86, 24, 98)
ALERT = (255, 84, 120)            # the ONE colour that leaves the purple
                                  # family, reserved for "this is silence":
                                  # an unconnected engine port. It has to
                                  # break the palette or it stops reading as
                                  # an alarm at arm's length.

# DSP load bar — deliberately shifted toward blue/cyan rather than the
# violet ACCENT, so it reads as its own instrument (processing headroom)
# instead of looking like a third level meter. Still inside the dark
# purple world (hue pulled toward blue, not a clashing primary blue), and
# still carries a real severity gradient: calm at low load, hot near the
# quantum deadline, same amber/red stops the level meters already use so
# "hot" means the same thing everywhere in this UI.
DSP_BLUE = (70, 150, 235)         # calm blue-cyan, low DSP load
DSP_AMBER = (235, 176, 60)        # == level-meter hot colour (line ~5945)
DSP_RED = (255, 40, 40)           # == level-meter clip colour; xrun flash
DSP_BAR_TRACK = (22, 12, 30)      # empty-track background, slightly
                                  # bluer than BG so the bar reads as a
                                  # distinct strip even at zero load
RADIUS = 16                       # rounded-rect radius for slots/tiles
TAB_BAR_H = 100                   # touch tab bar height (companion)
TAP_SLOP = 18                     # px of movement that still counts as a tap
PRESS_FX_SECS = 0.15              # glow-pulse feedback duration (quick flash)
PRESSED_FADE_SECS = 0.14          # cancelled pressed-highlight fade-out
SLIDE_SECS = 0.18                 # companion tab slide transition
SCROLL_STEP = 84                  # drag px per row of list scroll
SWIPE_PX = 110                    # horizontal travel that commits a swipe
AXIS_COMMIT_PX = 40               # travel before the scroll-vs-swipe axis is
                                  # decided (resistive contact jitter must not
                                  # lock the axis at the 18px tap slop)
# Gesture debug: NEPTR_TOUCH_DEBUG=1 logs every classification (down/tap/
# swipe/axis, with measured deltas and the delivering event stream) at INFO.
TOUCH_DEBUG = os.environ.get("NEPTR_TOUCH_DEBUG", "0") == "1"
WAVE_SEND_DEBOUNCE = 0.25         # s after finger-lift before the OSC send
TRAIL_SECS = 0.30                 # WAVE stroke shimmer-trail lifetime
WAVE_VIEW = 'wave'                # current_view value for the WAVE destination
PARAM_TICK = 1.0 / 30.0           # slider->engine burst cadence (30 Hz)
# Max encoder detents per tick. HARD CEILING, learned the hard way
# (2026-08-09 22:36 incident): csound's midiin drains ONE event per k-cycle
# (~375/s at ksmps 128); a sustained 10-detent burst (300 msg/s) plus the
# touch daemon's ch16 stream and clock overran the rtmidi buffer and STALLED
# the engine's MIDI input until restart. 4/tick = 120 msg/s worst case,
# leaving the bus real headroom. Full-range slider travel ~0.4 s.
PARAM_BURST = 4
PARAM_DEADBAND = 0.025            # fraction of a param's span: "close enough"
PARAM_CHASE_LINGER = 1.5          # s after finger-lift the chase may keep going
# Late-join sync (ch16 CC102 -> full OSC snapshot). The old policy — 3 tries,
# 1.5 s apart, "any OSC message counts as synced" — lost both races that
# actually happen on the device:
#   1. COLD BOOT. neptr-ui starts in seconds; demiurge/csound needs far longer
#      (pipewire, interface probe, csound compile, state_persist restore). All
#      3 requests were spent before the engine could hear MIDI at all, and the
#      UI then sat on defaults forever while the engine ran restored DSP state
#      — the 2026-08-10 "on reboot it saved dsp setting but ui didn't reflect
#      them" report.
#   2. ANY unrelated broadcast (clock /cb_bpm, a looper position) set
#      osc_seen and cancelled the retries before a snapshot ever arrived.
# Now: keep asking, with a backing-off interval, until a real SNAPSHOT lands
# (SYNC_MIN_ADDRS distinct addresses). Cost per attempt is two MIDI bytes.
SYNC_RETRY_SECS = 1.5             # first re-send delay
SYNC_RETRY_BACKOFF = 1.6          # multiplied per attempt ...
SYNC_RETRY_MAX = 15.0             # ... capped here (engine may boot minutes late)
SYNC_RETRY_SECONDS_TOTAL = 900.0  # give up after this long without a snapshot
# Distinct OSC addresses that make a "full snapshot". The CC102 refresh burst
# carries dozens (every effect state + every param); ordinary change traffic
# carries one or two. 8 separates them with a wide margin.
SYNC_MIN_ADDRS = 8
SYNC_QUIET_SECS = 2.0             # catch-up echoes must not auto-navigate
# An ENGINE restart (patch switch, interface switch, `redem`, a crash) leaves
# the UI holding the previous engine's state with no announcement. The one
# signal it always produces is a long gap in OSC traffic followed by traffic
# again, so a first message after this much silence re-arms the sync. The
# request is issued on the NEXT loop pass, not inline, so the message that
# triggered it is handled normally (a real toggle still opens its param view)
# and only the answering burst falls inside the quiet window.
RESYNC_AFTER_SILENCE = 30.0
# Expression-pedal strip: an unlabeled left-to-right slider pinned to the
# bottom of every MENU view — manual expr control. Sends the ABSOLUTE ch1
# CC29 the physical pedal sends (engine routing incl. the sticky
# gkLastToggle29 claim is identical). Nothing is sent until first touched.
EXPR_H = 92                       # strip height reserved under the slots
EXPR_CC = 29

# Wavetable OSC target: live.conf keys wavetable_osc_host / wavetable_osc_port
# (documented in DEMIURGE_OS demiurge/docs/config-reference.md), env override
# NEPTR_WT_OSC_HOST / NEPTR_WT_OSC_PORT, defaults 127.0.0.1:8001.


def _read_live_conf(keys):
    """First readable live.conf wins: the deployed ~/demiurge/live.conf on the
    Pi, the repo-local copy on dev machines. key=value lines, # comments."""
    paths = [os.path.expanduser("~/demiurge/live.conf"),
             os.path.join(os.path.dirname(os.path.abspath(__file__)),
                          "live.conf")]
    for p in paths:
        try:
            with open(p) as f:
                text = f.read()
        except OSError:
            continue
        out = {}
        for line in text.splitlines():
            line = line.split("#", 1)[0].strip()
            if "=" in line:
                k, v = line.split("=", 1)
                if k.strip() in keys:
                    out[k.strip()] = v.strip()
        return out
    return {}


def wavetable_osc_target():
    conf = _read_live_conf({"wavetable_osc_host", "wavetable_osc_port"})
    host = os.environ.get("NEPTR_WT_OSC_HOST",
                          conf.get("wavetable_osc_host", "127.0.0.1"))
    try:
        port = int(os.environ.get("NEPTR_WT_OSC_PORT",
                                  conf.get("wavetable_osc_port", "8001")))
    except ValueError:
        port = 8001
    return host, port


# ---------------------------------------------------------------------------
# Touch calibration + rotation mapping
#
# SDL's evdev backend normalizes FINGER coords by the KERNEL-advertised axis
# range and knows nothing about ~/demiurge/touch-cal.conf. The calibrator
# stages a udev hwdb EVDEV_ABS entry when it can (no swap/inversion), which
# rewrites that advertised range — so the correct, double-apply-proof recipe
# (same as demiurge-touch-test's mirror mode) is: reconstruct RAW panel
# coords from the normalized event using the ranges the kernel advertises
# RIGHT NOW, then apply the cal file. If the hwdb layer is active the two
# cancel exactly; if it isn't (swap/inversion, or hwdb staging failed), the
# cal file does the work. No cal file -> identity. On Mac/dev: identity.
# ---------------------------------------------------------------------------

class TouchCal:
    """The affine raw->screen mapping from ~/demiurge/touch-cal.conf
    (written by demiurge-touch-test; format in config-reference.md)."""

    def __init__(self, swap_xy, rx0, rx1, ry0, ry1):
        self.swap_xy = bool(swap_xy)
        self.rx0, self.rx1 = float(rx0), float(rx1)
        self.ry0, self.ry1 = float(ry0), float(ry1)

    def apply_norm(self, rawx, rawy):
        if self.swap_xy:
            rawx, rawy = rawy, rawx
        nx = (rawx - self.rx0) / (self.rx1 - self.rx0)
        ny = (rawy - self.ry0) / (self.ry1 - self.ry0)
        return max(0.0, min(1.0, nx)), max(0.0, min(1.0, ny))

    @classmethod
    def load(cls):
        path = os.path.expanduser("~/demiurge/touch-cal.conf")
        try:
            with open(path) as f:
                text = f.read()
        except OSError:
            return None
        kv = {}
        for line in text.splitlines():
            m = re.match(r"^\s*([a-z_]+)\s*=\s*([^#]*)", line)
            if m:
                kv[m.group(1)] = m.group(2).strip()
        try:
            return cls(int(kv.get("swap_xy", 0)),
                       float(kv["raw_x_min"]), float(kv["raw_x_max"]),
                       float(kv["raw_y_min"]), float(kv["raw_y_max"]))
        except (KeyError, ValueError, ZeroDivisionError):
            return None


def _touch_kernel_ranges():
    """(x_range, y_range) the kernel advertises for the touch panel — what
    SDL normalizes finger coords by. Linux only; (0,4095) fallback (the
    ADS7846 default)."""
    default = ((0, 4095), (0, 4095))
    if not sys.platform.startswith("linux"):
        return default
    import fcntl
    try:
        with open("/proc/bus/input/devices") as f:
            text = f.read()
    except OSError:
        return default
    dev = None
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
                for w_ in line.split("=", 1)[1].split():
                    absbits = (absbits << 64) | int(w_, 16)
        m = re.search(r"(event\d+)", handlers)
        if m and (ev & (1 << 3)) and (absbits & 1) \
                and re.search(r"touch", name, re.I):
            dev = "/dev/input/" + m.group(1)
            break
    if not dev:
        return default
    ranges = []
    try:
        with open(dev, "rb", buffering=0) as f:
            for code in (0x00, 0x01):               # ABS_X, ABS_Y
                buf = bytearray(24)
                req = (2 << 30) | (24 << 16) | (0x45 << 8) | (0x40 + code)
                fcntl.ioctl(f, req, buf)
                _v, mn, mx, _f, _fl, _r = struct.unpack("iiiiii", buf)
                ranges.append((mn, mx) if mx > mn else (0, 4095))
    except OSError:
        return default
    return ranges[0], ranges[1]


class CpuLoad:
    """Whole-machine CPU busy fraction (0..1) from /proc/stat deltas.
    sample() is two integer reads of one tiny file; the caller invokes it at
    ~1 Hz, so there is no idle cost. Returns None where /proc/stat is absent
    (Mac dev) or on the first call (no delta yet)."""

    def __init__(self):
        self._prev = None

    def sample(self):
        try:
            with open("/proc/stat") as fh:
                f = fh.readline().split()
            vals = [int(x) for x in f[1:9]]
        except (OSError, ValueError, IndexError):
            return None
        idle = vals[3] + vals[4]               # idle + iowait
        total = sum(vals)
        prev, self._prev = self._prev, (idle, total)
        if prev is None or total <= prev[1]:
            return None
        return max(0.0, min(1.0, 1.0 - (idle - prev[0]) / (total - prev[1])))


class TouchMapper:
    """SDL touch/mouse event -> logical canvas (W x H) coordinates:
    cal-file mapping (finger events) + inverse NEPTR_ROT + inverse scale."""

    def __init__(self, rot, screen_size, inset_x=0, inset_y=0):
        self.rot = rot
        self.sw, self.sh = screen_size
        # The canvas no longer fills the screen: the master level strips take
        # METER_FRAC of the width, split between the two edges, and the canvas
        # is squeezed into what is left. Every screen<->canvas conversion has
        # to account for that or touch lands offset by the inset.
        self.inset_x = inset_x
        self.usable_w = max(1, self.sw - 2 * inset_x)
        # Same for the top band reserved for the compute meter.
        self.inset_y = inset_y
        self.usable_h = max(1, self.sh - inset_y)
        self.cal = TouchCal.load()
        self.kx, self.ky = _touch_kernel_ranges() if self.cal \
            else ((0, 4095), (0, 4095))
        log.info("touch: cal %s, kernel ranges x=%s y=%s, rot=%d",
                 "loaded" if self.cal else "none (identity)",
                 self.kx, self.ky, rot)

    def finger(self, e):
        """FINGERDOWN/MOTION/UP event (normalized e.x/e.y) -> canvas px."""
        nx, ny = e.x, e.y
        if self.cal:
            rawx = self.kx[0] + nx * (self.kx[1] - self.kx[0])
            rawy = self.ky[0] + ny * (self.ky[1] - self.ky[0])
            nx, ny = self.cal.apply_norm(rawx, rawy)
        return self._to_canvas(nx * self.sw, ny * self.sh)

    def mouse(self, pos):
        """Mouse event (screen px — SDL/hwdb already scaled) -> canvas px."""
        return self._to_canvas(pos[0], pos[1])

    def raw(self, rx, ry, ranges):
        """RAW evdev panel coords -> canvas px (the primary path on-device;
        SDL's touch layer is bypassed entirely — see touch_evdev.py). The
        cal file applies DIRECTLY on raw values, no SDL-normalized
        reconstruction; without a cal file, normalize by the device's own
        advertised ranges."""
        if self.cal:
            nx, ny = self.cal.apply_norm(rx, ry)
        else:
            xr = ranges.get(touch_evdev.ABS_X, (0, 4095))
            yr = ranges.get(touch_evdev.ABS_Y, (0, 4095))
            nx = (rx - xr[0]) / max(1, xr[1] - xr[0])
            ny = (ry - yr[0]) / max(1, yr[1] - yr[0])
            nx = max(0.0, min(1.0, nx))
            ny = max(0.0, min(1.0, ny))
        return self._to_canvas(nx * self.sw, ny * self.sh)

    def canvas_to_screen(self, x, y):
        """Forward transform (for drawing overlays on the real screen)."""
        if self.rot == 0:
            u, v, rw, rh = x, y, W, H
        elif self.rot == 90:
            u, v, rw, rh = y, W - 1 - x, H, W
        elif self.rot == 180:
            u, v, rw, rh = W - 1 - x, H - 1 - y, W, H
        else:                                        # 270
            u, v, rw, rh = H - 1 - y, x, H, W
        return (u * self.usable_w / rw + self.inset_x,
                v * self.usable_h / rh + self.inset_y)

    def _to_canvas(self, sx, sy):
        rw, rh = (W, H) if self.rot in (0, 180) else (H, W)
        # un-inset first: screen x lives in [inset, sw-inset]
        sx = (sx - self.inset_x) * self.sw / self.usable_w
        sy = (sy - self.inset_y) * self.sh / self.usable_h
        u = sx * rw / self.sw
        v = sy * rh / self.sh
        if self.rot == 0:
            x, y = u, v
        elif self.rot == 90:
            x, y = W - 1 - v, u
        elif self.rot == 180:
            x, y = W - 1 - u, H - 1 - v
        else:                                        # 270
            x, y = v, H - 1 - u
        return (max(0, min(W - 1, int(x))), max(0, min(H - 1, int(y))))


# Big-pixel block dissolve between views: the new screen flips in as chunky
# squares in random order. BLOCK_TRANS_FRAMES reveal batches, one batch per
# BLOCK_TRANS_DT seconds.
# ---- MASTER LEVEL STRIPS ----------------------------------------------------
# Total fraction of screen WIDTH given to the two always-on level strips,
# split evenly between the left and right edges. The canvas is squeezed into
# the remainder, so nothing is covered up - the UI just gets narrower.
METER_FRAC = 0.15
METER_FLOOR_DB = -60.0     # bottom of the strip
METER_HOT_DB = -6.0        # amber above this
METER_CLIP_DB = -1.0       # red above this

BLOCK_SIZE = 90
BLOCK_TRANS_FRAMES = 4
BLOCK_TRANS_DT = 0.055
INVERT_DWELL = 0.35   # seconds a menu slot stays inverted before entering the submenu
SLEEP_AFTER = 10.0    # seconds without OSC before the faces come out

# ---- METRO DOT (TASK 2/3a) --------------------------------------------------
# Fed by /beat_idx · /beat_down · /beat_period (chase_bliss_clock.orc instr
# 100, the AUTHORITATIVE tempo — correct whether NEPTR or PMOR is master).
# The dot runs its OWN local predictor between messages (phase from
# time.time() against the last beat's receipt time) rather than blinking on
# message arrival, which would read as laggy against the UI's capped,
# event-driven repaint. See _beat_phase() + the cheap-path draw in main().
BEAT_LIVE_TIMEOUT = 3.0     # s without a /beat_idx before the dot goes dark
BEAT_FLASH_FRAC = 0.22      # fraction of one beat the flash stays visible
BEAT_DOT_R_BEAT = 5         # px, ordinary beat
BEAT_DOT_R_DOWN = 7         # px, downbeat — distinct size, not just color

# ---- DSP LOAD BAR (TASK 3b) -------------------------------------------------
# Fed by /dsp_load (0..1+, 1.0 = xrun) + /dsp_xrun (cumulative count) from
# the C++ host (src/demiurge-csound-host.cpp) at the same 25 Hz the meters
# already run at — it's the only place that knows the real callback
# wall-time vs. the quantum deadline. Pinned to the physical TOP edge of the
# screen (y=0, post-rotation), full width between the two level-meter
# strips, same thickness as those strips (see meter_w in main() — one
# computed value, reused, so the two cannot drift apart).
DSP_LOAD_STALE_SECS = 3.0   # > the 1 Hz CPU poll (and the 0.75 s sleep-face tick)
DSP_LOAD_AMBER = 0.7        # amber past this fraction of the deadline
DSP_XRUN_FLASH_SECS = 0.25  # brief red flash duration on a genuine xrun

# Asymmetric slew for the displayed value (ui.dsp_load_disp), evaluated at
# render time against real elapsed wall-clock dt (time-based, not a fixed
# per-frame coefficient, so it behaves the same whether the cheap path is
# ticking at 25 Hz or has been idle a while). Attack is fast so a genuine
# spike/xrun is still honestly visible almost immediately; release is slow
# so a normal fluttering load doesn't strobe the bar at 25 Hz.
DSP_LOAD_ATTACK_TAU = 0.035   # s, ~1 cheap-path tick to reach a rising value
DSP_LOAD_RELEASE_TAU = 0.40   # s, gentle fall-off once load drops

# Companion mode
COMPANION_REFRESH_DT = 0.5   # dashboard live-data repaint cadence while open
PI_REFRESH_DT = 1.0          # how often to re-poll vcgencmd/sysfs metrics
IFACE_REFRESH_DT = 3.0       # how often to re-list interfaces on the AUDIO tab
COMPANION_TABS = ["DASH", "PATCH", "AUDIO", "SYS", "NET"]
COMPANION_TITLES = ["DASHBOARD", "PATCHES", "AUDIO", "SYSTEM", "NETWORK"]
POWER_ITEMS = ["LOW", "MEDIUM", "HIGH", "RESTART"]   # SYSTEM screen buttons
SETTINGS_REFRESH_DT = 3.0    # how often to re-read demiurge-audio --list
WIFI_REFRESH_DT = 5.0        # how often to re-read the cached AP list
# The audio graph is re-read often and never cached across a repaint: the
# launcher re-asserts its own wiring on any device or stage event, so a
# snapshot held for more than a moment would show links that no longer exist.
GRAPH_REFRESH_DT = 2.0       # how often to re-read demiurge-graph --list
MSG_DWELL = 3.0              # seconds a transient companion message stays up
# Patch switching is armed-then-confirmed: the window a second tap has to
# land in to actually load the patch (see _companion_activate).
PATCH_CONFIRM_SECS = 4.0

# ---------------------------------------------------------------------------
# Data: 19 menus (rows are listed leftmost slot first, i.e. s4 → s0 — EXCEPT
# PREAMP, which has always been listed s0 → s4; left as-is on purpose).
# 2026-07-13 batch: ONWARD (PITCH s4, H90 → s0), LOSSY + RESONATR (FILTERING
# s0/s1, WAH/AUTOMOOG → s2/s3), IMPULSE (GLITCH s0, glitch quartet shifted
# left), IR VERB, BASS (preamps/dist + OCTAVER + ENVFILTER)
# 2026-07-13 ten-effect batch: COMPRESS fills PREAMP s4; two NEW menus —
# SYNTH (TRACKSYN / PLLFUZZ / TONE EQ, s3+s4 free) and AIR (FEEDBACKER /
# DIFFUSION / CHAIN FB; SHIMMER removed 2026-07-13).
# 2026-07-13 CHASE BLISS: BLOOPER / ONWARD / HABIT MOVED in from
# FILTERING s4 / PITCH s4 / TIME s1 (those slots are now empty).
# 2026-07-26 DELAY/VERB EXPANSION REORG (phase 1 — menu split only, no new
# effects yet; see docs/specs/2026-07-26-delay-verb-expansion.md): the old
# TIME menu (MOOD, GRANULAR, REVERB, DELAY) is dissolved into THREE menus —
# 8 GRANULAR (MOOD, GRANULAR, +3 free), 9 DIGIVERB (REVERB, +4 free —
# GRAINVERB/NEBULA/SHIMMER/FREEZE land here in phase 2), 10 DELAY (DELAY,
# SPECDLY dual-homed, +3 free — TAPE DELAY/MOVE DELAY land here in phase 2).
# 2026-07-26 PHASE 2 (this batch): BINSCATTER takes SPECDLY's old slot in
# SPECTRAL (menu 2) — SPECDLY is no longer dual-homed, it now lives ONLY in
# DELAY (menu 10). DIGIVERB (9) filled: NEBULA/SHIMMER/FREEZE/GRAINVERB
# ahead of REVERB. DELAY (10) filled: TAPE DELAY + MOVE DELAY into the two
# of three free slots (one slot, index3, stays free).
# Everything from BASS onward shifted +2: 11 BASS,
# 12 SYNTH, 13 AIR, 14 LOOPER, 15 CHASE BLISS, 16 GLITCH, 17 NAM, 18 IR VERB
# — menu-prev from PRESETS (wrapping) now lands on IR VERB, then NAM,
# GLITCH, CHASE BLISS, LOOPER. Must match handler_menus.orc /
# handler_looper.orc (LOOPER hardcodes menu 14).
# ---------------------------------------------------------------------------

# 2026-09-01: menu 19 = UTILITY added (global master vol / BPM / RECORD page).
# The engine wraps nav at 21 now (dispatch/nav.orc: "21 menus: 18=IR VERB,
# 19=UTILITY, 20=NORRIS"); UTILITY is a CUSTOM page, not a 5-slot effect grid (it hosts
# three global controls — see _render_utility).
MENU_COUNT = 21   # must match the wrap modulus in dispatch/nav.orc

# CHASE BLISS menu (15, right after the LOOPER): BLOOPER / ONWARD /
# HABIT were MOVED here (not copied) from FILTERING s4 / PITCH s4 / TIME s1 —
# those slots are now "—" on purpose. s3 is MOOD II; s4 is free.
# NOTE: BLOOPER (toggle 56, instr 208, gkBLP_*) MODULATES the LOOPER's loops
# with occasional grain events — it owns no buffer of its own. That is why
# CHASE BLISS sits directly after the LOOPER (menu 14).
# LOSSY + RESONATR deliberately STAY in FILTERING.
#
# Slot layout reminder: this list is drawn LEFT->RIGHT, and the hardware slots
# run RIGHT->LEFT, so index 0 = slot 4 (CC72) ... index 4 = slot 0 (CC74).

MENUS = [
    {"name": "PRESETS",  "effects": [("sld", "OVERDRIVE"), ("hvy", "SOLID DIST"), ("pha", "PHASER"), ("trm", "TREMOLO"), ("amb", "AMBIENT")]},
    {"name": "VOLUME",   "effects": [("tape", "TAPE"), ("atk", "ATTACK"),  ("les", "LESLIE"),  ("rm",  "RINGMOD"), ("vol", "VOLUME")]},
    {"name": "SPECTRAL", "effects": [("bc",  "BCRUSHER"), ("bin", "BIN REDU"), ("bsc", "BINSCATR"), ("fmt", "FORMANT"), ("fms", "FORMS")]},
    {"name": "PITCH",    "effects": [("—", "—"),        ("pf",  "P-FORK"),  ("gps", "GPS"),     ("ps",  "OLD PS"),  ("h90", "H90")]},
    {"name": "FILTERING","effects": [("—", "—"),        ("mog", "AUTOMOOG"),("wah", "WAH"),     ("res", "RESONATR"),("lsy", "LOSSY")]},
    {"name": "MOD",      "effects": [("fla", "FLANGER"), ("trm", "TREMOLO"), ("pha", "PHASER"),  ("cho", "CHORUS"),  ("vib", "VIBRATO")]},
    {"name": "DRIVE",    "effects": [("muf", "BIGMUFF"), ("dhg", "DS-1K"),   ("rmz", "RAT-MZ"), ("mar", "MARSHALL"), ("od", "TUBESCREAM")]},
    {"name": "PREAMP",   "effects": [("pre", "MAIN PRE"), ("bpl", "BASS PRE L"), ("bph", "BASS PRE H"), ("bds", "BASS DIST"), ("cmp", "COMPRESS")]},
    # 2026-07-26: TIME dissolved into three menus (8/9/10) — see the reorg
    # comment block above. MOOD and GRANULAR keep their original slots
    # (index0=slot4/CC72=MOOD, index1=slot3/CC71=GRANULAR), matching
    # handler_menus.orc (both still fire from gkCurrentMenu == 8, unchanged).
    {"name": "GRANULAR", "effects": [("mod", "MOOD"),    ("grn", "GRANULAR"), ("—", "—"),        ("—", "—"),        ("—", "—")]},
    # REVERB moved to the LAST slot (index4/slot0/CC74 — user mandate) so the
    # phase-2 slot order reads NEBULA(s4), SHIMMER(s3), FREEZE(s2),
    # GRAINVERB(s1), REVERB(s0) — see
    # docs/specs/2026-07-26-delay-verb-expansion.md.
    {"name": "DIGIVERB", "effects": [("neb", "NEBULA"),  ("shm", "SHIMMER"), ("frz", "FREEZE"),  ("gvb", "GRAINVRB"), ("vrb", "REVERB")]},
    # DELAY keeps its TIME slot (index4/slot0 = gkCurrentMenu == 10 now).
    # SPECDLY now lives ONLY here (index1/slot3) — the SPECTRAL dual-home is
    # gone as of phase 2 (BINSCATTER took that slot instead). TAPE DELAY
    # lands at index0/slot4, MOVE DELAY at index2/slot2; index3/slot1 stays
    # free ("+1 free" per spec).
    {"name": "DELAY",    "effects": [("tdl", "TAPE DLY"), ("sdl", "SPECDLY"), ("mvd", "POLY DLY"), ("—", "—"),        ("dly", "DELAY")]},
    {"name": "BASS",     "effects": [("env", "ENV FILTER"), ("oct", "OCTAVER"), ("bds", "BASS DIST"), ("bph", "BASS PRE H"), ("bpl", "BASS PRE L")]},
    {"name": "SYNTH",    "effects": [("—", "—"), ("—", "—"), ("teq", "TONE EQ"), ("pll", "PLLFUZZ"), ("tsy", "TRACKSYN")]},
    # SHIMMER (toggle 65 / instr 188) REMOVED 2026-07-13 at the user's call —
    # a shimmer is achievable by routing the existing pitch shifter into reverb.
    {"name": "AIR",      "effects": [("cfb", "CHAIN FB"), ("—", "—"), ("—", "—"), ("dif", "DIFFUSION"), ("fbk", "FEEDBACKER")]},
    # LOOPER is menu 14 — handler_looper.orc HARDCODES this index in its
    # $LP_* gesture macros; do not move it without updating that file.
    {"name": "LOOPER",   "effects": [("lp0", "CH1"), ("lp1", "CH2"), ("lp2", "CH3"), ("lp3", "CH4"), ("lp4", "CH5")]},
    # CHASE BLISS (menu 15) sits RIGHT AFTER the LOOPER on purpose: BLOOPER is a
    # granular modifier OF the looper's loops, so the two menus are neighbours.
    # Drawn left->right = slot4 .. slot0. s4 free,
    # s3 MOOD II (CC71), s2 HABIT (CC76), s1 ONWARD (CC75), s0 BLOOPER (CC74).
    # index 0 = slot4 (free) ... index 4 = slot0. The ORIGINAL MOOD (t17, instrs
    # 401-405) stays exactly where it is in the GRANULAR menu — different effect.
    {"name": "CHASE BLISS", "effects": [("—", "—"), ("md2", "MOOD II"), ("hab", "HABIT"), ("onw", "ONWARD"), ("blp", "BLOOPER")]},
    # GLITCH (menu 16) — 2026-07-15 TOTAL REBUILD (v2), moved to sit right
    # after CHASE BLISS: see stutter.orc INTEGRATION NOTES. Every effect is
    # CLOCKED to the shared CHASE BLISS tick; P2 = per-effect PROBABILITY;
    # CC29 = instant force-trigger. REPEAT/CHOP/RVRS reuse the retired
    # WGUIDE/SQMOD/FSHIFT toggles+buses; BBCUT/IMPULSE ids unchanged.
    {"name": "GLITCH",   "effects": [("rvs", "RVRS"),  ("chp", "CHOP"),   ("rpt", "REPEAT"),  ("bbc", "BBCUT"),   ("imp", "IMPULSE")]},
    {"name": "NAM",      "effects": [("nam0", "AC30"), ("nam1", "DELUXE"), ("nam2", "FENDER"), ("nam3", "TWEED"), ("nam4", "CRUNCH")]},
    {"name": "IR VERB",  "effects": [("ir4", "WOOD RM"), ("ir3", "CHAMBER"), ("ir2", "HALL"), ("ir1", "PLATE"), ("ir0", "SPRING")]},
    # UTILITY (menu 19) — NOT a 5-slot effect grid. It is a custom page hosting
    # three GLOBAL controls: master output volume (ch16 CC103 / OSC /master_out),
    # master BPM (ch16 CC104 / OSC /master_bpm + live /clock_bpm), and a master
    # RECORD toggle (shells out to `demiurge-record`). The five placeholder
    # slots below are only here to satisfy the "every menu has five slots"
    # structural invariant — _render_menu special-cases UTILITY_MENU and draws
    # _render_utility instead, so they are never toggled or shown.
    {"name": "UTILITY",  "effects": [("—", "—"), ("—", "—"), ("—", "—"), ("—", "—"), ("—", "—")]},
    # NORRIS (menu 20, 2026-10-05) — the SoundMagic (Michael Norris) spectral
    # rack, a touch-only CUSTOM page (sm_page.py). Like UTILITY the five
    # placeholders only satisfy the five-slots invariant; the engine has no
    # slot-button/encoder branches for menu 20, so the hardware slots do nothing.
    {"name": "NORRIS",   "effects": [("—", "—"), ("—", "—"), ("—", "—"), ("—", "—"), ("—", "—")]},
]
assert len(MENUS) == MENU_COUNT


def _menu_index(prefix):
    """Menu whose slots carry <prefix>N codes. Derived, never hardcoded — the
    NAM/IR row scans key off these, and menu order changes when menus are added."""
    for i, m in enumerate(MENUS):
        if any(code.startswith(prefix) and code[len(prefix):].isdigit()
               for code, _ in m["effects"]):
            return i
    return -1


NAM_MENU = _menu_index("nam")
IR_MENU = _menu_index("ir")
CB_MENU = next((i for i, m in enumerate(MENUS) if m["name"] == "CHASE BLISS"), -1)
LOOPER_MENU = next((i for i, m in enumerate(MENUS) if m["name"] == "LOOPER"), -1)
UTILITY_MENU = next((i for i, m in enumerate(MENUS) if m["name"] == "UTILITY"), -1)
# NORRIS (menu 20) is a custom page: the SoundMagic rack (sm_page.py)
NORRIS_MENU = next((i for i, m in enumerate(MENUS) if m["name"] == "NORRIS"), -1)
PRESETS_MENU = next((i for i, m in enumerate(MENUS) if m["name"] == "PRESETS"), 0)

# ---------------------------------------------------------------- hyperGuitar
# HYPERGUITAR (menu 21) is a CONDITIONAL page: it exists only while the guitar
# is plugged in over USB and answering its heartbeat. That is why it is NOT in
# MENUS — MENUS is the fixed list every index and test is pinned against, and
# appending to it would make MENU_COUNT lie. The engine gates the same way
# (nav.orc wraps at 21 + gkHgPresent), so the two agree by construction.
#
# hyperGuitar's own NeoTrellis went into the PMOR v2 rebuild, so this page is
# not a convenience — it is the instrument's only control surface.
HG_MENU = MENU_COUNT

# Lane entry labels. These MIRROR gHgLane[] in
#   ______2026NEW/hyperGuitar/bela/hyperGuitar6chPan/hg_lanes.cpp
# and tests/test_hyperguitar_link.py parses BOTH files and asserts they are
# identical, label for label. Duplicating the strings is the lesser evil:
# streaming them over OSC would mean the page renders blank until the guitar
# answers, and csound has no good way to hold a table of strings.
#
# Drawn top-to-bottom = LEFT to RIGHT on the encoders, matching how the engine
# numbers lanes (lane 0 = leftmost = hardware slot 4 = CC72/CC22).
HG_LANES = [
    {"name": "PITCH EXP", "entries": [
        "EXP +/-12", "EXP +/-5", "EXP +/-7",
        "EXP +/-24", "EXP DOWN", "EXP UP"]},
    {"name": "PS STEPS", "entries": [
        "OCT DOWN", "4TH DOWN", "5TH DOWN", "OCT UP", "5TH UP",
        "MIN 3RD DN", "MAJ 3RD DN", "SHIMMER", "DETUNE 12C"]},
    {"name": "FORMANT", "entries": [
        "FORM 0.75", "FORM 0.90", "FORM 1.10", "FORM 1.30",
        "FORM DEEP", "FORM HIGH", "FORM SILK"]},
    {"name": "DYNAMICS", "entries": [
        "WAH SOFT", "WAH VOCAL", "WAH SCREAM",
        "FWAH SOFT", "FWAH MID", "FWAH WIDE",
        "SAG SUBTLE", "SAG BEND", "SAG WOBBLE", "POG SWELL"]},
    {"name": "SYNTH", "entries": [
        "SAW LEAD", "MOOG LEAD", "ACID SQUELCH", "WAVEFOLDER",
        "RINGMOD BELL", "SYNC LEAD", "FM GROWL", "CHIP LEAD",
        "SUB GROWL", "FORMANT VOWEL"]},
]
assert len(HG_LANES) == 5, "five NEPTR encoders, five lanes"

# ROW -> SLOT, REVERSED (2026-09-15).
#
# Every other menu is drawn left->right while the hardware slots run
# right->left, so UI row N is hardware slot 4-N and _ROW_BTN_LOGICAL encodes
# exactly that. hyperGuitar was built on the same assumption and the player
# reported the encoder order came out backwards: the lane they expect on the
# first encoder was on the last one.
#
# So for THIS page the row->slot mapping is the identity instead: lane 0 (PS
# GLIDE) sits on hardware slot 0, lane 4 (SYNTH) on slot 4. The engine's
# menu-21 dispatch branches were moved to match, and
# tests/test_hyperguitar_link.py pins the two together so they cannot drift
# apart again.
HG_ROW_SLOT = [4, 3, 2, 1, 0]

HG_MENU_NAME = "HYPERGUITAR"


# PRESET GESTURE (2026-09-09): on the PRESETS page, a touch on a slot drives the
# IN-ENGINE preset bank (no daemon, no CLI). <5s = RECALL that slot; 5..8s =
# CAPTURE current full state into it; >=8s = DELETE it. The 5 on-screen rows are
# onboard bank slots 13..17 (the DUODECIMUS foot owns 1..12). The op is sent as a
# ch16 CC (106/105/107) with the slot in the value; state_persist.orc does the
# giNeptrState snapshot/restore + persistence. Decision is at finger-UP from
# elapsed time (the main loop blocks on OSC, so no per-frame tick is needed).
try:
    import preset_gesture as _preset_gesture
except Exception:            # UI must still start if the helper is missing
    _preset_gesture = None

# ---------------------------------------------------------------------------
# Engine link: touch -> the SAME ch1 CCs the Teensy emits (see
# DEMIURGE_OS demiurge/docs/engine-ui.md). The engine's handler_remap.orc
# permutes PHYSICAL encoder CCs into LOGICAL ones, so the UI — like the
# Teensy — must emit the PHYSICAL numbers.
#
# Slot buttons: every menu maps UI row -> hardware slot with row0 = slot4
# (the "drawn leftmost = slot4" convention; verified against every
# gkCurrentMenu branch of handler_menus.orc 2026-08-09 — including PREAMP,
# whose header comment claims s0-first but whose dispatch is default-order,
# and NAM/IR, whose rows land row0 = slot4 too). The LOOPER alone lists
# CH1..CH5 = slot0..slot4.
# ---------------------------------------------------------------------------

_ROW_BTN_LOGICAL = [72, 71, 76, 75, 74]      # UI row 0..4 -> logical slot CC
_LOOPER_BTN_LOGICAL = [74, 75, 76, 71, 72]   # looper row = CH index = slot
_PHYS_FOR_BTN = {74: 70, 75: 73, 76: 72, 71: 71, 72: 75}   # logical -> physical
_PHYS_FOR_ROT = {20: 26, 23: 24, 24: 20, 25: 23, 26: 22, 21: 21, 22: 25}


def slot_button_cc(menu_idx, row):
    """PHYSICAL button CC that toggles `row` of `menu_idx` (0-4)."""
    logical = (_LOOPER_BTN_LOGICAL if menu_idx == LOOPER_MENU
               else _ROW_BTN_LOGICAL)[row]
    return _PHYS_FOR_BTN[logical]


def param_rot_cc(code, param):
    """(physical rotation CC, arming toggle id) for a param slider, or None
    when the engine has no encoder path for it. The engine's rotation
    dispatch is gkLastToggle-keyed, so pulses are only valid while the
    engine's last-toggled effect IS this effect — the caller gates on the
    toggle id against the OSC-tracked /last_toggle."""
    if code.startswith("nam") and code[3:].isdigit():
        code = "nam"                       # rows share the two gain params
    hit = PARAM_CC.get((code, param))
    if hit is None:
        return None
    logical, toggle_id = hit
    return _PHYS_FOR_ROT[logical], toggle_id


# ---------------------------------------------------------------------------
# NAM/IR variant-list overlay: row -> physical rotation CC, mirroring
# slot_button_cc()'s row -> physical TOGGLE CC above. Sourced directly from
# the rot_slot{0,1,2,3,4}_cc{24,25,26,21,22}.orc file headers ("SLOT n
# ENCODER ROTATION"), which is the same convention _ROW_BTN_LOGICAL encodes
# for buttons: UI row 0 = hardware slot4 ... UI row 4 = hardware slot0.
# ---------------------------------------------------------------------------
_ROW_ROT_LOGICAL = [22, 21, 26, 25, 24]      # UI row 0..4 -> logical rotate CC


def slot_rotate_cc(menu_idx, row):
    """PHYSICAL rotation CC that cycles the variant in UI row `row` (0-4) of
    `menu_idx`. Only meaningful for NAM_MENU / IR_MENU, whose slot rotations
    are hijacked for variant browsing while those menus are open (see
    rot_slot*_cc*.orc) -- unrelated to param_rot_cc(), which is the normal
    per-effect param-slider CC table."""
    return _PHYS_FOR_ROT[_ROW_ROT_LOGICAL[row]]


def _ui_row_for_state_row(menu_idx, state_row):
    """The MENUS[menu_idx] slot index (0-4) whose effect code is nam<r> or
    ir<r>. NAM's UI rows are drawn in state-row order; IR's are drawn
    reversed (MENUS lists ir4..ir0) -- this looks the mapping up from MENUS
    itself rather than hardcoding the reversal, so a future re-ordering of
    the IR VERB row list can't silently desync the rotate-CC lookup from the
    button-CC lookup slot_button_cc() already uses correctly."""
    prefix = 'nam' if menu_idx == NAM_MENU else 'ir'
    target = f'{prefix}{state_row}'
    for i, (code, _name) in enumerate(MENUS[menu_idx]['effects']):
        if code == target:
            return i
    return state_row


# ---8<--- generated by scripts/gen_param_cc_map.py — do not hand-edit ---
# {(code, param): (LOGICAL rotation CC, arming gkLastToggle id)} transcribed
# mechanically from handler_menus.orc's rotation chains + handler_osc.orc's
# var->address tables. ('trm','depth') is reachable from CC21 and CC22; the
# generator keeps the first.
#
# Addresses whose feedback publishes a LOCAL rescale of the global rather than
# the global itself (`kVibDepNorm = gkVIB_depth * 200`, `kTdlHeadUI =
# gkTdlHeadMode - 1`, `kirmix0 = gkIRRowMix[0]`) are resolved by the
# generator following that alias. Before it did, those (code, param) pairs
# were simply absent here, param_rot_cc() returned None, and the touch slider
# had no encoder CC to drive at all — the dead-fader mode that hid
# ('vib','depth'), ('fla','depth') and ('tdl','head'). The IR row mixes used
# to be hand-appended below for the same reason; they are generated now.
#
# Why NAM's gains and the IR mixes all live on CC20/CC23 rather than on the
# slot rotations every other effect's dry/wet uses: menus 17 (NAM) and 18
# (IR VERB) intercept ALL FIVE slot rotations for the variant browse, so a
# slot-rotation home is unreachable inside those two menus — which is
# precisely the page where this UI draws these sliders. CC20 (P2) and CC23
# (P3) are the two menu-encoder rotations no menu shadows.
PARAM_CC = {
    ('atk', 'mix'): (21, 2),
    ('atk', 'range'): (25, 2),
    ('atk', 'retrig'): (26, 2),
    ('atk', 'sens'): (23, 2),
    ('atk', 'shape'): (24, 2),
    ('atk', 'time'): (20, 2),
    ('bbc', 'flt'): (23, 20),
    ('bbc', 'mix'): (25, 20),
    ('bbc', 'prob'): (20, 20),
    ('bc', 'pedal'): (26, 44),
    ('bds', 'drive'): (26, 42),
    ('bin', 'pedal'): (26, 38),
    ('blp', 'char'): (26, 56),
    ('blp', 'motion'): (20, 56),
    ('blp', 'scat'): (23, 56),
    ('blp', 'size'): (25, 56),
    ('bph', 'drive'): (25, 41),
    ('bpl', 'drive'): (24, 40),
    ('bsc', 'fback'): (23, 67),
    ('bsc', 'mix'): (26, 67),
    ('bsc', 'scatter'): (20, 67),
    ('bsc', 'window'): (24, 67),
    ('cfb', 'ceil'): (24, 60),
    ('cfb', 'gain'): (20, 60),
    ('cfb', 'ret'): (22, 60),
    ('cfb', 'tone'): (23, 60),
    ('cho', 'depth'): (23, 7),
    ('cho', 'mix'): (25, 7),
    ('cho', 'rate'): (20, 7),
    ('chp', 'depth'): (21, 24),
    ('chp', 'prob'): (20, 24),
    ('chp', 'rate'): (23, 24),
    ('chp', 'shape'): (25, 24),
    ('cmp', 'amt'): (22, 57),
    ('cmp', 'blend'): (24, 57),
    ('cmp', 'gate'): (25, 57),
    ('cmp', 'makeup'): (23, 57),
    ('cmp', 'speed'): (20, 57),
    ('dhg', 'blend'): (20, 33),
    ('dhg', 'drive'): (21, 33),
    ('dhg', 'tight'): (23, 33),
    ('dhg', 'tone'): (22, 33),
    ('dif', 'depth'): (20, 64),
    ('dif', 'duck'): (24, 64),
    ('dif', 'rate'): (23, 64),
    ('dif', 'wet'): (25, 64),
    ('dly', 'fback'): (23, 15),
    ('dly', 'mix'): (24, 15),
    ('dly', 'mode'): (25, 15),
    ('dly', 'semi'): (22, 15),
    ('dly', 'time'): (20, 15),
    ('env', 'att'): (21, 55),
    ('env', 'dec'): (24, 55),
    ('env', 'mode'): (23, 55),
    ('env', 'sens'): (20, 55),
    ('env', 'wet'): (22, 55),
    ('fbk', 'bloom'): (23, 63),
    ('fbk', 'harm'): (20, 63),
    ('fbk', 'level'): (24, 63),
    ('fbk', 'vib'): (25, 63),
    ('fla', 'depth'): (22, 9),
    ('fla', 'fback'): (23, 9),
    ('fla', 'rate'): (20, 9),
    ('fms', 'mix'): (22, 28),
    ('fms', 'shift'): (20, 28),
    ('fmt', 'mix'): (21, 5),
    ('fmt', 'shift'): (20, 5),
    ('frz', 'damp'): (20, 72),
    ('frz', 'level'): (26, 72),
    ('frz', 'listen'): (23, 72),
    ('gps', 'dly'): (22, 4),
    ('gps', 'fback'): (23, 4),
    ('gps', 'mix'): (26, 4),
    ('gps', 'semi'): (20, 4),
    ('grn', 'dens'): (20, 18),
    ('grn', 'mix'): (21, 18),
    ('grn', 'pitch'): (22, 18),
    ('grn', 'size'): (23, 18),
    ('gvb', 'decay'): (23, 73),
    ('gvb', 'mix'): (25, 73),
    ('gvb', 'spacing'): (20, 73),
    ('h90', 'mix'): (24, 26),
    ('h90', 'semi'): (20, 26),
    ('hab', 'blend'): (22, 36),
    ('hab', 'mix'): (26, 36),
    ('hab', 'repeats'): (23, 36),
    ('hab', 'size'): (20, 36),
    ('imp', 'dec'): (23, 48),
    ('imp', 'mix'): (24, 48),
    ('imp', 'prob'): (20, 48),
    ('imp', 'tone'): (25, 48),
    ('ir0', 'mix'): (20, 49),
    ('ir1', 'mix'): (20, 50),
    ('ir2', 'mix'): (20, 51),
    ('ir3', 'mix'): (20, 52),
    ('ir4', 'mix'): (20, 53),
    ('les', 'depth'): (26, 10),
    ('les', 'speed'): (20, 10),
    ('lsy', 'bw'): (23, 46),
    ('lsy', 'loss'): (20, 46),
    ('lsy', 'mix'): (24, 46),
    ('lsy', 'pkt'): (22, 46),
    ('mar', 'drive'): (25, 32),
    ('mar', 'pres'): (20, 32),
    ('mar', 'tight'): (23, 32),
    ('mar', 'tone'): (22, 32),
    ('md2', 'clock'): (20, 66),
    ('md2', 'gen'): (24, 66),
    ('md2', 'length'): (23, 66),
    ('md2', 'mix'): (21, 66),
    ('mod', 'clock'): (20, 17),
    ('mod', 'dens'): (23, 17),
    ('mod', 'dly'): (21, 17),
    ('mod', 'mix'): (22, 17),
    ('mog', 'dist'): (25, 14),
    ('mog', 'level'): (22, 14),
    ('mog', 'res'): (20, 14),
    ('mog', 'sens'): (21, 14),
    ('mog', 'type'): (23, 14),
    ('muf', 'drive'): (22, 35),
    ('muf', 'mids'): (20, 35),
    ('muf', 'tone'): (21, 35),
    ('muf', 'voice'): (23, 35),
    # POLY DLY v5 (2026-10-22): L/R tap ratios as integer num/den, plus
    # attack/decay. CC25 stays the generated EXPR row.
    ('mvd', 'numl'): (20, 69),
    ('mvd', 'denl'): (23, 69),
    ('mvd', 'numr'): (24, 69),
    ('mvd', 'denr'): (26, 69),
    ('mvd', 'att'): (21, 69),
    ('mvd', 'dec'): (22, 69),
    ('nam', 'in_gain'): (23, 37),   # 2026-09-01: swapped In<->Out encoders
    ('nam', 'out_gain'): (20, 37),  # (physical P2/P3 were reversed) — labels stay correct
    ('neb', 'dens'): (20, 70),
    ('neb', 'mix'): (22, 70),
    ('neb', 'size'): (23, 70),
    ('oct', 'char'): (23, 54),
    ('oct', 'sub'): (21, 54),
    ('oct', 'tone'): (20, 54),
    ('od', 'blend'): (23, 31),
    ('od', 'drive'): (24, 31),
    ('od', 'morph'): (20, 31),
    ('od', 'tone'): (22, 31),
    ('onw', 'blend'): (20, 45),
    ('onw', 'density'): (26, 45),
    ('onw', 'err'): (21, 45),
    ('onw', 'follow'): (24, 45),
    ('onw', 'mix'): (25, 45),
    ('onw', 'oct'): (22, 45),
    ('onw', 'size'): (23, 45),
    ('pf', 'mix'): (21, 27),
    ('pf', 'semi'): (20, 27),
    ('pha', 'depth'): (26, 8),
    ('pha', 'fback'): (22, 8),
    ('pha', 'rate'): (20, 8),
    ('pha', 'stages'): (23, 8),
    ('pll', 'intv'): (20, 62),
    ('pll', 'lock'): (23, 62),
    ('pll', 'mix'): (25, 62),
    ('pll', 'vmix'): (24, 62),
    ('ps', 'mix'): (25, 3),
    ('ps', 'semi'): (20, 3),
    ('res', 'fb'): (23, 47),
    ('res', 'frz'): (21, 47),
    ('res', 'mix'): (25, 47),
    ('res', 'oct'): (22, 47),
    ('res', 'onset'): (20, 47),
    ('rm', 'depth'): (25, 11),
    ('rm', 'freq'): (20, 11),
    ('rmz', 'drive'): (26, 34),
    ('rmz', 'midf'): (20, 34),
    ('rmz', 'midg'): (23, 34),
    ('rmz', 'tone'): (22, 34),
    ('rpt', 'char'): (23, 23),
    ('rpt', 'len'): (25, 23),
    ('rpt', 'mix'): (26, 23),
    ('rpt', 'prob'): (20, 23),
    ('rvs', 'len'): (23, 25),
    ('rvs', 'mix'): (22, 25),
    ('rvs', 'prob'): (20, 25),
    ('sdl', 'fback'): (23, 16),
    ('sdl', 'maxdel'): (20, 16),
    ('sdl', 'mix'): (24, 16),
    ('shm', 'bloom'): (23, 71),
    ('shm', 'intv'): (20, 71),
    ('shm', 'mix'): (21, 71),
    ('tape', 'amt'): (22, 21),
    ('tape', 'fail'): (20, 21),
    ('tape', 'model'): (23, 21),
    ('tdl', 'center'): (24, 68),
    ('tdl', 'fback'): (23, 68),
    ('tdl', 'head'): (26, 68),
    ('tdl', 'mix'): (22, 68),
    ('tdl', 'time'): (20, 68),
    ('tdl', 'width'): (25, 68),
    ('teq', 'freq'): (20, 58),
    ('teq', 'high'): (25, 58),
    ('teq', 'low'): (24, 58),
    ('teq', 'mid'): (26, 58),
    ('teq', 'tilt'): (23, 58),
    ('trm', 'depth'): (21, 43),
    ('trm', 'mode'): (20, 43),
    ('trm', 'rate'): (23, 43),
    ('tsy', 'cut'): (20, 61),
    ('tsy', 'det'): (21, 61),
    ('tsy', 'glide'): (25, 61),
    ('tsy', 'mix'): (24, 61),
    ('tsy', 'res'): (23, 61),
    ('tsy', 'sub'): (26, 61),
    ('vib', 'depth'): (24, 6),
    ('vib', 'fback'): (23, 6),
    ('vib', 'rate'): (20, 6),
    ('vol', 'level'): (24, 1),
    ('vrb', 'damp'): (23, 19),
    ('vrb', 'mix'): (26, 19),
    ('vrb', 'room'): (20, 19),
    ('wah', 'freq'): (26, 22),
    ('wah', 'res'): (20, 22),
}
# --->8--- end generated block ---

EFFECT_PARAMS = {
    "cln": [("state", "State", 0, 1)],
    "sld": [("state", "State", 0, 1)],
    "drt": [("state", "State", 0, 1)],
    "trm": [("rate", "Rate Hz", 0.5, 20), ("depth", "Depth", 0, 1), ("mode", "Style", 0, 3)],
    "pre": [("voice", "Voice", 0, 2)],
    "bpl": [("drive", "Drive", 0, 1)],
    "bph": [("drive", "Drive", 0, 1)],
    "bds": [("drive", "Drive", 0, 1), ("blend", "Clean Low", 0, 1), ("tone", "Tone", 0, 1)],
    "amb": [("state", "State", 0, 1)],
    "vol": [("level", "Level", 0, 1)],
    "atk": [("mix", "Mix", 0, 1), ("time", "Attack ms", 10, 2000), ("sens", "Sensitivity", 0, 1), ("shape", "Shape", 0, 1), ("range", "Range", 0, 1), ("retrig", "Retrig ms", 20, 150)],
    "ps":  [("semi", "Semitones", -12, 12), ("mix", "Dry %", 0, 100)],
    "h90": [("semi", "Semitones", -12, 12), ("mix", "Mix", 0, 1)],
    "pf":  [("semi", "Semitones", -12, 12), ("mix", "Mix", 0, 1)],
    "gps": [("mix", "Mix", 0, 1), ("semi", "Semitones", -12, 12), ("fback", "Feedback", 0, 1), ("dly", "Delay", 0, 0.1)],
    "fmt": [("shift", "Shift", -12, 12), ("mix", "Mix", 0, 1)],
    "fms": [("shift", "Shift", -12, 12), ("mix", "Mix", 0, 1)],
    "vib": [("rate", "Rate Hz", 0, 12), ("depth", "Depth", 0, 1.0), ("fback", "Feedback", 0, 0.7)],
    "cho": [("rate", "Rate Hz", 0, 8), ("depth", "Depth", 0, 1), ("mix", "Mix", 0, 1)],
    "pha": [("rate", "Rate Hz", 0, 12), ("depth", "Depth", 0, 1), ("fback", "Feedback", 0, 1), ("stages", "Stages", 1, 24)],
    "fla": [("rate", "Rate Hz", 0, 12), ("depth", "Depth", 0, 1.0), ("fback", "Feedback", -1, 1)],
    "les": [("speed", "Speed Hz", 0.1, 10), ("depth", "Mic Dist", 0, 1)],
    "hvy": [("state", "State", 0, 1)],
    "od":  [("drive", "Drive", 0, 1), ("tone", "Tone", 0, 1), ("morph", "Body", 0, 1), ("blend", "Clean Blend", 0, 1)],
    "mar": [("drive", "Drive", 0, 1), ("tone", "Tone", 0, 1), ("pres", "Presence", 0, 1), ("tight", "Tight", 0, 1)],
    "dhg": [("drive", "Drive", 0, 1), ("tone", "Tone", 0, 1), ("blend", "Fat", 0, 1), ("tight", "Edge", 0, 1)],
    "rmz": [("drive", "Drive", 0, 1), ("tone", "Filter", 0, 1), ("midf", "Mid Freq", 0, 1), ("midg", "Mid Gain", 0, 1)],
    "muf": [("drive", "Sustain", 0, 1), ("tone", "Tone", 0, 1), ("mids", "Mids", 0, 1), ("voice", "Voicing", 0, 1)],
    "rm":  [("depth", "Depth", 0, 1), ("freq", "Freq Hz", 0.05, 250)],
    "mog": [("sens", "Sensitivity", 0, 1), ("res", "Resonance", 0, 0.95), ("type", "Type", 1, 4), ("level", "Level", 0.0, 2.0), ("dist", "Drive", 0, 1)],
    "wah": [("freq", "Freq", 0, 1), ("res", "Res", 0.5, 0.97)],
    "sdl": [("mix", "Mix", 0, 1), ("maxdel", "Max Del", 0, 1), ("fback", "Feedback", 0, 1)],
    # BINSCATTER (2026-07-26 delay/verb expansion phase 2) — split out of
    # SPECDLY, spectraldelay.orc instr 201: same PV engine, per-bin RANDOM
    # delay table ("scatter"). P1 mix, P2 scatter amount (regenerates the
    # random table when turned), P3 feedback (soft-limited 0.5-1.1), P4 max
    # delay window.
    "bsc": [("mix", "Mix", 0, 1), ("scatter", "Scatter", 0, 1),
            ("fback", "Feedback", 0.5, 1.1), ("window", "Window s", 0.02, 1.0)],
    "bin": [("pedal", "Reduce", 0, 1)],
    "bc":  [("pedal", "Coarse", 0, 1)],
    "hab": [("mix", "Mix", 0, 1), ("size", "Size", 0, 1), ("repeats", "Repeats", 0, 1), ("blend", "Blend", 0, 1), ("scan", "Scan", 0, 1.0), ("rec", "Buffer", 0, 1)],
    "mod": [("mix", "Mix", 0, 1), ("clock", "Pitch", 0, 1), ("dens", "Density", 0, 1400), ("dly", "Max Dly", 0, 1)],
    "grn": [("mix", "Mix", 0, 1), ("dens", "Density", 10, 400), ("size", "Size", 0.05, 1.2), ("pitch", "Pitch", 0, 24)],
    "dly": [("time", "Time", 0, 700000), ("mix", "Mix", 0, 1), ("fback", "Feedback", 0, 1.2), ("semi", "Semi", -5, 12), ("mode", "Repeats", 0, 1)],
    "vrb": [("mix", "Mix", 0, 1), ("room", "Room", 0, 0.999), ("damp", "Damp", 0, 1)],
    # GLITCH v2 (2026-07-15) — stutter.orc INTEGRATION NOTES item I.
    "bbc": [("mix", "Mix", 0, 1), ("prob", "Prob", 0, 1), ("flt", "Filter", 0, 1)],
    "rpt": [("mix", "Mix", 0, 1), ("prob", "Prob", 0, 1), ("char", "Char", 0, 1), ("len", "Len", 0, 2)],
    "chp": [("depth", "Depth", 0, 1), ("prob", "Prob", 0, 1), ("rate", "Rate", 0, 1), ("shape", "Shape", 0, 1)],
    "rvs": [("mix", "Mix", 0, 1), ("prob", "Prob", 0, 1), ("len", "Len", 0, 1)],
    "tape": [("amt", "Amount", 0, 1), ("fail", "Fail", 0, 1), ("model", "Model", 0, 1)],
    # Amp sim is 100% wet, always — no mix knob (removed 2026-07-27).
    # P1 = OUT gain (post-NAMProcess), P2 = IN gain (pre-NAMProcess), dB.
    "nam0": [("out_gain", "Out dB", -18, 18), ("in_gain", "In dB", -12, 18)],
    "nam1": [("out_gain", "Out dB", -18, 18), ("in_gain", "In dB", -12, 18)],
    "nam2": [("out_gain", "Out dB", -18, 18), ("in_gain", "In dB", -12, 18)],
    "nam3": [("out_gain", "Out dB", -18, 18), ("in_gain", "In dB", -12, 18)],
    "nam4": [("out_gain", "Out dB", -18, 18), ("in_gain", "In dB", -12, 18)],
    # 2026-07-13 batch
    # ONWARD v3 = onset-driven, harmonic-following dynamic sampler.
    # "gate"/"sust"/"type" are GONE; "follow"/"density" are new (2026-07-14).
    "onw": [("mix", "Mix", 0, 1), ("blend", "Blend", 0, 1), ("size", "Size", 0, 1),
            ("follow", "Follow", 0, 1), ("density", "Density", 0, 1),
            ("err", "Error", 0, 1), ("oct", "Octave", -1, 1)],
    # MOOD II — reworked 2026-07-13: SLIP-only, no micro-looper, plain bypass
    # toggle (66) like everything else. Params: Mix, tonal-ladder Clock,
    # window Length, and the new free-running Generative knob.
    "md2": [("mix", "Mix", 0, 1), ("clock", "Clock", 0, 1),
            ("length", "Length", 0, 1), ("gen", "Generative", 0, 1)],
    "lsy": [("mix", "Mix", 0, 1), ("loss", "Loss", 0, 1), ("bw", "Bandwidth", 0, 1), ("pkt", "Packets", 0, 1)],
    "res": [("mix", "Mix", 0, 1), ("onset", "Onset", 0, 1), ("fb", "Feedback", 0, 1), ("oct", "Octave", -1, 1), ("frz", "Freeze", 0, 1)],
    "imp": [("mix", "Mix", 0, 1), ("prob", "Prob", 0, 1), ("dec", "Decay", 0, 1), ("tone", "Tone", 0, 1)],
    "oct": [("sub", "SUB", 0, 1), ("tone", "TONE", 0, 1), ("char", "CHAR", 0, 1)],
    "env": [("wet", "WET", 0, 1), ("sens", "SENS", 0, 1), ("mode", "MODE", 0, 1), ("att", "ATT", 0, 1), ("dec", "DEC", 0, 1)],
    "ir0": [("mix", "Mix", 0, 1)],
    "ir1": [("mix", "Mix", 0, 1)],
    "ir2": [("mix", "Mix", 0, 1)],
    "ir3": [("mix", "Mix", 0, 1)],
    "ir4": [("mix", "Mix", 0, 1)],
    # BLOOPER = grain-EVENT modulator of the LOOPER's loops (it owns no buffer).
    # The loop plays normally; occasional grain events punctuate it.
    # (output level knob removed 2026-07-14 — BLOOPER is always unity level)
    "blp": [("motion", "Motion", 0, 1), ("scat", "Mod Amt", 0, 1), ("size", "Grain", 0, 1), ("char", "Throw", 0, 2)],
    # 2026-07-13 ten-effect batch (toggles 57-65, instrs 180-188)
    "cmp": [("amt", "AMT", 0, 1), ("speed", "SPEED", 0, 1), ("makeup", "MKUP", 0, 1), ("blend", "BLEND", 0, 1), ("gate", "GATE", 0, 1)],
    "teq": [("mid", "MID", 0, 1), ("freq", "FREQ", 0, 1), ("tilt", "TILT", 0, 1), ("low", "LOW", 0, 1), ("high", "HIGH", 0, 1)],
    "cfb": [("ret", "Return", 0, 1), ("gain", "Feedback", 0, 1), ("tone", "Tone", 0, 1), ("ceil", "Ceiling", 0.1, 0.95)],
    "tsy": [("mix", "Blend", 0, 1), ("cut", "Cutoff", 0, 1), ("res", "Res", 0, 1), ("glide", "Glide", 0, 1), ("sub", "Sub", 0, 1), ("det", "Detune", 0, 1)],
    "pll": [("mix", "DryWet", 0, 1), ("intv", "Interval", 0, 4), ("lock", "LockSpd", 0, 1), ("vmix", "Voice", 0, 1)],
    "fbk": [("level", "Level", 0, 1), ("harm", "Harmonic", 0, 0.75), ("bloom", "Bloom", 0, 1), ("vib", "Vibrato", 0, 1)],
    "dif": [("wet", "DryWet", 0, 1), ("depth", "Depth", 0, 1), ("rate", "ModRate", 0, 1), ("duck", "Duck", 0, 1)],
    # 2026-07-26 delay/verb expansion phase 2 (docs/specs/2026-07-26-delay-
    # verb-expansion.md) — TAPE DELAY, MOVE DELAY, NEBULA, SHIMMER, FREEZE,
    # GRAINVERB (upgrade of the existing grainverb.orc, new UI entry).
    #
    # TAPE DELAY (tapedelay.orc, instr 190). P1 mix, P2 time (head-1 ms,
    # grid-snap optional csound-side), P3 feedback, P4/P5 drive the in-loop
    # bandpass-ish "loop filter" (center Hz / Q width), P6 head mode
    # (stepped mask 1,2,3,2+3,1+2,1+3,1+2+3 — see STEP_LABELS TDL_HEADS,
    # 0-based index like every other stepped param in this file).
    "tdl": [("mix", "Mix", 0, 1), ("time", "Time ms", 60, 1200),
            ("fback", "Feedback", 0, 1.15), ("center", "Filt Ctr", 250, 5000),
            ("width", "Filt Q", 0.5, 6), ("head", "Head", 0, 6)],
    # POLY DELAY v5 (polydelay, instr 191): two clock-locked taps, each a
    # NUM/DEN fraction of a beat (integers 1..16, whole-stepped on touch via
    # PARAM_STEP), plus Attack/Decay (0..1). Repeats (1..8) and pitch Interval
    # (-12..+12 semi) are touch-only absolute CCs drawn in the top strip, not
    # sliders — see _render_mvd_controls.
    "mvd": [("numl", "Num L", 1, 16), ("denl", "Den L", 1, 16),
            ("numr", "Num R", 1, 16), ("denr", "Den R", 1, 16),
            ("att", "Attack", 0, 1), ("dec", "Decay", 0, 1)],
    # NEBULA (nebula.orc, instr 193) — Supermassive-style FDN.
    "neb": [("mix", "Mix", 0, 1), ("dens", "Density", 0, 1), ("size", "Size", 0, 1)],
    # SHIMMER (shimmerverb.orc, instr 194). P2 interval is stepped
    # (+12/+7/+5/-12 — SHM_INTERVALS).
    "shm": [("mix", "Mix", 0, 1), ("intv", "Interval", 0, 3), ("bloom", "Bloom", 0, 1)],
    # FREEZE (freezeverb.orc, instr 195) — v2 UX. No mix/bypass-crossfade
    # knob here; P1 is the frozen-bed level itself (see spec section 6).
    "frz": [("level", "Bed Level", 0, 1), ("damp", "Damp", 0, 1), ("listen", "Listen Gain", 0, 1)],
    # GRAINVERB upgrade (existing grainverb.orc, character-preserving —
    # wide stereo, tempo-aware grains, in-loop damping).
    "gvb": [("mix", "Mix", 0, 1), ("spacing", "Spacing", 0, 1), ("decay", "Decay", 0, 1)],
}

# Stepped params rendered with a text label instead of the numeric value
# (LOOPER speed-index precedent). Keyed by (effect_code, param_name).
# The renderer indexes the list with int(round(value)), so the Csound-side
# range must be 0..len-1 — EXCEPT ("fbk","harm"), which Csound stores as
# 0/.25/.5/.75 (the DSP does int(harm*4)); scale_4x re-maps it for display.
TRM_MODES = ["SINE", "OPTO", "BIAS", "HARM"]
BLP_CHARS = ["NEAR", "THROW", "FAR"]
HAB_REC   = ["FROZEN", "REC"]
DLY_MODES = ["NORMAL", "REVERSE"]
PLL_INTERVALS = ["/4", "/2", "x1", "x2", "x3"]
FBK_HARMS = ["FUND", "+OCT", "+OCT5", "+2OCT"]
RPT_LENS = ["1/2 TICK", "1 TICK", "2 TICK"]   # gkRPT_len stepped 0..2
# 2026-07-26 delay/verb expansion phase 2 — TAPE DELAY head mask (0-based
# index per this file's convention, NOT the orc spec's 1-7 mask numbering),
# MOVE DELAY grid division + mode tables (mode table already 0..14, matches
# the spec's numbering directly), SHIMMER pitch interval.
TDL_HEADS = ["1", "2", "3", "2+3", "1+2", "1+3", "1+2+3"]
SHM_INTERVALS = ["+12", "+7", "+5", "-12"]
# POLY DELAY (mvd) v5 touch-only absolute selects (ch1). Repeats 1..8 rides
# CC110 as round((n-1)/7*127); pitch Interval -12..+12 semi rides CC111 as
# round((semi+12)/24*127). Both are engine-authoritative: the strip highlights
# whatever /mvd_reps · /mvd_intv echoes back, never a local write (same
# discipline as every slot toggle).
MVD_REPS_CC = 110        # ch1 CC — repeats 1..8 (absolute)
MVD_INTV_CC = 111        # ch1 CC — pitch interval -12..+12 semi (absolute)
# UTILITY page (menu 19) global controls — ch16 (companion_input.UTILITY_CHANNEL)
MASTER_VOL_CC = 103      # ch16 CC — master output volume (round(vol/1.5*127))
MASTER_BPM_CC = 104      # ch16 CC — master manual BPM (round((bpm-40)/260*127))
MASTER_VOL_MAX = 1.5     # unity = 1.0
MASTER_BPM_MIN = 40
MASTER_BPM_MAX = 300
RECORD_CMD = "demiurge-record"   # wrapper at /usr/local/bin (Pi only)
STEP_LABELS = {
    ("trm", "mode"): TRM_MODES,
    ("blp", "char"): BLP_CHARS,
    ("hab", "rec"): HAB_REC,
    ("dly", "mode"): DLY_MODES,
    ("pll", "intv"): PLL_INTERVALS,
    ("fbk", "harm"): FBK_HARMS,
    ("rpt", "len"): RPT_LENS,
    ("tdl", "head"): TDL_HEADS,
    ("shm", "intv"): SHM_INTERVALS,
}
# Params whose Csound value is a fraction of the step index (value * 4 = step).
STEP_SCALE = {("fbk", "harm"): 4}

# TRUE per-detent engine step for continuous params whose $ENC_ROT step in the
# .orc handlers is LARGER than the slider chase's default assumption of span/50
# (= 0.02 on a 0..1 range). When the chase under-estimated the step it fired too
# many encoder detents per tick, overshot the finger target, the engine echoed
# back past it, and the slider OSCILLATED — but only while dragging (idle, echo
# == held value, no fight). Worse for these because the real step (0.03-0.05) is
# also bigger than PARAM_DEADBAND (0.025), so a wrong estimate could never even
# settle. Using the real step here makes the burst land on target in one or two
# ticks with no overshoot. Values copied verbatim from the engine ENC_ROT calls
# (blooper 0.03; *_oct / AUTOMOOG level 0.04; the prob / mix / grain-size family
# 0.05). Params NOT listed use span/50, which is correct for them (their engine
# step is <= 0.02, so the chase under-shoots and converges monotonically — no
# oscillation). STEP_LABELS (discrete) still takes precedence over this.
PARAM_STEP = {
    ("blp", "scat"):   0.03, ("blp", "motion"): 0.03, ("blp", "size"): 0.03,
    ("res", "oct"):    0.04, ("onw", "oct"):    0.04, ("mog", "level"): 0.04,
    ("bbc", "prob"):   0.05, ("chp", "prob"):   0.05, ("imp", "prob"): 0.05,
    ("rpt", "prob"):   0.05, ("rvs", "prob"):   0.05, ("grn", "size"): 0.05,
    ("h90", "mix"):    0.05, ("pf",  "mix"):    0.05, ("gps", "mix"):  0.05,
    ("fms", "mix"):    0.05, ("fmt", "mix"):    0.05,
    # POLY DLY v5 integer num/den: one whole step per detent (span 15 / 50
    # would be 0.3 and overshoot every chase burst).
    ("mvd", "numl"):   1.0,  ("mvd", "denl"):   1.0,
    ("mvd", "numr"):   1.0,  ("mvd", "denr"):   1.0,
}

# Build flat list of all effect codes for convenience
ALL_EFFECTS = {}
for menu in MENUS:
    for code, display_name in menu["effects"]:
        ALL_EFFECTS[code] = display_name

# Preset menu: slot-to-preset-index mapping (-1=dry, 1=overdrive,
# 2=solid-dist, 3=ambient). Toggles (pha, trm) light from their _state OSC.
SLOT_TO_PRESET = {0: 1, 1: 2, 4: 3}

# Preset menu is mode-switched (Csound owns the mode via the PRESET_BANK launch
# flag and reports it over OSC /preset_bank). The UI is mode-blind: it holds ALL
# label tables and draws whichever bank Csound reports. Surf = the existing gig
# presets (untouched). Bass = the bass-rig bank; its codes are real effect codes
# so slot on-state reuses the normal bypass-state mechanism. Gorcore (bank 2,
# 2026-08-09) = the newer preset set (ids 10-14, handler_menus GORCORE
# branches); slots light from /preset like surf.
SURF_PRESETS = MENUS[0]["effects"]
BASS_PRESETS = [
    ("bpl", "PRE LIGHT"), ("bph", "PRE HEAVY"), ("tgl", "TOTAL GLITCH"),
    ("bds", "BASS DIST"), ("h90", "H90 WAMMY"),
]
GORCORE_PRESETS = [
    ("gc1", "VIB+SLAP"), ("gc2", "PRESET 2"), ("gc3", "PRESET 3"),
    ("gc4", "PRESET 4"), ("gc5", "AMBIENT"),
]
# UI row -> gkCurrentPreset id when that GORCORE slot is active.
GORCORE_SLOT_TO_PRESET = {0: 10, 1: 11, 2: 12, 3: 13, 4: 14}

# ---------------------------------------------------------------------------
# NAM amp rows — slot labels name the actual amp, read from the profile files.
# Files are named <row+1><variant+1>_amp_name.nam (e.g. 12_ac30_saturated.nam),
# so dropping a new file in automatically adds its name here — no UI edit.
# Csound reports each row's selected variant over /nam_sel<r>; the slot label
# follows the selection as the encoder turns. Rows without files fall back to
# the static family labels in MENUS.
# ---------------------------------------------------------------------------
NAM_PROFILE_DIR = os.environ.get(
    "NEPTR_NAM_DIR", os.path.expanduser("~/demiurge/profiles/nam"))

def _scan_nam_rows():
    try:
        names = [f for f in os.listdir(NAM_PROFILE_DIR) if f.endswith(".nam")]
    except OSError:
        return {}                     # dev machine without profiles
    rows = {}
    for f in sorted(names):
        stem = f[:-4]
        if len(stem) > 3 and stem[:2].isdigit() and stem[2] == "_":
            row, var = int(stem[0]) - 1, int(stem[1]) - 1
            rows.setdefault(row, {})[var] = stem[3:].replace("_", " ").upper()
    return {r: [v[k] for k in sorted(v)] for r, v in rows.items()}

NAM_ROWS = _scan_nam_rows()

# ---------------------------------------------------------------------------
# IR VERB rows (menu 18) — same live-label mechanism as NAM: files are named
# <row+1><variant+1>_label.wav (e.g. 21_plate_emt140_1s.wav); Csound reports
# the selected variant per row over /ir_sel<r>.
# ---------------------------------------------------------------------------
IR_DIR = os.environ.get("NEPTR_IR_DIR", os.path.expanduser("~/demiurge/ir"))

def _scan_ir_rows():
    try:
        names = [f for f in os.listdir(IR_DIR) if f.endswith(".wav")]
    except OSError:
        return {}                     # dev machine without IRs
    rows = {}
    for f in sorted(names):
        stem = f[:-4]
        if len(stem) > 3 and stem[:2].isdigit() and stem[2] == "_":
            row, var = int(stem[0]) - 1, int(stem[1]) - 1
            rows.setdefault(row, {})[var] = stem[3:].replace("_", " ").upper()
    return {r: [v[k] for k in sorted(v)] for r, v in rows.items()}

IR_ROWS = _scan_ir_rows()


def _value_fmt(min_value, max_value):
    """Integer display for big or whole-stepped ranges (semitones, stages,
    ms); 0-1-ish continuous ranges stay two-decimal. (The old UI's version
    of this misclassified 0..1 int-literal ranges as integer display.)"""
    span = max_value - min_value
    if span >= 50 or (
        isinstance(min_value, int) and isinstance(max_value, int)
        and 1 < span <= 30
    ):
        return "{:.0f}"
    return "{:.2f}"


# ---------------------------------------------------------------------------
# Font helpers (freesansbold bundled with pygame — no system font lookup)
# ---------------------------------------------------------------------------

_fonts = {}

def font(size):
    f = _fonts.get(size)
    if f is None:
        f = pygame.font.Font(None, size)
        _fonts[size] = f
    return f


def text_fit(text, size, color, max_w):
    """Render text, shrinking the font until it fits max_w."""
    f = font(size)
    while f.size(text)[0] > max_w and size > 16:
        size -= 4
        f = font(size)
    return f.render(text, True, color)


def blit_centered(dst, surf, cx, cy):
    dst.blit(surf, (int(cx - surf.get_width() / 2), int(cy - surf.get_height() / 2)))


def text_fit_multiline(text, size, color, max_w, max_h):
    """Render text single-line if it fits; otherwise split at a space and render
    two lines. Returns (list[Surface], final_size)."""
    f = font(size)
    while f.size(text)[0] > max_w and size > 20:
        size -= 4
        f = font(size)
    if f.size(text)[0] <= max_w:
        return [f.render(text, True, color)], size
    words = text.split()
    if len(words) >= 2:
        mid = max(1, len(words) // 2)
        line1 = ' '.join(words[:mid])
        line2 = ' '.join(words[mid:])
        f2 = font(size)
        while (max(f2.size(line1)[0], f2.size(line2)[0]) > max_w
               or f2.get_height() * 2 + 6 > max_h) and size > 20:
            size -= 4
            f2 = font(size)
        return [f2.render(line1, True, color), f2.render(line2, True, color)], size
    return [f.render(text, True, color)], size


# ---------------------------------------------------------------------------
# Cached visual assets — built ONCE (first use), blitted forever after.
# No per-frame gradient or glow computation anywhere.
# ---------------------------------------------------------------------------

_asset_cache = {}


def bg_gradient():
    """Full-canvas vertical gradient (BG family). One row-fill pass at
    startup, a single Surface blit per repaint after that."""
    s = _asset_cache.get("bg")
    if s is None:
        s = pygame.Surface((W, H))
        for y in range(H):
            t = y / (H - 1)
            s.fill((int(BG_TOP[0] + (BG_BOT[0] - BG_TOP[0]) * t),
                    int(BG_TOP[1] + (BG_BOT[1] - BG_TOP[1]) * t),
                    int(BG_TOP[2] + (BG_BOT[2] - BG_TOP[2]) * t)),
                   (0, y, W, 1))
        _asset_cache["bg"] = s
    return s


def glow_dot(radius, color):
    """Additive radial glow sprite (press feedback, sleep halo, stroke
    trails). Blitted with BLEND_ADD, which IGNORES the alpha channel and adds
    raw RGB — so the falloff must be baked into the RGB itself (premultiplied).
    The pre-2026-08-09 version stored full-strength RGB under a fading alpha,
    which BLEND_ADD turned into a solid uniform disc: the "discoloration"
    visible behind the sleep faces."""
    key = ("glow", radius, color)
    s = _asset_cache.get(key)
    if s is None:
        s = pygame.Surface((radius * 2, radius * 2), pygame.SRCALPHA)
        r_, g_, b_ = color
        for i in range(radius, 0, -2):
            a = (1 - i / radius) ** 2 * (110 / 255.0)
            pygame.draw.circle(s, (int(r_ * a), int(g_ * a), int(b_ * a), 255),
                               (radius, radius), i)
        _asset_cache[key] = s
    return s


def tab_icon(i, size, color):
    """Tiny procedural glyph per companion tab (cached per color/size).
    0 DASH bars, 1 PATCH grid, 2 AUDIO speaker, 3 SYSTEM power-circle,
    4 NET wifi arcs. (The sine glyph that was tab 4 is now wave_icon() —
    WAVE is a top-level destination, not a settings tab.)"""
    key = ("icon", i, size, color)
    s = _asset_cache.get(key)
    if s is not None:
        return s
    s = pygame.Surface((size, size), pygame.SRCALPHA)
    c, m = color, size
    if i == 0:                                    # three rising bars
        bw = m // 5
        for j, hgt in enumerate((0.4, 0.7, 1.0)):
            x = j * (bw + m // 8)
            pygame.draw.rect(s, c, (x, int(m * (1 - hgt)), bw, int(m * hgt)),
                             border_radius=2)
    elif i == 1:                                  # 2x2 tile grid
        t = m // 2 - 2
        for (x, y) in ((0, 0), (m - t, 0), (0, m - t), (m - t, m - t)):
            pygame.draw.rect(s, c, (x, y, t, t), 2, border_radius=3)
    elif i == 2:                                  # speaker + arc
        pygame.draw.polygon(s, c, [(0, m // 3), (m // 3, m // 3),
                                   (int(m * 0.58), m // 8),
                                   (int(m * 0.58), int(m * 0.88)),
                                   (m // 3, 2 * m // 3), (0, 2 * m // 3)])
        pygame.draw.arc(s, c, (int(m * 0.5), m // 5, m // 2, int(m * 0.6)),
                        -1.1, 1.1, 2)
    elif i == 3:                                  # power symbol
        pygame.draw.circle(s, c, (m // 2, m // 2 + 2), m // 2 - 2, 2)
        pygame.draw.line(s, c, (m // 2, 0), (m // 2, m // 2), 3)
    else:                                         # wifi: three arcs + dot
        for j, rr in enumerate((0.95, 0.62, 0.30)):
            box = (int(m / 2 - m * rr / 2), int(m * 0.9 - m * rr),
                   int(m * rr), int(m * rr))
            pygame.draw.arc(s, c, box, 0.7, 2.44, 3 if j == 2 else 2)
        pygame.draw.circle(s, c, (m // 2, int(m * 0.88)), 3)
    _asset_cache[key] = s
    return s


def wave_icon(size, color):
    """One sine cycle — the WAVE destination's mark, on the main screen's
    top-left button now that WAVE is not a settings tab."""
    key = ("wave_icon", size, color)
    s = _asset_cache.get(key)
    if s is not None:
        return s
    s = pygame.Surface((size, size), pygame.SRCALPHA)
    m = size
    pts = [(x, int(m / 2 - (m / 2 - 2) * math.sin(x * 2 * math.pi / m)))
           for x in range(0, m, 2)]
    pygame.draw.lines(s, color, False, pts, 3)
    _asset_cache[key] = s
    return s


# Musician-facing labels for engine-setting values. The CLI's value is the
# truth; this is presentation only, and anything unlisted shows verbatim.
# 24-bit appears nowhere on purpose — csound's realtime ALSA cannot do it.
SETTING_LABELS = {
    "bits": {"16": "16-bit", "32": "32-bit  (recommended)", "float": "float"},
    "mode": {"sync": "SYNC LAYER", "bypass": "BYPASS"},
}


def setting_label(key, value):
    return SETTING_LABELS.get(key, {}).get(value, str(value))


# ─────────────────────────── on-screen keyboard ───────────────────────────
# Sized for the hardware, not for a desktop metaphor. The panel is a 480x800
# resistive single-touch screen (canvas 720x1280), and a fingertip is ~7-9 mm.
# A 10-column QWERTY row would give ~48 physical px per key — below a reliable
# touch target on a panel that also wobbles. So: FIVE columns of ~90 physical
# px, alphabetical (faster to scan than a QWERTY broken across a narrow grid,
# and this keyboard is used rarely). Commit-on-release with a press preview,
# like every other control here, so a mis-landed finger can slide off a key
# without typing it.
#
# Rows of (label, action, span). action: a literal character, or a verb.
KB_PAGES = [
    [[("a", "a", 1), ("b", "b", 1), ("c", "c", 1), ("d", "d", 1), ("e", "e", 1)],
     [("f", "f", 1), ("g", "g", 1), ("h", "h", 1), ("i", "i", 1), ("j", "j", 1)],
     [("k", "k", 1), ("l", "l", 1), ("m", "m", 1), ("n", "n", 1), ("o", "o", 1)],
     [("p", "p", 1), ("q", "q", 1), ("r", "r", 1), ("s", "s", 1), ("t", "t", 1)],
     [("u", "u", 1), ("v", "v", 1), ("w", "w", 1), ("x", "x", 1), ("y", "y", 1)],
     [("z", "z", 1), ("space", " ", 2), ("SHIFT", "@shift", 1),
      ("123", "@page", 1)],
     [("DEL", "@back", 2), ("JOIN", "@ok", 3)]],
    [[("0", "0", 1), ("1", "1", 1), ("2", "2", 1), ("3", "3", 1), ("4", "4", 1)],
     [("5", "5", 1), ("6", "6", 1), ("7", "7", 1), ("8", "8", 1), ("9", "9", 1)],
     [("!", "!", 1), ("@", "@@", 1), ("#", "#", 1), ("$", "$", 1), ("%", "%", 1)],
     [("^", "^", 1), ("&", "&", 1), ("*", "*", 1), ("(", "(", 1), (")", ")", 1)],
     [("-", "-", 1), ("_", "_", 1), ("=", "=", 1), ("+", "+", 1), (".", ".", 1)],
     [(",", ",", 1), (":", ":", 1), ("/", "/", 1), ("?", "?", 1), ("~", "~", 1)],
     [("abc", "@page", 2), ("DEL", "@back", 1), ("JOIN", "@ok", 2)]],
]
KB_COLS = 5
KB_REVEAL_SECS = 1.2      # how long the last typed character stays legible


def rounded_slot(c, rect, on, pressed_glow=False, outline=OFF_OUTLINE):
    """The one slot/tile look, shared by menus and companion lists:
    ON  = white fill (the identity look, kept) + soft accent halo,
    OFF = rounded outline."""
    if on:
        halo = rect.inflate(10, 10)
        pygame.draw.rect(c, ACCENT_DIM, halo, border_radius=RADIUS + 5)
        pygame.draw.rect(c, FG, rect, border_radius=RADIUS)
    else:
        pygame.draw.rect(c, outline, rect, 3, border_radius=RADIUS)


# ---------------------------------------------------------------------------
# UI state machine + renderer
# ---------------------------------------------------------------------------

class NeptrUI:
    def __init__(self):
        # View state: 'menu0' .. 'menu18' for menu views, or effect code for effect view
        self.current_view = 'menu0'
        self.preset_bank = 0   # 0=surf, 1=bass; set by Csound via OSC /preset_bank
        self.title_text = "INPUT"
        # Cache of all received parameter values  {'/vol_level': 0.5, ...}
        self.param_values = {}
        # Bypass states: {effect_code: int}  0=ON, 1=BYPASSED
        self.bypass_states = {}
        self.last_osc_time = time.time()
        self.is_sleeping = False
        self.last_menu_index = 0
        self.dirty = True
        # Skip repaints when nothing visible changed.
        self._last_rendered_view = None
        self._last_rendered_signature = None
        # Invert-then-enter-submenu switch
        self.pending_switch_at = None
        self.pending_switch_code = None
        # Block-dissolve transition: blocks of the new view still to reveal
        self.trans_blocks = []
        self.trans_next_at = None
        # Sleep faces
        self.faces = []
        self.face_current = 0          # index into self.faces
        self.face_check_at = None
        self.face_revert_at = None
        self.face_showing_other = False
        # timings = original x0.75 (25% faster)
        self.face_interval = 0.75
        self.face2_duration = 0.3
        self.face2_probability = 0.15
        self.other_face_duration = 1.875
        self.other_face_probability = 0.16

        self.canvas = pygame.Surface((W, H))
        self.clean = None   # last fully-composed view (glitch source)

        # ----- companion mode -----
        self.input_state = companion_input.InputState()
        self.acts = actions.from_env()
        self.companion = companion_mod.CompanionController(
            item_counts=self._companion_item_counts,
            default_sel=self._companion_default_sel)
        self.patches = []
        self.active_patch = None         # patch currently in live.conf
        # Patch switches are ARMED, not instant: the first tap moves the
        # cursor and asks for confirmation, a second tap within
        # PATCH_CONFIRM_SECS commits. One stray contact on the resistive panel
        # used to be enough to load whatever patch the cursor sat on — with
        # the cursor pinned at row 0 that meant car_mode, at LOW power, with an
        # engine restart. (2026-08-10 "it keeps going to car mode".)
        self.patch_armed = None          # index awaiting confirmation
        self.patch_armed_until = None
        self.interfaces = []             # cached demiurge-interface --list
        self._iface_cache_at = 0.0
        self.power_label = None
        self.companion_msg = None
        self.companion_msg_until = None
        self.companion_refresh_at = None
        self._pi_cache = {}
        self._pi_cache_at = 0.0

        # ----- looper state (menu 14) -----
        # state: 0=idle, 1=rec, 2=play, 3=overdub, 4=stopped, 5=sync-closing
        self.lp_states = [0, 0, 0, 0, 0]
        self.lp_pos    = [0.0, 0.0, 0.0, 0.0, 0.0]   # 0..1 playback fraction
        self.lp_speed  = [6, 6, 6, 6, 6]              # index into giLpSpeeds table
        # Drum sample mode (drumloops.orc): /lp_drum_ch = channel holding the
        # one drum loop (1-5, -1 = none), /lp_drum_idx = loaded file index into
        # the sorted list (-1 = none), /lp_drum_n = files in the drum folder.
        self.lp_drum_ch    = -1
        self.lp_drum_idx   = -1
        self.lp_drum_count = 0
        self._drum_files = None          # cached folder listing for filenames
        self._drum_files_count = -1      # lp_drum_count the cache was built for

        # ----- NAM amp rows (menu 17) -----
        self.nam_sel = [0, 0, 0, 0, 0]   # selected variant per row (csound /nam_sel<r>)

        # ----- IR VERB rows (menu 18) -----
        self.ir_sel = [0, 0, 0, 0, 0]    # selected variant per row (csound /ir_sel<r>)

        # hyperGuitar link (menu 20). hg_present is driven ONLY by /hg_present
        # from the engine, which in turn is driven by the guitar's heartbeat —
        # the UI never probes USB or the network itself, so there is exactly
        # one authority for "is the guitar there" and the page cannot appear
        # here while the engine refuses to navigate to it.
        self.hg_present = False
        # Mirrors of the Bela's lane state, chased from /hg_lane. Never written
        # on touch — the guitar echoes every change back, so a dead link shows
        # as a page that visibly does not respond rather than one that lies.
        self.hg_lane_on = [0] * 5
        self.hg_lane_idx = [0] * 5

        # ----- MASTER LEVEL STRIPS (always on, both screen edges) -----
        # Fed by /meter from signal_chain.orc, taken immediately before `outs`
        # so it shows what actually leaves the converters. Values are rms in
        # linear amplitude; the display converts to dB.
        #
        # peak-hold: the engine sends at 25 Hz, which is too coarse to catch a
        # transient by eye, so a held peak decays slowly on top of the bar.
        self.meter_l = 0.0
        self.meter_r = 0.0
        self.meter_pk_l = 0.0
        self.meter_pk_r = 0.0
        self.meter_seen = 0.0            # time of last /meter (staleness)
        # Clip latch: peak at or above full scale. HELD for a few seconds so a
        # single clipped buffer cannot flash past unseen - the question this
        # meter exists to answer is "did my chain ever go over", and that has
        # to survive blinking.
        self.meter_clip_l = 0.0          # time of last clip, per side
        self.meter_clip_r = 0.0

        # ----- METRO DOT (TASK 2/3a): local predictor fed by /beat_idx ·
        # /beat_down · /beat_period. beat_anchor_time is OUR wall-clock
        # receipt time of the last beat edge, NOT anything the engine sends
        # — that's the whole point: a late/dropped message just means the
        # next edge re-anchors, instead of the dot free-running from a
        # stale engine-side timestamp.
        self.beat_period = 0.5           # s, last known beat period (/beat_period)
        self.beat_down = 0               # 0/1, downbeat flag of the last edge
        self.beat_anchor_time = None     # time.time() of the last /beat_idx; None = never seen
        self.beat_seen = 0.0             # same instant, for staleness (BEAT_LIVE_TIMEOUT)

        # ----- DSP LOAD BAR (TASK 3b): fed by /dsp_load · /dsp_xrun from the
        # C++ host, 25 Hz. -----
        self.dsp_load = 0.0              # 0..1+ fraction of the quantum deadline (raw, latest)
        self.dsp_load_seen = 0.0         # time of last /dsp_load (staleness)
        self.dsp_xruns = 0               # cumulative xrun count, engine-authoritative
        self.dsp_xrun_flash_until = 0.0  # brief red flash on a NEW xrun
        # Asymmetric-slew display value (fast attack / slow release, see
        # DSP_LOAD_ATTACK_TAU/DSP_LOAD_RELEASE_TAU + _draw_dsp_bar) and the
        # wall-clock time it was last advanced, so the filter is time-based
        # (not frame-count-based) and self-corrects across any gap.
        self.dsp_load_disp = 0.0
        self.dsp_load_disp_t = None

        # ----- NAM/IR variant-list overlay (menu 17/18 only) -----
        # {"menu": NAM_MENU|IR_MENU, "row": state row 0-4} while open, else
        # None. Modal, reachable from the nam{r}/ir{r} effect view's new
        # BROWSE button (see _render_effect). Deliberately NOT part of the
        # companion self.picker/self.patcher family (those only exist while
        # self.companion.active is True); this one overlays the NORMAL
        # (non-companion) view, so it needs its own render()/touch_down
        # short-circuits -- see those insertion points below.
        self.variant_list = None

        # ----- UTILITY page (menu 19) global controls -----
        # All engine-authoritative: the sliders chase the OSC echo, the RECORD
        # indicator tracks the wrapper's status. master_out unity = 1.0.
        self.master_out = 1.0            # /master_out echo (0..1.5)
        self.master_bpm = 120.0          # /master_bpm echo — manual BPM setting
        self.clock_bpm = 120.0           # /clock_bpm echo — LIVE authoritative tempo
        self.clock_bpm_seen = False      # no /clock_bpm received yet -> the readout shows "--.-", never an invented 120
        self.recording = False           # demiurge-record state (poll + toggle)
        self._mvol_rect = None           # master-volume slider hit rect
        self.sm = sm_page.SmPage(sys.modules[__name__])   # SoundMagic spectral rack page
        self._mbpm_rect = None           # master-BPM slider hit rect
        self._master_sent_at = 0.0       # ch16 CC rate-limit (shared, like expr)

        # ----- POLY DELAY (mvd) extras -----
        # Repeats / pitch-interval selection. Engine-authoritative (echoes
        # /mvd_reps 1..8 · /mvd_intv -12..12); taps never write these locally.
        self.mvd_reps = 4                # /mvd_reps echo (engine default 4)
        self.mvd_intv = 7                # /mvd_intv echo (signed semitones, default +7)

        # ----- touch interaction -----
        self.hit = []                 # [(Rect, action_tuple)] rebuilt per render
        self.press_fx = None          # (canvas_x, canvas_y, t0) glow pulse
        self.touch_pos = None         # current finger canvas pos while down
        self.touch_start = None       # (x, y) at finger-down
        self.touch_moved = False      # exceeded TAP_SLOP since down
        # None | 'wave' | 'bars' | ('nav', screen) | ('param', name, lo, hi,
        # phys_cc, Rect) — 'nav' covers both list scroll (vertical) and tab
        # swipe (horizontal), split by axis dominance on first movement.
        self.drag_target = None
        self._scroll_accum = 0
        self._nav_start = None        # immutable finger-down point for swipes
        self._nav_axis = None         # None until dominant axis decided
        self.list_scroll = {}         # companion screen -> first visible row

        # ----- engine link (touch -> Teensy-identical CCs) -----
        self.engine = companion_input.EngineOut()
        self.last_toggle = -1         # OSC /last_toggle mirror (slider arming)
        self.osc_seen = False         # any OSC arrived at all
        # A single stray message is NOT a synced UI. The CC102 answer is a
        # burst covering every state/param address, so "synced" means we have
        # seen SYNC_MIN_ADDRS distinct addresses; until then the request keeps
        # going out on a backing-off timer (the engine may still be starting —
        # on a cold boot demiurge comes up long after neptr-ui does).
        self.synced = False
        self._sync_addrs = set()
        self.sync_retry_at = None     # next sync-request re-send deadline
        self.sync_retry_dt = SYNC_RETRY_SECS
        self.sync_deadline = None     # stop asking after this (None = no limit)
        self.sync_retries = 0
        self.resync_pending = False   # engine looks restarted; re-ask next pass
        # While a CC102 full-state rebroadcast is landing, *_state echoes are
        # CATCH-UP data, not fresh toggles — update silently, never auto-
        # navigate into a param view (found on-device: the UI teleported into
        # whatever effect happened to be ON at startup).
        self.sync_quiet_until = 0.0
        self._lp_touch_cc = None      # held looper row's physical CC
        # PRESETS-page hold gesture: (start_t, row, action, rect) while a finger
        # is held on a preset slot; resolved at finger-up (see touch_up) into an
        # in-engine bank op (ch16 CC105/106/107, slot in value).
        self._preset_hold = None
        # expression-pedal slider (bottom of menu views): local-state thumb
        # (CC29 has no OSC echo), absolute sends, silent until first touch.
        self.expr_value = 0.0
        self._expr_rect = None
        self._expr_sent_at = 0.0

        # pressed-state: element under the finger highlights IMMEDIATELY on
        # touch-down ("as if it's about to turn on"); the RELEASE decides the
        # behavior. Swipe classification cancels it with a quick fade.
        self.pressed_hit = None       # (Rect, action) while finger is down
        self.pressed_fade = None      # (Rect, t0) cancelled highlight fading

        # param slider chase: dict(code,param,phys,lo,hi,target,until) | None
        self.param_chase = None
        self.param_tick_at = None     # next allowed pulse time (rate cap)
        self.param_hint_until = None  # "re-toggle to arm" hint expiry
        self._param_hits = []         # [(Rect, name, lo, hi)] from last render
        self._stroke_trail = []       # [(x, y, t)] WAVE shimmer trail
        # companion tab slide transition
        self.slide_old = None         # canvas copy of the outgoing tab
        self.slide_new = None
        self.slide_dir = 0
        self.slide_t0 = None

        # ----- WAVE destination (wavetable editor) -----
        # WAVE is a TOP-LEVEL view (current_view == WAVE_VIEW) reached from the
        # main screen's top-left button — not a companion tab and not a
        # setting. Drawing a wavetable is making a sound, not configuring the
        # box, so it does not belong behind the settings menu.
        self.wave = wavetable.WaveState()
        self.wave_return_view = None  # menu view the WAVE back arrow returns to
        self.wt_host, self.wt_port = wavetable_osc_target()
        self._wt_client = None        # lazy SimpleUDPClient
        self.wt_send_at = None        # debounce deadline for the OSC send
        self.wt_sends = 0             # sends this session (status line)
        self.wt_last_send = None
        self._wave_rects = None       # (wave_rect, bars_rect) from last render

        # ----- settings caches (AUDIO / NETWORK tabs) -----
        # Every value here is READ from a demiurge CLI and written back
        # through the same CLI — the UI owns no configuration state of its
        # own. Refreshed off-thread so a shell-out never stalls the UI loop.
        self.audio_settings = {}      # demiurge-audio --list
        self._audio_cache_at = 0.0
        self.wifi = None              # demiurge-wifi --status-machine
        self.wifi_nets = []           # demiurge-wifi --list
        self._wifi_cache_at = 0.0
        self.wifi_busy = False        # a join/scan is in flight
        self.graph = None             # demiurge-graph --list (see actions)
        self._graph_cache_at = 0.0
        # Value picker + keyboard + patcher overlays (None/False = closed).
        self.picker = None            # {"key","label","options","value","effect"}
        self.keyboard = None          # see _open_keyboard()
        self.patcher = False          # PATCHER overlay open? (see _open_patcher)
        self.graph_pick = None        # port dict tapped as one end of a new link
        self.setting_armed = None     # (key, value) awaiting a confirming tap
        self.setting_armed_until = None
        log.info("wavetable OSC target %s:%d", self.wt_host, self.wt_port)

        # ----- persisted UI position ("remember where I was") -----
        # UI-owned view state ONLY (ui_state.py explains the boundary). The
        # store debounces + change-detects, so this costs one small atomic
        # rename every few seconds of actual navigation, and nothing at idle.
        self.state_store = ui_state.Store()
        # A saved EFFECT view is re-entered only once the engine's own /menu
        # echo confirms we are on the menu that effect lives in — the engine
        # owns which menu is current, the UI owns which slot was open on it.
        self.restore_effect = None
        self.restore_menu = None
        # PRESET RECALL WINDOW. A recall toggles a dozen effects in a burst,
        # and each toggle-ON would otherwise drag the view onto that effect's
        # param page - so the screen walks through them and parks on whichever
        # landed last. Mid-song that means losing the LOOPER page you were
        # actually using. While this timestamp is in the future, toggle-driven
        # navigation is suppressed; the toggles still register and still
        # repaint, they just do not MOVE you.
        self.preset_recall_until = 0.0

    _LP_SPEED_LABELS = ["-2x", "-1x", "-½", "-¼", "+¼", "+½", "+1x", "+2x"]

    # Same folder drumloops.orc scans: Pi deploy path first, repo-relative
    # fallback for dev/VST machines. Derived from the real $HOME rather than
    # a literal username, so it matches whatever Pi user is actually running
    # this (see launch_phase4.sh's --omacro:DRUMLOOP_DIR, which overrides the
    # .orc's own gorpi7 fallback the same way).
    _DRUM_DIRS = [
        os.path.join(os.path.expanduser("~"), "demiurge", "samples", "drumloops"),
        os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     "samples", "drumloops"),
    ]

    def _drum_filename(self, idx):
        """Resolve the loaded drum file's name UI-side (no OSC strings): list
        the drum folder in drumloops.orc's ordering — .wav/.aif/.aiff/.flac,
        one alphabetical sort over all of them. Cached; rebuilt whenever
        /lp1_drumn reports a different file count. Returns None if the folder
        is missing or the index is out of range."""
        if self._drum_files is None or self._drum_files_count != self.lp_drum_count:
            files = []
            for d in self._DRUM_DIRS:
                try:
                    names = os.listdir(d)
                except OSError:
                    continue
                files = sorted(n for n in names
                               if n.endswith((".wav", ".aif", ".aiff", ".flac")))
                break
            self._drum_files = files
            self._drum_files_count = self.lp_drum_count
        if 0 <= idx < len(self._drum_files):
            return self._drum_files[idx]
        return None

    def load_faces(self):
        faces_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "protoFACES")
        for i in range(1, 8):
            img = pygame.image.load(os.path.join(faces_dir, f"FACE{i}.png")).convert_alpha()
            if img.get_size() != (720, 720):
                img = pygame.transform.smoothscale(img, (720, 720))
            self.faces.append(img)

    # ----- persisted position ---------------------------------------------

    def state_snapshot(self):
        """The UI-owned "where was I" facts, in ui_state.DEFAULTS' shape.

        Deliberately absent: bypass_states, param_values, preset bank, looper
        transport, NAM/IR selection. The engine owns those and re-broadcasts
        them on the CC102 sync — persisting them here would create a second,
        immediately-stale source of truth for the same values."""
        view = self.current_view
        if self.restore_effect is not None:
            view = self.restore_effect      # not applied yet; keep remembering it
        return {
            "view": view,
            "menu": int(self.last_menu_index),
            "companion": 1 if self.companion.active else 0,
            "companion_screen": companion_mod.screen_name(
                self.companion.screen),
            "companion_sel": int(self.companion.sel),
            "scroll_patches": int(self.list_scroll.get(
                companion_mod.PATCHES, -1)),
            "scroll_audio": int(self.list_scroll.get(companion_mod.AUDIO, -1)),
        }

    def restore_position(self, saved, now=None):
        """Apply a ui_state dict from the previous run. Everything is range-
        checked against the CURRENT world (menus, patch list), so a stale or
        hand-edited file degrades to the default view instead of breaking."""
        now = time.time() if now is None else now
        menu = saved.get("menu", 0)
        try:
            menu = int(menu)
        except (TypeError, ValueError):
            menu = 0
        self.last_menu_index = max(0, min(menu, len(MENUS) - 1))
        view = str(saved.get("view", "menu0"))
        if view.startswith("menu") and view[4:].isdigit():
            idx = max(0, min(int(view[4:]), len(MENUS) - 1))
            self.current_view = f"menu{idx}"
            self.last_menu_index = idx
            self.title_text = MENUS[idx]["name"]
        elif view in ALL_EFFECTS:
            # Effect views wait for the engine to confirm menu + on-state.
            self.current_view = f"menu{self.last_menu_index}"
            self.title_text = MENUS[self.last_menu_index]["name"]
            self.restore_effect = view
            self.restore_menu = self.last_menu_index
        else:
            self.current_view = "menu0"
            self.title_text = MENUS[0]["name"]
            self.last_menu_index = 0

        for key, screen in (("scroll_patches", companion_mod.PATCHES),
                            ("scroll_audio", companion_mod.AUDIO)):
            try:
                val = int(saved.get(key, -1))
            except (TypeError, ValueError):
                val = -1
            if val >= 0:
                self.list_scroll[screen] = val

        if int(saved.get("companion", 0) or 0):
            self.patches = actions.list_patches(self.acts.patches_dir)
            self.refresh_active_patch()
            self._refresh_interfaces(now, force=True)
        # A pre-rename state file holds an INTEGER here. Those indices no
        # longer mean what they meant (old 3 = AUDIO, new 3 = SYSTEM), so
        # screen_index() resolves names only and sends anything else — stale
        # integer, hand-edited junk, a tab that no longer exists — to the
        # default tab rather than to the wrong one.
        self.companion.restore(companion_mod.screen_index(
                                   saved.get("companion_screen")),
                               int(saved.get("companion_sel", 0) or 0),
                               active=bool(int(saved.get("companion", 0) or 0)),
                               now=now)
        self.dirty = True

    # ----- WAVE destination ------------------------------------------------

    def on_wave(self):
        """True when the WAVE editor is what's on screen. One predicate for
        every caller (touch routing, OSC-send repaint, trail animation) so the
        editor can never be half-open: it is a view, and views are mutually
        exclusive with the companion menu."""
        return not self.companion.active and self.current_view == WAVE_VIEW

    def enter_wave(self):
        """Open WAVE from the main screen's top-left button. Remembers the
        menu it was opened from so the back arrow returns there."""
        if self.current_view == WAVE_VIEW:
            return
        self.wave_return_view = self.current_view
        self.current_view = WAVE_VIEW
        self.title_text = "WAVETABLE"
        self._cancel_pending_switch()
        self.dirty = True

    def leave_wave(self):
        """Back out of WAVE to wherever it was opened from, falling back to
        the engine's current menu if that view is no longer valid."""
        back = self.wave_return_view or 'menu%d' % self.last_menu_index
        idx = int(back[4:]) if back.startswith('menu') and back[4:].isdigit() \
            else self.last_menu_index
        if not 0 <= idx < len(MENUS):
            idx = 0
        self.wave_return_view = None
        self.current_view = 'menu%d' % idx
        self.title_text = MENUS[idx]["name"]
        self.dirty = True

    # ----- patch selection -------------------------------------------------

    def refresh_active_patch(self):
        """Re-read which patch live.conf currently holds (cheap: two small
        file reads). Every other DEMIURGE UI already marks it; this UI did
        not, which is why row 0 looked like the live patch."""
        self.active_patch = actions.active_patch(self.acts.patches_dir)
        return self.active_patch

    def _companion_default_sel(self, screen):
        """Where the cursor lands when a companion screen is entered: on the
        thing that is currently LIVE, not on row 0."""
        if screen == companion_mod.PATCHES:
            if self.active_patch in self.patches:
                return self.patches.index(self.active_patch)
        elif screen == companion_mod.AUDIO:
            for i, iface in enumerate(self.interfaces):
                if iface.get("active"):
                    return i
        return None

    # ----- OSC handlers (called on the main thread) -----------------------

    def note_osc(self, addr):
        """Record an inbound OSC address for late-join sync accounting. The
        UI counts as SYNCED once a snapshot's worth of distinct addresses has
        arrived — one stray broadcast must not cancel the retries."""
        self.osc_seen = True
        if self.synced:
            # Traffic after a long silence = a different engine than the one
            # we synced with (restart / patch switch). Ask again.
            if time.time() - self.last_osc_time > RESYNC_AFTER_SILENCE:
                self.resync_pending = True
            return
        self._sync_addrs.add(addr)
        if len(self._sync_addrs) >= SYNC_MIN_ADDRS:
            self.synced = True
            self.sync_retry_at = None
            log.info("engine sync: snapshot received (%d addresses, %d request(s))",
                     len(self._sync_addrs), self.sync_retries)

    def _touch(self):
        self.last_osc_time = time.time()
        if self.is_sleeping:
            self.wake_up()

    def _cancel_pending_switch(self):
        self.pending_switch_at = None
        self.pending_switch_code = None

    def handle_menu_change(self, menu_index):
        self._touch()
        self._cancel_pending_switch()
        # HG_MENU sits one past MENUS and is only reachable while the guitar
        # is present; clamping to len(MENUS)-1 unconditionally would silently
        # bounce every hyperGuitar menu echo back to IR VERB.
        hi = HG_MENU if (self.hg_present and menu_index == HG_MENU) else len(MENUS) - 1
        menu_index = max(0, min(menu_index, hi))
        if menu_index != NORRIS_MENU:
            self.sm.close_sheet()
        if menu_index == UTILITY_MENU:
            # Entering the UTILITY page: refresh the RECORD indicator from the
            # wrapper (the only state the engine does not broadcast over OSC).
            self._poll_record_status()
        if time.time() < self.preset_recall_until:
            # A recall must not move the menu either. Track the engine's index
            # so nav stays consistent, but leave the VIEW where the player put
            # it. Without this the /menu echo in the recall burst still yanked
            # the screen even with toggle navigation suppressed.
            self.last_menu_index = menu_index
            self.dirty = True
            return
        new_view = f'menu{menu_index}'
        menu_name = HG_MENU_NAME if menu_index == HG_MENU else MENUS[menu_index]["name"]
        if new_view == self.current_view and self.title_text == menu_name:
            # Already on this menu — but the echo still CONFIRMS it, which is
            # what a pending effect-view restore is waiting for.
            self.last_menu_index = menu_index
            self._try_restore_effect()
            return
        self.last_menu_index = menu_index
        self.current_view = new_view
        self.title_text = menu_name
        self.dirty = True
        self._try_restore_effect()

    def _try_restore_effect(self):
        """Re-open the effect view the last run ended on, once the engine has
        confirmed BOTH halves of the picture: its current menu still contains
        that effect, and that effect is currently ON. Stays armed (rather than
        giving up) while the answer is still unknown — during the CC102
        snapshot the /menu echo can easily land before the /xxx_state echoes.
        Returns True if the view was restored."""
        if self.restore_effect is None:
            return False
        if self.restore_menu != self.last_menu_index:
            self.restore_effect = self.restore_menu = None
            return False
        if self.bypass_states.get(self.restore_effect, 1) != 0:
            return False                # off, or not reported yet — keep waiting
        code = self.restore_effect
        self.restore_effect = self.restore_menu = None
        self.enter_effect_view(code)
        return True

    def handle_preset_change(self, preset_index):
        self._touch()
        # Arm the window BEFORE the early-out: the bypass burst follows a
        # recall even when the index is unchanged (re-pressing the same foot
        # preset re-applies it), and that burst is exactly what must not move
        # the view.
        self.preset_recall_until = time.time() + 1.5
        if self.param_values.get('/preset') == preset_index:
            return
        self.param_values['/preset'] = preset_index
        if self.current_view == 'menu0':
            self.dirty = True

    def handle_preset_bank_change(self, bank):
        self._touch()
        bank = max(0, min(2, int(bank)))     # 0=surf 1=bass 2=gorcore
        if bank == self.preset_bank:
            return
        self.preset_bank = bank
        if self.current_view == 'menu0':
            self.dirty = True

    def _menu_effects(self, menu_idx):
        """Effects for a menu, bank-aware for the mode-switched PRESETS menu.
        The NAM menu's slot labels are live: each names the amp currently
        selected in that row (follows the encoder via /nam_sel<r>)."""
        if menu_idx >= len(MENUS):
            # HG_MENU (21) sits one PAST MENUS — the hyperGuitar page draws
            # lanes from HG_LANES, not effect codes, so it holds no effect
            # slots and the honest answer is "none". Without this, every
            # caller that asks "is this effect on the view I'm showing?"
            # (handle_state_change, both branches) raised IndexError while
            # that page was up — inside route_osc, which the main loop calls
            # unguarded, so the FIRST effect toggle with the guitar plugged in
            # took the whole screen down mid-set.
            return []
        if menu_idx == 0:
            return (SURF_PRESETS, BASS_PRESETS,
                    GORCORE_PRESETS)[self.preset_bank]
        if menu_idx == NAM_MENU and NAM_ROWS:
            out = []
            for i, fallback in enumerate(MENUS[NAM_MENU]["effects"]):
                variants = NAM_ROWS.get(i)
                if variants:
                    sel = min(max(self.nam_sel[i], 0), len(variants) - 1)
                    out.append((f"nam{i}", variants[sel]))
                else:
                    out.append(fallback)
            return out
        if menu_idx == IR_MENU and IR_ROWS:
            out = []
            for slot, fallback in enumerate(MENUS[IR_MENU]["effects"]):
                row = int(fallback[0][2:])        # ("ir4", ...) -> row 4
                variants = IR_ROWS.get(row)
                if variants:
                    sel = min(max(self.ir_sel[row], 0), len(variants) - 1)
                    out.append((f"ir{row}", variants[sel]))
                else:
                    out.append(fallback)
            return out
        return MENUS[menu_idx]["effects"]

    def enter_effect_view(self, effect_code):
        self._cancel_pending_switch()
        # Effect may have been bypassed during the invert dwell.
        if self.bypass_states.get(effect_code, 1) != 0:
            return
        self.current_view = effect_code
        self.title_text = ALL_EFFECTS.get(effect_code, effect_code.upper())
        # NAM slots title with the actual amp selected in that row.
        if effect_code.startswith('nam') and effect_code[3:].isdigit():
            variants = NAM_ROWS.get(int(effect_code[3:]))
            if variants:
                sel = min(max(self.nam_sel[int(effect_code[3:])], 0), len(variants) - 1)
                self.title_text = variants[sel]
        # IR VERB slots title with the actual IR selected in that row.
        if effect_code.startswith('ir') and effect_code[2:].isdigit():
            variants = IR_ROWS.get(int(effect_code[2:]))
            if variants:
                sel = min(max(self.ir_sel[int(effect_code[2:])], 0), len(variants) - 1)
                self.title_text = variants[sel]
        self.dirty = True

    def handle_state_change(self, effect_code, bypass_value):
        """Effect state changed. 0=ON, 1=BYPASSED."""
        self._touch()
        new_bypass = int(bypass_value)
        if self.bypass_states.get(effect_code) == new_bypass:
            return
        self.bypass_states[effect_code] = new_bypass

        if time.time() < self.sync_quiet_until:
            self.dirty = True
            return                      # sync catch-up: state only, no nav

        if new_bypass == 0:
            self._cancel_pending_switch()
            if time.time() < self.preset_recall_until:
                # Preset recall in flight: record the state, repaint, but do
                # NOT navigate. See preset_recall_until.
                self.dirty = True
                return
            # If the effect's slot is visible on the current menu, invert it
            # first and dwell briefly before glitching into the submenu.
            view = self.current_view
            on_visible_menu = (
                view.startswith('menu')
                and any(c == effect_code for c, _ in self._menu_effects(int(view[4:])))
            )
            if on_visible_menu:
                self.dirty = True
                self.pending_switch_code = effect_code
                self.pending_switch_at = time.time() + INVERT_DWELL
            else:
                self.enter_effect_view(effect_code)
        else:
            if self.pending_switch_code == effect_code:
                self._cancel_pending_switch()
            # Bypassed — if we were viewing this effect's params, STAY on the
            # view showing the off-state (dimmed sliders, power ring off);
            # 2026-08-09 field-test call: closing the view on bypass was the
            # surprising behavior. Back/menu nav leaves it as before.
            if self.current_view == effect_code:
                self.dirty = True
            elif self.current_view.startswith('menu'):
                menu_idx = int(self.current_view[4:])
                if any(c == effect_code for c, _ in self._menu_effects(menu_idx)):
                    self.dirty = True
            elif self.current_view in EFFECT_PARAMS:
                # 2026-09-15: "toggle effects on the neptr body and the ui
                # doesn't reflect it" — the encoders, not the screen.
                #
                # Both surfaces emit the SAME physical CC into the SAME midiin
                # (handler_remap.orc's table and _PHYS_FOR_BTN are exact
                # inverses), so the engine cannot tell them apart and the
                # broadcast always arrives. What differed was WHERE the UI was
                # standing when it landed. A toggle-ON parks us on that
                # effect's param page (enter_effect_view, just above) while the
                # engine stays on its menu, so from the second encoder press
                # onward the two tests above are asking about a view the engine
                # never heard of: not this effect, not a menu -> bypass_states
                # was updated and NOTHING was drawn. A finger cannot reach that
                # state, because a slot tap is only possible from the menu you
                # are already looking at — which is exactly why this read as
                # "the UI only tracks the touchscreen".
                #
                # Fall back to the engine's own menu (last_menu_index is
                # written only by the /menu echo, same idiom as the back
                # action): its slot grid is the one surface that shows every
                # slot's on/off at once, and it mirrors what toggle-ON already
                # does. Only EFFECT pages are left this way — a modal, the WAVE
                # editor or the companion must not be yanked out from under the
                # player by a footswitch.
                self.handle_menu_change(self.last_menu_index)
                self.dirty = True

    def update_param(self, osc_addr, value):
        self._touch()
        prev = self.param_values.get(osc_addr)
        if prev is not None:
            try:
                if float(prev) == float(value):
                    return
            except (TypeError, ValueError):
                if prev == value:
                    return
        self.param_values[osc_addr] = value
        view = self.current_view
        if view in EFFECT_PARAMS and osc_addr.startswith(f"/{view}_"):
            self.dirty = True

    # ----- sleep / wake ----------------------------------------------------

    def is_looper_recording(self):
        """True while ANY looper channel is physically capturing audio right
        now. States 1 (REC), 5 (sync-close) and 6 (trim-close) are ALL still
        writing into the buffer — looper.orc keeps recording through both
        close states until they finalize (see effects/looper.orc's state
        machine comment at the top of the file). TASK 3c: the faces/sleep
        screen must never come up mid-take."""
        return any(s in (1, 5, 6) for s in self.lp_states)

    def _beat_phase(self, now):
        """0..1 phase since the last beat edge this UI received, for the
        metro dot's local predictor (TASK 2/3a) — or None if the beat clock
        isn't currently live (never seen one, or it's gone stale: the engine
        is down, or the clock hasn't ticked in a while). Deliberately keyed
        off OUR OWN receipt wall-clock time (beat_anchor_time), not an
        engine-side timestamp, so a late or dropped /beat_idx just means the
        NEXT edge re-anchors cleanly — the dot never drifts, it just waits."""
        if self.beat_anchor_time is None:
            return None
        if now - self.beat_seen > BEAT_LIVE_TIMEOUT:
            return None
        period = max(0.05, self.beat_period)
        return ((now - self.beat_anchor_time) % period) / period

    def enter_sleep(self):
        self.is_sleeping = True
        self.trans_blocks = []
        self.trans_next_at = None
        self._cancel_pending_switch()
        self.face_current = 0
        self.face_showing_other = False
        self.face_check_at = time.time() + self.face_interval
        self.face_revert_at = None
        self.dirty = True

    def wake_up(self):
        self.is_sleeping = False
        self.face_check_at = None
        self.face_revert_at = None
        self.face_current = 0
        self.face_showing_other = False
        # Force a fresh render on wake.
        self._last_rendered_view = None
        self._last_rendered_signature = None
        self.dirty = True

    def tick_faces(self, now):
        """Face switching while sleeping. Returns True if the face changed."""
        changed = False
        if self.face_revert_at is not None and now >= self.face_revert_at:
            self.face_current = 0
            self.face_showing_other = False
            self.face_revert_at = None
            changed = True
        if self.face_check_at is not None and now >= self.face_check_at:
            self.face_check_at = now + self.face_interval
            if not self.face_showing_other and self.face_revert_at is None:
                if random.random() < self.other_face_probability:
                    self.face_showing_other = True
                    self.face_current = random.randint(2, 6)   # FACE3..FACE7
                    self.face_revert_at = now + self.other_face_duration
                    changed = True
                elif random.random() < self.face2_probability:
                    self.face_current = 1                       # FACE2
                    self.face_revert_at = now + self.face2_duration
                    changed = True
        return changed

    # ----- touch input -----------------------------------------------------
    #
    # touch_down/move/up receive CANVAS coordinates (TouchMapper already
    # applied cal + rotation). Taps fire the hit-region registered during the
    # last render; drags drive the wave editor / harmonic bars / list scroll.
    # Everything here is ADDITIVE — the MIDI companion gestures and the OSC
    # view logic are untouched.

    # Gesture model (2026-08-09 field-test rework): touch events are
    # note-on/note-off. Finger-DOWN only *starts* a candidate gesture;
    # movement past the swipe threshold CLASSIFIES it as a swipe (paging
    # runs, and no tap action ever fires); a release inside TAP_SLOP
    # classifies it as a tap, which fires ON THE RELEASE — instantly.
    # Nothing fires on the down edge. The ONE deliberate exception: LOOPER
    # rows map finger-down/up to transport press/release CCs, because the
    # engine's rec/close/hold classifier needs true press timing.

    def touch_down(self, x, y):
        now = time.time()
        if self.is_sleeping:
            self.wake_up()               # a tap only wakes; swallowed
            self.last_osc_time = now
            self.touch_start = None
            return
        self.last_osc_time = now         # defer the sleep countdown
        self.companion.touch_activity(now)
        if self.sm.sheet is not None and not self.companion.active:
            self.sm.sheet_down(x, y, now)    # SoundMagic deep sheet is modal
            self.drag_target = 'smsheet'
            self.touch_start = None
            self.dirty = True
            return
        self.touch_start = (x, y)
        self.touch_pos = (x, y)
        self.touch_moved = False
        self.drag_target = None
        self._scroll_accum = 0
        self._nav_start = (x, y)
        self._nav_axis = None
        if self.on_wave() and self._wave_rects:
            wr, br = self._wave_rects
            if wr.collidepoint(x, y):
                self.drag_target = 'wave'
                self.wave.begin_stroke()
                self.wt_send_at = None
                self._feed_wave_point(x, y)
                self.dirty = True
                return
            if br.collidepoint(x, y):
                self.drag_target = 'bars'
                self.wave.begin_bar_stroke()
                self.wt_send_at = None
                self._feed_bar_point(x, y)
                self.dirty = True
                return
        if not self.companion.active and self.variant_list is None:
            # UTILITY page: the master volume / BPM sliders own the gesture
            # (absolute ch16 CC sends, like the expr strip but with an OSC echo).
            if self.current_view == f'menu{NORRIS_MENU}' \
                    and self.sm.touch_down(x, y, now):
                self.drag_target = 'sm'
                self.touch_start = None
                self.dirty = True
                return
            if self.current_view == f'menu{UTILITY_MENU}':
                if self._mvol_rect is not None \
                        and self._mvol_rect.inflate(16, 28).collidepoint(x, y):
                    self.drag_target = 'mvol'
                    self._mvol_send(x)
                    self.touch_start = None
                    return
                if self._mbpm_rect is not None \
                        and self._mbpm_rect.inflate(16, 28).collidepoint(x, y):
                    self.drag_target = 'mbpm'
                    self._mbpm_send(x)
                    self.touch_start = None
                    return
            # THE looper exception: the finger IS the hardware button —
            # press CC now, release CC at finger-up, so tap/double-tap/
            # 0.4s-hold reach the engine's classifier with true timing.
            if self.current_view == f'menu{LOOPER_MENU}':
                for rect, action in self.hit:
                    if action[0] == 'lp' and rect.collidepoint(x, y):
                        self._lp_touch_cc = action[1]
                        self.engine.press(self._lp_touch_cc)
                        self.press_fx = (x, y, now)
                        self._press_mark(rect, action)
                        self.touch_start = None      # up only releases
                        return
            # PRESETS page, GORCORE bank ONLY: arm a HOLD on a slot. Finger-up
            # resolves it (tap = recall, 5s = capture, 8s = delete — see
            # touch_up). Surf/bass keep their factory-preset select (fall through
            # to the normal tap). Guarded so any failure falls back to the tap.
            if self.current_view == f'menu{PRESETS_MENU}' \
                    and self.preset_bank == 2 \
                    and _preset_gesture is not None:
                try:
                    for rect, action in self.hit:
                        if action[0] == 'slot' and rect.collidepoint(x, y):
                            self._preset_hold = (now, action[2], action, rect)
                            self.press_fx = (x, y, now)
                            self._press_mark(rect, action)
                            self.touch_start = None   # up resolves the gesture
                            return
                except Exception:
                    self._preset_hold = None          # never break touch
            # Expression-pedal strip (menu views): the slider owns the
            # gesture — a tap jumps the pedal there, a drag rides it.
            if self.current_view.startswith('menu') \
                    and self._expr_rect is not None \
                    and self._expr_rect.inflate(16, 28).collidepoint(x, y):
                self.drag_target = 'expr'
                self._expr_send(x)
                self.touch_start = None
                return
            # Param sliders: grab the slider surface (it owns the gesture).
            if self.current_view in EFFECT_PARAMS:
                for rect, name, lo, hi in self._param_hits:
                    if rect.collidepoint(x, y):
                        self._param_grab(name, lo, hi, rect, x, y, now)
                        return
        # Everything else is a swipe/tap CANDIDATE: arm the nav classifier
        # and light the touched element's pressed-state NOW (zero-latency
        # acknowledgment; the release decides the behavior).
        # 'main' = normal menu views (horizontal swipe pages the 19 menus);
        # a companion screen number = tab swipe + list scroll.
        self.sm.on_press(None, now)
        for rect, action in self.hit:
            if rect.collidepoint(x, y):
                self._press_mark(rect, action)
                self.sm.on_press(action, now)
                break
        if self.patcher:
            # Modal overlay: a drag scrolls the patcher's own list. Without
            # this the gesture would reach the tab bar behind the overlay and
            # switch tabs under it.
            self.drag_target = ('gscroll', 'patcher')
        elif self.variant_list is not None:
            # Same modal-scroll treatment for the NAM/IR variant list, keyed
            # under its own list_scroll slot so a drag here can never bleed
            # into the patcher's or a companion screen's scroll position.
            self.drag_target = ('gscroll', 'vlist')
        elif self.companion.active:
            self.drag_target = ('nav', self.companion.screen)
        elif self.current_view.startswith('menu'):
            self.drag_target = ('nav', 'main')

    # ----- pressed-state visual (down = highlight, release = behavior) -----

    def _press_mark(self, rect, action):
        self.pressed_hit = (rect.copy(), action)
        self.pressed_fade = None
        self.dirty = True

    def _press_cancel(self):
        """The gesture became a swipe/drag — fade the highlight, no action."""
        if self.pressed_hit is not None:
            self.pressed_fade = (self.pressed_hit[0], time.time())
            self.pressed_hit = None
            self.dirty = True

    def _press_commit(self):
        """A tap fired — the highlight hands off to the action's own visual
        (view change / engine-confirmed state)."""
        self.pressed_hit = None
        self.pressed_fade = None

    def _pressed(self, action):
        """Is this exact hit-action currently under the finger? Renderers use
        it to draw the element in its WOULD-BE state (an OFF slot draws as
        ON, a tab as selected) — the full outcome preview, not just a glow."""
        return self.pressed_hit is not None and self.pressed_hit[1] == action

    def _pressed_token(self):
        """Repaint-dedup token for the pressed/fading highlight."""
        if self.pressed_hit is not None:
            return ('p', tuple(self.pressed_hit[0]))
        if self.pressed_fade is not None:
            return ('f', round((time.time() - self.pressed_fade[1]) * 20))
        return None

    def _draw_pressed(self, c):
        """Accent overlay on the touched element. Solid while held; quick
        fade when the gesture resolved to a swipe."""
        if self.pressed_hit is not None:
            rect, k = self.pressed_hit[0], 1.0
        elif self.pressed_fade is not None:
            rect, t0 = self.pressed_fade
            k = 1.0 - (time.time() - t0) / PRESSED_FADE_SECS
            if k <= 0:
                self.pressed_fade = None
                return
        else:
            return
        s = pygame.Surface(rect.size, pygame.SRCALPHA)
        s.fill((ACCENT[0], ACCENT[1], ACCENT[2], int(52 * k)))
        c.blit(s, rect.topleft)
        pygame.draw.rect(c, ACCENT, rect, max(2, int(4 * k)),
                         border_radius=RADIUS)

    def touch_move(self, x, y):
        if self.touch_start is None:
            if isinstance(self.drag_target, tuple) \
                    and self.drag_target[0] == 'param':
                self._param_track(x, y)
            elif self.drag_target == 'expr':
                self._expr_send(x)
            elif self.drag_target == 'mvol':
                self._mvol_send(x)
            elif self.drag_target == 'mbpm':
                self._mbpm_send(x)
            elif self.drag_target == 'sm':
                self.sm.drag_move(x, y)
                self.dirty = True
            elif self.drag_target == 'smsheet':
                if self.sm.sheet_move(x, y):
                    self.dirty = True
            return
        self.touch_pos = (x, y)
        if (abs(x - self.touch_start[0]) > TAP_SLOP
                or abs(y - self.touch_start[1]) > TAP_SLOP):
            self.touch_moved = True
        # A tap can no longer fire once the finger moved past slop or slid
        # off the element — revert the pressed preview (quick fade).
        if self.pressed_hit is not None and (
                self.touch_moved
                or not self.pressed_hit[0].collidepoint(x, y)):
            self._press_cancel()
        if self.drag_target == 'wave':
            self._feed_wave_point(x, y)
            self.dirty = True
        elif self.drag_target == 'bars':
            self._feed_bar_point(x, y)
            self.dirty = True
        elif (isinstance(self.drag_target, tuple)
              and self.drag_target[0] == 'gscroll' and self.touch_moved):
            # Vertical only — there is no tab to swipe to from inside a modal.
            key = self.drag_target[1]
            self._scroll_accum += y - self.touch_start[1]
            self.touch_start = (self.touch_start[0], y)
            rows = -self._scroll_accum // SCROLL_STEP
            if rows:
                self._scroll_accum += rows * SCROLL_STEP
                cur = self.list_scroll.get(key, 0)
                self.list_scroll[key] = max(0, cur + rows)
                self.companion.touch_activity(time.time())
                self.dirty = True
        elif (isinstance(self.drag_target, tuple)
              and self.drag_target[0] == 'nav' and self.touch_moved):
            screen = self.drag_target[1]
            dx = x - self._nav_start[0]
            dy = y - self._nav_start[1]
            if screen == 'main':
                # Menu paging: swipe left = next menu, right = previous —
                # the engine's own nav CCs, so /menu drives the view change.
                # No axis lock here (vertical means nothing on a menu view):
                # any horizontally-dominant travel past the threshold pages.
                if abs(dx) >= SWIPE_PX and abs(dx) >= abs(dy):
                    if TOUCH_DEBUG:
                        log.info("gesture: SWIPE %s dx=%d dy=%d -> menu %s",
                                 "left" if dx < 0 else "right", dx, dy,
                                 "next" if dx < 0 else "prev")
                    self.engine.tap(74 if dx < 0 else 76)   # phys next/prev
                    self.drag_target = None
                    self.touch_start = None
                return
            # Companion screens: decide scroll-vs-swipe only after real
            # commitment travel (AXIS_COMMIT_PX), so resistive contact
            # jitter at finger-down can never lock a swipe into 'v'.
            if self._nav_axis is None \
                    and max(abs(dx), abs(dy)) >= AXIS_COMMIT_PX:
                self._nav_axis = 'h' if abs(dx) >= abs(dy) else 'v'
                if TOUCH_DEBUG:
                    log.info("gesture: axis=%s dx=%d dy=%d screen=%s",
                             self._nav_axis, dx, dy, screen)
            if self._nav_axis == 'v' and screen in (1, 3):
                self._scroll_accum += y - self.touch_start[1]
                self.touch_start = (self.touch_start[0], y)
                rows = -self._scroll_accum // SCROLL_STEP
                if rows:
                    self._scroll_accum += rows * SCROLL_STEP
                    cur = self.list_scroll.get(screen, 0)
                    self.list_scroll[screen] = max(0, cur + rows)
                    self.companion.touch_activity(time.time())
                    self.dirty = True
            elif self._nav_axis == 'h' and abs(dx) >= SWIPE_PX:
                # Swipe commits mid-gesture (feels immediate). Content
                # follows the finger: swipe left -> next tab.
                now = time.time()
                target = screen + (1 if dx < 0 else -1)
                if 0 <= target < len(companion_mod.SCREENS):
                    evs = self.companion.set_screen(target, now)
                    if evs:
                        self._arm_slide(1 if target > screen else -1)
                        self.list_scroll.pop(screen, None)
                        self._apply_companion_events(evs, now)
                self.drag_target = None
                self.touch_start = None              # gesture consumed

    # Onboard PRESETS page rows 0..4 map to preset-bank slots 13..17 (foot
    # DUODECIMUS owns 1..12). The touchscreen drives the SAME in-engine bank as
    # the foot, via ch16 CCs with the slot in the value: 106=recall (tap),
    # 105=capture (5s hold), 107=delete (8s hold). No daemon / no CLI.
    PRESET_ONBOARD_BASE = 13
    PRESET_CAPTURE_CC = 105
    PRESET_RECALL_CC = 106
    PRESET_DELETE_CC = 107

    def _preset_bank_op(self, outcome, slot):
        """Send the in-engine bank op for a PRESETS-page gesture (slot 13..17)."""
        cc = {"select": self.PRESET_RECALL_CC,
              "capture": self.PRESET_CAPTURE_CC,
              "delete": self.PRESET_DELETE_CC}.get(outcome)
        if cc is not None:
            self.engine.cc(cc, slot, channel=companion_input.UTILITY_CHANNEL)

    def touch_up(self, x, y):
        now = time.time()
        if self._preset_hold is not None:            # PRESETS-page hold resolve
            start, row, action, _rect = self._preset_hold
            self._preset_hold = None
            self._press_commit()
            self.last_osc_time = now
            self.dirty = True
            try:
                outcome = _preset_gesture.classify(now - start)
                slot = self.PRESET_ONBOARD_BASE + row   # rows 0..4 -> slots 13..17
                # tap=recall, 5s=capture, 8s=delete — all hit the in-engine bank
                self._preset_bank_op(outcome, slot)
            except Exception:
                pass
            return
        if self._lp_touch_cc is not None:            # looper row release
            self.engine.release(self._lp_touch_cc)
            self._lp_touch_cc = None
            self._press_commit()
            self.dirty = True
            self.last_osc_time = now
            return
        if self.drag_target == 'expr':
            self._expr_send(x, final=True)        # land the exact value
            self.drag_target = None
            self.touch_pos = None
            self.last_osc_time = now
            return
        if self.drag_target == 'smsheet':
            self.sm.sheet_up(x, y)
            self.drag_target = None
            self.touch_pos = None
            self.last_osc_time = now
            self.dirty = True
            return
        if self.drag_target == 'sm':
            self.sm.drag_up(x, y)
            self.drag_target = None
            self.touch_pos = None
            self.last_osc_time = now
            self.dirty = True
            return
        if self.sm.consume_lp_fired():             # long-press opened the sheet
            self._press_cancel()
            self.drag_target = None
            self.touch_pos = None
            self.touch_start = None
            self.dirty = True
            return
        if self.drag_target in ('mvol', 'mbpm'):
            (self._mvol_send if self.drag_target == 'mvol'
             else self._mbpm_send)(x, final=True)
            self.drag_target = None
            self.touch_pos = None
            self.last_osc_time = now
            return
        if isinstance(self.drag_target, tuple) and self.drag_target[0] == 'param':
            # keep chasing briefly so the value settles on the last target
            if self.param_chase is not None:
                self.param_chase['until'] = now + PARAM_CHASE_LINGER
            self.drag_target = None
            self.touch_pos = None
            self.touch_start = None
            self.last_osc_time = now
            return
        if self.touch_start is None:
            return
        self.last_osc_time = now
        drag = self.drag_target
        self.drag_target = None
        self.touch_pos = None
        if drag == 'wave':
            self.wave.end_stroke()
            self.wt_send_at = now + WAVE_SEND_DEBOUNCE
            self.dirty = True
            self.touch_start = None
            return
        if drag == 'bars':
            self.wave.end_bar_stroke()
            self.wt_send_at = now + WAVE_SEND_DEBOUNCE
            self.dirty = True
            self.touch_start = None
            return
        fired = False
        if not self.touch_moved:
            for rect, action in self.hit:
                if rect.collidepoint(x, y):
                    if TOUCH_DEBUG:
                        log.info("gesture: TAP %s at (%d,%d)", action, x, y)
                    self.press_fx = (x, y, now)
                    self._press_commit()          # highlight hands off
                    self._fire(action, now)
                    fired = True
                    break
        if not fired:
            self._press_cancel()
        self.touch_start = None

    def _fire(self, action, now):
        """Dispatch a tap on a registered hit region."""
        kind = action[0]
        if kind == 'companion_toggle':
            self._apply_companion_events(self.companion.toggle(now), now)
        elif kind == 'tab':
            prev = self.companion.screen
            evs = self.companion.set_screen(action[1], now)
            if evs:
                self._arm_slide(1 if action[1] > prev else -1)
                self.list_scroll.pop(prev, None)
            self._apply_companion_events(evs, now)
        elif kind == 'item':
            _, screen, idx = action
            self.companion.set_sel(idx, now)
            self._companion_activate(screen, idx, now)
            self.dirty = True
        elif kind == 'slot':
            _, menu_idx, row, code = action
            # Symmetric toggle: EVERY slot tap emits the slot's encoder-
            # button CC, exactly as the Teensy would. OFF -> the engine
            # turns it on and its OSC echo opens the param view (the
            # existing invert-dwell flow); ON -> the engine bypasses it.
            # NO local state change — the slot flips only when the engine's
            # state broadcast comes back (engine-authoritative, no flicker).
            self.engine.tap(slot_button_cc(menu_idx, row))
        elif kind in ('sm_tile', 'sm_name', 'sm_power'):
            self.sm.tap(action)
            self.dirty = True
        elif kind == 'hg_slot':
            # HG_ROW_SLOT reverses row->slot: see its definition. The engine's
            # menu-20 branch forwards this to the guitar as /hg/lane.
            self.engine.tap(slot_button_cc(HG_MENU, HG_ROW_SLOT[action[1]]))
        elif kind == 'hg_rot':
            _, row, up = action
            # lane 0 has no encoder behaviour — the pedal drives it
            if row == 0:
                return
            self.engine.rotate(slot_rotate_cc(HG_MENU, HG_ROW_SLOT[row]), up)
        elif kind == 'fx_toggle':
            # Power button in the effect view: bypass the effect being
            # viewed. Same CC as its slot button on its home menu (the
            # engine's current menu — the view was entered from it).
            _, code = action
            for row, (c, _n) in enumerate(
                    self._menu_effects(self.last_menu_index)):
                if c == code:
                    self.engine.tap(slot_button_cc(self.last_menu_index, row))
                    break
        elif kind == 'wave_open':
            self.enter_wave()
        elif kind == 'pick':
            self._pick_value(action[1], now)
        elif kind == 'pick_cancel':
            self._close_picker()
        elif kind == 'gport':
            self._graph_tap_port(action[1], now)
        elif kind == 'glink':
            self._graph_tap_link(action[1], action[2], now)
        elif kind == 'greset':
            self._graph_reset(now)
        elif kind == 'gclose':
            self._close_patcher()
        elif kind == 'vlist_open':
            _, menu_idx, row = action
            self.variant_list = {'menu': menu_idx, 'row': row}
            self.list_scroll.pop('vlist', None)
            self.dirty = True
        elif kind == 'vlist_close':
            self.variant_list = None
            self.list_scroll.pop('vlist', None)
            self.dirty = True
        elif kind == 'vlist_pick':
            self._variant_pick(action[1], now)
        elif kind == 'rec_toggle':
            self._toggle_record(now)
        elif kind == 'mvd_reps':
            self._mvd_reps_pick(action[1], now)
        elif kind == 'mvd_intv':
            self._mvd_intv_step(action[1], now)
        elif kind == 'kb':
            self._kb_key(action[1], now)
        elif kind == 'net_rescan':
            self._set_msg('scanning…', now)
            self._refresh_wifi(now, rescan=True)
        elif kind == 'net_disconnect':
            self._disconnect_network(now)
        elif kind == 'back':
            if self.current_view == WAVE_VIEW:
                self.leave_wave()
            elif self.current_view not in ('menu%d' % self.last_menu_index,):
                self.current_view = f'menu{self.last_menu_index}'
                self.title_text = MENUS[self.last_menu_index]["name"]
                self.dirty = True

    # ----- wave-editor touch geometry --------------------------------------

    # Out-of-bounds clamp gestures (2026-08-09): X always tracks the finger,
    # even when it leaves the rect vertically mid-stroke. BELOW the surface
    # writes 0 for every X swept (a swift bottom-edge drag IS the clear — the
    # buttons are gone), ABOVE writes max. In and out mid-stroke stays one
    # continuous stroke (the wavetable interpolators never see a gap).

    def _feed_wave_point(self, x, y):
        wr, _ = self._wave_rects
        phase = (x - wr.x) / max(1, wr.width - 1)
        if y > wr.bottom:
            amp = 0.0                      # below the graph = flatten to zero
        elif y < wr.y:
            amp = 1.0                      # above the graph = full positive
        else:
            amp = 1.0 - 2.0 * (y - wr.y) / max(1, wr.height - 1)
        self.wave.add_point(max(0.0, min(1.0, phase)), amp)
        self._trail_add(x, min(max(y, wr.y), wr.bottom))

    def _feed_bar_point(self, x, y):
        _, br = self._wave_rects
        frac = (x - br.x) / max(1, br.width - 1)
        k = wavetable.bar_at(frac)
        # below = silent bar, above = full bar; between = linear (the clamp
        # falls out of the same formula)
        mag = 1.0 - (y - br.y) / max(1, br.height - 1)
        self.wave.stroke_magnitude(k, max(0.0, min(1.0, mag)))
        self._trail_add(x, min(max(y, br.y), br.bottom))

    def _trail_add(self, x, y):
        self._stroke_trail.append((x, y, time.time()))

    def _trail_prune(self, now):
        self._stroke_trail = [(x, y, t) for x, y, t in self._stroke_trail
                              if now - t < TRAIL_SECS]

    # ----- param sliders: finger drag -> encoder rotation pulses ------------
    #
    # The engine's rotation dispatch is RELATIVE (CW=127/CCW=0 per detent)
    # and gkLastToggle-keyed, so the slider CHASES the finger: at most one
    # pulse per PARAM_TICK (~30 msg/s, the touch-daemon-style rate cap) in
    # the direction of the target, until the OSC-echoed value lands inside
    # the deadband. The thumb only ever draws the engine-confirmed value;
    # the finger target shows as a thin marker line while chasing.

    def _param_grab(self, name, lo, hi, rect, x, y, now):
        view = self.current_view
        mapping = param_rot_cc(view, name)
        if mapping is None:
            return                          # no encoder path: slider is inert
        phys, toggle_id = mapping
        if self.last_toggle != toggle_id:
            # The engine's encoders are armed to a different effect; a pulse
            # would edit THAT one. Surface why the drag is ignored.
            self.param_hint_until = now + 2.5
            self.dirty = True
            return
        self.drag_target = ('param', name)
        self.param_chase = dict(code=view, param=name, phys=phys,
                                lo=lo, hi=hi, rect=rect.copy(),
                                target=self._param_value_at(rect, lo, hi, y),
                                until=None)
        self.touch_start = None             # slider owns the gesture
        self.press_fx = (x, y, now)
        self.dirty = True

    def _param_value_at(self, rect, lo, hi, y):
        frac = 1.0 - (y - rect.y) / max(1, rect.height - 1)
        return lo + (hi - lo) * max(0.0, min(1.0, frac))

    def _param_track(self, x, y):
        ch = self.param_chase
        if ch is None:
            return
        ch['target'] = self._param_value_at(ch['rect'], ch['lo'], ch['hi'], y)
        self.dirty = True

    def tick_param_chase(self, now):
        """Drive the engine value to the finger target in real time. The
        encoder path is the only write path (relative detents), so per tick
        (~30 Hz) a BURST of pulses proportional to the remaining error goes
        out — up to PARAM_BURST detents — which tracks a full-range flick in
        a couple hundred ms. While the finger is down the slider DRAWS the
        finger target directly (no rubber-banding); the engine echo only
        reconciles after release."""
        ch = self.param_chase
        if ch is None:
            return
        dragging = (isinstance(self.drag_target, tuple)
                    and self.drag_target[0] == 'param')
        if not dragging:
            if ch['until'] is None or now >= ch['until'] \
                    or self.current_view != ch['code']:
                self.param_chase = None
                self.dirty = True
                return
        cur = self.param_values.get(f"/{ch['code']}_{ch['param']}", ch['lo'])
        try:
            cur = float(cur)
        except (TypeError, ValueError):
            cur = float(ch['lo'])
        err = ch['target'] - cur
        span = (ch['hi'] - ch['lo']) or 1
        if abs(err) <= abs(span) * PARAM_DEADBAND:
            if not dragging:
                self.param_chase = None
                self.dirty = True
            return
        if self.param_tick_at is not None and now < self.param_tick_at:
            return
        self.param_tick_at = now + PARAM_TICK
        # ~50 detents/span is the engine's dominant $ENC_ROT convention
        # (0.02 steps on 0..1); burst enough detents to close the error.
        #
        # BUT stepped params (STEP_LABELS: discrete positions like
        # TAPE DELAY's "head", SHIMMER's "intv", ...)
        # move by ONE WHOLE STEP per encoder pulse on the engine side, not
        # span/50 — e.g. a span=15 param over 16 labels, so one detent
        # = a whole mode. Assuming span/50 there computed a step_est of
        # 15/50=0.3, which is ~50x too small: every burst wildly overshot
        # the target and the slider chase oscillated past it and back
        # every tick — this was the "MOVE DELAY sliders tweaking out" root
        # cause (2026-08-23 diagnosis, reproduced on the old mvd 'div'/'mode'
        # stepped params). Fixed by
        # deriving the true per-detent size from the label count when one
        # exists: span/(n_labels-1) — this exactly matches every stepped
        # param's actual $ENC_ROT step in the .orc handlers (verified
        # against tdl/head, dly/mode, pll/intv, rpt/len,
        # fbk/harm).
        labels = STEP_LABELS.get((ch['code'], ch['param']))
        if labels and len(labels) > 1:
            step_est = abs(span) / (len(labels) - 1)
        else:
            # exact engine step for the params whose real detent is bigger than
            # span/50 (blooper + the prob/mix/oct family) — using span/50 there
            # overshot and made the slider oscillate while dragging. Everything
            # else keeps span/50, which is correct (see PARAM_STEP note).
            step_est = PARAM_STEP.get((ch['code'], ch['param']), abs(span) / 50.0)
        n = max(1, min(PARAM_BURST, round(abs(err) / step_est)))
        up = (err > 0) == (span > 0)
        for _ in range(n):
            self.engine.rotate(ch['phys'], up)

    # ----- wavetable OSC out -----------------------------------------------

    def send_wavetable(self):
        """Fire the OSC burst (called from the main loop when the post-edit
        debounce deadline passes). Four message kinds — see
        DEMIURGE_OS demiurge/docs/wavetable-osc.md:
          /neptr/wavetable            ,b   2048 float32 (big-endian) blob,
                                           for blob-capable receivers
          /neptr/wavetable/chunk x128 17f  offset + 16 samples — the
                                           Csound-native path (OSClisten has
                                           no blob type; verified cs6.18)
          /neptr/wavetable/end        ,f   2048 (table complete marker)
          /neptr/wavetable/harmonics       64 + mags + phases
        UDP to localhost/LAN — cheap, done inline; failures are logged,
        never fatal."""
        self.wt_send_at = None
        try:
            if self._wt_client is None:
                self._wt_client = SimpleUDPClient(self.wt_host, self.wt_port)
            self._wt_client.send_message("/neptr/wavetable",
                                         self.wave.table_blob())
            for chunk in self.wave.chunk_args():
                self._wt_client.send_message("/neptr/wavetable/chunk", chunk)
            self._wt_client.send_message("/neptr/wavetable/end",
                                         float(wavetable.N))
            self._wt_client.send_message("/neptr/wavetable/harmonics",
                                         self.wave.harmonics_args())
            self.wt_sends += 1
            self.wt_last_send = time.time()
            if self.on_wave():
                self.dirty = True
        except OSError as exc:
            log.warning("wavetable OSC send failed: %s", exc)

    # ----- companion tab slide transition ----------------------------------

    def _arm_slide(self, direction):
        self.slide_old = self.canvas.copy()
        self.slide_new = None
        self.slide_dir = direction
        self.slide_t0 = None

    def tick_slide(self, now):
        """Advance the tab slide. Returns True if the canvas changed."""
        if self.slide_old is None or self.slide_new is None:
            return False
        if self.slide_t0 is None:
            self.slide_t0 = now
        k = (now - self.slide_t0) / SLIDE_SECS
        if k >= 1.0:
            self.canvas.blit(self.slide_new, (0, 0))
            self.slide_old = self.slide_new = None
            self.slide_t0 = None
            return True
        e = 1 - (1 - k) * (1 - k)                     # ease-out
        off = int(W * e) * self.slide_dir
        self.canvas.blit(self.slide_old, (-off, 0))
        self.canvas.blit(self.slide_new, (W * self.slide_dir - off, 0))
        return True

    # ----- rendering -------------------------------------------------------

    def _signature(self, view):
        """Hashable snapshot of everything the current view renders."""
        if view.startswith('menu'):
            menu_idx = int(view[4:])
            if menu_idx == NORRIS_MENU:
                return ('norris', self.sm.signature())
            if menu_idx == UTILITY_MENU:
                # Custom page: live tempo + master vol/bpm + record indicator.
                return ('utility', round(self.clock_bpm, 1),
                        round(self.master_out, 3), round(self.master_bpm, 1),
                        bool(self.recording))
            if menu_idx == LOOPER_MENU:
                # LOOPER view renders from lp_* state, not bypass_states —
                # without these in the signature the repaint dedup would
                # swallow every looper update after the first paint.
                return ('looper',
                        tuple(self.lp_states),
                        tuple(round(p, 2) for p in self.lp_pos),
                        tuple(self.lp_speed),
                        self.lp_drum_ch, self.lp_drum_idx, self.lp_drum_count)
            sig = tuple(
                self._slot_is_on(menu_idx, i, code)
                for i, (code, _name) in enumerate(self._menu_effects(menu_idx))
            )
            # NAM/IR menus render live variant labels — the selections must be
            # part of the signature or the repaint dedup swallows label changes.
            if menu_idx == CB_MENU:
                # CHASE BLISS: BLOOPER + HABIT paint live status badges
                return ('menu', menu_idx, self.preset_bank, sig,
                        int(self.param_values.get('/hab_rec', 1)),
                        int(self.param_values.get('/blp_active', 0)))
            if menu_idx == NAM_MENU:
                return ('menu', menu_idx, self.preset_bank, sig, tuple(self.nam_sel))
            if menu_idx == IR_MENU:
                return ('menu', menu_idx, self.preset_bank, sig, tuple(self.ir_sel))
            return ('menu', menu_idx, self.preset_bank, sig)
        elif view in EFFECT_PARAMS:
            is_active = self.bypass_states.get(view, 1) == 0
            vals = tuple(
                self.param_values.get(f"/{view}_{p[0]}", p[2])
                for p in EFFECT_PARAMS[view]
            )
            ch = self.param_chase
            chase = (round(ch['target'], 3) if ch is not None
                     and ch['code'] == view else None)
            hint = (self.param_hint_until is not None
                    and time.time() < self.param_hint_until)
            if view == 'mvd':
                # POLY DLY's top strip: repeats/interval echoes plus the tempo
                # the ms readout derives from.
                return ('effect', view, is_active, vals, chase, hint,
                        self.mvd_reps, self.mvd_intv,
                        round(self.clock_bpm, 1) if self.clock_bpm_seen else None)
            return ('effect', view, is_active, vals, chase, hint)
        elif view == WAVE_VIEW:
            # The editor repaints from self.dirty (strokes, trail, OSC sends),
            # not from a value snapshot — but it still needs its own signature
            # so the dedup never mistakes it for another view.
            return ('wave', self.wt_sends, self.wt_send_at is not None,
                    len(self._stroke_trail))
        return ('none',)

    def _slot_is_on(self, menu_idx, slot_idx, code):
        if menu_idx == 0:
            if self.preset_bank == 1:
                # Bass bank: codes are real effect codes — reuse bypass state.
                return self.bypass_states.get(code, 1) == 0
            if self.preset_bank == 2:
                # Gorcore bank: slots light from the active preset id.
                return GORCORE_SLOT_TO_PRESET.get(slot_idx, -2) \
                    == self.param_values.get('/preset', -1)
            if code in ("pha", "trm"):
                return self.bypass_states.get(code, 1) == 0
            return SLOT_TO_PRESET.get(slot_idx, -2) == self.param_values.get('/preset', -1)
        return self.bypass_states.get(code, 1) == 0

    def render(self):
        """Compose the current view onto self.canvas. Returns True if a
        repaint is actually needed (and arms the glitch on view changes)."""
        self.dirty = False
        if self.is_sleeping:
            self._render_sleep()
            self._last_rendered_view = None
            self._last_rendered_signature = None
            return True

        if self.companion.active:
            self._render_companion(self.canvas)
            if self.slide_old is not None:
                # tab slide armed: keep the fresh view aside, tick_slide
                # composes old->new; first frame still shows the old tab.
                self.slide_new = self.canvas.copy()
                self.canvas.blit(self.slide_old, (0, 0))
            self._last_rendered_view = None
            self._last_rendered_signature = None
            return True
        self.slide_old = self.slide_new = None     # slides are companion-only

        if self.variant_list is not None:
            # Modal, same treatment as the companion picker/patcher overlays
            # (_render_companion) but reachable from the normal
            # (non-companion) view: repaints the whole screen and owns every
            # hit region itself every frame, so nothing behind it -- not
            # even the stale param-slider hit rects from the effect view it
            # sits over -- can be tapped through it (see touch_down guard).
            self.hit = []
            self._render_variant_list(self.canvas)
            self._draw_pressed(self.canvas)
            self.clean = self.canvas.copy()
            self._last_rendered_view = None
            self._last_rendered_signature = None
            return True


        if self.sm.sheet is not None:
            # SoundMagic deep sheet / RACK sheet: full-screen modal.
            self.hit = []
            self.sm.render_sheet(self, self.canvas)
            self.clean = self.canvas.copy()
            self._last_rendered_view = None
            self._last_rendered_signature = None
            return True

        view = self.current_view
        signature = (self._signature(view), self._pressed_token(),
                     round(self.expr_value, 3))
        if (self._last_rendered_view == view
                and self._last_rendered_signature == signature):
            return False
        view_changed = (self._last_rendered_view is not None
                        and view != self._last_rendered_view)
        self._last_rendered_view = view
        self._last_rendered_signature = signature

        c = self.canvas
        old = c.copy() if view_changed else None
        self.hit = []
        c.blit(bg_gradient(), (0, 0))
        blit_centered(c, text_fit(self.title_text, 68, FG, W - 200), W / 2, TITLE_CENTER_Y)
        pygame.draw.line(c, OFF_OUTLINE, (0, DIVIDER_Y), (W, DIVIDER_Y), 1)
        pygame.draw.line(c, FG, (PAD_X, DIVIDER_Y), (W - PAD_X, DIVIDER_Y), 1)
        self._draw_header_buttons(c, back=(view in EFFECT_PARAMS
                                           or view == WAVE_VIEW),
                                  wave=view.startswith('menu'))

        if view == WAVE_VIEW:
            self._render_wave(c)
        elif view.startswith('menu'):
            self._render_menu(c, int(view[4:]))
        elif view in EFFECT_PARAMS:
            self._render_effect(c, view)
        self._draw_pressed(c)

        self.clean = c.copy()
        if view_changed:
            # Start the block dissolve: put the old frame back and flip
            # chunky squares of the new view in over a few frames.
            c.blit(old, (0, 0))
            self.trans_blocks = [
                (bx, by)
                for bx in range(0, W, BLOCK_SIZE)
                for by in range(0, H, BLOCK_SIZE)
            ]
            random.shuffle(self.trans_blocks)
            self._reveal_blocks()
            self.trans_next_at = time.time() + BLOCK_TRANS_DT
        else:
            # An in-place update repainted the whole view; any transition
            # still running has nothing left to reveal.
            self.trans_blocks = []
            self.trans_next_at = None
        return True

    def _draw_header_buttons(self, c, back=False, close=False, wave=False):
        """Touch affordances in the title strip: top-right '≡' opens the
        settings menu (or '✕' closes it), top-left '‹' backs out of an effect
        or WAVE view — or, on a menu view where there is nothing to back out
        of, the top-left slot carries the WAVE button instead. Generous
        ~110px hit boxes (>= 48px min target).

        `back` and `wave` are mutually exclusive by construction (back is
        passed for effect/WAVE views, wave only for menu views), so the
        top-left hit region never has two owners. Nothing else claims that
        corner: the menu grid starts below DIVIDER_Y and the expression strip
        is pinned to the bottom, so WAVE steals no existing gesture."""
        # right button
        bx = W - 76
        r = pygame.Rect(W - 116, 0, 116, DIVIDER_Y - 4)
        col = OFF_TEXT
        if close:
            pygame.draw.line(c, col, (bx + 4, 30), (bx + 40, 66), 5)
            pygame.draw.line(c, col, (bx + 4, 66), (bx + 40, 30), 5)
        else:
            for i in range(3):
                pygame.draw.rect(c, col, (bx, 32 + i * 16, 44, 6),
                                 border_radius=3)
        self.hit.append((r, ('companion_toggle',)))
        if wave:
            # WAVE: top-level destination, top-left corner. Sine glyph + word
            # so it reads as somewhere to GO, never as a back arrow.
            rw = pygame.Rect(0, 0, 128, DIVIDER_Y - 4)
            lit = self._pressed(('wave_open',))
            gcol = BG if lit else ACCENT
            if lit:
                pygame.draw.rect(c, FG, pygame.Rect(6, 4, 116, DIVIDER_Y - 14),
                                 border_radius=RADIUS)
            c.blit(wave_icon(46, gcol), (41, 20))
            lab = font(24).render("WAVE", True, gcol)
            c.blit(lab, (int(64 - lab.get_width() / 2), 76))
            self.hit.append((rw, ('wave_open',)))
        if back:
            rb = pygame.Rect(0, 0, 116, DIVIDER_Y - 4)
            pygame.draw.lines(c, col, False,
                              [(64, 26), (34, 48), (64, 70)], 6)
            self.hit.append((rb, ('back',)))
            # Power/bypass button (effect views only): toggles the viewed
            # effect via its slot's encoder-button CC. Sits left of the
            # companion button; ACCENT ring = the effect is currently ON.
            pr = pygame.Rect(W - 238, 0, 112, DIVIDER_Y - 4)
            on = self.bypass_states.get(self.current_view, 1) == 0
            pc = ACCENT if on else OFF_TEXT
            cx_, cy_ = pr.centerx, 48
            pygame.draw.circle(c, pc, (cx_, cy_ + 4), 20, 3)
            pygame.draw.line(c, pc, (cx_, cy_ - 22), (cx_, cy_ + 2), 4)
            self.hit.append((pr, ('fx_toggle', self.current_view)))

    def _render_expr(self, c):
        """Unlabeled expression-pedal slider pinned to the bottom of every
        menu view: left-to-right = heel-to-toe, absolute CC29. Thumb is
        UI-local (the pedal CC has no OSC echo)."""
        track = pygame.Rect(PAD_X + 8, CONTENT_BOTTOM - EXPR_H + 22,
                            W - 2 * (PAD_X + 8), 46)
        self._expr_rect = track
        pygame.draw.rect(c, (14, 0, 16), track, border_radius=RADIUS)
        fill_w = int((track.width - 10) * self.expr_value)
        if fill_w > 0:
            pygame.draw.rect(c, ACCENT_DIM,
                             pygame.Rect(track.x + 5, track.y + 5,
                                         fill_w, track.height - 10),
                             border_radius=RADIUS - 6)
        pygame.draw.rect(c, OFF_OUTLINE, track, 2, border_radius=RADIUS)
        tx = track.x + 5 + fill_w
        pygame.draw.rect(c, ACCENT,
                         pygame.Rect(max(track.x + 4, min(track.right - 12,
                                                          tx - 4)),
                                     track.y + 4, 8, track.height - 8),
                         border_radius=3)

    def _expr_send(self, x, final=False):
        tr = self._expr_rect
        v = max(0.0, min(1.0, (x - tr.x) / max(1, tr.width - 1)))
        self.expr_value = v
        now = time.time()
        if final or now - self._expr_sent_at >= PARAM_TICK:
            self._expr_sent_at = now
            self.engine.cc(EXPR_CC, int(round(v * 127)))
        self.dirty = True

    # ----- UTILITY page (menu 19) global controls --------------------------

    def _poll_record_status(self):
        """Refresh self.recording from `demiurge-record status` (exit 0 =
        recording, exit 1 = idle). Wrapped so a missing binary / dev machine
        never crashes or blocks the UI — a short timeout, and any failure just
        leaves the last-known state."""
        try:
            r = subprocess.run([RECORD_CMD, "status"],
                               stdout=subprocess.DEVNULL,
                               stderr=subprocess.DEVNULL,
                               timeout=2.0)
            self.recording = (r.returncode == 0)
            self.dirty = True
        except (OSError, subprocess.SubprocessError):
            pass                            # binary absent / dev box: keep state

    def _toggle_record(self, now):
        """RECORD button: fire `demiurge-record toggle` (non-blocking) and flip
        the local indicator optimistically. Never fatal if the wrapper is
        missing."""
        try:
            subprocess.Popen([RECORD_CMD, "toggle"],
                             stdout=subprocess.DEVNULL,
                             stderr=subprocess.DEVNULL)
            self.recording = not self.recording
        except OSError:
            pass                            # binary absent / dev box: no-op
        self.dirty = True

    def _master_slider_send(self, cc, x, tr, lo, hi, final=False):
        """Absolute ch16 CC from a horizontal slider position (like the expr
        strip, but on the utility channel and rate-limited together). Returns
        the mapped value in [lo, hi]."""
        frac = max(0.0, min(1.0, (x - tr.x) / max(1, tr.width - 1)))
        now = time.time()
        if final or now - self._master_sent_at >= PARAM_TICK:
            self._master_sent_at = now
            self.engine.cc(cc, int(round(frac * 127)),
                           channel=companion_input.UTILITY_CHANNEL)
        self.dirty = True
        return lo + (hi - lo) * frac

    def _mvol_send(self, x, final=False):
        if self._mvol_rect is None:
            return
        self.master_out = self._master_slider_send(
            MASTER_VOL_CC, x, self._mvol_rect, 0.0, MASTER_VOL_MAX, final)

    def _mbpm_send(self, x, final=False):
        if self._mbpm_rect is None:
            return
        v = self._master_slider_send(
            MASTER_BPM_CC, x, self._mbpm_rect, MASTER_BPM_MIN, MASTER_BPM_MAX,
            final)
        self.master_bpm = round(v)          # integer BPM

    def _render_utility(self, c):
        """Custom UTILITY page (menu 19): live TEMPO readout, MASTER volume
        slider, master BPM slider, and a RECORD toggle. No slot grid, no expr
        strip — these are global, end-of-chain controls."""
        self._expr_rect = None      # utility has no expr pedal; clear stale rect
        cx = W / 2
        # LIVE tempo — the authoritative running clock (may differ from the
        # manual BPM below when a loop or external MIDI clock is driving).
        blit_centered(c, text_fit("TEMPO", 46, OFF_TEXT, W - 80),
                      cx, CONTENT_TOP + 34)
        blit_centered(c, text_fit(("%.1f" % self.clock_bpm) if self.clock_bpm_seen else "--.-", 150, FG, W - 80),
                      cx, CONTENT_TOP + 130)

        # MASTER volume slider (0..1.5, unity 1.0).
        vol_y = CONTENT_TOP + 250
        blit_centered(c, font(48).render("MASTER", True, TRACK_ACTIVE),
                      cx, vol_y)
        vtr = pygame.Rect(PAD_X + 20, vol_y + 40, W - 2 * (PAD_X + 20), 70)
        self._mvol_rect = vtr
        frac = max(0.0, min(1.0, self.master_out / MASTER_VOL_MAX))
        self._draw_h_slider(c, vtr, frac, SLIDER_COLORS[0])
        # Unity (1.0) tick mark on the track.
        ux = vtr.x + 5 + int((vtr.width - 10) * (1.0 / MASTER_VOL_MAX))
        pygame.draw.line(c, FG, (ux, vtr.y - 6), (ux, vtr.bottom + 6), 2)
        blit_centered(c, font(40).render("%.2f" % self.master_out, True, FG),
                      cx, vtr.bottom + 34)

        # MASTER BPM slider (40..300, integer).
        bpm_y = vtr.bottom + 90
        blit_centered(c, font(48).render("BPM", True, TRACK_ACTIVE), cx, bpm_y)
        btr = pygame.Rect(PAD_X + 20, bpm_y + 40, W - 2 * (PAD_X + 20), 70)
        self._mbpm_rect = btr
        bfrac = max(0.0, min(1.0, (self.master_bpm - MASTER_BPM_MIN)
                             / (MASTER_BPM_MAX - MASTER_BPM_MIN)))
        self._draw_h_slider(c, btr, bfrac, SLIDER_COLORS[1])
        blit_centered(c, font(40).render("%d" % int(round(self.master_bpm)),
                                         True, FG), cx, btr.bottom + 34)

        # RECORD toggle — red ● REC while recording, grey ○ REC when idle.
        rec_rect = pygame.Rect(PAD_X + 40, btr.bottom + 90, W - 2 * (PAD_X + 40),
                               130)
        pressed = self._pressed(('rec_toggle',))
        rec_on = self.recording != pressed          # preview the flip on press
        if rec_on:
            pygame.draw.rect(c, (150, 24, 30), rec_rect, border_radius=RADIUS)
            dot_col, txt_col, glyph = (255, 70, 70), FG, "●"
        else:
            pygame.draw.rect(c, EMPTY_OUTLINE, rec_rect, 3,
                             border_radius=RADIUS)
            dot_col, txt_col, glyph = OFF_TEXT, OFF_TEXT, "○"
        lab = font(84).render(f"{glyph} REC", True, txt_col)
        blit_centered(c, lab, rec_rect.centerx, rec_rect.centery)
        self.hit.append((rec_rect, ('rec_toggle',)))

    def _draw_h_slider(self, c, tr, frac, color):
        """Shared horizontal fill-slider look (utility page). Matches the expr
        strip's track/fill/thumb construction."""
        pygame.draw.rect(c, (14, 0, 16), tr, border_radius=RADIUS)
        fill_w = int((tr.width - 10) * frac)
        if fill_w > 0:
            pygame.draw.rect(c, color,
                             pygame.Rect(tr.x + 5, tr.y + 5,
                                         fill_w, tr.height - 10),
                             border_radius=RADIUS - 6)
        pygame.draw.rect(c, OFF_OUTLINE, tr, 2, border_radius=RADIUS)
        tx = tr.x + 5 + fill_w
        pygame.draw.rect(c, TRACK_ACTIVE,
                         pygame.Rect(max(tr.x + 4, min(tr.right - 12, tx - 4)),
                                     tr.y + 4, 8, tr.height - 8),
                         border_radius=3)

    def _render_looper(self, c):
        """Custom renderer for the LOOPER menu (LOOPER_MENU, currently 14).
        5 channel rows."""
        area_h = CONTENT_BOTTOM - EXPR_H - CONTENT_TOP
        slot_h = (area_h - 4 * SLOT_SPACING) / 5
        STATE_LABELS = ["IDLE", "REC", "PLAY", "OVD", "STOP", "SYNC"]
        STATE_COLORS = [EMPTY_OUTLINE, (220, 40, 40), (40, 200, 80), (200, 120, 40), (120, 60, 180), (40, 160, 200)]
        DRUM_COL = (220, 170, 40)   # amber — sample mode, sits beside the state palette
        for ch in range(5):
            y = CONTENT_TOP + ch * (slot_h + SLOT_SPACING)
            rect = pygame.Rect(PAD_X + SLOT_INSET, int(y) + SLOT_INSET,
                               W - 2 * (PAD_X + SLOT_INSET),
                               int(slot_h) - 2 * SLOT_INSET)
            state = min(self.lp_states[ch], 5)
            # The row is a live transport button: finger-down/up mirror the
            # hardware encoder press/release (touch_down handles 'lp').
            self.hit.append((rect.inflate(SLOT_SPACING, SLOT_SPACING),
                             ('lp', slot_button_cc(LOOPER_MENU, ch))))
            spd_lbl = self._LP_SPEED_LABELS[max(0, min(7, self.lp_speed[ch]))]
            state_col = STATE_COLORS[state]
            drum = (ch == self.lp_drum_ch - 1 and self.lp_drum_idx >= 0)

            # Background fill
            if state in (2, 3):
                pygame.draw.rect(c, FG, rect)
                text_col = BG
                bar_bg = OFF_OUTLINE
            else:
                pygame.draw.rect(c, BG, rect)
                pygame.draw.rect(c, DRUM_COL if drum else state_col, rect, 3)
                text_col = FG if state in (1, 4, 5) else OFF_TEXT
                bar_bg = EMPTY_OUTLINE

            # Channel label (left side) — "DRUM n/N" on the drum channel
            if drum:
                n = self.lp_drum_idx + 1
                ch_lbl = f"DRUM {n}/{max(self.lp_drum_count, n)}"
                ch_col = text_col if state in (2, 3) else DRUM_COL
            else:
                ch_lbl = f"CH{ch + 1}"
                ch_col = text_col
            ch_surf = font(60).render(ch_lbl, True, ch_col)
            c.blit(ch_surf, (rect.x + 14, rect.centery - ch_surf.get_height() // 2))

            # Right side: speed label, except on the drum channel where the
            # state chip takes its place (rotation is browse there — varispeed
            # is pinned +1x, so the speed label would be dead weight).
            if drum:
                sp_surf = font(52).render(STATE_LABELS[state], True,
                                          text_col if state in (2, 3) else state_col)
            else:
                sp_surf = font(52).render(spd_lbl, True, text_col)
            c.blit(sp_surf, (rect.right - sp_surf.get_width() - 14,
                             rect.centery - sp_surf.get_height() // 2))

            # Centre: drum filename when resolvable (fitted into the gap
            # between the two labels), else the state label
            fname = self._drum_filename(self.lp_drum_idx) if drum else None
            if fname:
                x0 = rect.x + 14 + ch_surf.get_width() + 20
                x1 = rect.right - 14 - sp_surf.get_width() - 20
                blit_centered(c, text_fit(fname, 48, text_col, max(x1 - x0, 120)),
                              (x0 + x1) / 2, rect.centery)
            elif not drum:
                st_surf = font(72).render(STATE_LABELS[state], True, state_col if state < 2 else text_col)
                blit_centered(c, st_surf, rect.centerx, rect.centery)

            # Playback progress bar (thin strip at bottom of slot, playing/overdub only)
            if state in (2, 3) and self.lp_pos[ch] > 0:
                bar_rect = pygame.Rect(rect.x + 4, rect.bottom - 8,
                                       rect.width - 8, 5)
                pygame.draw.rect(c, bar_bg, bar_rect)
                fill_w = int(bar_rect.width * self.lp_pos[ch])
                if fill_w > 0:
                    pygame.draw.rect(c, TRACK_ACTIVE,
                                     pygame.Rect(bar_rect.x, bar_rect.y,
                                                 fill_w, bar_rect.height))

    def _render_hyperguitar(self, c):
        """The five hyperGuitar lanes.

        Each row is one NEPTR encoder: the row body toggles the lane, the two
        arrows at the right edge scroll its preset list. Both emit the SAME
        physical CCs the encoder would (slot_button_cc / slot_rotate_cc), so
        this page and the hardware encoders drive one code path in the engine
        rather than two.

        Nothing here writes local state. The lane flips only when the guitar
        echoes /hg_lane back through the engine — same engine-authoritative
        discipline as the ordinary slot grid, and the reason a dead USB link
        shows as a page that visibly does not respond instead of one that lies.
        """
        area_h = CONTENT_BOTTOM - EXPR_H - CONTENT_TOP
        slot_h = (area_h - 4 * SLOT_SPACING) / 5
        for row in range(5):
            lane = HG_LANES[row]
            y = CONTENT_TOP + row * (slot_h + SLOT_SPACING)
            rect = pygame.Rect(PAD_X + SLOT_INSET, int(y) + SLOT_INSET,
                               W - 2 * (PAD_X + SLOT_INSET),
                               int(slot_h) - 2 * SLOT_INSET)

            is_on = bool(self.hg_lane_on[row])
            if self._pressed(('hg_slot', row)):
                is_on = not is_on
            rounded_slot(c, rect, is_on)
            text_col = BG if is_on else OFF_TEXT

            # Arrow column on the right. Sized for a finger, not for looks —
            # this is the control you use most on this page.
            aw = max(56, int(rect.height * 0.9))
            up_rect = pygame.Rect(rect.right - aw - 8, rect.y + 4,
                                  aw, rect.height - 8)
            dn_rect = pygame.Rect(up_rect.x - aw - 6, rect.y + 4,
                                  aw, rect.height - 8)
            self.hit.append((dn_rect, ('hg_rot', row, False)))
            self.hit.append((up_rect, ('hg_rot', row, True)))
            # The toggle hit area is everything LEFT of the arrows, so a scroll
            # tap can never be read as a toggle.
            self.hit.append((pygame.Rect(rect.x, rect.y,
                                         dn_rect.x - rect.x, rect.height),
                             ('hg_slot', row)))

            idx = self.hg_lane_idx[row]
            entries = lane["entries"]
            label = entries[idx] if 0 <= idx < len(entries) else "?"

            name_surf = text_fit(lane["name"], 40, text_col, 260)
            c.blit(name_surf, (rect.x + 14,
                               rect.centery - name_surf.get_height() // 2))

            # Current entry + position. The "3/7" is what makes an accumulator
            # usable — without it you cannot tell a wrap from a stuck encoder.
            val_w = max(dn_rect.x - (rect.x + 14 + name_surf.get_width() + 18), 120)
            val_surf = text_fit(f"{label}  {idx + 1}/{len(entries)}", 44,
                                text_col, val_w)
            c.blit(val_surf, (rect.x + 14 + name_surf.get_width() + 18,
                              rect.centery - val_surf.get_height() // 2))

            for r, glyph in ((dn_rect, "<"), (up_rect, ">")):
                pygame.draw.rect(c, text_col, r, 2, border_radius=RADIUS)
                g = font(48).render(glyph, True, text_col)
                blit_centered(c, g, r.centerx, r.centery)

    def _render_menu(self, c, menu_idx):
        if menu_idx == LOOPER_MENU:
            self._render_looper(c)
            self._render_expr(c)
            return
        if menu_idx == NORRIS_MENU:
            self.sm.render_page(self, c)
            self._render_expr(c)
            return
        if menu_idx == UTILITY_MENU:
            # Custom global-controls page — no slot grid, no expr strip.
            self._render_utility(c)
            return
        if menu_idx == HG_MENU:
            # hyperGuitar's five lanes. Its own grid, because a lane shows a
            # SCROLL POSITION ("FORM 1.10", 3 of 7) and the ordinary slot grid
            # has nowhere to put that.
            self._render_hyperguitar(c)
            self._render_expr(c)
            return
        effects = self._menu_effects(menu_idx)
        area_h = CONTENT_BOTTOM - EXPR_H - CONTENT_TOP
        slot_h = (area_h - 4 * SLOT_SPACING) / 5
        for slot_idx in range(5):
            y = CONTENT_TOP + slot_idx * (slot_h + SLOT_SPACING)
            rect = pygame.Rect(PAD_X + SLOT_INSET, int(y) + SLOT_INSET,
                               W - 2 * (PAD_X + SLOT_INSET),
                               int(slot_h) - 2 * SLOT_INSET)
            if slot_idx >= len(effects) or effects[slot_idx][1] == "—":
                pygame.draw.rect(c, EMPTY_OUTLINE, rect, 3,
                                 border_radius=RADIUS)
                continue
            code, display_name = effects[slot_idx]
            is_on = self._slot_is_on(menu_idx, slot_idx, code)
            # Pressed preview: draw the OUTCOME of the tap (a symmetric
            # toggle) while the finger is down; release commits or reverts.
            if self._pressed(('slot', menu_idx, slot_idx, code)):
                is_on = not is_on
            rounded_slot(c, rect, is_on)
            col = BG if is_on else OFF_TEXT
            self.hit.append((rect.inflate(SLOT_SPACING, SLOT_SPACING),
                             ('slot', menu_idx, slot_idx, code)))
            badge = self._slot_badge(code, is_on)
            label_w = rect.width - 24
            if badge:
                label_w -= 150
            surfs, _ = text_fit_multiline(display_name, 84, col,
                                          label_w, rect.height - 16)
            if len(surfs) == 1:
                blit_centered(c, surfs[0], rect.centerx, rect.centery)
            else:
                lh = surfs[0].get_height()
                gap = 6
                y0 = rect.centery - (lh * 2 + gap) / 2 + lh / 2
                for i, surf in enumerate(surfs):
                    blit_centered(c, surf, rect.centerx, y0 + i * (lh + gap))
            if badge:
                txt, bcol = badge
                bs = font(46).render(txt, True, bcol)
                c.blit(bs, (rect.right - bs.get_width() - 18,
                            rect.centery - bs.get_height() // 2))
        self._render_expr(c)

    def _slot_badge(self, code, is_on):
        """Live transport chip drawn inside an ON slot. Plain text, no widgets —
        the whole point is that the user can see at a glance what the button
        just did (HABIT: is the buffer still recording? BLOOPER: is there even
        a loop to granulate?)."""
        if not is_on:
            return None
        if code == 'hab':
            rec = int(self.param_values.get('/hab_rec', 1))
            return ("REC", (170, 30, 30)) if rec else ("HOLD", (30, 90, 170))
        if code == 'blp':
            n = int(self.param_values.get('/blp_active', 0))
            if n <= 0:
                return ("NO LOOP", (170, 90, 20))
            return ("x%d" % n, (20, 130, 60))
        return None

    def _render_effect(self, c, view):
        params = EFFECT_PARAMS[view]
        is_active = self.bypass_states.get(view, 1) == 0
        # NAM/IR rows: a BROWSE affordance opens the full scrollable variant
        # list for this row (all names, current one highlighted, tap to
        # select) -- see _render_variant_list / _variant_pick below.
        browse_menu = browse_row = None
        if view.startswith('nam') and view[3:].isdigit():
            browse_menu, browse_row = NAM_MENU, int(view[3:])
        elif view.startswith('ir') and view[2:].isdigit():
            browse_menu, browse_row = IR_MENU, int(view[2:])
        browse_shown = False
        if browse_menu is not None:
            rows = NAM_ROWS if browse_menu == NAM_MENU else IR_ROWS
            variants = rows.get(browse_row) or []
            if variants:
                browse_shown = True
                sel = self.nam_sel[browse_row] if browse_menu == NAM_MENU else self.ir_sel[browse_row]
                sel = min(max(sel, 0), len(variants) - 1)
                brect = pygame.Rect(PAD_X, CONTENT_TOP - 4, W - 2 * PAD_X, 64)
                blit_lit = self._pressed(('vlist_open', browse_menu, browse_row))
                rounded_slot(c, brect, blit_lit)
                bcol = BG if blit_lit else TRACK_ACTIVE
                blit_centered(c, text_fit(
                    f"BROWSE {len(variants)} — {variants[sel]}", 34,
                    bcol, brect.width - 40), brect.centerx, brect.centery)
                self.hit.append((brect, ('vlist_open', browse_menu, browse_row)))
        # POLY DLY (mvd): a top strip carries the repeats chips, interval
        # stepper and the L/R ratio readout; the param sliders sit below it.
        mvd_reserve = self._render_mvd_controls(c) if view == 'mvd' else 0
        n = len(params)
        col_spacing = 14
        col_w = (W - 2 * PAD_X - (n - 1) * col_spacing) / n
        label_zone = 145
        track_top = CONTENT_TOP + 8 + (76 if browse_shown else 0) + mvd_reserve
        track_bottom = CONTENT_BOTTOM - label_zone
        self._param_hits = []

        dragging = (isinstance(self.drag_target, tuple)
                    and self.drag_target[0] == 'param')
        for idx, (param_name, label_text, min_val, max_val) in enumerate(params):
            x0 = PAD_X + idx * (col_w + col_spacing)
            value = self.param_values.get(f"/{view}_{param_name}", min_val)
            try:
                value = float(value)
            except (TypeError, ValueError):
                value = float(min_val)
            # While the finger is on this slider, the thumb IS the finger —
            # absolute, live, both directions. Engine echo reconciles after
            # release (tick_param_chase keeps bursting until it lands).
            ch = self.param_chase
            if dragging and ch is not None and ch['code'] == view \
                    and ch['param'] == param_name:
                value = ch['target']
            span = (max_val - min_val) or 1
            frac = max(0.0, min(1.0, (value - min_val) / span))

            color = SLIDER_COLORS[idx % len(SLIDER_COLORS)]
            if is_active:
                track, fill, text = TRACK_ACTIVE, color, FG
            else:
                track = TRACK_DIM
                fill = tuple(int(ch * 0.35) for ch in color)
                text = OFF_TEXT

            inset = 12
            tr = pygame.Rect(int(x0 + inset), track_top,
                             int(col_w - 2 * inset), track_bottom - track_top)
            gap = 4
            fill_h = int((tr.height - 2 * gap) * frac)
            if fill_h > 0:
                fr = pygame.Rect(tr.x + gap, tr.bottom - gap - fill_h,
                                 tr.width - 2 * gap, fill_h)
                pygame.draw.rect(c, fill, fr)
            pygame.draw.rect(c, track, tr, 2)

            # Finger-draggable when the engine has an encoder path for it
            # (touch_down consults param_rot_cc + the /last_toggle arm gate).
            if is_active:
                self._param_hits.append((tr.inflate(2 * inset, 12),
                                         param_name, min_val, max_val))

            cx = x0 + col_w / 2
            blit_centered(c, text_fit(label_text, 42, text, col_w - 8),
                          cx, CONTENT_BOTTOM - 108)
            if is_active:
                labels = STEP_LABELS.get((view, param_name))
                if labels:
                    scaled = value * STEP_SCALE.get((view, param_name), 1)
                    step = max(0, min(len(labels) - 1, int(round(scaled))))
                    vstr = labels[step]
                else:
                    vstr = _value_fmt(min_val, max_val).format(value)
                blit_centered(c, text_fit(vstr, 56, text, col_w - 8),
                              cx, CONTENT_BOTTOM - 42)

        # Drag attempted while the engine's encoders are armed to another
        # effect: explain instead of silently ignoring.
        if self.param_hint_until is not None \
                and time.time() < self.param_hint_until:
            blit_centered(c, text_fit(
                "KNOBS ARMED ELSEWHERE — TOGGLE THE SLOT TO ARM TOUCH",
                34, TRACK_ACTIVE, W - 60), W / 2, CONTENT_TOP + 20)

    def _render_variant_list(self, c):
        """Full-screen scrollable list of every variant in ONE NAM/IR row --
        current hardware selection highlighted live (● marker + inverted
        cursor when scrolled onto it), tap picks a variant. Modal: owns
        every hit region while open (render() clears self.hit before calling
        this, same contract as _render_picker/_render_patcher)."""
        vl = self.variant_list
        menu_idx, row = vl['menu'], vl['row']
        rows = NAM_ROWS if menu_idx == NAM_MENU else IR_ROWS
        variants = rows.get(row) or []
        sel_arr = self.nam_sel if menu_idx == NAM_MENU else self.ir_sel
        sel = min(max(sel_arr[row], 0), max(0, len(variants) - 1))
        title = MENUS[menu_idx]['name']

        c.blit(bg_gradient(), (0, 0))
        blit_centered(c, text_fit(title, 60, FG, W - 200), W / 2, TITLE_CENTER_Y)
        pygame.draw.line(c, OFF_OUTLINE, (0, DIVIDER_Y), (W, DIVIDER_Y), 1)
        pygame.draw.line(c, FG, (PAD_X, DIVIDER_Y), (W - PAD_X, DIVIDER_Y), 1)
        # close button, same corner/glyph as the picker's ✕
        bx = W - 76
        r = pygame.Rect(W - 116, 0, 116, DIVIDER_Y - 4)
        pygame.draw.line(c, OFF_TEXT, (bx + 4, 30), (bx + 40, 66), 5)
        pygame.draw.line(c, OFF_TEXT, (bx + 4, 66), (bx + 40, 30), 5)
        self.hit.append((r, ('vlist_close',)))

        if not variants:
            blit_centered(c, text_fit('no variants found', 48, OFF_TEXT, W - 40),
                          W / 2, (CONTENT_TOP + CONTENT_BOTTOM) / 2)
            return

        n = len(variants)
        row_h = 108
        top = CONTENT_TOP + 20
        rows_fit = max(1, (CONTENT_BOTTOM - 12 - top) // row_h)
        scroll = self.list_scroll.get('vlist')
        if scroll is not None:
            start = max(0, min(scroll, max(0, n - rows_fit)))
            self.list_scroll['vlist'] = start
        elif n > rows_fit:
            start = max(0, min(sel - rows_fit // 2, n - rows_fit))
        else:
            start = 0

        for i in range(start, min(n, start + rows_fit)):
            y = top + (i - start) * row_h
            rect = pygame.Rect(PAD_X + 18, y, W - 2 * (PAD_X + 18), row_h - 18)
            lit = (i == sel) or self._pressed(('vlist_pick', i))
            rounded_slot(c, rect, lit)
            col = BG if lit else OFF_TEXT
            blit_centered(c, text_fit(variants[i], 50, col, rect.width - 90),
                          rect.centerx, rect.centery)
            if i == sel:
                mc = BG if lit else TRACK_ACTIVE
                pygame.draw.circle(c, mc, (rect.right - 40, rect.centery), 14)
            self.hit.append((rect.inflate(0, 18), ('vlist_pick', i)))

    def _variant_pick(self, target_idx, now):
        """Tap-select a NAM/IR variant. Sends the SAME relative rotation CC
        the hardware encoder for this row would send -- repeated the short
        way around the wrap -- so touch and hardware share one dispatch
        path (rot_slot*_cc*.orc) and one source of truth. No local state
        write: the list's highlight only moves once the engine's
        /nam_sel<r> or /ir_sel<r> echo comes back, exactly like every slot
        toggle in this UI already behaves."""
        vl = self.variant_list
        if vl is None:
            return
        menu_idx, row = vl['menu'], vl['row']
        rows = NAM_ROWS if menu_idx == NAM_MENU else IR_ROWS
        variants = rows.get(row) or []
        n = len(variants)
        if n == 0 or not (0 <= target_idx < n):
            return
        cur_sel = self.nam_sel if menu_idx == NAM_MENU else self.ir_sel
        cur = cur_sel[row]
        if cur == target_idx:
            return
        ui_row = _ui_row_for_state_row(menu_idx, row)
        cc = slot_rotate_cc(menu_idx, ui_row)
        fwd = (target_idx - cur) % n      # steps CW (up) to reach target
        back = (cur - target_idx) % n     # steps CCW (down) to reach target
        up = fwd <= back
        steps = fwd if up else back

        def _do(cc=cc, up=up, steps=steps):
            for _ in range(steps):
                self.engine.rotate(cc, up)
                time.sleep(0.004)   # let each synthetic CC land its own k-block
        threading.Thread(target=_do, daemon=True).start()

    # ----- POLY DELAY (mvd) repeats + interval controls --------------------

    def _render_mvd_controls(self, c):
        """Top strip of the POLY DLY effect view: a REPEATS row of 8 chips
        (1..8), an INTERVAL row [-] +7 [+] in signed semitones, and one
        readout line (L/R ratios, plus both tap times in ms when the live tempo
        is known). Returns the vertical space (px) it reserved so
        _render_effect can push the param sliders down below it. Highlights
        follow the engine's /mvd_reps · /mvd_intv echoes only."""
        y0 = CONTENT_TOP - 4
        lab_w = 150

        # REPEATS — 8 chips, selected one lit.
        ry = y0 + 6
        blit_centered(c, text_fit("REPEATS", 30, OFF_TEXT, lab_w - 10),
                      PAD_X + lab_w / 2 - 10, ry + 28)
        n = 8
        gap = 8
        x0 = PAD_X + lab_w
        cw = (W - PAD_X - x0 - (n - 1) * gap) / n
        for i in range(n):
            cr = pygame.Rect(int(x0 + i * (cw + gap)), ry, int(cw), 56)
            lit = (i + 1 == self.mvd_reps) or self._pressed(('mvd_reps', i + 1))
            rounded_slot(c, cr, lit)
            col = BG if lit else OFF_TEXT
            blit_centered(c, text_fit(str(i + 1), 34, col, cr.width - 8),
                          cr.centerx, cr.centery)
            self.hit.append((cr, ('mvd_reps', i + 1)))

        # INTERVAL — [-] value [+], signed semitones.
        iy = ry + 56 + 12
        blit_centered(c, text_fit("INTERVAL", 30, OFF_TEXT, lab_w - 10),
                      PAD_X + lab_w / 2 - 10, iy + 28)
        bw = 110
        mid = x0 + (W - PAD_X - x0) / 2
        for sign, bx in ((-1, int(x0)), (1, int(W - PAD_X - bw))):
            br = pygame.Rect(bx, iy, bw, 56)
            act = ('mvd_intv', sign)
            lit = self._pressed(act)
            rounded_slot(c, br, lit)
            blit_centered(c, text_fit("\u2212" if sign < 0 else "+", 40,
                                      BG if lit else TRACK_ACTIVE, bw - 16),
                          br.centerx, br.centery)
            self.hit.append((br, act))
        v = int(self.mvd_intv)
        vtxt = "0" if v == 0 else ("+%d" % v if v > 0 else "\u2212%d" % -v)
        blit_centered(c, text_fit(vtxt, 44, FG, 140), mid, iy + 28)

        # Readout: L/R fractions from the live slider values, tap times from
        # the live tempo (one beat = 60000/bpm ms) when /clock_bpm has landed.
        def pv(name, d):
            try:
                return int(round(float(self.param_values.get(f"/mvd_{name}", d))))
            except (TypeError, ValueError):
                return d
        nl, dl, nr, dr = pv("numl", 1), pv("denl", 2), pv("numr", 1), pv("denr", 3)
        line = f"L {nl}/{dl}   R {nr}/{dr}"
        if self.clock_bpm_seen and self.clock_bpm > 0:
            beat = 60000.0 / self.clock_bpm
            line += "   T %d / %d ms" % (round(beat * nl / max(dl, 1)),
                                          round(beat * nr / max(dr, 1)))
        ty = iy + 56 + 14
        blit_centered(c, text_fit(line, 34, FG, W - 80), W / 2, ty + 16)
        return (ty + 40) - CONTENT_TOP

    def _mvd_reps_pick(self, n, now):
        """Tap-select POLY DLY repeats 1..8. CC110 is an ABSOLUTE select, so one
        send sets it; the highlight follows the /mvd_reps echo."""
        if not (1 <= n <= 8):
            return
        self.engine.cc(MVD_REPS_CC, int(round((n - 1) / 7 * 127)))

    def _mvd_intv_step(self, delta, now):
        """Step the POLY DLY pitch interval by +-1 semitone (clamped -12..+12)
        from the last /mvd_intv echo; CC111 absolute. No local write."""
        semi = max(-12, min(12, int(self.mvd_intv) + int(delta)))
        self.engine.cc(MVD_INTV_CC, int(round((semi + 12) / 24 * 127)))

    def _render_sleep(self):
        c = self.canvas
        c.blit(bg_gradient(), (0, 0))
        # Soft accent halo behind the face — presentation polish only, the
        # faces themselves are untouched (they are the identity).
        halo = glow_dot(330, ACCENT_DIM)
        c.blit(halo, (W // 2 - 330, H // 2 - 330),
               special_flags=pygame.BLEND_ADD)
        # Kivy placement was top edge y=149; 2026-10-05 user asked for the
        # face placed so its art is centred in the usable area (FACE_X/FACE_Y above).
        c.blit(self.faces[self.face_current], (FACE_X, FACE_Y))
        self.hit = []          # sleeping screen: taps only wake

    # ----- companion mode --------------------------------------------------

    def _companion_item_counts(self):
        # Navigable items per screen, in tab order:
        #   DASHBOARD 0 (read-only), PATCHES N,
        #   AUDIO     SIGNAL PATH + interfaces + one row per engine setting,
        #   SYSTEM    power levels + RESTART,
        #   NETWORK   one row per visible network.
        return [0, len(self.patches),
                1 + len(self.interfaces) + len(self._audio_rows()),
                len(POWER_ITEMS), len(self.wifi_nets)]

    def _set_msg(self, text, now):
        self.companion_msg = text
        self.companion_msg_until = now + MSG_DWELL

    def _refresh_power_label(self):
        self.power_label = pi_status.power_label()

    def _refresh_interfaces(self, now, force=False):
        """Re-list interfaces on a daemon thread (it shells out to
        demiurge-interface, so it never blocks the UI loop). Rate-limited to
        IFACE_REFRESH_DT unless forced (companion entry)."""
        if not force and now - self._iface_cache_at < IFACE_REFRESH_DT:
            return
        self._iface_cache_at = now

        def _do():
            self.interfaces = actions.list_interfaces()
            # The list only exists now, so this is the first moment the AUDIO
            # cursor can actually be bounded (restored or otherwise).
            self.companion._clamp_sel()
            self.dirty = True
        threading.Thread(target=_do, daemon=True).start()

    def _refresh_audio_settings(self, now, force=False):
        """Re-read `demiurge-audio --list` off-thread (it shells out, so it
        must never run on the UI loop). Rate-limited like the interface list."""
        if not force and now - self._audio_cache_at < SETTINGS_REFRESH_DT:
            return
        self._audio_cache_at = now

        def _do():
            self.audio_settings = actions.audio_settings()
            self.dirty = True
        threading.Thread(target=_do, daemon=True).start()

    def _refresh_graph(self, now, force=False):
        """Re-read `demiurge-graph --list` off-thread.

        Polled rather than cached-once because the graph genuinely moves under
        us: the launcher re-applies its wiring whenever a device appears, a
        stage dies or live.conf changes, and it scrubs links it did not ask
        for. A PATCHER showing a snapshot from thirty seconds ago would be
        exactly as misleading as the no-UI-at-all situation this replaces."""
        if not force and now - self._graph_cache_at < GRAPH_REFRESH_DT:
            return
        self._graph_cache_at = now

        def _do():
            self.graph = actions.graph_state()
            self.dirty = True
        threading.Thread(target=_do, daemon=True).start()

    def _refresh_wifi(self, now, force=False, rescan=False):
        """Re-read the Wi-Fi status + AP list off-thread. The plain list is
        served from NetworkManager's cache and is fast; `rescan` forces a real
        scan that takes seconds, which is exactly why this never blocks the
        UI loop."""
        if not force and not rescan and now - self._wifi_cache_at < WIFI_REFRESH_DT:
            return
        self._wifi_cache_at = now
        if rescan:
            self.wifi_busy = True

        def _do():
            try:
                self.wifi = actions.wifi_status()
                self.wifi_nets = actions.wifi_list(rescan=rescan)
            finally:
                self.wifi_busy = False
                self.dirty = True
        threading.Thread(target=_do, daemon=True).start()

    def _audio_rows(self):
        """The engine-setting rows shown under the interface list on the AUDIO
        screen, in a fixed order, skipping anything the CLI didn't report.
        Returns [(key, label, setting_dict)]."""
        return [(k, lab, self.audio_settings[k])
                for k, lab in actions.AUDIO_ROWS if k in self.audio_settings]

    def _refresh_pi(self, now):
        if now - self._pi_cache_at >= PI_REFRESH_DT:
            self._pi_cache = pi_status.gather()
            self._pi_cache_at = now
            self.power_label = self._pi_cache.get("power_label")

    def _companion_activate(self, screen, sel, now):
        """Dispatch an arcade tap. Side effects run in a daemon thread so the
        UI loop never blocks on a patch switch, power change, or restart."""
        if screen == 1 and 0 <= sel < len(self.patches):
            name = self.patches[sel]
            if name == self.active_patch:
                self._set_msg("already loaded", now)
                self.patch_armed = None
                return
            # Two-step commit. A patch switch rewrites live.conf, changes the
            # power governor and RESTARTS the audio engine — far too much to
            # hang off one contact on a resistive panel. The first tap arms
            # and says so; only a second tap inside the window commits.
            if (self.patch_armed != sel or self.patch_armed_until is None
                    or now >= self.patch_armed_until):
                self.patch_armed = sel
                self.patch_armed_until = now + PATCH_CONFIRM_SECS
                self._set_msg(f"tap again → {name}", now)
                return
            self.patch_armed = None
            self.patch_armed_until = None
            self._set_msg(f"→ {name}", now)
            level = actions.patch_power(self.acts.patches_dir, name)
            # Leave a trace in the journal. Until now a patch switch was
            # completely invisible (actions runs the CLI with capture_output,
            # so even demiurge-set's own output went nowhere) — which is why
            # "it keeps going to car mode" had no log to check against.
            log.info("patch switch: %s -> %s (power=%s, confirmed)",
                     self.active_patch, name, level)

            # Power first (so the governor is already at the target level
            # before the audio engine restarts at it — matters most for
            # car_mode's brownout-safe low power), then the patch switch.
            def _do(name=name, level=level):
                self.acts.power_set.run(level=level)
                self.acts.activator.activate(name)
                self._refresh_power_label()
                self.refresh_active_patch()   # move the ● to the new patch
                self.dirty = True
            threading.Thread(target=_do, daemon=True).start()
        elif screen == companion_mod.SYSTEM and 0 <= sel < len(POWER_ITEMS):
            item = POWER_ITEMS[sel]
            if item == "RESTART":
                self._set_msg("restarting…", now)
                threading.Thread(target=self.acts.restart.run, daemon=True).start()
            else:
                level = item.lower()              # low / medium / high
                self._set_msg(f"power → {item}", now)

                def _do(level=level):
                    self.acts.power_set.run(level=level)
                    time.sleep(0.6)               # let the governor settle
                    self._refresh_power_label()
                    self.dirty = True
                threading.Thread(target=_do, daemon=True).start()
        elif screen == companion_mod.AUDIO:
            # The AUDIO screen is three stacked sections sharing one cursor:
            # the SIGNAL PATH row, then the interface list, then the engine
            # settings. Row 0 opens the PATCHER; everything after it shifts
            # down by one.
            n_if = len(self.interfaces)
            if sel == 0:
                self._open_patcher(now)
                return
            if 1 <= sel < 1 + n_if:
                iface = self.interfaces[sel - 1]
                if iface.get("active"):
                    self._set_msg("already active", now)
                    return
                name = iface["name"]
                self._set_msg(f"→ {name}", now)

                # demiurge-interface restarts the demiurge service itself; the
                # re-list afterwards moves the ● marker once the switch lands.
                def _do(name=name):
                    actions.set_interface(name)
                    self.interfaces = actions.list_interfaces()
                    self.dirty = True
                threading.Thread(target=_do, daemon=True).start()
                return
            rows = self._audio_rows()
            ridx = sel - n_if - 1
            if 0 <= ridx < len(rows):
                key, label, setting = rows[ridx]
                self._open_picker(key, label, setting)
        elif screen == companion_mod.NETWORK and 0 <= sel < len(self.wifi_nets):
            self._activate_network(self.wifi_nets[sel], now)

    def tick_companion(self, now):
        """Feed the controller the latest input snapshot and act on events.
        Returns True if the screen needs repainting."""
        prev_screen = self.companion.screen
        events = self.companion.update(now, **self.input_state.snapshot())
        if events and self.companion.active \
                and self.companion.screen != prev_screen:
            self._arm_slide(1 if self.companion.screen > prev_screen else -1)
            self.list_scroll.pop(prev_screen, None)
        return self._apply_companion_events(events, now)

    def _apply_companion_events(self, events, now):
        """Act on companion state-machine events (from MIDI ticks OR touch
        taps — one shared path). Returns True if a repaint is needed."""
        repaint = False
        for ev in events:
            if ev == "enter":
                if self.is_sleeping:
                    self.wake_up()
                self.trans_blocks = []
                self.trans_next_at = None
                self.list_scroll = {}
                self.slide_old = self.slide_new = None
                self.patches = actions.list_patches(self.acts.patches_dir)
                self.refresh_active_patch()
                self.patch_armed = None
                self.patch_armed_until = None
                # The cursor default depends on the list we just loaded, so
                # re-seat it now that patches/active_patch are known.
                self.companion.enter_screen(self.companion.screen)
                self._refresh_interfaces(now, force=True)
                self._refresh_power_label()
                self._refresh_pi(now)
                self.companion_refresh_at = now + COMPANION_REFRESH_DT
                self.dirty = True
                repaint = True
            elif ev == "exit":
                self._close_patcher()
                self.last_osc_time = now           # restart the sleep countdown
                self._last_rendered_view = None     # force a clean normal repaint
                self._last_rendered_signature = None
                self.companion_msg = None
                self.dirty = True
                repaint = True
            elif ev == "sleep":
                self.companion_msg = None
                self.enter_sleep()
                repaint = True
            elif ev == "nav":
                # Navigating away closes the PATCHER: it is a modal over ONE
                # tab, and leaving it open over another would let a stale
                # graph sit on top of an unrelated screen.
                self._close_patcher()
                if self.companion.screen == companion_mod.AUDIO:
                    self._refresh_interfaces(now)   # rate-limited re-list
                    self._refresh_audio_settings(now)
                    self._refresh_graph(now)
                elif self.companion.screen == companion_mod.NETWORK:
                    self._refresh_wifi(now)
                if self.companion.screen != 1:
                    self.patch_armed = None         # leaving PATCHES disarms
                    self.patch_armed_until = None
                self.dirty = True
                repaint = True
            elif isinstance(ev, tuple) and ev[0] == "activate":
                self._companion_activate(ev[1], ev[2], now)
                self.dirty = True
                repaint = True
        return repaint

    def _render_companion(self, c):
        self.hit = []
        # Overlays are MODAL: they repaint the whole screen and register every
        # hit region themselves, so nothing behind them can be tapped through.
        # self.hit was just cleared, which is what makes that true.
        if self.keyboard is not None:
            self._render_keyboard(c)
            self._draw_pressed(c)
            return
        if self.picker is not None:
            self._render_picker(c)
            self._draw_pressed(c)
            return
        if self.patcher:
            self._render_patcher(c)
            if self.companion_msg:
                blit_centered(c, text_fit(self.companion_msg, 34, TRACK_ACTIVE,
                                          W - 40), W / 2, CONTENT_BOTTOM - 130)
            self._draw_pressed(c)
            return
        c.blit(bg_gradient(), (0, 0))
        screen = self.companion.screen
        blit_centered(c, text_fit(COMPANION_TITLES[screen], 68, FG, W - 200),
                      W / 2, TITLE_CENTER_Y)
        pygame.draw.line(c, OFF_OUTLINE, (0, DIVIDER_Y), (W, DIVIDER_Y), 1)
        pygame.draw.line(c, FG, (PAD_X, DIVIDER_Y), (W - PAD_X, DIVIDER_Y), 1)
        self._draw_header_buttons(c, close=True)
        if screen == 0:
            self._render_dashboard(c)
        elif screen == companion_mod.PATCHES:
            self._render_patches(c)
        elif screen == companion_mod.AUDIO:
            self._render_audio(c)
        elif screen == companion_mod.SYSTEM:
            self._render_system(c)
        else:
            self._render_net(c)
        if self.companion_msg:
            blit_centered(c, text_fit(self.companion_msg, 40, TRACK_ACTIVE, W - 40),
                          W / 2, CONTENT_BOTTOM - TAB_BAR_H - 26)
        self._render_tabs(c, screen)
        self._draw_pressed(c)

    def _render_dashboard(self, c):
        """Dense text readout (terminal-visualizer style): system stats up top,
        then every live MIDI channel as `label:value` cells filling the screen.
        No sliders — numbers and letters only."""
        d = self._pi_cache

        def sval(key, suffix=""):
            v = d.get(key)
            return f"{v}{suffix}" if v is not None else "--"

        # ---- system stats: two rows of three label/value cells ----
        stats = [
            ("PWR", d.get("power_label") or self.power_label or "--"),
            ("TEMP", sval("temp_c", "C")),
            ("FREQ", sval("freq_mhz")),
            ("LOAD", sval("load")),
            ("MAXF", sval("max_mhz")),
            ("THR", "ok" if d.get("throttled") == 0 else (d.get("throttled_label") or "--")),
        ]
        col_w = (W - 2 * PAD_X) / 3
        y = CONTENT_TOP + 12
        for i, (lab, val) in enumerate(stats):
            cx = PAD_X + (i % 3) * col_w
            ry = y + (i // 3) * 78
            c.blit(font(34).render(lab, True, OFF_TEXT), (int(cx) + 8, ry))
            c.blit(text_fit(str(val), 46, FG, col_w - 16), (int(cx) + 8, ry + 30))

        grid_top = y + 2 * 78 + 18
        pygame.draw.line(c, OFF_OUTLINE, (PAD_X, grid_top - 10),
                         (W - PAD_X, grid_top - 10), 1)
        c.blit(font(30).render("INPUTS", True, OFF_TEXT), (PAD_X + 8, grid_top - 4))
        grid_top += 40

        # ---- telemetry grid: 3 columns of label:value cells ----
        tel = self.input_state.telemetry()
        cols = 3
        rows = (len(tel) + cols - 1) // cols
        bottom = CONTENT_BOTTOM - TAB_BAR_H - 16   # leave room for the tab bar
        cell_h = (bottom - grid_top) / rows
        cell_w = (W - 2 * PAD_X) / cols
        for idx, (lab, val) in enumerate(tel):
            col = idx % cols
            row = idx // cols
            x = PAD_X + col * cell_w
            cy = grid_top + row * cell_h + cell_h / 2
            shown = "--" if val is None else str(val)
            color = FG if (val is not None and val > 0) else TRACK_DIM
            ls = font(38).render(lab + ":", True, OFF_TEXT)
            c.blit(ls, (int(x) + 8, int(cy - ls.get_height() / 2)))
            vs = text_fit(shown, 38, color, cell_w - ls.get_width() - 26)
            c.blit(vs, (int(x) + 8 + ls.get_width() + 10,
                        int(cy - vs.get_height() / 2)))

    def _list_window(self, screen, n, row_h):
        """(start, rows_fit, top): scrollable-list geometry shared by the
        PATCH and AUDIO tabs. A touch drag sets list_scroll[screen]; without
        one the window centers on the cursor (joystick behavior, unchanged)."""
        top = CONTENT_TOP + 18
        rows_fit = max(1, (CONTENT_BOTTOM - TAB_BAR_H - 12 - top) // row_h)
        scroll = self.list_scroll.get(screen)
        if scroll is not None:
            start = max(0, min(scroll, max(0, n - rows_fit)))
            self.list_scroll[screen] = start
        elif n > rows_fit:
            start = max(0, min(self.companion.sel - rows_fit // 2,
                               n - rows_fit))
        else:
            start = 0
        return start, rows_fit, top

    def _render_patches(self, c):
        """Portrait list of the patches in ~/demiurge/sets. Cursor = inverted;
        the patch actually loaded in live.conf gets the ● marker (same
        vocabulary as the POWER and AUDIO screens, and as the terminal
        companion's ▸). Without that marker the cursor alone read as "this is
        the live patch" and row 0 — car_mode — always looked selected."""
        if not self.patches:
            blit_centered(c, text_fit("no patches found", 48, OFF_TEXT, W - 40),
                          W / 2, (CONTENT_TOP + CONTENT_BOTTOM) / 2)
            return
        sel = self.companion.sel
        n = len(self.patches)
        row_h = 80
        start, rows_fit, top = self._list_window(1, n, row_h)
        for i in range(start, min(n, start + rows_fit)):
            y = top + (i - start) * row_h
            rect = pygame.Rect(PAD_X + 8, y, W - 2 * (PAD_X + 8), row_h - 12)
            lit = i == sel or self._pressed(('item', 1, i))
            armed = (i == self.patch_armed and self.patch_armed_until
                     is not None and time.time() < self.patch_armed_until)
            rounded_slot(c, rect, lit, pressed_glow=armed)
            col = BG if lit else OFF_TEXT
            blit_centered(c, text_fit(self.patches[i], 50, col, rect.width - 74),
                          rect.centerx, rect.centery)
            if self.patches[i] == self.active_patch:
                mc = BG if lit else TRACK_ACTIVE
                pygame.draw.circle(c, mc, (rect.right - 30, rect.centery), 12)
            self.hit.append((rect.inflate(0, 12), ('item', 1, i)))

    def _render_system(self, c):
        """SYSTEM: how the box RUNS — power mode + restart. Cursor =
        inverted; the currently-active power mode gets a ● marker."""
        active = (self.power_label or "").upper()
        active_idx = {"LOW": 0, "MED": 1, "MEDIUM": 1, "HIGH": 2}.get(active, -1)
        sel = self.companion.sel
        top = CONTENT_TOP + 40
        row_h = 168
        for i, item in enumerate(POWER_ITEMS):
            y = top + i * row_h
            rect = pygame.Rect(PAD_X + 18, y, W - 2 * (PAD_X + 18), row_h - 22)
            is_sel = (i == sel) or self._pressed(('item', companion_mod.SYSTEM, i))
            rounded_slot(c, rect, is_sel)
            col = BG if is_sel else OFF_TEXT
            blit_centered(c, text_fit(item, 70, col, rect.width - 110),
                          rect.centerx, rect.centery)
            if i == active_idx:
                mc = BG if is_sel else TRACK_ACTIVE
                pygame.draw.circle(c, mc, (rect.right - 44, rect.centery), 17)
            self.hit.append((rect.inflate(0, 22),
                             ('item', companion_mod.SYSTEM, i)))

    def _blit_wrapped(self, c, text, size, color, x, y, max_w, max_h):
        """Greedy word-wrap at a FIXED size, clipped to max_h. Returns the y
        below the last line drawn.

        Deliberately not text_fit_multiline: that caps at two lines and shrinks
        the font until they fit, which turns a long sentence into unreadable
        micro-text. Help copy here is a full sentence from a CLI and its whole
        job is to be read, so the size holds and the box wraps instead."""
        f = font(size)
        line_h = f.get_linesize()
        words = str(text).split()
        y0, line = y, ""
        for word in words:
            trial = (line + " " + word).strip()
            if line and f.size(trial)[0] > max_w:
                if y + line_h > y0 + max_h:
                    return y
                c.blit(f.render(line, True, color), (int(x), int(y)))
                y += line_h
                line = word
            else:
                line = trial
        if line and y + line_h <= y0 + max_h:
            c.blit(f.render(line, True, color), (int(x), int(y)))
            y += line_h
        return y

    def _section_label(self, c, text, y):
        """Small caps heading that separates the sections of a settings
        screen, with a hairline to its right."""
        lab = font(28).render(text, True, OFF_TEXT)
        c.blit(lab, (PAD_X + 12, int(y)))
        lx = PAD_X + 24 + lab.get_width()
        ly = int(y + lab.get_height() / 2)
        pygame.draw.line(c, OFF_OUTLINE, (lx, ly), (W - PAD_X - 12, ly), 1)
        return y + lab.get_height() + 10

    def _render_audio(self, c):
        """AUDIO: how the box SOUNDS — which interface it runs on, then the
        four engine settings.

        Both sections share ONE cursor (interfaces first, then settings), so
        the joystick still walks the whole screen top to bottom. Every row
        shows its CURRENT VALUE on the right, so nothing has to be opened to
        find out what it is set to. Interface names are runtime data from
        demiurge-interface; the setting rows are runtime data from
        demiurge-audio, including their help text — no value or wording is
        hardcoded here."""
        sel = self.companion.sel
        n_if = len(self.interfaces)
        rows = self._audio_rows()
        row_h = 76
        top = CONTENT_TOP + 6
        bottom = CONTENT_BOTTOM - TAB_BAR_H - 12

        # SIGNAL PATH comes FIRST — above the interface and the buffer size —
        # because it is the only row that can say "you are wired to nothing".
        # Everything below it is a tuning question; this one is a does-sound-
        # come-out question, and it went unanswerable for a whole evening.
        top = self._section_label(c, "SIGNAL PATH", top)
        rect = pygame.Rect(PAD_X + 8, int(top), W - 2 * (PAD_X + 8), row_h - 12)
        is_sel = (sel == 0) or self._pressed(('item', companion_mod.AUDIO, 0))
        broken = self._graph_broken()
        rounded_slot(c, rect, is_sel,
                     outline=ALERT if broken else OFF_OUTLINE)
        col = BG if is_sel else (ALERT if broken else OFF_TEXT)
        c.blit(font(38).render("PATCHER", True, col),
               (rect.x + 22, rect.centery - 19))
        summary = text_fit(self._graph_summary(), 34,
                           BG if is_sel else (ALERT if broken else TRACK_ACTIVE),
                           rect.width // 2 + 40)
        c.blit(summary, (rect.right - 34 - summary.get_width(),
                         rect.centery - summary.get_height() // 2))
        self.hit.append((rect.inflate(0, 12), ('item', companion_mod.AUDIO, 0)))
        top += row_h + 10

        top = self._section_label(c, "INTERFACE", top)
        if not self.interfaces:
            c.blit(font(34).render("none attached", True, TRACK_DIM),
                   (PAD_X + 16, int(top) + 6))
            top += 54
        for i, iface in enumerate(self.interfaces):
            if top + row_h > bottom:
                break
            idx = 1 + i                      # 0 is the SIGNAL PATH row
            rect = pygame.Rect(PAD_X + 8, int(top), W - 2 * (PAD_X + 8),
                               row_h - 12)
            is_sel = (idx == sel) or self._pressed(
                ('item', companion_mod.AUDIO, idx))
            rounded_slot(c, rect, is_sel)
            col = BG if is_sel else OFF_TEXT
            c.blit(text_fit(iface["name"], 44, col, rect.width - 110),
                   (rect.x + 22, rect.centery - 22))
            if iface.get("active"):
                mc = BG if is_sel else TRACK_ACTIVE
                pygame.draw.circle(c, mc, (rect.right - 34, rect.centery), 13)
            elif iface.get("chosen"):
                mc = BG if is_sel else OFF_TEXT
                pygame.draw.circle(c, mc, (rect.right - 34, rect.centery),
                                   13, 3)
            self.hit.append((rect.inflate(0, 12),
                             ('item', companion_mod.AUDIO, idx)))
            top += row_h

        top += 10
        top = self._section_label(c, "ENGINE", top)
        if not rows:
            c.blit(font(30).render("settings unavailable", True, TRACK_DIM),
                   (PAD_X + 16, int(top) + 6))
            return
        for j, (key, label, setting) in enumerate(rows):
            if top + row_h > bottom:
                break
            idx = 1 + n_if + j
            rect = pygame.Rect(PAD_X + 8, int(top), W - 2 * (PAD_X + 8),
                               row_h - 12)
            is_sel = (idx == sel) or self._pressed(
                ('item', companion_mod.AUDIO, idx))
            rounded_slot(c, rect, is_sel)
            col = BG if is_sel else OFF_TEXT
            c.blit(font(38).render(label, True, col),
                   (rect.x + 22, rect.centery - 19))
            val = text_fit(setting_label(key, setting["value"]), 40,
                           BG if is_sel else TRACK_ACTIVE, rect.width // 2)
            c.blit(val, (rect.right - 34 - val.get_width(),
                         rect.centery - val.get_height() // 2))
            self.hit.append((rect.inflate(0, 12),
                             ('item', companion_mod.AUDIO, idx)))
            top += row_h

        # Help panel for whatever the cursor is on — the CLI's own mode-aware
        # sentence, readable WITHOUT opening the row. This is where "the buffer
        # size does nothing in bypass mode" becomes visible at a glance.
        # Pinned just above the tab bar so it never jumps as the cursor moves.
        ridx = sel - n_if - 1
        if 0 <= ridx < len(rows):
            eff = rows[ridx][2].get("effect") or ""
            panel = pygame.Rect(PAD_X + 8, bottom - 190,
                                W - 2 * (PAD_X + 8), 182)
            if panel.top > top + 8:
                pygame.draw.rect(c, (14, 0, 16), panel, border_radius=RADIUS)
                pygame.draw.rect(c, OFF_OUTLINE, panel, 2,
                                 border_radius=RADIUS)
                c.blit(font(26).render(rows[ridx][1], True, ACCENT),
                       (panel.x + 18, panel.y + 12))
                self._blit_wrapped(c, eff, 30, OFF_TEXT, panel.x + 18,
                                   panel.y + 46, panel.width - 36,
                                   panel.height - 58)

    # ----- NETWORK screen --------------------------------------------------

    def _render_net(self, c):
        """NETWORK: which Wi-Fi the box is on, its IP (so it can be found on
        the network at all), and the visible networks strongest-first.

        ● = joined, hollow ring = a saved network in range. Tapping a saved or
        open network joins it; tapping a secured unknown one opens the
        keyboard. Everything here is runtime data from demiurge-wifi."""
        w = self.wifi
        top = CONTENT_TOP + 6

        # --- header: where we are, and how to reach us ---
        box = pygame.Rect(PAD_X + 8, int(top), W - 2 * (PAD_X + 8), 118)
        pygame.draw.rect(c, (14, 0, 16), box, border_radius=RADIUS)
        pygame.draw.rect(c, OFF_OUTLINE, box, 2, border_radius=RADIUS)
        if w and w.get("state") == "connected" and w.get("ssid"):
            c.blit(text_fit(w["ssid"], 44, TRACK_ACTIVE, box.width - 40 - 2 * 160),
                   (box.x + 20, box.y + 12))
            ip = w.get("ip") or "no address"
            c.blit(font(32).render("IP  " + ip, True, OFF_TEXT),
                   (box.x + 20, box.y + 64))
        else:
            state = (w or {}).get("state", "unknown")
            msg = {"nowifi": "no Wi-Fi adapter",
                   "connecting": "connecting…"}.get(state, "not connected")
            c.blit(text_fit(msg, 40, OFF_TEXT, box.width - 40),
                   (box.x + 20, box.y + 36))
        # RESCAN sits in the header so it can't be confused with a network row
        rr = pygame.Rect(box.right - 168, box.y + 20, 148, 78)
        lit = self._pressed(('net_rescan',)) or self.wifi_busy
        rounded_slot(c, rr, lit)
        blit_centered(c, font(28).render(
            "…" if self.wifi_busy else "RESCAN", True, BG if lit else OFF_TEXT),
            rr.centerx, rr.centery)
        self.hit.append((rr, ('net_rescan',)))
        # DISCONNECT sits on the same row, immediately left of RESCAN, same
        # size/style, same reasoning — it must not be confused with a
        # network row either.
        dr = pygame.Rect(rr.x - 12 - rr.width, rr.y, rr.width, rr.height)
        dlit = self._pressed(('net_disconnect',))
        connected = bool(self.wifi and self.wifi.get('ssid'))
        rounded_slot(c, dr, dlit)
        blit_centered(c, font(24).render(
            "DISCONNECT", True, BG if dlit else (OFF_TEXT if connected else TRACK_DIM)),
            dr.centerx, dr.centery)
        if connected:
            self.hit.append((dr, ('net_disconnect',)))
        top = box.bottom + 16

        top = self._section_label(c, "NETWORKS", top)
        if not self.wifi_nets:
            c.blit(font(34).render("none visible — tap RESCAN", True, TRACK_DIM),
                   (PAD_X + 16, int(top) + 6))
            return

        sel = self.companion.sel
        n = len(self.wifi_nets)
        row_h = 78
        bottom = CONTENT_BOTTOM - TAB_BAR_H - 12
        rows_fit = max(1, int((bottom - top) // row_h))
        scroll = self.list_scroll.get(companion_mod.NETWORK)
        if scroll is not None:
            start = max(0, min(scroll, max(0, n - rows_fit)))
            self.list_scroll[companion_mod.NETWORK] = start
        elif n > rows_fit:
            start = max(0, min(sel - rows_fit // 2, n - rows_fit))
        else:
            start = 0

        for i in range(start, min(n, start + rows_fit)):
            net = self.wifi_nets[i]
            rect = pygame.Rect(PAD_X + 8, int(top + (i - start) * row_h),
                               W - 2 * (PAD_X + 8), row_h - 12)
            armed = (self.setting_armed == ('wifi', net["ssid"])
                     and self.setting_armed_until is not None
                     and time.time() < self.setting_armed_until)
            is_sel = (i == sel) or self._pressed(
                ('item', companion_mod.NETWORK, i))
            rounded_slot(c, rect, is_sel, pressed_glow=armed)
            col = BG if is_sel else OFF_TEXT
            c.blit(text_fit(net["ssid"], 40, col, rect.width - 230),
                   (rect.x + 22, rect.centery - 20))
            # signal + lock, right-aligned before the state marker
            meta = "%d%%" % net["signal"]
            if net["security"] == "open":
                meta += "  open"
            ms = font(28).render(meta, True, BG if is_sel else TRACK_DIM)
            c.blit(ms, (rect.right - 66 - ms.get_width(),
                        rect.centery - ms.get_height() // 2))
            if net["active"]:
                mc = BG if is_sel else TRACK_ACTIVE
                pygame.draw.circle(c, mc, (rect.right - 34, rect.centery), 13)
            elif net["saved"]:
                mc = BG if is_sel else OFF_TEXT
                pygame.draw.circle(c, mc, (rect.right - 34, rect.centery),
                                   13, 3)
            self.hit.append((rect.inflate(0, 12),
                             ('item', companion_mod.NETWORK, i)))

    def _activate_network(self, net, now):
        """Tap on a network row. A saved or open network joins after a
        confirming second tap (it drops the current connection, so one stray
        contact on a resistive panel must not do it). A secured unknown one
        opens the keyboard instead — there is nothing to confirm yet."""
        if self.wifi_busy:
            # A join/disconnect/rescan is already running. Without this, a
            # stray second contact on a resistive panel (or an impatient
            # tap) fires a SECOND nmcli con-up on top of one already
            # mid-handshake, which cancels it with reason=new-activation —
            # this was silently killing real join attempts before they
            # could ever finish, on every hardware attempt this session.
            self._set_msg("busy…", now)
            return
        if net.get("active"):
            self._set_msg("already connected", now)
            return
        if not net.get("saved") and net.get("security") != "open":
            self._open_keyboard(net["ssid"])
            return
        token = ('wifi', net["ssid"])
        if (self.setting_armed != token or self.setting_armed_until is None
                or now >= self.setting_armed_until):
            self.setting_armed = token
            self.setting_armed_until = now + PATCH_CONFIRM_SECS
            self._set_msg("tap again → %s" % net["ssid"], now)
            return
        self.setting_armed = None
        self.setting_armed_until = None
        self._join_network(net["ssid"], None, now)

    def _join_network(self, ssid, password, now):
        """Run the join off-thread and report the CLI's own verdict. The
        password (when there is one) is handed straight to actions.wifi_join,
        which pipes it to the CLI on stdin — it is never logged here, never
        persisted, and the local reference dies with this call."""
        self._set_msg("joining %s…" % ssid, now)
        self.wifi_busy = True

        def _do():
            try:
                ok, msg = actions.wifi_join(ssid, password)
                self._set_msg(msg, time.time())
                self.wifi = actions.wifi_status()
                self.wifi_nets = actions.wifi_list()
            finally:
                self.wifi_busy = False
                self.dirty = True
        threading.Thread(target=_do, daemon=True).start()

    def _disconnect_network(self, now):
        """Drop the current wifi link. Does not change any profile's
        autoconnect — whether something reconnects afterward is that
        profile's own explicit setting, not a decision this button makes."""
        if not (self.wifi and self.wifi.get('ssid')):
            self._set_msg('not connected', now)
            return
        self._set_msg('disconnecting…', now)
        self.wifi_busy = True

        def _do():
            try:
                actions.wifi_disconnect()
                self._set_msg('disconnected', time.time())
                self.wifi = actions.wifi_status()
                self.wifi_nets = actions.wifi_list()
            finally:
                self.wifi_busy = False
                self.dirty = True
        threading.Thread(target=_do, daemon=True).start()

    # ----- PATCHER (the audio graph) ---------------------------------------
    #
    # Lives behind the SIGNAL PATH row on the AUDIO tab, because the graph is a
    # SOUND question ("does my instrument reach the engine, and how fast"),
    # not a SYSTEM one ("how hot is the Pi allowed to run"). It sits above the
    # interface and the buffer size for the same reason: buffer size is worth
    # nothing if the engine's input is wired to nothing at all, which is
    # exactly the state the device was silently in on 2026-08-09.
    #
    # Every read and every write goes through demiurge-graph. The UI never
    # calls pw-link, never remembers the graph across a repaint, and never
    # decides what "default" means — `reset` asks the launcher, which owns it.

    # ASCII only: the UI font has no arrow or cross glyph, and a tofu box
    # where the alarm should be is worse than no alarm at all.
    PATH_LABEL = {"direct": "DIRECT", "loopback": "LOOPBACK",
                  "none": "!! NOTHING", "internal": "INTERNAL"}

    def _graph_mode(self):
        return (self.graph or {}).get("mode")

    def _graph_broken(self):
        """Is any engine port wired to nothing? That IS silence, and it is the
        one condition allowed to break the palette (ALERT)."""
        g = self.graph
        if not g or g.get("mode") != "sync":
            return False
        return bool(g.get("warnings"))

    def _graph_summary(self):
        """One short right-aligned string for the SIGNAL PATH row."""
        g = self.graph
        if not g:
            return "…"
        mode = g.get("mode")
        if mode == "bypass":
            return "sync layer off"
        if mode == "nopipewire":
            return "no graph"
        if mode != "sync":
            return "unavailable"
        ip, op = g.get("in_path"), g.get("out_path")
        if ip == "none":
            return "!! NO INPUT"
        if op == "none":
            return "!! NO OUTPUT"
        return "%s·%s" % (self.PATH_LABEL.get(ip, "?"),
                          self.PATH_LABEL.get(op, "?"))

    def _node_display(self, node_name):
        for n in (self.graph or {}).get("nodes", []):
            if n["name"] == node_name:
                return n["display"]
        return node_name

    def _port_label(self, port_name):
        """'csound6 · IN 1' — short enough for a 480px panel. Falls back to the
        raw PipeWire name if the port has gone away under us."""
        for p in (self.graph or {}).get("ports", []):
            if p["name"] == port_name:
                return "%s · %s" % (self._node_display(p["node"]), p["display"])
        return port_name

    # Only the two ends a musician actually patches between. MIDI is not audio;
    # the loopback nodes are the launcher's own plumbing and contribute ~24
    # permanently-unconnected rows on a real device, which would bury the one
    # engine port that means silence. Loopback still appears in the LINKS list
    # (that is where DIRECT vs LOOPBACK is read off), and getting back to a
    # loopback route is what RESET is for.
    PATCHABLE_KINDS = ("engine", "interface")

    def _patcher_ports(self):
        """The ports worth showing, engine first."""
        ports = [p for p in (self.graph or {}).get("ports", [])
                 if p["kind"] in self.PATCHABLE_KINDS
                 and not p["display"].startswith("MON")]
        order = {"engine": 0, "interface": 1}
        return sorted(ports, key=lambda p: (order.get(p["kind"], 2),
                                            p["dir"] != "out", p["name"]))

    def _patcher_rows(self):
        """Flat, uniform-height row list so one scroll offset covers the whole
        overlay: [('hdr', text) | ('link', link) | ('port', port)]."""
        g = self.graph or {}
        rows = [("hdr", "LINKS")]
        links = [l for l in g.get("links", []) if l["path"] != "internal"
                 or "engine-in" in l["flags"] or "engine-out" in l["flags"]]
        rows += [("link", l) for l in links]
        if not links:
            rows.append(("none", "no links at all"))
        rows.append(("hdr", "PORTS — tap two to connect"))
        rows += [("port", p) for p in self._patcher_ports()]
        return rows

    def _open_patcher(self, now):
        self.patcher = True
        self.graph_pick = None
        self.setting_armed = None
        self.setting_armed_until = None
        self.list_scroll.pop('patcher', None)
        self._refresh_graph(now, force=True)
        self.dirty = True

    def _close_patcher(self):
        self.patcher = False
        self.graph_pick = None
        self.setting_armed = None
        self.setting_armed_until = None
        self.list_scroll.pop('patcher', None)
        self.dirty = True

    def _armed(self, token, now=None):
        now = time.time() if now is None else now
        return (self.setting_armed == token
                and self.setting_armed_until is not None
                and now < self.setting_armed_until)

    def _arm(self, token, msg, now):
        self.setting_armed = token
        self.setting_armed_until = now + PATCH_CONFIRM_SECS
        self._set_msg(msg, now)
        self.dirty = True

    def _graph_tap_port(self, name, now):
        """Tap a port. First tap picks one end, second tap on a port of the
        OPPOSITE direction makes the link. Connecting is not destructive, so
        it commits on the second tap without an arm step — unlike disconnect
        and reset, which both take sound away."""
        ports = {p["name"]: p for p in self._patcher_ports()}
        port = ports.get(name)
        if port is None:
            return
        pick = self.graph_pick
        if pick is None or pick["name"] == name:
            self.graph_pick = None if (pick and pick["name"] == name) else port
            if self.graph_pick is not None:
                self._set_msg("from %s — tap the other end"
                              % self._port_label(name), now)
            self.dirty = True
            return
        if pick["dir"] == port["dir"]:
            self._set_msg("tap %s to finish this link"
                          % ("an input" if pick["dir"] == "out" else "an output"),
                          now)
            return
        src, dst = (pick, port) if pick["dir"] == "out" else (port, pick)
        self.graph_pick = None
        self._set_msg("%s -> %s" % (self._port_label(src["name"]),
                                    self._port_label(dst["name"])), now)

        def _do(s=src["name"], d=dst["name"]):
            ok, msg = actions.graph_connect(s, d)
            if not ok and msg:
                self._set_msg(msg.splitlines()[0], time.time())
            self.graph = actions.graph_state()
            self.dirty = True
        threading.Thread(target=_do, daemon=True).start()

    def _graph_tap_link(self, src, dst, now):
        """Tap a link to remove it — armed, then confirmed. Pulling the last
        link off an engine port is instant silence, so this gets the same
        two-tap gate as a patch switch."""
        token = ('glink', src, dst)
        if not self._armed(token, now):
            engine_end = any(
                p["kind"] == "engine" and p["name"] in (src, dst)
                for p in (self.graph or {}).get("ports", []))
            self._arm(token,
                      "tap again to CUT — this can be silence" if engine_end
                      else "tap again to cut this link", now)
            return
        self.setting_armed = None
        self.setting_armed_until = None
        self._set_msg("cut", now)

        def _do():
            actions.graph_disconnect(src, dst)
            self.graph = actions.graph_state()
            self.dirty = True
        threading.Thread(target=_do, daemon=True).start()

    def _graph_reset(self, now):
        """The way back. A user who has patched themselves into silence must
        never need ssh — this drops their overrides and asks the launcher to
        re-assert the wiring it means to have. Armed like every other action
        that changes what comes out of the speakers."""
        if not self._armed(('greset',), now):
            self._arm(('greset',), "tap again to restore the default patching",
                      now)
            return
        self.setting_armed = None
        self.setting_armed_until = None
        self.graph_pick = None
        self._set_msg("restoring…", now)

        def _do():
            actions.graph_reset()
            time.sleep(1.2)           # the launcher re-applies on its watcher
            self.graph = actions.graph_state()
            self.dirty = True
        threading.Thread(target=_do, daemon=True).start()

    def _render_patcher(self, c):
        """Full-screen view of the audio graph. Owns every hit region while it
        is open (the caller clears self.hit first)."""
        now = time.time()
        g = self.graph or {}
        c.blit(bg_gradient(), (0, 0))
        blit_centered(c, text_fit("PATCHER", 60, FG, W - 200),
                      W / 2, TITLE_CENTER_Y)
        pygame.draw.line(c, OFF_OUTLINE, (0, DIVIDER_Y), (W, DIVIDER_Y), 1)
        pygame.draw.line(c, FG, (PAD_X, DIVIDER_Y), (W - PAD_X, DIVIDER_Y), 1)
        bx = W - 76
        self.hit.append((pygame.Rect(W - 116, 0, 116, DIVIDER_Y - 4),
                         ('gclose',)))
        pygame.draw.line(c, OFF_TEXT, (bx + 4, 30), (bx + 40, 66), 5)
        pygame.draw.line(c, OFF_TEXT, (bx + 4, 66), (bx + 40, 30), 5)

        mode = g.get("mode")
        if mode != "sync":
            msg = {"bypass": "Sync layer is off — the graph only exists in "
                             "sync mode. The chain program owns the interface "
                             "directly, so there is nothing to patch.",
                   "nopipewire": "The audio graph is not running, so there is "
                                 "nothing to patch.",
                   }.get(mode, "The patcher is unavailable — demiurge-graph "
                               "did not answer.")
            self._blit_wrapped(c, msg, 38, OFF_TEXT, PAD_X + 20,
                               CONTENT_TOP + 60, W - 2 * PAD_X - 40, 400)
            return

        # ---- the latency-relevant fact, at the top, in words ----
        top = CONTENT_TOP + 6
        for label, key in (("IN", "in_path"), ("OUT", "out_path")):
            path = g.get(key)
            bad = path == "none"
            band = pygame.Rect(PAD_X + 8, int(top), W - 2 * (PAD_X + 8), 96)
            pygame.draw.rect(c, (14, 0, 16), band, border_radius=RADIUS)
            pygame.draw.rect(c, ALERT if bad else OFF_OUTLINE, band, 2,
                             border_radius=RADIUS)
            c.blit(font(34).render(label, True, OFF_TEXT),
                   (band.x + 20, band.centery - 17))
            val = text_fit(self.PATH_LABEL.get(path, "?"), 46,
                           ALERT if bad else TRACK_ACTIVE, band.width - 300)
            c.blit(val, (band.x + 110, band.centery - val.get_height() // 2))
            note = {"direct": "no extra latency",
                    "loopback": "+1 buffer",
                    "none": "SILENCE"}.get(path, "")
            ns = font(28).render(note, True, ALERT if bad else TRACK_DIM)
            c.blit(ns, (band.right - 20 - ns.get_width(),
                        band.centery - ns.get_height() // 2))
            top = band.bottom + 10

        # ---- anything unconnected, shouted ----
        for warn in g.get("warnings", []):
            if warn["code"] != "dangling":
                continue
            c.blit(text_fit("!! " + warn["text"], 28, ALERT, W - 2 * PAD_X - 20),
                   (PAD_X + 12, int(top)))
            top += 34

        # ---- what a tap will do right now ----
        hint = ("from %s — tap the other end" % self._port_label(self.graph_pick["name"])
                if self.graph_pick else "tap a port, then another, to connect")
        c.blit(text_fit(hint, 28, ACCENT if self.graph_pick else TRACK_DIM,
                        W - 2 * PAD_X - 20), (PAD_X + 12, int(top) + 4))
        top += 42

        # ---- scrolling body ----
        reset_top = CONTENT_BOTTOM - 108
        rows = self._patcher_rows()
        row_h = 70
        rows_fit = max(1, int((reset_top - 16 - top) // row_h))
        start = max(0, min(self.list_scroll.get('patcher', 0),
                           max(0, len(rows) - rows_fit)))
        self.list_scroll['patcher'] = start
        for i in range(start, min(len(rows), start + rows_fit)):
            kind, payload = rows[i]
            y = int(top + (i - start) * row_h)
            if kind == "hdr":
                self._section_label(c, payload, y + 12)
                continue
            if kind == "none":
                c.blit(font(30).render(payload, True, TRACK_DIM),
                       (PAD_X + 20, y + 16))
                continue
            rect = pygame.Rect(PAD_X + 8, y, W - 2 * (PAD_X + 8), row_h - 12)
            if kind == "link":
                token = ('glink', payload["src"], payload["dst"])
                armed = self._armed(token, now)
                lit = self._pressed(token) or armed
                rounded_slot(c, rect, lit, pressed_glow=armed)
                col = BG if lit else OFF_TEXT
                c.blit(text_fit("%s  ->  %s" % (self._port_label(payload["src"]),
                                                self._port_label(payload["dst"])),
                                30, col, rect.width - 190),
                       (rect.x + 18, rect.centery - 16))
                badge = self.PATH_LABEL.get(payload["path"], "")
                bs = font(26).render(badge, True,
                                     BG if lit else (TRACK_ACTIVE
                                                     if payload["path"] == "direct"
                                                     else TRACK_DIM))
                c.blit(bs, (rect.right - 20 - bs.get_width(),
                            rect.centery - bs.get_height() // 2))
                self.hit.append((rect.inflate(0, 12), token))
                continue
            # a port
            port = payload
            token = ('gport', port["name"])
            picked = (self.graph_pick is not None
                      and self.graph_pick["name"] == port["name"])
            dangling = port["kind"] == "engine" and port["links"] == 0
            lit = self._pressed(token) or picked
            # When one end is already picked, every port that could complete
            # the link is outlined in the accent: the preview is of the whole
            # outcome, not just the row under the finger.
            candidate = (self.graph_pick is not None and not picked
                         and self.graph_pick["dir"] != port["dir"])
            outline = ALERT if dangling else (ACCENT if candidate else OFF_OUTLINE)
            rounded_slot(c, rect, lit, outline=outline)
            col = BG if lit else (ALERT if dangling else OFF_TEXT)
            c.blit(text_fit(self._port_label(port["name"]), 32, col,
                            rect.width - 200),
                   (rect.x + 18, rect.centery - 17))
            right = "!! NOTHING" if dangling else (
                "%s  %d" % (port["dir"].upper(), port["links"]))
            rs = font(26).render(right, True,
                                 BG if lit else (ALERT if dangling else TRACK_DIM))
            c.blit(rs, (rect.right - 20 - rs.get_width(),
                        rect.centery - rs.get_height() // 2))
            self.hit.append((rect.inflate(0, 12), token))

        # ---- the way back, always on screen, never scrolled off ----
        rr = pygame.Rect(PAD_X + 18, reset_top, W - 2 * (PAD_X + 18), 92)
        armed = self._armed(('greset',), now)
        lit = self._pressed(('greset',)) or armed
        rounded_slot(c, rr, lit, pressed_glow=armed)
        blit_centered(c, text_fit(
            "TAP AGAIN TO RESTORE" if armed else "RESET TO DEFAULT", 42,
            BG if lit else TRACK_ACTIVE, rr.width - 60), rr.centerx, rr.centery)
        self.hit.append((rr, ('greset',)))

    # ----- value picker (engine settings) ----------------------------------

    def _open_picker(self, key, label, setting):
        """Open the option list for one engine setting. Options and help text
        come from demiurge-audio — the UI never invents either, which is what
        keeps the bypass-mode buffer-size caveat honest in all three UIs at
        once."""
        self.picker = {"key": key, "label": label,
                       "options": list(setting.get("options") or []),
                       "value": setting.get("value"),
                       "effect": setting.get("effect") or ""}
        self.setting_armed = None
        self.setting_armed_until = None
        self.dirty = True

    def _close_picker(self):
        self.picker = None
        self.setting_armed = None
        self.setting_armed_until = None
        self.dirty = True

    def _pick_value(self, value, now):
        """Choose an option. Settings that restart the engine (and can
        therefore silence a running set) need a confirming second tap; the
        rest apply immediately."""
        pk = self.picker
        if pk is None:
            return
        key = pk["key"]
        if value == pk["value"]:
            self._close_picker()
            return
        if key in actions.AUDIO_CONFIRM:
            token = (key, value)
            if (self.setting_armed != token or self.setting_armed_until is None
                    or now >= self.setting_armed_until):
                self.setting_armed = token
                self.setting_armed_until = now + PATCH_CONFIRM_SECS
                self._set_msg("tap again — this restarts audio", now)
                self.dirty = True
                return
        label = pk["label"]
        self._close_picker()
        self._set_msg("%s → %s" % (label, setting_label(key, value)), now)

        def _do():
            actions.set_audio(key, value)
            self.audio_settings = actions.audio_settings()
            self.dirty = True
        threading.Thread(target=_do, daemon=True).start()

    def _render_picker(self, c):
        """Full-screen option list for one setting. Owns every hit region
        while it is open (the caller clears self.hit first), so nothing
        underneath can be tapped through it."""
        pk = self.picker
        c.blit(bg_gradient(), (0, 0))
        blit_centered(c, text_fit(pk["label"], 60, FG, W - 200),
                      W / 2, TITLE_CENTER_Y)
        pygame.draw.line(c, OFF_OUTLINE, (0, DIVIDER_Y), (W, DIVIDER_Y), 1)
        pygame.draw.line(c, FG, (PAD_X, DIVIDER_Y), (W - PAD_X, DIVIDER_Y), 1)
        # close button, same corner and glyph as the companion's ✕
        bx = W - 76
        r = pygame.Rect(W - 116, 0, 116, DIVIDER_Y - 4)
        pygame.draw.line(c, OFF_TEXT, (bx + 4, 30), (bx + 40, 66), 5)
        pygame.draw.line(c, OFF_TEXT, (bx + 4, 66), (bx + 40, 30), 5)
        self.hit.append((r, ('pick_cancel',)))

        options = pk["options"]
        row_h = 108
        top = CONTENT_TOP + 20
        for i, opt in enumerate(options):
            rect = pygame.Rect(PAD_X + 18, top + i * row_h,
                               W - 2 * (PAD_X + 18), row_h - 18)
            armed = (self.setting_armed == (pk["key"], opt)
                     and self.setting_armed_until is not None
                     and time.time() < self.setting_armed_until)
            lit = self._pressed(('pick', opt)) or armed
            rounded_slot(c, rect, lit, pressed_glow=armed)
            col = BG if lit else OFF_TEXT
            blit_centered(c, text_fit(setting_label(pk["key"], opt), 52, col,
                                      rect.width - 120),
                          rect.centerx, rect.centery)
            if opt == pk["value"]:
                mc = BG if lit else TRACK_ACTIVE
                pygame.draw.circle(c, mc, (rect.right - 40, rect.centery), 15)
            self.hit.append((rect.inflate(0, 18), ('pick', opt)))

        # The CLI's own sentence about what this setting actually does, in the
        # CURRENT mode. Rendered verbatim: the UI must not paraphrase it into
        # a latency promise the engine cannot keep.
        y = top + len(options) * row_h + 16
        if pk["effect"] and y < CONTENT_BOTTOM - 40:
            self._blit_wrapped(c, pk["effect"], 30, TRACK_DIM, PAD_X + 12, y,
                               W - 2 * PAD_X - 24, CONTENT_BOTTOM - y - 12)

    # ----- on-screen keyboard (Wi-Fi password) -----------------------------

    def _open_keyboard(self, ssid):
        """Open the password entry for `ssid`.

        The typed password lives ONLY in self.keyboard["text"] — a plain string
        in this process. It is never written to ui-state.conf, never logged,
        never sent over OSC, and never placed in a command line: on commit it
        goes to actions.wifi_join, which pipes it to demiurge-wifi on stdin.
        _close_keyboard drops the reference."""
        self.keyboard = {
            "ssid": ssid,
            "text": "",
            "page": 0,
            "shift": False,     # one-shot
            "caps": False,      # sticky (double-tap SHIFT)
            "reveal": False,    # SHOW toggle: whole password in clear
            "last_at": 0.0,     # when the last character was typed
        }
        self.dirty = True

    def _close_keyboard(self):
        self.keyboard = None
        self.dirty = True

    def _kb_key(self, action, now):
        """Handle one keyboard press (already committed on release)."""
        kb = self.keyboard
        if kb is None:
            return
        if action == "@shift":
            # tap = one-shot caps, tap again while armed = caps lock
            if kb["shift"] and not kb["caps"]:
                kb["caps"], kb["shift"] = True, False
            elif kb["caps"]:
                kb["caps"] = False
            else:
                kb["shift"] = True
        elif action == "@page":
            kb["page"] = 1 - kb["page"]
        elif action == "@back":
            kb["text"] = kb["text"][:-1]
        elif action == "@reveal":
            kb["reveal"] = not kb["reveal"]
        elif action == "@cancel":
            self._close_keyboard()
            return
        elif action == "@ok":
            ssid, password = kb["ssid"], kb["text"]
            if len(password) < 8:
                self._set_msg("password must be at least 8 characters", now)
                self.dirty = True
                return
            self._close_keyboard()          # drops the plaintext reference
            self._join_network(ssid, password, now)
            return
        else:
            ch = "@" if action == "@@" else action
            if len(kb["text"]) < 63:
                if kb["shift"] or kb["caps"]:
                    ch = ch.upper()
                kb["text"] += ch
                kb["last_at"] = now
                kb["shift"] = False
        self.dirty = True

    def _kb_geometry(self):
        """(rows, key_h, top) for the current page — shared by the renderer
        and nothing else (hit regions are registered during the render)."""
        rows = KB_PAGES[self.keyboard["page"]]
        key_h = 125
        top = CONTENT_BOTTOM - 12 - len(rows) * key_h
        return rows, key_h, top

    def _render_keyboard(self, c):
        """Password entry. Owns every hit region while open (the caller clears
        self.hit first)."""
        kb = self.keyboard
        c.blit(bg_gradient(), (0, 0))
        blit_centered(c, text_fit(kb["ssid"], 52, FG, W - 220),
                      W / 2, TITLE_CENTER_Y)
        pygame.draw.line(c, OFF_OUTLINE, (0, DIVIDER_Y), (W, DIVIDER_Y), 1)
        pygame.draw.line(c, FG, (PAD_X, DIVIDER_Y), (W - PAD_X, DIVIDER_Y), 1)
        # cancel, in the companion's ✕ corner
        bx = W - 76
        pygame.draw.line(c, OFF_TEXT, (bx + 4, 30), (bx + 40, 66), 5)
        pygame.draw.line(c, OFF_TEXT, (bx + 4, 66), (bx + 40, 30), 5)
        self.hit.append((pygame.Rect(W - 116, 0, 116, DIVIDER_Y - 4),
                         ('kb', '@cancel')))

        # --- the field ---
        # Masked by default. The LAST character stays legible for a moment
        # (the phone convention) because a resistive panel mis-lands often and
        # a row of identical dots gives no way to notice. SHOW reveals it all;
        # it defaults off and is a deliberate press on a device in your hands.
        # 112 tall so the SHOW toggle inside it still clears a 48-physical-px
        # touch target once the 720x1280 canvas is scaled to the 480x800 panel.
        fld = pygame.Rect(PAD_X + 8, CONTENT_TOP + 6, W - 2 * (PAD_X + 8), 112)
        pygame.draw.rect(c, (14, 0, 16), fld, border_radius=RADIUS)
        pygame.draw.rect(c, OFF_OUTLINE, fld, 2, border_radius=RADIUS)
        text, n = kb["text"], len(kb["text"])
        if kb["reveal"]:
            shown = text
        elif n and time.time() - kb["last_at"] < KB_REVEAL_SECS:
            shown = "•" * (n - 1) + text[-1]
        else:
            shown = "•" * n
        if shown:
            c.blit(text_fit(shown, 46, TRACK_ACTIVE, fld.width - 210),
                   (fld.x + 20, fld.centery - 23))
        else:
            c.blit(font(34).render("password", True, TRACK_DIM),
                   (fld.x + 20, fld.centery - 17))
        sr = pygame.Rect(fld.right - 168, fld.y + 12, 148, 88)
        lit = kb["reveal"] or self._pressed(('kb', '@reveal'))
        rounded_slot(c, sr, lit)
        blit_centered(c, font(26).render("SHOW", True, BG if lit else OFF_TEXT),
                      sr.centerx, sr.centery)
        self.hit.append((sr, ('kb', '@reveal')))

        # character count / minimum hint
        hint = "%d characters" % n if n >= 8 else "at least 8 characters"
        c.blit(font(26).render(hint, True, TRACK_DIM),
               (fld.x + 20, fld.bottom + 8))

        # --- the keys ---
        rows, key_h, top = self._kb_geometry()
        avail = W - 2 * PAD_X
        unit = avail / KB_COLS
        for r, row in enumerate(rows):
            x = PAD_X
            for (label, action, span) in row:
                w = unit * span
                rect = pygame.Rect(int(x) + 5, int(top + r * key_h) + 5,
                                   int(w) - 10, key_h - 10)
                # "@@" is the literal @ character, not a verb — without this
                # it renders with the function-key treatment and reads as a
                # control rather than something you can type.
                special = action.startswith("@") and action != "@@"
                on = self._pressed(('kb', action))
                if action == "@shift" and (kb["shift"] or kb["caps"]):
                    on = True
                # JOIN is the commit: accent outline + accent text so it never
                # reads as just another key on a grid of identical keys.
                commit = (action == "@ok")
                rounded_slot(c, rect, on,
                             outline=(ACCENT if commit else
                                      OFF_OUTLINE if special else
                                      EMPTY_OUTLINE))
                col = BG if on else (ACCENT if commit else
                                     OFF_TEXT if special else FG)
                lab = label
                if len(action) == 1 and not special \
                        and (kb["shift"] or kb["caps"]) and action.isalpha():
                    lab = label.upper()
                if action == "@shift" and kb["caps"]:
                    lab = "CAPS"
                blit_centered(c, text_fit(lab, 44 if len(lab) == 1 else 30,
                                          col, rect.width - 16),
                              rect.centerx, rect.centery)
                self.hit.append((rect.inflate(6, 6), ('kb', action)))
                x += w

    # ----- WAVE screen -----------------------------------------------------

    def _render_wave(self, c):
        """Wavetable editor: draw the cycle with a finger (top panel), sculpt
        its 64-partial spectrum on log-spaced bars (middle), OSC status line
        (bottom). No buttons (2026-08-09): sweep along/below the bottom edge
        to zero everything the finger crosses (that IS the clear), along/above
        the top to max it. Both panels are drag surfaces — their rects are
        kept in self._wave_rects for the touch handlers."""
        wave_rect = pygame.Rect(PAD_X + 8, CONTENT_TOP + 8,
                                W - 2 * (PAD_X + 8), 570)
        bars_rect = pygame.Rect(PAD_X + 8, wave_rect.bottom + 52,
                                W - 2 * (PAD_X + 8), 310)
        self._wave_rects = (wave_rect, bars_rect)
        ws = self.wave

        # --- waveform panel ---
        pygame.draw.rect(c, (14, 0, 16), wave_rect, border_radius=RADIUS)
        midy = wave_rect.centery
        # phase grid at 1/4 marks + center line
        for q in (0.25, 0.5, 0.75):
            x = wave_rect.x + int(wave_rect.width * q)
            pygame.draw.line(c, EMPTY_OUTLINE, (x, wave_rect.y + 6),
                             (x, wave_rect.bottom - 6), 1)
        pygame.draw.line(c, OFF_OUTLINE, (wave_rect.x + 4, midy),
                         (wave_rect.right - 4, midy), 1)
        # the cycle: one sample column per x pixel (2048 -> ~660 points)
        n = wavetable.N
        pts = []
        half = (wave_rect.height - 12) / 2
        for px in range(wave_rect.width - 8):
            idx = int(px * (n - 1) / max(1, wave_rect.width - 9))
            v = max(-1.0, min(1.0, ws.wave[idx]))
            pts.append((wave_rect.x + 4 + px, int(midy - v * half)))
        if len(pts) > 1:
            pygame.draw.lines(c, ACCENT_DIM, False, pts, 5)   # soft under-glow
            pygame.draw.lines(c, TRACK_ACTIVE, False, pts, 2)
        # snap-join hint: ghost dot at the right edge showing where the cycle
        # restarts (wave[0]) — line the end up with it for a click-free loop
        y0 = int(midy - max(-1.0, min(1.0, ws.wave[0])) * half)
        pygame.draw.circle(c, OFF_TEXT, (wave_rect.right - 8, y0), 6, 2)
        pygame.draw.circle(c, OFF_TEXT, (wave_rect.x + 8,
                                         pts[0][1] if pts else midy), 4)
        pygame.draw.rect(c, OFF_OUTLINE, wave_rect, 2, border_radius=RADIUS)
        lab = font(30).render("DRAW  ·  amplitude vs phase", True, OFF_TEXT)
        c.blit(lab, (wave_rect.x + 14, wave_rect.y + 10))

        # --- harmonic bars, log-spaced (lower partials get more room) ---
        pygame.draw.rect(c, (14, 0, 16), bars_rect, border_radius=RADIUS)
        mags = ws.magnitudes()
        peak = max(max(mags), 1.0)
        for k in range(1, wavetable.NH + 1):
            f0, f1 = wavetable.bar_edges(k)
            x0 = bars_rect.x + 3 + f0 * (bars_rect.width - 6)
            x1 = bars_rect.x + 3 + f1 * (bars_rect.width - 6)
            bw = max(1, int(x1 - x0) - 2)
            m = mags[k - 1] / peak
            bh = int((bars_rect.height - 10) * m)
            col = SLIDER_COLORS[(k - 1) % len(SLIDER_COLORS)]
            if bh > 0:
                pygame.draw.rect(c, col, (int(x0) + 1,
                                          bars_rect.bottom - 5 - bh, bw, bh))
            else:
                pygame.draw.line(c, EMPTY_OUTLINE,
                                 (int(x0) + 1, bars_rect.bottom - 6),
                                 (int(x0) + bw, bars_rect.bottom - 6), 1)
        pygame.draw.rect(c, OFF_OUTLINE, bars_rect, 2, border_radius=RADIUS)
        lab = font(30).render("HARMONICS 1-64  ·  drag to sculpt", True, OFF_TEXT)
        c.blit(lab, (bars_rect.x + 12, bars_rect.y - 30))
        for k in (1, 2, 4, 8, 16, 32, 64):     # log axis ticks
            f0, f1 = wavetable.bar_edges(k)
            x = bars_rect.x + 3 + (f0 + f1) / 2 * (bars_rect.width - 6)
            t = font(24).render(str(k), True, OFF_TEXT)
            c.blit(t, (int(x - t.get_width() / 2), bars_rect.bottom + 6))

        # --- stroke shimmer trail (subtle, additive, auto-expiring) ---
        now = time.time()
        self._trail_prune(now)
        if self._stroke_trail:
            spark = glow_dot(12, ACCENT_DIM)
            for tx, ty, _t in self._stroke_trail[-24:]:
                c.blit(spark, (int(tx) - 12, int(ty) - 12),
                       special_flags=pygame.BLEND_ADD)

        # --- OSC status line (edge-sweep hints double as the reset path) ---
        status = "OSC  %s:%d" % (self.wt_host, self.wt_port)
        if self.wt_sends:
            status += "  ·  sent x%d" % self.wt_sends
        if self.wt_send_at is not None:
            status += "  ·  …"
        status += "  ·  sweep bottom = clear · top = max"
        blit_centered(c, text_fit(status, 30, TRACK_ACTIVE, W - 60),
                      W / 2, bars_rect.bottom + 46)

    def _render_tabs(self, c, screen):
        """Touch-first tab bar: full-width segments, icon + label, active tab
        filled with the accent halo treatment. Each segment is its own
        >=TAB_BAR_H hit region."""
        n = len(COMPANION_TABS)
        seg = W / n
        bar_top = CONTENT_BOTTOM - TAB_BAR_H
        pygame.draw.line(c, OFF_OUTLINE, (0, bar_top - 6), (W, bar_top - 6), 1)
        for i, lab in enumerate(COMPANION_TABS):
            x0 = i * seg
            rect = pygame.Rect(int(x0) + 5, bar_top, int(seg) - 10, TAB_BAR_H)
            if i == screen or self._pressed(('tab', i)):
                rounded_slot(c, rect, True)
                col = BG
            else:
                pygame.draw.rect(c, OFF_OUTLINE, rect, 2, border_radius=RADIUS)
                col = OFF_TEXT
            icon = tab_icon(i, 34, col)
            c.blit(icon, (rect.centerx - 17, rect.y + 12))
            blit_centered(c, text_fit(lab, 30, col, rect.width - 8),
                          rect.centerx, rect.bottom - 26)
            self.hit.append((pygame.Rect(int(x0), bar_top - 6, int(seg),
                                         TAB_BAR_H + 6), ('tab', i)))

    def _reveal_blocks(self):
        """Flip the next batch of big-pixel blocks from the new view in."""
        # Batch size is fixed from the full grid so each frame reveals an
        # equal share regardless of how many are left.
        total = (W // BLOCK_SIZE + 1) * (H // BLOCK_SIZE + 1)
        per_frame = max(1, total // BLOCK_TRANS_FRAMES)
        for _ in range(min(per_frame, len(self.trans_blocks))):
            bx, by = self.trans_blocks.pop()
            bw = min(BLOCK_SIZE, W - bx)
            bh = min(BLOCK_SIZE, H - by)
            self.canvas.blit(self.clean.subsurface((bx, by, bw, bh)), (bx, by))

    def tick_transition(self, now):
        """Advance the block dissolve. Returns True if the canvas changed."""
        if not self.trans_blocks or now < (self.trans_next_at or 0):
            return False
        self._reveal_blocks()
        if self.trans_blocks:
            self.trans_next_at = now + BLOCK_TRANS_DT
        else:
            # Snap fully clean at the end.
            self.canvas.blit(self.clean, (0, 0))
            self.trans_next_at = None
        return True

    # ----- main-loop deadline ----------------------------------------------

    def next_deadline(self, now):
        # While the companion is open it owns the idle->sleep timeout, so the
        # normal OSC-silence sleep countdown is suspended.
        deadlines = ([self.last_osc_time + SLEEP_AFTER]
                     if (not self.is_sleeping and not self.companion.active)
                     else [])
        for t in (self.pending_switch_at, self.trans_next_at,
                  self.face_check_at, self.face_revert_at, self.wt_send_at,
                  self.sync_retry_at, self.param_hint_until):
            if t is not None:
                deadlines.append(t)
        if self.param_chase is not None:
            deadlines.append(now + PARAM_TICK)
        lpd = self.sm.lp_deadline()
        if lpd is not None:
            deadlines.append(lpd)
        if self._stroke_trail:
            deadlines.append(now + 0.05)     # let the shimmer trail fade out
        if self.pressed_fade is not None:
            deadlines.append(now + 0.03)     # pressed-highlight fade frames
        if self.press_fx is not None or (
                self.slide_old is not None and self.slide_new is not None):
            deadlines.append(now + 0.03)    # animation frames
        cd = self.companion.next_deadline(now)
        if cd is not None:
            deadlines.append(cd)
        if self.companion.active and self.companion_refresh_at is not None:
            deadlines.append(self.companion_refresh_at)
        # TASK 2/3a: the metro dot needs the loop to keep waking at roughly
        # the cheap-path's own 25 Hz so it can blink smoothly BETWEEN
        # /beat_idx messages (which arrive once per BEAT — far slower than a
        # blink needs, 0.1875-0.75 s across the loop-tempo band). Only while
        # the beat clock is actually live, so an idle/silent rig goes right
        # back to pure event-driven waiting (zero idle CPU) the moment the
        # clock stops ticking — the same BEAT_LIVE_TIMEOUT used to blank the
        # dot itself in _beat_phase().
        if (self.beat_anchor_time is not None
                and now - self.beat_seen <= BEAT_LIVE_TIMEOUT):
            deadlines.append(now + 0.04)
        return min(deadlines) if deadlines else now + 1.0


# ---------------------------------------------------------------------------
# OSC routing (server thread -> queue -> main thread)
# ---------------------------------------------------------------------------

def route_osc(ui, addr, val):
    ui.note_osc(addr)                   # late-join sync accounting
    if addr == '/preset_recall':
        # The engine fires this the instant a bank slot is recalled, BEFORE the
        # burst of per-effect bypass echoes it triggers. Arming here is what
        # makes a preset press leave the view completely alone: the toggles
        # still register and repaint, they just do not navigate.
        # 2.0 s comfortably covers the burst (measured: a recall unpacks ~260
        # params and echoes every effect's state within a few hundred ms).
        ui.preset_recall_until = time.time() + 2.0
        return
    if addr == '/meter':
        # (L, R) rms from the engine, 25 Hz. Deliberately does NOT set
        # ui.dirty: the strips live outside the canvas and are painted by
        # their own cheap path, so a level change never triggers a full
        # canvas re-render. That is what keeps this free.
        try:
            l, r = (float(val[0]), float(val[1])) if isinstance(val, (list, tuple)) \
                   else (float(val), float(val))
        except (TypeError, ValueError, IndexError):
            return
        ui.meter_l, ui.meter_r = l, r
        ui.meter_pk_l = max(l, ui.meter_pk_l * 0.90)
        ui.meter_pk_r = max(r, ui.meter_pk_r * 0.90)
        now_ = time.time()
        # 0.999 rather than 1.0: the converter truncates at full scale and
        # there is NO limiter anywhere before `outs`, so anything reaching
        # here is already being hard-clipped.
        if l >= 0.999:
            ui.meter_clip_l = now_
        if r >= 0.999:
            ui.meter_clip_r = now_
        ui.meter_seen = now_
        return
    if addr == '/beat_idx':
        # Beat-EDGE telemetry from chase_bliss_clock.orc instr 100 (the
        # AUTHORITATIVE tempo — correct whether NEPTR or PMOR is master).
        # Arrives once per beat, not once per blink frame — this is the
        # (re-)anchor for the metro dot's local predictor (TASK 2/3a); the
        # dot itself is painted on the cheap 25 Hz present() path (see
        # main()), so — same discipline as /meter — this deliberately does
        # NOT set ui.dirty.
        ui.beat_anchor_time = time.time()
        ui.beat_seen = ui.beat_anchor_time
        return
    if addr == '/beat_down':
        ui.beat_down = int(val)
        return
    if addr == '/beat_period':
        ui.beat_period = max(0.05, float(val))
        return
    if addr == '/dsp_load':
        # Real callback wall-time / quantum period from the C++ host (TASK
        # 3b) — the only place that knows the actual deadline. 25 Hz, same
        # cadence as /meter; painted on the same cheap path, so no dirty.
        #
        # The asymmetric slew (fast attack / slow release, see
        # DSP_LOAD_ATTACK_TAU/DSP_LOAD_RELEASE_TAU) is applied HERE, against
        # real elapsed wall-clock time since the last message, rather than
        # at render time — a real spike must still show up even if nothing
        # else makes the UI repaint in between. _draw_dsp_bar just reads
        # ui.dsp_load_disp and handles the (separate) stale-blanking case,
        # since staleness is a "no message arrived" condition route_osc by
        # definition never sees.
        # 2026-10: the bar now shows real CPU use sampled from /proc/stat in
        # main() (CpuLoad). The host's /dsp_load never reached the bar, and
        # two sources for one bar would fight, so the OSC value is ignored.
        return
        try:
            target = max(0.0, float(val))
        except (TypeError, ValueError):
            return
        now_ = time.time()
        if ui.dsp_load_disp_t is None:
            ui.dsp_load_disp = target          # first sample: snap, no history to slew from
        else:
            dt = max(0.0, now_ - ui.dsp_load_disp_t)
            rising = target > ui.dsp_load_disp
            tau = DSP_LOAD_ATTACK_TAU if rising else DSP_LOAD_RELEASE_TAU
            k = 1.0 - math.exp(-dt / tau) if tau > 0 else 1.0
            ui.dsp_load_disp += (target - ui.dsp_load_disp) * k
        ui.dsp_load_disp_t = now_
        ui.dsp_load = target
        ui.dsp_load_seen = now_
        return
    if addr == '/dsp_xrun':
        # Cumulative xrun COUNT (monotonic) — a rising edge triggers the
        # bar's brief red flash.
        try:
            n = int(val)
        except (TypeError, ValueError):
            return
        if n > ui.dsp_xruns:
            ui.dsp_xrun_flash_until = time.time() + DSP_XRUN_FLASH_SECS
        ui.dsp_xruns = n
        return
    if addr == '/menu':
        ui.handle_menu_change(int(val))
    elif addr == '/last_toggle':
        # Which effect the engine's encoders are armed to — gates the touch
        # param sliders (their rotation CCs dispatch on gkLastToggle).
        lt = int(val)
        if lt != ui.last_toggle:
            ui.last_toggle = lt
            if ui.current_view in EFFECT_PARAMS:
                ui.dirty = True
    elif addr == '/hg_present':
        # hyperGuitar plugged in / unplugged. The engine is the ONLY authority
        # for this (it watches the guitar's heartbeat); the UI never probes.
        present = bool(int(val))
        if present != ui.hg_present:
            ui._touch()
            ui.hg_present = present
            if not present:
                # Leaving the page is the engine's job — it snaps gkCurrentMenu
                # to 0 and echoes /menu. Here we only make sure a stale lane
                # state cannot be shown if the guitar comes back.
                ui.hg_lane_on = [0] * 5
            ui.dirty = True
    elif addr == '/hg_lane':
        # val arrives as the 3-int tuple (lane, on, index) the engine relays.
        try:
            lane, on, idx = (int(v) for v in val)
        except (TypeError, ValueError):
            return
        if 0 <= lane < 5:
            if ui.hg_lane_on[lane] != on or ui.hg_lane_idx[lane] != idx:
                ui._touch()
                ui.hg_lane_on[lane] = on
                ui.hg_lane_idx[lane] = idx
                if ui.current_view == f'menu{HG_MENU}':
                    ui.dirty = True
    elif addr == '/preset':
        ui.handle_preset_change(int(val))
    elif addr == '/preset_bank':
        ui.handle_preset_bank_change(int(val))
    elif addr == '/preamp_state':
        # Csound sends /preamp_state; UI effect code for the MAIN PRE slot is "pre".
        ui.handle_state_change('pre', val)
    elif addr == '/tgl_state':
        # gkTotalGlitch: 0=all glitch ON, 1=all glitch OFF — maps to bass slot "tgl".
        ui.handle_state_change('tgl', val)
    elif addr == '/nam_out_gain':
        for i in range(5):
            ui.update_param(f'/nam{i}_out_gain', val)
    elif addr == '/nam_in_gain':
        for i in range(5):
            ui.update_param(f'/nam{i}_in_gain', val)
    elif addr.startswith('/nam_sel'):
        r = int(addr[8:])
        if 0 <= r < 5 and ui.nam_sel[r] != int(val):
            ui._touch()
            ui.nam_sel[r] = int(val)
            # Refresh the visible label wherever this row is on screen.
            if ui.current_view == f'menu{NAM_MENU}' or ui.current_view == f'nam{r}' \
                    or (ui.variant_list is not None and ui.variant_list['menu'] == NAM_MENU
                        and ui.variant_list['row'] == r):
                if ui.current_view == f'nam{r}':
                    variants = NAM_ROWS.get(r)
                    if variants:
                        ui.title_text = variants[min(max(int(val), 0), len(variants) - 1)]
                ui.dirty = True
    elif addr.startswith('/ir_sel'):
        r = int(addr[7:])
        if 0 <= r < 5 and ui.ir_sel[r] != int(val):
            ui._touch()
            ui.ir_sel[r] = int(val)
            # Refresh the visible label wherever this row is on screen.
            if ui.current_view == f'menu{IR_MENU}' or ui.current_view == f'ir{r}' \
                    or (ui.variant_list is not None and ui.variant_list['menu'] == IR_MENU
                        and ui.variant_list['row'] == r):
                if ui.current_view == f'ir{r}':
                    variants = IR_ROWS.get(r)
                    if variants:
                        ui.title_text = variants[min(max(int(val), 0), len(variants) - 1)]
                ui.dirty = True
    elif addr == '/preset_trim':
        # Per-preset output trim (linear gain, ±6 dB around 1.0). Menu 0 / CC20
        # edits the ACTIVE preset's stored value. Cached only — the PRESETS menu
        # renders slots, not sliders, so there is no widget to repaint. Session-
        # only on the Csound side (giPresetTrim ftable).
        ui._touch()
        ui.param_values[addr] = float(val)
    elif addr == '/stereo_in':
        # input-mode mirror (true stereo vs legacy mono sum) — no widget yet,
        # cached so future views/dashboards can read it.
        ui._touch()
        ui.param_values[addr] = int(val)
    elif addr in ('/cb_bpm', '/cb_subdiv'):
        # CHASE BLISS master clock debug telemetry (chase_bliss_clock.orc
        # INTEGRATION NOTES item 3) — display-only cache, no toggle UI.
        ui._touch()
        ui.param_values[addr] = float(val)
    elif addr == '/metro_state':
        # Test metronome on/off (gkMetroOn) — display-only cache, no
        # footswitch binding yet (chase_bliss_clock.orc INTEGRATION NOTES 2).
        ui._touch()
        ui.param_values[addr] = int(val)
    elif addr == '/blp_active':
        # How many looper channels BLOOPER is currently granulating. Drives the
        # CHASE BLISS slot badge (and warns "NO LOOP" when there is nothing to
        # granulate — the one thing that would otherwise look like a dead effect).
        ui._touch()
        ui.param_values[addr] = int(val)
        if ui.current_view == f'menu{CB_MENU}':
            ui.dirty = True
    elif addr == '/hab_rec':
        # HABIT ring buffer: 1 = recording, 0 = frozen. Slot badge + effect view.
        ui._touch()
        ui.param_values[addr] = int(val)
        if ui.current_view in (f'menu{CB_MENU}', 'hab'):
            ui.dirty = True
    elif addr == '/lp_drum_ch':
        # Drum sample mode (drumloops.orc): channel holding the one drum loop
        # (1-5, -1 = none). Exact match — keep ABOVE the startswith('/lp')
        # family so it can never be swallowed by a substring branch.
        ui._touch()
        ui.lp_drum_ch = int(val)
        if ui.current_view == f'menu{LOOPER_MENU}':
            ui.dirty = True
    elif addr == '/lp_drum_idx':
        # Loaded file index into the sorted drum list, -1 = none (the random
        # pick per detent happens csound-side; the index is authoritative).
        ui._touch()
        ui.lp_drum_idx = int(val)
        if ui.current_view == f'menu{LOOPER_MENU}':
            ui.dirty = True
    elif addr == '/lp_drum_n':
        # Number of files the drum folder scan found — invalidates the UI-side
        # filename cache when it changes (see _drum_filename).
        ui._touch()
        ui.lp_drum_count = int(val)
        if ui.current_view == f'menu{LOOPER_MENU}':
            ui.dirty = True
    elif addr.startswith('/lp') and '_state' in addr:
        ch = int(addr[3]) - 1   # /lp1_state → index 0
        if 0 <= ch < 5:
            ui._touch()
            was_recording = ui.is_looper_recording()
            ui.lp_states[ch] = int(val)
            now_recording = ui.is_looper_recording()
            # TASK 3c fix: this used to gate ui.dirty on the LOOPER menu
            # ALONE, so a global "is anything recording" indicator (the
            # faces/sleep suppression, and any future recording badge) went
            # stale on every other page — lp_states[] itself always stayed
            # correct (the assignment above is unconditional), only the
            # repaint that would SHOW the change was being swallowed
            # elsewhere. A recording transition now also forces a repaint
            # regardless of the current view.
            if (ui.current_view == f'menu{LOOPER_MENU}'
                    or was_recording != now_recording):
                ui.dirty = True
    elif addr.startswith('/lp') and '_pos' in addr:
        ch = int(addr[3]) - 1
        if 0 <= ch < 5:
            ui._touch()
            ui.lp_pos[ch] = float(val)
            if ui.current_view == f'menu{LOOPER_MENU}':
                ui.dirty = True
    elif addr.startswith('/lp') and '_speed' in addr:
        ch = int(addr[3]) - 1
        if 0 <= ch < 5:
            ui._touch()
            ui.lp_speed[ch] = int(val)
            if ui.current_view == f'menu{LOOPER_MENU}':
                ui.dirty = True
    elif addr == '/sm_state':
        # SoundMagic rack: (fx, on) -- engine-authoritative tile state.
        ui._touch()
        if isinstance(val, (tuple, list)) and len(val) >= 2 and ui.sm.on_state(val[0], val[1]):
            if ui.current_view == f'menu{NORRIS_MENU}':
                ui.dirty = True
    elif addr == '/sm_p':
        # SoundMagic rack: (fx, p, val) -- param echo (also snaps optimistic drags).
        ui._touch()
        if isinstance(val, (tuple, list)) and len(val) >= 3:
            if ui.sm.on_param(val[0], val[1], val[2]) \
                    and (ui.current_view == f'menu{NORRIS_MENU}' or ui.sm.sheet is not None):
                ui.dirty = True
    elif addr == '/sm_hero':
        # SoundMagic rack: (fx, k, idx) -- hero slot remap, learned live.
        ui._touch()
        if isinstance(val, (tuple, list)) and len(val) >= 3 and ui.sm.on_hero(val[0], val[1], val[2]):
            if ui.current_view == f'menu{NORRIS_MENU}':
                ui.dirty = True
    elif addr == '/sm_pedal':
        # SoundMagic rack: (fx, idx) -- pedal (CC29) target, drawn as a PEDAL tag.
        ui._touch()
        if isinstance(val, (tuple, list)) and len(val) >= 2 and ui.sm.on_pedal(val[0], val[1]):
            if ui.current_view == f'menu{NORRIS_MENU}':
                ui.dirty = True
    elif addr == '/master_out':
        # UTILITY page master output volume (0..1.5) — engine-authoritative.
        # BUT ignore the echo while the finger is dragging this slider: the local
        # drag advances master_out every frame while the CC goes out only ~30 Hz
        # and quantized to 0..127, so a stale, coarse echo landing mid-drag would
        # snap the thumb backward and the next move would push it forward again —
        # the "jumps only while moving" bug. The drag owns the value; the echo
        # reconciles once the finger lifts (same discipline as the param sliders).
        ui._touch()
        if ui.drag_target != 'mvol':
            ui.master_out = float(val)
            if ui.current_view == f'menu{UTILITY_MENU}':
                ui.dirty = True
    elif addr == '/master_bpm':
        # UTILITY page manual BPM setting (may differ from the live tempo below).
        # Same drag-guard as /master_out above — don't let a mid-drag echo fight
        # the finger.
        ui._touch()
        if ui.drag_target != 'mbpm':
            ui.master_bpm = float(val)
            if ui.current_view == f'menu{UTILITY_MENU}':
                ui.dirty = True
    elif addr == '/clock_bpm':
        # LIVE authoritative tempo — what is actually running (a loop or an
        # external MIDI clock may drive it away from the manual /master_bpm).
        ui._touch()
        ui.clock_bpm = float(val)
        ui.clock_bpm_seen = True
        if ui.current_view == f'menu{UTILITY_MENU}':
            ui.dirty = True
    elif addr == '/rec_state':
        # Master RECORD state (gkRecState): 1 = recording, 0 = idle. The engine
        # broadcasts this, so it is authoritative over the demiurge-record poll
        # (which only seeds the indicator on cold entry to the page).
        ui._touch()
        ui.recording = bool(int(val))
        if ui.current_view == f'menu{UTILITY_MENU}':
            ui.dirty = True
    elif addr == '/mvd_reps':
        # POLY DLY repeats 1..8 — engine-authoritative.
        ui._touch()
        ui.mvd_reps = max(1, min(8, int(val)))
        if ui.current_view == 'mvd':
            ui.dirty = True
    elif addr == '/mvd_intv':
        # POLY DLY pitch interval, signed semitones -12..+12.
        ui._touch()
        ui.mvd_intv = max(-12, min(12, int(val)))
        if ui.current_view == 'mvd':
            ui.dirty = True
    elif addr.startswith('/') and '_' in addr:
        code, param = addr[1:].split('_', 1)
        known_param = any(p[0] == param for p in EFFECT_PARAMS.get(code, []))
        if param == 'state' and code in ALL_EFFECTS:
            ui.handle_state_change(code, val)
        if known_param:
            ui.update_param(addr, val)


# ---------------------------------------------------------------------------
# Companion MIDI input
# ---------------------------------------------------------------------------

def choose_midi_port():
    """Pick the MIDI input the companion reads. NEPTR_MIDI_PORT overrides
    (substring match). Returns None (companion disabled, normal UI fine) if
    mido or any port is unavailable.

    Preference order matters: read the Teensy's OWN hardware USB-MIDI port
    first. It emits CCs whenever the Teensy is plugged in, completely
    independent of Csound / the demiurge audio engine — which is what wires the
    Teensy into "Midi Through" (14:0). Reading "Midi Through" would make sensor
    streaming die whenever the audio engine is off, so the raw source comes
    first; the bridge / Through aggregation are only fallbacks."""
    want = os.environ.get("NEPTR_MIDI_PORT")
    try:
        import mido
        names = mido.get_input_names()
    except Exception as exc:                       # mido/backend missing
        log.warning("companion: MIDI unavailable (%s)", exc)
        return None
    if want:
        for n in names:
            if want in n:
                return n
        return want                                 # try verbatim
    # Teensy raw source first (audio-engine-independent), then bridge, then the
    # Through aggregation as a last resort.
    for kw in ("Teensy", "NEPTR Serial Bridge", "Bridge", "Through"):
        for n in names:
            if kw.lower() in n.lower():
                return n
    return names[0] if names else None


def start_companion_input(ui, q):
    """Spawn the MIDI reader thread. Returns the stop Event (or None if no
    port). The reader feeds ui.input_state and wakes the main loop on control
    changes by enqueuing a sentinel."""
    port = choose_midi_port()
    if not port:
        log.warning("companion: no MIDI input port found; companion disabled")
        return None
    stop = threading.Event()

    def wake():
        q.put(("__wake__", 0))

    # Telemetry publishing for the terminal companion is handled by the
    # UI-independent `demiurge-monitor` system service now — the UI only reads
    # MIDI to drive its own companion menu / dashboard.
    threading.Thread(
        target=companion_input.read_loop,
        args=(ui.input_state, port, wake, stop),
        daemon=True,
    ).start()
    log.info("companion: MIDI input on %s", port)
    return stop


# ---------------------------------------------------------------------------
# Display / main loop
# ---------------------------------------------------------------------------

def main():
    rot = int(os.environ.get("NEPTR_ROT", "270"))
    if rot not in (0, 90, 180, 270):
        rot = 270
    windowed = os.environ.get("NEPTR_WINDOWED", "0") == "1"

    pygame.display.init()
    pygame.font.init()

    if windowed:
        scale = float(os.environ.get("NEPTR_SCALE", "0.45"))
        out_w, out_h = (W, H) if rot in (0, 180) else (H, W)
        screen = pygame.display.set_mode((int(out_w * scale), int(out_h * scale)))
        pygame.display.set_caption("NEPTR Phase 4 UI")
    else:
        screen = pygame.display.set_mode((0, 0), pygame.FULLSCREEN)
    pygame.mouse.set_visible(False)

    ui = NeptrUI()
    ui.load_faces()

    # "Remember where I was": re-apply the persisted UI position BEFORE any
    # engine traffic arrives. Only view/cursor state is restored — every DSP
    # fact is filled in moments later by the CC102 snapshot below.
    saved = ui_state.load(ui.state_store.path)
    ui.restore_position(saved)
    log.info("ui-state: restored view=%s menu=%s companion=%s/%s from %s",
             ui.current_view if ui.restore_effect is None else ui.restore_effect,
             ui.last_menu_index, saved.get("companion"),
             saved.get("companion_screen"), ui.state_store.path)

    q = queue.Queue()
    dispatcher = Dispatcher()

    def enqueue(addr, *args):
        # Single-argument messages stay a bare scalar — that is what every
        # existing route_osc branch expects and hundreds of them rely on it.
        # MULTI-argument messages pass the whole tuple through instead: before
        # 2026-09-15 they were silently truncated to args[0], which made
        # /hg_lane (lane, on, index) arrive as just the lane number. Nothing
        # sent more than one argument until then, so this widens the contract
        # without changing any existing behaviour.
        if len(args) > 1:
            q.put((addr, args))
        elif args:
            q.put((addr, args[0]))
    dispatcher.set_default_handler(enqueue)

    # The OSC port is shared ground: demiurge-web can hold it (it is meant to
    # yield, but a stale or older build may not). Losing the engine's state
    # feed must NEVER cost the user the screen they play with, so bind in the
    # background and keep trying. Exiting here put this process in a systemd
    # restart loop and left the panel black.
    osc_server_ref = {}

    def serve_osc():
        delay = 1.0
        while True:
            try:
                srv = ThreadingOSCUDPServer(("127.0.0.1", 8000), dispatcher)
            except OSError as exc:
                log.warning("OSC port 127.0.0.1:8000 busy (%s); retrying in "
                            "%.0fs — controls work, engine state is stale",
                            exc.strerror or exc, delay)
                time.sleep(delay)
                delay = min(delay * 1.5, 15.0)
                continue
            log.info("OSC listening on 127.0.0.1:8000")
            osc_server_ref["srv"] = srv
            ui.engine.sync_request()   # late bind: resync once we can hear
            srv.serve_forever()
            return

    threading.Thread(target=serve_osc, daemon=True).start()
    log.info("UI up, display %s rot=%d", screen.get_size(), rot)

    companion_stop = start_companion_input(ui, q)

    # Late-join sync: ask the engine to rebroadcast its full OSC state
    # (ch16 CC102), retrying a few times until any OSC arrives — a UI
    # (re)start mid-session comes up with live slot/param state.
    ui.engine.sync_request()
    ui.sync_retries = 1
    ui.sync_retry_dt = SYNC_RETRY_SECS
    ui.sync_retry_at = time.time() + SYNC_RETRY_SECS
    ui.sync_deadline = time.time() + SYNC_RETRY_SECONDS_TOTAL
    ui.sync_quiet_until = time.time() + SYNC_QUIET_SECS

    # Touch input source (2026-08-09 rework): on-device, SDL2/KMSDRM drops
    # ALL touch motion on the resistive panel (down/up pairs at stale
    # coordinates — gestures impossible at the SDL layer), so the UI reads
    # the kernel evdev stream directly (touch_evdev.py, daemon-style) and
    # IGNORES SDL finger/mouse events entirely while the raw reader is
    # active. Mac dev mode: no evdev -> plain SDL mouse, as before. The
    # mapper applies ~/demiurge/touch-cal.conf + the NEPTR_ROT transform.
    meter_w = int(screen.get_size()[0] * METER_FRAC / 2)
    mapper = TouchMapper(rot, screen.get_size(), inset_x=meter_w, inset_y=meter_w)
    raw_touch = touch_evdev.TouchReader(wake=lambda: q.put(("__wake__", 0)))
    smoother = touch_evdev.Smoother()
    bridger = touch_evdev.GapBridger()
    use_evdev = raw_touch.start()
    if use_evdev:
        log.info("touch: RAW evdev reader on %s (%s) ranges=%s — "
                 "SDL touch/mouse events ignored",
                 raw_touch.dev, raw_touch.name, raw_touch.ranges)
    else:
        log.info("touch: no raw evdev device; SDL mouse/finger events drive touch")

    # pygame rotates counterclockwise for positive angles; map so the default
    # matches the previous Kivy orientation. If the panel comes up upside
    # down, flip NEPTR_ROT between 90 and 270 — no code change needed.
    rot_angle = {0: 0, 90: 90, 180: 180, 270: -90}[rot]

    # The rotate+smoothscale is the expensive half of present() and the
    # canvas usually has not changed between meter updates, so the transformed
    # surface is cached and only rebuilt when the canvas actually repaints.
    # A meter tick is then one blit of that cache plus two thin bars.
    present_cache = {"surf": None}
    sw_, sh_ = screen.get_size()
    inner_w = sw_ - 2 * meter_w
    content_h = sh_ - meter_w          # canvas height under the compute-meter band

    # Metro dot lives in the top-left meter_w x meter_w square (left of the
    # DSP bar, above the left meter strip's content) — see _draw_metro_dot.
    # Centred in that square, sized to 80% of it, derived from meter_w so it
    # can never drift from the meters/bar. The down/beat size contrast is
    # kept at the same ratio the old fixed-px values used (7:5) so it still
    # reads clearly at the larger size.
    dot_cx = dot_cy = meter_w / 2.0
    dot_r_down = 0.4 * meter_w                     # 80% of available diameter
    dot_r_beat = dot_r_down * (BEAT_DOT_R_BEAT / BEAT_DOT_R_DOWN)

    def _meter_bar(x, w, val, peak, clip_t=0.0):
        """One edge strip. val/peak are linear PEAK; drawn as dBFS."""
        pygame.draw.rect(screen, (8, 8, 10), (x, 0, w, sh_))
        if ui.meter_seen and (time.time() - ui.meter_seen) > 2.0:
            return                              # engine quiet: leave it dark
        def _db_frac(v):
            if v <= 1e-6:
                return 0.0
            db = 20.0 * math.log10(v)
            return max(0.0, min(1.0, (db - METER_FLOOR_DB) / -METER_FLOOR_DB))
        f = _db_frac(val)
        pad = max(2, w // 8)
        bx, bw = x + pad, w - 2 * pad
        h = int(sh_ * f)
        if h > 0:
            db = 20.0 * math.log10(max(val, 1e-6))
            col = (235, 60, 60) if db > METER_CLIP_DB else \
                  (235, 176, 60) if db > METER_HOT_DB else ACCENT
            pygame.draw.rect(screen, col, (bx, sh_ - h, bw, h))
        pf = _db_frac(peak)
        if pf > 0:
            py = sh_ - int(sh_ * pf)
            pygame.draw.rect(screen, (255, 255, 255), (bx, max(0, py - 2), bw, 2))
        # CLIP: a solid red cap across the top of the strip, held 3 s.
        if clip_t and (time.time() - clip_t) < 3.0:
            pygame.draw.rect(screen, (255, 40, 40), (x, 0, w, max(6, sh_ // 90)))

    def _draw_meters():
        _meter_bar(0, meter_w, ui.meter_l, ui.meter_pk_l, ui.meter_clip_l)
        _meter_bar(sw_ - meter_w, meter_w, ui.meter_r, ui.meter_pk_r,
                   ui.meter_clip_r)

    def _draw_metro_dot(now_):
        """A dot, centred in the top-left meter_w x meter_w screen-space
        square (to the LEFT of the DSP bar, which starts at x=meter_w; ABOVE
        where the left meter strip's active fill normally sits), that blinks
        on the beat using ui._beat_phase()'s local predictor. Overlays the
        header on EVERY view (never added to ui.hit — purely visual).

        Position/size are screen-space, derived from meter_w exactly like
        _draw_meters()/_draw_dsp_bar() — a physical-corner concept, not a
        canvas coordinate, so this deliberately does NOT go through
        mapper.canvas_to_screen(); it is already correct under every
        NEPTR_ROT value by construction. Centre: (meter_w/2, meter_w/2).
        Radius: up to 0.4*meter_w (80% of the square's available diameter),
        set up as dot_r_down/dot_r_beat above.
        """
        phase = ui._beat_phase(now_)
        if phase is None:
            return                      # beat clock not live: draw nothing
        is_down = bool(ui.beat_down)
        k = max(0.0, 1.0 - phase / BEAT_FLASH_FRAC) if phase < BEAT_FLASH_FRAC else 0.0
        base_r = dot_r_down if is_down else dot_r_beat
        r = max(2, int(base_r * (0.55 + 0.45 * k)))
        if k > 0.05:
            col = FG if is_down else ACCENT   # downbeat flash reads distinct (white)
        else:
            col = ACCENT_DIM                  # dim "idle" dot between flashes
        pygame.draw.circle(screen, col, (int(dot_cx), int(dot_cy)), r)

    def _lerp_rgb(a, b, t):
        t = 0.0 if t < 0.0 else (1.0 if t > 1.0 else t)
        return (a[0] + (b[0] - a[0]) * t,
                a[1] + (b[1] - a[1]) * t,
                a[2] + (b[2] - a[2]) * t)

    def _draw_dsp_bar(now_):
        """TASK 3b: the honest callback-wall-time/quantum fraction measured
        in src/demiurge-csound-host.cpp (the only place that knows the real
        deadline), 1.0 = xrun.

        Position: flush to the PHYSICAL top edge of the screen (y=0 in
        screen space), spanning the inner width between the two level-meter
        strips. This is drawn directly in screen space exactly the way
        _draw_meters() draws its edge strips — not mapped from a canvas
        coordinate via mapper.canvas_to_screen like the metro dot — because
        "top of the screen" is a physical-display concept (like the
        meters' left/right edges), not a point that lives inside the
        rotating canvas content. Screen space IS already post-rotation, so
        this needs no rotation math at all: y=0 is the real top edge under
        every NEPTR_ROT value by construction.

        Thickness: exactly meter_w (the same screen-px value _draw_meters()
        uses for its strip width, itself derived from METER_FRAC) — one
        shared number, not a second constant that could drift from it.

        Z-order: drawn AFTER _draw_meters() so it caps between them. The
        metro dot (_draw_metro_dot(), drawn after this) no longer lives
        inside this bar's footprint at all — it sits in the top-left
        meter_w x meter_w square to the LEFT of x0, which this bar's inset
        leaves empty, so the two no longer need a stacking resolution.
        """
        thickness = meter_w                      # shared with _draw_meters
        x0, x1 = meter_w, sw_ - meter_w
        track = pygame.Rect(x0, 0, max(0, x1 - x0), thickness)
        pygame.draw.rect(screen, DSP_BAR_TRACK, track)

        stale = (not ui.dsp_load_seen
                 or (now_ - ui.dsp_load_seen) > DSP_LOAD_STALE_SECS)
        if stale:
            # Dark/empty rather than frozen — and drop the slew history so
            # telemetry resuming after a gap attacks fresh instead of
            # lurching from a long-stale number (route_osc's '/dsp_load'
            # handler snaps instead of slewing whenever dsp_load_disp_t is
            # None).
            ui.dsp_load_disp = 0.0
            ui.dsp_load_disp_t = None
            return

        # Slewing already happened in route_osc's '/dsp_load' handler
        # (real elapsed time between messages, not frame count) — just
        # read the result here.
        load = ui.dsp_load_disp

        flashing = now_ < ui.dsp_xrun_flash_until
        if flashing or load >= 0.995:
            col = DSP_RED
        elif load <= DSP_LOAD_AMBER:
            col = _lerp_rgb(DSP_BLUE, DSP_AMBER, load / DSP_LOAD_AMBER)
        else:
            col = _lerp_rgb(DSP_AMBER, DSP_RED,
                             (load - DSP_LOAD_AMBER) / (1.0 - DSP_LOAD_AMBER))

        # Float width the whole way through, round once at the end: the
        # input (load) now moves smoothly thanks to the slew above, so a
        # single round() per frame doesn't reintroduce 1px rounding jitter.
        fw = min(1.0, load) * (x1 - x0)
        fill_w = int(round(fw))
        if fill_w > 0:
            pygame.draw.rect(screen, col, pygame.Rect(x0, 0, fill_w, thickness))

    def present(canvas_changed=True):
        if canvas_changed or present_cache["surf"] is None:
            surf = ui.canvas
            if rot_angle:
                surf = pygame.transform.rotate(surf, rot_angle)
            if surf.get_size() != (inner_w, content_h):
                surf = pygame.transform.smoothscale(surf, (inner_w, content_h))
            present_cache["surf"] = surf
        # Compute-meter band: the top meter_w px are reserved for the bar, the
        # canvas lives entirely BELOW it (mapper.inset_y mirrors this).
        screen.blit(present_cache["surf"], (meter_w, meter_w))
        _draw_meters()
        now_overlay = time.time()
        _draw_dsp_bar(now_overlay)
        _draw_metro_dot(now_overlay)
        # touch press feedback: a brief glow pulse at the tap point, drawn
        # straight on the output (no canvas copy — one small additive blit).
        if ui.press_fx is not None:
            fx_x, fx_y, fx_t0 = ui.press_fx
            k = (time.time() - fx_t0) / PRESS_FX_SECS
            if k < 1.0:
                r_ = 40 + int(36 * k)
                g = glow_dot(r_, ACCENT)
                sx, sy = mapper.canvas_to_screen(fx_x, fx_y)
                screen.blit(g, (int(sx) - r_, int(sy) - r_),
                            special_flags=pygame.BLEND_ADD)
        pygame.display.flip()

    cpu_load = CpuLoad()
    cpu_next_at = 0.0
    finger_native = False          # latch: ignore SDL's touch->mouse echo
    last_present = 0.0
    running = True

    # systemd stops the UI with SIGTERM (`systemctl restart neptr-ui`). Catch
    # it so the loop exits normally and the final position reaches disk —
    # otherwise a restart always loses the last few seconds of navigation.
    def _term(_sig, _frm):
        nonlocal running
        running = False
        q.put(("__wake__", 0))
    for _sig in (signal.SIGTERM, signal.SIGINT):
        try:
            signal.signal(_sig, _term)
        except (ValueError, OSError):
            pass                    # not on the main thread / unsupported

    while running:
        now = time.time()
        timeout = max(0.01, min(1.0, ui.next_deadline(now) - now))
        if not ui.is_sleeping:
            # Awake, the loop also has to notice SDL touch events promptly
            # (they don't wake the OSC queue). 20 Hz idle poll — empty
            # event.get + no repaint, negligible CPU. Asleep, the face
            # timers (~0.75 s) already pace the loop, and SDL queues any
            # wake-tap until then, so nothing is lost.
            timeout = min(timeout, 0.05)
        if ui.touch_start is not None or ui.drag_target is not None \
                or ui._lp_touch_cc is not None:
            timeout = min(timeout, 1 / 60)   # finger down: 60 Hz tracking
        try:
            msg = q.get(timeout=timeout)
        except queue.Empty:
            msg = None

        # Drain the burst: many OSC messages can arrive at once; apply them
        # all, then repaint a single time. "__wake__" is the companion MIDI
        # thread nudging us to re-evaluate input — no routing needed.
        while msg is not None:
            if msg[0] != "__wake__":
                route_osc(ui, *msg)
            try:
                msg = q.get_nowait()
            except queue.Empty:
                msg = None

        # Raw evdev touch (the ONLY touch source when active). Runs of
        # 'move's are coalesced to the latest position per drain; every
        # down/up survives in order for the gesture classifier.
        if use_evdev:
            tnow = time.time()
            pending = []
            for kind, rx, ry in touch_evdev.coalesce(raw_touch.drain()):
                cx, cy = mapper.raw(rx, ry, raw_touch.ranges)
                pending.extend(bridger.apply(kind, cx, cy, tnow))
            pending.extend(bridger.flush(tnow))
            for kind, cx, cy in pending:
                cooked = smoother.apply(kind, cx, cy)
                if cooked is None:
                    continue                      # micro-jitter, dropped
                kind, cx, cy = cooked
                if TOUCH_DEBUG and kind != "move":
                    log.info("touch-evdev: %s canvas=(%d,%d)", kind, cx, cy)
                if kind == "down":
                    ui.touch_down(cx, cy)
                elif kind == "move":
                    ui.touch_move(cx, cy)
                else:
                    ui.touch_up(cx, cy)

        for ev in pygame.event.get():
            if ev.type == pygame.QUIT:
                running = False
            elif use_evdev and ev.type in (
                    pygame.FINGERDOWN, pygame.FINGERMOTION, pygame.FINGERUP,
                    pygame.MOUSEBUTTONDOWN, pygame.MOUSEMOTION,
                    pygame.MOUSEBUTTONUP):
                pass                  # raw reader owns touch; no double-fire
            elif ev.type == pygame.FINGERDOWN:
                finger_native = True
                ui.touch_down(*mapper.finger(ev))
            elif ev.type == pygame.FINGERMOTION:
                ui.touch_move(*mapper.finger(ev))
            elif ev.type == pygame.FINGERUP:
                ui.touch_up(*mapper.finger(ev))
            elif ev.type == pygame.MOUSEBUTTONDOWN and not finger_native:
                ui.touch_down(*mapper.mouse(ev.pos))
            elif ev.type == pygame.MOUSEMOTION and not finger_native:
                if ui.touch_start is not None or ui.drag_target is not None:
                    ui.touch_move(*mapper.mouse(ev.pos))
            elif ev.type == pygame.MOUSEBUTTONUP and not finger_native:
                ui.touch_up(*mapper.mouse(ev.pos))
            elif (windowed and ev.type == pygame.KEYDOWN
                  and ev.key in (pygame.K_ESCAPE, pygame.K_q)):
                running = False       # dev-window convenience only

        now = time.time()
        repaint = False

        # Compute meter source: real CPU use, polled once per second. The
        # bar repaints on the existing 25 Hz cheap path; this only updates
        # the value (fast attack, slow release like before).
        if now >= cpu_next_at:
            cpu_next_at = now + 1.0
            cpu = cpu_load.sample()
            if cpu is not None:
                prev_disp = ui.dsp_load_disp if ui.dsp_load_disp_t else cpu
                ui.dsp_load_disp = cpu if cpu > prev_disp else \
                    prev_disp + (cpu - prev_disp) * 0.5
                ui.dsp_load_disp_t = now
                ui.dsp_load = cpu
                ui.dsp_load_seen = now

        # Companion: evaluate the hold/nav/idle state machine every iteration
        # (it needs ticking even with no new MIDI, e.g. to time the 3s hold).
        if ui.tick_companion(now):
            repaint = True

        # Timers
        # TASK 3c: never let the faces/sleep screen come up mid-take — any
        # loop channel still physically recording (states 1/5/6, see
        # is_looper_recording()) blocks entry. Waking back up while already
        # asleep needs no extra code here: the /lp{n}_state OSC handler
        # already calls ui._touch(), which wakes immediately on ANY state
        # change, REC included.
        if (not ui.is_sleeping and not ui.companion.active
                and now - ui.last_osc_time > SLEEP_AFTER
                and not ui.is_looper_recording()):
            ui.enter_sleep()
        if ui.pending_switch_at is not None and now >= ui.pending_switch_at:
            ui.enter_effect_view(ui.pending_switch_code)
        if ui.is_sleeping and ui.tick_faces(now):
            ui.dirty = True

        # Engine-link timers: sync-request retries + param-slider chase.
        # Keep asking until a real SNAPSHOT lands (not merely "some OSC"), on
        # a backing-off interval — on a cold boot the engine is still starting
        # long after the UI is up, and the answer only exists once it is.
        if ui.resync_pending:
            ui.resync_pending = False
            ui.synced = False
            ui._sync_addrs.clear()
            ui.engine.sync_request()
            ui.sync_retries += 1
            ui.sync_retry_dt = SYNC_RETRY_SECS
            ui.sync_retry_at = now + SYNC_RETRY_SECS
            ui.sync_deadline = now + SYNC_RETRY_SECONDS_TOTAL
            ui.sync_quiet_until = now + SYNC_QUIET_SECS
            log.info("engine sync: traffic after a long silence — re-syncing")
        if ui.sync_retry_at is not None and now >= ui.sync_retry_at:
            if ui.synced or (ui.sync_deadline is not None
                             and now >= ui.sync_deadline):
                if not ui.synced:
                    log.warning("engine sync: no snapshot after %d requests; "
                                "giving up (engine down?)", ui.sync_retries)
                ui.sync_retry_at = None
            else:
                ui.engine.sync_request()
                ui.sync_retries += 1
                ui.sync_retry_dt = min(ui.sync_retry_dt * SYNC_RETRY_BACKOFF,
                                       SYNC_RETRY_MAX)
                ui.sync_retry_at = now + ui.sync_retry_dt
                ui.sync_quiet_until = now + SYNC_QUIET_SECS
        # The snapshot may have completed the picture a restored effect view
        # was waiting on (menu + on-state).
        if ui.restore_effect is not None and ui.synced:
            # One shot: the whole burst is drained in a single loop pass, so
            # by now menu + on-state are both known. Still off -> the user
            # stays on the menu view rather than in a dead effect page.
            if not ui._try_restore_effect():
                ui.restore_effect = ui.restore_menu = None
        ui.tick_param_chase(now)
        if ui.sm.tick(now, ui.touch_start is not None, ui.touch_moved):
            ui.dirty = True

        # Persist the UI position (debounced + change-detected inside).
        ui.state_store.tick(now, ui.state_snapshot())

        # WAVE stroke trail fade-out repaints (cheap; only while it exists).
        if ui._stroke_trail and ui.on_wave():
            ui.dirty = True
        # Cancelled pressed-highlight fade frames.
        if ui.pressed_fade is not None:
            ui.dirty = True

        # Companion live-data repaint cadence + metric polling + message expiry.
        if ui.companion.active:
            ui._refresh_pi(now)
            if ui.companion.screen == companion_mod.AUDIO:
                ui._refresh_interfaces(now)         # rate-limited re-list
                ui._refresh_audio_settings(now)
                ui._refresh_graph(now)
            elif ui.companion.screen == companion_mod.NETWORK:
                ui._refresh_wifi(now)
            if (ui.companion_refresh_at is not None
                    and now >= ui.companion_refresh_at):
                ui.dirty = True
                ui.companion_refresh_at = now + COMPANION_REFRESH_DT
            if (ui.companion_msg_until is not None
                    and now >= ui.companion_msg_until):
                ui.companion_msg = None
                ui.companion_msg_until = None
                ui.dirty = True

        # Wavetable OSC send: debounced to WAVE_SEND_DEBOUNCE after the last
        # edit commit (finger lift / clear / undo).
        if ui.wt_send_at is not None and now >= ui.wt_send_at:
            ui.send_wavetable()

        # Press-glow pulse: keep repainting while it runs, one last repaint
        # to erase it when it expires.
        if ui.press_fx is not None:
            if now - ui.press_fx[2] >= PRESS_FX_SECS:
                ui.press_fx = None
            repaint = True

        if ui.dirty:
            repaint = ui.render() or repaint
        if ui.tick_transition(now):
            repaint = True
        if ui.tick_slide(now):
            repaint = True

        if repaint:
            # During an active wave/bars drag, cap presents at ~25 fps — the
            # canvas repaint is cheap but the rotate+scale present is not
            # free on the Pi, and it shares cores with csound.
            if (ui.drag_target in ('wave', 'bars')
                    and now - last_present < 0.04):
                ui.dirty = True          # coalesce into the next loop pass
            else:
                present()
                last_present = now
        elif now - last_present >= 0.04:
            # No canvas change, but the level strips must still move. This is
            # the cheap path: blit the CACHED transformed canvas and repaint
            # two thin bars - no rotate, no smoothscale, no canvas render.
            # 25 Hz matches the engine's send rate exactly, so nothing is
            # dropped and nothing is drawn twice.
            present(canvas_changed=False)
            last_present = now

    ui.state_store.flush(ui.state_snapshot())   # final position, on the way out
    if companion_stop is not None:
        companion_stop.set()
    raw_touch.stop()
    ui.engine.close()
    # The OSC server is created on a background thread now and may not exist
    # yet (or at all, if the port stayed busy), so shutting it down is
    # best-effort — a clean exit must not depend on it.
    srv = osc_server_ref.get("srv")
    if srv is not None:
        srv.shutdown()
    pygame.quit()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("Program interrupted.")
