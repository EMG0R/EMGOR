"""Room calibration: 6 live colored markers on the 1920px LED ring, moved
by sliders in a phone-friendly web UI (`GET /calibrate`), persisted as
named ring positions for GORMAPPER (a projection-mapping app) to consume.

Calibration is a server-side OVERLAY on top of RingEngine._emit: while
active it replaces the frame that actually gets sent to strips/satellites/
taps/preview with a dim base + bright markers, WITHOUT touching the
running scene state (`RingEngine._scene`/`_old_scene`) at all -- the scene
keeps rendering underneath every frame (so its animation/crossfade clocks
keep advancing), it's just not what goes out over the wire while
calibration is active. `done()`/`cancel()` simply flip `active` back to
False and the very next frame is the untouched scene output again.

Default point set (name -> marker color): frontL #ff4444 (red),
frontR #ff44ff (magenta), backL #4444ff (blue), backR #44ffff (cyan),
projL #44ff44 (green), projR #ffff44 (yellow). Default indices spread
evenly around the ring: `ring_len // n_points * i` for i in
0..n_points-1 (1920px / 6 points -> 0, 320, 640, 960, 1280, 1600).

Marker rendering: each point draws a 5px bright dot (index-2..index+2,
mod ring_len, so it wraps correctly across the 1919->0 seam) in its
color, over a ~5% neutral gray base (dim enough to see the room, bright
enough that markers stand out).

Persistence: only the SAVED (POST /calibration/done'd) named indices +
an updated timestamp are persisted, via StateStore's own top-level
"calibration" key (see persist.py) -- never the live in-progress
positions. A point moved live but not `done()`-ed is lost if the process
restarts (same "in-progress edits aren't durable" contract as everything
else that's live-only until explicitly saved).

Consumer contract (GORMAPPER, see docs/CALIBRATION_DESIGN.md): GET
/calibration returns {"points": {name: index, ...}, "updated": iso8601}.
`projL > projR` means the projection window crosses the ring seam (wraps
through index 0).
"""
from __future__ import annotations

import re
from dataclasses import dataclass
from datetime import datetime, timezone

import numpy as np

DEFAULT_POINTS: dict[str, str] = {
    "frontL": "#ff4444",
    "frontR": "#ff44ff",
    "backL": "#4444ff",
    "backR": "#44ffff",
    "projL": "#44ff44",
    "projR": "#ffff44",
}

BASE_GRAY_FRAC = 0.05   # ~5% neutral gray base while calibration is active
MARKER_HALF_WIDTH = 2   # index-2..index+2 -> 5px marker

_HEX_RE = re.compile(r"^#?[0-9a-fA-F]{6}$")


def validate_hex_color(hex_str) -> str:
    """'#rrggbb' or 'rrggbb' -> normalized '#rrggbb'. Raises ValueError."""
    if not isinstance(hex_str, str) or not _HEX_RE.match(hex_str.strip()):
        raise ValueError(f"bad hex color: {hex_str!r}")
    s = hex_str.strip().lstrip("#")
    return "#" + s


def _hex_to_uint8(hex_str: str) -> np.ndarray:
    s = hex_str.lstrip("#")
    return np.array([int(s[i:i + 2], 16) for i in (0, 2, 4)], dtype=np.uint8)


def spread_indices(n: int, ring_len: int) -> list[int]:
    """n points evenly spread around a ring_len ring, starting at 0."""
    if n <= 0:
        return []
    step = ring_len // n
    return [i * step for i in range(n)]


@dataclass
class CalibrationPoint:
    name: str
    index: int
    color: str  # '#rrggbb'


class CalibrationManager:
    """Owns calibration mode for one RingEngine: live overlay state (while
    active) + the persisted named points (survives restarts). `render()`
    is called from RingEngine._emit every frame while active."""

    def __init__(self, ring_len: int):
        self.ring_len = ring_len
        self.active = False
        self.points: dict[str, CalibrationPoint] = {}
        self.saved: dict[str, int] = {}     # name -> index, persisted
        self.updated: str | None = None     # iso8601 of the last done()

    # -- lifecycle ----------------------------------------------------------
    def start(self, points: dict[str, dict] | None = None) -> dict:
        """Enter calibration mode.

        `points` (optional): {name: {"index": int?, "color": "#hex"?}}.
        Omitted/empty -> the default 6-point set. For each point: an
        explicit "index" wins; else the previously SAVED index for that
        name (if any); else an evenly spread default. An explicit "color"
        wins; else the default color for a known point name; a custom
        point name with no default and no given color is an error.
        Raises ValueError on a bad color or out-of-range index.
        """
        names = (list(points.keys()) if points is not None
                else list(DEFAULT_POINTS.keys()))
        spread = spread_indices(len(names), self.ring_len)
        new_points: dict[str, CalibrationPoint] = {}
        for i, name in enumerate(names):
            spec = (points or {}).get(name) or {}
            if not isinstance(spec, dict):
                raise ValueError(f"point '{name}': spec must be an object")
            color = spec.get("color") or DEFAULT_POINTS.get(name)
            if color is None:
                raise ValueError(f"point '{name}': color is required "
                                 f"(no default for this name)")
            color = validate_hex_color(color)

            idx = spec.get("index")
            if idx is None:
                idx = self.saved.get(name, spread[i])
            try:
                idx = int(idx)
            except (TypeError, ValueError):
                raise ValueError(f"point '{name}': bad index {idx!r}")
            if not (0 <= idx < self.ring_len):
                raise ValueError(f"point '{name}': index {idx} out of range "
                                 f"0..{self.ring_len - 1}")
            new_points[name] = CalibrationPoint(name=name, index=idx, color=color)
        self.points = new_points
        self.active = True
        return self.status()

    def move(self, updates: dict[str, int]) -> dict:
        """Move one or more live points. Raises KeyError for an unknown
        point name, ValueError for an out-of-range index or if calibration
        mode isn't active."""
        if not self.active:
            raise ValueError("calibration mode is not active")
        for name in updates:
            if name not in self.points:
                raise KeyError(name)
        validated: dict[str, int] = {}
        for name, idx in updates.items():
            try:
                idx_i = int(idx)
            except (TypeError, ValueError):
                raise ValueError(f"point '{name}': bad index {idx!r}")
            if not (0 <= idx_i < self.ring_len):
                raise ValueError(f"point '{name}': index {idx_i} out of "
                                 f"range 0..{self.ring_len - 1}")
            validated[name] = idx_i
        for name, idx_i in validated.items():
            self.points[name].index = idx_i
        return self.status()

    def done(self) -> dict:
        """Persist all current live point positions, exit calibration mode.
        Normal scene rendering resumes on the very next frame."""
        self.saved = {name: p.index for name, p in self.points.items()}
        self.updated = datetime.now(timezone.utc).isoformat()
        self.active = False
        return self.saved_status()

    def cancel(self) -> None:
        """Exit calibration mode WITHOUT saving; nothing written."""
        self.active = False

    # -- status --------------------------------------------------------------
    def status(self) -> dict:
        """Live (in-progress, possibly-unsaved) status."""
        return {
            "active": self.active,
            "points": {name: {"index": p.index, "color": p.color}
                      for name, p in self.points.items()},
        }

    def saved_status(self) -> dict:
        """The SAVED (persisted) named points -- GET /calibration contract."""
        return {"points": dict(self.saved), "updated": self.updated}

    # -- persistence ----------------------------------------------------------
    def for_persist(self) -> dict:
        return {"points": dict(self.saved), "updated": self.updated}

    def restore(self, saved: dict) -> None:
        """saved: {"points": {name: index, ...}, "updated": iso8601|None}.
        Missing/corrupt/odd-shaped -> leaves saved/updated at defaults
        (empty/None) -- never blocks startup."""
        if not isinstance(saved, dict):
            return
        points = saved.get("points")
        if isinstance(points, dict):
            out: dict[str, int] = {}
            for name, idx in points.items():
                try:
                    idx_i = int(idx)
                except (TypeError, ValueError):
                    continue
                if 0 <= idx_i < self.ring_len:
                    out[str(name)] = idx_i
            self.saved = out
        updated = saved.get("updated")
        if isinstance(updated, str):
            self.updated = updated

    # -- rendering ------------------------------------------------------------
    def render(self) -> np.ndarray:
        """Build the overlay uint8 (ring_len,3) frame: dim base + bright
        5px markers, wrapping correctly across the 1919->0 seam. Only
        called (from RingEngine._emit) while active."""
        base = int(round(255 * BASE_GRAY_FRAC))
        frame = np.full((self.ring_len, 3), base, dtype=np.uint8)
        for p in self.points.values():
            rgb = _hex_to_uint8(p.color)
            for d in range(-MARKER_HALF_WIDTH, MARKER_HALF_WIDTH + 1):
                frame[(p.index + d) % self.ring_len] = rgb
        return frame
