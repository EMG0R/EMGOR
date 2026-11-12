import pathlib

import pytest

from septagraph.types import *
from septagraph.codec import *
from tests.golden_fixtures import GOLDEN_FIXTURES, HIT_BASIC
from septagraph.axes import K

GOLDEN_DIR = pathlib.Path(__file__).parents[2] / "golden"

H = HIT_BASIC


def test_hit_roundtrip_and_address():
    pkt = encode_hit(H)
    assert pkt.startswith(b"/sept/v1/hit\0")
    assert decode(pkt) == [H] or decode(pkt)[0].mu == pytest.approx(H.mu)  # float32 tolerance
    out = decode(pkt)[0]
    assert isinstance(out, Hit)
    assert out.voice_id == "grid:7" and out.layer == 0 and out.onset is True
    assert out.velocity == pytest.approx(0.8)


def test_hit_release_onset_zero():
    (out,) = decode(encode_hit(Hit("grid:7", 1, False, 0.0, HALF, ZERO)))
    assert out.onset is False and out.layer == 1


def test_move_global():
    m = Move("", HALF)
    (out,) = decode(encode_move(m))
    assert out.voice_id == "" and out.mu == pytest.approx(HALF)


def test_shape_roundtrip():
    s = Septagraph("bow", 0, 2.5, (Knot(0, HALF, ZERO), Knot(1, vec(*[0.6] * K), vec(*[0.1] * K))))
    (out,) = decode(encode_shape(s))
    assert out.shape_id == "bow" and len(out.knots) == 2 and out.duration_s == pytest.approx(2.5)
    assert out.knots[1].mu == pytest.approx(tuple([0.6] * K))
    assert out.knots[1].sigma == pytest.approx(tuple([0.1] * K))


def test_manifest_catalog_needs_end():
    m = Manifest("lab", "catalog", "Machine Lab", None, None,
                 (CatalogPoint(0, HALF, ZERO, "breakBot", 12.0, 0.1, 1.0),
                  CatalogPoint(1, vec(*[0.3] * K), ZERO, "tammy", 8.0, 0.2, 1.0)))
    d = Decoder()
    outs = d.feed(encode_manifest(m))
    assert len(outs) == 1 and outs[0].kind == "catalog" and len(outs[0].points) == 2
    assert outs[0].points[1].instrument == "tammy"
    assert outs[0].points[0].max_rate_hz == pytest.approx(12.0)


def test_manifest_continuous_bounds():
    m = Manifest("chuck", "continuous", "sept-render-chuck", ZERO, tuple([1.0] * K), ())
    (out,) = decode(encode_manifest(m))
    assert out.kind == "continuous" and out.lo == ZERO and out.hi == tuple([1.0] * K) and out.points == ()


def test_manifest_across_packets_and_begin_invalidates():
    """begin/point/end may arrive as separate packets; a new begin discards partial state."""
    from pythonosc.osc_packet import OscPacket
    m = Manifest("lab", "catalog", "Machine Lab", None, None,
                 (CatalogPoint(0, HALF, ZERO, "a", 1.0, 0.0, 1.0),
                  CatalogPoint(1, HALF, ZERO, "b", 1.0, 0.0, 1.0)))
    msgs = [tm.message for tm in OscPacket(encode_manifest(m)).messages]
    assert len(msgs) == 4
    d = Decoder()
    assert d.feed(msgs[0].dgram) == []  # begin
    assert d.feed(msgs[1].dgram) == []  # point 0
    assert d.feed(msgs[0].dgram) == []  # begin again -> resets
    assert d.feed(msgs[2].dgram) == []  # point 1 only
    (out,) = d.feed(msgs[3].dgram)  # end
    assert [p.instrument for p in out.points] == ["b"]


def test_manifest_request_roundtrip():
    pkt = encode_manifest_request()
    assert pkt.startswith(b"/sept/v1/manifest/request\0")
    assert decode(pkt) == [ManifestRequest()]


def test_unknown_address_ignored():
    from pythonosc.osc_message_builder import OscMessageBuilder
    b = OscMessageBuilder("/sept/v1/whatever")
    b.add_arg(1)
    assert decode(b.build().dgram) == []


def test_out_of_range_is_clamped_not_dropped():
    from pythonosc.osc_message_builder import OscMessageBuilder
    b = OscMessageBuilder("/sept/v1/hit")
    for a in ("v", 0, 1, 3.0):
        b.add_arg(a)
    for _ in range(2 * K):          # mu + sigma
        b.add_arg(2.0)
    (out,) = decode(b.build().dgram)
    assert out.mu == tuple([1.0] * K) and out.velocity == 1.0


def test_malformed_message_ignored():
    from pythonosc.osc_message_builder import OscMessageBuilder
    b = OscMessageBuilder("/sept/v1/hit")
    b.add_arg("v")
    assert decode(b.build().dgram) == []
    assert decode(b"garbage") == []


@pytest.mark.parametrize("name", sorted(GOLDEN_FIXTURES))
def test_golden_bytes_stable(name):
    assert GOLDEN_FIXTURES[name]() == (GOLDEN_DIR / f"{name}.osc").read_bytes()


def test_golden_hit_basic_direct():
    assert encode_hit(H) == (GOLDEN_DIR / "hit_basic.osc").read_bytes()
