"""nl.py robustness: JSON extraction, validation retry, keyword fallback."""
import json

import pytest

from lightserver import nl


def test_extract_json_plain():
    assert nl._extract_json('{"a": 1}') == {"a": 1}


def test_extract_json_with_prose_and_fences():
    text = 'Sure! Here is your scene:\n```json\n{"name": "x", "layers": [{"type": "solid"}]}\n```\nEnjoy!'
    assert nl._extract_json(text)["name"] == "x"


def test_extract_json_nested_braces_in_strings():
    text = 'noise {"name": "br{ace}", "layers": [{"type": "solid"}]} trailing'
    assert nl._extract_json(text)["name"] == "br{ace}"


def test_extract_json_no_object_raises():
    with pytest.raises(ValueError):
        nl._extract_json("no json here at all")


def test_fallback_matches_rain_keywords():
    scene = nl._fallback_scene("gentle rain drizzle")
    assert "rain" in scene["name"]


def test_fallback_matches_comet():
    scene = nl._fallback_scene("a comet bouncing back and forth")
    assert "comet" in scene["name"]


def test_generate_scene_without_claude_never_raises(monkeypatch):
    monkeypatch.setattr(nl, "find_claude", lambda: None)
    scene, source = nl.generate_scene("total nonsense zzzz qqq")
    assert source == "fallback"
    from lightserver.scene import validate_scene
    validate_scene(scene)


def test_generate_scene_retries_then_falls_back(monkeypatch):
    calls = []

    def bad_claude(prompt, claude_bin):
        calls.append(prompt)
        return "this is not json"

    monkeypatch.setattr(nl, "find_claude", lambda: "/fake/claude")
    monkeypatch.setattr(nl, "_run_claude", bad_claude)
    scene, source = nl.generate_scene("purple waves")
    assert len(calls) == 2  # initial + one retry with the error fed back
    assert source == "fallback"
    assert "previous attempt was rejected" in calls[1]


def test_generate_scene_accepts_good_llm_output(monkeypatch):
    good = {"name": "llm", "layers": [{"type": "solid", "color": "#123456"}]}

    monkeypatch.setattr(nl, "find_claude", lambda: "/fake/claude")
    monkeypatch.setattr(nl, "_run_claude", lambda p, b: json.dumps(good))
    scene, source = nl.generate_scene("whatever")
    assert source == "llm"
    assert scene["name"] == "llm"
