"""RTP-MIDI (AppleMIDI) -- MIDI out over ethernet with nothing to install.

macOS speaks this natively (Audio MIDI Setup -> Network), so a laptop
sees the neuralGrid as an ordinary MIDI port and Ableton just lists it.
The wire format is fussy and unforgiving, so it is built and parsed by
pure functions that can be checked byte for byte.

Reference: RFC 6295 (RTP-MIDI) + Apple's session protocol.
"""
import struct

import pytest

from septagraph.pi.netmidi import (
    APPLEMIDI_SIGNATURE, PAYLOAD_TYPE, NetworkMidiOut, RtpMidiSession,
    build_exchange, build_rtp_midi, parse_exchange, timestamp_units,
)


class TestSessionHandshake:
    def test_an_invitation_is_recognised(self):
        pkt = build_exchange("IN", token=0xDEADBEEF, ssrc=0x11223344, name="laptop")
        msg = parse_exchange(pkt)
        assert msg.command == "IN"
        assert msg.token == 0xDEADBEEF
        assert msg.ssrc == 0x11223344
        assert msg.name == "laptop"

    def test_it_carries_the_apple_signature_and_protocol_2(self):
        pkt = build_exchange("IN", token=1, ssrc=2, name="x")
        assert pkt[:2] == APPLEMIDI_SIGNATURE
        assert pkt[2:4] == b"IN"
        assert struct.unpack(">I", pkt[4:8])[0] == 2

    def test_the_name_is_null_terminated(self):
        pkt = build_exchange("OK", token=1, ssrc=2, name="neuralGrid")
        assert pkt.endswith(b"neuralGrid\x00")

    def test_goodbye_carries_no_name(self):
        pkt = build_exchange("BY", token=1, ssrc=2, name=None)
        assert len(pkt) == 16
        assert parse_exchange(pkt).command == "BY"

    def test_a_packet_that_is_not_ours_is_not_claimed(self):
        assert parse_exchange(b"\x80\x61\x00\x01" + b"\x00" * 20) is None

    def test_a_truncated_packet_does_not_explode(self):
        assert parse_exchange(b"\xff\xffIN") is None


class TestClock:
    def test_the_clock_runs_at_ten_kilohertz(self):
        # AppleMIDI timestamps are in units of 100 microseconds
        assert timestamp_units(1.0) - timestamp_units(0.0) == 10_000

    def test_it_is_an_integer_count(self):
        assert isinstance(timestamp_units(1.2345), int)


class TestMidiOnTheWire:
    def test_a_note_rides_in_an_rtp_packet(self):
        pkt = build_rtp_midi(b"\x90\x3c\x64", seq=7, ts=1234, ssrc=0xAABBCCDD)
        assert pkt[0] == 0x80                  # version 2
        assert pkt[1] == 0x80 | PAYLOAD_TYPE   # marker + dynamic PT 97
        assert struct.unpack(">H", pkt[2:4])[0] == 7
        assert struct.unpack(">I", pkt[4:8])[0] == 1234
        assert struct.unpack(">I", pkt[8:12])[0] == 0xAABBCCDD
        assert pkt[13:] == b"\x90\x3c\x64"

    def test_a_short_payload_uses_the_one_byte_header(self):
        pkt = build_rtp_midi(b"\x90\x3c\x64", seq=0, ts=0, ssrc=0)
        header = pkt[12]
        assert header & 0x80 == 0     # B=0, short form
        assert header & 0x40 == 0     # J=0, no recovery journal
        assert header & 0x0F == 3     # three MIDI bytes

    def test_a_long_payload_switches_to_the_two_byte_header(self):
        payload = b"\x90\x3c\x64" * 8   # 24 bytes, past the 15-byte limit
        pkt = build_rtp_midi(payload, seq=0, ts=0, ssrc=0)
        assert pkt[12] & 0x80 == 0x80
        length = ((pkt[12] & 0x0F) << 8) | pkt[13]
        assert length == len(payload)
        assert pkt[14:] == payload


class TestTheOutputPort:
    """NetworkMidiOut is shaped like the existing midiout so every mode
    can talk to it without knowing it is on a network."""

    def test_it_sends_nothing_when_no_laptop_is_connected(self):
        sent = []
        out = NetworkMidiOut(_send=lambda data: sent.append(data))
        out.note_on(60, vel=100, ch=0)
        assert sent == []

    def test_a_connected_peer_gets_the_note(self):
        sent = []
        out = NetworkMidiOut(_send=lambda data: sent.append(data))
        out.connect()
        out.note_on(60, vel=100, ch=0)
        assert len(sent) == 1
        assert sent[0][-3:] == b"\x90\x3c\x64"

    def test_channel_lands_in_the_status_byte(self):
        sent = []
        out = NetworkMidiOut(_send=lambda d: sent.append(d))
        out.connect()
        out.note_on(60, vel=100, ch=3)
        assert sent[0][-3] == 0x93

    def test_note_off_and_cc_are_standard(self):
        sent = []
        out = NetworkMidiOut(_send=lambda d: sent.append(d))
        out.connect()
        out.note_off(60, ch=0)
        out.cc(74, 64, ch=0)
        assert sent[0][-3:] == b"\x80\x3c\x00"
        assert sent[1][-3:] == b"\xb0\x4a\x40"

    def test_the_sequence_number_advances(self):
        sent = []
        out = NetworkMidiOut(_send=lambda d: sent.append(d))
        out.connect()
        out.note_on(60, ch=0)
        out.note_on(61, ch=0)
        s0 = struct.unpack(">H", sent[0][2:4])[0]
        s1 = struct.unpack(">H", sent[1][2:4])[0]
        assert s1 == s0 + 1

    def test_it_wraps_instead_of_overflowing(self):
        sent = []
        out = NetworkMidiOut(_send=lambda d: sent.append(d))
        out.connect()
        out._seq = 0xFFFF
        out.note_on(60, ch=0)
        out.note_on(61, ch=0)
        # the first packet uses 0xFFFF, the next rolls to 0 rather than
        # overflowing the two-byte field
        assert struct.unpack(">H", sent[0][2:4])[0] == 0xFFFF
        assert struct.unpack(">H", sent[1][2:4])[0] == 0

    def test_values_out_of_range_are_clamped_not_corrupting(self):
        sent = []
        out = NetworkMidiOut(_send=lambda d: sent.append(d))
        out.connect()
        out.cc(74, 999, ch=0)
        assert sent[0][-1] == 127

    def test_list_ports_reports_the_session(self):
        out = NetworkMidiOut(_send=lambda d: None, name="neuralGrid")
        assert "neuralGrid" in out.list_ports()[0]


class TestTheSessionServer:
    def test_it_answers_an_invitation_with_an_acceptance(self):
        replies = []
        s = RtpMidiSession(name="neuralGrid", _send=lambda d, a: replies.append((d, a)))
        invite = build_exchange("IN", token=0x1234, ssrc=0x9999, name="laptop")
        s.handle(invite, ("192.168.0.5", 5004))
        msg = parse_exchange(replies[0][0])
        assert msg.command == "OK"
        assert msg.token == 0x1234        # the peer's token comes back
        assert msg.name == "neuralGrid"

    def test_it_answers_a_clock_sync_with_its_own_time(self):
        replies = []
        s = RtpMidiSession(name="ng", _send=lambda d, a: replies.append((d, a)))
        ck = APPLEMIDI_SIGNATURE + b"CK" + struct.pack(
            ">IBxxxQQQ", 0x9999, 0, 111, 0, 0)
        s.handle(ck, ("192.168.0.5", 5005))
        reply = replies[0][0]
        assert reply[2:4] == b"CK"
        count = reply[8]
        assert count == 1                  # responder answers count 1
        ts1 = struct.unpack(">Q", reply[12:20])[0]
        assert ts1 == 111                  # the peer's own timestamp echoed

    def test_a_goodbye_drops_the_peer(self):
        s = RtpMidiSession(name="ng", _send=lambda d, a: None)
        s.handle(build_exchange("IN", token=1, ssrc=7, name="l"), ("10.0.0.2", 5004))
        assert s.peers
        s.handle(build_exchange("BY", token=1, ssrc=7, name=None), ("10.0.0.2", 5004))
        assert not s.peers

    def test_noise_on_the_port_is_ignored(self):
        s = RtpMidiSession(name="ng", _send=lambda d, a: None)
        s.handle(b"garbage", ("10.0.0.2", 5004))
        assert not s.peers
