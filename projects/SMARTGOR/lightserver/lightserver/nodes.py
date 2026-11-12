"""Multi-node registry: config load + one independent Engine per node.

Config file `nodes.json` lives at the repo root (next to this package dir):

    {"nodes": [
      {"name": "fcob-strip",  "host": "fcob-strip.local",
       "ip": "192.168.8.168", "pixel_count": 800, "enabled": true},
      {"name": "fcob-strip2", "host": "fcob-strip2.local",
       "ip": null, "pixel_count": 60, "enabled": true}
    ]}

Fields per node: name (unique id), host (mDNS hostname, preferred),
ip (last-known/fallback IP or null), pixel_count, enabled, port (optional,
default 21324). The FIRST node in the list is the default node.

Each enabled node gets its own Engine (own scene, own buffer at its own
pixel count, own render thread, own UDP sender). Offline nodes cost almost
nothing: their sender drops frames until hostname resolution succeeds
(retried every ~20 s), so nodes are picked up automatically when they
appear on the network.
"""
from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path

from . import DEFAULT_FPS, DEFAULT_IP, DEFAULT_PORT, NUM_PIXELS
from .engine import Engine
from .persist import StateStore
from .scene import SceneValidationError

DEFAULT_NODES_PATH = Path(__file__).resolve().parent.parent / "nodes.json"


@dataclass
class NodeConfig:
    name: str
    host: str | None = None
    ip: str | None = None
    pixel_count: int = 60
    enabled: bool = True
    port: int = DEFAULT_PORT

    def as_dict(self) -> dict:
        return {"name": self.name, "host": self.host, "ip": self.ip,
                "pixel_count": self.pixel_count, "enabled": self.enabled,
                "port": self.port}


def default_node_config() -> NodeConfig:
    """Node 1 — the original 800px FCOB strip, unchanged behavior."""
    return NodeConfig(name="fcob-strip", host="fcob-strip.local",
                      ip=DEFAULT_IP, pixel_count=NUM_PIXELS, enabled=True)


def load_node_configs(path: str | Path | None = None) -> list[NodeConfig]:
    """Load nodes.json; fall back to the single default node if missing/bad."""
    p = Path(path) if path else DEFAULT_NODES_PATH
    try:
        raw = json.loads(p.read_text()).get("nodes", [])
    except (OSError, json.JSONDecodeError, AttributeError):
        raw = []
    out: list[NodeConfig] = []
    seen: set[str] = set()
    for d in raw:
        try:
            name = str(d["name"])
            if name in seen:
                continue
            seen.add(name)
            out.append(NodeConfig(
                name=name,
                host=d.get("host") or None,
                ip=d.get("ip") or None,
                pixel_count=int(d.get("pixel_count", 60)),
                enabled=bool(d.get("enabled", True)),
                port=int(d.get("port", DEFAULT_PORT)),
            ))
        except (KeyError, TypeError, ValueError):
            continue
    if not out:
        out = [default_node_config()]
    return out


class NodeManager:
    """Holds the node registry and one Engine per enabled node.

    The first added node is the default. Targeting:
      get(None)     -> default engine
      get("name")   -> that node's engine (KeyError if unknown/disabled)
      targets(None) -> all enabled engines
      targets(name) -> just that one
    """

    def __init__(self) -> None:
        self.configs: dict[str, NodeConfig] = {}
        self.engines: dict[str, Engine] = {}
        self.default_name: str | None = None
        self.store: StateStore | None = None  # optional scene persistence

    # -- construction ------------------------------------------------------
    def add(self, cfg: NodeConfig, engine: Engine | None) -> None:
        self.configs[cfg.name] = cfg
        if engine is not None:
            self.engines[cfg.name] = engine
            if self.default_name is None:
                self.default_name = cfg.name

    @classmethod
    def from_configs(cls, configs: list[NodeConfig], fps: int = DEFAULT_FPS,
                     mode: str = "udp", seed: int | None = None) -> "NodeManager":
        m = cls()
        for cfg in configs:
            eng = None
            if cfg.enabled:
                eng = Engine(num_pixels=cfg.pixel_count, fps=fps, mode=mode,
                             ip=cfg.ip or "", port=cfg.port, seed=seed,
                             host=cfg.host, name=cfg.name)
            m.add(cfg, eng)
        return m

    @classmethod
    def single(cls, engine: Engine) -> "NodeManager":
        """Wrap an existing engine as the only node (backward-compat path)."""
        m = cls()
        cfg = NodeConfig(name=engine.name, host=engine.host,
                         ip=getattr(engine.sender, "ip", None),
                         pixel_count=engine.n, enabled=True)
        m.add(cfg, engine)
        return m

    # -- targeting ---------------------------------------------------------
    @property
    def default(self) -> Engine:
        if self.default_name is None:
            raise RuntimeError("no enabled nodes")
        return self.engines[self.default_name]

    def get(self, name: str | None = None) -> Engine:
        if name is None:
            return self.default
        if name not in self.engines:
            raise KeyError(name)
        return self.engines[name]

    def targets(self, name: str | None = None) -> list[tuple[str, Engine]]:
        if name is not None:
            return [(name, self.get(name))]
        order = [self.default_name] + [n for n in self.engines
                                       if n != self.default_name]
        return [(n, self.engines[n]) for n in order if n is not None]

    # -- lifecycle ---------------------------------------------------------
    def start_all(self) -> None:
        for eng in self.engines.values():
            eng.start_background()

    def start_others_background(self) -> None:
        """Start every non-default engine in the background (default runs
        blocking in the caller's thread)."""
        for name, eng in self.engines.items():
            if name != self.default_name:
                eng.start_background()

    def set_scene_all(self, scene: dict, crossfade: float | None = None) -> None:
        for _, eng in self.targets(None):
            if crossfade is None:
                eng.set_scene(scene)
            else:
                eng.set_scene(scene, crossfade=crossfade)

    # -- persistence -------------------------------------------------------
    def restore(self, store: StateStore) -> None:
        """Attach a StateStore and apply its persisted per-node state on top
        of whatever boot default was already loaded. Nodes without a
        persisted entry keep the default; a bad/missing state file is a
        no-op. Call BEFORE start_all()."""
        self.store = store
        for name, st in store.load().items():
            eng = self.engines.get(name)
            if eng is None:
                continue
            scene = st.get("scene")
            if scene:
                try:
                    eng.set_scene(scene, crossfade=0)
                except SceneValidationError:
                    continue  # stale/bad persisted scene: keep the default
            if not st.get("streaming", True):
                eng.stop_streaming()

    def persist(self) -> None:
        """Snapshot every node's current scene + streaming intent to the
        attached store (no-op without one). Best-effort, never raises."""
        if self.store is None:
            return
        states = {name: {"scene": eng.current_scene,
                         "streaming": not eng.stopped}
                  for name, eng in self.engines.items()}
        self.store.save(states)

    def shutdown(self) -> None:
        for eng in self.engines.values():
            eng.shutdown()

    # -- status ------------------------------------------------------------
    def status(self) -> list[dict]:
        """Registry + live status for GET /nodes. Never blocks on DNS —
        reports the sender's cached resolution state."""
        out = []
        for name, cfg in self.configs.items():
            d = cfg.as_dict()
            d["default"] = name == self.default_name
            eng = self.engines.get(name)
            if eng is None:
                d.update({"online": False, "scene": None, "streaming": False})
            else:
                d["scene"] = eng.current_scene_name
                d["streaming"] = bool(eng.streaming and eng.mode == "udp")
                snd = eng.sender
                if snd is not None:
                    d["resolved_ip"] = snd.resolved_ip
                    d["last_known_ip"] = snd.ip
                    d["frames_sent"] = snd.frames_sent
                    d["last_send_ts"] = snd.last_send_ts
                    d["last_error"] = snd.last_error
                    d["online"] = bool((snd.resolved_ip or snd.ip)
                                       and snd.frames_sent > 0
                                       and snd.last_error is None)
                else:  # headless / preview engine
                    d["online"] = False
            out.append(d)
        return out
