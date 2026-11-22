"""Frame Protocol v1: exact packet byte layout against the spec."""
import struct

from lightserver.protocol import (FLAG_SHOW, build_frame_packets, build_packet,
                                  parse_packet)


def make_frame(n=800):
    # deterministic non-trivial pattern
    return bytes((i * 7 + c * 31) % 256 for i in range(n) for c in range(3))


def test_800px_frame_is_two_packets_with_exact_headers():
    frame = make_frame(800)
    pkts = build_frame_packets(frame)
    assert len(pkts) == 2

    p0, p1 = pkts
    # packet 0: offset 0, count 400, flags 0
    assert p0[0:2] == b"FC"
    assert p0[2] == 1                                    # version
    assert p0[3] == 0                                    # flags: no SHOW
    assert struct.unpack("<H", p0[4:6])[0] == 0          # offset
    assert struct.unpack("<H", p0[6:8])[0] == 400        # count
    assert len(p0) == 8 + 3 * 400
    assert p0[8:] == frame[: 3 * 400]

    # packet 1: offset 400, count 400, flags bit0 = SHOW
    assert p1[0:2] == b"FC"
    assert p1[2] == 1
    assert p1[3] == FLAG_SHOW
    assert struct.unpack("<H", p1[4:6])[0] == 400
    assert struct.unpack("<H", p1[6:8])[0] == 400
    assert p1[8:] == frame[3 * 400:]


def test_little_endian_offsets():
    pkt = build_packet(0x1234, b"\x00" * 3, show=True)
    assert pkt[4] == 0x34 and pkt[5] == 0x12  # little-endian uint16
    assert pkt[3] & FLAG_SHOW


def test_parse_round_trip():
    frame = make_frame(800)
    off = 0
    for pkt in build_frame_packets(frame):
        version, flags, offset, count, rgb = parse_packet(pkt)
        assert version == 1
        assert offset == off
        assert rgb == frame[off * 3:(off + count) * 3]
        off += count
    assert off == 800


def test_partial_frame_single_packet_has_show():
    pkts = build_frame_packets(bytes(3 * 10))
    assert len(pkts) == 1
    assert pkts[0][3] & FLAG_SHOW
    assert struct.unpack("<H", pkts[0][6:8])[0] == 10
