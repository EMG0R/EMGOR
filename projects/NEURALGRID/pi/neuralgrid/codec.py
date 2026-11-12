"""Pure codec functions for the neuralGrid ESP32<->Pi protocol.

Implements exactly docs/PROTOCOL.md:
    - COBS framing with 0x00 delimiter
    - payload = TYPE(1) || BODY(n) || CRC16-LE(2)
    - CRC = CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflect, no xorout)
      computed over TYPE || BODY

No serial/pyserial dependency here on purpose: this module must be importable
and testable on a machine with no hardware and no pyserial installed.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass
from typing import List, Tuple

# ---------------------------------------------------------------------------
# Message type constants
# ---------------------------------------------------------------------------

# Up: ESP32 -> Pi
TYPE_HELLO = 0x01
TYPE_KEY = 0x10
TYPE_ANALOG = 0x11

# Down: Pi -> ESP32
TYPE_LEDS_FULL = 0x20
TYPE_LEDS_PARTIAL = 0x21
TYPE_BRIGHTNESS = 0x22
TYPE_CLEAR = 0x23
TYPE_PING = 0x30

DELIMITER = 0x00

GRID_W = 8
GRID_H = 8
N_LEDS = GRID_W * GRID_H  # 64


# ---------------------------------------------------------------------------
# COBS (Consistent Overhead Byte Stuffing)
# ---------------------------------------------------------------------------


def cobs_encode(data: bytes) -> bytes:
    """Encode `data` (which may contain 0x00 bytes) into a COBS frame with
    no 0x00 bytes present anywhere in the output. The caller is responsible
    for appending the 0x00 delimiter after this.
    """
    if not isinstance(data, (bytes, bytearray)):
        raise TypeError("cobs_encode expects bytes")

    output = bytearray()
    # Reserve a byte for the first code; we backpatch code bytes as we go.
    block_start = 0
    output.append(0)  # placeholder for first code byte
    code = 1

    for byte in data:
        if byte == 0:
            output[block_start] = code
            block_start = len(output)
            output.append(0)  # placeholder
            code = 1
        else:
            output.append(byte)
            code += 1
            if code == 0xFF:
                output[block_start] = code
                block_start = len(output)
                output.append(0)  # placeholder
                code = 1

    output[block_start] = code
    return bytes(output)


def cobs_decode(data: bytes) -> bytes:
    """Decode a COBS-encoded frame (without the trailing 0x00 delimiter)
    back into the original bytes. Raises ValueError on malformed input.
    """
    if not isinstance(data, (bytes, bytearray)):
        raise TypeError("cobs_decode expects bytes")

    output = bytearray()
    i = 0
    n = len(data)

    if n == 0:
        return bytes(output)

    while i < n:
        code = data[i]
        if code == 0:
            raise ValueError("cobs_decode: unexpected zero byte in input")
        i += 1
        block_end = i + code - 1
        if block_end > n:
            raise ValueError("cobs_decode: malformed frame (code overruns buffer)")
        output.extend(data[i:block_end])
        i = block_end
        if code != 0xFF and i < n:
            output.append(0)

    return bytes(output)


# ---------------------------------------------------------------------------
# CRC-16/CCITT-FALSE
# ---------------------------------------------------------------------------

_CRC_POLY = 0x1021
_CRC_INIT = 0xFFFF


def crc16_ccitt(data: bytes) -> int:
    """CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflect, no xorout."""
    crc = _CRC_INIT
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ _CRC_POLY) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc & 0xFFFF


# ---------------------------------------------------------------------------
# Frame encode/decode
# ---------------------------------------------------------------------------


class FrameError(ValueError):
    """Raised when a frame fails to decode or fails CRC verification."""


def encode_frame(msg_type: int, body: bytes) -> bytes:
    """Build a full on-wire frame: COBS(TYPE || BODY || CRC16-LE) || 0x00."""
    if not (0 <= msg_type <= 0xFF):
        raise ValueError("msg_type must be a single byte (0..255)")
    payload = bytes([msg_type]) + bytes(body)
    crc = crc16_ccitt(payload)
    payload_with_crc = payload + struct.pack("<H", crc)
    return cobs_encode(payload_with_crc) + bytes([DELIMITER])


def decode_frame(cobs_payload_without_delimiter: bytes) -> Tuple[int, bytes]:
    """Decode a single COBS-encoded frame (no trailing 0x00) into
    (msg_type, body), verifying the CRC16. Raises FrameError on any
    decode/CRC failure.
    """
    try:
        raw = cobs_decode(cobs_payload_without_delimiter)
    except ValueError as exc:
        raise FrameError(f"COBS decode failed: {exc}") from exc

    if len(raw) < 3:
        raise FrameError("frame too short (need TYPE + CRC16 minimum)")

    payload, crc_bytes = raw[:-2], raw[-2:]
    (received_crc,) = struct.unpack("<H", crc_bytes)
    computed_crc = crc16_ccitt(payload)
    if received_crc != computed_crc:
        raise FrameError(
            f"CRC mismatch: received 0x{received_crc:04X}, "
            f"computed 0x{computed_crc:04X}"
        )

    msg_type = payload[0]
    body = payload[1:]
    return msg_type, body


# ---------------------------------------------------------------------------
# Typed helpers: DOWN message builders (Pi -> ESP32)
# ---------------------------------------------------------------------------


def leds_full(rgb_list) -> bytes:
    """Build LEDS_FULL frame body from a list of 64 (r, g, b) tuples.

    index i -> x = i % 8, y = i // 8
    """
    rgb_list = list(rgb_list)
    if len(rgb_list) != N_LEDS:
        raise ValueError(f"leds_full requires exactly {N_LEDS} (r,g,b) tuples, got {len(rgb_list)}")
    body = bytearray()
    for r, g, b in rgb_list:
        for v in (r, g, b):
            if not (0 <= v <= 255):
                raise ValueError("rgb components must be 0..255")
            body.append(v)
    return encode_frame(TYPE_LEDS_FULL, bytes(body))


def leds_partial(updates) -> bytes:
    """Build LEDS_PARTIAL frame body from a list of (x, y, r, g, b) tuples."""
    updates = list(updates)
    if len(updates) > 255:
        raise ValueError("leds_partial supports at most 255 updates (count is u8)")
    body = bytearray([len(updates)])
    for x, y, r, g, b in updates:
        for v in (x, y, r, g, b):
            if not (0 <= v <= 255):
                raise ValueError("x,y,r,g,b must all be 0..255")
        body.extend((x, y, r, g, b))
    return encode_frame(TYPE_LEDS_PARTIAL, bytes(body))


def brightness(level: int) -> bytes:
    if not (0 <= level <= 255):
        raise ValueError("level must be 0..255")
    return encode_frame(TYPE_BRIGHTNESS, bytes([level]))


def clear() -> bytes:
    return encode_frame(TYPE_CLEAR, b"")


def ping() -> bytes:
    return encode_frame(TYPE_PING, b"")


# ---------------------------------------------------------------------------
# Typed helpers: UP message parsers (ESP32 -> Pi)
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class Hello:
    proto_ver: int
    grid_w: int
    grid_h: int
    n_analog: int
    fw_ver: int


@dataclass(frozen=True)
class Key:
    x: int
    y: int
    edge: int  # 1 = press, 0 = release

    @property
    def pressed(self) -> bool:
        return self.edge == 1


@dataclass(frozen=True)
class Analog:
    values: Tuple[int, ...]  # raw ADC values, 0..4095, one per channel


def parse_hello(body: bytes) -> Hello:
    if len(body) != 5:
        raise FrameError(f"HELLO body must be 5 bytes, got {len(body)}")
    proto_ver, grid_w, grid_h, n_analog, fw_ver = body
    return Hello(proto_ver, grid_w, grid_h, n_analog, fw_ver)


def parse_key(body: bytes) -> Key:
    if len(body) != 3:
        raise FrameError(f"KEY body must be 3 bytes, got {len(body)}")
    x, y, edge = body
    return Key(x, y, edge)


def parse_analog(body: bytes) -> Analog:
    if len(body) < 1:
        raise FrameError("ANALOG body must have at least the count byte")
    n = body[0]
    expected_len = 1 + 2 * n
    if len(body) != expected_len:
        raise FrameError(
            f"ANALOG body length mismatch: n={n} implies {expected_len} bytes, got {len(body)}"
        )
    values = struct.unpack(f"<{n}H", body[1:])
    return Analog(values)


# Dispatch table from msg_type -> parser, for convenience.
UP_PARSERS = {
    TYPE_HELLO: parse_hello,
    TYPE_KEY: parse_key,
    TYPE_ANALOG: parse_analog,
}


def parse_up_message(msg_type: int, body: bytes):
    """Parse an UP message body into its typed dataclass given its msg_type.
    Raises FrameError for unknown types.
    """
    parser = UP_PARSERS.get(msg_type)
    if parser is None:
        raise FrameError(f"unknown UP message type: 0x{msg_type:02X}")
    return parser(body)
