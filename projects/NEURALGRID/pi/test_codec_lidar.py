#!/usr/bin/env python3
"""Unit tests for neuralgrid.codec_lidar — pure, no hardware/pyserial needed.

    python3 test_codec_lidar.py            # run all tests
    python3 -m unittest test_codec_lidar   # equivalent

Covers the fw3/fw4 LIDAR (0x13) / LIDARDIAG (0x14) parsers (pre-existing)
and the fw5 latency-work additions: ACK (0x15), TIMING (0x16), and the
LOCAL_ECHO (0x24) down-frame builder.
"""

from __future__ import annotations

import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from neuralgrid import codec, codec_lidar  # noqa: E402


class TestLidar(unittest.TestCase):
    def test_parse_lidar_roundtrip(self):
        body = struct.pack("<4H", 100, 200, 0xFFFF, 4000)
        lid = codec_lidar.parse_lidar(body)
        self.assertEqual(lid.ranges, (100, 200, 0xFFFF, 4000))
        self.assertTrue(lid.valid(0))
        self.assertFalse(lid.valid(2))
        self.assertEqual(lid.n_valid, 3)

    def test_parse_lidar_bad_length(self):
        with self.assertRaises(codec.FrameError):
            codec_lidar.parse_lidar(b"\x00" * 6)


class TestLidarDiag(unittest.TestCase):
    def _make_body(self, fail_stage=None):
        alone_mask = 0b0101  # N, E respond alone
        zombie = 0
        bitmap = bytearray(16)
        for addr in (0x2E, 0x2F, 0x30, 0x31):
            bitmap[addr >> 3] |= 1 << (addr & 7)
        body = bytes([alone_mask, zombie]) + bytes(bitmap)
        if fail_stage is not None:
            body += bytes(fail_stage)
        return body

    def test_parse_fw3_body(self):
        d = codec_lidar.parse_lidardiag(self._make_body())
        self.assertEqual(d.responded_alone, (True, False, True, False))
        self.assertFalse(d.zombie)
        self.assertTrue(d.alone_meaningful)
        self.assertIn(0x2E, d.ack_addrs)
        self.assertIsNone(d.fail_stage)

    def test_parse_fw4_body_with_fail_stage(self):
        d = codec_lidar.parse_lidardiag(self._make_body(fail_stage=[0, 1, 2, 4]))
        self.assertEqual(d.fail_stage, (0, 1, 2, 4))

    def test_zombie_masks_alone_meaningful(self):
        body = bytearray(self._make_body())
        body[1] = 1  # zombie
        d = codec_lidar.parse_lidardiag(bytes(body))
        self.assertTrue(d.zombie)
        self.assertFalse(d.alone_meaningful)

    def test_bad_length_rejected(self):
        with self.assertRaises(codec.FrameError):
            codec_lidar.parse_lidardiag(b"\x00" * 5)


class TestAck(unittest.TestCase):
    def test_parse_ack_roundtrip(self):
        body = struct.pack("<IIB", 1000, 1250, 0x20)
        ack = codec_lidar.parse_ack(body)
        self.assertEqual(ack.recv_us, 1000)
        self.assertEqual(ack.done_us, 1250)
        self.assertEqual(ack.acked_type, 0x20)
        self.assertEqual(ack.apply_us, 250)

    def test_apply_us_wraps_safely(self):
        # done_us wrapped past 2**32 relative to recv_us
        body = struct.pack("<IIB", 0xFFFFFFF0, 0x00000010, 0x21)
        ack = codec_lidar.parse_ack(body)
        self.assertEqual(ack.apply_us, 0x20)  # 16 + 16, across the wrap

    def test_bad_length_rejected(self):
        with self.assertRaises(codec.FrameError):
            codec_lidar.parse_ack(b"\x00" * 8)


class TestTiming(unittest.TestCase):
    def test_parse_timing_roundtrip(self):
        vals = (10, 20, 30, 40, 50, 60, 7000)
        body = struct.pack("<7H", *vals)
        t = codec_lidar.parse_timing(body)
        self.assertEqual(t.phases, vals)
        d = t.as_dict()
        self.assertEqual(d["loop_total"], 7000)
        self.assertEqual(set(d.keys()), set(codec_lidar.TIMING_PHASES))

    def test_bad_length_rejected(self):
        with self.assertRaises(codec.FrameError):
            codec_lidar.parse_timing(b"\x00" * 10)


class TestLocalEcho(unittest.TestCase):
    def test_local_echo_frame_decodes(self):
        frame = codec_lidar.local_echo(True)
        # frame = COBS(...) || 0x00; strip the delimiter and decode.
        self.assertEqual(frame[-1], 0x00)
        msg_type, body = codec.decode_frame(frame[:-1])
        self.assertEqual(msg_type, codec_lidar.TYPE_LOCAL_ECHO)
        self.assertEqual(body, b"\x01")

    def test_local_echo_off(self):
        frame = codec_lidar.local_echo(False)
        msg_type, body = codec.decode_frame(frame[:-1])
        self.assertEqual(body, b"\x00")


class TestLidarHeal(unittest.TestCase):
    """fw7: TYPE_LIDAR_HEAL (0x25) down-frame builder — same shape as
    local_echo above."""

    def test_lidar_heal_frame_decodes(self):
        frame = codec_lidar.lidar_heal(True)
        self.assertEqual(frame[-1], 0x00)
        msg_type, body = codec.decode_frame(frame[:-1])
        self.assertEqual(msg_type, codec_lidar.TYPE_LIDAR_HEAL)
        self.assertEqual(body, b"\x01")

    def test_lidar_heal_off(self):
        frame = codec_lidar.lidar_heal(False)
        msg_type, body = codec.decode_frame(frame[:-1])
        self.assertEqual(body, b"\x00")


class TestFsrAnalog(unittest.TestCase):
    """fw6: 4 FSR channels appended to ANALOG (n: 4 -> 8), channel order
    [0..3]=softpot N/S/E/W (unchanged) [4..7]=FSR 0..3. codec.py itself is
    frozen and NOT modified for this — parse_analog/parse_hello were
    already generic in n (PROTOCOL.md: "Adding FSR/piezo later only
    increases n; the contract does not change"), so these tests exercise
    the existing frozen parsers at n=8 to prove that claim, rather than
    testing anything new added to codec.py."""

    def test_parse_analog_8_channels(self):
        vals = (100, 200, 300, 400, 10, 20, 30, 40)
        body = bytes([8]) + struct.pack("<8H", *vals)
        a = codec.parse_analog(body)
        self.assertEqual(a.values, vals)
        # indices 0-3 (softpot) unchanged in meaning; 4-7 are the new FSRs
        self.assertEqual(a.values[:4], (100, 200, 300, 400))
        self.assertEqual(a.values[4:], (10, 20, 30, 40))

    def test_parse_analog_8_channels_bad_length_rejected(self):
        # n says 8 but only 7 values present
        body = bytes([8]) + struct.pack("<7H", *([0] * 7))
        with self.assertRaises(codec.FrameError):
            codec.parse_analog(body)

    def test_parse_analog_still_accepts_4_channels(self):
        # backwards compatibility: older firmware (fw < 6) sends n=4
        vals = (1, 2, 3, 4)
        body = bytes([4]) + struct.pack("<4H", *vals)
        a = codec.parse_analog(body)
        self.assertEqual(a.values, vals)

    def test_hello_n_analog_8_fw_6(self):
        body = bytes([1, 8, 8, 8, 6])  # proto_ver, grid_w, grid_h, n_analog, fw_ver
        h = codec.parse_hello(body)
        self.assertEqual(h.n_analog, 8)
        self.assertEqual(h.fw_ver, 6)

    def test_parse_up_message_ext_analog_8_channels_via_hello_fallback(self):
        # parse_up_message_ext falls back to codec.parse_up_message for
        # ANALOG (not a lidar-specific type) -- prove that path also
        # handles n=8 correctly end-to-end through the ext dispatcher.
        vals = tuple(range(8))
        body = bytes([8]) + struct.pack("<8H", *vals)
        a = codec_lidar.parse_up_message_ext(codec.TYPE_ANALOG, body)
        self.assertEqual(a.values, vals)


class TestExtDispatch(unittest.TestCase):
    def test_parse_up_message_ext_covers_new_types(self):
        ack_body = struct.pack("<IIB", 1, 2, 0x20)
        self.assertIsInstance(
            codec_lidar.parse_up_message_ext(codec_lidar.TYPE_ACK, ack_body),
            codec_lidar.LedAck,
        )
        timing_body = struct.pack("<7H", *([0] * 7))
        self.assertIsInstance(
            codec_lidar.parse_up_message_ext(codec_lidar.TYPE_TIMING, timing_body),
            codec_lidar.Timing,
        )

    def test_falls_back_to_base_codec(self):
        hello_body = bytes([1, 8, 8, 4, 5])
        msg = codec_lidar.parse_up_message_ext(codec.TYPE_HELLO, hello_body)
        self.assertEqual(msg.fw_ver, 5)


if __name__ == "__main__":
    unittest.main()
