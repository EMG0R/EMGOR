"""Window projection: device space -> coordinate space (spec §C).

A window is axis-aligned: ``center[8]``, ``extent[8]`` (half-widths, >= 0),
and an *assignment* mapping controller channels to coordinate indices.
Windows live only in the controller; they never travel on the wire.
"""
from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Sequence

from .axes import K, clamp01
from .types import CatalogPoint, Vec, vec


@dataclass
class Channel:
    name: str
    dims: int
    kind: str  # "discrete" | "continuous"
    size: tuple[int, ...] | None = None


@dataclass
class InputManifest:
    controller_id: str
    channels: tuple[Channel, ...]

    @staticmethod
    def load(path) -> "InputManifest":
        data = json.loads(Path(path).read_text())
        channels = tuple(
            Channel(
                name=c["name"],
                dims=c["dims"],
                kind=c["kind"],
                size=tuple(c["size"]) if c.get("size") is not None else None,
            )
            for c in data["channels"]
        )
        return InputManifest(controller_id=data["controller_id"], channels=channels)


@dataclass
class Window:
    center: Vec
    extent: Vec
    assign: dict[str, tuple[int, ...]]  # channel name -> coord idx per dim; -1 = unassigned


def _cell_center(d: float, size: int) -> float:
    """Discrete channel device coordinate for cell index derived from ``d``.

    Callers already pass ``d = (i + 0.5) / size`` for discrete channels
    (per spec §C); this helper is kept for symmetry / potential reuse.
    """
    return d


def project(w: Window, channel: Channel, d: Sequence[float], velocity: float | None = None) -> Vec:
    """Project a device coordinate ``d`` (len == channel.dims) into K-space."""
    mu = list(w.center)
    idxs = w.assign.get(channel.name, tuple([-1] * channel.dims))
    for dim, a in enumerate(idxs):
        if a is None or a < 0:
            continue
        mu[a] = clamp01(w.center[a] + (2.0 * d[dim] - 1.0) * w.extent[a])

    loud_assigned = any(a == 1 for a in idxs if a is not None)
    if velocity is not None and not loud_assigned:
        mu[1] = clamp01(w.center[1] - w.extent[1] + 2.0 * w.extent[1] * velocity)

    return vec(*mu)


def modulate(mu: Vec, idx: int, delta: float, w: Window) -> Vec:
    """Apply a controller-side bipolar modulator delta to axis ``idx``."""
    delta = max(-1.0, min(1.0, float(delta)))
    out = list(mu)
    out[idx] = clamp01(out[idx] + delta * w.extent[idx])
    return vec(*out)


def _inf_dist(a: Vec, b: Vec) -> float:
    return max(abs(x - y) for x, y in zip(a, b))


def cell_table(
    w: Window,
    channel: Channel,
    catalog: Sequence[CatalogPoint] = (),
    snap_radius: float = 0.06,
) -> dict[tuple[int, ...], tuple[Vec, "CatalogPoint | None"]]:
    """Precompute, for a discrete channel, the (mu, snapped-point-or-None) per cell.

    Each cell's projected ``mu`` is replaced-for-lookup by the nearest
    catalog point when ``dist_inf(mu, point.mu) <= snap_radius``, so
    ``/hit`` becomes a lookup rather than a search.
    """
    if channel.kind != "discrete" or channel.size is None:
        raise ValueError("cell_table requires a discrete channel with a size")

    table: dict[tuple[int, ...], tuple[Vec, "CatalogPoint | None"]] = {}
    size = channel.size

    def _iter_cells(dims_left, prefix):
        if not dims_left:
            yield tuple(prefix)
            return
        for i in range(dims_left[0]):
            yield from _iter_cells(dims_left[1:], prefix + [i])

    for cell in _iter_cells(list(size), []):
        d = tuple((i + 0.5) / n for i, n in zip(cell, size))
        mu = project(w, channel, d)
        best = None
        best_dist = None
        for p in catalog:
            dist = _inf_dist(mu, p.mu)
            if dist <= snap_radius and (best_dist is None or dist < best_dist):
                best = p
                best_dist = dist
        table[cell] = (mu, best)

    return table
