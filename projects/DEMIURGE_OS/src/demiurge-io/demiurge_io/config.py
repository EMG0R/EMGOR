"""config.py — loads and hot-reloads ~/demiurge/sensors.json.

Schema (see the top-level task spec / README for the field-by-field
rationale — this module just loads and defaults it):

    {"sources": [{"id": ..., "label": ..., "units": ..., "in_lo": ...,
                  "in_hi": ..., "dropout": "hold", "deadzone": 0.0,
                  "smooth_ms": 0.0, "enabled": true}, ...],
     "maps":    [{"id": ..., "source": ..., "enabled": true,
                  "zone_lo": 0.0, "zone_hi": 1.0, "outside": "clamp",
                  "invert": false, "curve": "lin", "out_lo": 0.0,
                  "out_hi": 1.0, "mode": "continuous", "combine": "replace",
                  "slew_ms": 0.0, "steps": 8,
                  "target": {"sink": "csound", "kind": "expr"|"cc", ...}},
                 ...]}

HOT RELOAD: the daemon calls `maybe_reload()` on a cheap interval (an
os.stat() per tick, not a re-parse) — a config edit takes effect on its
next mtime-changed check without a process restart. If the file is
mid-write (rare, but editors do it) and produces invalid JSON, the PREVIOUS
good config is kept and an error is printed; the daemon must never crash or
go silent just because an edit was caught half-written.

PER-PATCH MAPS (parameter pool, docs/parameters.md §5): see PatchMapWatcher at
the bottom. sensors.json keeps hardware calibration and the global fallback
maps; ~/demiurge/patches/<name>/<name>.map.json (else flat patches/<name>.map.json) holds the active patch's maps.

SAFE BOOT: if ~/demiurge/sensors.json does not exist yet, a default is
written that declares all known Teensy sensors (so telemetry/meters work
out of the box) but zero maps — nothing is routed to the engine until a
human opts in. See default_config().
"""

from __future__ import annotations

import json
import os
import sys
from pathlib import Path
from typing import NamedTuple, Optional

# $HOME-relative at runtime (Path.home() reads $HOME), never /home/<user>.
DEFAULT_ROOT = Path.home() / "demiurge"
DEFAULT_PATH = DEFAULT_ROOT / "sensors.json"

_SOURCE_DEFAULTS = {
    "label": "", "units": "", "in_lo": 0.0, "in_hi": 1.0,
    "dropout": "hold", "deadzone": 0.0, "smooth_ms": 0.0, "enabled": True,
}
_MAP_DEFAULTS = {
    "enabled": True, "zone_lo": 0.0, "zone_hi": 1.0, "outside": "clamp",
    "invert": False, "curve": "lin", "out_lo": 0.0, "out_hi": 1.0,
    "mode": "continuous", "combine": "replace", "slew_ms": 0.0, "steps": 8,
}


class Config(NamedTuple):
    sources: dict          # id -> source dict (defaults applied)
    maps: list              # ordered list of map dicts (defaults applied), config order == priority order for combine="replace"/"max"


def default_config(descriptors: dict) -> dict:
    """A safe-boot config: every known sensor declared (so it shows up in
    telemetry / a meters UI), nothing routed anywhere."""
    sources = []
    for d in descriptors.values():
        sources.append({
            "id": d.source_id, "label": d.label, "units": d.units,
            # USABLE band, not the electrical one — see SourceDescriptor.usable
            "in_lo": d.usable[0], "in_hi": d.usable[1],
            "dropout": "hold", "deadzone": 0.02, "smooth_ms": 30.0,
            "enabled": True,
        })
    return {"sources": sources, "maps": []}


def _apply_defaults(entry: dict, defaults: dict) -> dict:
    out = dict(defaults)
    out.update(entry)
    return out


def parse_config(raw: dict) -> Config:
    sources = {}
    for s in raw.get("sources", []):
        if "id" not in s:
            continue
        sources[s["id"]] = _apply_defaults(s, _SOURCE_DEFAULTS)
    return Config(sources=sources, maps=normalize_maps(raw.get("maps", [])))


def normalize_maps(raw_maps) -> list:
    """Defaults applied, malformed entries dropped. Shared by sensors.json and
    the per-patch map files so there is ONE definition of a valid map.
    `target` is either the original {"sink","kind",...} dict or the pool's
    {"param": "/stage/name"}; both stay valid."""
    maps = []
    for m in raw_maps or []:
        if not isinstance(m, dict):
            continue
        if "id" not in m or "source" not in m or not isinstance(m.get("target"), dict):
            continue
        maps.append(_apply_defaults(m, _MAP_DEFAULTS))
    return maps


def load_config(path: Path = DEFAULT_PATH, descriptors: Optional[dict] = None) -> Config:
    """Load sensors.json, writing a safe-boot default first if it's missing."""
    if not path.exists():
        path.parent.mkdir(parents=True, exist_ok=True)
        default = default_config(descriptors or {})
        path.write_text(json.dumps(default, indent=2) + "\n")
        print(f"[demiurge-io] wrote default config: {path}", file=sys.stderr)
    with open(path) as f:
        raw = json.load(f)
    return parse_config(raw)


class ConfigWatcher:
    """Cheap mtime-polled hot reload. Call maybe_reload() on a timer; it
    only re-parses the file when its mtime has actually changed, and never
    replaces a good config with a broken one."""

    def __init__(self, path: Path = DEFAULT_PATH, descriptors: Optional[dict] = None):
        self.path = path
        self.descriptors = descriptors or {}
        self.config = load_config(path, self.descriptors)
        try:
            self._mtime = os.stat(path).st_mtime
        except OSError:
            self._mtime = 0.0

    def maybe_reload(self) -> bool:
        """Returns True if the config actually changed and was reloaded."""
        try:
            mtime = os.stat(self.path).st_mtime
        except OSError:
            return False
        if mtime == self._mtime:
            return False
        try:
            new_config = load_config(self.path, self.descriptors)
        except (json.JSONDecodeError, OSError) as e:
            print(f"[demiurge-io] config reload failed, keeping previous: {e}",
                  file=sys.stderr, flush=True)
            self._mtime = mtime   # don't retry-spam a broken file every tick
            return False
        self.config = new_config
        self._mtime = mtime
        print(f"[demiurge-io] reloaded {self.path} "
              f"({len(new_config.sources)} sources, {len(new_config.maps)} maps)",
              file=sys.stderr, flush=True)
        return True


# ── per-patch map store ─────────────────────────────────────────────────────

def active_patch(live_conf: Path) -> Optional[str]:
    """The `# set: <name>` header of live.conf (demiurge-patch already writes
    it, so switching patches needs no change there)."""
    try:
        with open(live_conf) as f:
            for line in f:
                s = line.strip()
                if s.lower().startswith("# set:"):
                    return s.split(":", 1)[1].strip() or None
    except OSError:
        pass
    return None


def patch_map_path(patch: str, root: Path = DEFAULT_ROOT) -> Path:
    """Map file for <patch>, folder-first (docs/patch-folders.md):
         patches/<patch>/<patch>.map.json   (patch folder)
         patches/<patch>.map.json           (flat, legacy)
    An existing file wins, folder before flat. If neither exists yet (a save),
    the folder location is used when the patch folder exists, else flat.
    Path separators in a patch name are refused so a header cannot escape
    patches/."""
    if "/" in patch or "\\" in patch or patch in ("", ".", ".."):
        raise ValueError(f"bad patch name {patch!r}")
    base = root / "patches"
    folder_map = base / patch / f"{patch}.map.json"
    flat_map = base / f"{patch}.map.json"
    if folder_map.is_file():
        return folder_map
    if flat_map.is_file():
        return flat_map
    return folder_map if (base / patch).is_dir() else flat_map


def atomic_write_json(path: Path, obj) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(json.dumps(obj, indent=2) + "\n")
    os.replace(tmp, path)       # readers never see a half-write


def load_patch_map(path: Path) -> tuple[list, dict]:
    """-> (normalized maps, baseline values). Raises on unreadable/bad JSON so
    the caller can decide to keep the previous good copy."""
    with open(path) as f:
        doc = json.load(f)
    values = doc.get("values", {})
    values = {k: float(v) for k, v in values.items()
              if isinstance(k, str) and isinstance(v, (int, float))
              and not isinstance(v, bool)} if isinstance(values, dict) else {}
    return normalize_maps(doc.get("maps", [])), values


def save_patch_map(patch: str, maps: list, values: Optional[dict] = None,
                   root: Path = DEFAULT_ROOT) -> Path:
    p = patch_map_path(patch, root)
    atomic_write_json(p, {"version": 1, "patch": patch, "maps": maps,
                          "values": values or {}})
    return p


def merge_maps(global_maps: list, patch_maps: list) -> list:
    """Global (sensors.json) maps first; a patch map with the same id replaces
    it IN PLACE (config order is combine="replace" priority); new ids append."""
    out = list(global_maps)
    index = {m["id"]: i for i, m in enumerate(out)}
    for m in patch_maps:
        if m["id"] in index:
            out[index[m["id"]]] = m
        else:
            index[m["id"]] = len(out)
            out.append(m)
    return out


class PatchMapWatcher:
    """Follows the active patch: stats live.conf and the patch's map file on
    the same cheap mtime poll as ConfigWatcher. No map file for the patch ->
    empty maps, i.e. today's sensors.json-only behaviour is the fallback.

    A bad/half-written file for the SAME patch keeps the previous good maps;
    a patch SWITCH to a missing/bad file clears them (never keep another
    song's mappings). `patch_changed` is True on the reload that follows a
    switch, so the daemon can apply the new patch's baseline `values` once."""

    def __init__(self, root: Path = DEFAULT_ROOT):
        self.root = Path(root)
        self.live_conf = self.root / "live.conf"
        self.patch: Optional[str] = None
        self.maps: list = []
        self.values: dict = {}
        self.patch_changed = False
        self._live_mtime = None
        self._map_mtime = None
        self.maybe_reload(force=True)

    @staticmethod
    def _mtime(p: Path):
        try:
            return os.stat(p).st_mtime
        except OSError:
            return None

    def maybe_reload(self, force: bool = False) -> bool:
        self.patch_changed = False
        live_m = self._mtime(self.live_conf)
        patch = self.patch
        if force or live_m != self._live_mtime:
            self._live_mtime = live_m
            patch = active_patch(self.live_conf)
        map_p = None
        if patch:
            try:
                map_p = patch_map_path(patch, self.root)
            except ValueError:
                patch = None
        map_m = self._mtime(map_p) if map_p else None
        if not force and patch == self.patch and map_m == self._map_mtime:
            return False
        switched = patch != self.patch
        self.patch, self._map_mtime = patch, map_m
        if map_p is None or map_m is None:
            maps, values = [], {}
        else:
            try:
                maps, values = load_patch_map(map_p)
            except (json.JSONDecodeError, OSError, ValueError, TypeError) as e:
                print(f"[demiurge-io] patch map reload failed ({map_p.name}), "
                      f"keeping previous: {e}", file=sys.stderr, flush=True)
                if not switched:
                    return False
                maps, values = [], {}
        self.maps, self.values = maps, values
        self.patch_changed = switched
        print(f"[demiurge-io] patch {patch!r}: {len(maps)} patch maps",
              file=sys.stderr, flush=True)
        return True
