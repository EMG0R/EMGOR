import pytest

from septagraph.manifest import ManifestStore
from septagraph.types import ZERO, CatalogPoint, Manifest, vec
from septagraph.axes import K


def _pt(pid, mu, instrument="x"):
    return CatalogPoint(pid, mu, ZERO, instrument, 10.0, 0.0, 1.0)


CAT_A = Manifest(
    "lab", "catalog", "Machine Lab", None, None,
    (
        _pt(0, vec(*[0.0] * K), "breakBot"),
        _pt(1, vec(*[0.5] * K), "tammy"),
        _pt(2, vec(*[1.0] * K), "gana_pati"),
    ),
)

CONT_A = Manifest("chuck", "continuous", "sept-render-chuck", ZERO, vec(*[1.0] * K), ())
CONT_B = Manifest("chuck2", "continuous", "other", vec(*[0.5] * K), vec(*[1.0] * K), ())


def test_update_replaces_by_renderer_id():
    s = ManifestStore()
    s.update(CAT_A)
    assert len(s.catalog()) == 3
    replacement = Manifest("lab", "catalog", "Machine Lab", None, None, (_pt(0, ZERO, "only"),))
    s.update(replacement)
    assert len(s.catalog()) == 1
    assert s.catalog()[0].instrument == "only"


def test_catalog_spans_all_catalog_renderers():
    s = ManifestStore()
    s.update(CAT_A)
    s.update(Manifest("lab2", "catalog", "Lab 2", None, None, (_pt(0, ZERO, "other"),)))
    assert len(s.catalog()) == 4


def test_nearest_picks_closest_of_three():
    s = ManifestStore()
    s.update(CAT_A)
    nearest = s.nearest(vec(*[0.45] * K))
    assert nearest.instrument == "tammy"
    nearest = s.nearest(vec(*[0.9] * K))
    assert nearest.instrument == "gana_pati"


def test_nearest_empty_returns_none():
    s = ManifestStore()
    assert s.nearest(vec(*[0.5] * K)) is None


def test_within_sigma_zero_exact_matches_only():
    s = ManifestStore()
    s.update(CAT_A)
    hits = s.within_sigma(vec(*[0.5] * K), ZERO)
    assert [p.instrument for p in hits] == ["tammy"]
    assert s.within_sigma(vec(*[0.45] * K), ZERO) == []


def test_within_sigma_wide_returns_multiple():
    s = ManifestStore()
    s.update(CAT_A)
    hits = s.within_sigma(vec(*[0.45] * K), vec(*[0.1] * K))
    names = sorted(p.instrument for p in hits)
    assert names == ["tammy"]  # only tammy at 0.5 is within 0.1 of 0.45 on all 8 axes
    hits2 = s.within_sigma(vec(*[0.25] * K), vec(*[0.3] * K))
    names2 = sorted(p.instrument for p in hits2)
    assert names2 == ["breakBot", "tammy"]


def test_continuous_covers():
    s = ManifestStore()
    s.update(CONT_A)
    s.update(CONT_B)
    covers_low = sorted(s.continuous_covers(vec(*[0.1] * K)))
    assert covers_low == ["chuck"]
    covers_high = sorted(s.continuous_covers(vec(*[0.75] * K)))
    assert covers_high == ["chuck", "chuck2"]
    covers_all = sorted(s.continuous_covers(vec(*[0.9] * K)))
    assert covers_all == ["chuck", "chuck2"]
