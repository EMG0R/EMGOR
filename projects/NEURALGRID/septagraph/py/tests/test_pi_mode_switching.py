"""The whole gesture, through the real app: boot blank, hold the four
corners, pick a mode off the grid."""
import time

import pytest

from septagraph.pi import leds
from septagraph.pi.app import App, build_arg_parser
from septagraph.pi.menu import MENU_SLOTS, MODE_COLORS


@pytest.fixture
def app(tmp_path, monkeypatch):
    monkeypatch.setattr("septagraph.pi.app.STATE_DIR", tmp_path)
    monkeypatch.setattr("septagraph.pi.app.STATE_PATH", tmp_path / "app_state.json")
    monkeypatch.setattr("septagraph.pi.app.leds.CROSSFADE_S", 0.0)
    a = App(build_arg_parser().parse_args(["--fake-grid", "--encoder-port", "0"]))
    yield a
    a.stop()


CORNERS = ((0, 0), (7, 0), (0, 7), (7, 7))


def _hold_all_corners(app, hold_s=1.0):
    for c in CORNERS:
        app.link.simulate_key(*c, 1)
    # back-date rather than really waiting
    for c in CORNERS:
        app._corners._down[c] = time.monotonic() - hold_s
    app._check_menu_gesture()


class TestItBootsBlank:
    def test_the_default_mode_is_the_blank_controller(self, app):
        assert app.active_name == "blank"

    def test_it_does_not_come_up_wearing_the_machine_labs_colours(self, app):
        frame = app.active_mode.led_frame()
        r, g, b = frame[leds.idx(4, 4)]
        assert b > r > g          # dim lavender, not the lab's amber

    def test_even_if_it_was_left_in_the_lab_last_time(self, tmp_path, monkeypatch):
        monkeypatch.setattr("septagraph.pi.app.STATE_DIR", tmp_path)
        monkeypatch.setattr("septagraph.pi.app.STATE_PATH", tmp_path / "app_state.json")
        args = build_arg_parser().parse_args(["--fake-grid", "--encoder-port", "0"])
        a = App(args)
        a.switch_mode("direct_lab")
        a.stop()
        b = App(args)
        try:
            assert b.active_name == "blank"
        finally:
            b.stop()


class TestTheMenuOpens:
    def test_holding_all_four_corners_opens_it(self, app):
        _hold_all_corners(app)
        assert app.active_name == "menu"

    def test_a_short_hold_does_not(self, app):
        for c in CORNERS:
            app.link.simulate_key(*c, 1)
        app._check_menu_gesture()
        assert app.active_name == "blank"

    def test_two_corners_rotate_instead_of_opening_the_menu(self, app):
        before = app.orientation.rot
        for c in ((0, 0), (0, 7)):
            app.link.simulate_key(*c, 1)
            app._corners._down[c] = time.monotonic() - 1.0
        with app._gesture_lock:
            for c in ((0, 0), (0, 7)):
                app._corner_held[c] = time.monotonic() - 1.0
        app._check_menu_gesture()
        app._check_orientation_gesture()
        assert app.active_name == "blank"
        assert app.orientation.rot != before

    def test_letting_go_of_the_menu_does_not_spin_the_grid(self, app):
        """Lifting fingers one at a time goes 4 -> 3 -> 2, and that last
        pair is an edge pair held long past the rotate threshold."""
        _hold_all_corners(app)
        before = app.orientation.rot
        with app._gesture_lock:
            for c in CORNERS:
                app._corner_held[c] = time.monotonic() - 2.0
        for c in ((7, 7), (0, 7)):
            app.link.simulate_key(*c, 0)
        app._check_orientation_gesture()
        assert app.orientation.rot == before

    def test_and_a_deliberate_rotate_still_works_after_a_menu_trip(self, app):
        _hold_all_corners(app)
        for c in CORNERS:
            app.link.simulate_key(*c, 0)      # fully clean
        before = app.orientation.rot
        for c in ((0, 0), (0, 7)):
            app.link.simulate_key(*c, 1)
            app._corners._down[c] = time.monotonic() - 1.0
        with app._gesture_lock:
            for c in ((0, 0), (0, 7)):
                app._corner_held[c] = time.monotonic() - 1.0
        app._check_orientation_gesture()
        assert app.orientation.rot != before

    def test_four_corners_do_NOT_also_spin_the_grid(self, app):
        before = app.orientation.rot
        _hold_all_corners(app)
        with app._gesture_lock:
            for c in CORNERS:
                app._corner_held[c] = time.monotonic() - 1.0
        app._check_orientation_gesture()
        assert app.orientation.rot == before
        assert app.active_name == "menu"


class TestPickingAMode:
    @pytest.mark.parametrize("coord,expected", sorted(MENU_SLOTS.items()))
    def test_each_pad_goes_where_it_says(self, app, coord, expected):
        _hold_all_corners(app)
        for c in CORNERS:            # let go before choosing
            app.link.simulate_key(*c, 0)
        app.link.simulate_key(coord[0], coord[1], 1)
        assert app.active_name == expected

    def test_modulettes_really_changes_the_lab_layout(self, app):
        app.switch_mode("modulettes")
        assert app.modes["direct_lab"].layout_mode == "modulettes"

    def test_and_leaving_it_puts_the_lab_back(self, app):
        app.switch_mode("modulettes")
        app.switch_mode("direct_lab")
        assert app.modes["direct_lab"].layout_mode != "modulettes"

    def test_the_menu_shows_every_destination(self, app):
        _hold_all_corners(app)
        frame = app.active_mode.led_frame()
        for (x, y), name in MENU_SLOTS.items():
            assert frame[leds.idx(x, y)] == MODE_COLORS[name]


class TestTransitions:
    def test_switching_crossfades_rather_than_cutting(self, app, monkeypatch):
        monkeypatch.setattr("septagraph.pi.app.leds.CROSSFADE_S", 0.15)
        app.switch_mode("monome")
        assert app._crossfader.active

    def test_the_outgoing_mode_is_silenced(self, app):
        app.switch_mode("blank")
        app.link.simulate_key(2, 2, 1)
        assert app.modes["blank"].held_notes
        app.switch_mode("monome")
        assert not app.modes["blank"].held_notes


class TestOneLinkOnly:
    def test_the_encoder_is_a_mode_not_a_second_serial_reader(self, app):
        # the bug this replaced: two processes with /dev/ttyUSB0 open
        assert "encoder" in app.modes
        app.switch_mode("encoder")
        app.link.simulate_key(1, 1, 1)
        assert app.active_mode.name == "encoder"
