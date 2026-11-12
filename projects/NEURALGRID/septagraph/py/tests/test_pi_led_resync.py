"""The grid must heal itself.

We only ever send what CHANGED, and we skip sending at all when nothing
did. That is right for the serial budget and wrong for reliability: if a
partial update is lost or garbled on the wire, that pad keeps its old
colour forever, because the app believes it already sent the right thing
and nothing about that pad will change again until it is next pressed.

Reported on hardware as "lights stay brighter after it has been clicked"
-- the release update never landed. In software the release is correct,
so the fix is not in the mode: the display has to reconverge on its own.
"""
import time

import pytest

from septagraph.pi import leds
from septagraph.pi.app import LED_RESYNC_S, App, build_arg_parser


@pytest.fixture
def app(tmp_path, monkeypatch):
    monkeypatch.setattr("septagraph.pi.app.STATE_DIR", tmp_path)
    monkeypatch.setattr("septagraph.pi.app.STATE_PATH", tmp_path / "app_state.json")
    a = App(build_arg_parser().parse_args(["--fake-grid", "--encoder-port", "0"]))
    # Let the boot crossfade finish before measuring anything. Forcing
    # CROSSFADE_S to 0 does NOT do this -- it leaves the display stuck on
    # the fade's "from" frame, which is blank, and every assertion below
    # then compares black against black and passes for the wrong reason.
    time.sleep(leds.CROSSFADE_S + 0.05)
    a._render_leds_once()
    assert any(px != (0, 0, 0) for px in a.link.last_frame), "grid never lit"
    yield a
    a.stop()


def _age_the_resync(app, seconds):
    app._last_full_at = time.monotonic() - seconds


class TestAStalePadHeals:
    def test_a_pixel_lost_on_the_wire_comes_back(self, app):
        app._render_leds_once()
        i = leds.idx(4, 4)
        correct = app.link.last_frame[i]
        # a partial update that never arrived: the grid is showing
        # something the app has no idea about
        app.link.last_frame[i] = (255, 255, 255)
        _age_the_resync(app, LED_RESYNC_S + 0.1)
        app._render_leds_once()
        assert app.link.last_frame[i] == correct

    def test_it_heals_even_though_nothing_changed(self, app):
        """The hard case: the app's own frame is identical to what it
        last sent, so the early-out would skip the send entirely."""
        app._render_leds_once()
        before = list(app.link.last_frame)
        app.link.last_frame = leds.blank()
        _age_the_resync(app, LED_RESYNC_S + 0.1)
        app._render_leds_once()
        assert app.link.last_frame == before

    def test_a_press_that_never_released_on_the_grid_is_corrected(self, app):
        app._render_leds_once()
        i = leds.idx(2, 2)
        rest = app.link.last_frame[i]
        app.link.simulate_key(2, 2, 1)
        lit = app.link.last_frame[i]
        assert lit != rest
        app.link.simulate_key(2, 2, 0)
        app.link.last_frame[i] = lit        # the release update was lost
        _age_the_resync(app, LED_RESYNC_S + 0.1)
        app._render_leds_once()
        assert app.link.last_frame[i] == rest


class TestItDoesNotBecomeAFirehose:
    def test_an_idle_grid_is_still_quiet_between_resyncs(self, app):
        app._render_leds_once()
        sent = app._frames_sent
        for _ in range(20):
            app._render_leds_once()
        assert app._frames_sent == sent

    def test_the_resync_costs_exactly_one_frame(self, app):
        app._render_leds_once()
        sent = app._frames_sent
        _age_the_resync(app, LED_RESYNC_S + 0.1)
        app._render_leds_once()
        app._render_leds_once()
        app._render_leds_once()
        assert app._frames_sent == sent + 1

    def test_ordinary_changes_still_go_as_partial_updates(self, app):
        app._render_leds_once()
        app.link.sent_partials = 0
        orig = app.link.send_leds_partial
        def spy(updates):
            app.link.sent_partials += 1
            orig(updates)
        app.link.send_leds_partial = spy
        app.link.simulate_key(1, 1, 1)
        assert app.link.sent_partials == 1
