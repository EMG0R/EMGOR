"""ManifestStore: merges renderer manifests, answers nearest/within-sigma queries.

Brute force over numpy arrays rebuilt on ``update`` — catalog sizes are at
most a few hundred points, so this stays well under 50 us per query.
"""
from __future__ import annotations

from typing import Optional, Sequence

import numpy as np

from .types import CatalogPoint, Manifest, Vec


class ManifestStore:
    def __init__(self) -> None:
        self._manifests: dict[str, Manifest] = {}

    def update(self, m: Manifest) -> None:
        """Replace any previously stored manifest for ``m.renderer_id``."""
        self._manifests[m.renderer_id] = m

    def catalog(self) -> list[CatalogPoint]:
        points: list[CatalogPoint] = []
        for m in self._manifests.values():
            if m.kind == "catalog":
                points.extend(m.points)
        return points

    def _catalog_matrix(self, points: Optional[Sequence[CatalogPoint]]):
        pts = list(points) if points is not None else self.catalog()
        if not pts:
            return pts, None
        return pts, np.array([p.mu for p in pts], dtype=float)

    def nearest(self, mu: Vec, points: Optional[Sequence[CatalogPoint]] = None) -> CatalogPoint | None:
        pts, mat = self._catalog_matrix(points)
        if mat is None:
            return None
        d = np.linalg.norm(mat - np.array(mu, dtype=float), axis=1)
        return pts[int(np.argmin(d))]

    def within_sigma(
        self, mu: Vec, sigma: Vec, points: Optional[Sequence[CatalogPoint]] = None
    ) -> list[CatalogPoint]:
        pts, mat = self._catalog_matrix(points)
        if mat is None:
            return []
        diff = np.abs(mat - np.array(mu, dtype=float))
        ok = np.all(diff <= np.array(sigma, dtype=float), axis=1)
        return [p for p, keep in zip(pts, ok) if keep]

    def continuous_covers(self, mu: Vec) -> list[str]:
        out = []
        mu_arr = np.array(mu, dtype=float)
        for m in self._manifests.values():
            if m.kind != "continuous" or m.lo is None or m.hi is None:
                continue
            lo = np.array(m.lo, dtype=float)
            hi = np.array(m.hi, dtype=float)
            if bool(np.all(mu_arr >= lo) and np.all(mu_arr <= hi)):
                out.append(m.renderer_id)
        return out
