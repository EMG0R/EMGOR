"""Letting something else drive the lights.

The wire contract is 64 (r,g,b) tuples, so anything that can produce an array
can drive the grid. What matters is that a model doing so cannot take the
instrument down with it: a bad frame, an exception, or a slow call must never
blank the grid or kill the link.
"""
import pytest

from septagraph.dimensional.app import MODE_RUN, DimensionalApp


def _app(tmp_path):
    return DimensionalApp(calibration_path=tmp_path / "c.json", start_mode=MODE_RUN)


class TestDriving:
    def test_a_source_takes_over_the_grid(self, tmp_path):
        a = _app(tmp_path)
        a.set_led_source(lambda: [(9, 9, 9)] * 64)
        assert a.led_frame() == [(9, 9, 9)] * 64

    def test_it_is_called_fresh_every_frame(self, tmp_path):
        a = _app(tmp_path)
        n = {"i": 0}
        def src():
            n["i"] += 1
            return [(n["i"], 0, 0)] * 64
        a.set_led_source(src)
        a.led_frame(); a.led_frame()
        assert n["i"] == 2

    def test_clearing_it_gives_the_instrument_back(self, tmp_path):
        a = _app(tmp_path)
        a.set_led_source(lambda: [(9, 9, 9)] * 64)
        a.set_led_source(None)
        a.on_key(x=2, y=5, pressed=True, t_mono=1.0)
        assert a.led_frame()[5 * 8 + 2] != (0, 0, 0)

    def test_calibration_still_owns_the_grid_while_calibrating(self, tmp_path):
        # guidance cues must not be overridable, or you cannot see what to do
        a = DimensionalApp(calibration_path=tmp_path / "c.json")
        a.set_led_source(lambda: [(0, 0, 0)] * 64)
        assert any(px != (0, 0, 0) for px in a.led_frame())


class TestItCannotBreakTheInstrument:
    def test_an_exception_falls_back_instead_of_blanking(self, tmp_path):
        a = _app(tmp_path)
        a.on_key(x=2, y=5, pressed=True, t_mono=1.0)
        a.set_led_source(lambda: 1 / 0)
        assert a.led_frame()[5 * 8 + 2] != (0, 0, 0)

    def test_a_wrong_length_frame_is_rejected(self, tmp_path):
        a = _app(tmp_path)
        a.set_led_source(lambda: [(1, 2, 3)] * 10)
        assert len(a.led_frame()) == 64

    def test_out_of_range_values_are_clamped_not_crashed(self, tmp_path):
        a = _app(tmp_path)
        a.set_led_source(lambda: [(999, -5, 12.7)] * 64)
        assert a.led_frame()[0] == (255, 0, 12)

    def test_returning_nothing_falls_back(self, tmp_path):
        a = _app(tmp_path)
        a.on_key(x=2, y=5, pressed=True, t_mono=1.0)
        a.set_led_source(lambda: None)
        assert a.led_frame()[5 * 8 + 2] != (0, 0, 0)

    def test_a_repeatedly_failing_source_is_reported(self, tmp_path):
        a = _app(tmp_path)
        a.set_led_source(lambda: 1 / 0)
        a.led_frame()
        assert a.state()["led_source_error"]


class TestPartialUpdates:
    """A model driving at rate should not resend 64 pixels to change one."""

    def test_an_unchanged_frame_sends_nothing(self):
        from septagraph.dimensional.leds import led_diff
        f = [(1, 2, 3)] * 64
        assert led_diff(f, f) == []

    def test_only_changed_pixels_are_listed(self):
        from septagraph.dimensional.leds import led_diff
        a = [(0, 0, 0)] * 64
        b = list(a); b[9] = (4, 5, 6)
        assert led_diff(a, b) == [(1, 1, 4, 5, 6)]

    def test_a_first_frame_is_a_full_send(self):
        from septagraph.dimensional.leds import led_diff
        assert led_diff(None, [(0, 0, 0)] * 64) is None


class TestDrivenFromOutside:
    """The ESP32 is a passthrough, so the seam is a 64-RGB array. Anything that
    can POST one should be able to own the grid -- a model, a sketch, a shell
    script -- without living inside this process."""

    def test_a_pushed_frame_takes_the_grid(self, tmp_path):
        a = _app(tmp_path)
        a.push_leds([(7, 8, 9)] * 64)
        assert a.led_frame() == [(7, 8, 9)] * 64

    def test_a_junk_frame_is_refused_and_reported(self, tmp_path):
        a = _app(tmp_path)
        assert a.push_leds([(1, 2, 3)] * 3) is False
        assert a.state()["led_source_error"]

    def test_the_instrument_takes_back_over_if_the_driver_stops(self, tmp_path):
        a = _app(tmp_path)
        a.on_key(x=2, y=5, pressed=True, t_mono=1.0)
        a.push_leds([(0, 0, 0)] * 64, now=100.0)
        assert a.led_frame(now=100.1) == [(0, 0, 0)] * 64
        assert a.led_frame(now=200.0)[5 * 8 + 2] != (0, 0, 0)

    def test_a_driver_that_keeps_pushing_keeps_the_grid(self, tmp_path):
        a = _app(tmp_path)
        a.push_leds([(4, 4, 4)] * 64, now=100.0)
        a.push_leds([(5, 5, 5)] * 64, now=101.0)
        assert a.led_frame(now=101.2) == [(5, 5, 5)] * 64

    def test_releasing_hands_it_straight_back(self, tmp_path):
        a = _app(tmp_path)
        a.on_key(x=2, y=5, pressed=True, t_mono=1.0)
        a.push_leds([(0, 0, 0)] * 64, now=100.0)
        a.release_leds()
        assert a.led_frame(now=100.1)[5 * 8 + 2] != (0, 0, 0)

    def test_calibration_still_wins(self, tmp_path):
        from septagraph.dimensional.app import DimensionalApp
        a = DimensionalApp(calibration_path=tmp_path / "c.json")
        a.push_leds([(0, 0, 0)] * 64)
        assert any(px != (0, 0, 0) for px in a.led_frame())
