"""API surface: /pixels exactness, /scene, /state, /stop, /prompt fallback."""
import json
from pathlib import Path

import pytest
from fastapi.testclient import TestClient

from lightserver import nl as nl_mod
from lightserver.api import create_app
from lightserver.engine import Engine

SCENES_DIR = Path(__file__).resolve().parent.parent / "scenes"


@pytest.fixture()
def eng():
    return Engine(mode="headless")


@pytest.fixture()
def client(eng):
    return TestClient(create_app(eng))


def settle(eng, frames=40):
    for _ in range(frames):
        frame = eng.step()
    return frame


def test_pixels_endpoint_sets_exact_values(client, eng):
    r = client.post("/pixels", json={"pixels": {"10": "#a1b2c3", "0": "#ffffff",
                                                "799": "#010203"}})
    assert r.status_code == 200
    frame = settle(eng)
    assert tuple(frame[10]) == (0xA1, 0xB2, 0xC3)
    assert tuple(frame[0]) == (0xFF, 0xFF, 0xFF)
    assert tuple(frame[799]) == (0x01, 0x02, 0x03)
    assert tuple(frame[11]) == (0, 0, 0)


def test_pixels_merge_onto_current(client, eng):
    client.post("/pixels", json={"pixels": {"1": "#ff0000"}})
    settle(eng)
    client.post("/pixels", json={"pixels": {"2": "#00ff00"}, "base": "current"})
    frame = settle(eng)
    assert tuple(frame[1]) == (255, 0, 0)   # earlier pixel survives the merge
    assert tuple(frame[2]) == (0, 255, 0)


def test_pixels_rejects_out_of_range(client):
    r = client.post("/pixels", json={"pixels": {"800": "#ffffff"}})
    assert r.status_code == 422


def test_scene_endpoint_accepts_examples(client, eng):
    scene = json.loads((SCENES_DIR / "archive" / "aurora.json").read_text())
    r = client.post("/scene", json=scene)
    assert r.status_code == 200
    assert eng.current_scene_name == "aurora"


def test_scene_endpoint_rejects_invalid(client):
    r = client.post("/scene", json={"layers": [{"type": "nope"}]})
    assert r.status_code == 422
    assert "type" in r.json()["detail"]


def test_get_scene_returns_the_live_scene_verbatim(client, eng):
    scene = json.loads((SCENES_DIR / "archive" / "aurora.json").read_text())
    client.post("/scene", json=scene)
    r = client.get("/scene")
    assert r.status_code == 200
    body = r.json()
    assert body["scene"] == scene
    assert body["node"]


def test_state_reports_scene_and_fps(client, eng):
    client.post("/scene", json={"name": "s", "layers": [{"type": "solid", "color": "#102030"}]})
    eng.step()
    s = client.get("/state").json()
    assert s["scene"] == "s"
    assert s["fps_target"] == 30
    assert s["num_pixels"] == 800
    assert s["frame_count"] >= 1
    assert s["streaming"] is False  # headless engine never streams UDP


def test_stop_and_resume(client, eng):
    r = client.post("/stop")
    assert r.status_code == 200 and eng.streaming is False
    r = client.post("/resume").json()
    assert r["ok"] is True


def test_prompt_falls_back_without_claude(client, eng, monkeypatch):
    monkeypatch.setattr(nl_mod, "find_claude", lambda: None)
    r = client.post("/prompt", json={"text": "slow purple rain that pools at the bottom"})
    assert r.status_code == 200
    body = r.json()
    assert body["source"] == "fallback"
    assert body["scene"]["layers"]  # a real, valid scene was applied
    frame = settle(eng)
    assert frame.max() > 0


def test_scenes_listing(client):
    # live scenes dir holds just the default; examples live in scenes/archive
    r = client.get("/scenes").json()
    assert "default" in r
    assert "twin-pulse" not in r and "rain" not in r


def test_scenes_listing_includes_sync_pulse(client):
    r = client.get("/scenes").json()
    assert "sync-pulse" in r


def test_time_endpoint_shape(client):
    r = client.get("/time")
    assert r.status_code == 200
    body = r.json()
    assert set(body) == {"unix", "monotonic"}
    assert isinstance(body["unix"], float)
    assert isinstance(body["monotonic"], float)


def test_time_endpoint_unix_is_fresh(client):
    import time as time_mod
    before = time_mod.time()
    body = client.get("/time").json()
    after = time_mod.time()
    assert before <= body["unix"] <= after


def test_time_endpoint_monotonic_increases(client):
    a = client.get("/time").json()["monotonic"]
    b = client.get("/time").json()["monotonic"]
    assert b >= a
