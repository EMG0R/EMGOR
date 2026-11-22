"""Satellite 'dot' nodes: config load/validate, mirror sampling (incl. ring
seam wrap), independent mode override, RingEngine dispatch, ring-only API,
persistence."""
import colorsys
import json
from pathlib import Path

import numpy as np
import pytest
from fastapi.testclient import TestClient

from lightserver.api import create_app
from lightserver.nodes import NodeConfig
from lightserver.persist import StateStore
from lightserver.ring import RingManager, RingSegment
from lightserver.satellites import (AUTO_BOOST_FLOOR, AUTO_BOOST_MAX_GAIN,
                                    AUTO_BOOST_SMOOTHING_ALPHA,
                                    BOOST_ENVELOPE_RELEASE_DECAY,
                                    VIVID_BLACK_GUARD, VIVID_LIFT_DEFAULT,
                                    VIVID_LIFT_MAX, VIVID_LIFT_MIN,
                                    SatelliteConfigError,
                                    SatelliteRuntime, apply_vivid,
                                    hex_to_uint8, hsv_to_rgb_np,
                                    load_satellites, normalize_boost,
                                    rgb_to_hsv_np, scale_preserve_hue,
                                    validate_vivid_lift,
                                    validate_vivid_saturation)

ROOT = Path(__file__).resolve().parent.parent

CONFIGS = [
    NodeConfig(name="fcob-strip", host="fcob-strip.local", pixel_count=800),
    NodeConfig(name="fcob-strip2", host="fcob-strip2.local", pixel_count=480),
    NodeConfig(name="fcob-strip3", host="fcob-strip3.local", pixel_count=640),
    NodeConfig(name="dot1", host="dot1.local", pixel_count=7),
    NodeConfig(name="dot2", host="dot2.local", pixel_count=7),
]


def make_segments():
    return [RingSegment("fcob-strip", 0, 800),
            RingSegment("fcob-strip2", 800, 480),
            RingSegment("fcob-strip3", 1280, 640)]


# -- config -----------------------------------------------------------------

def test_repo_satellites_json_loads_and_matches_nodes_json():
    from lightserver.nodes import load_node_configs
    configs = load_node_configs(ROOT / "nodes.json")
    sats = load_satellites(ROOT / "satellites.json", configs, 1920)
    assert [s.node for s in sats] == ["dot1", "dot2"]
    assert [s.anchor for s in sats] == [0, 960]
    assert all(s.mode == "mirror" for s in sats)
    assert all(s.pixel_count == 7 for s in sats)


def test_missing_satellites_file_is_empty(tmp_path):
    assert load_satellites(tmp_path / "nope.json", CONFIGS, 1920) == []


def test_empty_satellites_list_is_empty(tmp_path):
    p = tmp_path / "satellites.json"
    p.write_text(json.dumps({"satellites": []}))
    assert load_satellites(p, CONFIGS, 1920) == []


@pytest.mark.parametrize("bad", [
    {"satellites": [{"node": "nope", "anchor": 0}]},                 # unknown node
    {"satellites": [{"node": "dot1", "anchor": 1920}]},               # out of range
    {"satellites": [{"node": "dot1", "anchor": -1}]},                 # out of range
    {"satellites": [{"node": "dot1", "mode": "bogus"}]},              # bad mode
    {"satellites": [{"node": "dot1"}, {"node": "dot1"}]},             # duplicate
    {"satellites": [{"node": "dot1", "color": "not-a-color"}]},       # bad color
    {"satellites": "nope"},                                          # not a list
])
def test_satellite_config_rejects_inconsistency(tmp_path, bad):
    p = tmp_path / "satellites.json"
    p.write_text(json.dumps(bad))
    with pytest.raises(SatelliteConfigError):
        load_satellites(p, CONFIGS, 1920)


def test_hex_to_uint8():
    assert tuple(hex_to_uint8("#ff8000")) == (255, 128, 0)
    assert tuple(hex_to_uint8("fff")) == (255, 255, 255)
    with pytest.raises(ValueError):
        hex_to_uint8("nope")


# -- mirror sampling ----------------------------------------------------

def make_ring_frame(n=1920):
    frame = np.zeros((n, 3), dtype=np.uint8)
    for i in range(n):
        frame[i] = (i % 256, 0, 0)
    return frame


def test_mirror_samples_seven_pixels_centered_on_anchor():
    sat = SatelliteRuntime(node="dot1", pixel_count=7, anchor=100, mode="mirror")
    frame = make_ring_frame()
    out = sat.render(frame, 1920)
    assert out.shape == (7, 3)
    expected = [frame[i] for i in range(97, 104)]
    assert [tuple(px) for px in out] == [tuple(px) for px in expected]


def test_mirror_wraps_at_ring_seam_anchor_1919():
    sat = SatelliteRuntime(node="dot1", pixel_count=7, anchor=1919, mode="mirror")
    frame = make_ring_frame()
    out = sat.render(frame, 1920)
    expected_idx = [1916, 1917, 1918, 1919, 0, 1, 2]
    assert [tuple(px) for px in out] == [tuple(frame[i]) for i in expected_idx]


def test_mirror_wraps_at_ring_seam_anchor_0():
    sat = SatelliteRuntime(node="dot1", pixel_count=7, anchor=0, mode="mirror")
    frame = make_ring_frame()
    out = sat.render(frame, 1920)
    expected_idx = [1917, 1918, 1919, 0, 1, 2, 3]
    assert [tuple(px) for px in out] == [tuple(frame[i]) for i in expected_idx]


def test_independent_mode_shows_flat_color_ignoring_ring():
    sat = SatelliteRuntime(node="dot1", pixel_count=7, anchor=100,
                           mode="independent", color="#00ff00")
    frame = make_ring_frame()
    out = sat.render(frame, 1920)
    assert out.shape == (7, 3)
    assert all(tuple(px) == (0, 255, 0) for px in out)


def test_independent_mode_without_color_falls_back_to_mirror():
    sat = SatelliteRuntime(node="dot1", pixel_count=7, anchor=100,
                           mode="independent", color=None)
    frame = make_ring_frame()
    out = sat.render(frame, 1920)
    expected = [frame[i] for i in range(97, 104)]
    assert [tuple(px) for px in out] == [tuple(px) for px in expected]


# -- RingEngine dispatch ------------------------------------------------

def satellites_runtime():
    return [SatelliteRuntime(node="dot1", pixel_count=7, anchor=0, mode="mirror"),
            SatelliteRuntime(node="dot2", pixel_count=7, anchor=960, mode="mirror")]


class FakeSender:
    def __init__(self):
        self.frames = []
        self.resolved_ip = "1.2.3.4"
        self.ip = "1.2.3.4"
        self.frames_sent = 0
        self.last_send_ts = None
        self.last_error = None

    def send_frame(self, b):
        self.frames.append(b)
        self.frames_sent += 1

    def close(self):
        pass


def ring_engine_with_satellites(mode="headless"):
    from lightserver.ring import RingEngine
    return RingEngine(make_segments(), CONFIGS, mode=mode, seed=1,
                      satellites=satellites_runtime())


def test_satellite_frames_are_14_bytes_of_pixels_each():
    eng = ring_engine_with_satellites(mode="udp")
    fakes = {}
    for sat in eng.satellites:
        sat.sender.close()
        fakes[sat.node] = sat.sender = FakeSender()
    for name in eng.senders:
        eng.senders[name].close()
        eng.senders[name] = FakeSender()
    eng.streaming = True
    eng.step()
    assert len(fakes["dot1"].frames[0]) == 7 * 3
    assert len(fakes["dot2"].frames[0]) == 7 * 3
    eng.shutdown()


def test_offline_satellite_does_not_affect_ring_or_strips():
    eng = ring_engine_with_satellites(mode="udp")
    for name in list(eng.senders):
        eng.senders[name].close()
        eng.senders[name] = FakeSender()
    for sat in eng.satellites:
        sat.sender.close()
        sat.sender = None  # board never flashed / not on WiFi yet
    eng.streaming = True
    eng.step()  # must not raise
    assert eng.senders["fcob-strip"].frames_sent == 1
    assert eng.senders["fcob-strip3"].frames_sent == 1


def test_headless_satellites_have_no_sender():
    eng = ring_engine_with_satellites(mode="headless")
    assert all(sat.sender is None for sat in eng.satellites)
    eng.streaming = True
    eng.step()  # must not raise


# -- ring-only API --------------------------------------------------------

@pytest.fixture()
def rmanager():
    m = RingManager.from_configs(CONFIGS, make_segments(), mode="headless",
                                 satellites=satellites_runtime())
    yield m
    m.shutdown()


@pytest.fixture()
def rclient(rmanager):
    return TestClient(create_app(rmanager.default, rmanager))


def test_api_get_satellites_lists_anchor_mode_online(rclient):
    r = rclient.get("/satellites")
    assert r.status_code == 200
    body = r.json()
    assert {s["node"] for s in body} == {"dot1", "dot2"}
    dot1 = next(s for s in body if s["node"] == "dot1")
    assert dot1["anchor"] == 0
    assert dot1["mode"] == "mirror"
    assert dot1["online"] is False  # headless mode: no sender


def test_api_reposition_satellite(rclient, rmanager):
    r = rclient.post("/satellites/dot1", json={"anchor": 500})
    assert r.status_code == 200
    assert r.json()["satellite"]["anchor"] == 500
    sat = next(s for s in rmanager.ring.satellites if s.node == "dot1")
    assert sat.anchor == 500
    assert sat.mode == "mirror"  # unchanged


def test_api_reposition_bad_anchor_422(rclient):
    assert rclient.post("/satellites/dot1", json={"anchor": 1920}).status_code == 422
    assert rclient.post("/satellites/dot1", json={"anchor": -1}).status_code == 422


def test_api_unknown_satellite_404(rclient):
    assert rclient.post("/satellites/dot9", json={"anchor": 5}).status_code == 404


def test_api_switch_to_independent_mode_overrides_mirror(rclient, rmanager):
    r = rclient.post("/satellites/dot2", json={"mode": "independent",
                                               "color": "#ff00ff"})
    assert r.status_code == 200
    body = r.json()["satellite"]
    assert body["mode"] == "independent"
    assert body["color"] == "#ff00ff"

    marks = {"960": "#000000"}
    rmanager.ring.set_scene({"name": "m", "layers": [{"type": "paint",
                             "background": "#111111", "pixels": marks}]},
                             crossfade=0)
    frame = rmanager.ring.render_frame()
    sat = next(s for s in rmanager.ring.satellites if s.node == "dot2")
    out = sat.render(frame, rmanager.ring.n)
    assert all(tuple(px) == (255, 0, 255) for px in out)  # independent color, not ring

    # release back to mirror
    r = rclient.post("/satellites/dot2", json={"mode": "mirror"})
    assert r.status_code == 200 and r.json()["satellite"]["mode"] == "mirror"
    out2 = sat.render(frame, rmanager.ring.n)
    assert not all(tuple(px) == (255, 0, 255) for px in out2)


def test_api_invalid_mode_422(rclient):
    assert rclient.post("/satellites/dot1", json={"mode": "bogus"}).status_code == 422


def test_api_satellites_empty_on_plain_node_manager():
    from lightserver.engine import Engine
    from lightserver.nodes import NodeManager
    eng = Engine(num_pixels=60, mode="headless")
    m = NodeManager.single(eng)
    client = TestClient(create_app(eng, m))
    assert client.get("/satellites").json() == []
    assert client.post("/satellites/dot1", json={"anchor": 1}).status_code == 404
    m.shutdown()


# -- persistence ----------------------------------------------------------

def test_satellite_anchor_mode_persist_and_restore(tmp_path):
    store = StateStore(tmp_path / "state.json")
    m = RingManager.from_configs(CONFIGS, make_segments(), mode="headless",
                                 satellites=satellites_runtime())
    m.store = store
    m.set_satellite("dot1", anchor=1234, mode="independent", color="#123456")
    m.persist()
    m.shutdown()

    saved = json.loads((tmp_path / "state.json").read_text())
    assert saved["satellites"]["dot1"] == {"anchor": 1234, "mode": "independent",
                                           "color": "#123456", "boost": "off",
                                           "vivid_saturation": None,
                                           "vivid_lift": VIVID_LIFT_DEFAULT}

    m2 = RingManager.from_configs(CONFIGS, make_segments(), mode="headless",
                                  satellites=satellites_runtime())
    m2.restore(store)
    dot1 = next(s for s in m2.ring.satellites if s.node == "dot1")
    assert dot1.anchor == 1234
    assert dot1.mode == "independent"
    assert dot1.color == "#123456"
    dot2 = next(s for s in m2.ring.satellites if s.node == "dot2")
    assert dot2.anchor == 960  # untouched satellite keeps its config default
    m2.shutdown()


def test_persisting_satellites_does_not_clobber_scene_and_vice_versa(tmp_path):
    store = StateStore(tmp_path / "state.json")
    m = RingManager.from_configs(CONFIGS, make_segments(), mode="headless",
                                 satellites=satellites_runtime())
    m.store = store
    scene = {"name": "keepme", "layers": [{"type": "solid", "color": "#123456"}]}
    m.ring.set_scene(scene, crossfade=0)
    m.persist()  # writes both nodes + satellites
    m.set_satellite("dot1", anchor=42)
    m.persist()
    m.shutdown()

    saved = json.loads((tmp_path / "state.json").read_text())
    assert saved["nodes"]["ring"]["scene"]["name"] == "keepme"
    assert saved["satellites"]["dot1"]["anchor"] == 42


# -- boost: normalize_boost -------------------------------------------------

def test_normalize_boost_auto_and_off():
    assert normalize_boost("auto") == "auto"
    assert normalize_boost("AUTO") == "auto"
    assert normalize_boost("off") == "off"
    assert normalize_boost("OFF") == "off"
    assert normalize_boost(None) == "off"


def test_normalize_boost_both_spellings_of_off_collapse():
    assert normalize_boost(1.0) == "off"
    assert normalize_boost(1) == "off"
    assert normalize_boost("1.0") == "off"
    assert normalize_boost("1") == "off"


def test_normalize_boost_numeric_gain():
    assert normalize_boost(2.5) == 2.5
    assert normalize_boost("2.5") == 2.5
    assert normalize_boost(3) == 3.0


@pytest.mark.parametrize("bad", [0, -1, -2.5, "0", "-1", "bogus", True, False, [1, 2]])
def test_normalize_boost_rejects_garbage(bad):
    with pytest.raises(ValueError):
        normalize_boost(bad)


# -- boost: scale_preserve_hue -----------------------------------------------

def test_scale_preserve_hue_no_clip():
    window = np.array([[10, 20, 30]] * 7, dtype=np.uint8)
    out = scale_preserve_hue(window, 2.0)
    assert tuple(out[0]) == (20, 40, 60)


def test_scale_preserve_hue_clips_by_rescale_preserving_ratio():
    # peak channel 100 * 5 = 500 -> would blow out; rescale factor = 255/500
    window = np.array([[100, 50, 25]] * 7, dtype=np.uint8)
    out = scale_preserve_hue(window, 5.0)
    px = out[0]
    assert px[0] == 255  # brightest channel clipped exactly to 255
    # ratios preserved: 50/100 and 25/100 held constant (within uint8 rounding)
    assert abs(int(px[1]) - 127) <= 1
    assert abs(int(px[2]) - 64) <= 1


def test_scale_preserve_hue_black_window_stays_black():
    window = np.zeros((7, 3), dtype=np.uint8)
    out = scale_preserve_hue(window, 100.0)
    assert np.all(out == 0)


def test_scale_preserve_hue_nonpositive_gain_is_black():
    window = np.array([[10, 20, 30]] * 7, dtype=np.uint8)
    out = scale_preserve_hue(window, 0.0)
    assert np.all(out == 0)


# -- boost: SatelliteRuntime.render with boost="off"/numeric/"auto" ---------

def test_boost_off_is_passthrough():
    sat = SatelliteRuntime(node="dot1", pixel_count=7, anchor=100, mode="mirror",
                           boost="off")
    frame = make_ring_frame()
    out = sat.render(frame, 1920)
    expected = [frame[i] for i in range(97, 104)]
    assert [tuple(px) for px in out] == [tuple(px) for px in expected]


def test_boost_numeric_gain_scales_and_clips_by_rescale():
    frame = np.zeros((1920, 3), dtype=np.uint8)
    frame[100] = (100, 50, 25)
    sat = SatelliteRuntime(node="dot1", pixel_count=1, anchor=100, mode="mirror",
                           boost=5.0)
    out = sat.render(frame, 1920)
    px = out[0]
    assert px[0] == 255
    assert abs(int(px[1]) - 127) <= 1
    assert abs(int(px[2]) - 64) <= 1


def test_boost_does_not_apply_in_independent_mode_with_color():
    sat = SatelliteRuntime(node="dot1", pixel_count=7, anchor=100,
                           mode="independent", color="#010203", boost="auto")
    frame = make_ring_frame()
    out = sat.render(frame, 1920)
    assert all(tuple(px) == (1, 2, 3) for px in out)  # untouched, not boosted


# -- boost: "auto" peak normalization ----------------------------------------

def test_boost_auto_normalizes_peak_to_255_preserving_ratios():
    frame = np.zeros((1920, 3), dtype=np.uint8)
    frame[100] = (100, 50, 25)
    sat = SatelliteRuntime(node="dot1", pixel_count=1, anchor=100, mode="mirror",
                           boost="auto")
    # run several frames so the smoothed gain settles near the target
    for _ in range(60):
        out = sat.render(frame, 1920)
    px = out[0]
    assert px[0] == 255
    assert abs(int(px[1]) - 127) <= 1
    assert abs(int(px[2]) - 64) <= 1


def test_boost_auto_black_window_stays_black_no_divide_by_zero():
    frame = np.zeros((1920, 3), dtype=np.uint8)
    sat = SatelliteRuntime(node="dot1", pixel_count=7, anchor=100, mode="mirror",
                           boost="auto")
    for _ in range(10):
        out = sat.render(frame, 1920)
    assert np.all(out == 0)


def test_boost_auto_near_black_window_is_capped_at_max_gain():
    # peak below AUTO_BOOST_FLOOR: full normalization would need a huge
    # gain (255/peak); guard rail caps it at AUTO_BOOST_MAX_GAIN instead.
    peak_val = AUTO_BOOST_FLOOR - 1  # e.g. 7
    frame = np.zeros((1920, 3), dtype=np.uint8)
    frame[100] = (peak_val, 0, 0)
    sat = SatelliteRuntime(node="dot1", pixel_count=1, anchor=100, mode="mirror",
                           boost="auto")
    for _ in range(200):  # let smoothing fully settle
        out = sat.render(frame, 1920)
    expected = min(peak_val * AUTO_BOOST_MAX_GAIN, 255)
    assert abs(int(out[0][0]) - expected) <= 2
    assert int(out[0][0]) < 255  # capped gain, NOT full 255 normalization


def test_boost_auto_at_or_above_floor_normalizes_fully_to_255():
    frame = np.zeros((1920, 3), dtype=np.uint8)
    frame[100] = (int(AUTO_BOOST_FLOOR), 0, 0)
    sat = SatelliteRuntime(node="dot1", pixel_count=1, anchor=100, mode="mirror",
                           boost="auto")
    for _ in range(200):
        out = sat.render(frame, 1920)
    assert int(out[0][0]) == 255


def test_boost_auto_gain_attack_is_instant_not_smoothed():
    # 2026-08 peak-hold-envelope bugfix: unlike the old symmetric EMA
    # (which made the gain "breathe" up over ~8 frames even on ATTACK),
    # the envelope's attack is instant -- a brand new bright peak fully
    # normalizes the very first frame. See BOOST_ENVELOPE_RELEASE_DECAY.
    frame = np.zeros((1920, 3), dtype=np.uint8)
    frame[100] = (200, 0, 0)
    sat = SatelliteRuntime(node="dot1", pixel_count=1, anchor=100, mode="mirror",
                           boost="auto")
    first = sat.render(frame, 1920)[0][0]
    assert int(first) == 255  # fully normalized on frame 1, no ramp-up


def test_boost_auto_envelope_release_holds_gain_after_peak_drops():
    # The core fade-out bugfix: once a bright peak has been seen, the
    # envelope (and therefore the gain) does NOT immediately fall back
    # down just because the current window got dimmer -- it releases
    # slowly. So a window that just dropped from bright to dim renders
    # at a gain still close to what the bright peak needed, not a
    # freshly-recomputed (much higher) gain for the new dim peak.
    bright = np.zeros((1920, 3), dtype=np.uint8)
    bright[100] = (255, 0, 0)
    dim = np.zeros((1920, 3), dtype=np.uint8)
    dim[100] = (50, 0, 0)
    sat = SatelliteRuntime(node="dot1", pixel_count=1, anchor=100, mode="mirror",
                           boost="auto")
    sat.render(bright, 1920)  # envelope jumps to 255, gain settles to 1.0
    assert abs(sat._boost_gain - 1.0) < 1e-6
    out = sat.render(dim, 1920)[0][0]
    # if gain snapped to the new (dim) peak's target, 50 would be
    # re-normalized straight to 255 -- that's the bug. Instead it should
    # render close to its own true value (gain ~= 1.0 still held).
    assert int(out) < 100
    assert abs(sat._boost_gain - 1.0) < 0.05


def test_boost_auto_effective_gain_reported_in_status():
    frame = np.zeros((1920, 3), dtype=np.uint8)
    frame[100] = (200, 0, 0)
    sat = SatelliteRuntime(node="dot1", pixel_count=1, anchor=100, mode="mirror",
                           boost="auto")
    for _ in range(200):
        sat.render(frame, 1920)
    status = sat.as_status()
    assert status["boost"] == "auto"
    assert status["effective_gain"] > 1.0


def test_boost_numeric_effective_gain_reported_in_status():
    sat = SatelliteRuntime(node="dot1", pixel_count=7, anchor=0, mode="mirror",
                           boost=2.5)
    assert sat.as_status()["effective_gain"] == 2.5


def test_boost_off_effective_gain_is_one_in_status():
    sat = SatelliteRuntime(node="dot1", pixel_count=7, anchor=0, mode="mirror")
    assert sat.as_status()["boost"] == "off"
    assert sat.as_status()["effective_gain"] == 1.0


# -- boost: config file (satellites.json) ------------------------------------

def test_load_satellites_boost_default_off():
    p_configs = CONFIGS
    import tempfile
    with tempfile.TemporaryDirectory() as d:
        p = Path(d) / "satellites.json"
        p.write_text(json.dumps({"satellites": [{"node": "dot1", "anchor": 0}]}))
        sats = load_satellites(p, p_configs, 1920)
    assert sats[0].boost == "off"


def test_load_satellites_boost_auto_and_numeric(tmp_path):
    p = tmp_path / "satellites.json"
    p.write_text(json.dumps({"satellites": [
        {"node": "dot1", "anchor": 0, "boost": "auto"},
        {"node": "dot2", "anchor": 1, "boost": 2.5},
    ]}))
    sats = load_satellites(p, CONFIGS, 1920)
    assert sats[0].boost == "auto"
    assert sats[1].boost == 2.5


def test_load_satellites_rejects_bad_boost(tmp_path):
    p = tmp_path / "satellites.json"
    p.write_text(json.dumps({"satellites": [{"node": "dot1", "boost": "nonsense"}]}))
    with pytest.raises(SatelliteConfigError):
        load_satellites(p, CONFIGS, 1920)


def test_load_satellites_rejects_negative_boost(tmp_path):
    p = tmp_path / "satellites.json"
    p.write_text(json.dumps({"satellites": [{"node": "dot1", "boost": -1}]}))
    with pytest.raises(SatelliteConfigError):
        load_satellites(p, CONFIGS, 1920)


# -- boost: API ---------------------------------------------------------------

def test_api_set_boost_auto(rclient, rmanager):
    r = rclient.post("/satellites/dot1", json={"boost": "auto"})
    assert r.status_code == 200
    assert r.json()["satellite"]["boost"] == "auto"
    sat = next(s for s in rmanager.ring.satellites if s.node == "dot1")
    assert sat.boost == "auto"


def test_api_set_boost_numeric(rclient, rmanager):
    r = rclient.post("/satellites/dot1", json={"boost": 2.5})
    assert r.status_code == 200
    assert r.json()["satellite"]["boost"] == 2.5


def test_api_set_boost_off_both_spellings(rclient):
    r1 = rclient.post("/satellites/dot1", json={"boost": "off"})
    assert r1.json()["satellite"]["boost"] == "off"
    r2 = rclient.post("/satellites/dot1", json={"boost": 1.0})
    assert r2.json()["satellite"]["boost"] == "off"


def test_api_get_satellites_reports_boost_and_effective_gain(rclient):
    body = rclient.get("/satellites").json()
    dot1 = next(s for s in body if s["node"] == "dot1")
    assert dot1["boost"] == "off"
    assert dot1["effective_gain"] == 1.0


def test_api_set_boost_garbage_is_422(rclient):
    assert rclient.post("/satellites/dot1",
                        json={"boost": "bogus"}).status_code == 422
    assert rclient.post("/satellites/dot1",
                        json={"boost": -1}).status_code == 422
    assert rclient.post("/satellites/dot1",
                        json={"boost": 0}).status_code == 422


# -- boost: persistence --------------------------------------------------------

def test_boost_persists_and_restores(tmp_path):
    store = StateStore(tmp_path / "state.json")
    m = RingManager.from_configs(CONFIGS, make_segments(), mode="headless",
                                 satellites=satellites_runtime())
    m.store = store
    m.set_satellite("dot1", boost="auto")
    m.set_satellite("dot2", boost=3.0)
    m.persist()
    m.shutdown()

    saved = json.loads((tmp_path / "state.json").read_text())
    assert saved["satellites"]["dot1"]["boost"] == "auto"
    assert saved["satellites"]["dot2"]["boost"] == 3.0

    m2 = RingManager.from_configs(CONFIGS, make_segments(), mode="headless",
                                  satellites=satellites_runtime())
    m2.restore(store)
    dot1 = next(s for s in m2.ring.satellites if s.node == "dot1")
    dot2 = next(s for s in m2.ring.satellites if s.node == "dot2")
    assert dot1.boost == "auto"
    assert dot2.boost == 3.0
    m2.shutdown()


def test_boost_bad_persisted_value_falls_back_to_config_default(tmp_path):
    p = tmp_path / "state.json"
    p.write_text(json.dumps({"version": 1, "nodes": {},
                             "satellites": {"dot1": {"anchor": 0, "mode": "mirror",
                                                     "color": None,
                                                     "boost": "nonsense"}}}))
    store = StateStore(p)
    m = RingManager.from_configs(CONFIGS, make_segments(), mode="headless",
                                 satellites=satellites_runtime())
    m.restore(store)
    dot1 = next(s for s in m.ring.satellites if s.node == "dot1")
    assert dot1.boost == "off"  # config-file default kept, bad value ignored
    m.shutdown()


# -- boost: "vivid" peak-anchored hue-preserving boost ----------------------
#
# 2026-08 bugfix: the original "vivid" pushed EVERY pixel at/above the
# black-guard independently to V=1.0, which flattened a fading comet
# trail in the dot mirror window into a block of flat max-color (user
# report: "the trails do not work now looks bad"). The fix is
# PEAK-ANCHORED: the window's brightest pixel (V_peak) maps to V_out=1.0,
# every other pixel is scaled proportionally (V_in/V_peak) and then
# reshaped by a per-satellite `lift` exponent (default 0.7) -- so the
# brightest pixel is still maximal, but a fade/gradient across the window
# stays visibly a fade, never a flat block. `apply_vivid(window,
# saturation_floor, lift=1.0, peak_gain=None)` defaults to lift=1.0
# (strictly linear/proportional) and peak_gain=None (this window's own
# instantaneous 1/V_peak, i.e. the fully-settled value with no temporal
# smoothing state to thread through) unless a test says otherwise.

@pytest.mark.parametrize("rgb", [
    (200, 40, 40),    # reddish
    (40, 200, 40),    # greenish
    (40, 40, 200),    # blueish
    (200, 200, 40),   # yellowish
    (40, 200, 200),   # cyanish
    (200, 40, 200),   # magentaish
    (120, 90, 60),    # muted brown
    (30, 60, 90),     # muted blue-teal
])
def test_vivid_preserves_hue_exactly_across_color_wheel(rgb):
    window = np.array([rgb] * 7, dtype=np.uint8)
    out, _ = apply_vivid(window, None)
    expected_h, _, _ = colorsys.rgb_to_hsv(*(c / 255.0 for c in rgb))
    out_h, _, _ = colorsys.rgb_to_hsv(*(int(c) / 255.0 for c in out[0]))
    assert abs(out_h - expected_h) < 1e-3


def test_vivid_uniform_window_peak_pixel_maxes_to_255():
    # every pixel equals the peak, so every pixel lands at V_out=1.0.
    window = np.array([[100, 50, 25]] * 7, dtype=np.uint8)
    out, _ = apply_vivid(window, None)
    assert int(out[0].max()) == 255


def test_vivid_peak_pixel_lands_at_255_dimmer_pixel_stays_proportionally_dimmer():
    # simulated comet: a bright head pixel and a dimmer trailing pixel,
    # same hue -- this is the exact bug scenario (old "vivid" would have
    # maxed BOTH to 255, flattening the trail into a block).
    window = np.zeros((7, 3), dtype=np.uint8)
    window[0] = (200, 100, 0)  # head: peak of the window
    window[1] = (100, 50, 0)   # trail: exactly half the head's V
    out, _ = apply_vivid(window, None, lift=1.0)  # linear, easiest to reason about
    assert int(out[0].max()) == 255  # head (the peak) hits max
    trail_v = int(out[1].max())
    assert trail_v < 255                    # ...but the trail pixel does NOT
    assert abs(trail_v - 128) <= 2           # half the peak's V in, half V out (linear)


def test_vivid_fade_order_is_preserved_monotonic():
    # a monotonically-brightening ramp across the window must stay a
    # monotonically-brightening ramp in V after vivid, for any valid lift
    # -- a dimmer input pixel must never end up brighter than a brighter
    # one ("brightest for brightest", per the user's bug report).
    ramp = np.array([[v, v // 2, 0] for v in (20, 60, 100, 140, 180, 220, 255)],
                    dtype=np.uint8)
    for lift in (VIVID_LIFT_MIN, 0.7, 1.0, VIVID_LIFT_MAX):
        out, _ = apply_vivid(ramp, None, lift=lift)
        v_out = [int(px.max()) for px in out]
        assert v_out == sorted(v_out)  # non-decreasing, same order as input


def test_vivid_lift_below_one_makes_dim_pixels_punchier_but_still_dimmer():
    window = np.zeros((7, 3), dtype=np.uint8)
    window[0] = (255, 0, 0)   # peak
    window[1] = (64, 0, 0)    # dim trail pixel: V_in/V_peak = 64/255 ~ 0.251
    out_linear, _ = apply_vivid(window, None, lift=1.0)
    out_lifted, _ = apply_vivid(window, None, lift=0.7)
    v_linear = int(out_linear[1].max())
    v_lifted = int(out_lifted[1].max())
    assert v_lifted > v_linear     # lift<1 punches the dim pixel up...
    assert v_lifted < 255          # ...but it still never reaches the peak
    expected = (64.0 / 255.0) ** 0.7 * 255.0
    assert abs(v_lifted - expected) <= 1


def test_vivid_saturation_floor_raises_low_saturation_pixel():
    # low-saturation (washed out) pixel: S = (100-90)/100 = 0.1
    window = np.array([[100, 95, 90]] * 7, dtype=np.uint8)
    out, _ = apply_vivid(window, 0.6)
    _, s_out, _ = colorsys.rgb_to_hsv(*(int(c) / 255.0 for c in out[0]))
    assert s_out >= 0.6 - 0.01


def test_vivid_saturation_floor_never_lowers_high_saturation():
    window = np.array([[200, 20, 20]] * 7, dtype=np.uint8)  # already high S
    _, s_in, _ = colorsys.rgb_to_hsv(200 / 255, 20 / 255, 20 / 255)
    out, _ = apply_vivid(window, 0.3)  # floor well below existing S
    _, s_out, _ = colorsys.rgb_to_hsv(*(int(c) / 255.0 for c in out[0]))
    assert s_out >= s_in - 0.01


def test_vivid_keep_saturation_default_matches_input_s_exactly():
    window = np.array([[180, 90, 30]] * 7, dtype=np.uint8)
    _, s_in, _ = colorsys.rgb_to_hsv(180 / 255, 90 / 255, 30 / 255)
    out, _ = apply_vivid(window, None)
    _, s_out, _ = colorsys.rgb_to_hsv(*(int(c) / 255.0 for c in out[0]))
    assert abs(s_out - s_in) < 0.01


def test_vivid_whole_window_below_black_guard_is_untouched():
    # V_peak itself is below the guard -> the whole window passes
    # through unchanged, "black stays black".
    dark_val = int(round((VIVID_BLACK_GUARD * 255) - 1))  # strictly below floor
    window = np.array([[dark_val, dark_val // 2, 0]] * 7, dtype=np.uint8)
    out, _ = apply_vivid(window, None)
    assert np.array_equal(out, window)


def test_vivid_black_pixel_guard_boundary_at_or_above_is_processed():
    bright_val = int(round(VIVID_BLACK_GUARD * 255))  # at/just above floor
    window = np.array([[bright_val, 0, 0]] * 7, dtype=np.uint8)  # uniform -> IS the peak
    out, _ = apply_vivid(window, None)
    assert int(out[0].max()) == 255


def test_vivid_mixed_window_below_guard_pixel_stays_untouched_above_guard_peak_maxes():
    window = np.array([[0, 0, 0], [100, 50, 25]] + [[0, 0, 0]] * 5, dtype=np.uint8)
    out, _ = apply_vivid(window, None)
    assert tuple(out[0]) == (0, 0, 0)  # below guard, untouched
    assert int(out[1].max()) == 255    # above guard AND the window's peak -> maxed


def test_vivid_mean_gain_is_uniform_across_pixels_when_lift_is_linear():
    # with lift=1.0, V_out/V_in is the SAME constant (1/V_peak) for every
    # above-guard pixel -- that's the defining property of peak-anchored
    # proportional scaling (unlike the old per-pixel-independent "vivid",
    # where each pixel had its own gain = 1/its own V_in).
    window = np.zeros((7, 3), dtype=np.uint8)
    window[0] = (100, 0, 0)   # the peak: v_in = 100/255, v_out = 1.0
    window[1] = (50, 0, 0)    # half the peak: v_in = 50/255, v_out = 0.5
    out, gain = apply_vivid(window, None, lift=1.0)
    expected_gain = 255.0 / 100.0  # = 1 / V_peak, same for both pixels
    assert abs(gain - expected_gain) < 1e-6


def test_vivid_mean_gain_reflects_lift_shaping():
    # with lift < 1.0 the per-pixel gain (V_out/V_in) is no longer
    # uniform -- the dimmer pixel gets relatively MORE gain than the peak
    # pixel (which is still gain = 1/V_peak exactly, since 1**lift == 1).
    window = np.zeros((7, 3), dtype=np.uint8)
    window[0] = (100, 0, 0)   # peak
    window[1] = (50, 0, 0)    # half the peak
    out, gain = apply_vivid(window, None, lift=0.7)
    peak_gain = 255.0 / 100.0
    dim_v_out = (0.5) ** 0.7
    dim_gain = dim_v_out / (50.0 / 255.0)
    expected_gain = (peak_gain + dim_gain) / 2.0
    assert abs(gain - expected_gain) < 1e-6
    assert dim_gain > peak_gain  # lift<1 gives the dimmer pixel more relative boost


def test_vivid_mean_gain_is_one_when_whole_window_below_guard():
    window = np.array([[1, 0, 0]] * 7, dtype=np.uint8)  # well below guard
    out, gain = apply_vivid(window, None)
    assert gain == 1.0


def test_vivid_explicit_peak_gain_overrides_instantaneous_peak():
    # peak_gain lets a caller (SatelliteRuntime, with its temporally-
    # smoothed factor) decouple "what gain to apply" from "what this
    # frame's raw peak is" -- passing a peak_gain lower than the
    # instantaneous 1/V_peak means the peak pixel does NOT hit 255 this
    # frame (simulating a smoothed gain still ramping up, i.e. "breathing").
    window = np.zeros((7, 3), dtype=np.uint8)
    window[0] = (100, 0, 0)  # peak: instantaneous gain would be 255/100=2.55
    out, gain = apply_vivid(window, None, lift=1.0, peak_gain=1.5)  # under-shoot
    assert int(out[0].max()) < 255
    assert abs(int(out[0].max()) - round(100 * 1.5)) <= 1


def test_rgb_to_hsv_np_matches_colorsys():
    rgb = np.array([[10, 200, 90], [255, 0, 0], [0, 0, 0], [255, 255, 255]],
                   dtype=np.uint8)
    h, s, v = rgb_to_hsv_np(rgb)
    for i, px in enumerate(rgb):
        eh, es, ev = colorsys.rgb_to_hsv(*(int(c) / 255.0 for c in px))
        assert abs(h[i] - eh) < 1e-9
        assert abs(s[i] - es) < 1e-9
        assert abs(v[i] - ev) < 1e-9


def test_hsv_to_rgb_np_matches_colorsys():
    samples = [(0.1, 0.5, 0.8), (0.6, 1.0, 1.0), (0.0, 0.0, 0.0), (0.9, 0.2, 0.4)]
    h = np.array([s[0] for s in samples])
    s = np.array([s[1] for s in samples])
    v = np.array([s[2] for s in samples])
    out = hsv_to_rgb_np(h, s, v)
    for i, (hh, ss, vv) in enumerate(samples):
        er, eg, eb = colorsys.hsv_to_rgb(hh, ss, vv)
        expected = (round(er * 255), round(eg * 255), round(eb * 255))
        assert tuple(int(c) for c in out[i]) == expected


def test_vivid_boost_via_satellite_render():
    # single-pixel mirror window: the window's own peak IS that pixel, so
    # once the smoothed peak-gain settles it lands at exactly V_out=1.0,
    # same as the single-pixel case always did.
    frame = np.zeros((1920, 3), dtype=np.uint8)
    frame[100] = (100, 50, 25)
    sat = SatelliteRuntime(node="dot1", pixel_count=1, anchor=100, mode="mirror",
                           boost="vivid")
    for _ in range(200):  # let the smoothed peak-gain fully settle
        out = sat.render(frame, 1920)
    assert int(out[0].max()) == 255
    _, s_in, _ = colorsys.rgb_to_hsv(100 / 255, 50 / 255, 25 / 255)
    _, s_out, _ = colorsys.rgb_to_hsv(*(int(c) / 255.0 for c in out[0]))
    assert abs(s_out - s_in) < 0.01  # keep-S default


def test_vivid_boost_with_saturation_floor_via_satellite_render():
    frame = np.zeros((1920, 3), dtype=np.uint8)
    frame[100] = (100, 95, 90)  # washed-out, low S
    sat = SatelliteRuntime(node="dot1", pixel_count=1, anchor=100, mode="mirror",
                           boost="vivid", vivid_saturation=0.7)
    for _ in range(200):
        out = sat.render(frame, 1920)
    _, s_out, _ = colorsys.rgb_to_hsv(*(int(c) / 255.0 for c in out[0]))
    assert s_out >= 0.7 - 0.01


def test_vivid_boost_does_not_apply_in_independent_mode_with_color():
    sat = SatelliteRuntime(node="dot1", pixel_count=7, anchor=100,
                           mode="independent", color="#010203", boost="vivid")
    frame = make_ring_frame()
    out = sat.render(frame, 1920)
    assert all(tuple(px) == (1, 2, 3) for px in out)  # untouched, not boosted


def test_vivid_boost_preserves_fading_trail_not_flat_block():
    # THE bug scenario, reproduced directly: a "comet" -- one bright head
    # pixel and a dimmer trailing pixel of the same hue -- in a 2px
    # mirror window. Old "vivid" would max BOTH to V=255 (flat block,
    # "trails do not work"); the fix keeps the trail visibly dimmer than
    # the head once the smoothed peak-gain settles.
    frame = np.zeros((1920, 3), dtype=np.uint8)
    frame[99] = (100, 50, 0)   # trail (dimmer, same hue)
    frame[100] = (200, 100, 0)  # head (peak)
    sat = SatelliteRuntime(node="dot1", pixel_count=2, anchor=100, mode="mirror",
                           boost="vivid", vivid_lift=1.0)  # linear, easiest to check
    for _ in range(200):  # settle
        out = sat.render(frame, 1920)
    head_v, trail_v = int(out[1].max()), int(out[0].max())
    assert head_v == 255
    assert trail_v < 255
    assert abs(trail_v - 128) <= 2  # exactly half the head's V, half the V out (linear)


def test_vivid_boost_gain_attack_is_instant_not_smoothed():
    # mirrors test_boost_auto_gain_attack_is_instant_not_smoothed: the
    # peak-hold envelope's attack is instant, so a brand new bright peak
    # is fully normalized on the very first frame (see
    # BOOST_ENVELOPE_RELEASE_DECAY) -- unlike the old symmetric EMA this
    # replaced, which made even the ATTACK breathe up over ~8 frames.
    frame = np.zeros((1920, 3), dtype=np.uint8)
    frame[100] = (200, 0, 0)
    sat = SatelliteRuntime(node="dot1", pixel_count=1, anchor=100, mode="mirror",
                           boost="vivid", vivid_lift=1.0)
    first = int(sat.render(frame, 1920)[0].max())
    assert first == 255  # fully peak-normalized on frame 1, no ramp-up


def test_vivid_envelope_release_holds_gain_after_peak_drops():
    # vivid mirrors auto's envelope-release fix: once a bright peak has
    # been seen, a subsequently dimmer window does NOT get its gain
    # snapped back up to fully re-normalize itself -- the gain (1/E)
    # stays held near what the earlier bright peak needed, so the
    # dimmer content renders proportionally dimmer, not maxed again.
    bright = np.zeros((1920, 3), dtype=np.uint8)
    bright[100] = (255, 0, 0)
    dim = np.zeros((1920, 3), dtype=np.uint8)
    dim[100] = (64, 0, 0)  # 1/4 the bright peak's V
    sat = SatelliteRuntime(node="dot1", pixel_count=1, anchor=100, mode="mirror",
                           boost="vivid", vivid_lift=1.0)
    sat.render(bright, 1920)  # envelope -> 1.0 (V), peak_gain -> ~1.0
    assert abs(sat._vivid_peak_gain - 1.0) < 1e-3
    out = sat.render(dim, 1920)[0].max()
    # if the gain had snapped to re-normalize this frame's own peak, out
    # would be 255 (the bug). Held gain instead keeps it close to 64.
    assert int(out) < 100


def test_vivid_effective_gain_reported_in_status():
    frame = np.zeros((1920, 3), dtype=np.uint8)
    frame[100] = (100, 0, 0)
    sat = SatelliteRuntime(node="dot1", pixel_count=1, anchor=100, mode="mirror",
                           boost="vivid", vivid_lift=1.0)
    for _ in range(200):  # settle
        sat.render(frame, 1920)
    status = sat.as_status()
    assert status["boost"] == "vivid"
    assert abs(status["effective_gain"] - (255.0 / 100.0)) < 1e-2


def test_vivid_status_includes_vivid_saturation_field():
    sat = SatelliteRuntime(node="dot1", pixel_count=7, anchor=0, mode="mirror",
                           boost="vivid", vivid_saturation=0.5)
    status = sat.as_status()
    assert status["vivid_saturation"] == 0.5


def test_vivid_status_includes_vivid_lift_field_default_and_custom():
    default_sat = SatelliteRuntime(node="dot1", pixel_count=7, anchor=0, mode="mirror",
                                   boost="vivid")
    assert default_sat.as_status()["vivid_lift"] == VIVID_LIFT_DEFAULT
    custom_sat = SatelliteRuntime(node="dot1", pixel_count=7, anchor=0, mode="mirror",
                                  boost="vivid", vivid_lift=1.2)
    assert custom_sat.as_status()["vivid_lift"] == 1.2


# -- boost: normalize_boost + validate_vivid_saturation for "vivid" ---------

def test_normalize_boost_vivid_case_variants():
    assert normalize_boost("vivid") == "vivid"
    assert normalize_boost("VIVID") == "vivid"
    assert normalize_boost("ViViD") == "vivid"


def test_validate_vivid_saturation_none_passthrough():
    assert validate_vivid_saturation(None) is None


def test_validate_vivid_saturation_valid_range():
    assert validate_vivid_saturation(0.0) == 0.0
    assert validate_vivid_saturation(1.0) == 1.0
    assert validate_vivid_saturation(0.42) == 0.42


@pytest.mark.parametrize("bad", [-0.01, 1.01, -1, 2, "bogus"])
def test_validate_vivid_saturation_rejects_out_of_range(bad):
    with pytest.raises(ValueError):
        validate_vivid_saturation(bad)


def test_validate_vivid_lift_none_uses_default():
    assert validate_vivid_lift(None) == VIVID_LIFT_DEFAULT == 0.7


def test_validate_vivid_lift_valid_range():
    assert validate_vivid_lift(VIVID_LIFT_MIN) == VIVID_LIFT_MIN
    assert validate_vivid_lift(VIVID_LIFT_MAX) == VIVID_LIFT_MAX
    assert validate_vivid_lift(1.0) == 1.0
    assert validate_vivid_lift(0.42) == 0.42


@pytest.mark.parametrize("bad", [0.29, 1.51, -1, 5, "bogus"])
def test_validate_vivid_lift_rejects_out_of_range(bad):
    with pytest.raises(ValueError):
        validate_vivid_lift(bad)


# -- boost: "vivid" config file (satellites.json) ----------------------------

def test_load_satellites_boost_vivid_and_saturation(tmp_path):
    p = tmp_path / "satellites.json"
    p.write_text(json.dumps({"satellites": [
        {"node": "dot1", "anchor": 0, "boost": "vivid", "vivid_saturation": 0.6},
        {"node": "dot2", "anchor": 1, "boost": "vivid"},
    ]}))
    sats = load_satellites(p, CONFIGS, 1920)
    assert sats[0].boost == "vivid"
    assert sats[0].vivid_saturation == 0.6
    assert sats[1].boost == "vivid"
    assert sats[1].vivid_saturation is None


def test_load_satellites_rejects_bad_vivid_saturation(tmp_path):
    p = tmp_path / "satellites.json"
    p.write_text(json.dumps({"satellites": [
        {"node": "dot1", "boost": "vivid", "vivid_saturation": 1.5}]}))
    with pytest.raises(SatelliteConfigError):
        load_satellites(p, CONFIGS, 1920)


def test_load_satellites_boost_vivid_lift_explicit_and_default(tmp_path):
    p = tmp_path / "satellites.json"
    p.write_text(json.dumps({"satellites": [
        {"node": "dot1", "anchor": 0, "boost": "vivid", "vivid_lift": 1.2},
        {"node": "dot2", "anchor": 1, "boost": "vivid"},  # omitted -> default
    ]}))
    sats = load_satellites(p, CONFIGS, 1920)
    assert sats[0].vivid_lift == 1.2
    assert sats[1].vivid_lift == VIVID_LIFT_DEFAULT


def test_load_satellites_rejects_bad_vivid_lift(tmp_path):
    p = tmp_path / "satellites.json"
    p.write_text(json.dumps({"satellites": [
        {"node": "dot1", "boost": "vivid", "vivid_lift": 2.0}]}))
    with pytest.raises(SatelliteConfigError):
        load_satellites(p, CONFIGS, 1920)


# -- boost: "vivid" API -------------------------------------------------------

def test_api_set_boost_vivid(rclient, rmanager):
    r = rclient.post("/satellites/dot1", json={"boost": "vivid"})
    assert r.status_code == 200
    assert r.json()["satellite"]["boost"] == "vivid"
    sat = next(s for s in rmanager.ring.satellites if s.node == "dot1")
    assert sat.boost == "vivid"


def test_api_set_vivid_saturation(rclient, rmanager):
    r = rclient.post("/satellites/dot1",
                     json={"boost": "vivid", "vivid_saturation": 0.6})
    assert r.status_code == 200
    assert r.json()["satellite"]["vivid_saturation"] == 0.6


def test_api_set_vivid_saturation_independently_of_boost(rclient, rmanager):
    rclient.post("/satellites/dot1", json={"boost": "vivid"})
    r = rclient.post("/satellites/dot1", json={"vivid_saturation": 0.3})
    assert r.status_code == 200
    body = r.json()["satellite"]
    assert body["boost"] == "vivid"       # unchanged
    assert body["vivid_saturation"] == 0.3


@pytest.mark.parametrize("bad", [-0.1, 1.1, "bogus"])
def test_api_set_vivid_saturation_out_of_range_422(rclient, bad):
    r = rclient.post("/satellites/dot1", json={"vivid_saturation": bad})
    assert r.status_code == 422


def test_api_set_vivid_lift(rclient, rmanager):
    r = rclient.post("/satellites/dot1",
                     json={"boost": "vivid", "vivid_lift": 1.1})
    assert r.status_code == 200
    assert r.json()["satellite"]["vivid_lift"] == 1.1


def test_api_set_vivid_lift_independently_of_boost(rclient, rmanager):
    rclient.post("/satellites/dot1", json={"boost": "vivid"})
    r = rclient.post("/satellites/dot1", json={"vivid_lift": 0.5})
    assert r.status_code == 200
    body = r.json()["satellite"]
    assert body["boost"] == "vivid"  # unchanged
    assert body["vivid_lift"] == 0.5


@pytest.mark.parametrize("bad", [0.29, 1.51, "bogus"])
def test_api_set_vivid_lift_out_of_range_422(rclient, bad):
    r = rclient.post("/satellites/dot1", json={"vivid_lift": bad})
    assert r.status_code == 422


def test_api_get_satellites_reports_vivid_boost_saturation_lift_and_gain(rclient, rmanager):
    rmanager.set_satellite("dot1", boost="vivid", vivid_saturation=0.5, vivid_lift=0.9)
    rmanager.ring.set_scene({"name": "m", "layers": [
        {"type": "paint", "background": "#111111", "pixels": {"0": "#ff0000"}}]},
        crossfade=0)
    rmanager.ring.render_frame()
    sat = next(s for s in rmanager.ring.satellites if s.node == "dot1")
    sat.render(rmanager.ring.render_frame(), rmanager.ring.n)
    body = rclient.get("/satellites").json()
    dot1 = next(s for s in body if s["node"] == "dot1")
    assert dot1["boost"] == "vivid"
    assert dot1["vivid_saturation"] == 0.5
    assert dot1["vivid_lift"] == 0.9
    assert "effective_gain" in dot1


def test_api_get_satellites_reports_vivid_lift_default(rclient):
    body = rclient.get("/satellites").json()
    dot1 = next(s for s in body if s["node"] == "dot1")
    assert dot1["vivid_lift"] == VIVID_LIFT_DEFAULT


# -- boost: "vivid" persistence ----------------------------------------------

def test_vivid_boost_and_saturation_persist_and_restore(tmp_path):
    store = StateStore(tmp_path / "state.json")
    m = RingManager.from_configs(CONFIGS, make_segments(), mode="headless",
                                 satellites=satellites_runtime())
    m.store = store
    m.set_satellite("dot1", boost="vivid", vivid_saturation=0.65)
    m.persist()
    m.shutdown()

    saved = json.loads((tmp_path / "state.json").read_text())
    assert saved["satellites"]["dot1"]["boost"] == "vivid"
    assert saved["satellites"]["dot1"]["vivid_saturation"] == 0.65

    m2 = RingManager.from_configs(CONFIGS, make_segments(), mode="headless",
                                  satellites=satellites_runtime())
    m2.restore(store)
    dot1 = next(s for s in m2.ring.satellites if s.node == "dot1")
    assert dot1.boost == "vivid"
    assert dot1.vivid_saturation == 0.65
    m2.shutdown()


def test_vivid_lift_persists_and_restores(tmp_path):
    store = StateStore(tmp_path / "state.json")
    m = RingManager.from_configs(CONFIGS, make_segments(), mode="headless",
                                 satellites=satellites_runtime())
    m.store = store
    m.set_satellite("dot1", boost="vivid", vivid_lift=1.25)
    m.persist()
    m.shutdown()

    saved = json.loads((tmp_path / "state.json").read_text())
    assert saved["satellites"]["dot1"]["vivid_lift"] == 1.25

    m2 = RingManager.from_configs(CONFIGS, make_segments(), mode="headless",
                                  satellites=satellites_runtime())
    m2.restore(store)
    dot1 = next(s for s in m2.ring.satellites if s.node == "dot1")
    assert dot1.vivid_lift == 1.25
    m2.shutdown()


def test_vivid_lift_bad_persisted_value_falls_back_to_config_default(tmp_path):
    p = tmp_path / "state.json"
    p.write_text(json.dumps({"version": 1, "nodes": {},
                             "satellites": {"dot1": {"anchor": 0, "mode": "mirror",
                                                     "color": None, "boost": "vivid",
                                                     "vivid_lift": 99.0}}}))
    store = StateStore(p)
    m = RingManager.from_configs(CONFIGS, make_segments(), mode="headless",
                                 satellites=satellites_runtime())
    m.restore(store)
    dot1 = next(s for s in m.ring.satellites if s.node == "dot1")
    assert dot1.vivid_lift == VIVID_LIFT_DEFAULT  # bad value ignored, default kept
    m.shutdown()


def test_vivid_persisted_state_missing_vivid_lift_field_uses_default(tmp_path):
    # simulates a state.json written by a version of the server BEFORE
    # vivid_lift existed: the key is simply absent, not null -- restore
    # must not error and must fall back to VIVID_LIFT_DEFAULT cleanly.
    p = tmp_path / "state.json"
    p.write_text(json.dumps({"version": 1, "nodes": {},
                             "satellites": {"dot1": {"anchor": 0, "mode": "mirror",
                                                     "color": None, "boost": "vivid"}}}))
    store = StateStore(p)
    m = RingManager.from_configs(CONFIGS, make_segments(), mode="headless",
                                 satellites=satellites_runtime())
    m.restore(store)  # must not raise
    dot1 = next(s for s in m.ring.satellites if s.node == "dot1")
    assert dot1.boost == "vivid"
    assert dot1.vivid_lift == VIVID_LIFT_DEFAULT
    m.shutdown()


# -- boost: peak-hold envelope / comet fade-out simulation (2026-08 bugfix) --
#
# Real-world bug report: "the fadeout does not work — it should fade out
# smoothly; we just wanted bias towards brighter." The OLD "vivid"/"auto"
# gain was a raw per-frame peak target (1/V_peak or 255/peak), only ever
# smoothed SYMMETRICALLY (AUTO_BOOST_SMOOTHING_ALPHA). As a comet's bright
# head left the dot's mirror window, the window's own peak shrank frame
# over frame (only the dimmer fading trail left), so the *target* gain
# kept RISING -- the symmetric smoothing chased it up, re-amplifying the
# fading tail back toward max instead of letting it fade, then the tail
# would suddenly drop out entirely once it crossed the black-guard ("snap
# to black"). The fix is a peak-HOLD envelope (fast attack / slow
# ~3s-half-life release, BOOST_ENVELOPE_RELEASE_DECAY) applied to BOTH
# "auto" and "vivid": gain only rises again once the envelope has decayed
# well below the held peak, so a fading trail renders through a ~flat gain
# (i.e. accurately, proportional to its own true fade on the strip) while
# ambient dim content still gets boosted bright again after enough quiet.

def _comet_frame(val, idx=100):
    frame = np.zeros((1920, 3), dtype=np.uint8)
    frame[idx] = (val, 0, 0)
    return frame


def _simulate_comet_pass(boost, vivid_lift=1.0):
    """Rising head -> peak -> exponentially-fading tail -> black, then a
    long run of steady dim ambient content. Returns (head_out,
    tail_pairs, ambient_outs) where tail_pairs is [(v_in, v_out), ...]
    for the strictly-decaying trail portion (post-peak, above black), and
    ambient_outs is the rendered value at several points during a long
    run of a fixed dim (peak_val=15, above both floor/guard) window,
    sampled to show the gain eventually climbing again."""
    sat = SatelliteRuntime(node="dot1", pixel_count=1, anchor=100, mode="mirror",
                           boost=boost, vivid_lift=vivid_lift)
    for v in (0, 50, 100, 150, 200, 255):  # rising head
        head_out = int(sat.render(_comet_frame(v), 1920)[0][0])
    tail_pairs = []
    val = 255.0
    for _ in range(60):
        val *= 0.85  # exponential fade, same spirit as a real comet trail
        if val < 1:
            break
        v_in = int(round(val))
        v_out = int(sat.render(_comet_frame(v_in), 1920)[0][0])
        tail_pairs.append((v_in, v_out))
    ambient_outs = []
    for i in range(1, 901):
        out = int(sat.render(_comet_frame(15), 1920)[0][0])
        if i in (30, 300, 900):  # 1s, 10s, 30s @ 30fps
            ambient_outs.append(out)
    return head_out, tail_pairs, ambient_outs


@pytest.mark.parametrize("boost", ["auto", "vivid"])
def test_comet_pass_head_hits_max(boost):
    head_out, _, _ = _simulate_comet_pass(boost)
    assert head_out == 255


@pytest.mark.parametrize("boost", ["auto", "vivid"])
def test_comet_fadeout_is_monotonically_decreasing_no_plateau_no_replateau(boost):
    # THE bugfix assertion: the rendered tail must strictly follow the
    # input down -- never hold flat at max, never rise back up (the
    # "re-amplified tail" bug) and never snap straight to 0 skipping
    # intermediate values (the "snap to black" symptom of the same bug).
    _, tail_pairs, _ = _simulate_comet_pass(boost)
    outs = [o for _, o in tail_pairs]
    assert len(outs) > 10  # sanity: the fade actually has several frames
    assert all(outs[i] >= outs[i + 1] for i in range(len(outs) - 1)), (
        f"tail output is not monotonically decreasing: {outs}")
    assert outs[0] < 255  # no plateau at max right after the peak frame


@pytest.mark.parametrize("boost", ["auto", "vivid"])
def test_comet_fadeout_output_is_proportional_to_input_not_reamplified(boost):
    # The held-envelope gain should stay close to the ~1.0 the peak frame
    # settled on, so tail output tracks input closely (accurate to what's
    # actually fading on the strip) rather than being renormalized toward
    # max as the old per-frame-peak bug did.
    _, tail_pairs, _ = _simulate_comet_pass(boost)
    for v_in, v_out in tail_pairs:
        assert v_out <= v_in * 1.3 + 3  # never wildly re-amplified
        assert v_out >= v_in * 0.7 - 1  # not clipped/undershooting either


@pytest.mark.parametrize("boost", ["auto", "vivid"])
def test_comet_ambient_dim_content_gets_reboosted_after_quiet(boost):
    # "bias towards brighter" preserved: after enough quiet time with no
    # new bright peak, the envelope relaxes and a steady dim window is
    # boosted bright again, same as it would be if it had never seen the
    # earlier bright comet.
    _, _, ambient_outs = _simulate_comet_pass(boost)
    out_1s, out_10s, out_30s = ambient_outs
    assert out_1s < out_10s < out_30s  # gain keeps climbing back as envelope relaxes
    assert out_30s >= 250  # eventually fully boosted, same as if never boosted


def test_boost_envelope_release_decay_matches_documented_three_second_half_life():
    # BOOST_ENVELOPE_RELEASE_DECAY is derived, not hand-picked -- verify
    # it actually implements a ~3s half-life at the documented 30fps
    # assumption: decaying for 90 frames should land within 1% of 0.5.
    remaining = BOOST_ENVELOPE_RELEASE_DECAY ** 90
    assert abs(remaining - 0.5) < 0.005
