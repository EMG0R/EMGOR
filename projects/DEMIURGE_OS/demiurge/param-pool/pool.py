"""pool.py — DESIGN SKELETON for the DEMIURGE parameter pool. UNWIRED, NOT RUN.

Would live inside demiurge-io (src/demiurge-io/demiurge_io/pool.py). The daemon's
existing dispatch loop (conditioning -> router) feeds `Pool.on_source()` and the
existing sinks (sinks/csound.py, + a generic osc sink) are registered with
`Pool.add_sink()`. See docs/parameters.md.

Contract: every value that crosses this API is a float in 0..1.
"""
from __future__ import annotations

import json
import os
from pathlib import Path
from typing import Callable, Dict, List, Optional

HOME_DEMIURGE = Path.home() / "demiurge"


def clamp01(v: float) -> float:
    return 0.0 if v < 0.0 else (1.0 if v > 1.0 else float(v))


def patch_map_path(patch: str, root: Path = HOME_DEMIURGE) -> Path:
    """~/demiurge/patches/<patch>.map.json — sits next to <patch>.conf."""
    return root / "patches" / f"{patch}.map.json"


def active_patch(live_conf: Path = HOME_DEMIURGE / "live.conf") -> Optional[str]:
    """The `# set: <name>` header of live.conf (the patch copy already writes it)."""
    try:
        for line in live_conf.read_text().splitlines():
            s = line.strip()
            if s.lower().startswith("# set:"):
                return s.split(":", 1)[1].strip() or None
    except OSError:
        pass
    return None


def atomic_write_json(path: Path, obj) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(json.dumps(obj, indent=2) + "\n")
    os.replace(tmp, path)  # readers never see a half-write


class ParamRegistry:
    """path -> sink target dict, in the shape sinks already take, e.g.
    {"sink": "csound", "kind": "expr", "id": 30}. Loaded from per-stage manifests
    (~/demiurge/params/*.json) and, for Csound, synthesized from the generated
    handler/expr_targets.json so there is one source of truth, not two lists."""

    def __init__(self) -> None:
        self._targets: Dict[str, dict] = {}
        self._meta: Dict[str, dict] = {}

    def register(self, path: str, target: dict, **meta) -> None:
        self._targets[path] = target
        self._meta[path] = meta  # label, default, native{min,max,curve,unit}

    def target(self, path: str) -> Optional[dict]:
        return self._targets.get(path)

    def paths(self) -> List[str]:
        return sorted(self._targets)

    def load_expr_targets(self, doc: dict) -> None:
        """expr_targets.json: {"targets":[{"id","label","writes":[...]}]}."""
        for t in doc.get("targets", []):
            writes = t.get("writes") or [f"id{t['id']}"]
            path = "/csound/" + writes[0]
            self.register(path, {"sink": "csound", "kind": "expr", "id": t["id"]},
                          label=t.get("label", ""))


class Pool:
    def __init__(self, registry: ParamRegistry) -> None:
        self.registry = registry
        self.values: Dict[str, float] = {}
        self._subs: List[Callable[[str, float], None]] = []
        self._sinks: Dict[str, object] = {}   # name -> object with send(target, value)
        self.maps: List[dict] = []             # existing sensors.json map schema
        self.patch: Optional[str] = None

    # -- wiring (daemon does this at start) ---------------------------------
    def add_sink(self, name: str, sink) -> None:
        self._sinks[name] = sink

    def subscribe(self, fn: Callable[[str, float], None]) -> None:
        """fn(path, value01) — telemetry /param fan-out, web UI echo, monitor snapshot."""
        self._subs.append(fn)

    # -- the core verb -------------------------------------------------------
    def publish(self, path: str, value: float) -> bool:
        """Set a parameter (0..1). The one and only route to an engine."""
        v = clamp01(value)
        target = self.registry.target(path)
        if target is None:
            return False                       # unknown param: dropped, never guessed
        self.values[path] = v
        sink = self._sinks.get(target.get("sink", ""))
        if sink is not None:
            sink.send(target, v)               # rate cap / loopback allowlist live in the sink layer
        for fn in self._subs:
            fn(path, v)
        return True

    # -- hardware -> parameter ----------------------------------------------
    def map(self, source: str, path: str, **router_fields) -> dict:
        """Add/replace a mapping using the existing router map schema."""
        entry = {"id": f"{source}->{path}", "source": source,
                 "target": {"param": path}, **router_fields}
        self.maps = [m for m in self.maps if m["id"] != entry["id"]] + [entry]
        return entry

    def unmap(self, map_id: str) -> None:
        self.maps = [m for m in self.maps if m["id"] != map_id]

    def on_source(self, source: str, norm: float) -> None:
        """Called by the daemon after conditioning.py. Real impl runs router.apply_map
        (zone/curve/mode/slew/combine) per map; elided here on purpose."""
        for m in self.maps:
            if m["source"] == source and m.get("enabled", True):
                self.publish(m["target"]["param"], norm)

    # -- per-patch store -------------------------------------------------------
    def save(self, patch: Optional[str] = None) -> Path:
        patch = patch or self.patch
        if not patch:
            raise ValueError("no active patch to save mappings under")
        p = patch_map_path(patch)
        atomic_write_json(p, {"version": 1, "patch": patch, "maps": self.maps,
                              "values": self.values})
        return p

    def load(self, patch: str, global_maps: Optional[List[dict]] = None) -> None:
        """Global (sensors.json) maps first, then the patch's maps override by id.
        A missing or bad patch file leaves the previous maps in place."""
        merged = {m["id"]: m for m in (global_maps or [])}
        try:
            doc = json.loads(patch_map_path(patch).read_text())
            for m in doc.get("maps", []):
                merged[m["id"]] = m
            baseline = doc.get("values", {})
        except (OSError, ValueError):
            baseline = {}
        self.patch = patch
        self.maps = list(merged.values())
        for path, v in baseline.items():
            self.publish(path, v)
