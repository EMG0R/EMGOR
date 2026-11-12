"""Ring topology: ONE render engine at ring resolution, sliced per strip.

PHYSICAL RING (as installed in the room): the three FCOB strips form a
single closed 1920px loop. Walking the ring forward:

    fcob-strip   px 0..799   = ring[   0.. 799]   (beginning of board1
                                                   touches end of board3)
    fcob-strip2  px 0..479   = ring[ 800..1279]   (beginning of board2
                                                   touches end of board1)
    fcob-strip3  px 0..639   = ring[1280..1919]   (beginning of board3
                                                   touches end of board2)
    ... and ring[1919] is physically adjacent to ring[0].

Config file `ring.json` lives at the repo root (next to `nodes.json`):

    {"segments": [
      {"node": "fcob-strip",  "ring_offset": 0,    "length": 800, "reversed": false},
      {"node": "fcob-strip2", "ring_offset": 800,  "length": 480, "reversed": false},
      {"node": "fcob-strip3", "ring_offset": 1280, "length": 640, "reversed": false}
    ]}

Segments are ordered; `ring_offset` is optional (derived from the running
sum) but if present must match the derived value. `length` defaults to the
node's pixel_count from nodes.json and must match it when both are given.
`reversed: true` means the strip is physically mounted backwards: its
pixel 0 sits at the END of its ring span, so its slice is flipped before
sending. Total ring length is DERIVED (sum of lengths), never hardcoded —
a future 4th board is added by appending one entry here + one in nodes.json.

Architecture: RingEngine subclasses Engine at the full ring resolution.
Scenes render in ring space (one buffer, one clock, one physics world), so
effects cross strip boundaries with plain buffer continuity, and the
0 <-> N-1 seam is handled by wrap-aware layers (particles physics
`"wrap": true`, palettes/waves whose period divides N). Each frame the
uint8 buffer is sliced per segment and handed to that node's own UdpSender
(Frame Protocol v1, offset 0 per node). A missing node just doesn't
receive its slice — its sender drops frames until mDNS resolves — and the
rest of the ring is unaffected.

RingManager presents the NodeManager interface to the API but the ring is
THE ONLY control target: every targeting call resolves to the one
RingEngine regardless of any `node` argument (the ring cannot be split
from the public API — node params are accepted and ignored). GET /nodes
still reports per-strip transport status from the per-segment senders.

Persistence: the ring's {scene, streaming} is stored under the single
node name "ring" in state.json (same v1 schema — clean migration: old
per-strip entries are simply ignored, so the boot default applies once,
then ring state persists as before).
"""
from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path

from . import DEFAULT_FPS
from .calibration import CalibrationManager
from .engine import Engine
from .nodes import NodeConfig, NodeManager
from .protocol import UdpSender
from .satellites import (VALID_MODES, SatelliteRuntime, normalize_boost,
                         validate_vivid_lift, validate_vivid_saturation)
from .sync import SyncManager
from .taps import TapManager

DEFAULT_RING_PATH = Path(__file__).resolve().parent.parent / "ring.json"

RING_NAME = "ring"


class RingConfigError(ValueError):
    pass


@dataclass
class RingSegment:
    node: str            # node name from nodes.json (host/ip/port live there)
    ring_offset: int     # first ring index of this segment
    length: int          # pixels
    reversed: bool = False  # strip mounted backwards: flip slice before send

    def as_dict(self) -> dict:
        return {"node": self.node, "ring_offset": self.ring_offset,
                "length": self.length, "reversed": self.reversed}


def load_ring_segments(path: str | Path | None,
                       configs: list[NodeConfig]) -> list[RingSegment]:
    """Load + validate ring.json against the node registry.

    Raises RingConfigError on inconsistency (unknown node, offset/length
    mismatch, duplicates) — a broken ring config should fail loudly at
    boot, not half-drive the strips.
    """
    p = Path(path) if path else DEFAULT_RING_PATH
    try:
        raw = json.loads(p.read_text())
    except (OSError, json.JSONDecodeError) as e:
        raise RingConfigError(f"cannot read ring config {p}: {e}")
    seg_list = raw.get("segments") if isinstance(raw, dict) else None
    if not isinstance(seg_list, list) or not seg_list:
        raise RingConfigError(f"{p}: 'segments' must be a non-empty list")

    by_name = {c.name: c for c in configs}
    out: list[RingSegment] = []
    seen: set[str] = set()
    offset = 0
    for i, d in enumerate(seg_list):
        if not isinstance(d, dict) or "node" not in d:
            raise RingConfigError(f"segments[{i}]: must be an object with 'node'")
        node = str(d["node"])
        if node in seen:
            raise RingConfigError(f"segments[{i}]: duplicate node '{node}'")
        seen.add(node)
        cfg = by_name.get(node)
        if cfg is None:
            raise RingConfigError(f"segments[{i}]: unknown node '{node}' "
                                  f"(not in nodes.json)")
        length = int(d.get("length", cfg.pixel_count))
        if length != cfg.pixel_count:
            raise RingConfigError(
                f"segments[{i}] '{node}': length {length} != "
                f"pixel_count {cfg.pixel_count} in nodes.json")
        if "ring_offset" in d and int(d["ring_offset"]) != offset:
            raise RingConfigError(
                f"segments[{i}] '{node}': ring_offset {d['ring_offset']} != "
                f"derived {offset} (segments must be contiguous, in order)")
        out.append(RingSegment(node=node, ring_offset=offset, length=length,
                               reversed=bool(d.get("reversed", False))))
        offset += length
    return out


def ring_length(segments: list[RingSegment]) -> int:
    return sum(s.length for s in segments)


class RingEngine(Engine):
    """One Engine rendering the whole ring; per-frame slice + dispatch.

    num_pixels = derived ring length. `mode="udp"` creates one UdpSender
    per segment (targeting that node's host/ip/port from nodes.json);
    any other mode leaves senders as None (tests/preview/headless).
    """

    def __init__(self, segments: list[RingSegment], configs: list[NodeConfig],
                 fps: int = DEFAULT_FPS, mode: str = "udp",
                 seed: int | None = None,
                 satellites: list[SatelliteRuntime] | None = None):
        n = ring_length(segments)
        # Engine's own single-target sender is unused: init headless-shaped,
        # then restore the requested mode so streaming semantics hold.
        super().__init__(num_pixels=n, fps=fps, mode="headless", seed=seed,
                         name=RING_NAME)
        self.mode = mode
        if mode == "preview":
            from .preview import TerminalPreview
            self.preview = TerminalPreview(n)
        by_name = {c.name: c for c in configs}
        self.segments = list(segments)
        self.senders: dict[str, UdpSender | None] = {}
        for seg in self.segments:
            cfg = by_name[seg.node]
            self.senders[seg.node] = (
                UdpSender(cfg.ip or None, cfg.port, host=cfg.host)
                if mode == "udp" else None)

        # Satellite "dot" nodes: anchored to a ring position, mirrored (or
        # independently colored) each frame — cheap, ~pixel_count px each.
        # An offline dot is harmless: same lazy-mDNS UdpSender as a strip.
        self.satellites = list(satellites or [])
        for sat in self.satellites:
            cfg = by_name.get(sat.node)
            sat.sender = (UdpSender(cfg.ip or None, cfg.port, host=cfg.host)
                         if (mode == "udp" and cfg is not None) else None)

        # Canvas taps: self-expiring UDP subscriptions to the raw ring
        # canvas for external consumers (see taps.py). Independent of
        # `mode`/`self.streaming` -- a tap is presence-based (TTL), not
        # tied to whether the strips themselves are streaming.
        self.taps = TapManager(n)

        # Room calibration overlay (see calibration.py): while active,
        # _emit substitutes this frame for whatever the scene rendered,
        # for EVERY consumer (strips, satellites, taps, preview) -- the
        # scene keeps rendering underneath untouched, it's just not what
        # goes out over the wire. No special-casing needed for
        # satellites/taps: they naturally see the overlay because it's
        # the same `frame` object passed to all of them below.
        self.calibration = CalibrationManager(n)

        # Output-delay AV sync (see sync.py, docs/SYNC_DESIGN.md): strips
        # + satellites are held back `output_delay_ms` behind the current
        # frame so they land in step with a slow external projection
        # consumer (GORMAPPER, via a tap); taps themselves always get the
        # freshest frame. Bypassed entirely while calibration is active
        # (see _emit below).
        self.sync = SyncManager(fps=fps)

        # Scene the ring was showing right before a sync-pulse calibration
        # scene was activated via POST /sync/pulse/start; restored verbatim
        # by POST /sync/pulse/stop. None when no pulse is in progress.
        self._pre_pulse_scene: dict | None = None

    # -- output ------------------------------------------------------------
    def slice_segment(self, frame, seg: RingSegment):
        """The ring->strip mapping: ring[offset : offset+length], flipped
        when the strip is mounted reversed."""
        sub = frame[seg.ring_offset:seg.ring_offset + seg.length]
        return sub[::-1] if seg.reversed else sub

    def _emit(self, frame) -> None:
        if self.calibration.active:
            # Calibration overlay bypasses the delay buffer entirely --
            # markers must respond to slider drags instantly, on every
            # consumer (strips, satellites, taps, preview) alike.
            frame = self.calibration.render()
            delayed = frame
        else:
            self.sync.push(frame)
            delayed = self.sync.delayed_frame(frame)
        if self.streaming:
            for seg in self.segments:
                snd = self.senders.get(seg.node)
                if snd is not None:
                    snd.send_frame(self.slice_segment(delayed, seg).tobytes())
            for sat in self.satellites:
                if sat.sender is not None:
                    sat.sender.send_frame(sat.render(delayed, self.n).tobytes())
        with self._lock:
            scene = self._scene
        # Taps (pixel + semantic) always get the CURRENT frame, undelayed
        # -- the delay only applies to the physical outputs above.
        self.taps.emit(frame, scene=scene,
                       output_delay_ms=self.sync.output_delay_ms)
        if self.preview is not None:
            self.preview.show(frame)

    def shutdown(self) -> None:
        super().shutdown()
        for snd in self.senders.values():
            if snd is not None:
                snd.close()
        for sat in self.satellites:
            if sat.sender is not None:
                sat.sender.close()
        self.taps.close()

    # -- stats -------------------------------------------------------------
    def stats(self) -> dict:
        s = super().stats()
        s["ring"] = {
            "length": self.n,
            "segments": [dict(seg.as_dict(), **self._sender_stats(seg.node))
                         for seg in self.segments],
        }
        return s

    def _sender_stats(self, node: str) -> dict:
        snd = self.senders.get(node)
        if snd is None:
            return {"online": False}
        return {
            "resolved_ip": snd.resolved_ip,
            "last_known_ip": snd.ip,
            "frames_sent": snd.frames_sent,
            "last_send_ts": snd.last_send_ts,
            "last_error": snd.last_error,
            "online": bool((snd.resolved_ip or snd.ip)
                           and snd.frames_sent > 0 and snd.last_error is None),
        }


class RingManager(NodeManager):
    """NodeManager facade where the RING is the only control target.

    Public API consequence (documented contract): /prompt, /scene, /pixels,
    /stop, /resume always act on the whole ring — a `node` argument is
    ACCEPTED AND IGNORED, so a caller (e.g. the website agent) is
    physically unable to split the ring. GET /nodes keeps per-strip
    transport status.
    """

    is_ring = True

    def __init__(self, segments: list[RingSegment], engine: RingEngine,
                 configs: list[NodeConfig]) -> None:
        super().__init__()
        self.segments = segments
        for cfg in configs:
            self.configs[cfg.name] = cfg
        self.engines[RING_NAME] = engine
        self.default_name = RING_NAME

    @classmethod
    def from_configs(cls, configs: list[NodeConfig],
                     segments: list[RingSegment], fps: int = DEFAULT_FPS,
                     mode: str = "udp", seed: int | None = None,
                     satellites: list[SatelliteRuntime] | None = None
                     ) -> "RingManager":
        eng = RingEngine(segments, configs, fps=fps, mode=mode, seed=seed,
                         satellites=satellites)
        return cls(segments, eng, configs)

    @property
    def ring(self) -> RingEngine:
        return self.engines[RING_NAME]  # type: ignore[return-value]

    # -- targeting: everything is the ring ---------------------------------
    def get(self, name: str | None = None) -> Engine:
        return self.ring  # node names ignored by design (ring is atomic)

    def targets(self, name: str | None = None):
        return [(RING_NAME, self.ring)]

    # -- status: per-strip transport from the segment senders --------------
    def status(self) -> list[dict]:
        eng = self.ring
        out = []
        for seg in self.segments:
            cfg = self.configs.get(seg.node)
            d = cfg.as_dict() if cfg else {"name": seg.node}
            d.update(seg.as_dict())
            d["default"] = False
            d["scene"] = eng.current_scene_name
            d["streaming"] = bool(eng.streaming and eng.mode == "udp")
            d.update(eng._sender_stats(seg.node))
            out.append(d)
        return out

    # -- satellites ("dot" nodes) --------------------------------------------
    def satellites_status(self) -> list[dict]:
        return [sat.as_status() for sat in self.ring.satellites]

    def set_satellite(self, name: str, anchor: int | None = None,
                      mode: str | None = None, color: str | None = None,
                      boost: str | float | None = None,
                      vivid_saturation: float | None = None,
                      vivid_lift: float | None = None) -> dict:
        """Reposition/switch-mode/set-color/set-boost a satellite. Raises
        KeyError for an unknown satellite, ValueError for a bad
        anchor/mode/boost/vivid_saturation/vivid_lift. `vivid_saturation`
        and `vivid_lift` are independently settable — a caller can PATCH
        either on an already-"vivid" satellite without resending `boost`."""
        sat = next((s for s in self.ring.satellites if s.node == name), None)
        if sat is None:
            raise KeyError(name)
        if anchor is not None:
            if not (0 <= anchor < self.ring.n):
                raise ValueError(f"anchor {anchor} out of range 0..{self.ring.n - 1}")
            sat.anchor = anchor
        if mode is not None:
            if mode not in VALID_MODES:
                raise ValueError(f"invalid mode '{mode}' (must be one of "
                                 f"{sorted(VALID_MODES)})")
            sat.mode = mode
        if color is not None:
            sat.color = color
        if boost is not None:
            sat.boost = normalize_boost(boost)  # raises ValueError on garbage
        if vivid_saturation is not None:
            # raises ValueError on garbage/out-of-range
            sat.vivid_saturation = validate_vivid_saturation(vivid_saturation)
        if vivid_lift is not None:
            # raises ValueError on garbage/out-of-range
            sat.vivid_lift = validate_vivid_lift(vivid_lift)
        return sat.as_status()

    # -- canvas taps -----------------------------------------------------
    def taps_status(self) -> list[dict]:
        return self.ring.taps.status_list()

    def tap_status(self, name: str) -> dict | None:
        return self.ring.taps.status_one(name)

    def set_tap(self, name: str, host: str, port: int | None = None,
               fps_divisor: int | None = None, ttl: int | None = None,
               offset: int | None = None, count: int | None = None,
               semantic_port: int | None = None) -> dict:
        """Create/keepalive a tap; raises ValueError on bad params or a
        missing port on first create."""
        return self.ring.taps.set_tap(name, host, port=port, fps_divisor=fps_divisor,
                                      ttl=ttl, offset=offset, count=count,
                                      semantic_port=semantic_port)

    def disable_tap(self, name: str) -> None:
        """Raises KeyError for a never-configured tap name."""
        self.ring.taps.disable(name)

    # -- room calibration --------------------------------------------------
    def calibration_start(self, points: dict | None = None) -> dict:
        """Raises ValueError on a bad color/index in `points`."""
        return self.ring.calibration.start(points)

    def calibration_move(self, updates: dict) -> dict:
        """Raises KeyError for an unknown point name, ValueError for an
        out-of-range index or if calibration mode isn't active."""
        return self.ring.calibration.move(updates)

    def calibration_done(self) -> dict:
        return self.ring.calibration.done()

    def calibration_cancel(self) -> None:
        self.ring.calibration.cancel()

    def calibration_status(self) -> dict:
        return self.ring.calibration.status()

    def calibration_saved(self) -> dict:
        return self.ring.calibration.saved_status()

    # -- output-delay AV sync (see sync.py, docs/SYNC_DESIGN.md) ----------
    def sync_status(self) -> dict:
        return self.ring.sync.status()

    def set_sync(self, output_delay_ms: int) -> dict:
        """Raises ValueError if `output_delay_ms` is out of range."""
        self.ring.sync.set_delay_ms(output_delay_ms)
        return self.ring.sync.status()

    def sync_pulse_start(self) -> dict:
        """Activate the installed sync-pulse calibration scene, remembering
        whatever scene was running so sync_pulse_stop() can restore it
        exactly. Calling start() again while a pulse is already running
        does NOT overwrite the remembered prior scene (idempotent)."""
        from .nl import SCENES_DIR
        pulse_path = Path(SCENES_DIR) / "sync-pulse.json"
        pulse_scene = json.loads(pulse_path.read_text())
        if self.ring._pre_pulse_scene is None:
            with self.ring._lock:
                is_pulse = self.ring._scene.name == pulse_scene.get("name")
            if not is_pulse:
                self.ring._pre_pulse_scene = self.ring.current_scene
        self.ring.set_scene(pulse_scene, crossfade=0)
        return {"scene": self.ring.current_scene_name}

    def sync_pulse_stop(self) -> dict:
        """Restore whatever scene was running before sync_pulse_start(),
        exactly. A no-op (keeps the current scene) if no pulse was ever
        started."""
        prior = self.ring._pre_pulse_scene
        if prior is not None:
            self.ring.set_scene(prior, crossfade=0)
            self.ring._pre_pulse_scene = None
        return {"scene": self.ring.current_scene_name}

    # -- persistence: extends NodeManager's {scene, streaming} with the
    # satellites' {anchor, mode, color}, stored under a separate top-level
    # key in the same state.json (see persist.py) -----------------------
    def restore(self, store) -> None:
        super().restore(store)
        self.ring.taps.restore_config(store.load_taps())
        self.ring.calibration.restore(store.load_calibration())
        self.ring.sync.restore(store.load_sync())
        saved = store.load_satellites()
        for sat in self.ring.satellites:
            st = saved.get(sat.node)
            if not st:
                continue
            anchor = st.get("anchor")
            if isinstance(anchor, int) and 0 <= anchor < self.ring.n:
                sat.anchor = anchor
            mode = st.get("mode")
            if mode in VALID_MODES:
                sat.mode = mode
            sat.color = st.get("color")
            try:
                sat.boost = normalize_boost(st.get("boost", "off"))
            except ValueError:
                pass  # bad persisted value: keep the config-file default
            try:
                sat.vivid_saturation = validate_vivid_saturation(
                    st.get("vivid_saturation"))
            except ValueError:
                pass  # bad persisted value: keep the config-file default
            try:
                # st.get("vivid_lift") -> None for state.json written
                # before this field existed; validate_vivid_lift(None)
                # cleanly resolves that to VIVID_LIFT_DEFAULT (0.7), same
                # as an omitted config-file field — no error either way.
                sat.vivid_lift = validate_vivid_lift(st.get("vivid_lift"))
            except ValueError:
                pass  # bad persisted value: keep the config-file default

    def persist(self) -> None:
        super().persist()
        if self.store is None:
            return
        sat_states = {sat.node: {"anchor": sat.anchor, "mode": sat.mode,
                                 "color": sat.color, "boost": sat.boost,
                                 "vivid_saturation": sat.vivid_saturation,
                                 "vivid_lift": sat.vivid_lift}
                     for sat in self.ring.satellites}
        self.store.save_satellites(sat_states)
        self.store.save_taps(self.ring.taps.config_for_persist())
        self.store.save_calibration(self.ring.calibration.for_persist())
        self.store.save_sync(self.ring.sync.for_persist())
