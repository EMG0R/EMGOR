#!/usr/bin/env python3
"""neuralGrid live hardware-input monitor.

Runs ON the Pi, owns /dev/ttyUSB0 via neuralgrid.link.GridLink, and serves a
single-page dark UI over HTTP with a Server-Sent-Events stream:

    python3 monitor.py                      # defaults: /dev/ttyUSB0, 0.0.0.0:8765
    python3 monitor.py --port /dev/ttyUSB0 --http-port 8765

Open http://neuralgrid.local:8765 from any machine on the LAN.

Shows:
  * the 8x8 key grid, cells lighting live on press/release
  * the 4 SoftPot lanes (N/S/E/W + GPIO), raw ADC bar + value, rolling
    sparkline, jitter (max consecutive jump over the last ~10 samples),
    and the TouchDetector's gated touched/position — clearly separated
    from the raw feed (raw is violently noisy: no pulldowns fitted)
  * the 4 VL53L1X lidar lanes (N/S/E/W, addresses 0x2A..0x2D), range in mm
    with a bar scaled 0..2000mm; a channel reporting 0xFFFF (or a firmware
    that sends no LIDAR frames at all — e.g. the original grid_link) shows
    as DEAD / absent
  * connection status: HELLO info, analog frames/sec, key event count

stdlib only (plus pyserial via neuralgrid.link). SSE, no websockets.
UI updates are downsampled to ~15 fps regardless of the 50 Hz analog rate.
"""

from __future__ import annotations

import argparse
import colorsys
import glob
import json
import math
import sys
import threading
import time
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse, parse_qs

# monitor.py lives in pi/tools/; the neuralgrid package lives in pi/.
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
sys.path.insert(0, str(Path(__file__).resolve().parent))  # sibling latency_probe.py

from neuralgrid import codec, codec_lidar  # noqa: E402
from neuralgrid.link_lidar import LidarGridLink  # noqa: E402
from neuralgrid.touch import TouchDetector  # noqa: E402
import latency_probe  # noqa: E402

GRID_W = 8
GRID_H = 8
N_ANALOG = 4          # softpots only; TouchDetector stays scoped to these
CHANNEL_NAMES = ("N", "S", "E", "W")
CHANNEL_GPIOS = (32, 33, 34, 35)
N_FSR = 4             # fw6: channels [4..7] of the 8-channel ANALOG frame
FSR_NAMES = ("FSR0", "FSR1", "FSR2", "FSR3")
FSR_GPIOS = (36, 39, 15, 12)
FSR_THRESHOLD = 300   # raw 0..4095: simple "pressed" state, no gating logic
# Software-only sensitivity fix (fw/wiring untouched): the FSR lanes read far
# too hot on light touches, so a power-curve response is applied to the raw
# ADC value before it drives the bar/sparkline/threshold badge. The noise
# floor is subtracted and the remainder normalized to 0..1, then raised to
# FSR_GAMMA and rescaled back to the full 0..4095 range — this preserves the
# full output range (unlike a linear gain, which caps the max output) while
# still reducing sensitivity to light touches. Live-tunable via
# GET /fsr?gamma=<float>&floor=<int> (see MonitorState.fsr_gamma/fsr_floor);
# these two are just the startup defaults.
FSR_GAMMA = 3.3       # ~10x less sensitive at mid-press (0.5**3.3 ~ 0.1); tune live via /fsr?gamma=
FSR_FLOOR = 40        # raw counts of noise floor, subtracted before scaling
N_LIDAR = 4
LIDAR_NAMES = ("N", "S", "E", "W")
LIDAR_ADDRS = ("0x2A", "0x2B", "0x2C", "0x2D")
LIDAR_XSHUT = (4, 13, 14, 27)
LIDAR_BAR_MM = 2000       # lidar bar full-scale, mm
LIDAR_STALE_S = 1.0       # no LIDAR frame this long -> lanes show absent
SPARK_LEN = 60          # raw samples kept per channel for the sparkline
JITTER_WINDOW = 10      # samples for the max-consecutive-jump readout
UI_FPS = 15.0           # SSE downsample rate


LAVA_FPS = 20.0           # lavalamp render rate
LAVA_GLOBAL_BRIGHTNESS = 120  # sent once per connection; per-pad levels are
                              # scaled in the RGB values themselves
LAVA_MIN = 0.05           # idle field floor (fraction of full)
LAVA_MAX = 0.15           # idle field ceiling
LAVA_PRESS = 1.00         # held-pad level (direct on/off, no fade)
LAVA_HUE = 0.78           # HSV purple/violet centre
LAVA_HUE_VAR = 0.05       # +/- hue drift, stays clearly purple
LAVA_PARTIAL_MAX = 20     # send LEDS_PARTIAL when at most this many pads
                          # changed since the last sent frame (e.g. a single
                          # key press/release = 1 pad); above that, a full
                          # LEDS_FULL is cheaper than a long PARTIAL list.
                          # See fw5 latency work: this is the Pi-side half of
                          # "write only changed pixels" — the firmware now
                          # also diffs against its own shadow framebuffer, so
                          # this mainly saves serial bytes + a bit of Pi CPU,
                          # while a key-press pad-only update on the wire
                          # keeps the firmware's per-frame I2C work minimal
                          # even before its own diff kicks in.

# LED-ack + link-RTT rolling windows (fw5 telemetry; harmless no-ops <fw5)
LATENCY_WINDOW = 40

RETRY_S = 2.0             # port-absent / open-failure retry interval
HELLO_TIMEOUT_S = 12.0    # no HELLO this long despite pings -> reconnect
FIRST_HELLO_GRACE_S = 30.0  # after (re)open: opening the port resets the
                            # ESP32 (DTR toggle) and its boot — lidar init
                            # with dead sensors timing out — can take >12s;
                            # reconnecting mid-boot resets it again (thrash)
KEEPALIVE_S = 5.0         # ping interval (fw=2 answers every ping with HELLO)


def autodetect_port() -> str | None:
    """Stable by-id symlink first (survives ttyUSB0->ttyUSB1 renumbering
    after a replug), then raw ttyUSB nodes. Returns None when absent."""
    candidates = (
        sorted(glob.glob("/dev/serial/by-id/usb-Silicon_Labs_CP2102*"))
        + sorted(glob.glob("/dev/serial/by-id/*"))
        + sorted(glob.glob("/dev/ttyUSB*"))
    )
    return candidates[0] if candidates else None


class MonitorState:
    """All live hardware state, guarded by one lock; snapshotted per SSE tick."""

    def __init__(self) -> None:
        self.lock = threading.Lock()
        self.hello: dict | None = None
        self.hello_time: float | None = None
        self.hello_mono: float | None = None   # monotonic, for the watchdog
        # serial link supervision (written by SerialSupervisor)
        self.link_state = "no-device"          # no-device|connecting|connected|reconnecting
        self.link_port: str | None = None
        self.reconnects = 0
        self.pressed = [[False] * GRID_W for _ in range(GRID_H)]
        self.key_events = 0
        self.last_key: dict | None = None
        self.values = [0] * N_ANALOG
        self.spark = [deque(maxlen=SPARK_LEN) for _ in range(N_ANALOG)]
        self.detector = TouchDetector(n_channels=N_ANALOG)
        self.touch = [{"touched": False, "position": 0.0} for _ in range(N_ANALOG)]
        # fw6: FSR channels 4..7 — raw bar + sparkline + a simple threshold
        # state (no TouchDetector gating; that class is softpot-specific).
        self.fsr_values = [0] * N_FSR
        self.fsr_spark = [deque(maxlen=SPARK_LEN) for _ in range(N_FSR)]
        # live-tunable software sensitivity fix; see FSR_GAMMA/FSR_FLOOR above
        # and GET /fsr?gamma=&floor=
        self.fsr_gamma = FSR_GAMMA
        self.fsr_floor = FSR_FLOOR
        self.analog_frames = 0
        self.frame_errors = 0
        # lidar: raw u16 per channel (0xFFFF = invalid/dead), None = never seen
        self.lidar_values: list[int] | None = None
        self.lidar_frames = 0
        self.lidar_time: float | None = None
        # boot XSHUT diagnostic (TYPE_LIDARDIAG, fw>=3); None = never seen
        self.lidar_diag: dict | None = None
        # ambient LED renderer (lavalamp) on/off, toggled from the web UI
        self.lavalamp = True
        # fps bookkeeping
        self._fps_count = 0
        self._fps_t0 = time.monotonic()
        self.fps = 0.0
        # -- fw5 latency telemetry ------------------------------------------
        self.last_ping_mono: float | None = None       # set right before a
                                                        # PING write; consumed
                                                        # by the next HELLO
        self.link_rtt_ms = deque(maxlen=LATENCY_WINDOW)  # PING->HELLO, ms
        self.led_ack_full_ms = deque(maxlen=LATENCY_WINDOW)     # fw5+ TYPE_ACK
        self.led_ack_partial_ms = deque(maxlen=LATENCY_WINDOW)  # fw5+ TYPE_ACK
        self.timing: dict | None = None   # latest fw5 TYPE_TIMING, phase->us
        self.latency_test_running = False
        self.latency_test_result: dict | None = None

    # -- GridLink callbacks (called from the reader thread) ------------------

    def on_hello(self, h) -> None:
        hello = {
            "proto_ver": h.proto_ver,
            "grid_w": h.grid_w,
            "grid_h": h.grid_h,
            "n_analog": h.n_analog,
            "fw_ver": h.fw_ver,
        }
        with self.lock:
            changed = hello != self.hello
            self.hello = hello
            self.hello_time = time.time()
            self.hello_mono = time.monotonic()
            # link RTT = this HELLO's arrival minus the PING that (probably)
            # provoked it. Firmware answers HELLO immediately on PING, so
            # this is the round-trip link latency task 1(a) asks for.
            if self.last_ping_mono is not None:
                self.link_rtt_ms.append(
                    (time.monotonic() - self.last_ping_mono) * 1000.0)
                self.last_ping_mono = None
        if changed:  # fw=2 answers every keepalive ping — don't spam the log
            print(f"HELLO: proto={h.proto_ver} grid={h.grid_w}x{h.grid_h} "
                  f"analog={h.n_analog} fw={h.fw_ver}", flush=True)

    def on_ack(self, ack) -> None:
        """fw5+ TYPE_ACK: recv-complete -> show()-complete latency for one
        APPLIED LEDS_FULL/LEDS_PARTIAL frame."""
        ms = ack.apply_us / 1000.0
        with self.lock:
            if ack.acked_type == codec.TYPE_LEDS_FULL:
                self.led_ack_full_ms.append(ms)
            else:
                self.led_ack_partial_ms.append(ms)

    def on_timing(self, t) -> None:
        """fw5+ TYPE_TIMING: per-phase max-microseconds-since-last-report."""
        with self.lock:
            self.timing = t.as_dict()

    def on_key(self, k) -> None:
        with self.lock:
            if 0 <= k.y < GRID_H and 0 <= k.x < GRID_W:
                self.pressed[k.y][k.x] = k.pressed
            self.key_events += 1
            self.last_key = {"x": k.x, "y": k.y, "pressed": k.pressed}

    def on_analog(self, a) -> None:
        # fw6: a.values may hold 8 channels (softpot N/S/E/W, FSR0-3) or
        # still 4 against older firmware — index defensively either way.
        vals = list(a.values[:N_ANALOG])
        fsr_vals = list(a.values[N_ANALOG:N_ANALOG + N_FSR])
        with self.lock:
            readings = self.detector.update(vals + [0] * (N_ANALOG - len(vals)))
            for i, v in enumerate(vals):
                self.values[i] = v
                self.spark[i].append(v)
            self.touch = [
                {"touched": r.touched, "position": round(r.position, 4)}
                for r in readings
            ]
            for i, v in enumerate(fsr_vals):
                self.fsr_values[i] = v
                self.fsr_spark[i].append(v)
            self.analog_frames += 1
            self._fps_count += 1
            now = time.monotonic()
            dt = now - self._fps_t0
            if dt >= 1.0:
                self.fps = self._fps_count / dt
                self._fps_count = 0
                self._fps_t0 = now

    def on_lidar(self, l) -> None:
        with self.lock:
            self.lidar_values = list(l.ranges[:N_LIDAR])
            self.lidar_frames += 1
            self.lidar_time = time.monotonic()

    def on_lidardiag(self, d) -> None:
        first = False
        with self.lock:
            first = self.lidar_diag is None
            self.lidar_diag = {
                "responded_alone": list(d.responded_alone),
                "alone_meaningful": d.alone_meaningful,
                "zombie": d.zombie,
                "ack_addrs": [f"0x{a:02X}" for a in d.ack_addrs],
                "fail_stage": (
                    [codec_lidar.FAIL_STAGE_NAMES.get(x, f"stage {x}")
                     for x in d.fail_stage]
                    if d.fail_stage is not None else None
                ),
            }
        if first:
            print(f"LIDARDIAG: alone={list(d.responded_alone)} "
                  f"zombie={d.zombie} "
                  f"ack={[hex(a) for a in d.ack_addrs]}", flush=True)

    def on_frame_error(self, exc, frame) -> None:
        with self.lock:
            self.frame_errors += 1

    # -- snapshot for the UI --------------------------------------------------

    def snapshot(self) -> dict:
        with self.lock:
            chans = []
            for i in range(N_ANALOG):
                s = list(self.spark[i])
                tail = s[-JITTER_WINDOW:]
                jitter = max(
                    (abs(tail[j] - tail[j - 1]) for j in range(1, len(tail))),
                    default=0,
                )
                chans.append({
                    "name": CHANNEL_NAMES[i],
                    "gpio": CHANNEL_GPIOS[i],
                    "raw": self.values[i],
                    "jitter": jitter,
                    "spark": s,
                    "touched": self.touch[i]["touched"],
                    "position": self.touch[i]["position"],
                })
            fsrs = []
            gamma, floor = self.fsr_gamma, self.fsr_floor

            def fsr_curve(x):
                # normalize (raw - floor) to 0..1, then apply the power curve
                # and rescale back to the full 0..4095 range.
                n = max(0.0, min(1.0, (x - floor) / (4095 - floor)))
                return 4095.0 * (n ** gamma)

            for i in range(N_FSR):
                raw = self.fsr_values[i]
                raw_spark = list(self.fsr_spark[i])
                # software-only sensitivity fix: subtract the noise floor,
                # normalize, apply the power curve (gamma), rescale to the
                # full 0..4095 range. Applied to the bar/sparkline/threshold;
                # the raw ADC value is still reported separately (see "raw"
                # below) so the fix is visible.
                v = fsr_curve(raw)
                s = [fsr_curve(x) for x in raw_spark]
                fsrs.append({
                    "name": FSR_NAMES[i],
                    "gpio": FSR_GPIOS[i],
                    "raw": raw,
                    "value": round(v, 1),
                    "spark": s,
                    "pressed": v >= FSR_THRESHOLD,
                })
            # lidar lanes: absent when never seen OR stream went stale
            # (covers the original grid_link firmware, which sends no 0x13)
            stale = (self.lidar_time is None
                     or time.monotonic() - self.lidar_time > LIDAR_STALE_S)
            lidars = []
            for i in range(N_LIDAR):
                if stale or self.lidar_values is None:
                    mm, state = None, "absent"
                elif self.lidar_values[i] == codec_lidar.LIDAR_INVALID:
                    mm, state = None, "dead"
                else:
                    mm, state = self.lidar_values[i], "ok"
                lidars.append({
                    "name": LIDAR_NAMES[i],
                    "addr": LIDAR_ADDRS[i],
                    "xshut": LIDAR_XSHUT[i],
                    "mm": mm,
                    "state": state,
                })
            return {
                "hello": self.hello,
                "hello_age": (round(time.monotonic() - self.hello_mono, 1)
                              if self.hello_mono else None),
                "link": {
                    "state": self.link_state,
                    "port": self.link_port,
                    "reconnects": self.reconnects,
                },
                "grid": [[1 if c else 0 for c in row] for row in self.pressed],
                "key_events": self.key_events,
                "last_key": self.last_key,
                "channels": chans,
                "fsrs": fsrs,
                "fsr_gamma": round(gamma, 6),
                "fsr_floor": floor,
                "lidars": lidars,
                "lidar_diag": self.lidar_diag,
                "lidar_frames": self.lidar_frames,
                "fps": round(self.fps, 1),
                "analog_frames": self.analog_frames,
                "frame_errors": self.frame_errors,
                "lavalamp": self.lavalamp,
                "link_rtt_ms": latency_probe.stats(list(self.link_rtt_ms)),
                "led_ack_full_ms": latency_probe.stats(list(self.led_ack_full_ms)),
                "led_ack_partial_ms": latency_probe.stats(list(self.led_ack_partial_ms)),
                "timing": self.timing,
                "latency_test_running": self.latency_test_running,
                "latency_test": self.latency_test_result,
            }


STATE = MonitorState()
SUP: "SerialSupervisor | None" = None  # set in main(); used by /latency_test


def _log(msg: str) -> None:
    print(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] {msg}", flush=True)


class SerialSupervisor:
    """Owns the serial link forever. Never raises out of its thread, so the
    HTTP server can never be killed by serial trouble (and vice versa: the
    supervisor thread is independent of HTTP handler threads).

    - Port absent at startup (or any time): retry the glob every RETRY_S.
    - Deafness watchdog: fw=2 answers every keepalive ping with a HELLO; if
      none arrives for HELLO_TIMEOUT_S, or the reader thread dies, or a
      write raises (replugged/re-enumerated USB leaves a stale fd), the link
      is closed, the port re-globbed, reopened, and re-pinged.
    """

    def __init__(self, forced_port: str | None = None) -> None:
        self.forced_port = forced_port
        self._link: LidarGridLink | None = None

    def start(self) -> None:
        threading.Thread(target=self._run, name="serial-supervisor",
                         daemon=True).start()

    # -- internals -----------------------------------------------------------

    def _set(self, state: str, port: str | None = None) -> None:
        with STATE.lock:
            STATE.link_state = state
            STATE.link_port = port

    def _close(self) -> None:
        link, self._link = self._link, None
        if link is not None:
            try:
                link.stop()
            except Exception:
                pass

    @staticmethod
    def _hard_reset_to_run(link: LidarGridLink) -> None:
        """Force the ESP32 into a clean app boot after opening the port.

        DTR/RTS glitches from opens/closes (esp. abrupt process death) can
        strand a standard auto-program dev board in the silent ROM download
        mode. Toggle EN (RTS) low->high with IO0 (DTR) released so it always
        reboots into the application.
        """
        try:
            s = link._serial
            s.dtr = False          # IO0 released (high) -> run mode
            s.rts = True           # EN low: hold in reset
            time.sleep(0.1)
            s.rts = False          # EN high: boot the app
            time.sleep(0.2)
            s.reset_input_buffer()  # drop boot-loader/boot-log garbage
        except Exception:
            pass

    def _run(self) -> None:
        first = True
        while True:
            try:
                self._connect_and_watch(first)
            except Exception as exc:  # belt and braces: never die
                _log(f"serial supervisor error: {exc!r}")
            first = False
            self._close()
            time.sleep(RETRY_S)

    def _connect_and_watch(self, first: bool) -> None:
        # 1) wait for a port to exist
        port = self.forced_port or autodetect_port()
        if port is None:
            self._set("no-device")
            if first:
                _log("no serial port found — waiting for device "
                     f"(retrying every {RETRY_S:.0f}s)")
            return  # outer loop sleeps and retries

        # 2) open it (a stale by-id symlink can still fail here)
        self._set("connecting" if first else "reconnecting", port)
        try:
            link = LidarGridLink(
                port,
                on_hello=STATE.on_hello,
                on_key=STATE.on_key,
                on_analog=STATE.on_analog,
                on_lidar=STATE.on_lidar,
                on_lidardiag=STATE.on_lidardiag,
                on_ack=STATE.on_ack,
                on_timing=STATE.on_timing,
                on_frame_error=STATE.on_frame_error,
            )
        except Exception as exc:
            _log(f"open {port} failed: {exc!r} — retrying in {RETRY_S:.0f}s")
            return
        self._link = link
        self._hard_reset_to_run(link)
        link.start()
        with STATE.lock:
            if not first:
                STATE.reconnects += 1
            STATE.hello_mono = None  # demand a fresh HELLO on this connection
        _log(f"serial {'connected' if first else 'reconnected'}: {port}")

        # 3) ping + watch (the ESP32 streams nothing until it hears a PING)
        opened = time.monotonic()
        last_ping = 0.0
        while True:
            now = time.monotonic()
            if now - last_ping >= KEEPALIVE_S or last_ping == 0.0:
                try:
                    with STATE.lock:
                        STATE.last_ping_mono = time.monotonic()
                    link.send_ping()
                except Exception as exc:
                    _log(f"serial write failed ({exc!r}) — reconnecting")
                    return
                last_ping = now
            # reader thread exits silently on read errors (unplug): detect it
            t = link._reader_thread
            if t is not None and not t.is_alive():
                _log("serial reader thread died — reconnecting")
                return
            with STATE.lock:
                hello_mono = STATE.hello_mono
                STATE.link_state = ("connected" if hello_mono is not None
                                    else STATE.link_state)
            if hello_mono is not None:
                ref, limit = hello_mono, HELLO_TIMEOUT_S
            else:  # fresh connection: allow for the ESP32's post-reset boot
                ref, limit = opened, FIRST_HELLO_GRACE_S
            if now - ref > limit:
                _log(f"no HELLO for {now - ref:.1f}s despite pings — "
                     "reconnecting")
                return
            time.sleep(1.0)


# ---------------------------------------------------------------------------
# Lavalamp — ambient LED renderer
# ---------------------------------------------------------------------------

class Lavalamp:
    """Ambient purple lava-lamp field on the 8x8 NeoTrellis LEDs.

    Runs in its own daemon thread at LAVA_FPS. Per-pad intensity drifts
    smoothly between LAVA_MIN and LAVA_MAX of full brightness via a sum of
    slow drifting sine fields (periods ~15-30s, phases warped by nested
    sines so nothing repeats visibly or looks symmetric). Hue stays in the
    violet band (LAVA_HUE +/- LAVA_HUE_VAR).

    A pressed pad snaps directly to LAVA_PRESS (slightly desaturated = a
    touch whiter), holds while held, and snaps straight back into the
    field on release — no envelope, no fade.

    Brightness is baked into the RGB values sent (send_leds_full); the
    global brightness register is set once per connection to
    LAVA_GLOBAL_BRIGHTNESS so 50% pads have headroom.

    Never raises out of its thread; skips frames while the link is down
    (SerialSupervisor owns reconnection) and resumes when it returns.
    LED index ordering per codec.leds_full: i -> x = i % 8, y = i // 8.
    """

    def __init__(self, supervisor: SerialSupervisor) -> None:
        self._sup = supervisor
        self._bright_link = None   # link object we last sent brightness on
        self._was_on = True
        self._last_frame = None    # list of 64 (r,g,b) last actually SENT;
                                   # None forces a full frame (new/changed
                                   # link, or resume from off)

    def start(self) -> None:
        threading.Thread(target=self._run, name="lavalamp",
                         daemon=True).start()

    # -- internals -----------------------------------------------------------

    @staticmethod
    def _field(x: int, y: int, t: float) -> float:
        """Smooth organic value in -1..1 from 3 drifting sine fields."""
        v = (math.sin(0.90 * x + 0.33 * t + 1.7 * math.sin(0.131 * t))
             + math.sin(1.10 * y - 0.27 * t + 2.3 * math.sin(0.093 * t))
             + math.sin(0.65 * (x + y) + 0.21 * t + 0.9 * math.sin(0.057 * t)))
        return v / 3.0

    def _frame(self, t: float) -> list:
        with STATE.lock:
            pressed = [row[:] for row in STATE.pressed]
        leds = []
        for y in range(GRID_H):
            for x in range(GRID_W):
                on = pressed[y][x]
                if on:  # direct on/off: full press level, a touch whiter
                    level, sat = LAVA_PRESS, 0.65
                else:
                    level = LAVA_MIN + (LAVA_MAX - LAVA_MIN) * (
                        0.5 + 0.5 * self._field(x, y, t))
                    sat = 0.90
                hue = (LAVA_HUE + LAVA_HUE_VAR * math.sin(
                    0.55 * x - 0.40 * y + 0.12 * t)) % 1.0
                r, g, b = colorsys.hsv_to_rgb(hue, sat, level)
                leds.append((int(r * 255), int(g * 255), int(b * 255)))
        return leds

    def _run(self) -> None:
        _log(f"lavalamp renderer started: {LAVA_FPS:.0f}fps send_leds_full, "
             f"purple field {LAVA_MIN:.0%}-{LAVA_MAX:.0%}, press {LAVA_PRESS:.0%} "
             f"direct on/off, global brightness "
             f"{LAVA_GLOBAL_BRIGHTNESS} once per connection")
        interval = 1.0 / LAVA_FPS
        while True:
            time.sleep(interval)
            now = time.monotonic()
            try:
                with STATE.lock:
                    connected = STATE.link_state == "connected"
                    on = STATE.lavalamp
                link = self._sup._link
                if not connected or link is None:
                    self._bright_link = None
                    self._last_frame = None
                    continue
                if not on:
                    if self._was_on:      # just toggled off: blank the grid
                        self._was_on = False
                        link.send_clear()
                        self._last_frame = None
                    continue
                self._was_on = True
                if link is not self._bright_link:
                    link.send_brightness(LAVA_GLOBAL_BRIGHTNESS)
                    self._bright_link = link
                    self._last_frame = None  # new link: shadow is stale
                frame = self._frame(now)
                # fw5 latency work: send only the pads that actually changed
                # (typically 1, on press/release) via LEDS_PARTIAL instead of
                # always re-sending all 64 pixels. The ambient field itself
                # drifts continuously so most idle frames still touch most
                # pixels and fall back to LEDS_FULL — this mainly matters for
                # the key->LED path, which is exactly what task 3(e) asks for.
                if self._last_frame is None:
                    link.send_leds_full(frame)
                else:
                    changed = [
                        (x, y, *frame[y * GRID_W + x])
                        for y in range(GRID_H) for x in range(GRID_W)
                        if frame[y * GRID_W + x] != self._last_frame[y * GRID_W + x]
                    ]
                    if changed and len(changed) <= LAVA_PARTIAL_MAX:
                        link.send_leds_partial(changed)
                    elif changed:
                        link.send_leds_full(frame)
                    # else: nothing changed, skip the write entirely
                self._last_frame = frame
            except Exception:
                # link died mid-write or mid-toggle: supervisor will
                # reconnect; just skip frames until it does
                self._bright_link = None
                self._last_frame = None


# ---------------------------------------------------------------------------
# HTTP
# ---------------------------------------------------------------------------

class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):  # quiet access log
        pass

    def do_GET(self):
        if self.path in ("/", "/index.html") or self.path.startswith("/?") \
                or self.path.startswith("/index.html?"):
            # ?fsr_gamma=<float>&fsr_floor=<int> on the page URL itself is a
            # shortcut for GET /fsr?gamma=&floor= (e.g. bookmarking a tuned
            # link) — same validation, applied before the page renders.
            q = parse_qs(urlparse(self.path).query)
            if q:
                with STATE.lock:
                    if "fsr_gamma" in q:
                        try:
                            g = float(q["fsr_gamma"][0])
                            if 0.0 < g <= 10.0:
                                STATE.fsr_gamma = g
                        except (ValueError, IndexError):
                            pass
                    if "fsr_floor" in q:
                        try:
                            f = int(float(q["fsr_floor"][0]))
                            if 0 <= f <= 4095:
                                STATE.fsr_floor = f
                        except (ValueError, IndexError):
                            pass
            body = PAGE.encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        elif self.path.startswith("/lavalamp"):
            # /lavalamp?set=on|off|toggle  (bare /lavalamp just reports)
            q = self.path.split("set=", 1)[1] if "set=" in self.path else ""
            with STATE.lock:
                if q == "on":
                    STATE.lavalamp = True
                elif q == "off":
                    STATE.lavalamp = False
                elif q == "toggle":
                    STATE.lavalamp = not STATE.lavalamp
                on = STATE.lavalamp
            body = json.dumps({"lavalamp": on}).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        elif self.path.startswith("/fsr"):
            # GET /fsr?gamma=<float>&floor=<int> — live-tune the FSR software
            # sensitivity fix (no redeploy/restart needed). Bare /fsr just
            # reports the current values. Unrecognized/invalid params are
            # ignored rather than erroring, so a typo can't 400 the panel.
            # `gain` is accepted for backward compat with the old linear-gain
            # param but is mapped onto gamma (best-effort) rather than used
            # directly, since the two aren't equivalent.
            q = parse_qs(urlparse(self.path).query)
            with STATE.lock:
                if "gamma" in q:
                    try:
                        g = float(q["gamma"][0])
                        if 0.0 < g <= 10.0:
                            STATE.fsr_gamma = g
                    except (ValueError, IndexError):
                        pass
                elif "gain" in q:
                    try:
                        g = float(q["gain"][0])
                        if 0.0 < g <= 10.0:
                            STATE.fsr_gamma = g
                    except (ValueError, IndexError):
                        pass
                if "floor" in q:
                    try:
                        f = int(float(q["floor"][0]))
                        if 0 <= f <= 4095:
                            STATE.fsr_floor = f
                    except (ValueError, IndexError):
                        pass
                out = {"gamma": STATE.fsr_gamma, "floor": STATE.fsr_floor}
            body = json.dumps(out).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        elif self.path == "/latency_test":
            self.handle_latency_test()
        elif self.path == "/events":
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Cache-Control", "no-cache")
            self.send_header("Connection", "keep-alive")
            self.end_headers()
            interval = 1.0 / UI_FPS
            try:
                while True:
                    snap = STATE.snapshot()
                    payload = json.dumps(snap, separators=(",", ":"))
                    self.wfile.write(b"data: " + payload.encode("utf-8") + b"\n\n")
                    self.wfile.flush()
                    time.sleep(interval)
            except (BrokenPipeError, ConnectionResetError, OSError):
                pass
        else:
            self.send_response(404)
            self.send_header("Content-Length", "0")
            self.end_headers()

    def handle_latency_test(self) -> None:
        """GET /latency_test — runs latency_probe against the live link
        (borrowed from the supervisor, not a second serial connection) and
        returns the JSON report. Pauses the lavalamp for a clean run (its
        continuous frames would otherwise pollute the LED-ack samples) and
        always restores it afterward. Blocks this request's handler thread
        for a few seconds; other SSE/HTTP connections are unaffected
        (ThreadingHTTPServer)."""
        link = SUP._link if SUP is not None else None
        if link is None or STATE.link_state != "connected":
            body = json.dumps({"error": "link not connected"}).encode("utf-8")
            self.send_response(503)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        with STATE.lock:
            if STATE.latency_test_running:
                body = json.dumps({"error": "already running"}).encode("utf-8")
                self.send_response(409)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
                return
            STATE.latency_test_running = True
            prev_lava = STATE.lavalamp
            STATE.lavalamp = False
        time.sleep(0.15)  # let any in-flight lavalamp frame finish

        try:
            report = latency_probe.run(
                link, rtt_samples=15, led_samples=20, stream_s=1.5,
                log=lambda m: None,
            )
        except Exception as exc:
            report = {"error": repr(exc)}
        finally:
            with STATE.lock:
                STATE.lavalamp = prev_lava
                STATE.latency_test_running = False
                STATE.latency_test_result = report

        body = json.dumps(report).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


# ---------------------------------------------------------------------------
# Inline UI
# ---------------------------------------------------------------------------

PAGE = r"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>neuralGrid monitor</title>
<style>
  :root {
    --bg: #0c0e12;
    --panel: #141821;
    --panel2: #1a1f2b;
    --line: #262c3a;
    --text: #d7dce6;
    --dim: #7d8699;
    --accent: #4ec9b0;
    --raw: #e0b34c;
    --hot: #ff5c7a;
    --touch: #4e9cff;
  }
  * { box-sizing: border-box; }
  body {
    margin: 0; padding: 18px 22px;
    background: var(--bg); color: var(--text);
    font: 15px/1.4 ui-monospace, "SF Mono", Menlo, Consolas, monospace;
  }
  h1 { font-size: 18px; margin: 0 0 4px; letter-spacing: 2px; color: var(--accent); }
  #status { color: var(--dim); font-size: 14px; margin-bottom: 16px; min-height: 20px; }
  #status .ok { color: var(--accent); }
  #status .bad { color: var(--hot); }
  .layout { display: flex; gap: 26px; flex-wrap: wrap; align-items: flex-start; }

  /* --- 8x8 grid --- */
  .gridpanel { background: var(--panel); border: 1px solid var(--line);
               border-radius: 10px; padding: 16px; }
  .gridpanel h2, .lanes h2 { font-size: 13px; color: var(--dim); margin: 0 0 10px;
                             text-transform: uppercase; letter-spacing: 1.5px; }
  #grid { display: grid; grid-template-columns: repeat(8, 46px);
          grid-auto-rows: 46px; gap: 6px; }
  .cell { background: var(--panel2); border: 1px solid var(--line);
          border-radius: 6px; transition: background .08s, box-shadow .08s; }
  .cell.on { background: var(--accent); border-color: var(--accent);
             box-shadow: 0 0 14px 2px rgba(78,201,176,.55); }
  #lastkey { margin-top: 10px; color: var(--dim); font-size: 13px; min-height: 18px; }

  /* --- softpot lanes --- */
  .lanes { flex: 1 1 520px; min-width: 480px; }
  .lane { background: var(--panel); border: 1px solid var(--line);
          border-radius: 10px; padding: 12px 16px; margin-bottom: 12px; }
  .lane-head { display: flex; align-items: baseline; gap: 12px; margin-bottom: 8px; }
  .lane-name { font-size: 17px; font-weight: bold; }
  .lane-gpio { color: var(--dim); font-size: 12px; }
  .lane-raw { margin-left: auto; color: var(--raw); font-size: 17px;
              min-width: 60px; text-align: right; }
  .lane-jit { color: var(--dim); font-size: 12px; min-width: 110px; text-align: right; }
  .lane-jit b { color: var(--text); }
  .lane-jit.noisy b { color: var(--hot); }
  .rawbar { height: 14px; background: var(--panel2); border-radius: 4px;
            overflow: hidden; margin-bottom: 8px; }
  .rawbar > div { height: 100%; width: 0%; background: var(--raw);
                  opacity: .85; }
  .row2 { display: flex; gap: 14px; align-items: center; }
  canvas.spark { background: var(--panel2); border-radius: 4px;
                 width: 200px; height: 36px; flex: none; }
  .touchbox { flex: 1; display: flex; align-items: center; gap: 10px; }
  .badge { font-size: 12px; padding: 3px 10px; border-radius: 20px;
           border: 1px solid var(--line); color: var(--dim); flex: none;
           min-width: 88px; text-align: center; letter-spacing: 1px; }
  .badge.on { background: var(--touch); border-color: var(--touch);
              color: #fff; box-shadow: 0 0 10px rgba(78,156,255,.5); }
  .posbar { flex: 1; height: 10px; background: var(--panel2);
            border-radius: 5px; position: relative; }
  .posbar .dot { position: absolute; top: -4px; width: 18px; height: 18px;
                 border-radius: 50%; background: var(--touch);
                 margin-left: -9px; left: 0%; display: none;
                 box-shadow: 0 0 8px rgba(78,156,255,.7); }
  .posbar.on .dot { display: block; }
  .posval { color: var(--touch); font-size: 13px; min-width: 52px;
            text-align: right; }
  .legend { color: var(--dim); font-size: 12px; margin-top: 4px; }
  .legend .r { color: var(--raw); } .legend .t { color: var(--touch); }

  /* --- lidar lanes --- */
  .legend .l { color: var(--lidar); }
  :root { --lidar: #b48eff; }
  .lidar-lane { background: var(--panel); border: 1px solid var(--line);
                border-radius: 10px; padding: 10px 16px; margin-bottom: 10px;
                display: flex; align-items: center; gap: 12px; }
  .lidar-name { font-size: 17px; font-weight: bold; min-width: 20px; }
  .lidar-meta { color: var(--dim); font-size: 12px; min-width: 130px; }
  .lidar-bar { flex: 1; height: 14px; background: var(--panel2);
               border-radius: 4px; overflow: hidden; }
  .lidar-bar > div { height: 100%; width: 0%; background: var(--lidar);
                     opacity: .85; }
  .lidar-mm { color: var(--lidar); font-size: 17px; min-width: 92px;
              text-align: right; }
  .lidar-lane.dead .lidar-mm { color: var(--hot); font-size: 13px; }
  .lidar-lane.dead .lidar-bar > div { width: 0% !important; }
  .lidar-lane.absent .lidar-mm { color: var(--dim); font-size: 13px; }

  /* --- lidar boot diagnostic --- */
  .zombie { background: #3a1220; border: 2px solid var(--hot);
            border-radius: 10px; padding: 12px 16px; margin-bottom: 10px;
            color: #ffb3c2; font-size: 14px; font-weight: bold;
            box-shadow: 0 0 18px rgba(255,92,122,.35); }
  .diag-row { background: var(--panel); border: 1px solid var(--line);
              border-radius: 10px; padding: 8px 16px; margin-bottom: 8px;
              display: flex; align-items: center; gap: 12px; font-size: 14px; }
  .diag-name { font-weight: bold; min-width: 20px; }
  .diag-meta { color: var(--dim); font-size: 12px; min-width: 130px; }
  .diag-verdict { margin-left: auto; }
  .diag-yes { color: var(--accent); }
  .diag-no  { color: var(--hot); }
  .diag-unk { color: var(--dim); }

  /* --- latency panel --- */
  .lat-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px,1fr));
              gap: 10px; margin-bottom: 10px; }
  .lat-card { background: var(--panel); border: 1px solid var(--line);
              border-radius: 10px; padding: 10px 14px; }
  .lat-card .k { color: var(--dim); font-size: 11px; text-transform: uppercase;
                 letter-spacing: 1px; margin-bottom: 4px; }
  .lat-card .v { font-size: 20px; color: var(--accent); }
  .lat-card .v .unit { font-size: 12px; color: var(--dim); }
  .lat-card .sub { color: var(--dim); font-size: 11px; margin-top: 2px; }
  #latbtn { background: var(--panel2); color: var(--accent); border: 1px solid var(--line);
            border-radius: 6px; padding: 5px 14px; font: inherit; font-size: 13px;
            cursor: pointer; margin-bottom: 10px; }
  #latbtn:disabled { color: var(--dim); cursor: default; }
  .timing-row { display: flex; gap: 8px; flex-wrap: wrap; font-size: 12px;
                color: var(--dim); }
  .timing-row .t { background: var(--panel2); border-radius: 5px; padding: 3px 8px; }
  .timing-row .t b { color: var(--text); }
  .timing-row .t.hot b { color: var(--hot); }
  #latresult { font-size: 12px; color: var(--dim); margin-top: 8px; white-space: pre-wrap; }
</style>
</head>
<body>
<h1>neuralGrid MONITOR</h1>
<div id="status">connecting…</div>
<div id="controls" style="margin:-8px 0 14px">
  <button id="lavabtn" style="background:var(--panel2);color:var(--lidar);
    border:1px solid var(--line);border-radius:6px;padding:4px 12px;
    font:inherit;font-size:13px;cursor:pointer">lavalamp: …</button>
</div>

<div class="gridpanel" style="margin-bottom:16px">
  <h2>latency (fw5+)</h2>
  <div class="lat-grid" id="latcards"></div>
  <button id="latbtn">run latency test</button>
  <div id="latresult"></div>
  <div class="lat-grid" style="margin-top:2px">
    <div style="grid-column:1/-1">
      <div class="k" style="color:var(--dim);font-size:11px;text-transform:uppercase;
           letter-spacing:1px;margin-bottom:6px">firmware phase timing — max µs since last 500ms report</div>
      <div class="timing-row" id="timingrow">no TIMING frames yet (fw &lt; 5, or not booted since connect)</div>
    </div>
  </div>
</div>

<div class="layout">
  <div class="gridpanel">
    <h2>8×8 key grid</h2>
    <div id="grid"></div>
    <div id="lastkey"></div>
  </div>

  <div class="lanes">
    <h2>SoftPots — raw ADC (ungated) vs TouchDetector (gated)</h2>
    <div id="lanes"></div>
    <div class="legend">
      <span class="r">■ raw 0..4095</span> — no pulldowns fitted: untouched
      channels read full-range noise. &nbsp;
      <span class="t">■ TOUCH</span> = stability-gated detector state
      (touched + position 0..1). jitter = max consecutive jump, last 10 samples.
    </div>

    <h2 style="margin-top:18px">FSRs — software-scaled (fw6+)</h2>
    <div id="fsrtune" class="legend" style="margin-bottom:8px">
      sensitivity fix: <code>n = clamp((raw - floor) / (4095 - floor), 0, 1); value = 4095 * n ** gamma</code>
      &nbsp; gamma <input id="fsrgamma" type="number" step="0.1" min="0.01" max="10" style="width:5em">
      &nbsp; floor <input id="fsrfloor" type="number" step="1" min="0" max="4095" style="width:5em">
      &nbsp; <button id="fsrapply">apply</button>
      &nbsp;<span id="fsrtunestatus"></span>
    </div>
    <div id="fsrs"></div>
    <div class="legend">
      <span class="r">■ scaled 0..4095</span> — bar/sparkline/threshold all
      use the scaled value above; the small number next to the bar is the
      raw ADC reading (pre-scale) so both are visible.
      unwired/unpressed channels should read ~0 (external 10k pulldown,
      same convention as the softpots). <span class="t">■ PRESSED</span> =
      simple threshold state (scaled &ge; 300), no gating/debounce logic.
      Softpot lanes above are unaffected by this.
    </div>

    <h2 style="margin-top:18px">VL53L1X lidars — range (mm)</h2>
    <div id="lidars"></div>
    <div class="legend">
      <span class="l">■ range</span> bar scaled 0..2000mm.
      DEAD = firmware reports 0xFFFF (sensor failed init / no valid
      measurement — retried automatically every 5s). absent = no LIDAR
      frames on the wire (old firmware, or stream stopped).
    </div>

    <h2 style="margin-top:18px">lidar boot diagnostic — XSHUT / bus health</h2>
    <div id="zombiebanner" class="zombie" hidden>
      ZOMBIE SENSOR: a VL53L1X answered at 0x29 while every XSHUT was held
      LOW — one sensor is stuck awake because its XSHUT wire is not
      controlling it (dangling wire / cold joint). This is what shuffles
      which positions work. Fix that XSHUT connection first; per-channel
      results below are unreliable until it is gone.
    </div>
    <div id="diag"></div>
    <div id="diagacks" class="legend"></div>
  </div>
</div>

<script>
const W = 8, H = 8, NCH = 4;

// build grid (y=0 at top)
const grid = document.getElementById('grid');
const cells = [];
for (let y = 0; y < H; y++) {
  const row = [];
  for (let x = 0; x < W; x++) {
    const d = document.createElement('div');
    d.className = 'cell';
    grid.appendChild(d);
    row.push(d);
  }
  cells.push(row);
}

// build lanes
const lanesEl = document.getElementById('lanes');
const lanes = [];
for (let i = 0; i < NCH; i++) {
  const el = document.createElement('div');
  el.className = 'lane';
  el.innerHTML = `
    <div class="lane-head">
      <span class="lane-name"></span><span class="lane-gpio"></span>
      <span class="lane-jit">jitter <b>0</b></span>
      <span class="lane-raw">0</span>
    </div>
    <div class="rawbar"><div></div></div>
    <div class="row2">
      <canvas class="spark" width="200" height="36"></canvas>
      <div class="touchbox">
        <span class="badge">no touch</span>
        <div class="posbar"><div class="dot"></div></div>
        <span class="posval">—</span>
      </div>
    </div>`;
  lanesEl.appendChild(el);
  lanes.push({
    name: el.querySelector('.lane-name'),
    gpio: el.querySelector('.lane-gpio'),
    raw: el.querySelector('.lane-raw'),
    jit: el.querySelector('.lane-jit'),
    jitb: el.querySelector('.lane-jit b'),
    bar: el.querySelector('.rawbar > div'),
    spark: el.querySelector('canvas.spark'),
    badge: el.querySelector('.badge'),
    posbar: el.querySelector('.posbar'),
    dot: el.querySelector('.dot'),
    posval: el.querySelector('.posval'),
  });
}

// build FSR lanes (fw6): raw bar + sparkline + simple threshold badge
const NFSR = 4;
const fsrsEl = document.getElementById('fsrs');
const fsrLanes = [];
for (let i = 0; i < NFSR; i++) {
  const el = document.createElement('div');
  el.className = 'lane';
  el.innerHTML = `
    <div class="lane-head">
      <span class="lane-name"></span><span class="lane-gpio"></span>
      <span class="badge" style="margin-left:auto">no press</span>
      <span class="lane-raw">0</span>
      <span class="lane-rawadc" style="opacity:0.5;font-size:0.85em">raw 0</span>
    </div>
    <div class="rawbar"><div></div></div>
    <canvas class="spark" width="200" height="36"></canvas>`;
  fsrsEl.appendChild(el);
  fsrLanes.push({
    name: el.querySelector('.lane-name'),
    gpio: el.querySelector('.lane-gpio'),
    raw: el.querySelector('.lane-raw'),
    rawadc: el.querySelector('.lane-rawadc'),
    bar: el.querySelector('.rawbar > div'),
    spark: el.querySelector('canvas.spark'),
    badge: el.querySelector('.badge'),
  });
}

// build lidar lanes
const LIDAR_BAR_MM = 2000;
const lidarsEl = document.getElementById('lidars');
const lidarLanes = [];
for (let i = 0; i < NCH; i++) {
  const el = document.createElement('div');
  el.className = 'lidar-lane';
  el.innerHTML = `
    <span class="lidar-name"></span>
    <span class="lidar-meta"></span>
    <div class="lidar-bar"><div></div></div>
    <span class="lidar-mm">absent</span>`;
  lidarsEl.appendChild(el);
  lidarLanes.push({
    el,
    name: el.querySelector('.lidar-name'),
    meta: el.querySelector('.lidar-meta'),
    bar: el.querySelector('.lidar-bar > div'),
    mm: el.querySelector('.lidar-mm'),
  });
}

function drawSpark(cv, data) {
  const ctx = cv.getContext('2d');
  const w = cv.width, h = cv.height;
  ctx.clearRect(0, 0, w, h);
  if (data.length < 2) return;
  ctx.strokeStyle = '#e0b34c';
  ctx.lineWidth = 1.2;
  ctx.beginPath();
  const n = data.length;
  for (let i = 0; i < n; i++) {
    const x = (i / (n - 1)) * (w - 2) + 1;
    const y = h - 2 - (data[i] / 4095) * (h - 4);
    if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
  }
  ctx.stroke();
}

const statusEl = document.getElementById('status');
const lastkeyEl = document.getElementById('lastkey');
const lavaBtn = document.getElementById('lavabtn');
lavaBtn.onclick = () => fetch('/lavalamp?set=toggle');

// FSR sensitivity tuning: GET /fsr?gamma=&floor= applies live, no restart.
// fsrTuneEditing suppresses the SSE feed from clobbering the inputs while
// the user is actively typing into them.
let fsrTuneEditing = false;
const fsrGammaEl = document.getElementById('fsrgamma');
const fsrFloorEl = document.getElementById('fsrfloor');
const fsrApplyBtn = document.getElementById('fsrapply');
const fsrTuneStatus = document.getElementById('fsrtunestatus');
for (const el of [fsrGammaEl, fsrFloorEl]) {
  el.addEventListener('focus', () => { fsrTuneEditing = true; });
  el.addEventListener('blur', () => { fsrTuneEditing = false; });
}
fsrApplyBtn.onclick = async () => {
  const g = parseFloat(fsrGammaEl.value);
  const f = parseInt(fsrFloorEl.value, 10);
  fsrTuneStatus.textContent = 'applying…';
  try {
    const r = await fetch(`/fsr?gamma=${encodeURIComponent(g)}&floor=${encodeURIComponent(f)}`);
    const j = await r.json();
    fsrTuneStatus.textContent = `applied: gamma=${j.gamma} floor=${j.floor}`;
  } catch (e) {
    fsrTuneStatus.textContent = 'apply failed: ' + e;
  }
};

const latCardsEl = document.getElementById('latcards');
const latBtn = document.getElementById('latbtn');
const latResultEl = document.getElementById('latresult');
const timingRowEl = document.getElementById('timingrow');
const TIMING_PHASES = ['serial','keys','analog','lidar_poll','lidar_report','led_apply','loop_total'];

latBtn.onclick = async () => {
  latBtn.disabled = true;
  latBtn.textContent = 'running…';
  try {
    const r = await fetch('/latency_test');
    const j = await r.json();
    if (j.error) {
      latResultEl.textContent = 'latency test failed: ' + j.error;
    } else {
      latResultEl.textContent = JSON.stringify(j, null, 2);
    }
  } catch (e) {
    latResultEl.textContent = 'latency test failed: ' + e;
  }
  latBtn.disabled = false;
  latBtn.textContent = 'run latency test';
};

function fmtStat(s, unit) {
  if (!s || s.n === 0) return `<div class="v">—</div><div class="sub">no samples yet</div>`;
  return `<div class="v">${s.p50}<span class="unit"> ${unit} p50</span></div>` +
         `<div class="sub">p95 ${s.p95} · max ${s.max} · n=${s.n}</div>`;
}

function renderLatency(s) {
  const cards = [
    ['link RTT (PING→HELLO)', fmtStat(s.link_rtt_ms, 'ms')],
    ['LED ack — LEDS_FULL', fmtStat(s.led_ack_full_ms, 'ms')],
    ['LED ack — LEDS_PARTIAL', fmtStat(s.led_ack_partial_ms, 'ms')],
  ];
  latCardsEl.innerHTML = cards.map(([k, v]) =>
    `<div class="lat-card"><div class="k">${k}</div>${v}</div>`).join('');

  if (s.timing) {
    timingRowEl.innerHTML = TIMING_PHASES.map(p => {
      const us = s.timing[p] ?? 0;
      const hot = (p !== 'loop_total' && us > 2000) || (p === 'loop_total' && us > 5000);
      return `<span class="t${hot ? ' hot' : ''}">${p} <b>${us}µs</b></span>`;
    }).join('');
  }

  if (!s.latency_test_running && s.latency_test !== null && latResultEl.textContent === '') {
    latResultEl.textContent = JSON.stringify(s.latency_test, null, 2);
  }
}

function render(s) {
  // status
  let st = '';
  const lk = s.link || {};
  if (lk.state === 'connected') {
    st += `<span class="ok">LINK connected</span>`;
  } else {
    st += `<span class="bad">LINK ${lk.state || 'unknown'}</span>`;
  }
  if (lk.reconnects) st += ` (reconnects ${lk.reconnects})`;
  st += ` &nbsp;|&nbsp; `;
  if (s.hello) {
    st += `<span class="ok">HELLO</span> proto=${s.hello.proto_ver} ` +
          `grid=${s.hello.grid_w}x${s.hello.grid_h} ` +
          `analog=${s.hello.n_analog} fw=${s.hello.fw_ver}`;
    if (s.hello_age != null) {
      const cls = s.hello_age > 12 ? 'bad' : 'ok';
      st += ` <span class="${cls}">(${s.hello_age.toFixed(1)}s ago)</span>`;
    }
    st += ` &nbsp;|&nbsp; `;
  } else {
    st += `<span class="bad">no HELLO yet</span> &nbsp;|&nbsp; `;
  }
  st += `analog ${s.fps} fps &nbsp;|&nbsp; frames ${s.analog_frames}` +
        ` &nbsp;|&nbsp; lidar frames ${s.lidar_frames ?? 0}` +
        ` &nbsp;|&nbsp; key events ${s.key_events}`;
  if (s.frame_errors) st += ` &nbsp;|&nbsp; <span class="bad">frame errors ${s.frame_errors}</span>`;
  statusEl.innerHTML = st;

  // lavalamp toggle
  if (typeof s.lavalamp === 'boolean') {
    lavaBtn.textContent = 'lavalamp: ' + (s.lavalamp ? 'ON' : 'off');
    lavaBtn.style.color = s.lavalamp ? 'var(--lidar)' : 'var(--dim)';
  }

  renderLatency(s);

  // grid
  for (let y = 0; y < H; y++)
    for (let x = 0; x < W; x++)
      cells[y][x].classList.toggle('on', !!(s.grid[y] && s.grid[y][x]));
  if (s.last_key)
    lastkeyEl.textContent =
      `last: (${s.last_key.x},${s.last_key.y}) ` +
      (s.last_key.pressed ? 'press' : 'release');

  // lanes
  for (let i = 0; i < NCH && i < s.channels.length; i++) {
    const c = s.channels[i], L = lanes[i];
    L.name.textContent = c.name;
    L.gpio.textContent = `GPIO${c.gpio}`;
    L.raw.textContent = c.raw;
    L.jitb.textContent = c.jitter;
    L.jit.classList.toggle('noisy', c.jitter > 200);
    L.bar.style.width = (c.raw / 4095 * 100).toFixed(1) + '%';
    drawSpark(L.spark, c.spark);
    L.badge.textContent = c.touched ? 'TOUCHED' : 'no touch';
    L.badge.classList.toggle('on', c.touched);
    L.posbar.classList.toggle('on', c.touched);
    if (c.touched) {
      L.dot.style.left = (c.position * 100).toFixed(1) + '%';
      L.posval.textContent = c.position.toFixed(3);
    } else {
      L.posval.textContent = '—';
    }
  }

  // FSR lanes (fw6+; empty/absent against older firmware — just stays at
  // its initial "0 / no press" state, harmless)
  const fsrs = s.fsrs || [];
  for (let i = 0; i < NFSR && i < fsrs.length; i++) {
    const c = fsrs[i], L = fsrLanes[i];
    L.name.textContent = c.name;
    L.gpio.textContent = `GPIO${c.gpio}`;
    L.raw.textContent = c.value;
    L.rawadc.textContent = `raw ${c.raw}`;
    L.bar.style.width = (Math.max(0, Math.min(4095, c.value)) / 4095 * 100).toFixed(1) + '%';
    drawSpark(L.spark, c.spark);
    L.badge.textContent = c.pressed ? 'PRESSED' : 'no press';
    L.badge.classList.toggle('on', c.pressed);
  }
  if (!fsrTuneEditing && s.fsr_gamma != null) fsrGammaEl.value = s.fsr_gamma;
  if (!fsrTuneEditing && s.fsr_floor != null) fsrFloorEl.value = s.fsr_floor;

  // lidar lanes
  const lids = s.lidars || [];
  for (let i = 0; i < NCH && i < lids.length; i++) {
    const c = lids[i], L = lidarLanes[i];
    L.name.textContent = c.name;
    L.meta.textContent = `${c.addr} · XSHUT GPIO${c.xshut}`;
    L.el.classList.toggle('dead', c.state === 'dead');
    L.el.classList.toggle('absent', c.state === 'absent');
    if (c.state === 'ok') {
      L.mm.textContent = `${c.mm} mm`;
      L.bar.style.width =
        Math.min(100, c.mm / LIDAR_BAR_MM * 100).toFixed(1) + '%';
    } else {
      L.mm.textContent = c.state === 'dead' ? 'DEAD (0xFFFF)' : 'absent';
      L.bar.style.width = '0%';
    }
  }

  // lidar boot diagnostic panel
  const d = s.lidar_diag;
  const zb = document.getElementById('zombiebanner');
  const diagEl = document.getElementById('diag');
  const acksEl = document.getElementById('diagacks');
  if (!d) {
    zb.hidden = true;
    diagEl.innerHTML =
      '<div class="diag-row"><span class="diag-unk">no diagnostic ' +
      'received (firmware &lt; 3, or not booted since connect)</span></div>';
    acksEl.textContent = '';
  } else {
    zb.hidden = !d.zombie;
    const names = ['N', 'S', 'E', 'W'];
    const xshut = [4, 13, 14, 27];
    let html = '';
    for (let i = 0; i < NCH; i++) {
      let v;
      if (!d.alone_meaningful) {
        v = '<span class="diag-unk">unknown — zombie masks this test</span>';
      } else if (d.responded_alone[i]) {
        v = '<span class="diag-yes">sensor responds alone: YES ' +
            '(power + bus + sensor OK)</span>';
      } else {
        v = '<span class="diag-no">sensor responds alone: NO ' +
            '(power / SDA-SCL / sensor fault)</span>';
      }
      let fs = '';
      if (d.fail_stage) {
        const t = d.fail_stage[i];
        fs = `<span class="${t === 'ok' ? 'diag-yes' : 'diag-no'}"` +
             ` style="font-size:12px">bring-up: ${t}</span>`;
      }
      html += `<div class="diag-row"><span class="diag-name">${names[i]}</span>` +
              `<span class="diag-meta">XSHUT GPIO${xshut[i]}</span>${fs}` +
              `<span class="diag-verdict">${v}</span></div>`;
    }
    diagEl.innerHTML = html;
    acksEl.textContent =
      'bus scan with all XSHUT low (expect only NeoTrellis 0x2E–0x31): ' +
      (d.ack_addrs.length ? d.ack_addrs.join(' ') : 'nothing ACKed');
  }
}

let es;
function connect() {
  es = new EventSource('/events');
  es.onmessage = (e) => render(JSON.parse(e.data));
  es.onerror = () => {
    statusEl.innerHTML = '<span class="bad">SSE disconnected — retrying…</span>';
  };
}
connect();
</script>
</body>
</html>
"""


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def main() -> int:
    ap = argparse.ArgumentParser(description="neuralGrid live input monitor")
    ap.add_argument("--port", default=None,
                    help="serial port (default: auto — /dev/serial/by-id "
                         "symlink first, then /dev/ttyUSB*)")
    ap.add_argument("--http-host", default="0.0.0.0")
    ap.add_argument("--http-port", type=int, default=8765)
    args = ap.parse_args()

    # Serial runs under a supervisor: retries when the port is absent,
    # reconnects on replug/re-enumeration or deafness. HTTP serves regardless.
    global SUP
    sup = SerialSupervisor(forced_port=args.port)
    SUP = sup
    sup.start()

    # Ambient LED renderer (own daemon thread; skips frames while link down)
    Lavalamp(sup).start()

    srv = ThreadingHTTPServer((args.http_host, args.http_port), Handler)
    srv.daemon_threads = True
    _log(f"monitor UI on http://{args.http_host}:{args.http_port}/")
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
