"""Semantic side-channel: packet shape, size cap, top-8 truncation, 10Hz
cadence (every 3rd engine frame), position wrapping, color resolution,
energy/dominant/black-exclusion, strobe_phase, projector-block validation
and verbatim passthrough, semantic_port persistence, and the
disabled/expired/no-semantic-port silence guarantees."""
import json
import time

import numpy as np
import pytest
from fastapi.testclient import TestClient

from lightserver.api import create_app
from lightserver.engine import Engine
from lightserver.nodes import NodeConfig
from lightserver.persist import StateStore
from lightserver.ring import RingEngine, RingManager, RingSegment
from lightserver.scene import SceneInstance, SceneValidationError, validate_scene
from lightserver.semantic import (MAX_BYTES, MAX_TRACKS, SEPARATORS,
                                  _dominant, _energy, build_semantic_packet)
from lightserver.taps import SEMANTIC_FRAME_DIVISOR, TapManager

CONFIGS = [
    NodeConfig(name="fcob-strip", host="fcob-strip.local", pixel_count=800),
    NodeConfig(name="fcob-strip2", host="fcob-strip2.local", pixel_count=480),
    NodeConfig(name="fcob-strip3", host="fcob-strip3.local", pixel_count=640),
]


def make_segments():
    return [RingSegment("fcob-strip", 0, 800),
            RingSegment("fcob-strip2", 800, 480),
            RingSegment("fcob-strip3", 1280, 640)]


class FakeTapSocket:
    def __init__(self):
        self.sent = []  # list of (addr, bytes)

    def sendto(self, pkt, addr):
        self.sent.append((addr, pkt))

    def close(self):
        pass


def comet_scene(n=200, wrap=True):
    return {
        "name": "comet",
        "layers": [{
            "type": "particles",
            "physics": {"wrap": wrap},
            "particles": [{"pos": 10.0, "vel": 420.0, "size": 3.0,
                          "color": "#ffdd00", "lifetime": None}],
        }],
    }


def multi_particle_scene(n_particles=12, n=200):
    particles = [{"pos": float(i * 5), "vel": float(i), "size": 1.5,
                 "color": "#%02x%02x%02x" % (i * 20 % 255, 100, 200)}
                for i in range(n_particles)]
    return {"name": "swarm",
           "layers": [{"type": "particles", "physics": {"wrap": True},
                      "particles": particles}]}


# -- packet shape ------------------------------------------------------

def test_packet_has_expected_top_level_shape():
    scene = SceneInstance(comet_scene(), 200, seed=1)
    scene.render(1 / 30)
    frame = np.zeros((200, 3), dtype=np.uint8)
    pkt = build_semantic_packet(7, scene, frame)
    assert pkt["v"] == 1
    assert pkt["seq"] == 7
    assert pkt["scene"] == "comet"
    assert set(pkt["stats"]) == {"energy", "dominant", "strobe_phase"}
    assert isinstance(pkt["tracks"], list)
    assert "projector" in pkt


# -- ts field ------------------------------------------------------------

def test_packet_has_ts_field_as_fresh_float():
    scene = SceneInstance(comet_scene(), 200, seed=1)
    scene.render(1 / 30)
    frame = np.zeros((200, 3), dtype=np.uint8)
    before = time.time()
    pkt = build_semantic_packet(0, scene, frame)
    after = time.time()
    assert "ts" in pkt
    assert isinstance(pkt["ts"], float)
    assert before <= pkt["ts"] <= after


def test_ts_field_ordering_after_seq():
    """Not load-bearing for JSON semantics, but keeps the documented field
    order (v, seq, ts, scene, ...) stable for anyone diffing raw bytes."""
    scene = SceneInstance(comet_scene(), 200, seed=1)
    scene.render(1 / 30)
    frame = np.zeros((200, 3), dtype=np.uint8)
    pkt = build_semantic_packet(0, scene, frame)
    keys = list(pkt.keys())
    assert keys.index("seq") < keys.index("ts") < keys.index("scene")


def test_track_fields_present():
    scene = SceneInstance(comet_scene(), 200, seed=1)
    scene.render(1 / 30)
    frame = np.zeros((200, 3), dtype=np.uint8)
    pkt = build_semantic_packet(0, scene, frame)
    assert len(pkt["tracks"]) == 1
    tr = pkt["tracks"][0]
    for key in ("id", "layer", "pos", "vel", "color", "size", "brightness"):
        assert key in tr


# -- top-8 truncation ----------------------------------------------------

def test_top_8_truncation_by_brightness():
    scene = SceneInstance(multi_particle_scene(20), 200, seed=1)
    scene.render(1 / 30)
    frame = np.zeros((200, 3), dtype=np.uint8)
    pkt = build_semantic_packet(0, scene, frame)
    assert len(pkt["tracks"]) <= MAX_TRACKS
    brightnesses = [t["brightness"] for t in pkt["tracks"]]
    assert brightnesses == sorted(brightnesses, reverse=True)


def test_field_layers_contribute_no_tracks():
    scene_dict = {"name": "wavey", "layers": [{"type": "wave"}]}
    scene = SceneInstance(scene_dict, 200, seed=1)
    scene.render(1 / 30)
    frame = np.zeros((200, 3), dtype=np.uint8)
    pkt = build_semantic_packet(0, scene, frame)
    assert pkt["tracks"] == []


# -- size cap -------------------------------------------------------------

def test_packet_stays_under_size_cap_even_with_many_particles_and_long_name():
    long_name = "a" * 80
    sc = multi_particle_scene(40)
    sc["name"] = long_name
    sc["projector"] = {"style": "react", "energy": 0.5,
                       "palette": ["#ff0000", "#00ff00", "#0000ff", "#ffffff"],
                       "brightness": 0.9}
    scene = SceneInstance(sc, 200, seed=1)
    scene.render(1 / 30)
    frame = (np.random.default_rng(0).random((200, 3)) * 255).astype(np.uint8)
    pkt = build_semantic_packet(123456789, scene, frame)
    encoded = json.dumps(pkt, separators=SEPARATORS).encode()
    assert len(encoded) <= MAX_BYTES


# -- position wrapping ------------------------------------------------------

def test_track_position_wraps_into_ring_range():
    scene_dict = {
        "name": "wrapper",
        "layers": [{"type": "particles", "physics": {"wrap": True},
                    "particles": [{"pos": 5.0, "vel": 5000.0, "color": "#ffffff"}]}],
    }
    scene = SceneInstance(scene_dict, 100, seed=1)
    for _ in range(30):
        scene.render(1 / 30)
    frame = np.zeros((100, 3), dtype=np.uint8)
    pkt = build_semantic_packet(0, scene, frame)
    assert len(pkt["tracks"]) == 1
    assert 0.0 <= pkt["tracks"][0]["pos"] < 100.0


# -- color resolution -----------------------------------------------------

def test_static_particle_color_resolves_close_to_spawned_hex():
    scene = SceneInstance(comet_scene(), 200, seed=1)
    scene.render(1 / 30)
    frame = np.zeros((200, 3), dtype=np.uint8)
    pkt = build_semantic_packet(0, scene, frame)
    color = pkt["tracks"][0]["color"]
    r, g, b = int(color[1:3], 16), int(color[3:5], 16), int(color[5:7], 16)
    # #ffdd00 spawned color, full brightness/no fade yet -> resolves close
    assert r >= 250
    assert g >= 200
    assert b <= 30


# -- energy / dominant / black exclusion ------------------------------------

def test_energy_of_black_frame_is_zero():
    frame = np.zeros((100, 3), dtype=np.uint8)
    assert _energy(frame) == 0.0


def test_energy_of_white_frame_is_one():
    frame = np.full((100, 3), 255, dtype=np.uint8)
    assert _energy(frame) == 1.0


def test_dominant_all_black_returns_pure_black():
    frame = np.zeros((100, 3), dtype=np.uint8)
    assert _dominant(frame) == ["#000000"]


def test_dominant_excludes_near_black_pixels():
    frame = np.zeros((100, 3), dtype=np.uint8)
    frame[:90] = (2, 2, 2)          # near-black, excluded
    frame[90:] = (255, 220, 0)      # yellow accent, the only "real" content
    dom = _dominant(frame)
    assert dom[0] != "#000000"
    r = int(dom[0][1:3], 16)
    g = int(dom[0][3:5], 16)
    b = int(dom[0][5:7], 16)
    assert r > 200 and g > 150 and b < 60


def test_dominant_ignores_dim_trail_pixels_that_quantize_to_black():
    """A pixel can pass the luma>=8 near-black floor yet still quantize to
    the all-zero histogram bucket (e.g. (14,14,14)); a big dim trail full
    of such pixels must not out-vote a small bright accent and get
    reported back as '#000000' -- that value is reserved for the
    genuinely-all-black case."""
    frame = np.zeros((1000, 3), dtype=np.uint8)
    frame[:900] = (14, 14, 14)     # passes near-black floor, quantizes to 0
    frame[900:] = (255, 221, 0)    # bright yellow accent, few pixels
    dom = _dominant(frame)
    assert "#000000" not in dom


def test_dominant_returns_top_2():
    frame = np.zeros((100, 3), dtype=np.uint8)
    frame[:50] = (255, 0, 0)
    frame[50:] = (0, 0, 255)
    dom = _dominant(frame)
    assert len(dom) == 2


# -- strobe phase ------------------------------------------------------

def test_strobe_phase_null_when_inactive():
    scene = SceneInstance({"name": "still", "layers": [{"type": "solid", "color": "#ffffff"}]},
                          100, seed=1)
    scene.render(1 / 30)
    frame = np.zeros((100, 3), dtype=np.uint8)
    pkt = build_semantic_packet(0, scene, frame)
    assert pkt["stats"]["strobe_phase"] is None


def test_strobe_phase_present_when_active():
    scene_dict = {"name": "flash",
                 "layers": [{"type": "solid", "color": "#ffffff"}],
                 "master": {"strobe": {"mode": "strobe", "rate_hz": 2.0}}}
    scene = SceneInstance(scene_dict, 100, seed=1)
    scene.render(1 / 30)
    frame = np.zeros((100, 3), dtype=np.uint8)
    pkt = build_semantic_packet(0, scene, frame)
    phase = pkt["stats"]["strobe_phase"]
    assert phase is not None
    assert 0.0 <= phase < 1.0


# -- projector block: validation + verbatim passthrough ---------------------

def test_projector_object_accepted():
    scene = {"name": "p", "layers": [{"type": "solid", "color": "#000000"}],
            "projector": {"style": "react", "energy": 0.4}}
    validate_scene(scene)  # does not raise


@pytest.mark.parametrize("bad", ["react", ["react"], 5, True])
def test_projector_non_object_rejected(bad):
    scene = {"name": "p", "layers": [{"type": "solid", "color": "#000000"}],
            "projector": bad}
    with pytest.raises(SceneValidationError):
        validate_scene(scene)


def test_projector_null_is_fine():
    scene = {"name": "p", "layers": [{"type": "solid", "color": "#000000"}],
            "projector": None}
    validate_scene(scene)


def test_projector_missing_key_is_fine():
    scene = {"name": "p", "layers": [{"type": "solid", "color": "#000000"}]}
    validate_scene(scene)


def test_post_scene_rejects_non_object_projector():
    eng = Engine(mode="headless")
    c = TestClient(create_app(eng))
    r = c.post("/scene", json={"name": "p",
                               "layers": [{"type": "solid", "color": "#000000"}],
                               "projector": "react"})
    assert r.status_code == 422


def test_post_scene_accepts_object_projector_and_get_scene_returns_it_verbatim():
    eng = Engine(mode="headless")
    c = TestClient(create_app(eng))
    projector = {"style": "bleed", "energy": 0.8, "palette": ["#ff0000"]}
    r = c.post("/scene", json={"name": "p",
                               "layers": [{"type": "solid", "color": "#000000"}],
                               "projector": projector})
    assert r.status_code == 200
    r = c.get("/scene")
    assert r.json()["scene"]["projector"] == projector


def test_semantic_packet_forwards_projector_verbatim():
    projector = {"style": "react", "energy": 0.3, "palette": ["#123456"], "brightness": 0.5}
    scene_dict = dict(comet_scene())
    scene_dict["projector"] = projector
    scene = SceneInstance(scene_dict, 200, seed=1)
    scene.render(1 / 30)
    frame = np.zeros((200, 3), dtype=np.uint8)
    pkt = build_semantic_packet(0, scene, frame)
    assert pkt["projector"] == projector


def test_semantic_packet_projector_null_when_scene_has_none():
    scene = SceneInstance(comet_scene(), 200, seed=1)
    scene.render(1 / 30)
    frame = np.zeros((200, 3), dtype=np.uint8)
    pkt = build_semantic_packet(0, scene, frame)
    assert pkt["projector"] is None


# -- 10Hz cadence: every SEMANTIC_FRAME_DIVISOR-th engine frame -------------

def test_semantic_sent_every_third_frame():
    assert SEMANTIC_FRAME_DIVISOR == 3
    tm = TapManager(ring_len=200)
    tm._sock = FakeTapSocket()
    tm.set_tap("gm", host="1.2.3.4", port=9999, semantic_port=21326, count=200)
    scene = SceneInstance(comet_scene(n=200), 200, seed=1)
    frame = np.zeros((200, 3), dtype=np.uint8)
    for _ in range(9):
        scene.render(1 / 30)
        tm.emit(frame, scene=scene)
    semantic_pkts = [a for a in tm._sock.sent if a[0] == ("1.2.3.4", 21326)]
    assert len(semantic_pkts) == 3


def test_pixel_and_semantic_streams_go_to_different_ports():
    tm = TapManager(ring_len=200)
    tm._sock = FakeTapSocket()
    tm.set_tap("gm", host="1.2.3.4", port=9999, semantic_port=21326, count=200)
    scene = SceneInstance(comet_scene(n=200), 200, seed=1)
    frame = np.zeros((200, 3), dtype=np.uint8)
    scene.render(1 / 30)
    scene.render(1 / 30)
    scene.render(1 / 30)  # 3rd frame: semantic fires
    tm.emit(frame, scene=scene)
    tm.emit(frame, scene=scene)
    tm.emit(frame, scene=scene)
    ports = {addr[1] for addr, _ in tm._sock.sent}
    assert 9999 in ports  # pixel chunks
    assert 21326 in ports  # semantic


def test_semantic_packets_are_valid_json_matching_contract():
    tm = TapManager(ring_len=200)
    tm._sock = FakeTapSocket()
    tm.set_tap("gm", host="1.2.3.4", port=9999, semantic_port=21326, count=200)
    scene = SceneInstance(comet_scene(n=200), 200, seed=1)
    frame = np.zeros((200, 3), dtype=np.uint8)
    for _ in range(3):
        scene.render(1 / 30)
        tm.emit(frame, scene=scene)
    sem = [pkt for addr, pkt in tm._sock.sent if addr == ("1.2.3.4", 21326)]
    assert len(sem) == 1
    data = json.loads(sem[0])
    assert data["v"] == 1
    assert "seq" in data
    assert data["scene"] == "comet"


def test_seq_increments_once_per_semantic_frame_shared_across_taps():
    tm = TapManager(ring_len=200)
    tm._sock = FakeTapSocket()
    tm.set_tap("a", host="1.1.1.1", port=1, semantic_port=100, count=200)
    tm.set_tap("b", host="2.2.2.2", port=2, semantic_port=200, count=200)
    scene = SceneInstance(comet_scene(n=200), 200, seed=1)
    frame = np.zeros((200, 3), dtype=np.uint8)
    for _ in range(6):
        scene.render(1 / 30)
        tm.emit(frame, scene=scene)
    a_pkts = [json.loads(pkt) for addr, pkt in tm._sock.sent if addr[1] == 100]
    b_pkts = [json.loads(pkt) for addr, pkt in tm._sock.sent if addr[1] == 200]
    assert [p["seq"] for p in a_pkts] == [p["seq"] for p in b_pkts]
    assert len(a_pkts) == 2


# -- disabled / expired / no-semantic-port: silence ----------------------

def test_tap_without_semantic_port_sends_no_semantic_packets():
    tm = TapManager(ring_len=200)
    tm._sock = FakeTapSocket()
    tm.set_tap("gm", host="1.2.3.4", port=9999, count=200)  # no semantic_port
    scene = SceneInstance(comet_scene(n=200), 200, seed=1)
    frame = np.zeros((200, 3), dtype=np.uint8)
    for _ in range(6):
        scene.render(1 / 30)
        tm.emit(frame, scene=scene)
    assert all(addr[1] == 9999 for addr, _ in tm._sock.sent)


def test_disabled_tap_sends_no_semantic_packets():
    tm = TapManager(ring_len=200)
    tm._sock = FakeTapSocket()
    tm.set_tap("gm", host="1.2.3.4", port=9999, semantic_port=21326, count=200)
    tm.disable("gm")
    scene = SceneInstance(comet_scene(n=200), 200, seed=1)
    frame = np.zeros((200, 3), dtype=np.uint8)
    for _ in range(6):
        scene.render(1 / 30)
        tm.emit(frame, scene=scene)
    assert tm._sock.sent == []


def test_expired_tap_sends_no_semantic_packets(monkeypatch):
    tm = TapManager(ring_len=200)
    tm._sock = FakeTapSocket()
    fake_now = [1000.0]
    monkeypatch.setattr("lightserver.taps.time.monotonic", lambda: fake_now[0])
    tm.set_tap("gm", host="1.2.3.4", port=9999, semantic_port=21326, ttl=5, count=200)
    fake_now[0] += 10  # past ttl
    scene = SceneInstance(comet_scene(n=200), 200, seed=1)
    frame = np.zeros((200, 3), dtype=np.uint8)
    for _ in range(6):
        scene.render(1 / 30)
        tm.emit(frame, scene=scene)
    assert tm._sock.sent == []
    assert tm.status_one("gm")["enabled"] is False


def test_emit_without_scene_arg_sends_no_semantic_packets():
    """Backward compatibility: emit(frame) with no scene (existing callers/
    tests) never attempts to build/send a semantic packet."""
    tm = TapManager(ring_len=200)
    tm._sock = FakeTapSocket()
    tm.set_tap("gm", host="1.2.3.4", port=9999, semantic_port=21326, count=200)
    frame = np.zeros((200, 3), dtype=np.uint8)
    for _ in range(6):
        tm.emit(frame)  # no scene kwarg
    assert all(addr[1] == 9999 for addr, _ in tm._sock.sent)


# -- semantic_port validation ------------------------------------------

@pytest.mark.parametrize("semantic_port", [0, -1, 65536])
def test_bad_semantic_port_raises(semantic_port):
    tm = TapManager(ring_len=200)
    with pytest.raises(ValueError):
        tm.set_tap("t", host="1.2.3.4", port=9999, semantic_port=semantic_port)


def test_post_taps_bad_semantic_port_is_422():
    m = RingManager.from_configs(CONFIGS, make_segments(), mode="headless")
    m.ring.taps._sock = FakeTapSocket()
    c = TestClient(create_app(m.default, m), client=("10.20.30.40", 54321))
    r = c.post("/taps/gormapper", json={"port": 21325, "semantic_port": 99999})
    assert r.status_code == 422
    m.shutdown()


# -- semantic_port persistence -------------------------------------------

def test_semantic_port_round_trips_config_but_omitted_when_unset():
    tm = TapManager(ring_len=200)
    tm.set_tap("t", host="1.2.3.4", port=9999, semantic_port=21326)
    cfg = tm.config_for_persist()
    assert cfg["t"]["semantic_port"] == 21326

    tm_no_sem = TapManager(ring_len=200)
    tm_no_sem.set_tap("t2", host="1.2.3.4", port=9999)
    cfg2 = tm_no_sem.config_for_persist()
    assert "semantic_port" not in cfg2["t2"]  # backward-compatible persisted shape


def test_semantic_port_survives_restart_disabled(tmp_path):
    store = StateStore(tmp_path / "state.json")
    m = RingManager.from_configs(CONFIGS, make_segments(), mode="headless")
    m.store = store
    m.ring.taps.set_tap("gormapper", host="5.5.5.5", port=21325, semantic_port=21326)
    m.persist()
    m.shutdown()

    raw = json.loads((tmp_path / "state.json").read_text())
    assert raw["taps"]["gormapper"]["semantic_port"] == 21326

    m2 = RingManager.from_configs(CONFIGS, make_segments(), mode="headless")
    m2.restore(store)
    status = m2.ring.taps.status_one("gormapper")
    assert status["semantic_port"] == 21326
    assert status["enabled"] is False
    m2.shutdown()


def test_post_taps_sets_semantic_port_and_status_reports_it():
    m = RingManager.from_configs(CONFIGS, make_segments(), mode="headless")
    m.ring.taps._sock = FakeTapSocket()
    c = TestClient(create_app(m.default, m), client=("10.20.30.40", 54321))
    r = c.post("/taps/gormapper", json={"port": 21325, "semantic_port": 21326})
    assert r.status_code == 200
    assert r.json()["tap"]["semantic_port"] == 21326
    tap = next(t for t in c.get("/taps").json() if t["name"] == "gormapper")
    assert tap["semantic_port"] == 21326
    m.shutdown()


# -- RingEngine end-to-end: real render loop drives semantic packets -------

def test_ring_engine_step_emits_semantic_packets_for_particle_scene():
    eng = RingEngine(make_segments(), CONFIGS, mode="headless", seed=1)
    eng.taps._sock = FakeTapSocket()
    eng.taps.set_tap("gm", host="1.2.3.4", port=9999, semantic_port=21326)
    eng.set_scene({
        "name": "comet",
        "layers": [{"type": "particles", "physics": {"wrap": True},
                    "particles": [{"pos": 10.0, "vel": 420.0, "size": 3.0,
                                  "color": "#ffdd00", "lifetime": None}]}],
    }, crossfade=0)
    for _ in range(6):
        eng.step()
    sem = [json.loads(pkt) for addr, pkt in eng.taps._sock.sent if addr[1] == 21326]
    assert len(sem) == 2
    assert sem[0]["scene"] == "comet"
    assert len(sem[0]["tracks"]) == 1
    track = sem[0]["tracks"][0]
    assert track["vel"] == pytest.approx(420.0, rel=0.05)
    eng.shutdown()
