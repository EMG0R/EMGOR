"""sources/midi.py — ALSA-seq CC in. Absorbs what `demiurge-monitor` taps.

Two jobs, same single tap (docs/parameters.md §3.3):

1. SOURCE: every control_change becomes a Reading `/hw/midi/<ch>/<cc>` with
   raw = value/127 (already 0..1, so no sensors.json calibration entry is
   needed — daemon falls back to identity calibration for "/hw/" ids). Clock
   CCs (ch16 CC104-119) are NOT emitted: they are coordination, not values,
   and would flood the queue at clock rate.

2. SNAPSHOT: the SAME one-line `label:value label:value ...` file
   demiurge-monitor writes (default /tmp/neptr_companion_telemetry), same
   atomic tmp+os.replace, same label table (monitor.conf [map]; monitor's
   built-in Teensy table if absent), cadence from monitor.conf `interval`.
   Values are keyed by CC number only, like the monitor (channel ignored), and
   joystick axes rest at 64 via `center64`, so the companion reads it
   byte-compatibly. OFF unless snapshot_path is given: until monitor.service is
   retired (step C) two writers on one file would fight. Never run both.

mido is imported lazily so the module (and its pure helpers) import anywhere.
"""

from __future__ import annotations

import os
import sys
import time

from ..model import Reading

CLOCK_CH = 16
CLOCK_CCS = range(104, 120)

DEFAULT_SNAPSHOT = "/tmp/neptr_companion_telemetry"
DEFAULT_INTERVAL = 0.4
DEFAULT_CENTER = {27: 64, 28: 64}
DEFAULT_MAP = [
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
]


def neptr_pedal_normalize(ch: int, cc: int):
    """Python twin of midi_handler.orc's two pedal rewrites, in the SAME order.

    1. The body's own expression pedal (ch1 CC29 on the wire) is PARKED on CC30
       (demoted so it cannot fight the treadle for expr_cc29's soft-takeover).
    2. The DUODECIMUS treadle (ch7 CC14 Jack A / CC15 Jack B) is funnelled into
       ch1 CC29 AFTER step 1, so its output is never demoted.
    Result: `/hw/midi/1/29` is the LOGICAL expression pedal (= the DUODECIMUS
    treadle) and nothing else. tests/test_neptr.py proves this against the .orc.
    """
    if ch == 1 and cc == 29:
        return 1, 30
    if ch == 7 and cc in (14, 15):
        return 1, 29
    return ch, cc


def cc_to_reading(ch: int, cc: int, value: int, t: float):
    """MIDI CC (ch 1-16) -> Reading, or None for clock CCs."""
    if ch == CLOCK_CH and cc in CLOCK_CCS:
        return None
    return Reading(f"/hw/midi/{ch}/{cc}", max(0, min(127, int(value))) / 127.0, t)


def load_monitor_conf(path):
    """Parse monitor.conf like demiurge-monitor does -> (cmap, center, interval).
    Missing/unreadable file -> the monitor's built-in defaults."""
    cmap, center, interval, in_map = [], dict(DEFAULT_CENTER), DEFAULT_INTERVAL, False
    try:
        with open(path) as fh:
            lines = fh.read().splitlines()
    except (OSError, TypeError):
        return list(DEFAULT_MAP), center, interval
    for raw in lines:
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        if line.lower() == "[map]":
            in_map = True
            continue
        if in_map:
            if ":" in line:
                lbl, cc = line.split(":", 1)
                try:
                    cmap.append((lbl.strip(), int(cc.strip())))
                except ValueError:
                    pass
        elif "=" in line:
            k, v = line.split("=", 1)
            k, v = k.strip().lower(), v.strip()
            if k == "interval":
                try:
                    interval = float(v)
                except ValueError:
                    pass
            elif k == "center64":
                center = {}
                for tok in v.replace(",", " ").split():
                    try:
                        center[int(tok)] = 64
                    except ValueError:
                        pass
    return (cmap or list(DEFAULT_MAP)), center, interval


class Snapshot:
    """Latest value per CC -> the monitor's one-line format. Pure + one atomic write."""

    def __init__(self, cmap, center, path, interval):
        self.cmap, self.path, self.interval = cmap, path, interval
        self.values = dict(center)
        self._last = 0.0

    def update(self, cc: int, value: int) -> None:
        self.values[cc] = int(value)

    def render(self) -> str:
        return " ".join(f"{lbl}:{self.values.get(cc, '-')}" for lbl, cc in self.cmap)

    def maybe_publish(self, now: float) -> bool:
        if now - self._last < self.interval:
            return False
        self._last = now
        try:
            tmp = self.path + ".tmp"
            with open(tmp, "w") as fh:
                fh.write(self.render() + "\n")
            os.replace(tmp, self.path)      # readers never see a half-write
        except OSError as e:
            print(f"[demiurge-io] midi snapshot publish error: {e}",
                  file=sys.stderr, flush=True)
        return True


def pick_port(names, want="auto"):
    """Same preference order as demiurge-monitor.pick_port."""
    if not names:
        return None
    if want and want.lower() != "auto":
        for n in names:
            if want.lower() in n.lower():
                return n
        return want
    for kw in ("Teensy", "NEPTR Serial Bridge", "Bridge", "Through"):
        for n in names:
            if kw.lower() in n.lower():
                return n
    return names[0]


class MidiSource:
    id = "midi"
    descriptors: dict = {}

    def __init__(self, port: str = "auto", snapshot: Snapshot | None = None,
                 normalize=None, allow=None):
        self.port = port
        self.snapshot = snapshot
        # normalize(ch, cc) -> (ch, cc): applied first (NEPTR: the pedal funnel).
        # allow: set of (ch, cc) AFTER normalizing; None = everything (the
        # original behaviour). Used on the rig so only the pedal becomes a
        # pool source instead of every Teensy CC flooding the queue.
        self.normalize = normalize
        self.allow = set(allow) if allow else None

    def accepts(self, ch: int, cc: int):
        """-> (ch, cc) to publish, or None if filtered out."""
        if self.normalize:
            ch, cc = self.normalize(ch, cc)
        if self.allow is not None and (ch, cc) not in self.allow:
            return None
        return ch, cc

    def calibration(self, source_id: str):
        return None

    def run(self, on_reading, on_control=None) -> None:
        del on_control
        import mido
        while True:                          # reconnect loop (survives unplug)
            name = pick_port(mido.get_input_names(), self.port)
            if not name:
                time.sleep(2)
                continue
            print(f"[demiurge-io] midi source reading: {name}", file=sys.stderr, flush=True)
            try:
                with mido.open_input(name) as inport:
                    while True:
                        now = time.monotonic()
                        for msg in inport.iter_pending():
                            if msg.type != "control_change":
                                continue
                            if self.snapshot:
                                self.snapshot.update(msg.control, msg.value)
                            pair = self.accepts(msg.channel + 1, msg.control)
                            if pair is None:
                                continue
                            r = cc_to_reading(pair[0], pair[1], msg.value, now)
                            if r:
                                on_reading(r)
                        if self.snapshot:
                            self.snapshot.maybe_publish(now)
                        time.sleep(0.005)
            except Exception as e:          # noqa: BLE001 — reopen on any error
                print(f"[demiurge-io] midi read error ({e}); reopening in 2s",
                      file=sys.stderr, flush=True)
                time.sleep(2)
