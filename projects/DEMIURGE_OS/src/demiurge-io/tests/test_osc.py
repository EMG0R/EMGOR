"""Tests for the minimal OSC encoder — decode by hand against the OSC 1.0
wire spec rather than round-tripping through a library (we deliberately
don't depend on one)."""
import struct

from demiurge_io.osc import encode_osc


def _decode_osc_string(b: bytes, offset: int):
    end = b.index(b"\x00", offset)
    s = b[offset:end].decode("ascii")
    total_len = end - offset + 1
    total_len += (-total_len) % 4
    return s, offset + total_len


def test_encode_cc_message_ints():
    packet = encode_osc("/cc", 1, 20, 127)
    addr, off = _decode_osc_string(packet, 0)
    assert addr == "/cc"
    tags, off = _decode_osc_string(packet, off)
    assert tags == ",iii"
    ch, cc, val = struct.unpack_from(">iii", packet, off)
    assert (ch, cc, val) == (1, 20, 127)


def test_encode_p_message_int_float():
    packet = encode_osc("/p", 30, 0.75)
    addr, off = _decode_osc_string(packet, 0)
    assert addr == "/p"
    tags, off = _decode_osc_string(packet, off)
    assert tags == ",if"
    pid, = struct.unpack_from(">i", packet, off)
    off += 4
    pval, = struct.unpack_from(">f", packet, off)
    assert pid == 30
    assert abs(pval - 0.75) < 1e-6


def test_encode_sensor_message_string_float():
    packet = encode_osc("/sensor", "tof1", 0.5)
    addr, off = _decode_osc_string(packet, 0)
    assert addr == "/sensor"
    tags, off = _decode_osc_string(packet, off)
    assert tags == ",sf"
    sid, off = _decode_osc_string(packet, off)
    assert sid == "tof1"
    val, = struct.unpack_from(">f", packet, off)
    assert abs(val - 0.5) < 1e-6


def test_packet_length_always_multiple_of_4():
    for args in [("/cc", 1, 2, 3), ("/p", 1, 0.1), ("/sensor", "x", 0.0),
                 ("/a",), ("/longer/address/here", 1, 2, 3, 4)]:
        packet = encode_osc(*args)
        assert len(packet) % 4 == 0


def test_bool_rejected_to_avoid_int_ambiguity():
    import pytest
    with pytest.raises(TypeError):
        encode_osc("/cc", True, 1, 2)
