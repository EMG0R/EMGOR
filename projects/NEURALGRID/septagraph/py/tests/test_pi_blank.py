"""The default mode: a plain MIDI controller with no opinions.

Every sensor gets a control, every pad gets a note, nothing is named
after an instrument. This is what the grid IS until you choose otherwise.
"""
import pytest

from septagraph.pi import leds
from septagraph.pi.blank import (
    BlankMode, BASE_NOTE, SOFTPOT_CC, FSR_CC, LIDAR_CC, PRESS_COLOR, FIELD_COLOR,
)
from septagraph.pi.menu import MODE_COLORS
from septagraph.axes import K


class Spy:
    """Stands in for a MIDI output."""
    def __init__(self):
        self.sent = []
    def note_on(self, note, vel=100, ch=0):
        self.sent.append(("on", note, vel, ch))
    def note_off(self, note, ch=0):
        self.sent.append(("off", note, ch))
    def cc(self, cc, val, ch=0):
        self.sent.append(("cc", cc, val, ch))


@pytest.fixture
def m():
    spy = Spy()
    return BlankMode(spy), spy


class TestPads:
    def test_pads_are_chromatic_and_row_major(self, m):
        mode, _ = m
        assert mode.note_for_pad(0, 0) == BASE_NOTE
        assert mode.note_for_pad(1, 0) == BASE_NOTE + 1
        assert mode.note_for_pad(0, 1) == BASE_NOTE + 8
        assert mode.note_for_pad(7, 7) == BASE_NOTE + 63

    def test_no_scale_is_imposed(self, m):
        # every semitone present: a blank controller does not pick a key
        mode, _ = m
        notes = [mode.note_for_pad(x, y) for y in range(8) for x in range(8)]
        assert notes == sorted(notes)
        assert len(set(notes)) == 64

    def test_pressing_sends_note_on_and_releasing_sends_note_off(self, m):
        mode, spy = m
        mode.on_key(2, 3, 1)
        mode.on_key(2, 3, 0)
        note = BASE_NOTE + 3 * 8 + 2
        assert spy.sent == [("on", note, 100, 0), ("off", note, 0)]

    def test_a_release_with_nothing_held_sends_nothing(self, m):
        mode, spy = m
        mode.on_key(2, 3, 0)
        assert spy.sent == []


class TestEverySensorIsAControl:
    def test_softpots_take_the_first_four_ccs(self, m):
        mode, spy = m
        mode.on_analog([4095, 0, 0, 0, 0, 0, 0, 0])
        assert ("cc", SOFTPOT_CC[0], 127, 0) in spy.sent

    def test_the_force_sensors_are_controls_not_notes(self, m):
        mode, spy = m
        mode.on_analog([0, 0, 0, 0, 4095, 0, 0, 0])
        assert ("cc", FSR_CC[0], 127, 0) in spy.sent
        assert not [s for s in spy.sent if s[0] in ("on", "off")]

    def test_each_lidar_gets_its_own_control(self, m):
        mode, spy = m
        mode.on_lidar([0, 0, 0, 0])
        ccs = {s[1] for s in spy.sent if s[0] == "cc"}
        assert set(LIDAR_CC) <= ccs

    def test_no_two_sensors_share_a_cc(self):
        allcc = list(SOFTPOT_CC) + list(FSR_CC) + list(LIDAR_CC)
        assert len(set(allcc)) == len(allcc)

    def test_unchanged_values_are_not_resent(self, m):
        # a blank controller should not flood the wire while sitting still
        mode, spy = m
        mode.on_analog([2000] * K)
        first = len(spy.sent)
        mode.on_analog([2000] * K)
        assert len(spy.sent) == first


class TestItLooksLikeOneThing:
    """A dim light-purple field at rest; light blue only where you touch.

    Lighting all 64 pads in the press colour left nothing for a press to
    say -- the grid has to be quiet before it can answer."""

    def test_the_resting_field_is_purple(self, m):
        mode, _ = m
        r, g, b = mode.led_frame()[leds.idx(4, 4)]
        assert b > r > g          # violet: blue leads, red next, green least

    def test_and_it_is_SATURATED_enough_to_read_as_a_colour(self, m):
        """The one that actually bit. A pale colour dimmed down is not a
        dim colour, it is grey: (54,42,76) went to the grid and read as
        dim white. Channel ORDER alone passes happily on a near-white,
        which is why the first version of this test missed it."""
        r, g, b = m[0].led_frame()[leds.idx(4, 4)]
        assert (max(r, g, b) - min(r, g, b)) / max(r, g, b) >= 0.8

    def test_the_press_colour_is_saturated_too(self, m):
        mode, _ = m
        mode.on_key(4, 4, 1)
        r, g, b = mode.led_frame()[leds.idx(4, 4)]
        assert (max(r, g, b) - min(r, g, b)) / max(r, g, b) >= 0.8

    def test_and_the_field_is_far_below_the_press(self, m):
        """All 64 lit together read as one glow; at the level this
        started at, that glow looked white rather than purple."""
        mode, _ = m
        px = mode.led_frame()[leds.idx(4, 4)]
        assert 0 < sum(px) < sum(PRESS_COLOR) * 0.5

    def test_it_survives_gamma_instead_of_being_crushed_to_black(self, m):
        # LEDs are gamma-corrected before send; a naively "dim" colour
        # lands at 1-2/255 on the wire and reads as a dead grid
        mode, _ = m
        px = leds.apply_gamma(mode.led_frame()[leds.idx(4, 4)])
        assert sum(px) > 12

    def test_a_press_is_dark_purple(self, m):
        mode, _ = m
        mode.on_key(4, 4, 1)
        r, g, b = mode.led_frame()[leds.idx(4, 4)]
        assert b > r > g          # purple: blue leads, red next, green least
        assert (r, g, b) == PRESS_COLOR

    def test_only_the_pressed_pad_lights(self, m):
        mode, _ = m
        mode.on_key(4, 4, 1)
        frame = mode.led_frame()
        assert frame[leds.idx(0, 0)] != PRESS_COLOR
        assert frame[leds.idx(4, 4)] == PRESS_COLOR

    def test_a_held_pad_stands_out(self, m):
        mode, _ = m
        mode.on_key(4, 4, 1)
        frame = mode.led_frame()
        assert sum(frame[leds.idx(4, 4)]) > sum(frame[leds.idx(0, 0)])

    def test_releasing_puts_the_field_back(self, m):
        mode, _ = m
        mode.on_key(4, 4, 1)
        mode.on_key(4, 4, 0)
        frame = mode.led_frame()
        assert frame[leds.idx(4, 4)] == frame[leds.idx(0, 0)]


class TestLeavingCleanly:
    def test_panic_releases_everything_still_held(self, m):
        mode, spy = m
        mode.on_key(1, 1, 1)
        mode.on_key(2, 2, 1)
        spy.sent.clear()
        mode.panic()
        assert sorted(s[1] for s in spy.sent if s[0] == "off") == [
            BASE_NOTE + 9, BASE_NOTE + 18
        ]
        assert mode.led_frame()[leds.idx(1, 1)] == mode.led_frame()[leds.idx(0, 0)]
