"""Multi-node: registry load, manager targeting, node-aware API, offline
nodes never crash the engine."""
import json

import pytest
from fastapi.testclient import TestClient

from lightserver import nl as nl_mod
from lightserver.api import create_app
from lightserver.engine import Engine
from lightserver.nodes import NodeConfig, NodeManager, load_node_configs
from lightserver.protocol import UdpSender


# -- config loading --------------------------------------------------------

def test_load_node_configs_from_file(tmp_path):
    p = tmp_path / "nodes.json"
    p.write_text(json.dumps({"nodes": [
        {"name": "a", "host": "a.local", "ip": "10.0.0.1", "pixel_count": 800},
        {"name": "b", "host": "b.local", "ip": None, "pixel_count": 60},
        {"name": "c", "host": "c.local", "pixel_count": 60, "enabled": False},
    ]}))
    cfgs = load_node_configs(p)
    assert [c.name for c in cfgs] == ["a", "b", "c"]
    assert cfgs[0].pixel_count == 800 and cfgs[0].ip == "10.0.0.1"
    assert cfgs[1].ip is None and cfgs[1].enabled is True
    assert cfgs[2].enabled is False


def test_load_node_configs_missing_file_falls_back_to_node1(tmp_path):
    cfgs = load_node_configs(tmp_path / "nope.json")
    assert len(cfgs) == 1
    c = cfgs[0]
    assert c.name == "fcob-strip" and c.ip == "192.168.8.168"
    assert c.pixel_count == 800 and c.enabled


def test_repo_nodes_json_node1_unchanged():
    cfgs = load_node_configs()  # the real shipped nodes.json
    assert cfgs[0].name == "fcob-strip"
    assert cfgs[0].ip == "192.168.8.168" and cfgs[0].pixel_count == 800
    names = [c.name for c in cfgs]
    assert "fcob-strip2" in names and "fcob-strip3" in names


# -- manager ---------------------------------------------------------------

def make_manager():
    cfgs = [
        NodeConfig(name="fcob-strip", ip="127.0.0.1", pixel_count=800),
        NodeConfig(name="fcob-strip2", host="definitely-not-a-host-xyz.local",
                   pixel_count=60),
        NodeConfig(name="off", pixel_count=10, enabled=False),
    ]
    return NodeManager.from_configs(cfgs, mode="headless")


def test_manager_default_and_targeting():
    m = make_manager()
    assert m.default_name == "fcob-strip"
    assert m.get().n == 800
    assert m.get("fcob-strip2").n == 60
    assert [n for n, _ in m.targets(None)] == ["fcob-strip", "fcob-strip2"]
    assert [n for n, _ in m.targets("fcob-strip2")] == ["fcob-strip2"]
    with pytest.raises(KeyError):
        m.get("nope")
    with pytest.raises(KeyError):
        m.get("off")  # disabled: no engine


def test_manager_independent_scenes():
    m = make_manager()
    m.get("fcob-strip").set_scene(
        {"name": "red", "layers": [{"type": "solid", "color": "#ff0000"}]}, crossfade=0)
    m.get("fcob-strip2").set_scene(
        {"name": "green", "layers": [{"type": "solid", "color": "#00ff00"}]}, crossfade=0)
    f1 = m.get("fcob-strip").step()
    f2 = m.get("fcob-strip2").step()
    assert f1.shape == (800, 3) and tuple(f1[0]) == (255, 0, 0)
    assert f2.shape == (60, 3) and tuple(f2[0]) == (0, 255, 0)


def test_manager_status_shape():
    m = make_manager()
    st = m.status()
    by = {d["name"]: d for d in st}
    assert by["fcob-strip"]["default"] is True
    assert by["fcob-strip"]["pixel_count"] == 800
    assert by["off"]["enabled"] is False and by["off"]["online"] is False


# -- offline robustness ----------------------------------------------------

def test_sender_unresolvable_host_never_raises():
    s = UdpSender(None, 21324, host="definitely-not-a-host-xyz.local")
    s.send_frame(b"\x00" * 180)  # must not raise; frame dropped
    assert s.frames_sent == 0
    assert s.last_error and "resolve" in s.last_error
    s.close()


def test_sender_host_failure_falls_back_to_last_known_ip():
    s = UdpSender("127.0.0.1", 21324, host="definitely-not-a-host-xyz.local")
    s.send_frame(b"\x00" * 180)  # resolution fails -> falls back to 127.0.0.1
    assert s.frames_sent == 1
    s.close()


def test_udp_engine_with_offline_node_steps_fine():
    eng = Engine(num_pixels=60, mode="udp", ip="", port=21324,
                 host="definitely-not-a-host-xyz.local", name="fcob-strip2")
    eng.set_scene({"name": "s", "layers": [{"type": "solid", "color": "#123456"}]},
                  crossfade=0)
    for _ in range(3):
        eng.step()  # streaming=True, sends dropped, no exception
    assert eng.frame_count == 3
    eng.shutdown()


# -- API -------------------------------------------------------------------

@pytest.fixture()
def multi():
    m = make_manager()
    return m, TestClient(create_app(m.default, m))


def test_api_nodes_endpoint(multi):
    m, client = multi
    r = client.get("/nodes")
    assert r.status_code == 200
    body = r.json()
    assert body["default"] == "fcob-strip"
    names = [d["name"] for d in body["nodes"]]
    assert names == ["fcob-strip", "fcob-strip2", "off"]


def test_api_scene_no_node_applies_to_all_enabled(multi):
    m, client = multi
    r = client.post("/scene", json={"name": "s",
                                    "layers": [{"type": "solid", "color": "#102030"}]})
    assert r.status_code == 200
    assert sorted(r.json()["nodes"]) == ["fcob-strip", "fcob-strip2"]
    assert m.get("fcob-strip").current_scene_name == "s"
    assert m.get("fcob-strip2").current_scene_name == "s"


def test_api_scene_targets_one_node(multi):
    m, client = multi
    r = client.post("/scene?node=fcob-strip2",
                    json={"name": "only2", "layers": [{"type": "solid",
                                                       "color": "#ffffff"}]})
    assert r.status_code == 200 and r.json()["nodes"] == ["fcob-strip2"]
    assert m.get("fcob-strip2").current_scene_name == "only2"
    assert m.get("fcob-strip").current_scene_name != "only2"


def test_api_unknown_node_404(multi):
    _, client = multi
    assert client.post("/scene?node=ghost", json={"layers": []}).status_code == 404
    assert client.get("/state?node=ghost").status_code == 404
    assert client.post("/stop?node=ghost").status_code == 404


def test_api_pixels_targets_node_and_respects_its_range(multi):
    m, client = multi
    r = client.post("/pixels", json={"pixels": {"59": "#a1b2c3"},
                                     "node": "fcob-strip2"})
    assert r.status_code == 200 and r.json()["node"] == "fcob-strip2"
    r = client.post("/pixels", json={"pixels": {"60": "#a1b2c3"},
                                     "node": "fcob-strip2"})
    assert r.status_code == 422  # out of range for the 60px node
    r = client.post("/pixels", json={"pixels": {"799": "#a1b2c3"}})
    assert r.status_code == 200 and r.json()["node"] == "fcob-strip"


def test_api_prompt_accepts_vibe_and_node(multi, monkeypatch):
    m, client = multi
    monkeypatch.setattr(nl_mod, "find_claude", lambda: None)
    r = client.post("/prompt", json={"vibe": "calm blue ocean waves",
                                     "node": "fcob-strip2"})
    assert r.status_code == 200
    body = r.json()
    assert body["nodes"] == ["fcob-strip2"] and body["source"] == "fallback"


def test_api_stop_resume_per_node(multi):
    m, client = multi
    for eng in m.engines.values():
        eng.streaming = True
    r = client.post("/stop?node=fcob-strip2")
    assert r.status_code == 200 and r.json()["nodes"] == ["fcob-strip2"]
    assert m.get("fcob-strip2").streaming is False
    assert m.get("fcob-strip").streaming is True
    r = client.post("/stop")
    assert sorted(r.json()["nodes"]) == ["fcob-strip", "fcob-strip2"]
    assert m.get("fcob-strip").streaming is False


def test_api_state_includes_nodes_summary(multi):
    _, client = multi
    s = client.get("/state").json()
    assert s["num_pixels"] == 800  # default node stats, unchanged shape
    assert isinstance(s["nodes"], list) and len(s["nodes"]) == 3


def test_single_engine_backward_compat():
    eng = Engine(mode="headless")
    client = TestClient(create_app(eng))
    r = client.post("/scene", json={"name": "s",
                                    "layers": [{"type": "solid", "color": "#111111"}]})
    assert r.status_code == 200
    assert client.get("/nodes").json()["default"] == "fcob-strip"
