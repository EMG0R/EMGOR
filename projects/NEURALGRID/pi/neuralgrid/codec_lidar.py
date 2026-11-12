"""Lidar extension to the neuralGrid codec — ADDITIVE, does not edit codec.py.

grid_link_lidar firmware adds one upstream message on top of PROTOCOL.md:

    TYPE_LIDAR = 0x13
    BODY = 4 x u16 (little-endian) range in millimetres, channel order
    [0]=N, [1]=S, [2]=E, [3]=W (same convention as the softpot channels).
    0xFFFF = invalid: sensor dead/absent, measurement failed, or data stale.

Framing/CRC are identical to every other message (COBS + CRC16-CCITT-FALSE);
this module only supplies the typed body parser and a parse_up_message
extension that falls back to the original codec for all other types.

Like codec.py, this module is pure: no pyserial, importable anywhere.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass
from typing import Tuple

from . import codec

# Upstream message types added by grid_link_lidar (0x12 left free
# deliberately; 0x13/0x14 verified unused by docs/PROTOCOL.md).
TYPE_LIDAR = 0x13
TYPE_LIDARDIAG = 0x14
# fw5 latency-work additions (0x15/0x16 up, 0x24 down — see
# firmware/esp32/grid_link_lidar/README.md "Latency work (fw5)").
TYPE_ACK = 0x15       # up: LED-apply timing ack (recv_us, done_us, acked_type)
TYPE_TIMING = 0x16    # up: per-phase loop timing (debug telemetry)
TYPE_LOCAL_ECHO = 0x24  # down: u8 0/1, default 0 — Pi-gated key->LED echo
# fw7 addition (idle-LED flicker investigation, HANDOFF.md §12) — see
# firmware README's "fw7" section.
TYPE_LIDAR_HEAL = 0x25  # down: u8 0/1, default 1 (ON) — Pi-gated lidar self-heal pause

N_LIDAR = 4
LIDAR_INVALID = 0xFFFF  # dead / absent / failed / stale measurement
LIDAR_CHANNEL_NAMES = ("N", "S", "E", "W")
LIDAR_DEFAULT_ADDR = 0x29  # factory VL53L1X address (diag probes this)


@dataclass(frozen=True)
class Lidar:
    """One LIDAR frame: 4 ranges in mm, 0xFFFF = invalid/dead/timeout."""

    ranges: Tuple[int, ...]  # exactly 4 entries, mm, 0..0xFFFE valid

    def valid(self, i: int) -> bool:
        """True if channel i (0=N 1=S 2=E 3=W) holds a real measurement."""
        return self.ranges[i] != LIDAR_INVALID

    @property
    def n_valid(self) -> int:
        return sum(1 for r in self.ranges if r != LIDAR_INVALID)


def parse_lidar(body: bytes) -> Lidar:
    """Parse a TYPE_LIDAR body (strictly 8 bytes: 4 x u16 LE)."""
    expected = N_LIDAR * 2
    if len(body) != expected:
        raise codec.FrameError(
            f"LIDAR body must be {expected} bytes (4 x u16 LE), got {len(body)}"
        )
    ranges = struct.unpack(f"<{N_LIDAR}H", body)
    return Lidar(ranges)


@dataclass(frozen=True)
class LidarDiag:
    """Boot-time XSHUT diagnostic (TYPE_LIDARDIAG, fw_ver >= 3).

    Firmware procedure: all four XSHUT held LOW -> full bus scan (ack_addrs);
    anything ACKing 0x29 then is a ZOMBIE — a sensor whose XSHUT line is not
    actually resetting it (dangling wire / cold joint). Then each XSHUT is
    raised alone and 0x29 probed -> responded_alone per channel.
    """

    responded_alone: Tuple[bool, ...]  # 4 entries, order N/S/E/W
    zombie: bool                       # device stuck at 0x29 with all XSHUT low
    ack_addrs: Tuple[int, ...]         # every address that ACKed in the scan
    # fw >= 4: live per-channel last bring-up failure stage; None on fw 3.
    # 0 ok, 1 0x29-busy, 2 init failed, 3 config failed, 4 started-then-silent
    fail_stage: Tuple[int, ...] | None = None

    @property
    def alone_meaningful(self) -> bool:
        """responded_alone is trustworthy only when no zombie exists (a
        zombie ACKs 0x29 permanently, so every channel probe reads True)."""
        return not self.zombie


# Human-readable names for LidarDiag.fail_stage values (fw >= 4).
FAIL_STAGE_NAMES = {
    0: "ok",
    1: "0x29 busy (bus conflict)",
    2: "init failed (no ACK / model-ID / osc-cal)",
    3: "config write failed",
    4: "started, then silent (no data-ready 5s)",
}


def parse_lidardiag(body: bytes) -> LidarDiag:
    """Parse a TYPE_LIDARDIAG body: alone_mask:u8, zombie:u8, 16-byte ACK
    bitmap of the all-XSHUT-low bus scan (address a -> byte a>>3, bit a&7),
    plus (fw >= 4) 4 bytes of live per-channel bring-up failure stage."""
    base = 2 + 16
    if len(body) not in (base, base + N_LIDAR):
        raise codec.FrameError(
            f"LIDARDIAG body must be {base} (fw3) or {base + N_LIDAR} (fw4+) "
            f"bytes, got {len(body)}"
        )
    alone_mask, zombie = body[0], body[1]
    bitmap = body[2:base]
    ack = tuple(
        a for a in range(0x80) if (bitmap[a >> 3] >> (a & 7)) & 1
    )
    fail_stage = tuple(body[base:]) if len(body) > base else None
    return LidarDiag(
        responded_alone=tuple(bool(alone_mask & (1 << i)) for i in range(N_LIDAR)),
        zombie=bool(zombie),
        ack_addrs=ack,
        fail_stage=fail_stage,
    )


@dataclass(frozen=True)
class LedAck:
    """fw5 TYPE_ACK (0x15): timing for one APPLIED (not coalesced-away)
    LEDS_FULL/LEDS_PARTIAL frame. recv_us/done_us are raw ESP32 micros()
    snapshots (32-bit, wraps ~71min — callers comparing across a wrap
    should use the wraparound-safe helper below); done_us - recv_us (mod
    2**32) is the receive-complete -> show()-complete latency this
    firmware round-trip is trying to minimize.
    """

    recv_us: int
    done_us: int
    acked_type: int  # 0x20 (LEDS_FULL) or 0x21 (LEDS_PARTIAL)

    @property
    def apply_us(self) -> int:
        """Wraparound-safe recv->done delta in microseconds."""
        return (self.done_us - self.recv_us) & 0xFFFFFFFF


# Order matches firmware tickTiming(): [serial, keys, analog, lidar_poll,
# lidar_report, led_apply, loop_total] — max microseconds spent in that
# phase since the last report (then reset firmware-side).
TIMING_PHASES = (
    "serial", "keys", "analog", "lidar_poll", "lidar_report",
    "led_apply", "loop_total",
)


@dataclass(frozen=True)
class Timing:
    """fw5 TYPE_TIMING (0x16): per-phase max-microseconds-since-last-report
    debug telemetry, sent every ~500ms. A phase reading 0 means it simply
    never ran (e.g. led_apply with no LED frame that window) — that is
    useful signal, not missing data."""

    phases: Tuple[int, ...]  # 7 entries, order = TIMING_PHASES

    def as_dict(self) -> dict:
        return dict(zip(TIMING_PHASES, self.phases))


def parse_ack(body: bytes) -> LedAck:
    """Parse a TYPE_ACK body: recv_us:u32 LE, done_us:u32 LE, acked_type:u8."""
    if len(body) != 9:
        raise codec.FrameError(f"ACK body must be 9 bytes, got {len(body)}")
    recv_us, done_us, acked_type = struct.unpack("<IIB", body)
    return LedAck(recv_us, done_us, acked_type)


def parse_timing(body: bytes) -> Timing:
    """Parse a TYPE_TIMING body: 7 x u16 LE max-microseconds."""
    n = len(TIMING_PHASES)
    expected = n * 2
    if len(body) != expected:
        raise codec.FrameError(
            f"TIMING body must be {expected} bytes ({n} x u16 LE), got {len(body)}"
        )
    return Timing(struct.unpack(f"<{n}H", body))


def local_echo(enabled: bool) -> bytes:
    """Build a TYPE_LOCAL_ECHO (0x24) down-frame: u8 0/1. Default OFF —
    the Pi must explicitly opt in. See firmware README for the semantics
    (key press -> immediate white pad; release -> restores the last color
    the Pi actually commanded, i.e. never overrides the Pi's authority)."""
    return codec.encode_frame(TYPE_LOCAL_ECHO, bytes([1 if enabled else 0]))


def lidar_heal(enabled: bool) -> bytes:
    """Build a TYPE_LIDAR_HEAL (0x25, fw7+) down-frame: u8 0/1. Default ON
    firmware-side — lets the Pi pause the 5s dead-channel re-init
    round-robin (e.g. for the duration of a performance) if it turns out
    to be involved in the LED flicker (see HANDOFF.md §12 / firmware
    README's "fw7" section). No-op against older firmware, which silently
    ignores unknown down-frame types."""
    return codec.encode_frame(TYPE_LIDAR_HEAL, bytes([1 if enabled else 0]))


def parse_up_message_ext(msg_type: int, body: bytes):
    """parse_up_message with LIDAR + fw5 latency-message support: returns a
    Lidar for TYPE_LIDAR, a LidarDiag for TYPE_LIDARDIAG, a LedAck for
    TYPE_ACK, a Timing for TYPE_TIMING, otherwise defers to
    codec.parse_up_message (which raises FrameError for genuinely unknown
    types). codec.py is not modified.
    """
    if msg_type == TYPE_LIDAR:
        return parse_lidar(body)
    if msg_type == TYPE_LIDARDIAG:
        return parse_lidardiag(body)
    if msg_type == TYPE_ACK:
        return parse_ack(body)
    if msg_type == TYPE_TIMING:
        return parse_timing(body)
    return codec.parse_up_message(msg_type, body)
