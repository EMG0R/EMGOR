"""osc.py — the ~20-line OSC encoder the spec asks for instead of a dependency.

We only ever needed to SEND two shapes of message ("iii" and "if", plus a
telemetry "sf"), so a minimal encoder is a couple of struct.pack calls, not a
library. The parameter pool (sources/osc_in.py) now also RECEIVES, so a
matching ~40-line decoder lives at the bottom of this file; same rule, no
dependency. Kept separate from sinks/csound.py so
telemetry.py (talks to a different port/address) can reuse it without
importing the csound sink.

Wire format (OSC 1.0, the only part that matters for sending):
    <address string, null-padded to 4 bytes>
    <type-tag string ",xyz", null-padded to 4 bytes>
    <arguments, each 4 bytes big-endian: int32 for 'i', float32 for 'f'>
"""

from __future__ import annotations

import struct


def _osc_string(s: str) -> bytes:
    b = s.encode("ascii") + b"\x00"
    pad = (-len(b)) % 4
    return b + b"\x00" * pad


def encode_osc(address: str, *args) -> bytes:
    """Build one OSC message. Each arg must be int (-> 'i') or float (-> 'f')."""
    tags = ","
    body = bytearray()
    for a in args:
        if isinstance(a, bool):
            raise TypeError("bool is ambiguous as an OSC arg; pass int or float")
        if isinstance(a, int):
            tags += "i"
            body += struct.pack(">i", a)
        elif isinstance(a, float):
            tags += "f"
            body += struct.pack(">f", a)
        elif isinstance(a, str):
            tags += "s"
            body += _osc_string(a)
        else:
            raise TypeError(f"unsupported OSC arg type: {type(a)!r}")
    return _osc_string(address) + _osc_string(tags) + bytes(body)


# ── decode (parameter pool ingress) ─────────────────────────────────────────
# Tolerant by design: one malformed packet from a script or a phone must never
# raise into the ingress thread, so every failure is a None / empty list.

def _read_string(data: bytes, off: int):
    end = data.index(b"\x00", off)
    s = data[off:end].decode("ascii", errors="replace")
    n = end - off + 1
    n += (-n) % 4
    return s, off + n


def decode_osc(data: bytes):
    """One OSC message -> (address, [args]) or None if malformed.

    Handles i f d s T F (the tags a pool writer can plausibly send). An
    unsupported tag makes the whole message None rather than guessing the
    offsets of everything after it."""
    try:
        addr, off = _read_string(data, 0)
        if not addr.startswith("/"):
            return None
        tags, off = _read_string(data, off)
        if not tags.startswith(","):
            return None
        args = []
        for t in tags[1:]:
            if t == "i":
                args.append(struct.unpack_from(">i", data, off)[0]); off += 4
            elif t == "f":
                args.append(struct.unpack_from(">f", data, off)[0]); off += 4
            elif t == "d":
                args.append(struct.unpack_from(">d", data, off)[0]); off += 8
            elif t == "s":
                s, off = _read_string(data, off)
                args.append(s)
            elif t == "T":
                args.append(1)
            elif t == "F":
                args.append(0)
            else:
                return None
        return addr, args
    except (ValueError, struct.error, IndexError):
        return None


def decode_packet(data: bytes):
    """A datagram -> list of (address, args). Unwraps #bundle (nested too)."""
    if data.startswith(b"#bundle\x00"):
        out, off = [], 16            # 8 tag + 8 timetag
        while off + 4 <= len(data):
            (n,) = struct.unpack_from(">i", data, off)
            off += 4
            if n <= 0 or off + n > len(data):
                break
            out.extend(decode_packet(data[off:off + n]))
            off += n
        return out
    m = decode_osc(data)
    return [m] if m else []
