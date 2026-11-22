"""Scene persistence: StateStore atomicity/robustness, manager restore
semantics (persisted state beats the boot default, defaults when empty),
and API write-through so applied patterns stick across restarts."""
import json

from fastapi.testclient import TestClient

from lightserver.api import create_app
from lightserver.engine import Engine
from lightserver.nodes import NodeConfig, NodeManager
from lightserver.persist import StateStore

RED = {"name": "red", "layers": [{"type": "solid", "color": "#ff0000"}]}
BLUE = {"name": "blue", "layers": [{"type": "solid", "color": "#0000ff"}]}
DEFAULT = {"name": "default", "layers": [{"type": "solid", "color": "#111111"}]}


def make_manager():
    cfgs = [NodeConfig(name="fcob-strip", pixel_count=800),
            NodeConfig(name="fcob-strip2", pixel_count=640)]
    return NodeManager.from_configs(cfgs, mode="headless")


# -- StateStore ------------------------------------------------------------

def test_store_roundtrip(tmp_path):
    store = StateStore(tmp_path / "state.json")
    assert store.save({"a": {"scene": RED, "streaming": True},
                       "b": {"scene": BLUE, "streaming": False}})
    loaded = store.load()
    assert loaded["a"]["scene"] == RED and loaded["a"]["streaming"] is True
    assert loaded["b"]["scene"] == BLUE and loaded["b"]["streaming"] is False
    # file on disk is valid JSON with a version stamp (atomic replace landed)
    raw = json.loads((tmp_path / "state.json").read_text())
    assert raw["version"] == 1 and "nodes" in raw


def test_store_missing_file_is_empty(tmp_path):
    assert StateStore(tmp_path / "nope.json").load() == {}


def test_store_corrupt_file_is_empty(tmp_path):
    p = tmp_path / "state.json"
    p.write_text("{not json!!")
    assert StateStore(p).load() == {}
    p.write_text(json.dumps({"nodes": ["not", "a", "dict"]}))
    assert StateStore(p).load() == {}
    # bogus scene type for one node is filtered, the rest survives
    p.write_text(json.dumps({"nodes": {"a": {"scene": "nope"},
                                       "b": {"scene": RED}}}))
    loaded = StateStore(p).load()
    assert "a" not in loaded and loaded["b"]["scene"] == RED


def test_store_streaming_defaults_true(tmp_path):
    p = tmp_path / "state.json"
    p.write_text(json.dumps({"nodes": {"a": {"scene": RED}}}))
    assert StateStore(p).load()["a"]["streaming"] is True


# -- manager restore/persist ----------------------------------------------

def test_restore_overrides_boot_default_per_node(tmp_path):
    store = StateStore(tmp_path / "state.json")
    store.save({"fcob-strip": {"scene": RED, "streaming": True},
                "ghost-node": {"scene": BLUE, "streaming": True}})
    m = make_manager()
    m.set_scene_all(DEFAULT, crossfade=0)  # boot default (--scene)
    m.restore(store)
    assert m.get("fcob-strip").current_scene_name == "red"
    # no persisted entry -> keeps the boot default; ghost node is ignored
    assert m.get("fcob-strip2").current_scene_name == "default"


def test_restore_empty_store_keeps_defaults(tmp_path):
    m = make_manager()
    m.set_scene_all(DEFAULT, crossfade=0)
    m.restore(StateStore(tmp_path / "state.json"))  # no file
    for _, eng in m.targets(None):
        assert eng.current_scene_name == "default"


def test_restore_invalid_persisted_scene_keeps_default(tmp_path):
    store = StateStore(tmp_path / "state.json")
    store.save({"fcob-strip": {"scene": {"layers": "garbage"},
                               "streaming": True}})
    m = make_manager()
    m.set_scene_all(DEFAULT, crossfade=0)
    m.restore(store)
    assert m.get("fcob-strip").current_scene_name == "default"


def test_restore_applies_stopped_streaming(tmp_path):
    store = StateStore(tmp_path / "state.json")
    store.save({"fcob-strip": {"scene": RED, "streaming": False}})
    m = make_manager()
    m.restore(store)
    eng = m.get("fcob-strip")
    assert eng.current_scene_name == "red" and eng.stopped


def test_persist_snapshots_all_nodes(tmp_path):
    m = make_manager()
    m.store = StateStore(tmp_path / "state.json")
    m.get("fcob-strip").set_scene(RED, crossfade=0)
    m.get("fcob-strip2").set_scene(BLUE, crossfade=0)
    m.get("fcob-strip2").stop_streaming()
    m.persist()
    loaded = m.store.load()
    assert loaded["fcob-strip"] == {"scene": RED, "streaming": True}
    assert loaded["fcob-strip2"] == {"scene": BLUE, "streaming": False}


def test_persist_without_store_is_noop():
    make_manager().persist()  # must not raise


# -- explicit stop survives a run() (re)start ------------------------------

def test_run_does_not_resurrect_streaming_after_stop():
    eng = Engine(num_pixels=8, mode="udp", ip="127.0.0.1")
    try:
        eng.stop_streaming()
        eng.run(duration=0.05)
        assert eng.streaming is False and eng.stopped
        eng.resume_streaming()
        assert eng.streaming is True and not eng.stopped
    finally:
        eng.shutdown()


# -- API write-through -----------------------------------------------------

def api_client(tmp_path):
    m = make_manager()
    m.store = StateStore(tmp_path / "state.json")
    client = TestClient(create_app(m.default, m))
    return m, client


def test_api_scene_persists(tmp_path):
    m, client = api_client(tmp_path)
    r = client.post("/scene", json=RED)
    assert r.status_code == 200
    loaded = m.store.load()
    assert loaded["fcob-strip"]["scene"] == RED
    assert loaded["fcob-strip2"]["scene"] == RED  # no node -> all nodes
    # targeted scene only changes that node's persisted entry
    r = client.post("/scene", params={"node": "fcob-strip2"}, json=BLUE)
    assert r.status_code == 200
    loaded = m.store.load()
    assert loaded["fcob-strip"]["scene"] == RED
    assert loaded["fcob-strip2"]["scene"] == BLUE


def test_api_stop_resume_persist(tmp_path):
    m, client = api_client(tmp_path)
    client.post("/scene", json=RED)
    client.post("/stop", params={"node": "fcob-strip"})
    loaded = m.store.load()
    assert loaded["fcob-strip"]["streaming"] is False
    assert loaded["fcob-strip2"]["streaming"] is True
    client.post("/resume")
    assert m.store.load()["fcob-strip"]["streaming"] is True


def test_api_pixels_persist(tmp_path):
    m, client = api_client(tmp_path)
    r = client.post("/pixels", json={"pixels": {"3": "#a1b2c3"}})
    assert r.status_code == 200
    scene = m.store.load()["fcob-strip"]["scene"]
    assert scene["name"] == "paint"
    assert scene["layers"][0]["pixels"]["3"] == "#a1b2c3"


def test_restart_cycle_roundtrip(tmp_path):
    """Full cycle: apply via API -> 'restart' (new manager) -> restored."""
    m1, client = api_client(tmp_path)
    client.post("/scene", json=BLUE)
    m2 = make_manager()
    m2.set_scene_all(DEFAULT, crossfade=0)  # boot default
    m2.restore(StateStore(tmp_path / "state.json"))
    for _, eng in m2.targets(None):
        assert eng.current_scene_name == "blue"
        assert not eng.stopped
