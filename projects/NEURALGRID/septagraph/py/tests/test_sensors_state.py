"""Tests for the raw-sensor panel's backend state (App.get_sensors_state /
GET /api/state -> "sensors"): softpots, FSRs, lidars, LIDARDIAG, HELLO,
link, key events. Uses --fake-grid the same way test_led_transport.py does,
driving the App exactly as real hardware would via FakeGrid.simulate_*.
"""
from __future__ import annotations

import pytest

from septagraph.pi.app import App, FakeGrid, build_arg_parser


@pytest.fixture
def make_app(tmp_path, monkeypatch):
    monkeypatch.setattr("septagraph.pi.app.STATE_DIR", tmp_path)
    monkeypatch.setattr("septagraph.pi.app.STATE_PATH", tmp_path / "app_state.json")
    args = build_arg_parser().parse_args(["--fake-grid"])
    app = App(args)
    yield app
    app.stop()


def test_sensors_present_with_default_shape(make_app):
    app = make_app
    sensors = app.get_state()["sensors"]
    assert len(sensors["softpots"]) == 4
    assert len(sensors["fsrs"]) == 4
    assert len(sensors["lidars"]) == 4
    assert sensors["lidar_diag"] is None
    assert sensors["hello"] is None
    assert sensors["hello_age_s"] is None
    assert sensors["link_connected"] is True
    assert sensors["key_events"] == 0


def test_analog_event_populates_softpots_and_fsrs(make_app):
    app = make_app
    link: FakeGrid = app.link
    # N, S, E, W softpots + FSR0-3
    link.simulate_analog((100, 200, 300, 400, 4000, 0, 0, 0))

    sensors = app.get_state()["sensors"]
    softpots = {s["name"]: s["raw"] for s in sensors["softpots"]}
    assert softpots == {"N": 100, "S": 200, "E": 300, "W": 400}

    fsrs = {f["name"]: f for f in sensors["fsrs"]}
    assert fsrs["FSR0"]["raw"] == 4000
    assert 0.0 < fsrs["FSR0"]["value"] <= 1.0
    assert fsrs["FSR0"]["pressed"] is True  # well above the onset threshold
    assert fsrs["FSR1"]["raw"] == 0
    assert fsrs["FSR1"]["pressed"] is False


def test_lidar_event_reports_ok_and_invalid_channels(make_app):
    app = make_app
    link: FakeGrid = app.link
    LIDAR_INVALID = 0xFFFF
    link.simulate_lidar((500, LIDAR_INVALID, 1800, 100))

    sensors = app.get_state()["sensors"]
    lidars = {l["name"]: l for l in sensors["lidars"]}
    assert lidars["N"]["mm"] == 500
    assert lidars["N"]["ok"] is True
    assert lidars["N"]["age_s"] == 0.0

    assert lidars["S"]["mm"] is None
    assert lidars["S"]["ok"] is False
    assert lidars["S"]["age_s"] is None  # never seen a valid reading yet

    assert lidars["E"]["mm"] == 1800
    assert lidars["W"]["mm"] == 100


def test_lidar_channel_keeps_age_of_last_valid_after_going_invalid(make_app):
    app = make_app
    link: FakeGrid = app.link
    LIDAR_INVALID = 0xFFFF
    link.simulate_lidar((500, 500, 500, 500))
    link.simulate_lidar((LIDAR_INVALID, 500, 500, 500))

    sensors = app.get_state()["sensors"]
    lidars = {l["name"]: l for l in sensors["lidars"]}
    assert lidars["N"]["ok"] is False
    assert lidars["N"]["mm"] is None
    assert lidars["N"]["age_s"] is not None  # was valid a moment ago


def test_hello_event_populates_link_fields(make_app):
    app = make_app
    link: FakeGrid = app.link
    link.simulate_hello(proto_ver=1, grid_w=8, grid_h=8, n_analog=8, fw_ver=6)

    sensors = app.get_state()["sensors"]
    assert sensors["hello"] == {
        "proto_ver": 1, "grid_w": 8, "grid_h": 8, "n_analog": 8, "fw_ver": 6,
    }
    assert sensors["hello_age_s"] is not None


def test_key_events_counted(make_app):
    app = make_app
    link: FakeGrid = app.link
    link.simulate_key(0, 0, 1)
    link.simulate_key(0, 0, 0)
    link.simulate_key(1, 1, 1)

    sensors = app.get_state()["sensors"]
    assert sensors["key_events"] == 3


def test_lidardiag_event_populates_summary(make_app):
    app = make_app
    link: FakeGrid = app.link
    link.simulate_lidardiag(
        responded_alone=(True, True, False, True),
        zombie=False,
        ack_addrs=(0x29, 0x2A),
        fail_stage=(0, 0, 2, 0),
    )

    sensors = app.get_state()["sensors"]
    diag = sensors["lidar_diag"]
    assert diag["responded_alone"] == [True, True, False, True]
    assert diag["zombie"] is False
    assert diag["ack_addrs"] == ["0x29", "0x2A"]
    assert diag["fail_stage"][2] == "init failed (no ACK / model-ID / osc-cal)"
