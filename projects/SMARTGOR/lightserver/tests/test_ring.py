"""Ring topology: config, mapping/wraparound math, segment slicing, the
orbiting orb crossing strip boundaries, ring-only API, persistence."""
import json
from pathlib import Path

import numpy as np
import pytest
from fastapi.testclient import TestClient

from lightserver import nl as nl_mod
from lightserver.api import create_app
from lightserver.nodes import NodeConfig
from lightserver.persist import StateStore
from lightserver.physics import ParticleSystem
from lightserver.ring import (RingConfigError, RingEngine, RingManager,
                              RingSegment, load_ring_segments, ring_length)

ROOT = Path(__file__).resolve().parent.parent
SCENES_DIR = ROOT / "scenes"

CONFIGS = [
    NodeConfig(name="fcob-strip", host="fcob-strip.local", pixel_count=800),
    NodeConfig(name="fcob-strip2", host="fcob-strip2.local", pixel_count=480),
    NodeConfig(name="fcob-strip3", host="fcob-strip3.local", pixel_count=640),
]


def make_segments():
    return [RingSegment("fcob-strip", 0, 800),
            RingSegment("fcob-strip2", 800, 480),
            RingSegment("fcob-strip3", 1280, 640)]


# -- config ---------------------------------------------------------------

def test_repo_ring_json_loads_and_matches_nodes_json():
    from lightserver.nodes import load_node_configs
    configs = load_node_configs(ROOT / "nodes.json")
    segs = load_ring_segments(ROOT / "ring.json", configs)
    assert [s.node for s in segs] == ["fcob-strip", "fcob-strip2", "fcob-strip3"]
    assert [s.ring_offset for s in segs] == [0, 800, 1280]
    assert ring_length(segs) == 1920


def test_ring_offsets_derived_when_absent(tmp_path):
    p = tmp_path / "ring.json"
    p.write_text(json.dumps({"segments": [
        {"node": "fcob-strip"}, {"node": "fcob-strip2"},
        {"node": "fcob-strip3"}]}))
    segs = load_ring_segments(p, CONFIGS)
    assert [(s.ring_offset, s.length) for s in segs] == [
        (0, 800), (800, 480), (1280, 640)]


@pytest.mark.parametrize("bad", [
    {"segments": []},
    {"segments": [{"node": "nope"}]},
    {"segments": [{"node": "fcob-strip", "length": 999}]},
    {"segments": [{"node": "fcob-strip", "ring_offset": 5}]},
    {"segments": [{"node": "fcob-strip"}, {"node": "fcob-strip"}]},
])
def test_ring_config_rejects_inconsistency(tmp_path, bad):
    p = tmp_path / "ring.json"
    p.write_text(json.dumps(bad))
    with pytest.raises(RingConfigError):
        load_ring_segments(p, CONFIGS)


# -- mapping + slicing ----------------------------------------------------

def ring_engine(mode="headless"):
    return RingEngine(make_segments(), CONFIGS, mode=mode, seed=1)


def test_ring_engine_resolution_is_derived():
    eng = ring_engine()
    assert eng.n == 1920


def test_segment_slicing_maps_ring_indices_to_strips():
    eng = ring_engine()
    # paint known ring indices at every boundary
    marks = {0: "#ffffff", 799: "#ff0000", 800: "#00ff00", 1279: "#0000ff",
             1280: "#ffff00", 1919: "#00ffff"}
    eng.set_scene({"name": "m", "layers": [{"type": "paint", "background": "#000000",
                   "pixels": {str(k): v for k, v in marks.items()}}]}, crossfade=0)
    frame = eng.render_frame()
    s1, s2, s3 = (eng.slice_segment(frame, seg) for seg in eng.segments)
    assert tuple(s1[0]) == (255, 255, 255)     # ring 0    -> strip1 px 0
    assert tuple(s1[799]) == (255, 0, 0)       # ring 799  -> strip1 px 799
    assert tuple(s2[0]) == (0, 255, 0)         # ring 800  -> strip2 px 0
    assert tuple(s2[479]) == (0, 0, 255)       # ring 1279 -> strip2 px 479
    assert tuple(s3[0]) == (255, 255, 0)       # ring 1280 -> strip3 px 0
    assert tuple(s3[639]) == (0, 255, 255)     # ring 1919 -> strip3 px 639
    assert len(s1) == 800 and len(s2) == 480 and len(s3) == 640


def test_reversed_segment_is_flipped():
    segs = [RingSegment("fcob-strip", 0, 800),
            RingSegment("fcob-strip2", 800, 480, reversed=True),
            RingSegment("fcob-strip3", 1280, 640)]
    eng = RingEngine(segs, CONFIGS, mode="headless")
    eng.set_scene({"name": "m", "layers": [{"type": "paint", "background": "#000000",
                   "pixels": {"800": "#ff0000"}}]}, crossfade=0)
    frame = eng.render_frame()
    s2 = eng.slice_segment(frame, segs[1])
    assert tuple(s2[479]) == (255, 0, 0)  # ring 800 lands on flipped strip end


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


def test_emit_dispatches_correct_slice_lengths_per_node():
    eng = ring_engine(mode="udp")
    fakes = {}
    for name in eng.senders:
        eng.senders[name].close()
        fakes[name] = eng.senders[name] = FakeSender()
    eng.streaming = True
    eng.step()
    assert len(fakes["fcob-strip"].frames[0]) == 800 * 3
    assert len(fakes["fcob-strip2"].frames[0]) == 480 * 3
    assert len(fakes["fcob-strip3"].frames[0]) == 640 * 3
    eng.shutdown()


def test_offline_node_does_not_affect_rest_of_ring():
    eng = ring_engine(mode="udp")
    for name in list(eng.senders):
        eng.senders[name].close()
        eng.senders[name] = FakeSender()
    eng.senders["fcob-strip2"] = None  # node vanished entirely
    eng.streaming = True
    eng.step()  # must not raise
    assert eng.senders["fcob-strip"].frames_sent == 1
    assert eng.senders["fcob-strip3"].frames_sent == 1
    eng.shutdown()


# -- wrap physics + the orb ----------------------------------------------

def test_wrap_particle_position_wraps_modulo_ring():
    ps = ParticleSystem(1920, physics={"wrap": True},
                        initial=[{"pos": 1900, "vel": 100}])
    for _ in range(30):  # 1s at 30fps -> pos 2000 -> wraps to 80
        ps.step(1 / 30, 0.0)
    assert ps.count == 1
    assert abs(ps.pos[0] - 80.0) < 1.0


def test_wrap_splat_crosses_the_seam():
    ps = ParticleSystem(1920, physics={"wrap": True},
                        initial=[{"pos": 1919.5, "vel": 0, "size": 3.0}])
    ps.step(1 / 30, 0.0)
    out = np.zeros((1920, 3), dtype=np.float32)
    ps.render(out)
    assert out[1919].max() > 0 and out[0].max() > 0  # lit on BOTH sides of seam
    assert out[960].max() == 0


def test_orb_drive_modulates_speed_and_reverses_smoothly():
    """The default orb: top speed 128 px/s (lap/15s), wandering down toward
    42.7 px/s (lap/45s), smooth direction reversals ~1.5/min."""
    ps = ParticleSystem(
        1920,
        physics={"wrap": True,
                 "drive": {"top_speed": 128, "min_speed": 42.7,
                           "reverses_per_min": 1.5, "smooth_s": 2.5,
                           "mod_period_s": 37, "mod_period2_s": 23}},
        initial=[{"pos": 0, "vel": 128}], seed=7)
    dt = 1 / 30
    vels, positions = [], []
    for k in range(150 * 30):  # 150 s
        ps.step(dt, k * dt)
        vels.append(float(ps.vel[0]))
        positions.append(float(ps.pos[0]))
    speeds = [abs(v) for v in vels]
    assert max(speeds) <= 132          # never exceeds top speed
    assert max(speeds) >= 115          # actually reaches ~top speed
    # reverses occasionally: ~1.5/min -> expect a handful in 150s
    flips = sum(1 for a, b in zip(vels, vels[1:]) if a * b < 0)
    assert 1 <= flips <= 8
    # smooth: velocity never steps more than the chase limit allows
    dv = max(abs(b - a) for a, b in zip(vels, vels[1:]))
    assert dv < (128 + 132) * (dt / 2.5) + 1e-6
    # position never teleports (except the seam wrap)
    jumps = [abs(b - a) for a, b in zip(positions, positions[1:])]
    assert all(j <= 132 * dt + 1e-6 or j > 1900 for j in jumps)


def test_boost_orb_is_relative_brightening_over_the_bed():
    """blend=boost, opacity 0.5: center is 50% brighter than the local bed
    (in linear light), fading to no-op at the edges, ~20 LEDs visible."""
    eng = ring_engine()
    eng.set_scene({"name": "b", "layers": [
        {"type": "solid", "color": "#404040"},
        {"type": "particles", "blend": "boost", "opacity": 0.5,
         "physics": {"wrap": True, "bounce": {"enabled": False}},
         "particles": [{"pos": 960, "vel": 0, "color": "#ffffff",
                        "size": 5.0, "lifetime": None}],
         "render": {"falloff": "gaussian", "fade_out": False}}]},
        crossfade=0)
    eng.render_frame()
    hdr = eng.last_hdr.sum(axis=1)
    bed = hdr[100]
    assert bed > 0
    assert abs(hdr[960] / bed - 1.5) < 0.02       # +50% at the center
    assert abs(hdr[1800] / bed - 1.0) < 0.01      # no-op far away
    visible = int(np.count_nonzero(hdr > bed * 1.05))
    assert 14 <= visible <= 28                    # ~20 LEDs long


def test_default_scene_validates_and_orb_crosses_boundaries():
    from lightserver.scene import validate_scene
    scene = json.loads((SCENES_DIR / "default.json").read_text())
    validate_scene(scene)
    assert scene["name"] == "default"
    eng = RingEngine(make_segments(), CONFIGS, mode="headless", seed=3)
    eng.set_scene(scene, crossfade=0)
    orb = eng._scene.layers[2].system
    seen = set()
    for _ in range(90 * 30):  # 90 s
        eng.render_frame()
        p = orb.pos[0]
        assert 0 <= p < 1920
        seen.add(int(p) // 640)  # which third of the ring
    frame = eng.render_frame()
    assert frame.max() > 0
    assert len(seen) >= 2  # travelled across strip boundaries


# -- ring-only API --------------------------------------------------------

@pytest.fixture()
def rmanager():
    m = RingManager.from_configs(CONFIGS, make_segments(), mode="headless")
    yield m
    m.shutdown()


@pytest.fixture()
def rclient(rmanager):
    return TestClient(create_app(rmanager.default, rmanager))


def test_api_scene_applies_to_whole_ring(rclient, rmanager):
    r = rclient.post("/scene", json={"name": "s", "layers":
                                     [{"type": "solid", "color": "#102030"}]})
    assert r.status_code == 200
    assert r.json()["nodes"] == ["ring"]
    assert rmanager.ring.current_scene_name == "s"


def test_api_node_param_is_ignored_ring_is_atomic(rclient, rmanager, monkeypatch):
    monkeypatch.setattr(nl_mod, "find_claude", lambda: None)
    # node param on /prompt: accepted, ignored, still applies to the ring
    r = rclient.post("/prompt", json={"text": "purple", "node": "fcob-strip2"})
    assert r.status_code == 200
    assert r.json()["nodes"] == ["ring"]
    # /scene with ?node= also cannot split the ring
    r = rclient.post("/scene?node=fcob-strip3",
                     json={"name": "x", "layers": [{"type": "solid",
                                                    "color": "#010101"}]})
    assert r.status_code == 200 and r.json()["nodes"] == ["ring"]


def test_api_get_scene_reads_back_ring_scene(rclient, rmanager):
    scene = {"name": "readback", "layers": [{"type": "solid", "color": "#0a0b0c"}]}
    rclient.post("/scene", json=scene)
    r = rclient.get("/scene?node=fcob-strip2")  # node ignored, ring is atomic
    assert r.status_code == 200
    assert r.json()["scene"] == scene
    assert r.json()["node"] == "ring"


def test_api_pixels_use_ring_indices(rclient, rmanager):
    r = rclient.post("/pixels", json={"pixels": {"1900": "#ffffff"}})
    assert r.status_code == 200
    for _ in range(40):
        frame = rmanager.ring.step()
    assert tuple(frame[1900]) == (255, 255, 255)
    # strip3 receives it at local px 620 (1900 - 1280)
    s3 = rmanager.ring.slice_segment(frame, rmanager.segments[2])
    assert tuple(s3[620]) == (255, 255, 255)
    r = rclient.post("/pixels", json={"pixels": {"1920": "#ffffff"}})
    assert r.status_code == 422  # beyond ring length


def test_api_nodes_reports_ring_and_all_strips(rclient):
    body = rclient.get("/nodes").json()
    assert body["default"] == "ring"
    assert body["ring"]["length"] == 1920
    assert [s["ring_offset"] for s in body["ring"]["segments"]] == [0, 800, 1280]
    names = [n["name"] for n in body["nodes"]]
    assert names == ["fcob-strip", "fcob-strip2", "fcob-strip3"]


def test_api_debug_node_endpoint(rclient):
    r = rclient.get("/debug/node/fcob-strip2")
    assert r.status_code == 200
    assert r.json()["ring_offset"] == 800
    assert rclient.get("/debug/node/nope").status_code == 404


def test_api_state_reports_ring_resolution(rclient, rmanager):
    rmanager.ring.step()
    s = rclient.get("/state").json()
    assert s["num_pixels"] == 1920
    assert s["ring"]["length"] == 1920
    assert len(s["ring"]["segments"]) == 3


def test_api_stop_resume_whole_ring(rclient, rmanager):
    assert rclient.post("/stop?node=fcob-strip").status_code == 200
    assert rmanager.ring.stopped is True
    assert rclient.post("/resume").status_code == 200
    assert rmanager.ring.stopped is False


# -- persistence ----------------------------------------------------------

def test_ring_scene_persists_and_restores(tmp_path):
    store = StateStore(tmp_path / "state.json")
    m = RingManager.from_configs(CONFIGS, make_segments(), mode="headless")
    m.store = store
    scene = {"name": "keepme", "layers": [{"type": "solid", "color": "#123456"}]}
    m.ring.set_scene(scene, crossfade=0)
    m.persist()
    m.shutdown()
    saved = json.loads((tmp_path / "state.json").read_text())
    assert saved["nodes"]["ring"]["scene"]["name"] == "keepme"

    m2 = RingManager.from_configs(CONFIGS, make_segments(), mode="headless")
    m2.restore(store)
    assert m2.ring.current_scene_name == "keepme"
    m2.shutdown()


def test_old_per_node_state_is_ignored_boot_default_wins(tmp_path):
    """v1 per-strip state (pre-ring) migrates cleanly: entries are ignored,
    so the ring keeps its boot default (the 'default' scene)."""
    p = tmp_path / "state.json"
    p.write_text(json.dumps({"version": 1, "nodes": {
        "fcob-strip": {"scene": {"name": "old", "layers":
                                 [{"type": "solid", "color": "#111111"}]},
                       "streaming": True}}}))
    m = RingManager.from_configs(CONFIGS, make_segments(), mode="headless")
    boot = json.loads((SCENES_DIR / "default.json").read_text())
    m.set_scene_all(boot, crossfade=0)
    m.restore(StateStore(p))
    assert m.ring.current_scene_name == "default"
    m.shutdown()
