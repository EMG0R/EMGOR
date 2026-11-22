import json

import pytest

from septagraph.types import HALF, ZERO, CatalogPoint, vec
from septagraph.window import Channel, InputManifest, Window, cell_table, modulate, project
from septagraph.axes import K

GRID = Channel("grid", 2, "discrete", (8, 8))
RIBBON = Channel("ribbon", 1, "continuous", None)
W = Window(center=HALF, extent=vec(0.4, 0.3, 0, 0, 0.4, 0, 0, 0, 0), assign={"grid": (0, 4)})


def test_project_center_and_edges():
    assert project(W, GRID, (0.5, 0.5)) == pytest.approx(HALF)
    mu = project(W, GRID, (0.0, 1.0))
    assert mu[0] == pytest.approx(0.1) and mu[4] == pytest.approx(0.9)


def test_project_unassigned_indices_take_center():
    mu = project(W, GRID, (0.0, 1.0))
    for i in (1, 2, 3, 5, 6, 7):
        assert mu[i] == pytest.approx(0.5)


def test_project_clamps():
    w = Window(center=vec(0.9, *([0.5] * (K - 1))), extent=vec(0.5, *([0.0] * (K - 1))), assign={"grid": (0, -1)})
    mu = project(w, GRID, (1.0, 0.5))
    assert mu[0] == pytest.approx(1.0)  # would be 1.4 unclamped


def test_velocity_to_loud():
    assert project(W, GRID, (0.5, 0.5), velocity=1.0)[1] == pytest.approx(0.8)
    assert project(W, GRID, (0.5, 0.5), velocity=0.0)[1] == pytest.approx(0.2)


def test_velocity_ignored_when_loud_assigned():
    w = Window(center=HALF, extent=vec(*[0.3] * K), assign={"ribbon": (1,)})
    mu = project(w, RIBBON, (1.0,), velocity=0.0)
    # loud comes from the assigned channel projection, not velocity
    assert mu[1] == pytest.approx(0.8)


def test_discrete_cell_is_center_of_cell():
    # cell (3, 5) of an 8x8 grid -> d = (3.5/8, 5.5/8)
    d = ((3 + 0.5) / 8, (5 + 0.5) / 8)
    mu = project(W, GRID, d)
    assert mu[0] == pytest.approx(0.5 + (2 * d[0] - 1) * 0.4)
    assert mu[4] == pytest.approx(0.5 + (2 * d[1] - 1) * 0.4)


def test_modulate_scales_by_extent_and_clamps():
    mu = modulate(HALF, 0, 1.0, W)
    assert mu[0] == pytest.approx(0.9)  # 0.5 + 1.0*0.4
    mu2 = modulate(vec(*[0.95] * K), 0, 1.0, W)
    assert mu2[0] == pytest.approx(1.0)  # clamped
    # other indices untouched
    assert mu[1] == pytest.approx(0.5)


def test_cell_table_snaps():
    p = CatalogPoint(0, project(W, GRID, ((3 + 0.5) / 8, (5 + 0.5) / 8)), ZERO, "tammy", 8, 0, 1)
    tab = cell_table(W, GRID, [p])
    assert len(tab) == 64
    assert tab[(3, 5)][1] is p
    assert tab[(0, 0)][1] is None
    # unsnapped cells still carry the plain projected mu
    assert tab[(0, 0)][0] == pytest.approx(project(W, GRID, (0.5 / 8, 0.5 / 8)))


def test_cell_table_respects_snap_radius():
    p = CatalogPoint(0, project(W, GRID, ((3 + 0.5) / 8, (5 + 0.5) / 8)), ZERO, "tammy", 8, 0, 1)
    tab_tight = cell_table(W, GRID, [p], snap_radius=0.0001)
    # only the exact cell (which has distance 0) should still snap
    assert tab_tight[(3, 5)][1] is p
    assert tab_tight[(3, 4)][1] is None


def test_input_manifest_load(tmp_path):
    data = {
        "controller_id": "web-trackpad",
        "channels": [
            {"name": "pad", "dims": 2, "kind": "continuous", "size": None},
            {"name": "click", "dims": 1, "kind": "discrete", "size": [1]},
        ],
    }
    p = tmp_path / "trackpad.json"
    p.write_text(json.dumps(data))
    im = InputManifest.load(p)
    assert im.controller_id == "web-trackpad"
    assert len(im.channels) == 2
    assert im.channels[0].name == "pad" and im.channels[0].kind == "continuous"
    assert im.channels[1].size == (1,)
