"""The dimensional encoder, hosted as a mode.

It used to be its own systemd service with its own serial link -- which
meant two processes had /dev/ttyUSB0 open at once, splitting the ESP32's
byte stream between them. One process owns the link; the encoder becomes
a mode inside it, like everything else.
"""
import pytest

from septagraph.pi import leds
from septagraph.pi.encoder_mode import EncoderMode


class FakeDim:
    """Stands in for DimensionalApp."""
    def __init__(self):
        self.keys, self.analog, self.lidar = [], [], []
        self.frame = [(0, 0, 0)] * 64
    def on_key(self, x, y, pressed, t_mono=None):
        self.keys.append((x, y, pressed))
    def on_analog(self, values, t_mono=None):
        self.analog.append(tuple(values))
    def on_lidar(self, ranges, t_mono=None):
        self.lidar.append(tuple(ranges))
    def led_frame(self, now=None):
        return list(self.frame)


@pytest.fixture
def m():
    dim = FakeDim()
    return EncoderMode(dim), dim


class TestItForwardsTheInstrument:
    def test_a_press_reaches_the_encoder_as_a_press(self, m):
        mode, dim = m
        mode.on_key(3, 4, 1)
        assert dim.keys == [(3, 4, True)]

    def test_a_release_is_a_release(self, m):
        mode, dim = m
        mode.on_key(3, 4, 0)
        assert dim.keys == [(3, 4, False)]

    def test_analog_and_lidar_pass_straight_through(self, m):
        mode, dim = m
        mode.on_analog([1, 2, 3, 4, 5, 6, 7, 8])
        mode.on_lidar([10, 20, 30, 40])
        assert dim.analog == [(1, 2, 3, 4, 5, 6, 7, 8)]
        assert dim.lidar == [(10, 20, 30, 40)]


class TestItLooksLikeTheEncoder:
    def test_the_encoders_own_lights_are_what_you_see(self, m):
        mode, dim = m
        dim.frame[leds.idx(2, 2)] = (10, 20, 30)
        assert mode.led_frame()[leds.idx(2, 2)] == (10, 20, 30)

    def test_a_dark_encoder_frame_is_left_dark(self, m):
        """No corner marker. It only appeared when the frame happened to
        be entirely black, so it blinked in and out as the encoder drew --
        a pad flickering purple at the edge of vision reads as a fault,
        which is worse than the dark grid it was meant to explain."""
        mode, dim = m
        assert mode.led_frame() == [(0, 0, 0)] * 64

    def test_the_encoder_is_never_second_guessed(self, m):
        mode, dim = m
        dim.frame[leds.idx(3, 3)] = (9, 9, 9)
        assert mode.led_frame() == dim.led_frame()


class TestItFailsQuietly:
    def test_an_encoder_that_throws_does_not_take_the_grid_down(self):
        class Broken(FakeDim):
            def led_frame(self, now=None):
                raise RuntimeError("boom")
        mode = EncoderMode(Broken())
        assert len(mode.led_frame()) == 64
