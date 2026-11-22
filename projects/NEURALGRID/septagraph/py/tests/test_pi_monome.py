"""Monome mode -- be a grid, the way a grid actually is one.

A real monome is a USB device that serialosc enumerates, advertises over
Bonjour, and exposes as an OSC endpoint. Apps never talk to the hardware:
they ask serialosc what exists, then send /monome/grid/led/* to a port
and receive /monome/grid/key back. We implement that endpoint, so
anything that speaks to a grid -- norns included -- speaks to this.
"""
import pytest

from septagraph.pi import leds
from septagraph.axes import K
from septagraph.pi.monome import (
    DEFAULT_PREFIX, GRID_LEVELS, MonomeGrid, MonomeMode, SerialoscServer,
    osc_decode, osc_encode,
)


class TestOsc:
    def test_a_message_round_trips(self):
        addr, args = osc_decode(osc_encode("/monome/grid/key", 3, 4, 1))
        assert addr == "/monome/grid/key"
        assert args == [3, 4, 1]

    def test_strings_and_ints_together(self):
        addr, args = osc_decode(osc_encode("/serialosc/device", "m123", "monome 64", 15000))
        assert args == ["m123", "monome 64", 15000]

    def test_addresses_are_padded_to_four_bytes(self):
        assert len(osc_encode("/abc", 1)) % 4 == 0
        assert len(osc_encode("/abcdefg", 1)) % 4 == 0

    def test_floats_survive(self):
        addr, args = osc_decode(osc_encode("/x", 0.5))
        assert args[0] == pytest.approx(0.5)

    def test_rubbish_decodes_to_nothing(self):
        assert osc_decode(b"not osc at all") is None


class TestBeingAGrid:
    def test_it_reports_itself_as_a_64(self):
        g = MonomeGrid()
        assert g.width == 8 and g.height == 8

    def test_led_set_lights_one_pad(self):
        g = MonomeGrid()
        g.handle("/monome/grid/led/set", [2, 3, 1])
        assert g.level(2, 3) == GRID_LEVELS - 1
        assert g.level(0, 0) == 0

    def test_led_all_fills_and_clears(self):
        g = MonomeGrid()
        g.handle("/monome/grid/led/all", [1])
        assert all(g.level(x, y) for y in range(8) for x in range(8))
        g.handle("/monome/grid/led/all", [0])
        assert not any(g.level(x, y) for y in range(8) for x in range(8))

    def test_led_row_writes_a_bitmask(self):
        g = MonomeGrid()
        g.handle("/monome/grid/led/row", [0, 2, 0b00000101])
        assert g.level(0, 2) > 0
        assert g.level(1, 2) == 0
        assert g.level(2, 2) > 0

    def test_led_col_writes_a_bitmask(self):
        g = MonomeGrid()
        g.handle("/monome/grid/led/col", [3, 0, 0b00000011])
        assert g.level(3, 0) > 0 and g.level(3, 1) > 0 and g.level(3, 2) == 0

    def test_led_map_writes_a_whole_quadrant(self):
        g = MonomeGrid()
        g.handle("/monome/grid/led/map", [0, 0] + [0xFF] * 8)
        assert all(g.level(x, y) for y in range(8) for x in range(8))

    def test_varibright_levels_are_kept(self):
        g = MonomeGrid()
        g.handle("/monome/grid/led/level/set", [1, 1, 7])
        assert g.level(1, 1) == 7

    def test_level_all_sets_everything(self):
        g = MonomeGrid()
        g.handle("/monome/grid/led/level/all", [4])
        assert g.level(5, 5) == 4

    def test_an_unknown_message_is_ignored_not_fatal(self):
        g = MonomeGrid()
        g.handle("/monome/grid/led/nonsense", [1, 2, 3])
        assert g.level(0, 0) == 0


class TestHowItLooks:
    def test_an_unlit_grid_is_dark(self):
        m = MonomeMode(send=lambda a, args: None)
        assert m.led_frame() == [(0, 0, 0)] * 64

    def test_a_lit_pad_is_the_monome_white_yellow(self):
        m = MonomeMode(send=lambda a, args: None)
        m.grid.handle("/monome/grid/led/set", [1, 1, 1])
        r, g, b = m.led_frame()[leds.idx(1, 1)]
        assert r > 200 and g > 200 and b < 120

    def test_dimmer_levels_are_dimmer(self):
        m = MonomeMode(send=lambda a, args: None)
        m.grid.handle("/monome/grid/led/level/set", [0, 0, 15])
        m.grid.handle("/monome/grid/led/level/set", [1, 0, 4])
        assert sum(m.led_frame()[leds.idx(0, 0)]) > sum(m.led_frame()[leds.idx(1, 0)])


class TestKeysGoOut:
    def test_a_press_is_reported_as_a_grid_key(self):
        sent = []
        m = MonomeMode(send=lambda a, args: sent.append((a, args)))
        m.on_key(3, 4, 1)
        assert sent == [(f"{DEFAULT_PREFIX}/grid/key", [3, 4, 1])]

    def test_a_release_is_state_zero(self):
        sent = []
        m = MonomeMode(send=lambda a, args: sent.append((a, args)))
        m.on_key(3, 4, 0)
        assert sent[0][1] == [3, 4, 0]

    def test_the_prefix_can_be_changed_like_a_real_grid(self):
        sent = []
        m = MonomeMode(send=lambda a, args: sent.append((a, args)))
        m.handle("/sys/prefix", ["/mygrid"])
        m.on_key(0, 0, 1)
        assert sent[0][0] == "/mygrid/grid/key"

    def test_the_other_sensors_are_not_a_grids_business(self):
        sent = []
        m = MonomeMode(send=lambda a, args: sent.append((a, args)))
        m.on_analog([2000] * K)
        m.on_lidar([100] * 4)
        assert sent == []


class TestDiscovery:
    def test_listing_reports_one_device(self):
        replies = []
        s = SerialoscServer(device_id="neuralgrid", device_type="monome 64",
                            device_port=15000,
                            _send=lambda a, args, addr: replies.append((a, args, addr)))
        s.handle("/serialosc/list", ["10.0.0.9", 9000], ("10.0.0.9", 1234))
        addr, args, to = replies[0]
        assert addr == "/serialosc/device"
        assert args == ["neuralgrid", "monome 64", 15000]
        assert to == ("10.0.0.9", 9000)

    def test_notify_subscribes_and_says_nothing_yet(self):
        """serialosc's notify means "tell me when this CHANGES", not
        "tell me now" -- it sends one message on the next add/remove and
        then forgets you. Answering immediately invites a client that
        re-lists on every add (matron does) into a list -> notify -> add
        -> list spin."""
        replies = []
        s = SerialoscServer(device_id="neuralgrid", device_type="monome 64",
                            device_port=15000,
                            _send=lambda a, args, addr: replies.append((a, args, addr)))
        s.handle("/serialosc/notify", ["10.0.0.9", 9000], ("10.0.0.9", 1234))
        assert replies == []
        assert ("10.0.0.9", 9000) in s.watchers

    def test_a_subscriber_hears_about_a_real_change(self):
        replies = []
        s = SerialoscServer(device_id="neuralgrid", device_type="monome 64",
                            device_port=15000,
                            _send=lambda a, args, addr: replies.append((a, args, addr)))
        s.handle("/serialosc/notify", ["10.0.0.9", 9000], ("10.0.0.9", 1234))
        s.announce_added()
        assert replies[0][0] == "/serialosc/add"
        assert replies[0][1] == ["neuralgrid"]

    def test_and_is_forgotten_afterwards_like_the_real_thing(self):
        replies = []
        s = SerialoscServer(device_id="neuralgrid", device_type="monome 64",
                            device_port=15000,
                            _send=lambda a, args, addr: replies.append((a, args, addr)))
        s.handle("/serialosc/notify", ["10.0.0.9", 9000], ("10.0.0.9", 1234))
        s.announce_added()
        s.announce_added()
        assert len(replies) == 1

    def test_sys_info_describes_the_device(self):
        replies = []
        m = MonomeMode(send=lambda a, args: None,
                       _reply=lambda a, args, addr: replies.append((a, args)))
        m.handle("/sys/info", ["10.0.0.9", 9000], addr=("10.0.0.9", 1234))
        got = dict((a, args) for a, args in replies)
        assert got["/sys/size"] == [8, 8]
        assert "/sys/id" in got
        assert got["/sys/prefix"] == [DEFAULT_PREFIX]


class TestTheServerWiresTheDeviceUp:
    """The mode can answer /sys/info, but only if the server actually
    gave it somewhere to answer TO. It did not, and a device that never
    answers 'what are you?' is not discoverable no matter how well it is
    advertised."""

    def test_the_server_gives_the_mode_a_reply_path(self):
        from septagraph.pi.monome import MonomeServer
        mode = MonomeMode(send=lambda a, args: None)
        server = MonomeServer(mode)
        try:
            sent = []
            server._send_to = lambda a, args, addr: sent.append((a, args, addr))
            mode._reply = server._send_to
            mode.handle("/sys/info", ["10.0.0.9", 9000], addr=("10.0.0.9", 1234))
            assert [a for a, _, _ in sent]
        finally:
            server.stop()

    def test_the_wiring_is_done_by_the_server_itself(self):
        from septagraph.pi.monome import MonomeServer
        mode = MonomeMode(send=lambda a, args: None)
        server = MonomeServer(mode)
        try:
            assert mode._reply is not None
            # not the do-nothing default the mode was built with
            sent = []
            server._send_to = lambda a, args, addr: sent.append(a)
            mode.handle("/sys/info", ["10.0.0.9", 9000], addr=("10.0.0.9", 1234))
            assert "/sys/size" in sent
        finally:
            server.stop()
