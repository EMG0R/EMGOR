"""pool.py — the parameter registry + the one publish verb. Not a daemon: the
in-process API the existing Daemon calls (docs/parameters.md §3-4).

CONTRACT: every value crossing this API is a float in 0..1. Native ranges are
the egress sink's last-inch job (sinks/osc.py `native`), never the pool's.

REGISTRY: path -> sink-target dict, in the exact shape the existing sinks
already take ({"sink":"csound","kind":"expr","id":30}, {"sink":"osc",...}).
Two inputs, one merged view:

  * Csound is SYNTHESIZED from the generated handler/expr_targets.json
    (`id`, `label`, `writes`). It is never written to ~/demiurge/params/ — that
    would be a second Csound list to drift. One source of truth.
  * Every other stage writes its own manifest ~/demiurge/params/<stage>.json
    (write_manifest() below is the helper a stage's startup uses):
      {"stage":"sc-pad","port":9002,"version":1,"host":"127.0.0.1",
       "params":[{"path":"/sc-pad/cutoff","label":"Cutoff","default":0.5,
                  "address":"/cutoff","native":{"min":40,"max":12000,
                  "curve":"exp","unit":"Hz"}}]}
    A param may instead carry "midi":{"ch":1,"cc":20} to target the midi sink.
    A manifest named/declared stage "csound" is ignored (see above).

HOT RELOAD: refresh() re-reads when the params dir / expr_targets file mtimes
change (same philosophy as ConfigWatcher); a bad file keeps the previous
registry. Unknown paths are dropped, never guessed.
"""

from __future__ import annotations

import json
import os
import re
import sys
from pathlib import Path
from typing import Callable, Dict, List, Optional

from .config import DEFAULT_ROOT, atomic_write_json, normalize_maps

PARAMS_DIR = DEFAULT_ROOT / "params"

# Built-in pool parameters that are NOT in expr_targets.json: NEPTR's "intent"
# layer entries the pool must be able to address (docs/neptr-param-migration.md).
# Kept as data so the registry stays the one list. Opt-in per registry
# (ParamRegistry(builtins=True)) so a plain registry still contains exactly
# manifests + expr_targets.
#   /csound/neptr/expr  the ONE expression pedal: ch1 CC29 as `/cc` on 7770.
#       Through OSC it is exempt from the engine's "parked onboard pedal" demotion
#       (kFromOsc) and runs the identical downstream path as the DUODECIMUS
#       funnel: gkLastToggle29 routing + soft takeover in expr_cc29.orc.
#   /global/tap         a tempo tap toward demiurge-clock (sinks/transport.py).
BUILTIN_TARGETS = {
    "/csound/neptr/expr": ({"sink": "csound", "kind": "cc", "ch": 1, "cc": 29, "norm": True},
                           {"label": "Expression pedal (ch1 CC29)", "stage": "csound"}),
    "/global/tap": ({"sink": "transport", "kind": "tap"},
                    {"label": "Tempo tap (NEPTR arcade)", "stage": "global"}),
}


def expr_targets_candidates() -> List[Path]:
    """Same lookup order as demiurge_web.EXPR_TARGETS_CANDIDATES, $HOME-based."""
    h = Path.home()
    return [h / "NEPTR_phase4" / "csound" / "handler" / "expr_targets.json",
            h / "NEPTR" / "csound" / "handler" / "expr_targets.json"]


def clamp01(v: float) -> float:
    v = float(v)
    return 0.0 if v < 0.0 else (1.0 if v > 1.0 else v)


def _slug(s: str) -> str:
    return re.sub(r"[^A-Za-z0-9_-]+", "_", s).strip("_") or "param"


def write_manifest(stage: str, params: list, port: int, params_dir: Path = PARAMS_DIR,
                   host: str = "127.0.0.1", version: int = 1) -> Path:
    """What a stage calls at startup to publish its parameters. Atomic."""
    if stage == "csound":
        raise ValueError("csound's list is synthesized from expr_targets.json; "
                         "it must not have a manifest")
    p = Path(params_dir) / f"{_slug(stage)}.json"
    atomic_write_json(p, {"stage": stage, "port": int(port), "host": host,
                          "version": version, "params": params})
    return p


class ParamRegistry:
    def __init__(self, params_dir: Path = PARAMS_DIR,
                 expr_targets: Optional[Path] = None, builtins: bool = False):
        self.params_dir = Path(params_dir)
        self.expr_targets = Path(expr_targets) if expr_targets else None
        self.builtins = builtins
        self._targets: Dict[str, dict] = {}
        self._meta: Dict[str, dict] = {}
        self._hard_maps: List[dict] = []     # manifest `hard_maps` (parameters.md §11)
        self._sig = None

    # ── building ────────────────────────────────────────────────────────
    def register(self, path: str, target: dict, **meta) -> None:
        self._targets[path] = target
        self._meta[path] = meta

    def load_builtins(self) -> int:
        for path, (target, meta) in BUILTIN_TARGETS.items():
            self.register(path, dict(target), **meta)
        return len(BUILTIN_TARGETS)

    def load_expr_targets(self, doc: dict) -> int:
        """expr_targets.json {"targets":[{"id","label","writes":[...]}]} ->
        /csound/<first write>. A duplicate slug gets the id appended."""
        n = 0
        for t in doc.get("targets", []):
            if "id" not in t:
                continue
            writes = t.get("writes") or [f"id{t['id']}"]
            path = "/csound/" + writes[0]
            if path in self._targets:
                path = f"{path}_{t['id']}"
            self.register(path, {"sink": "csound", "kind": "expr", "id": int(t["id"])},
                          label=t.get("label", ""), stage="csound")
            n += 1
        return n

    def load_manifest(self, doc: dict) -> int:
        stage = doc.get("stage")
        if not isinstance(stage, str) or stage == "csound":
            return 0
        host = doc.get("host", "127.0.0.1")
        n = 0
        for m in normalize_maps(doc.get("hard_maps", [])):
            m = dict(m)
            m["hard"] = True
            self._hard_maps.append(m)
        for p in doc.get("params", []):
            path = p.get("path")
            if not isinstance(path, str) or not path.startswith(f"/{stage}/"):
                continue
            if isinstance(p.get("midi"), dict):
                target = {"sink": "midi", "ch": int(p["midi"].get("ch", 1)),
                          "cc": int(p["midi"]["cc"])}
            elif p.get("proto") == "p" and "port" in doc:
                target = {"sink": "osc", "host": host, "port": int(doc["port"]),
                          "proto": "p", "path": path}
            elif "address" in p and "port" in doc:
                target = {"sink": "osc", "host": host, "port": int(doc["port"]),
                          "address": p["address"]}
                if isinstance(p.get("native"), dict):
                    target["native"] = p["native"]
            else:
                continue
            self.register(path, target, label=p.get("label", ""),
                          default=p.get("default"), native=p.get("native"), stage=stage,
                          map="hard" if p.get("map") == "hard" else "soft")
            n += 1
        return n

    # ── reading ─────────────────────────────────────────────────────────
    def _files(self) -> List[Path]:
        try:
            return sorted(self.params_dir.glob("*.json"))
        except OSError:
            return []

    def _signature(self):
        sig = []
        et = self.expr_targets
        sig.append(("expr", os.stat(et).st_mtime if et and et.exists() else None))
        for f in self._files():
            try:
                sig.append((f.name, os.stat(f).st_mtime))
            except OSError:
                pass
        return tuple(sig)

    def refresh(self, force: bool = False) -> bool:
        """Rebuild if anything changed. True if the registry was replaced."""
        try:
            sig = self._signature()
        except OSError:
            return False
        if not force and sig == self._sig:
            return False
        new = ParamRegistry(self.params_dir, self.expr_targets, self.builtins)
        try:
            if self.builtins:
                new.load_builtins()
            if self.expr_targets and self.expr_targets.exists():
                new.load_expr_targets(json.loads(self.expr_targets.read_text()))
            for f in self._files():
                try:
                    new.load_manifest(json.loads(f.read_text()))
                except (OSError, ValueError, KeyError, TypeError) as e:
                    print(f"[demiurge-io] bad manifest {f.name} ignored: {e}",
                          file=sys.stderr, flush=True)
        except (OSError, ValueError) as e:
            print(f"[demiurge-io] registry reload failed, keeping previous: {e}",
                  file=sys.stderr, flush=True)
            self._sig = sig
            return False
        self._targets, self._meta, self._sig = new._targets, new._meta, sig
        self._hard_maps = new._hard_maps
        print(f"[demiurge-io] registry: {len(self._targets)} parameters",
              file=sys.stderr, flush=True)
        return True

    # ── lookup ──────────────────────────────────────────────────────────
    def target(self, path: str) -> Optional[dict]:
        return self._targets.get(path)

    def meta(self, path: str) -> dict:
        return self._meta.get(path, {})

    def paths(self) -> List[str]:
        return sorted(self._targets)

    # ── hard/soft mapping (parameters.md §11) ───────────────────────────
    def is_hard(self, path: str) -> bool:
        """HARD = mapping owned by the instrument's manifest; UI/patch can't remap."""
        return self._meta.get(path, {}).get("map") == "hard"

    def hard_maps(self) -> List[dict]:
        return list(self._hard_maps)

    def enforce_hard(self, maps: List[dict]) -> List[dict]:
        """Drop user (global/patch) maps aimed at a hard param (logged), then
        append the manifests' hard_maps. A hard map with the same id as a user
        map replaces it. Maps with legacy {sink,...} targets pass untouched."""
        out = []
        hard_ids = {m["id"] for m in self._hard_maps}
        for m in maps:
            tgt = m.get("target", {})
            if m.get("id") in hard_ids:
                continue
            if "param" in tgt and self.is_hard(tgt["param"]):
                print(f"[demiurge-io] map {m.get('id')!r} rejected: "
                      f"{tgt['param']} is hard-mapped", file=sys.stderr, flush=True)
                continue
            out.append(m)
        return out + self.hard_maps()


class Pool:
    """Registry + sink table + the single publish verb. The daemon owns one."""

    def __init__(self, registry: ParamRegistry):
        self.registry = registry
        self.values: Dict[str, float] = {}
        self._subs: List[Callable[[str, float], None]] = []
        self.sinks: Dict[str, object] = {}     # name -> object with send(target, value)

    def add_sink(self, name: str, sink) -> None:
        self.sinks[name] = sink

    def subscribe(self, fn: Callable[[str, float], None]) -> None:
        """fn(path, value01): telemetry /param echo, web UI, snapshot."""
        self._subs.append(fn)

    def resolve(self, target: dict) -> Optional[dict]:
        """{"param": path} -> the concrete sink target; concrete targets pass
        through unchanged (old {sink,kind,id} maps stay valid)."""
        if "param" in target:
            return self.registry.target(target["param"])
        return target

    def send_target(self, target: dict, value: float, t0=None) -> bool:
        """The ONE route toward an engine. Returns False if dropped (unknown
        param or unregistered sink) — a mis-typed map must not crash dispatch."""
        concrete = self.resolve(target)
        if concrete is None:
            return False
        sink = self.sinks.get(concrete.get("sink", "csound"))
        if sink is None:
            return False
        if "param" in target:
            value = clamp01(value)       # pool contract; legacy targets keep their own range
            self.values[target["param"]] = value
        if getattr(sink, "wants_t", False):
            sink.send(concrete, value, t0)      # e.g. tap: needs the press time
        else:
            sink.send(concrete, value)
        if "param" in target:
            for fn in self._subs:
                fn(target["param"], value)
        return True

    def publish(self, path: str, value: float) -> bool:
        """Direct parameter write (OSC `/p <path> <val>`, web UI, baselines)."""
        return self.send_target({"param": path}, value)

    def publish_out(self, path: str, value: float) -> bool:
        """Engine-origin report (`/pout`): record + echo to subscribers (the
        telemetry /param) but NEVER send to a sink, so it cannot loop back."""
        if self.registry.target(path) is None:
            return False
        value = clamp01(value)
        self.values[path] = value
        for fn in self._subs:
            fn(path, value)
        return True

    def can_remap(self, path: str) -> bool:
        """Web/API guard: False for hard params (caller refuses and logs)."""
        return not self.registry.is_hard(path)

    def flush(self) -> None:
        for s in self.sinks.values():
            f = getattr(s, "flush", None)
            if f:
                f()
