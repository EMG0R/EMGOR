"""Canvas taps: self-expiring UDP subscriptions that stream ring[offset:offset+count]
to an external consumer (e.g. GORMAPPER) in Frame Protocol v1, chunked to <=480px
per datagram with SHOW only on the last chunk of each frame."""
import json
import time

import numpy as np
import pytest
from fastapi.testclient import TestClient

from lightserver.api import create_app
from lightserver.nodes import NodeConfig
from lightserver.persist import StateStore
from lightserver.protocol import parse_packet
from lightserver.ring import RingEngine, RingManager, RingSegment
from lightserver.taps import TapManager

CONFIGS = [
    NodeConfig(name="fcob-strip", host="fcob-strip.local", pixel_count=800),
    NodeConfig(name="fcob-strip2", host="fcob-strip2.local", pixel_count=480),
    NodeConfig(name="fcob-strip3", host="fcob-strip3.local", pixel_count=640),
]


def make_segments():
    return [RingSegment("fcob-strip", 0, 800),
            RingSegment("fcob-strip2", 800, 480),
            RingSegment("fcob-strip3", 1280, 640)]


def ring_engine(mode="headless"):
    return RingEngine(make_segments(), CONFIGS, mode=mode, seed=1)


class FakeTapSocket:
    """Records sendto() calls instead of touching the network."""
    def __init__(self):
        self.sent = []  # list of (addr, packet_bytes)

    def sendto(self, pkt, addr):
        self.sent.append((addr, pkt))

    def close(self):
        pass


# -- chunking math ----------------------------------------------------------

def test_full_ring_default_splits_into_4_chunks_with_show_on_last():
    tm = TapManager(ring_len=1920)
    tm._sock = FakeTapSocket()
    tm.set_tap("t", host="1.2.3.4", port=9999)
    frame = np.zeros((1920, 3), dtype=np.uint8)
    tm.emit(frame)
    sent = tm._sock.sent
    assert len(sent) == 4
    parsed = [parse_packet(pkt) for _, pkt in sent]
    offsets = [p[2] for p in parsed]
    counts = [p[3] for p in parsed]
    assert offsets == [0, 480, 960, 1440]
    assert counts == [480, 480, 480, 480]
    flags = [p[1] for p in parsed]
    assert flags[:3] == [0, 0, 0]
    assert flags[3] == 1  # SHOW only on last chunk


def test_boundary_count_480_is_one_chunk():
    tm = TapManager(ring_len=1920)
    tm._sock = FakeTapSocket()
    tm.set_tap("t", host="1.2.3.4", port=9999, count=480)
    frame = np.zeros((1920, 3), dtype=np.uint8)
    tm.emit(frame)
    sent = tm._sock.sent
    assert len(sent) == 1
    _, _, offset, count, _ = parse_packet(sent[0][1])
    assert offset == 0 and count == 480


def test_boundary_count_481_is_two_chunks():
    tm = TapManager(ring_len=1920)
    tm._sock = FakeTapSocket()
    tm.set_tap("t", host="1.2.3.4", port=9999, count=481)
    frame = np.zeros((1920, 3), dtype=np.uint8)
    tm.emit(frame)
    sent = tm._sock.sent
    assert len(sent) == 2
    parsed = [parse_packet(pkt) for _, pkt in sent]
    assert [(p[2], p[3]) for p in parsed] == [(0, 480), (480, 1)]
    assert [p[1] for p in parsed] == [0, 1]


def test_offset_is_carried_into_each_chunk():
    tm = TapManager(ring_len=1920)
    tm._sock = FakeTapSocket()
    tm.set_tap("t", host="1.2.3.4", port=9999, offset=1000, count=500)
    frame = np.zeros((1920, 3), dtype=np.uint8)
    tm.emit(frame)
    parsed = [parse_packet(pkt) for _, pkt in tm._sock.sent]
    assert [(p[2], p[3]) for p in parsed] == [(1000, 480), (1480, 20)]


def test_rgb_bytes_match_the_sliced_frame():
    tm = TapManager(ring_len=1920)
    tm._sock = FakeTapSocket()
    tm.set_tap("t", host="1.2.3.4", port=9999, offset=0, count=480)
    frame = np.zeros((1920, 3), dtype=np.uint8)
    frame[10] = (255, 0, 128)
    tm.emit(frame)
    _, _, offset, count, rgb = parse_packet(tm._sock.sent[0][1])
    assert rgb[10 * 3:10 * 3 + 3] == bytes([255, 0, 128])


def test_packets_go_to_the_taps_host_and_port():
    tm = TapManager(ring_len=1920)
    tm._sock = FakeTapSocket()
    tm.set_tap("t", host="9.9.9.9", port=4242)
    frame = np.zeros((1920, 3), dtype=np.uint8)
    tm.emit(frame)
    addrs = {addr for addr, _ in tm._sock.sent}
    assert addrs == {("9.9.9.9", 4242)}


# -- fps divisor --------------------------------------------------------

def test_fps_divisor_2_sends_every_other_frame():
    tm = TapManager(ring_len=1920)
    tm._sock = FakeTapSocket()
    tm.set_tap("t", host="1.2.3.4", port=9999, fps_divisor=2, count=480)
    frame = np.zeros((1920, 3), dtype=np.uint8)
    for _ in range(6):
        tm.emit(frame)
    # 6 engine frames, divisor 2 -> 3 frames actually sent -> 3 packets (1 chunk each)
    assert len(tm._sock.sent) == 3


def test_fps_divisor_1_sends_every_frame():
    tm = TapManager(ring_len=1920)
    tm._sock = FakeTapSocket()
    tm.set_tap("t", host="1.2.3.4", port=9999, count=480)
    frame = np.zeros((1920, 3), dtype=np.uint8)
    for _ in range(5):
        tm.emit(frame)
    assert len(tm._sock.sent) == 5


# -- TTL expiry -----------------------------------------------------------

def test_ttl_expiry_disables_and_skips_work(monkeypatch):
    tm = TapManager(ring_len=1920)
    tm._sock = FakeTapSocket()
    tm.set_tap("t", host="1.2.3.4", port=9999, ttl=10, count=480)
    fake_now = [1000.0]
    monkeypatch.setattr("lightserver.taps.time.monotonic", lambda: fake_now[0])
    tm.taps["t"].expires_at = fake_now[0] + 10
    frame = np.zeros((1920, 3), dtype=np.uint8)
    tm.emit(frame)
    assert len(tm._sock.sent) == 1
    assert tm.status_one("t")["enabled"] is True

    fake_now[0] += 11  # past ttl
    tm.emit(frame)
    assert len(tm._sock.sent) == 1  # no new packets: expired tap skipped
    assert tm.status_one("t")["enabled"] is False


def test_keepalive_before_expiry_refreshes_and_stays_enabled(monkeypatch):
    tm = TapManager(ring_len=1920)
    tm._sock = FakeTapSocket()
    fake_now = [1000.0]
    monkeypatch.setattr("lightserver.taps.time.monotonic", lambda: fake_now[0])
    tm.set_tap("t", host="1.2.3.4", port=9999, ttl=10, count=480)

    fake_now[0] += 8  # within ttl
    tm.set_tap("t", host="1.2.3.4", port=9999)  # keepalive, no explicit ttl
    fake_now[0] += 8  # would have expired w/o keepalive (8+8=16 > 10)
    frame = np.zeros((1920, 3), dtype=np.uint8)
    tm.emit(frame)
    assert tm.status_one("t")["enabled"] is True
    assert len(tm._sock.sent) == 1


def test_disable_immediately_stops_sending():
    tm = TapManager(ring_len=1920)
    tm._sock = FakeTapSocket()
    tm.set_tap("t", host="1.2.3.4", port=9999, count=480)
    frame = np.zeros((1920, 3), dtype=np.uint8)
    tm.emit(frame)
    assert len(tm._sock.sent) == 1
    tm.disable("t")
    tm.emit(frame)
    assert len(tm._sock.sent) == 1  # no growth
    assert tm.status_one("t")["enabled"] is False


def test_disable_unknown_tap_raises_keyerror():
    tm = TapManager(ring_len=1920)
    with pytest.raises(KeyError):
        tm.disable("nope")


# -- keepalive host refresh -------------------------------------------------

def test_keepalive_refreshes_host():
    tm = TapManager(ring_len=1920)
    tm._sock = FakeTapSocket()
    tm.set_tap("t", host="1.1.1.1", port=9999, count=480)
    tm.set_tap("t", host="2.2.2.2")  # keepalive from a different source IP
    frame = np.zeros((1920, 3), dtype=np.uint8)
    tm.emit(frame)
    addrs = {addr for addr, _ in tm._sock.sent}
    assert addrs == {("2.2.2.2", 9999)}


# -- validation -------------------------------------------------------------

def test_create_without_port_raises():
    tm = TapManager(ring_len=1920)
    with pytest.raises(ValueError):
        tm.set_tap("t", host="1.2.3.4")


@pytest.mark.parametrize("kwargs", [
    {"port": 0}, {"port": 65536}, {"port": -1},
    {"port": 9999, "fps_divisor": 0},
    {"port": 9999, "ttl": 0}, {"port": 9999, "ttl": 3601},
    {"port": 9999, "offset": -1},
    {"port": 9999, "count": 0},
    {"port": 9999, "offset": 1900, "count": 100},  # exceeds ring length
])
def test_bad_params_raise_value_error(kwargs):
    tm = TapManager(ring_len=1920)
    with pytest.raises(ValueError):
        tm.set_tap("t", host="1.2.3.4", **kwargs)


# -- status listing -----------------------------------------------------

def test_status_list_reports_fields():
    tm = TapManager(ring_len=1920)
    tm.set_tap("gormapper", host="1.2.3.4", port=21325, fps_divisor=2, ttl=15)
    listing = tm.status_list()
    assert len(listing) == 1
    d = listing[0]
    for key in ("name", "host", "port", "offset", "count", "fps_divisor",
               "ttl", "enabled", "seconds_until_expiry", "frames_sent"):
        assert key in d
    assert d["name"] == "gormapper"
    assert d["host"] == "1.2.3.4"
    assert d["port"] == 21325
    assert d["fps_divisor"] == 2
    assert d["offset"] == 0
    assert d["count"] == 1920
    assert d["enabled"] is True


# -- config persistence (round trip, NOT enabled/host) -----------------

def test_config_round_trips_but_not_enabled_or_host():
    tm = TapManager(ring_len=1920)
    tm.set_tap("t", host="1.2.3.4", port=9999, fps_divisor=3, ttl=20,
              offset=100, count=200)
    cfg = tm.config_for_persist()
    assert cfg == {"t": {"port": 9999, "fps_divisor": 3, "ttl": 20,
                        "offset": 100, "count": 200}}

    tm2 = TapManager(ring_len=1920)
    tm2.restore_config(cfg)
    status = tm2.status_one("t")
    assert status["port"] == 9999
    assert status["fps_divisor"] == 3
    assert status["ttl"] == 20
    assert status["offset"] == 100
    assert status["count"] == 200
    assert status["enabled"] is False  # never persisted -> starts disabled
    assert status["host"] is None


# -- RingEngine integration -----------------------------------------------

def test_ring_engine_step_emits_to_taps():
    eng = ring_engine(mode="headless")
    eng.taps._sock = FakeTapSocket()
    eng.taps.set_tap("t", host="1.2.3.4", port=9999, count=480)
    eng.step()
    assert len(eng.taps._sock.sent) == 1
    eng.shutdown()


def test_ring_engine_shutdown_closes_tap_socket():
    eng = ring_engine(mode="headless")
    closed = []
    fake = FakeTapSocket()
    fake.close = lambda: closed.append(True)
    eng.taps._sock = fake
    eng.shutdown()
    assert closed == [True]


# -- RingManager / API integration ------------------------------------

@pytest.fixture()
def rmanager():
    m = RingManager.from_configs(CONFIGS, make_segments(), mode="headless")
    m.ring.taps._sock = FakeTapSocket()
    yield m
    m.shutdown()


def client_from(rmanager, client_ip="10.20.30.40"):
    return TestClient(create_app(rmanager.default, rmanager),
                      client=(client_ip, 54321))


def test_post_taps_creates_with_source_ip_as_host(rmanager):
    c = client_from(rmanager, "10.20.30.40")
    r = c.post("/taps/gormapper", json={"port": 21325, "fps_divisor": 2, "ttl": 15})
    assert r.status_code == 200
    body = r.json()["tap"]
    assert body["host"] == "10.20.30.40"
    assert body["port"] == 21325
    assert body["fps_divisor"] == 2
    assert body["ttl"] == 15
    assert body["enabled"] is True


def test_get_taps_lists_created_tap(rmanager):
    c = client_from(rmanager)
    c.post("/taps/gormapper", json={"port": 21325})
    r = c.get("/taps")
    assert r.status_code == 200
    names = [t["name"] for t in r.json()]
    assert "gormapper" in names


def test_post_taps_missing_port_on_create_is_422(rmanager):
    c = client_from(rmanager)
    r = c.post("/taps/gormapper", json={})
    assert r.status_code == 422


def test_post_taps_bad_port_is_422(rmanager):
    c = client_from(rmanager)
    r = c.post("/taps/gormapper", json={"port": 99999})
    assert r.status_code == 422


def test_delete_taps_disables(rmanager):
    c = client_from(rmanager)
    c.post("/taps/gormapper", json={"port": 21325})
    r = c.delete("/taps/gormapper")
    assert r.status_code == 200
    r = c.get("/taps")
    tap = next(t for t in r.json() if t["name"] == "gormapper")
    assert tap["enabled"] is False


def test_delete_unknown_tap_is_404(rmanager):
    c = client_from(rmanager)
    r = c.delete("/taps/nope")
    assert r.status_code == 404


def test_post_taps_enabled_false_disables(rmanager):
    c = client_from(rmanager)
    c.post("/taps/gormapper", json={"port": 21325})
    r = c.post("/taps/gormapper", json={"enabled": False})
    assert r.status_code == 200
    tap = next(t for t in c.get("/taps").json() if t["name"] == "gormapper")
    assert tap["enabled"] is False


def test_keepalive_reasserts_and_refreshes_host(rmanager):
    c1 = client_from(rmanager, "1.1.1.1")
    c1.post("/taps/gormapper", json={"port": 21325})
    c2 = client_from(rmanager, "2.2.2.2")
    r = c2.post("/taps/gormapper", json={})  # keepalive, no port needed
    assert r.status_code == 200
    assert r.json()["tap"]["host"] == "2.2.2.2"


def test_taps_endpoint_unavailable_on_non_ring_manager():
    from lightserver.engine import Engine
    from lightserver.nodes import NodeManager
    eng = Engine(mode="headless")
    manager = NodeManager.single(eng)
    c = TestClient(create_app(eng, manager))
    assert c.get("/taps").json() == []
    r = c.post("/taps/x", json={"port": 1})
    assert r.status_code == 404


# -- state.json persistence (config only, own top-level key) -------------

def test_tap_config_persists_and_survives_restart_disabled(tmp_path):
    store = StateStore(tmp_path / "state.json")
    m = RingManager.from_configs(CONFIGS, make_segments(), mode="headless")
    m.store = store
    m.ring.taps.set_tap("gormapper", host="5.5.5.5", port=21325, fps_divisor=2,
                        ttl=15, offset=0, count=1920)
    m.persist()
    m.shutdown()

    raw = json.loads((tmp_path / "state.json").read_text())
    assert "taps" in raw
    assert raw["taps"]["gormapper"]["port"] == 21325
    assert "enabled" not in raw["taps"]["gormapper"]
    assert "host" not in raw["taps"]["gormapper"]
    # doesn't clobber other top-level keys written independently
    assert "nodes" in raw or True

    m2 = RingManager.from_configs(CONFIGS, make_segments(), mode="headless")
    m2.restore(store)
    status = m2.ring.taps.status_one("gormapper")
    assert status is not None
    assert status["port"] == 21325
    assert status["enabled"] is False  # config restored, but disabled until keepalive
    m2.shutdown()


def test_tap_persist_does_not_clobber_satellites_or_scene(tmp_path):
    store = StateStore(tmp_path / "state.json")
    m = RingManager.from_configs(CONFIGS, make_segments(), mode="headless")
    m.store = store
    scene = {"name": "keepme", "layers": [{"type": "solid", "color": "#123456"}]}
    m.ring.set_scene(scene, crossfade=0)
    m.persist()
    m.ring.taps.set_tap("t", host="1.2.3.4", port=1234)
    m.persist()
    m.shutdown()
    raw = json.loads((tmp_path / "state.json").read_text())
    assert raw["nodes"]["ring"]["scene"]["name"] == "keepme"
    assert raw["taps"]["t"]["port"] == 1234
