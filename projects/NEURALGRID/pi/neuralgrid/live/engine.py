"""LiveEngine: the Pi-side owner of the whole live layer.

Wires together GridLink (serial transport to the ESP32 dumb bridge),
TouchDetector (softpot touch/menu-select), MidiOut, Clock, and a set of
Menu instances (one per softpot channel), and drives the render loop.

Everything the engine talks to is dependency-injected (link/midi_out/clock
are passed in, not constructed here) specifically so a headless selftest
can hand it fakes -- see live/fakes.py and run_live.py --selftest. Menu
*classes* (not instances) are injected via `menu_classes`; the engine
instantiates each with itself as `engine`, since menus need to reach
self.engine.midi_out / self.engine.clock.
"""

from __future__ import annotations

import logging
import threading
import time
from typing import Dict, List, Optional, Tuple, Type

import numpy as np

from .. import codec
from .. import config as config_mod
from ..touch import TouchDetector
from ..models.features import FeatureWindow
from .menu import GRID_H, GRID_W, Menu, RGB

log = logging.getLogger("neuralgrid.live.engine")

N_LEDS = GRID_W * GRID_H
N_SOFTPOTS = 4

_CHANNEL_INDEX = {"R": 0, "G": 1, "B": 2}

MENU_SWITCH_CUE_SECONDS = 0.15


def _apply_color_order(fb: List[RGB], order: str) -> List[RGB]:
    """Reorder each logical (R,G,B) tuple in fb into the wire's channel
    order (e.g. "GRB" -> wire byte0=G, byte1=R, byte2=B). Framebuffer stays
    logical RGB everywhere else; this is the one place the swap happens."""
    order = order.upper()
    if order == "RGB":
        return list(fb)
    try:
        idxs = [_CHANNEL_INDEX[c] for c in order]
    except KeyError as exc:
        raise ValueError(f"invalid color order {order!r}, must be a permutation of RGB") from exc
    if len(idxs) != 3 or set(idxs) != {0, 1, 2}:
        raise ValueError(f"invalid color order {order!r}, must be a permutation of RGB")
    return [(px[idxs[0]], px[idxs[1]], px[idxs[2]]) for px in fb]


class LiveEngine:
    def __init__(
        self,
        link,
        midi_out,
        clock,
        menu_classes: Dict[int, Type[Menu]],
        brightness: int = 180,
        color_order: str = "GRB",
        fps_cap: float = 60.0,
        default_menu: int = 0,
    ):
        self.link = link
        self.midi_out = midi_out
        self.clock = clock
        self.color_order = color_order
        self.brightness = brightness
        self._min_frame_interval = 1.0 / fps_cap if fps_cap > 0 else 0.0

        self.menus: Dict[int, Menu] = {idx: cls(self) for idx, cls in menu_classes.items()}
        if default_menu not in self.menus:
            raise ValueError(f"default_menu {default_menu} has no registered menu class")
        self.active_menu_idx = default_menu

        self.touch = TouchDetector(n_channels=N_SOFTPOTS)
        self.framebuffer: List[RGB] = [(0, 0, 0)] * N_LEDS
        self._prev_sent_fb: Optional[List[RGB]] = None
        self._last_send_time = 0.0

        self.hello: Optional[codec.Hello] = None
        self._cue_until = 0.0
        self._cue_color: RGB = (0, 0, 0)

        # -- config boundary (Phase 3) ------------------------------------
        # ACTIVE config: what menus read *right now*. Starts as the
        # default -- a pure superset of Phase 2's hard-coded behavior.
        # STAGED slot: written by stage_config() (from any thread -- the
        # model layer, later, or a manual stager for testing right now);
        # swapped into `config` only on a clock downbeat, never mid-frame,
        # never blocking the hot loop (see _on_clock_downbeat).
        self.config: Dict = config_mod.default_config()
        self._staged_lock = threading.Lock()
        self._staged: Optional[Tuple[Dict, str]] = None
        self._downbeat_count = 0

        # -- interaction features (Phase 4) -------------------------------
        # Rolling KEY+ANALOG event log -> fixed-length feature vector, fed
        # here (cheap, O(1) deque append, never blocks) and read by
        # live/stager_model.ModelStager off the hot path. Bookkeeping
        # only -- the hand-coded menus never read this.
        self.interaction_features = FeatureWindow()

        self.link.on_hello = self._on_hello
        self.link.on_key = self._on_key
        self.link.on_analog = self._on_analog

        self.clock.on_step = self._on_clock_step
        self.clock.on_downbeat = self._on_clock_downbeat

    @property
    def active_menu(self) -> Menu:
        return self.menus[self.active_menu_idx]

    # -- lifecycle ---------------------------------------------------

    def start(self) -> None:
        self.link.start()
        self.clock.start()
        self.link.send_ping()
        self.active_menu.on_enter()

    def stop(self) -> None:
        try:
            self.midi_out.all_notes_off()
        except Exception:  # noqa: BLE001
            log.warning("all_notes_off failed during shutdown", exc_info=True)
        try:
            self.link.send_clear()
        except Exception:  # noqa: BLE001
            log.warning("send_clear failed during shutdown", exc_info=True)
        self.clock.stop()
        self.link.stop()

    # -- link callbacks ---------------------------------------------------

    def _on_hello(self, hello: codec.Hello) -> None:
        self.hello = hello
        log.info("HELLO: %s", hello)
        self.link.send_brightness(self.brightness)

    def _on_key(self, key: codec.Key) -> None:
        self.interaction_features.on_key_event(key.x, key.y, bool(key.pressed))
        self.active_menu.on_key(key.x, key.y, bool(key.pressed))

    def _on_analog(self, analog: codec.Analog) -> None:
        self.interaction_features.on_analog_event(analog.values)
        readings = self.touch.update(analog.values)

        new_channel = self.touch.last_touched_channel
        if (
            new_channel is not None
            and new_channel != self.active_menu_idx
            and new_channel in self.menus
        ):
            self._switch_menu(new_channel)

        for ch, reading in enumerate(readings):
            self.active_menu.on_analog(ch, reading)

    def _switch_menu(self, idx: int) -> None:
        log.info("menu switch: %d (%s) -> %d (%s)",
                  self.active_menu_idx, self.active_menu.name, idx, self.menus[idx].name)
        self.active_menu.on_exit()
        self.active_menu_idx = idx
        self.active_menu.on_enter()
        self._arm_switch_cue(idx)

    def _arm_switch_cue(self, idx: int) -> None:
        # Brief full-grid flash in a per-menu color so a human gets instant
        # feedback that the switch registered, before the new menu's own
        # (probably mostly-black/empty) render takes over.
        cue_colors = {
            0: (255, 60, 0),
            1: (0, 200, 255),
            2: (200, 0, 200),
            3: (120, 120, 120),
        }
        self._cue_color = cue_colors.get(idx, (255, 255, 255))
        self._cue_until = time.monotonic() + MENU_SWITCH_CUE_SECONDS

    # -- clock callbacks ---------------------------------------------------

    def _on_clock_step(self, step: int) -> None:
        self.active_menu.on_clock_step(step)

    def _on_clock_downbeat(self) -> None:
        # Atomic staged -> active config swap, gated to the downbeat. This
        # fires whether the pulse came from a real external MIDI clock or
        # Clock's own free-running internal fallback (clock.py unifies
        # both onto on_downbeat), so hooking this one callback covers both
        # per the build spec.
        self._downbeat_count += 1
        with self._staged_lock:
            staged = self._staged
            self._staged = None
        if staged is not None:
            cfg, source = staged
            self.config = cfg  # plain reference assignment: atomic, no lock needed to read
            log.info("config swap on beat %d from %s", self._downbeat_count, source)
            new_menu = cfg.get("active_menu", self.active_menu_idx)
            if new_menu != self.active_menu_idx and new_menu in self.menus:
                self._switch_menu(new_menu)
        self.active_menu.on_clock_downbeat()

    # -- config boundary (Phase 3) ---------------------------------------

    def stage_config(self, data, source: str) -> bool:
        """Thread-safe: stage a new config to be swapped in atomically on
        the next downbeat. `data` may be a config dict or a
        (config.VECTOR_LEN,) float vector (e.g. a model's raw output).

        Validates immediately (wrong schema_version, wrong vector length,
        or out-of-range dict fields) and rejects on the spot -- the active
        config is never touched by a bad stage, and this never blocks the
        hot loop. Returns True if staged, False if rejected (and logs a
        warning either way it was rejected)."""
        cfg = self._decode_staged(data, source)
        if cfg is None:
            return False
        with self._staged_lock:
            self._staged = (cfg, source)
        return True

    @staticmethod
    def _decode_staged(data, source: str) -> Optional[Dict]:
        try:
            if isinstance(data, dict):
                sv = data.get("schema_version", config_mod.SCHEMA_VERSION)
                if sv != config_mod.SCHEMA_VERSION:
                    log.warning(
                        "rejected staged config from %s: schema_version %r != %d",
                        source, sv, config_mod.SCHEMA_VERSION,
                    )
                    return None
                cfg = dict(data)
                config_mod.validate(cfg)
                return cfg

            vec = np.asarray(data, dtype=np.float32)
            if vec.shape != (config_mod.VECTOR_LEN,):
                log.warning(
                    "rejected staged config from %s: wrong vector length %s (want (%d,))",
                    source, vec.shape, config_mod.VECTOR_LEN,
                )
                return None
            cfg = config_mod.from_vector(vec)
            config_mod.validate(cfg)
            return cfg
        except ValueError as exc:
            log.warning("rejected staged config from %s: %s", source, exc)
            return None
        except Exception:  # noqa: BLE001
            log.warning("rejected staged config from %s: unexpected error", source, exc_info=True)
            return None

    # -- render loop ---------------------------------------------------

    def tick(self) -> None:
        now = time.monotonic()
        self.active_menu.tick(now)

        if now < self._cue_until:
            for i in range(N_LEDS):
                self.framebuffer[i] = self._cue_color
        else:
            self.active_menu.render(self.framebuffer)

        if now - self._last_send_time >= self._min_frame_interval:
            if self.framebuffer != self._prev_sent_fb:
                self._send_frame()
            self._last_send_time = now

    def _send_frame(self) -> None:
        packed = _apply_color_order(self.framebuffer, self.color_order)
        self.link.send_leds_full(packed)
        self._prev_sent_fb = list(self.framebuffer)

    def run_forever(self, extra_tick=None) -> None:
        """`extra_tick`, if given, is called once per loop iteration after
        engine.tick() -- used by run_live.py --stage-demo to drive
        live/stager_manual.ManualStager without the engine needing to know
        anything about stagers."""
        self.start()
        try:
            while True:
                self.tick()
                if extra_tick is not None:
                    extra_tick()
                time.sleep(0.005)
        except KeyboardInterrupt:
            pass
        finally:
            self.stop()
