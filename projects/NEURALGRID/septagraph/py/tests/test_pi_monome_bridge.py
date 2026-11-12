"""The bridge: a serialosc that runs on the CLIENT and points here.

Monome apps ask their own localhost:12002 what is plugged in, so a
network grid cannot be discovered from the grid's side -- see the
module docstring. This is the part that CAN work, and the part that
makes it zero-configuration is that the grid serves the file with its
own address already written into it.
"""
import re

import pytest

from septagraph.pi import monome_bridge
from septagraph.pi.webui import bridge_script_for


class TestTheGridWritesItsOwnAddressIn:
    def test_the_host_it_was_fetched_from_is_baked_in(self):
        src = bridge_script_for("192.168.0.242:8767")
        assert 'GRID_HOST = "192.168.0.242"' in src

    def test_a_hostname_works_the_same(self):
        src = bridge_script_for("neuralgrid.local:8767")
        assert 'GRID_HOST = "neuralgrid.local"' in src

    def test_a_missing_port_is_fine(self):
        assert 'GRID_HOST = "neuralgrid.local"' in bridge_script_for("neuralgrid.local")

    def test_the_grids_OSC_port_is_written_in_not_the_web_port(self):
        src = bridge_script_for("192.168.0.242:8767", device_port=15000)
        assert "GRID_PORT = 15000" in src

    def test_a_junk_host_header_falls_back_rather_than_injecting(self):
        src = bridge_script_for('evil"\nimport os;os.system("rm -rf /")\n#')
        assert "rm -rf" not in src
        assert re.search(r'GRID_HOST = "[A-Za-z0-9._-]*"', src)

    def test_it_is_still_a_valid_program_afterwards(self):
        import ast
        ast.parse(bridge_script_for("192.168.0.242:8767"))


class TestItRewritesWhereRepliesGo:
    """The app names ITSELF when it says where to send key presses, but
    the grid is on another machine and its replies have to come back
    through the bridge."""

    def test_sys_info_is_pointed_at_the_bridge(self):
        b = monome_bridge.Bridge(grid_host="10.0.0.5")
        b.my_ip = "10.0.0.9"
        sent = []
        b._dev = type("S", (), {"sendto": lambda self, d, a: sent.append((d, a))})()
        b._from_app(monome_bridge.osc_encode("/sys/info", "127.0.0.1", 9999),
                    ("127.0.0.1", 40000))
        # the bridge also announces the return route on a new app; find
        # the message this test is actually about
        info = [monome_bridge.osc_decode(d) for d, _ in sent]
        addr, args = next(m for m in info if m[0] == "/sys/info")
        assert args == ["10.0.0.9", b.device_port]

    def test_and_it_remembers_who_asked(self):
        b = monome_bridge.Bridge(grid_host="10.0.0.5")
        b._dev = type("S", (), {"sendto": lambda self, d, a: None})()
        b._from_app(monome_bridge.osc_encode("/sys/info", "127.0.0.1", 9999),
                    ("127.0.0.1", 40000))
        assert b.app_addr == ("127.0.0.1", 40000)

    def test_led_messages_pass_through_untouched(self):
        b = monome_bridge.Bridge(grid_host="10.0.0.5")
        sent = []
        b._dev = type("S", (), {"sendto": lambda self, d, a: sent.append((d, a))})()
        original = monome_bridge.osc_encode("/monome/grid/led/set", 1, 2, 1)
        b._from_app(original, ("127.0.0.1", 40000))
        assert original in [d for d, _ in sent]          # byte for byte
        assert all(a == ("10.0.0.5", 15000) for _, a in sent)

    def test_a_key_from_the_grid_reaches_the_app(self):
        b = monome_bridge.Bridge(grid_host="10.0.0.5")
        sent = []
        b._dev = type("S", (), {"sendto": lambda self, d, a: sent.append((d, a))})()
        b.app_addr = ("127.0.0.1", 40000)
        key = monome_bridge.osc_encode("/monome/grid/key", 3, 4, 1)
        b._from_grid(key)
        assert sent == [(key, ("127.0.0.1", 40000))]

    def test_a_key_arriving_before_any_app_is_dropped_not_crashed(self):
        b = monome_bridge.Bridge(grid_host="10.0.0.5")
        b._dev = type("S", (), {"sendto": lambda self, d, a: pytest.fail("sent")})()
        b._from_grid(monome_bridge.osc_encode("/monome/grid/key", 0, 0, 1))


class TestOscSurvivesTheRoundTrip:
    def test_the_bridges_own_codec_agrees_with_the_grids(self):
        from septagraph.pi import monome as grid_side
        pkt = grid_side.osc_encode("/sys/size", 8, 8)
        assert monome_bridge.osc_decode(pkt) == ("/sys/size", [8, 8])
        pkt2 = monome_bridge.osc_encode("/monome/grid/key", 7, 7, 0)
        assert grid_side.osc_decode(pkt2) == ("/monome/grid/key", [7, 7, 0])


class TestItConfiguresTheGridWithoutBeingAsked:
    """A grid sends key presses to whatever /sys/host + /sys/port it was
    told, and to nowhere at all until it is told. Well-behaved apps send
    both; the bridge knows the topology either way, so it says so itself
    the moment an app turns up rather than leaving a grid whose lights
    work and whose keys silently go nowhere."""

    def _bridge_with_spy(self):
        b = monome_bridge.Bridge(grid_host="10.0.0.5")
        b.my_ip = "10.0.0.9"
        sent = []
        b._dev = type("S", (), {"sendto": lambda self, d, a: sent.append((d, a))})()
        return b, sent

    def test_the_first_app_message_also_points_the_grid_back_here(self):
        b, sent = self._bridge_with_spy()
        b._from_app(monome_bridge.osc_encode("/monome/grid/led/all", 1),
                    ("127.0.0.1", 40000))
        addressed = [monome_bridge.osc_decode(d)[0] for d, _ in sent]
        assert "/sys/host" in addressed and "/sys/port" in addressed

    def test_it_names_the_bridge_not_the_app(self):
        b, sent = self._bridge_with_spy()
        b._from_app(monome_bridge.osc_encode("/monome/grid/led/all", 1),
                    ("127.0.0.1", 40000))
        by_addr = {monome_bridge.osc_decode(d)[0]: monome_bridge.osc_decode(d)[1]
                   for d, _ in sent}
        assert by_addr["/sys/host"] == ["10.0.0.9"]
        assert by_addr["/sys/port"] == [b.device_port]

    def test_it_does_not_repeat_itself_on_every_message(self):
        b, sent = self._bridge_with_spy()
        for _ in range(5):
            b._from_app(monome_bridge.osc_encode("/monome/grid/led/all", 1),
                        ("127.0.0.1", 40000))
        assert sum(1 for d, _ in sent
                   if monome_bridge.osc_decode(d)[0] == "/sys/host") == 1

    def test_a_new_app_gets_it_again(self):
        b, sent = self._bridge_with_spy()
        b._from_app(monome_bridge.osc_encode("/monome/grid/led/all", 1),
                    ("127.0.0.1", 40000))
        b._from_app(monome_bridge.osc_encode("/monome/grid/led/all", 1),
                    ("127.0.0.1", 40001))
        assert sum(1 for d, _ in sent
                   if monome_bridge.osc_decode(d)[0] == "/sys/host") == 2
