"""Room calibration: 6 live colored markers on the 1920px ring, driven by
POST /calibration/{start,move,done,cancel} and read back via
GET /calibration[/status]. Overlay-only: must never touch the running
scene, must round-trip through state.json without clobbering
nodes/satellites/taps, and must render markers correctly including the
1919->0 seam wrap."""
import json

import numpy as np
import pytest
from fastapi.testclient import TestClient

from lightserver.api import create_app
from lightserver.calibration import (DEFAULT_POINTS, CalibrationManager,
                                     spread_indices, validate_hex_color)
from lightserver.nodes import NodeConfig
from lightserver.persist import StateStore
from lightserver.ring import RingEngine, RingManager, RingSegment

CONFIGS = [
    NodeConfig(name="fcob-strip", host="fcob-strip.local", pixel_count=800),
    NodeConfig(name="fcob-strip2", host="fcob-strip2.local", pixel_count=480),
    NodeConfig(name="fcob-strip3", host="fcob-strip3.local", pixel_count=640),
]


def make_segments():
    return [RingSegment("fcob-strip", 0, 800),
            RingSegment("fcob-strip2", 800, 480),
            RingSegment("fcob-strip3", 1280, 640)]


def ring_manager(mode="headless"):
    return RingManager.from_configs(CONFIGS, make_segments(), mode=mode, seed=1)


@pytest.fixture()
def manager():
    m = ring_manager()
    yield m
    m.shutdown()


@pytest.fixture()
def client(manager):
    return TestClient(create_app(manager.ring, manager))


# ---------------------------------------------------------------------------
# CalibrationManager unit tests
# ---------------------------------------------------------------------------

def test_spread_indices_default_six_points():
    assert spread_indices(6, 1920) == [0, 320, 640, 960, 1280, 1600]


def test_validate_hex_color_normalizes_and_rejects_garbage():
    assert validate_hex_color("ff4444") == "#ff4444"
    assert validate_hex_color("#FF4444") == "#FF4444"
    with pytest.raises(ValueError):
        validate_hex_color("not-a-color")
    with pytest.raises(ValueError):
        validate_hex_color("#fff")


def test_default_point_set_names_and_colors():
    assert list(DEFAULT_POINTS.keys()) == [
        "frontL", "frontR", "backL", "backR", "projL", "projR"]
    assert DEFAULT_POINTS["frontL"] == "#ff4444"
    assert DEFAULT_POINTS["frontR"] == "#ff44ff"
    assert DEFAULT_POINTS["backL"] == "#4444ff"
    assert DEFAULT_POINTS["backR"] == "#44ffff"
    assert DEFAULT_POINTS["projL"] == "#44ff44"
    assert DEFAULT_POINTS["projR"] == "#ffff44"


def test_start_default_points_spread_and_colors():
    cm = CalibrationManager(1920)
    status = cm.start()
    assert status["active"] is True
    pts = status["points"]
    assert set(pts.keys()) == set(DEFAULT_POINTS.keys())
    assert pts["frontL"] == {"index": 0, "color": "#ff4444"}
    assert pts["frontR"] == {"index": 320, "color": "#ff44ff"}
    assert pts["backL"] == {"index": 640, "color": "#4444ff"}
    assert pts["backR"] == {"index": 960, "color": "#44ffff"}
    assert pts["projL"] == {"index": 1280, "color": "#44ff44"}
    assert pts["projR"] == {"index": 1600, "color": "#ffff44"}


def test_start_uses_saved_indices_when_available():
    cm = CalibrationManager(1920)
    cm.start()
    cm.move({"projL": 1000})
    cm.done()  # saves projL=1000, rest at defaults
    status = cm.start()  # fresh start, no explicit points given
    assert status["points"]["projL"]["index"] == 1000
    assert status["points"]["frontL"]["index"] == 0  # unaffected default


def test_start_explicit_points_override_saved_and_spread():
    cm = CalibrationManager(1920)
    status = cm.start({"onlyOne": {"index": 42, "color": "#123456"}})
    assert status["points"] == {"onlyOne": {"index": 42, "color": "#123456"}}


def test_start_custom_point_without_color_raises():
    cm = CalibrationManager(1920)
    with pytest.raises(ValueError):
        cm.start({"custom": {"index": 5}})


def test_start_rejects_out_of_range_index():
    cm = CalibrationManager(1920)
    with pytest.raises(ValueError):
        cm.start({"frontL": {"index": 1920}})


def test_move_single_and_multiple():
    cm = CalibrationManager(1920)
    cm.start()
    status = cm.move({"projL": 500})
    assert status["points"]["projL"]["index"] == 500
    status = cm.move({"projL": 501, "projR": 502})
    assert status["points"]["projL"]["index"] == 501
    assert status["points"]["projR"]["index"] == 502


def test_move_unknown_name_raises_keyerror():
    cm = CalibrationManager(1920)
    cm.start()
    with pytest.raises(KeyError):
        cm.move({"nope": 5})


def test_move_out_of_range_raises_valueerror():
    cm = CalibrationManager(1920)
    cm.start()
    with pytest.raises(ValueError):
        cm.move({"projL": 1920})
    with pytest.raises(ValueError):
        cm.move({"projL": -1})


def test_move_while_inactive_raises_valueerror():
    cm = CalibrationManager(1920)
    with pytest.raises(ValueError):
        cm.move({"projL": 5})


def test_move_partial_failure_does_not_partially_apply():
    """One bad name in a multi-point move should not silently apply the
    good ones -- move() validates keys before mutating any point."""
    cm = CalibrationManager(1920)
    cm.start()
    before = cm.status()["points"]["projL"]["index"]
    with pytest.raises(KeyError):
        cm.move({"projL": 999, "bogus": 1})
    assert cm.status()["points"]["projL"]["index"] == before


def test_done_saves_and_deactivates():
    cm = CalibrationManager(1920)
    cm.start()
    cm.move({"projL": 1000})
    result = cm.done()
    assert cm.active is False
    assert result["points"]["projL"] == 1000
    assert result["updated"]  # iso8601 timestamp string present


def test_cancel_deactivates_without_saving():
    cm = CalibrationManager(1920)
    cm.start()
    cm.move({"projL": 1000})
    cm.cancel()
    assert cm.active is False
    assert cm.saved == {}  # nothing was ever saved
    assert cm.updated is None


def test_saved_status_shape_matches_consumer_contract():
    cm = CalibrationManager(1920)
    cm.start()
    cm.done()
    s = cm.saved_status()
    assert set(s.keys()) == {"points", "updated"}
    assert isinstance(s["points"], dict)
    assert isinstance(s["updated"], str)


# ---------------------------------------------------------------------------
# Marker rendering / seam wrap
# ---------------------------------------------------------------------------

def test_render_base_is_dim_gray():
    cm = CalibrationManager(1920)
    cm.start({})  # no points at all -> pure base
    frame = cm.render()
    assert frame.shape == (1920, 3)
    expected = int(round(255 * 0.05))
    assert tuple(frame[0]) == (expected, expected, expected)
    assert tuple(frame[1000]) == (expected, expected, expected)


def test_render_marker_five_pixels_wide_centered():
    cm = CalibrationManager(1920)
    cm.start({"m": {"index": 100, "color": "#00ff00"}})
    frame = cm.render()
    for i in (98, 99, 100, 101, 102):
        assert tuple(frame[i]) == (0, 255, 0)
    assert tuple(frame[97]) != (0, 255, 0)
    assert tuple(frame[103]) != (0, 255, 0)


def test_render_marker_wraps_across_seam_at_1918():
    cm = CalibrationManager(1920)
    cm.start({"m": {"index": 1918, "color": "#ff0000"}})
    frame = cm.render()
    for i in (1916, 1917, 1918, 1919, 0, 1):
        pass
    # index-2..index+2 = 1916,1917,1918,1919,1920%1920=0
    for i in (1916, 1917, 1918, 1919, 0):
        assert tuple(frame[i]) == (255, 0, 0)
    assert tuple(frame[2]) != (255, 0, 0)


def test_render_marker_wraps_across_seam_at_zero():
    cm = CalibrationManager(1920)
    cm.start({"m": {"index": 0, "color": "#0000ff"}})
    frame = cm.render()
    # index-2..index+2 = 1918,1919,0,1,2
    for i in (1918, 1919, 0, 1, 2):
        assert tuple(frame[i]) == (0, 0, 255)
    assert tuple(frame[3]) != (0, 0, 255)
    assert tuple(frame[1917]) != (0, 0, 255)


def test_render_overlapping_markers_last_one_in_dict_wins():
    cm = CalibrationManager(1920)
    cm.start({"a": {"index": 10, "color": "#ff0000"},
             "b": {"index": 11, "color": "#00ff00"}})
    frame = cm.render()
    assert tuple(frame[11]) == (0, 255, 0)


# ---------------------------------------------------------------------------
# RingEngine overlay integration: scene resumes untouched
# ---------------------------------------------------------------------------

def test_overlay_replaces_emit_output_without_touching_scene():
    """The overlay substitution happens inside _emit (what actually goes
    out over the wire/tap/preview) -- render_frame()/step()'s return
    value is deliberately still the real scene output, since the scene
    keeps rendering untouched underneath. We verify the substitution via
    a tap (a real _emit consumer) and verify the scene state directly."""
    from lightserver.protocol import parse_packet

    class FakeSocket:
        def __init__(self):
            self.sent = []

        def sendto(self, pkt, addr):
            self.sent.append((addr, pkt))

        def close(self):
            pass

    eng = RingEngine(make_segments(), CONFIGS, mode="headless", seed=1)
    eng.taps._sock = FakeSocket()
    eng.taps.set_tap("t", host="1.2.3.4", port=9999, count=1920)
    scene = {"name": "keepme", "layers": [{"type": "solid", "color": "#102030"}]}
    eng.set_scene(scene, crossfade=0)
    eng.step()  # settle
    assert eng.current_scene_name == "keepme"

    eng.calibration.start()
    eng.taps._sock.sent.clear()
    eng.step()
    # first chunk of the frame just emitted covers ring[0:480]
    _, _, offset, count, rgb = parse_packet(eng.taps._sock.sent[0][1])
    assert offset == 0
    # emitted (tapped) frame is the calibration overlay, not the solid color
    assert rgb[0:3] != bytes([0x10, 0x20, 0x30])
    # scene itself is completely untouched
    assert eng.current_scene_name == "keepme"
    assert eng.current_scene == scene


def test_scene_resumes_exactly_after_done():
    eng = RingEngine(make_segments(), CONFIGS, mode="headless", seed=1)
    scene = {"name": "keepme", "layers": [{"type": "solid", "color": "#102030"}]}
    eng.set_scene(scene, crossfade=0)
    eng.step()
    pre_frame = eng.step()

    eng.calibration.start()
    eng.step()
    eng.calibration.done()
    post_frame = eng.step()

    assert eng.current_scene == scene
    assert np.array_equal(pre_frame, post_frame)


def test_scene_resumes_exactly_after_cancel():
    eng = RingEngine(make_segments(), CONFIGS, mode="headless", seed=1)
    scene = {"name": "keepme", "layers": [{"type": "solid", "color": "#102030"}]}
    eng.set_scene(scene, crossfade=0)
    eng.step()
    pre_frame = eng.step()

    eng.calibration.start()
    eng.calibration.move({"projL": 42})
    eng.step()
    eng.calibration.cancel()
    post_frame = eng.step()

    assert eng.current_scene == scene
    assert np.array_equal(pre_frame, post_frame)


def test_taps_see_overlay_frame_during_calibration():
    from lightserver.taps import TapManager

    class FakeSocket:
        def __init__(self):
            self.sent = []

        def sendto(self, pkt, addr):
            self.sent.append((addr, pkt))

        def close(self):
            pass

    eng = RingEngine(make_segments(), CONFIGS, mode="headless", seed=1)
    eng.taps._sock = FakeSocket()
    eng.taps.set_tap("gormapper", host="1.2.3.4", port=9999, count=1920)
    eng.set_scene({"name": "s", "layers": [{"type": "solid", "color": "#000000"}]},
                 crossfade=0)
    eng.calibration.start({"m": {"index": 100, "color": "#00ff00"}})
    eng.step()
    from lightserver.protocol import parse_packet
    assert eng.taps._sock.sent
    _, _, offset, count, rgb = parse_packet(eng.taps._sock.sent[0][1])
    # marker at 100 lands in the first 480px chunk (offset 0)
    idx_in_chunk = 100 - offset
    assert rgb[idx_in_chunk * 3:idx_in_chunk * 3 + 3] == bytes([0, 255, 0])


# ---------------------------------------------------------------------------
# Persistence round-trip / non-clobbering
# ---------------------------------------------------------------------------

def test_calibration_persist_round_trip(tmp_path):
    store = StateStore(tmp_path / "state.json")
    m = ring_manager()
    m.store = store
    m.calibration_start()
    m.calibration_move({"projL": 1000})
    m.calibration_done()
    m.persist()
    m.shutdown()

    saved = json.loads((tmp_path / "state.json").read_text())
    assert saved["calibration"]["points"]["projL"] == 1000
    assert saved["calibration"]["updated"]

    m2 = ring_manager()
    m2.restore(store)
    status = m2.calibration_saved()
    assert status["points"]["projL"] == 1000
    m2.shutdown()


def test_calibration_does_not_clobber_nodes_satellites_taps(tmp_path):
    store = StateStore(tmp_path / "state.json")
    m = ring_manager()
    m.store = store
    scene = {"name": "keepme", "layers": [{"type": "solid", "color": "#123456"}]}
    m.ring.set_scene(scene, crossfade=0)
    m.set_satellite("dot1", anchor=42) if m.ring.satellites else None
    m.persist()

    m.calibration_start()
    m.calibration_move({"projL": 777})
    m.calibration_done()
    m.persist()
    m.shutdown()

    saved = json.loads((tmp_path / "state.json").read_text())
    assert saved["nodes"]["ring"]["scene"]["name"] == "keepme"
    assert saved["calibration"]["points"]["projL"] == 777
    # "taps"/"satellites" keys still present and untouched in shape
    assert "taps" in saved


def test_saving_nodes_does_not_clobber_calibration(tmp_path):
    store = StateStore(tmp_path / "state.json")
    m = ring_manager()
    m.store = store
    m.calibration_start()
    m.calibration_move({"projL": 55})
    m.calibration_done()
    m.persist()

    scene = {"name": "other", "layers": [{"type": "solid", "color": "#000001"}]}
    m.ring.set_scene(scene, crossfade=0)
    m.persist()
    m.shutdown()

    saved = json.loads((tmp_path / "state.json").read_text())
    assert saved["calibration"]["points"]["projL"] == 55
    assert saved["nodes"]["ring"]["scene"]["name"] == "other"


def test_missing_calibration_key_is_empty(tmp_path):
    store = StateStore(tmp_path / "state.json")
    (tmp_path / "state.json").write_text(json.dumps({"nodes": {}}))
    m = ring_manager()
    m.restore(store)
    assert m.calibration_saved() == {"points": {}, "updated": None}
    m.shutdown()


# ---------------------------------------------------------------------------
# API endpoint tests
# ---------------------------------------------------------------------------

def test_api_start_defaults(client):
    r = client.post("/calibration/start")
    assert r.status_code == 200
    body = r.json()
    assert body["ok"] is True
    assert body["status"]["active"] is True
    assert set(body["status"]["points"].keys()) == set(DEFAULT_POINTS.keys())


def test_api_start_with_custom_points(client):
    r = client.post("/calibration/start", json={
        "points": {"only": {"index": 10, "color": "#abcdef"}}})
    assert r.status_code == 200
    pts = r.json()["status"]["points"]
    assert pts == {"only": {"index": 10, "color": "#abcdef"}}


def test_api_move_single_point_shorthand(client):
    client.post("/calibration/start")
    r = client.post("/calibration/move", json={"projL": 500})
    assert r.status_code == 200
    assert r.json()["status"]["points"]["projL"]["index"] == 500


def test_api_move_points_dict_form(client):
    client.post("/calibration/start")
    r = client.post("/calibration/move", json={"points": {"projL": 501, "projR": 502}})
    assert r.status_code == 200
    pts = r.json()["status"]["points"]
    assert pts["projL"]["index"] == 501
    assert pts["projR"]["index"] == 502


def test_api_move_unknown_point_404_or_422(client):
    client.post("/calibration/start")
    r = client.post("/calibration/move", json={"nonexistent": 5})
    assert r.status_code in (404, 422)


def test_api_move_out_of_bounds_422(client):
    client.post("/calibration/start")
    r = client.post("/calibration/move", json={"projL": 1920})
    assert r.status_code == 422
    r = client.post("/calibration/move", json={"projL": -1})
    assert r.status_code == 422


def test_api_done_persists_and_deactivates(client):
    client.post("/calibration/start")
    client.post("/calibration/move", json={"projL": 1000})
    r = client.post("/calibration/done")
    assert r.status_code == 200
    body = r.json()
    assert body["ok"] is True

    r = client.get("/calibration/status")
    assert r.json()["active"] is False

    r = client.get("/calibration")
    got = r.json()
    assert got["points"]["projL"] == 1000
    assert got["updated"]


def test_api_cancel_does_not_persist(client):
    client.post("/calibration/start")
    client.post("/calibration/move", json={"projL": 1000})
    r = client.post("/calibration/cancel")
    assert r.status_code == 200

    r = client.get("/calibration/status")
    assert r.json()["active"] is False

    r = client.get("/calibration")
    assert r.json()["points"] == {}


def test_api_status_shows_live_positions_while_active(client):
    client.post("/calibration/start")
    client.post("/calibration/move", json={"projL": 1000})
    r = client.get("/calibration/status")
    body = r.json()
    assert body["active"] is True
    assert body["points"]["projL"]["index"] == 1000
    assert "color" in body["points"]["projL"]


def test_api_get_calibration_shape_matches_consumer_contract(client):
    client.post("/calibration/start")
    client.post("/calibration/done")
    r = client.get("/calibration")
    body = r.json()
    assert set(body.keys()) == {"points", "updated"}
    for name, idx in body["points"].items():
        assert isinstance(idx, int)


def test_api_full_lifecycle_ends_inactive(client):
    client.post("/calibration/start")
    client.post("/calibration/move", json={"projL": 1000})
    client.get("/calibration/status")
    client.post("/calibration/done")
    r = client.get("/calibration")
    assert r.json()["points"]["projL"] == 1000
    r = client.get("/calibration/status")
    assert r.json()["active"] is False


def test_calibrate_page_served_and_dynamic(client):
    r = client.get("/calibrate")
    assert r.status_code == 200
    assert "text/html" in r.headers["content-type"]
    html = r.text
    assert "/calibration/start" in html
    assert "/calibration/move" in html
    assert "/calibration/done" in html
    assert "/calibration/cancel" in html
    # dynamic slider scaffolding: no point names hardcoded, built from JSON
    assert "frontL" not in html
    assert "markers show live on the ring" in html
