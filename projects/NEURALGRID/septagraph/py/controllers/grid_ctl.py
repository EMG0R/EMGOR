"""GridController — neuralGrid as a Septagraph controller (plan Task 3.3).

Consumes ``LidarGridLink`` (or any object exposing the same
``on_key``/``on_analog``/``on_lidar`` callback slots, real or fake) and
produces `/sept/v1/hit` and `/sept/v1/move` traffic via a
``septagraph.net.Sender``. The window (device space -> 8D coordinate) and
its precomputed cell table (catalog snapping) live here; renderers never
see the window.

Fast-path rule: on_key does a dict lookup + encode + sendto, no blocking
I/O and no model in the loop (Global Constraint, plan + HANDOFF §10).
"""
from __future__ import annotations

import colorsys
import time
from collections import deque
from typing import Callable, Optional

from septagraph.codec import encode_hit, encode_move
from septagraph.manifest import ManifestStore
from septagraph.net import Sender
from septagraph.types import Hit, Move, Vec, vec
from septagraph.window import Channel, Window, cell_table, modulate, project

from septagraph.pi.fsr import FsrChannel, FsrConfig, N_FSR
from septagraph.axes import K

# Artistic-default axis assignment (SEPTAGRAPH_PLAN §7-8 / HANDOFF §2):
# grid x -> freq(0), grid y -> scale(4); softpots: N=rate(3) E=loud(1)
# S=dist(7) W=harm(2); lidar (min over 4) -> spread(6).
GRID_CHANNEL = Channel(name="grid", dims=2, kind="discrete", size=(8, 8))
SOFTPOT_AXIS = {"N": 3, "E": 1, "S": 7, "W": 2}
LIDAR_AXIS = 6
LOUD_AXIS = 1  # freq=0, loud=1, harm=2, rate=3, scale=4, az=5, spread=6, dist=7
FSR_MOVE_MAX_HZ = 50.0  # per-voice /move rate cap while an FSR is held

# Softpot signal path: raw ADC 0..4095. Below this, strip is untouched.
SOFTPOT_TOUCH_THRESHOLD = 120
SOFTPOT_MID = 2048.0
SOFTPOT_MEDIAN_WINDOW = 5
SOFTPOT_EMA_ALPHA = 0.3
MOVE_MAX_HZ = 50.0  # per-voice /move rate cap (softpot modulation)

LIDAR_MIN_MM = 100
LIDAR_MAX_MM = 800
LIDAR_MOVE_MAX_HZ = 20.0

WARM_WHITE = (255, 190, 110)  # LED language: physical catalog point
DEFAULT_SIGMA = vec(*([0.05] * K))  # unsnapped/continuum cell: modest honest uncertainty

CHANNEL_NAMES = ("N", "S", "E", "W")


def _median(values) -> float:
    s = sorted(values)
    n = len(s)
    mid = n // 2
    if n % 2:
        return s[mid]
    return (s[mid - 1] + s[mid]) / 2.0


class _SoftpotChannel:
    """Threshold + median(5) + EMA(0.3) -> bipolar delta in [-1, 1]."""

    def __init__(self):
        self._window = deque(maxlen=SOFTPOT_MEDIAN_WINDOW)
        self._ema: Optional[float] = None

    def update(self, raw: int) -> Optional[float]:
        if raw < SOFTPOT_TOUCH_THRESHOLD:
            self._window.clear()
            self._ema = None
            return None
        self._window.append(raw)
        med = _median(self._window)
        self._ema = med if self._ema is None else (
            SOFTPOT_EMA_ALPHA * med + (1.0 - SOFTPOT_EMA_ALPHA) * self._ema
        )
        delta = (self._ema - SOFTPOT_MID) / SOFTPOT_MID
        return max(-1.0, min(1.0, delta))


class GridController:
    def __init__(
        self,
        link,
        sender: Sender,
        window: Window,
        manifests: ManifestStore,
        leds: Callable[[bytes], None],
        snap_radius: float = 0.06,
    ):
        self.link = link
        self.sender = sender
        self.window = window
        self.manifests = manifests
        self.leds = leds
        self.snap_radius = snap_radius

        self._cell_table: dict = {}
        self._softpots = {name: _SoftpotChannel() for name in CHANNEL_NAMES}
        self._held: dict[tuple[int, int], Vec] = {}  # (x,y) -> current mu, while pressed
        self._last_move_time: dict[tuple[int, int], float] = {}
        self._lidar_last_move = 0.0

        # FSR strikes (HANDOFF §11 / this task): FSR n -> /hit at the window
        # center, voice_id "grid:fsrN"; held pressure -> /move on the loud
        # axis (1) for that same voice. See septagraph.pi.fsr for the shared
        # gamma-curve + onset/release edge detector.
        self.fsr_config = FsrConfig()
        self._fsr_channels = [FsrChannel() for _ in range(N_FSR)]
        self._fsr_held: dict[int, bool] = {}
        self._fsr_last_move: dict[int, float] = {}

        self.rebuild()

        if link is not None:
            link.on_key = self.on_key
            link.on_analog = self.on_analog
            link.on_lidar = self.on_lidar

    # -- window / manifest -------------------------------------------------

    def rebuild(self) -> None:
        catalog = self.manifests.catalog() if self.manifests is not None else []
        self._cell_table = cell_table(self.window, GRID_CHANNEL, catalog, self.snap_radius)
        self._render_leds()

    def on_manifest(self, m) -> None:
        self.manifests.update(m)
        self.rebuild()

    def set_targets(self, targets: list[tuple[str, int]]) -> None:
        """Swap the renderer targets this controller's /hit /move traffic goes
        to (septagraph preset switch: LAB_DIRECT/CONTINUUM/HYBRID each route
        to a different set). Rebuilds the ``Sender`` -- cheap (one UDP socket
        + no state), never touches ``septagraph/net.py``."""
        self.sender = Sender(targets=list(targets))

    # -- LEDs (150ms crossfade handled by leds.py at the app layer) ------

    def _render_leds(self) -> None:
        frame = bytearray(8 * 8 * 3)
        for (x, y), (mu, point) in self._cell_table.items():
            i = y * 8 + x
            if point is not None:
                r, g, b = WARM_WHITE
            else:
                # continuum cell: hue by y (blue-green family per LED language)
                hue = 0.42 + 0.15 * (y / 7.0)
                r, g, b = (int(c * 255) for c in colorsys.hsv_to_rgb(hue, 0.65, 0.55))
            frame[i * 3: i * 3 + 3] = bytes((r, g, b))
        if self.leds:
            self.leds(bytes(frame))

    # -- key events (fast path) ------------------------------------------

    def on_key(self, x: int, y: int, edge: int) -> None:
        voice_id = f"grid:{x}{y}"
        if edge:
            mu, point = self._cell_table.get((x, y), (None, None))
            if mu is None:
                mu = project(self.window, GRID_CHANNEL, ((x + 0.5) / 8.0, (y + 0.5) / 8.0), velocity=0.7)
            sigma = point.sigma if point is not None else DEFAULT_SIGMA
            hit = Hit(voice_id=voice_id, layer=0, onset=True, velocity=0.7, mu=mu, sigma=sigma)
            self._held[(x, y)] = mu
            self.sender.send(encode_hit(hit))
        else:
            self._held.pop((x, y), None)
            self._last_move_time.pop((x, y), None)
            hit = Hit(voice_id=voice_id, layer=0, onset=False, velocity=0.0, mu=vec(*([0.0] * K)), sigma=vec(*([0.0] * K)))
            self.sender.send(encode_hit(hit))

    # -- softpots (modulators, voice-bound /move) ------------------------

    def on_analog(self, raw) -> None:
        now = time.monotonic()
        for name, raw_val in zip(CHANNEL_NAMES, raw[:4]):
            delta = self._softpots[name].update(raw_val)
            if delta is None or not self._held:
                continue
            axis = SOFTPOT_AXIS[name]
            for key, mu in list(self._held.items()):
                last = self._last_move_time.get(key, 0.0)
                if (now - last) < (1.0 / MOVE_MAX_HZ):
                    continue
                new_mu = modulate(mu, axis, delta, self.window)
                self._held[key] = new_mu
                self._last_move_time[key] = now
                voice_id = f"grid:{key[0]}{key[1]}"
                self.sender.send(encode_move(Move(voice_id=voice_id, mu=new_mu)))

        # FSR strikes (channels 4..7, HANDOFF §11). `raw` may still be a
        # 4-tuple against older firmware/fake-grid callers -- skip silently.
        for i, raw_val in enumerate(raw[4:4 + N_FSR]):
            self._on_fsr_sample(i, raw_val, now)

    def _on_fsr_sample(self, i: int, raw_val: int, now: float) -> None:
        # FSRs NEVER emit /hit (user rule, locked 2026-09-09 -- housing-mounted
        # FSRs flex with pad presses and fired phantom strikes). Pressure only
        # modulates `loud` of an already-held FSR voice; nothing holds one in
        # v1, so this is effectively meters-only until a real pressure design.
        event, pressure = self._fsr_channels[i].update(raw_val, self.fsr_config)
        voice_id = f"grid:fsr{i}"
        if event == "release":
            self._fsr_held.pop(i, None)
            self._fsr_last_move.pop(i, None)
        elif self._fsr_held.get(i):
            last = self._fsr_last_move.get(i, 0.0)
            if (now - last) < (1.0 / FSR_MOVE_MAX_HZ):
                return
            self._fsr_last_move[i] = now
            mu = list(self.window.center)
            mu[LOUD_AXIS] = max(0.0, min(1.0, pressure))
            self.sender.send(encode_move(Move(voice_id=voice_id, mu=vec(*mu))))

    # -- lidar (global modulator, unheld /move) --------------------------

    def on_lidar(self, mm4) -> None:
        now = time.monotonic()
        if (now - self._lidar_last_move) < (1.0 / LIDAR_MOVE_MAX_HZ):
            return
        valid = [m for m in mm4 if 0 <= m < 0xFFFF]
        if not valid:
            return
        nearest = min(valid)
        clamped = max(LIDAR_MIN_MM, min(LIDAR_MAX_MM, nearest))
        norm = (clamped - LIDAR_MIN_MM) / float(LIDAR_MAX_MM - LIDAR_MIN_MM)
        spread_val = 1.0 - norm
        mu = list(self.window.center)
        mu[LIDAR_AXIS] = max(0.0, min(1.0, spread_val))
        self._lidar_last_move = now
        self.sender.send(encode_move(Move(voice_id="", mu=vec(*mu))))
