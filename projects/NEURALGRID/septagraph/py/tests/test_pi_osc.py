"""Generic OSC: the grid as a plain instrument anything can talk to.

Monome mode speaks monome's protocol because that is the point of it.
This is the other half -- every sensor out, the lights in, under one
obvious address space, so Max / SC / Pd / TouchDesigner can use the
whole instrument (force and lidar included, which a grid has no way to
express) without pretending to be something they are not.
"""
import pytest

from septagraph.pi import leds
from septagraph.pi.osc import (
    DEFAULT_PREFIX, OscMode, PAD, FORCE, RIBBON, LIDAR, LIDAR_RANGE_MM,
)
from septagraph.pi.monome import osc_decode, osc_encode
from septagraph.axes import K


@pytest.fixture
def m():
    sent = []
    return OscMode(send=lambda a, args: sent.append((a, args))), sent


class TestEverythingComesOut:
    def test_a_pad_press(self, m):
        mode, sent = m
        mode.on_key(3, 4, 1)
        assert sent == [(f"{DEFAULT_PREFIX}{PAD}", [3, 4, 1])]

    def test_a_pad_release(self, m):
        mode, sent = m
        mode.on_key(3, 4, 0)
        assert sent[0][1] == [3, 4, 0]

    def test_ribbons_and_force_are_separate_addresses(self, m):
        mode, sent = m
        mode.on_analog([4095, 0, 0, 0, 2048, 0, 0, 0])
        addrs = {a for a, _ in sent}
        assert f"{DEFAULT_PREFIX}{RIBBON}" in addrs
        assert f"{DEFAULT_PREFIX}{FORCE}" in addrs

    def test_values_are_normalised_with_the_channel_index(self, m):
        mode, sent = m
        mode.on_analog([4095, 0, 0, 0, 0, 0, 0, 0])
        rib = [args for a, args in sent if a.endswith(RIBBON)]
        assert rib[0][0] == 0                      # which ribbon
        assert rib[0][1] == pytest.approx(1.0, abs=0.01)

    def test_lidar_carries_both_millimetres_and_a_normalised_value(self, m):
        mode, sent = m
        mode.on_lidar([0, None, None, None])
        args = [a for addr, a in sent if addr.endswith(LIDAR)][0]
        assert args[0] == 0                        # which sensor
        assert args[1] == 0                        # mm, as measured
        assert args[2] == pytest.approx(1.0)       # near = 1

    def test_a_lidar_with_no_target_says_nothing_rather_than_lying(self, m):
        mode, sent = m
        mode.on_lidar([None, None, None, None])
        assert not [a for a, _ in sent if a.endswith(LIDAR)]

    def test_a_still_instrument_stays_quiet(self, m):
        mode, sent = m
        mode.on_analog([2000] * K)
        n = len(sent)
        mode.on_analog([2000] * K)
        assert len(sent) == n

    def test_but_a_moving_one_does_not(self, m):
        mode, sent = m
        mode.on_analog([2000] * K)
        n = len(sent)
        mode.on_analog([3000] + [2000] * (K - 1))
        assert len(sent) > n


class TestTheLightsComeIn:
    def test_one_pad(self, m):
        mode, _ = m
        mode.handle(f"{DEFAULT_PREFIX}/led/set", [2, 3, 255, 0, 0])
        assert mode.led_frame()[leds.idx(2, 3)] == (255, 0, 0)

    def test_the_whole_grid(self, m):
        mode, _ = m
        mode.handle(f"{DEFAULT_PREFIX}/led/all", [10, 20, 30])
        assert mode.led_frame() == [(10, 20, 30)] * 64

    def test_a_whole_frame_at_once(self, m):
        mode, _ = m
        payload = []
        for i in range(64):
            payload += [i * 3 % 256, 0, 0]
        mode.handle(f"{DEFAULT_PREFIX}/led/frame", payload)
        assert mode.led_frame()[5] == (15, 0, 0)

    def test_clearing(self, m):
        mode, _ = m
        mode.handle(f"{DEFAULT_PREFIX}/led/all", [255, 255, 255])
        mode.handle(f"{DEFAULT_PREFIX}/led/clear", [])
        assert mode.led_frame() == [(0, 0, 0)] * 64

    def test_out_of_range_values_are_clamped_not_crashed(self, m):
        mode, _ = m
        mode.handle(f"{DEFAULT_PREFIX}/led/set", [0, 0, 999, -5, 40])
        assert mode.led_frame()[0] == (255, 0, 40)

    def test_a_pad_off_the_grid_is_ignored(self, m):
        mode, _ = m
        before = mode.led_frame()
        mode.handle(f"{DEFAULT_PREFIX}/led/set", [99, 99, 255, 255, 255])
        assert mode.led_frame() == before

    def test_junk_does_not_take_the_instrument_down(self, m):
        mode, _ = m
        for args in ([], ["x"], ["a", "b", "c", "d", "e"]):
            mode.handle(f"{DEFAULT_PREFIX}/led/set", args)
        assert len(mode.led_frame()) == 64

    def test_an_unknown_address_is_ignored(self, m):
        mode, _ = m
        before = mode.led_frame()
        mode.handle("/something/else", [1, 2, 3])
        assert mode.led_frame() == before


class TestFindingEachOther:
    def test_whoever_talks_to_us_gets_the_stream(self, m):
        mode, _ = m
        mode.on_packet(osc_encode(f"{DEFAULT_PREFIX}/led/clear"), ("10.0.0.7", 9001))
        assert ("10.0.0.7", 9001) in mode.targets

    def test_connect_names_a_different_port_to_reply_on(self, m):
        """Max's udpsend and udpreceive are separate objects on separate
        ports, so 'reply to whoever asked' is not enough on its own."""
        mode, _ = m
        mode.handle(f"{DEFAULT_PREFIX}/connect", ["10.0.0.7", 9999],
                    addr=("10.0.0.7", 40000))
        assert ("10.0.0.7", 9999) in mode.targets

    def test_connect_with_only_a_port_uses_the_senders_address(self, m):
        mode, _ = m
        mode.handle(f"{DEFAULT_PREFIX}/connect", [9999], addr=("10.0.0.7", 40000))
        assert ("10.0.0.7", 9999) in mode.targets

    def test_disconnect_stops_the_stream(self, m):
        mode, _ = m
        mode.handle(f"{DEFAULT_PREFIX}/connect", [9999], addr=("10.0.0.7", 40000))
        mode.handle(f"{DEFAULT_PREFIX}/disconnect", [9999], addr=("10.0.0.7", 40000))
        assert ("10.0.0.7", 9999) not in mode.targets

    def test_the_same_listener_twice_is_still_one_listener(self, m):
        mode, _ = m
        for _ in range(3):
            mode.handle(f"{DEFAULT_PREFIX}/connect", [9999], addr=("10.0.0.7", 1))
        assert len(mode.targets) == 1


class TestItLooksLikeSomething:
    def test_an_undriven_grid_shows_it_is_waiting(self, m):
        """Dark would be indistinguishable from a crash, and unlike the
        encoder there is nothing else drawing here."""
        mode, _ = m
        assert any(px != (0, 0, 0) for px in mode.led_frame())

    def test_and_the_moment_anything_drives_it_that_stops(self, m):
        mode, _ = m
        mode.handle(f"{DEFAULT_PREFIX}/led/clear", [])
        assert mode.led_frame() == [(0, 0, 0)] * 64
