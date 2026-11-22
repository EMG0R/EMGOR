"""Tests for the orientation layer (septagraph/pi/orientation.py -- pure
geometry) and its wiring into App (septagraph/pi/app.py -- the rotation
gesture, hw->logical dispatch, logical->hw LED send). See
orientation.py's module docstring for the coordinate/rot conventions
these tests exercise.
"""
from __future__ import annotations

import time

import pytest

from septagraph.pi import leds, orientation
from septagraph.pi.app import App, FakeGrid, build_arg_parser
from septagraph.pi.orientation import (
    EDGE_CORNERS,
    EDGE_TO_ROT,
    Orientation,
    VALID_ROTS,
    analog_hw_to_logical,
    edge_map,
    hw_to_logical,
    lidar_hw_to_logical,
    logical_to_hw,
    rotate_frame,
    rotate_partial,
)


# ---------------------------------------------------------------------------
# Pure transforms
# ---------------------------------------------------------------------------


@pytest.mark.parametrize("rot", VALID_ROTS)
def test_hw_logical_round_trip_all_64_cells(rot):
    for x in range(8):
        for y in range(8):
            lx, ly = hw_to_logical(x, y, rot)
            assert 0 <= lx < 8 and 0 <= ly < 8
            hx, hy = logical_to_hw(lx, ly, rot)
            assert (hx, hy) == (x, y)


def test_rot0_is_identity():
    for x in range(8):
        for y in range(8):
            assert hw_to_logical(x, y, 0) == (x, y)
            assert logical_to_hw(x, y, 0) == (x, y)


@pytest.mark.parametrize("rot", VALID_ROTS)
def test_hw_to_logical_is_a_bijection(rot):
    seen = set()
    for x in range(8):
        for y in range(8):
            p = hw_to_logical(x, y, rot)
            assert p not in seen
            seen.add(p)
    assert len(seen) == 64


@pytest.mark.parametrize(
    "edge_name,corner_a,corner_b",
    [(name, a, b) for name, (a, b) in EDGE_CORNERS.items()],
)
def test_holding_an_edges_corners_puts_them_at_logical_top(edge_name, corner_a, corner_b):
    """Definition per the task spec: "that edge becomes top" means the two
    held corners end up as logical (0,0)/(7,0) -- the logical TOP row's
    corners -- once rot is set to EDGE_TO_ROT[edge_name]."""
    rot = EDGE_TO_ROT[edge_name]
    la = hw_to_logical(*corner_a, rot)
    lb = hw_to_logical(*corner_b, rot)
    assert {la, lb} == {(0, 0), (7, 0)}


def test_rotate_frame_moves_a_marker_pixel():
    # rot=90 (left edge -> top): hw (0,0) [top-left] should show whatever
    # is at logical (7,0) [top-right] -- since hw_to_logical(0,0,90) =
    # (7,0), i.e. the hw top-left cell displays the logical (7,0) content.
    logical = leds.blank()
    logical[0 * 8 + 7] = (9, 8, 7)  # logical (7, 0)
    hw = rotate_frame(logical, 90)
    assert hw[0 * 8 + 0] == (9, 8, 7)
    # everything else stays blank
    hw2 = list(hw)
    hw2[0] = (0, 0, 0)
    assert hw2 == leds.blank()


def test_rotate_frame_rot0_is_identity():
    logical = list(leds.blank())
    logical[5] = (1, 2, 3)
    assert rotate_frame(logical, 0) == logical


def test_rotate_frame_wrong_length_raises():
    with pytest.raises(ValueError):
        rotate_frame([(0, 0, 0)] * 10, 0)


def test_rotate_partial_converts_logical_xy_to_hw():
    updates = [(7, 0, 1, 2, 3)]  # logical (7,0)
    hw_updates = rotate_partial(updates, 90)
    assert hw_updates == [(0, 0, 1, 2, 3)]


def test_edge_map_identity_at_rot0():
    assert edge_map(0) == {"N": "N", "S": "S", "E": "E", "W": "W"}


def test_edge_map_rot90_left_becomes_top():
    # hw W (left) is now sitting at logical N (top); hw E is now at
    # logical S; hw N is now at logical E; hw S is now at logical W.
    assert edge_map(90) == {"N": "W", "S": "E", "E": "N", "W": "S"}


def test_edge_map_rot180_flips_opposite_edges():
    assert edge_map(180) == {"N": "S", "S": "N", "E": "W", "W": "E"}


def test_edge_map_rot270_right_becomes_top():
    assert edge_map(270) == {"N": "E", "S": "W", "E": "S", "W": "N"}


def test_edge_map_invalid_rot_raises():
    with pytest.raises(ValueError):
        edge_map(45)


def test_analog_hw_to_logical_identity_at_rot0():
    raw = (100, 200, 300, 400, 10, 20, 30, 40)
    assert analog_hw_to_logical(raw, 0) == raw


def test_analog_hw_to_logical_rot90_reorders_softpots_and_fsrs():
    # raw = hw N,S,E,W softpots + hw N,S,E,W FSRs (FSR placement assumed,
    # see orientation.py's module docstring).
    raw = (1, 2, 3, 4, 10, 20, 30, 40)  # N=1 S=2 E=3 W=4 / N=10 S=20 E=30 W=40
    out = analog_hw_to_logical(raw, 90)
    # logical N should read hw W's channel, logical S <- hw E, logical E <- hw N, logical W <- hw S
    assert out[:4] == (4, 3, 1, 2)
    assert out[4:8] == (40, 30, 10, 20)


def test_lidar_hw_to_logical_identity_at_rot0():
    mm4 = (500, 600, 700, 800)
    assert lidar_hw_to_logical(mm4, 0) == mm4


def test_lidar_hw_to_logical_rot180_swaps_opposite_edges():
    mm4 = (500, 600, 700, 800)  # N S E W
    out = lidar_hw_to_logical(mm4, 180)
    assert out == (600, 500, 800, 700)  # logical N<-hw S, S<-N, E<-W, W<-E


def test_orientation_dataclass_top_edge():
    assert Orientation(0).top_edge == "N"
    assert Orientation(90).top_edge == "W"
    assert Orientation(180).top_edge == "S"
    assert Orientation(270).top_edge == "E"


def test_orientation_dataclass_rejects_invalid_rot():
    with pytest.raises(ValueError):
        Orientation(45)


# ---------------------------------------------------------------------------
# Gesture + App wiring
# ---------------------------------------------------------------------------


@pytest.fixture
def make_app(tmp_path, monkeypatch):
    monkeypatch.setattr("septagraph.pi.app.STATE_DIR", tmp_path)
    monkeypatch.setattr("septagraph.pi.app.STATE_PATH", tmp_path / "app_state.json")
    args = build_arg_parser().parse_args(["--fake-grid"])
    app = App(args)
    # The grid now boots BLANK (2026-09-17); these tests are about the
    # Machine Lab mode's geometry and LED path, so ask for it explicitly
    # rather than relying on what the instrument comes up in.
    app.switch_mode("direct_lab")
    yield app
    app.stop()


def _hold(app, corner_a, corner_b, hold_s):
    """Press both corners, back-date their recorded press time by hold_s
    (avoids a real sleep in the test), then run the gesture check."""
    app.link.simulate_key(*corner_a, 1)
    app.link.simulate_key(*corner_b, 1)
    with app._gesture_lock:
        for c in (corner_a, corner_b):
            app._corner_held[c] = time.monotonic() - hold_s
    app._check_orientation_gesture()


def test_rot0_default_and_persisted_across_boot(tmp_path, monkeypatch):
    monkeypatch.setattr("septagraph.pi.app.STATE_DIR", tmp_path)
    monkeypatch.setattr("septagraph.pi.app.STATE_PATH", tmp_path / "app_state.json")
    args = build_arg_parser().parse_args(["--fake-grid"])
    app = App(args)
    assert app.orientation.rot == 0
    app._commit_orientation(270)
    app.stop()

    app2 = App(args)
    assert app2.orientation.rot == 270
    app2.stop()


def test_holding_right_edge_corners_rotates_after_800ms(make_app):
    app = make_app
    assert app.orientation.rot == 0
    _hold(app, (7, 0), (7, 7), 0.85)
    assert app.orientation.rot == 270  # E held -> rot 270 (see EDGE_TO_ROT)


def test_holding_below_threshold_does_not_rotate(make_app):
    app = make_app
    _hold(app, (7, 0), (7, 7), 0.5)
    assert app.orientation.rot == 0


def test_single_corner_never_rotates(make_app):
    app = make_app
    app.link.simulate_key(7, 0, 1)
    with app._gesture_lock:
        app._corner_held[(7, 0)] = time.monotonic() - 2.0
    app._check_orientation_gesture()
    assert app.orientation.rot == 0


def test_two_non_edge_pads_never_rotate(make_app):
    app = make_app
    # (3,3)/(4,4) are not a recognised corner pair at all -- simulate_key
    # will not even register them in _corner_held.
    app.link.simulate_key(3, 3, 1)
    app.link.simulate_key(4, 4, 1)
    with app._gesture_lock:
        app._corner_held[(3, 3)] = time.monotonic() - 2.0
        app._corner_held[(4, 4)] = time.monotonic() - 2.0
    app._check_orientation_gesture()
    assert app.orientation.rot == 0


def test_two_corners_of_different_edges_never_rotate(make_app):
    app = make_app
    # (0,0) [N/W corner] + (7,7) [S/E corner] are diagonal, not one edge.
    app.link.simulate_key(0, 0, 1)
    app.link.simulate_key(7, 7, 1)
    with app._gesture_lock:
        app._corner_held[(0, 0)] = time.monotonic() - 2.0
        app._corner_held[(7, 7)] = time.monotonic() - 2.0
    app._check_orientation_gesture()
    assert app.orientation.rot == 0


def test_rotation_blocked_until_both_corners_release(make_app):
    app = make_app
    _hold(app, (0, 7), (7, 7), 0.85)  # S edge -> rot 180
    assert app.orientation.rot == 180

    # Still held: back-date again (simulating more elapsed time) and
    # re-check -- must NOT re-fire / change anything while still held.
    with app._gesture_lock:
        app._corner_held[(0, 7)] = time.monotonic() - 5.0
        app._corner_held[(7, 7)] = time.monotonic() - 5.0
    app._check_orientation_gesture()
    assert app.orientation.rot == 180

    # Release both corners -> re-armed.
    app.link.simulate_key(0, 7, 0)
    app.link.simulate_key(7, 7, 0)
    assert app._gesture_armed is True

    # Hold the (now-different) top edge (N) and confirm it fires again.
    _hold(app, (0, 0), (7, 0), 0.85)
    assert app.orientation.rot == 0  # N -> rot 0


def test_mode_receives_logical_coordinates_after_rotation(make_app):
    app = make_app
    app._commit_orientation(90)  # W held -> becomes top
    mode = app.modes["direct_lab"]

    calls = []
    orig_on_key = mode.on_key
    mode.on_key = lambda x, y, edge: (calls.append((x, y, edge)), orig_on_key(x, y, edge))[-1]

    # hw (0, 0) should now be reported to the mode as logical (7, 0)
    # (hw_to_logical(0, 0, 90) == (7, 0)).
    app.link.simulate_key(0, 0, 1)
    assert calls[-1] == (7, 0, 1)


def test_led_frame_sent_is_rotated(make_app):
    app = make_app
    mode = app.modes["direct_lab"]
    x, y = next(iter(mode.layout.keys())).split(",")
    x, y = int(x), int(y)  # a populated LOGICAL pad

    app._commit_orientation(90)
    time.sleep(leds.CROSSFADE_S + 0.05)  # let the orientation crossfade finish
    app._render_leds_once()

    hx, hy = orientation.logical_to_hw(x, y, 90)
    link: FakeGrid = app.link
    # The colour that belongs at logical (x, y) must now be physically at
    # hw (hx, hy) -- proving the frame actually sent over the wire was
    # rotated, not left in logical order.
    expected_color = mode.led_frame()[leds.idx(x, y)]
    assert link.last_frame[leds.idx(hx, hy)] == expected_color


def test_analog_dispatched_to_mode_is_logical(make_app):
    app = make_app
    app._commit_orientation(90)
    mode = app.modes["direct_lab"]

    seen = []
    mode.on_analog = lambda raw: seen.append(raw)

    app.link.simulate_analog((1, 2, 3, 4, 10, 20, 30, 40))
    assert seen[-1][:4] == (4, 3, 1, 2)  # matches test_analog_hw_to_logical_rot90


def test_lidar_dispatched_to_mode_is_logical(make_app):
    app = make_app
    app._commit_orientation(180)
    mode = app.modes["direct_lab"]

    seen = []
    mode.on_lidar = lambda mm4: seen.append(mm4)

    app.link.simulate_lidar((500, 600, 700, 800))
    assert seen[-1] == (600, 500, 800, 700)


def test_api_state_exposes_orientation(make_app):
    app = make_app
    app._commit_orientation(270)
    state = app.get_state()
    assert state["orientation"] == {"rot": 270, "top_edge": "E"}


def test_manual_orientation_endpoint(make_app):
    app = make_app
    result = app.dispatch("orientation", {"rot": 180})
    assert result == {"ok": True, "rot": 180}
    assert app.orientation.rot == 180


def test_manual_orientation_endpoint_rejects_bad_rot(make_app):
    app = make_app
    result = app.dispatch("orientation", {"rot": 45})
    assert "error" in result
    assert app.orientation.rot == 0


def test_corner_press_still_strikes_normally(make_app):
    """Corners are never suppressed from the mode's on_key -- a corner
    press always triggers its usual strike, even mid-hold (documented
    behaviour, see App._on_key's comment)."""
    app = make_app
    mode = app.modes["direct_lab"]
    calls = []
    orig_on_key = mode.on_key
    mode.on_key = lambda x, y, edge: (calls.append((x, y, edge)), orig_on_key(x, y, edge))[-1]

    app.link.simulate_key(0, 0, 1)
    assert (0, 0, 1) in calls
