"""Per-node scene persistence: applied patterns stick across restarts.

State file `state.json` lives at the repo root (next to `nodes.json`):

    {"version": 1, "nodes": {
      "fcob-strip": {"scene": {...full scene JSON...}, "streaming": true}
    }, "satellites": {
      "dot1": {"anchor": 0, "mode": "mirror", "color": null, "boost": "off",
               "vivid_saturation": null, "vivid_lift": null}
    }}

Whenever a scene/prompt/pixels/stop/resume is applied through the API the
manager snapshots every node's current scene + streaming intent here
(atomic tmpfile + os.replace). On startup the persisted state is restored
per node; the boot default (e.g. `--scene scenes/fireflies.json`) only
applies to nodes with no persisted entry. A missing or corrupt state file
falls back cleanly to defaults — it never blocks startup.

The "satellites" key (anchor/mode/color per dot, see satellites.py) lives
in the same file, written/read independently of "nodes" — save()/load()
touch only the "nodes" key and save_satellites()/load_satellites() touch
only "satellites"; each preserves whatever the other key currently holds
so scene saves and satellite saves never clobber each other.

The "taps" key (canvas-tap CONFIG only — port/fps_divisor/ttl/offset/
count, keyed by tap name, see taps.py) lives in the same file the same
way: save_taps()/load_taps() touch only "taps", preserving "nodes" and
"satellites". Deliberately NOT persisted: a tap's `enabled` state and
`host` — every tap starts disabled after a restart until a live keepalive
re-asserts presence (and re-derives host from that request's source IP).

The "calibration" key ({"points": {name: index, ...}, "updated":
iso8601|None} -- the SAVED room-calibration markers, see calibration.py
and docs/CALIBRATION_DESIGN.md) lives in the same file the same way:
save_calibration()/load_calibration() touch only "calibration",
preserving "nodes", "satellites" and "taps". Only points written by
POST /calibration/done are persisted -- live in-progress moves are not.

The "sync" key ({"output_delay_ms": int} -- the output-delay AV sync
setting, see sync.py and docs/SYNC_DESIGN.md) lives in the same file the
same way: save_sync()/load_sync() touch only "sync", preserving "nodes",
"satellites", "taps" and "calibration".
"""
from __future__ import annotations

import json
import os
import tempfile
from pathlib import Path

STATE_VERSION = 1
DEFAULT_STATE_PATH = Path(__file__).resolve().parent.parent / "state.json"


class StateStore:
    """Tiny atomic JSON store for per-node {scene, streaming} state."""

    def __init__(self, path: str | Path | None = None) -> None:
        self.path = Path(path) if path else DEFAULT_STATE_PATH
        self.last_error: str | None = None

    def _read_raw(self) -> dict:
        try:
            raw = json.loads(self.path.read_text())
        except (OSError, json.JSONDecodeError):
            return {}
        return raw if isinstance(raw, dict) else {}

    def _write_raw(self, payload: dict) -> bool:
        """Atomically write `payload` as the whole state file. Best-effort:
        returns False and records last_error on failure instead of raising
        into the API path."""
        tmp_path = None
        try:
            fd, tmp_path = tempfile.mkstemp(dir=str(self.path.parent),
                                            prefix=".state-", suffix=".tmp")
            with os.fdopen(fd, "w") as f:
                json.dump(payload, f, indent=2)
                f.write("\n")
                f.flush()
                os.fsync(f.fileno())
            os.replace(tmp_path, self.path)
            self.last_error = None
            return True
        except OSError as e:
            self.last_error = str(e)
            if tmp_path is not None:
                try:
                    os.unlink(tmp_path)
                except OSError:
                    pass
            return False

    def load(self) -> dict[str, dict]:
        """Return {node_name: {"scene": dict|None, "streaming": bool}}.
        Missing/corrupt/odd-shaped file -> {} (defaults apply)."""
        nodes = self._read_raw().get("nodes")
        if not isinstance(nodes, dict):
            return {}
        out: dict[str, dict] = {}
        for name, d in nodes.items():
            if not isinstance(d, dict):
                continue
            scene = d.get("scene")
            if scene is not None and not isinstance(scene, dict):
                continue
            out[str(name)] = {"scene": scene,
                              "streaming": bool(d.get("streaming", True))}
        return out

    def save(self, node_states: dict[str, dict]) -> bool:
        """Atomically write "nodes", preserving whatever "satellites" (or
        any other top-level key) is already on disk."""
        raw = self._read_raw()
        raw["version"] = STATE_VERSION
        raw["nodes"] = node_states
        return self._write_raw(raw)

    def load_satellites(self) -> dict[str, dict]:
        """Return {sat_name: {"anchor": int, "mode": str, "color": str|None,
        "boost": str|float, "vivid_saturation": float|None,
        "vivid_lift": float|None}}.
        Missing/corrupt/odd-shaped file -> {} (config-file defaults
        apply). `boost`, `vivid_saturation`, and `vivid_lift` are passed
        through raw (not validated here) — the caller (RingManager.restore)
        normalizes/validates them and falls back to the config-file
        default on a bad value, same tolerance as anchor/mode. A
        pre-existing state.json written before `vivid_lift` existed simply
        lacks the key, so `d.get("vivid_lift")` -> None here, which
        validate_vivid_lift() resolves cleanly to VIVID_LIFT_DEFAULT —
        no error, no migration needed."""
        sats = self._read_raw().get("satellites")
        if not isinstance(sats, dict):
            return {}
        out: dict[str, dict] = {}
        for name, d in sats.items():
            if not isinstance(d, dict):
                continue
            try:
                anchor = int(d.get("anchor", 0))
            except (TypeError, ValueError):
                continue
            out[str(name)] = {"anchor": anchor,
                              "mode": str(d.get("mode", "mirror")),
                              "color": d.get("color"),
                              "boost": d.get("boost", "off"),
                              "vivid_saturation": d.get("vivid_saturation"),
                              "vivid_lift": d.get("vivid_lift")}
        return out

    def save_satellites(self, sat_states: dict[str, dict]) -> bool:
        """Atomically write "satellites", preserving "nodes"."""
        raw = self._read_raw()
        raw["version"] = STATE_VERSION
        raw["satellites"] = sat_states
        return self._write_raw(raw)

    def load_taps(self) -> dict[str, dict]:
        """Return {tap_name: {"port", "fps_divisor", "ttl", "offset",
        "count"}} (raw, unvalidated — TapManager.restore_config() validates
        and skips bad entries). Missing/corrupt/odd-shaped file -> {}."""
        taps = self._read_raw().get("taps")
        if not isinstance(taps, dict):
            return {}
        out: dict[str, dict] = {}
        for name, d in taps.items():
            if isinstance(d, dict):
                out[str(name)] = d
        return out

    def save_taps(self, tap_states: dict[str, dict]) -> bool:
        """Atomically write "taps", preserving "nodes" and "satellites"."""
        raw = self._read_raw()
        raw["version"] = STATE_VERSION
        raw["taps"] = tap_states
        return self._write_raw(raw)

    def load_calibration(self) -> dict:
        """Return {"points": {name: index, ...}, "updated": iso8601|None},
        the SAVED (POST /calibration/done'd) room-calibration markers (see
        calibration.py). Missing/corrupt/odd-shaped file -> {"points": {},
        "updated": None} -- never blocks startup. Raw pass-through (not
        validated here); CalibrationManager.restore() normalizes/validates
        and drops bad entries the same tolerant way satellites/taps do."""
        cal = self._read_raw().get("calibration")
        if not isinstance(cal, dict):
            return {"points": {}, "updated": None}
        points = cal.get("points")
        return {"points": points if isinstance(points, dict) else {},
                "updated": cal.get("updated")}

    def save_calibration(self, calibration_state: dict) -> bool:
        """Atomically write "calibration", preserving "nodes", "satellites"
        and "taps"."""
        raw = self._read_raw()
        raw["version"] = STATE_VERSION
        raw["calibration"] = calibration_state
        return self._write_raw(raw)

    def load_sync(self) -> dict:
        """Return {"output_delay_ms": int}, the output-delay AV sync
        setting (see sync.py). Missing/corrupt/odd-shaped file ->
        {"output_delay_ms": 0} -- never blocks startup. Raw pass-through
        (not validated here); SyncManager.restore() validates and falls
        back to the default (0) on a bad value."""
        sync = self._read_raw().get("sync")
        if not isinstance(sync, dict):
            return {"output_delay_ms": 0}
        return {"output_delay_ms": sync.get("output_delay_ms", 0)}

    def save_sync(self, sync_state: dict) -> bool:
        """Atomically write "sync", preserving "nodes", "satellites",
        "taps" and "calibration"."""
        raw = self._read_raw()
        raw["version"] = STATE_VERSION
        raw["sync"] = sync_state
        return self._write_raw(raw)
