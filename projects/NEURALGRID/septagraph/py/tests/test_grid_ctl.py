"""Tests for controllers.grid_ctl.GridController (plan Task 3.3)."""
from __future__ import annotations

import socket
import time
import types

import pytest

from septagraph.codec import Decoder
from septagraph.manifest import ManifestStore
from septagraph.net import Sender
from septagraph.types import CatalogPoint, Manifest, vec
from septagraph.window import Window

from controllers.grid_ctl import GridController, LOUD_AXIS, SOFTPOT_AXIS, SOFTPOT_TOUCH_THRESHOLD
from septagraph.axes import K


class LoopbackReceiver:
    """Minimal synchronous receiver: read whatever's on the socket right now."""

    def __init__(self, port: int):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("127.0.0.1", port))
        self.sock.settimeout(0.5)
        self.decoder = Decoder()

    def recv_one(self):
        data, _ = self.sock.recvfrom(65536)
        objs = self.decoder.feed(data)
        assert len(objs) == 1
        return objs[0]

    def close(self):
        self.sock.close()


@pytest.fixture
def loopback():
    port = 17700
    recv = LoopbackReceiver(port)
    sender = Sender(targets=[("127.0.0.1", port)])
    yield sender, recv
    sender.close()
    recv.close()


def make_window():
    return Window(
        center=vec(*([0.5] * K)),
        extent=vec(*([0.5] * K)),
        assign={"grid": (0, 4)},
    )


def make_manifests_with_point(x=2, y=3):
    # snapped cell's projected mu (grid channel, center=.5/extent=.5 everywhere):
    # d = ((x+.5)/8, (y+.5)/8); mu[0] from x, mu[4] from y, rest = center (0.5)
    dx = (x + 0.5) / 8.0
    dy = (y + 0.5) / 8.0
    mu = [0.5] * K
    mu[0] = max(0.0, min(1.0, 0.5 + (2 * dx - 1) * 0.5))
    mu[4] = max(0.0, min(1.0, 0.5 + (2 * dy - 1) * 0.5))
    point = CatalogPoint(
        point_id=0, mu=vec(*mu), sigma=vec(*([0.02] * K)),
        instrument="breakBot", max_rate_hz=12, vel_min=0.15, vel_max=1.0,
    )
    store = ManifestStore()
    store.update(Manifest(renderer_id="lab", kind="catalog", name="fixture", lo=None, hi=None, points=(point,)))
    return store, point


class FakeLink:
    """Stand-in for LidarGridLink: GridController assigns its callbacks here."""
    on_key = None
    on_analog = None
    on_lidar = None


def test_key_press_on_snapped_cell_emits_hit_at_catalog_mu(loopback):
    sender, recv = loopback
    store, point = make_manifests_with_point(2, 3)
    ctl = GridController(FakeLink(), sender, make_window(), store, leds=lambda b: None)

    ctl.on_key(2, 3, 1)
    hit = recv.recv_one()
    assert hit.onset is True
    assert hit.voice_id == "grid:23"
    assert hit.mu == pytest.approx(point.mu, abs=1e-6)


def test_key_release_emits_onset_zero_same_voice_id(loopback):
    sender, recv = loopback
    store, _ = make_manifests_with_point(2, 3)
    ctl = GridController(FakeLink(), sender, make_window(), store, leds=lambda b: None)

    ctl.on_key(2, 3, 1)
    recv.recv_one()  # the press
    ctl.on_key(2, 3, 0)
    hit = recv.recv_one()
    assert hit.onset is False
    assert hit.voice_id == "grid:23"


def test_analog_stream_while_held_emits_voice_bound_move_with_rate_axis_changed(loopback):
    sender, recv = loopback
    store, _ = make_manifests_with_point(2, 3)
    ctl = GridController(FakeLink(), sender, make_window(), store, leds=lambda b: None)
    ctl.on_key(2, 3, 1)
    recv.recv_one()  # drain the hit

    # N maps to rate axis (3); push it well above touch threshold and off-center
    raw_n_high = 3500
    ctl.on_analog((raw_n_high, 0, 0, 0))
    mv = recv.recv_one()
    assert mv.voice_id == "grid:23"
    base_mu = ctl._cell_table[(2, 3)][0]
    assert mv.mu[SOFTPOT_AXIS["N"]] != pytest.approx(base_mu[SOFTPOT_AXIS["N"]])
    # other axes unaffected by this modulation
    assert mv.mu[0] == pytest.approx(base_mu[0])


def test_untouched_strip_emits_nothing(loopback):
    sender, recv = loopback
    store, _ = make_manifests_with_point(2, 3)
    ctl = GridController(FakeLink(), sender, make_window(), store, leds=lambda b: None)
    ctl.on_key(2, 3, 1)
    recv.recv_one()  # drain

    ctl.on_analog((SOFTPOT_TOUCH_THRESHOLD - 1,) * 4)
    with pytest.raises(socket.timeout):
        recv.sock.settimeout(0.2)
        recv.recv_one()


def test_manifest_arrival_updates_led_frame_192_bytes_warm_white_at_snapped_index():
    frames = []
    store = ManifestStore()
    ctl = GridController(FakeLink(), Sender(targets=[]), make_window(), store, leds=lambda b: frames.append(b))
    frames.clear()

    _, point = make_manifests_with_point(2, 3)
    m = Manifest(renderer_id="lab", kind="catalog", name="fixture", lo=None, hi=None, points=(point,))
    ctl.on_manifest(m)

    assert len(frames) >= 1
    frame = frames[-1]
    assert len(frame) == 192
    i = 3 * 8 + 2  # y*8+x
    assert tuple(frame[i * 3: i * 3 + 3]) == (255, 190, 110)


# -- FSR strikes (task 4 / HANDOFF §11) -----------------------------------


def test_fsr_never_emits_hit_or_release(loopback):
    """Locked rule (2026-09-09): FSRs never strike. A full press/release
    cycle on FSR0 puts nothing on the wire."""
    sender, recv = loopback
    store, _ = make_manifests_with_point(2, 3)
    ctl = GridController(FakeLink(), sender, make_window(), store, leds=lambda b: None)

    ctl.on_analog((0, 0, 0, 0, 4000, 0, 0, 0))  # would have been a strike
    ctl.on_analog((0, 0, 0, 0, 4090, 0, 0, 0))  # held, higher pressure
    ctl.on_analog((0, 0, 0, 0, 0, 0, 0, 0))     # release
    with pytest.raises(Exception):
        recv.recv_one()  # loopback receiver times out: nothing was sent


def test_fsr_below_onset_emits_nothing(loopback):
    sender, recv = loopback
    store, _ = make_manifests_with_point(2, 3)
    ctl = GridController(FakeLink(), sender, make_window(), store, leds=lambda b: None)

    ctl.on_analog((0, 0, 0, 0, 41, 0, 0, 0))  # just above floor(40), gamma-curved far below onset
    with pytest.raises(socket.timeout):
        recv.sock.settimeout(0.2)
        recv.recv_one()


def test_on_analog_with_4_values_skips_fsr_processing_gracefully(loopback):
    """Backward compat: callers that only pass the 4 softpot channels
    (older firmware / any surviving 4-tuple caller) must not crash."""
    sender, recv = loopback
    store, _ = make_manifests_with_point(2, 3)
    ctl = GridController(FakeLink(), sender, make_window(), store, leds=lambda b: None)
    ctl.on_analog((0, 0, 0, 0))  # no exception


def test_set_targets_swaps_sender_targets():
    store, _ = make_manifests_with_point(2, 3)
    ctl = GridController(FakeLink(), Sender(targets=[]), make_window(), store, leds=lambda b: None)
    ctl.set_targets([("127.0.0.1", 9999)])
    assert ctl.sender.targets == [("127.0.0.1", 9999)]
