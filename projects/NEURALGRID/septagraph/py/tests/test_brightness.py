"""Tests for the global NeoTrellis brightness setting (App.set_brightness /
POST /api/brightness) -- NOT the LED frame transport path itself (see
test_led_transport.py, owned elsewhere): this only covers the one-shot
brightness command sent via link.send_brightness, its persistence in
app_state.json, and its exposure in get_state()/dispatch().
"""
from __future__ import annotations

import json

import pytest

from septagraph.pi.app import App, build_arg_parser


@pytest.fixture
def make_app(tmp_path, monkeypatch):
    monkeypatch.setattr("septagraph.pi.app.STATE_DIR", tmp_path)
    monkeypatch.setattr("septagraph.pi.app.STATE_PATH", tmp_path / "app_state.json")
    monkeypatch.delenv("NEURALGRID_BRIGHTNESS", raising=False)
    args = build_arg_parser().parse_args(["--fake-grid"])
    app = App(args)
    yield app
    app.stop()


def test_default_brightness_is_120(make_app):
    assert make_app.brightness == 120


def test_env_override_sets_initial_brightness(tmp_path, monkeypatch):
    monkeypatch.setattr("septagraph.pi.app.STATE_DIR", tmp_path)
    monkeypatch.setattr("septagraph.pi.app.STATE_PATH", tmp_path / "app_state.json")
    monkeypatch.setenv("NEURALGRID_BRIGHTNESS", "80")
    args = build_arg_parser().parse_args(["--fake-grid"])
    app = App(args)
    try:
        assert app.brightness == 80
    finally:
        app.stop()


def test_start_sends_brightness_to_link(make_app):
    app = make_app
    app.start()
    assert app.link.brightness == 120


def test_set_brightness_clamps_to_0_255(make_app):
    app = make_app
    app.set_brightness(999)
    assert app.brightness == 255
    assert app.link.brightness == 255
    app.set_brightness(-10)
    assert app.brightness == 0
    assert app.link.brightness == 0


def test_dispatch_brightness_updates_and_persists(make_app, tmp_path):
    app = make_app
    result = app.dispatch("brightness", {"level": 120})
    assert result == {"ok": True, "brightness": 120}
    assert app.brightness == 120
    assert app.link.brightness == 120
    saved = json.loads((tmp_path / "app_state.json").read_text())
    assert saved["brightness"] == 120


def test_saved_brightness_wins_over_env_on_next_load(tmp_path, monkeypatch):
    monkeypatch.setattr("septagraph.pi.app.STATE_DIR", tmp_path)
    state_path = tmp_path / "app_state.json"
    monkeypatch.setattr("septagraph.pi.app.STATE_PATH", state_path)
    monkeypatch.setenv("NEURALGRID_BRIGHTNESS", "80")
    state_path.write_text(json.dumps({"brightness": 15}))
    args = build_arg_parser().parse_args(["--fake-grid"])
    app = App(args)
    try:
        assert app.brightness == 15
    finally:
        app.stop()


def test_get_state_reports_brightness(make_app):
    app = make_app
    app.set_brightness(200)
    assert app.get_state()["brightness"] == 200
