"""The menu: how you get between modes without a screen.

Hold all four corners -> the menu. One pad per destination, lit in that
mode's own colour so the grid itself is the legend. Everything here is
pure geometry and colour; the app wiring is thin on purpose.
"""
import pytest

from septagraph.pi import leds
from septagraph.axes import K
from septagraph.pi.menu import (
    MENU_SLOTS, MenuMode, CornerWatch, MENU_HOLD_S, MODE_COLORS,
)


class TestWhereTheModesLive:
    def test_the_four_the_user_named_are_where_they_said(self):
        assert MENU_SLOTS[(7, 0)] == "direct_lab"   # top right
        assert MENU_SLOTS[(7, 1)] == "modulettes"   # the square below it
        assert MENU_SLOTS[(7, 7)] == "encoder"      # bottom right
        assert MENU_SLOTS[(0, 7)] == "monome"       # bottom left

    def test_top_left_returns_to_the_default(self):
        # there must always be a way back to the blank controller
        assert MENU_SLOTS[(0, 0)] == "blank"

    def test_generic_osc_sits_under_blank(self):
        # mirrors modulettes under direct_lab on the other side
        assert MENU_SLOTS[(0, 1)] == "osc"

    def test_nothing_else_is_a_target(self):
        assert len(MENU_SLOTS) == 6

    def test_every_slot_has_a_colour(self):
        for name in MENU_SLOTS.values():
            assert name in MODE_COLORS

    def test_monome_is_the_white_yellow_one(self):
        r, g, b = MODE_COLORS["monome"]
        assert r > 200 and g > 200 and b < 120   # warm white/yellow

    def test_blank_is_light_blue(self):
        r, g, b = MODE_COLORS["blank"]
        assert b > 200 and g > r


class TestWhatTheMenuLooksLike:
    def test_only_the_five_slots_are_lit(self):
        frame = MenuMode(lambda name: None).led_frame()
        lit = {(x, y) for y in range(8) for x in range(8)
               if frame[leds.idx(x, y)] != (0, 0, 0)}
        assert lit == set(MENU_SLOTS)

    def test_each_slot_wears_its_own_mode_colour(self):
        frame = MenuMode(lambda name: None).led_frame()
        for (x, y), name in MENU_SLOTS.items():
            assert frame[leds.idx(x, y)] == MODE_COLORS[name]


class TestChoosing:
    def test_pressing_a_slot_picks_that_mode(self):
        picked = []
        m = MenuMode(picked.append)
        m.on_key(7, 7, 1)
        assert picked == ["encoder"]

    def test_releasing_picks_nothing(self):
        picked = []
        MenuMode(picked.append).on_key(7, 7, 0)
        assert picked == []

    def test_pressing_a_blank_pad_does_nothing(self):
        picked = []
        MenuMode(picked.append).on_key(3, 3, 1)
        assert picked == []

    def test_the_menu_ignores_the_instrument_sensors(self):
        # no sound, no MIDI, no lab traffic while choosing
        m = MenuMode(lambda name: None)
        m.on_analog([2000] * K)
        m.on_lidar([100] * 4)
        assert m.led_frame()[leds.idx(0, 0)] == MODE_COLORS["blank"]


class TestTheCornerGesture:
    """All four corners opens the menu. Two corners of one edge already
    means 'rotate the grid', so the two gestures must not fight."""

    def test_all_four_held_long_enough_opens_the_menu(self):
        w = CornerWatch()
        for c in ((0, 0), (7, 0), (0, 7), (7, 7)):
            w.press(c, t=0.0)
        assert w.menu_ready(t=MENU_HOLD_S + 0.01) is True

    def test_but_not_before_the_hold_completes(self):
        w = CornerWatch()
        for c in ((0, 0), (7, 0), (0, 7), (7, 7)):
            w.press(c, t=0.0)
        assert w.menu_ready(t=MENU_HOLD_S - 0.05) is False

    def test_the_clock_starts_at_the_LAST_corner_pressed(self):
        # otherwise resting a hand on one corner primes the gesture
        w = CornerWatch()
        w.press((0, 0), t=0.0)
        w.press((7, 0), t=0.0)
        w.press((0, 7), t=0.0)
        w.press((7, 7), t=5.0)
        assert w.menu_ready(t=5.0 + MENU_HOLD_S - 0.05) is False
        assert w.menu_ready(t=5.0 + MENU_HOLD_S + 0.01) is True

    def test_two_corners_never_opens_the_menu(self):
        w = CornerWatch()
        w.press((0, 0), t=0.0)
        w.press((7, 0), t=0.0)
        assert w.menu_ready(t=100.0) is False

    def test_rotating_is_suppressed_while_all_four_are_down(self):
        # the whole point: four corners is NOT two edges' worth of rotate
        w = CornerWatch()
        for c in ((0, 0), (7, 0), (0, 7), (7, 7)):
            w.press(c, t=0.0)
        assert w.blocks_rotation() is True

    def test_and_allowed_again_with_only_one_edge_down(self):
        w = CornerWatch()
        w.press((0, 0), t=0.0)
        w.press((7, 0), t=0.0)
        assert w.blocks_rotation() is False

    def test_letting_go_of_four_corners_must_not_rotate_on_the_way_out(self):
        """The one that actually bit: lifting fingers one at a time from
        four passes THROUGH two corners of an edge, and those two have
        been held well past the rotate threshold -- so the grid span."""
        w = CornerWatch()
        for c in ((0, 0), (7, 0), (0, 7), (7, 7)):
            w.press(c, t=0.0)
        w.release((7, 7))
        w.release((0, 7))
        # (0,0) + (7,0) remain: an edge pair, held "long enough"
        assert w.blocks_rotation() is True

    def test_and_stays_blocked_until_every_finger_is_off(self):
        w = CornerWatch()
        for c in ((0, 0), (7, 0), (0, 7), (7, 7)):
            w.press(c, t=0.0)
        for c in ((7, 7), (0, 7), (7, 0)):
            w.release(c)
        assert w.blocks_rotation() is True      # one still down
        w.release((0, 0))
        assert w.blocks_rotation() is False     # clean slate

    def test_a_deliberate_two_corner_hold_afterwards_still_rotates(self):
        w = CornerWatch()
        for c in ((0, 0), (7, 0), (0, 7), (7, 7)):
            w.press(c, t=0.0)
        for c in ((0, 0), (7, 0), (0, 7), (7, 7)):
            w.release(c)
        w.press((0, 0), t=5.0)
        w.press((7, 0), t=5.0)
        assert w.blocks_rotation() is False

    def test_reaching_for_the_fourth_corner_does_not_rotate_en_route(self):
        # three down is already an edge pair plus one
        w = CornerWatch()
        w.press((0, 0), t=0.0)
        w.press((7, 0), t=0.0)
        w.press((0, 7), t=0.1)
        assert w.blocks_rotation() is True

    def test_it_will_not_fire_twice_without_releasing(self):
        w = CornerWatch()
        for c in ((0, 0), (7, 0), (0, 7), (7, 7)):
            w.press(c, t=0.0)
        assert w.menu_ready(t=1.0) is True
        w.consume()
        assert w.menu_ready(t=2.0) is False
        w.release((0, 0))
        for c in ((0, 0),):
            w.press(c, t=3.0)
        assert w.menu_ready(t=3.0 + MENU_HOLD_S + 0.01) is True

    def test_lifting_a_corner_cancels_the_hold(self):
        w = CornerWatch()
        for c in ((0, 0), (7, 0), (0, 7), (7, 7)):
            w.press(c, t=0.0)
        w.release((7, 7))
        assert w.menu_ready(t=10.0) is False
