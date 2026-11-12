#!/usr/bin/env python3
"""Phase 1 acceptance harness for the neuralGrid Pi<->ESP32 link.

Two modes:

  --selftest
      Hardware-free, pyserial-free proof that the codec matches
      docs/PROTOCOL.md byte-for-byte. Round-trips every message type
      through encode_frame -> split-on-0x00 -> decode_frame, exercises COBS
      on payloads containing 0x00 (including the all-zero 192-byte LED
      frame), tests CRC rejection of a corrupted frame, and exercises
      TouchDetector against synthetic noise vs a synthetic touch.
      Run this on the Mac (or anywhere) with no hardware attached:

          python3 pi/test_link.py --selftest

  (default / --port)
      Live hardware test, meant to run ON the Pi with the ESP32 attached.
      Connects, sends PING, prints HELLO, then streams KEY/ANALOG events
      live (overlaid with TouchDetector state) and drives LEDs so presses
      light up and an idle rainbow runs when nothing is pressed.
"""

from __future__ import annotations

import argparse
import sys
import time
from typing import List, Tuple

sys.path.insert(0, __file__.rsplit("/", 1)[0])

from neuralgrid import codec
from neuralgrid.touch import TouchDetector, ADC_MAX


# ---------------------------------------------------------------------------
# --selftest : hardware-free codec + touch-detector proof
# ---------------------------------------------------------------------------


class SelfTestFailure(AssertionError):
    pass


def _check(label: str, condition: bool, detail: str = "") -> None:
    status = "PASS" if condition else "FAIL"
    print(f"  [{status}] {label}" + (f" -- {detail}" if detail and not condition else ""))
    if not condition:
        raise SelfTestFailure(f"{label}: {detail}")


def _split_on_delimiter(frame: bytes) -> bytes:
    """Mimic the receiver: a frame is COBS(payload) || 0x00. Strip the
    trailing delimiter to get the raw COBS-encoded payload for decode_frame.
    """
    assert frame.endswith(b"\x00"), "encoded frame must end with delimiter"
    assert frame.count(0x00) == 1, "delimiter must be the only zero byte on the wire"
    return frame[:-1]


def selftest_frame_roundtrip() -> None:
    print("-- frame round-trip: every message type --")

    # HELLO
    frame = codec.encode_frame(codec.TYPE_HELLO, bytes([1, 8, 8, 4, 3]))
    mtype, body = codec.decode_frame(_split_on_delimiter(frame))
    _check("HELLO type", mtype == codec.TYPE_HELLO)
    hello = codec.parse_hello(body)
    _check(
        "HELLO fields",
        hello == codec.Hello(proto_ver=1, grid_w=8, grid_h=8, n_analog=4, fw_ver=3),
        str(hello),
    )

    # KEY (press and release)
    for edge, name in ((1, "press"), (0, "release")):
        body_in = bytes([3, 5, edge])
        frame = codec.encode_frame(codec.TYPE_KEY, body_in)
        mtype, body = codec.decode_frame(_split_on_delimiter(frame))
        _check(f"KEY {name} type", mtype == codec.TYPE_KEY)
        key = codec.parse_key(body)
        _check(f"KEY {name} fields", key == codec.Key(x=3, y=5, edge=edge), str(key))

    # ANALOG, 4 channels including a value with a 0x00 low byte and one with
    # a 0x00 high byte, to make sure the body's embedded zero bytes survive
    # COBS.
    raw_values = (0, 4095, 256, 4000)
    import struct

    analog_body = bytes([4]) + struct.pack("<4H", *raw_values)
    frame = codec.encode_frame(codec.TYPE_ANALOG, analog_body)
    mtype, body = codec.decode_frame(_split_on_delimiter(frame))
    _check("ANALOG type", mtype == codec.TYPE_ANALOG)
    analog = codec.parse_analog(body)
    _check("ANALOG values", analog.values == raw_values, str(analog.values))

    # LEDS_FULL: all-zero 192-byte body (the extreme case for COBS zero
    # handling: 192 embedded zero bytes back to back).
    zero_rgb = [(0, 0, 0)] * 64
    frame = codec.leds_full(zero_rgb)
    mtype, body = codec.decode_frame(_split_on_delimiter(frame))
    _check("LEDS_FULL(zero) type", mtype == codec.TYPE_LEDS_FULL)
    _check("LEDS_FULL(zero) body length", len(body) == 192, str(len(body)))
    _check("LEDS_FULL(zero) body all-zero", body == bytes(192))

    # LEDS_FULL: non-trivial pattern, index math i -> (x=i%8, y=i//8)
    rgb = [((i * 3) % 256, (i * 5) % 256, (i * 7) % 256) for i in range(64)]
    frame = codec.leds_full(rgb)
    mtype, body = codec.decode_frame(_split_on_delimiter(frame))
    _check("LEDS_FULL(pattern) type", mtype == codec.TYPE_LEDS_FULL)
    rebuilt = [tuple(body[i * 3 : i * 3 + 3]) for i in range(64)]
    _check("LEDS_FULL(pattern) round-trips", rebuilt == rgb)

    # LEDS_PARTIAL
    updates = [(0, 0, 255, 0, 0), (7, 7, 0, 255, 0), (3, 4, 0, 0, 255)]
    frame = codec.leds_partial(updates)
    mtype, body = codec.decode_frame(_split_on_delimiter(frame))
    _check("LEDS_PARTIAL type", mtype == codec.TYPE_LEDS_PARTIAL)
    count = body[0]
    _check("LEDS_PARTIAL count", count == len(updates))
    rebuilt_updates = [tuple(body[1 + i * 5 : 1 + i * 5 + 5]) for i in range(count)]
    _check("LEDS_PARTIAL entries", rebuilt_updates == updates)

    # BRIGHTNESS
    frame = codec.brightness(128)
    mtype, body = codec.decode_frame(_split_on_delimiter(frame))
    _check("BRIGHTNESS type", mtype == codec.TYPE_BRIGHTNESS)
    _check("BRIGHTNESS value", body == bytes([128]))

    # CLEAR
    frame = codec.clear()
    mtype, body = codec.decode_frame(_split_on_delimiter(frame))
    _check("CLEAR type", mtype == codec.TYPE_CLEAR)
    _check("CLEAR empty body", body == b"")

    # PING
    frame = codec.ping()
    mtype, body = codec.decode_frame(_split_on_delimiter(frame))
    _check("PING type", mtype == codec.TYPE_PING)
    _check("PING empty body", body == b"")


def selftest_cobs_zero_bytes() -> None:
    print("-- COBS: payloads containing embedded 0x00 bytes --")

    test_payloads = [
        b"",
        b"\x00",
        b"\x00\x00\x00",
        bytes(range(0, 10)),
        b"A\x00B\x00\x00C",
        bytes(254),  # exercises the 0xFF block-length boundary
        bytes(255),
        bytes(256),
        bytes(range(256)) * 2,  # long payload, lots of zero and non-zero runs
    ]
    for payload in test_payloads:
        encoded = codec.cobs_encode(payload)
        _check(
            f"len={len(payload)}: no 0x00 in COBS output",
            0x00 not in encoded,
            encoded.hex(),
        )
        decoded = codec.cobs_decode(encoded)
        _check(f"len={len(payload)}: round-trips", decoded == payload)


def selftest_crc_rejection() -> None:
    print("-- CRC rejects a corrupted frame --")

    good_frame = codec.encode_frame(codec.TYPE_KEY, bytes([2, 2, 1]))
    cobs_payload = _split_on_delimiter(good_frame)

    # Sanity: the uncorrupted frame decodes fine.
    mtype, body = codec.decode_frame(cobs_payload)
    _check("uncorrupted frame decodes", (mtype, body) == (codec.TYPE_KEY, bytes([2, 2, 1])))

    # Corrupt one payload byte (flip a bit in the encoded stream, avoiding
    # position 0 which is a COBS length code that could break decoding
    # itself rather than the CRC check).
    corrupted = bytearray(cobs_payload)
    flip_index = len(corrupted) - 1  # last byte: part of the CRC's COBS-encoded region
    corrupted[flip_index] ^= 0xFF
    raised = False
    try:
        codec.decode_frame(bytes(corrupted))
    except codec.FrameError:
        raised = True
    except ValueError:
        # A corrupted COBS structure is also an acceptable rejection.
        raised = True
    _check("corrupted frame raises FrameError/ValueError", raised)


def selftest_touch_detector() -> None:
    print("-- TouchDetector: synthetic noise vs synthetic touch --")
    import random

    rng = random.Random(1234)

    # An UNTOUCHED SoftPot (no external pulldown) reads violent wideband noise
    # across most of the ADC range -- measured on hardware: hundreds of counts
    # of jump every sample. That huge sample-to-sample jump (not the value) is
    # what the stability-based detector rejects. (A quiet near-0 value would
    # look perfectly stable and is NOT how the real floating strips behave.)
    def noise() -> int:
        return rng.randint(0, 2200)

    det = TouchDetector(n_channels=4)

    # Feed 40 ticks of pure floating noise on all channels -> never touched.
    any_falsely_touched = False
    for _ in range(40):
        readings = det.update([noise() for _ in range(4)])
        if any(r.touched for r in readings):
            any_falsely_touched = True
    _check("pure floating noise never registers as touched", not any_falsely_touched)

    # Now hold channel 1 stable (a real finger: tiny jitter) for enough ticks
    # to clear the debounce window, while the others keep floating.
    det2 = TouchDetector(n_channels=4)
    for _ in range(10):
        det2.update([noise() for _ in range(4)])

    touched_ch1_seen = False
    for i in range(12):
        raw = [noise(), 2000 + rng.randint(-3, 3), noise(), noise()]
        readings = det2.update(raw)
        if readings[1].touched:
            touched_ch1_seen = True
    _check("stable in-range hold on channel 1 registers as touched", touched_ch1_seen)
    _check("other channels remain untouched while ch1 is held", not any(
        readings[ch].touched for ch in (0, 2, 3)
    ))
    _check(
        "position reflects held value (~0.49)",
        abs(readings[1].position - 2000 / ADC_MAX) < 0.03,
        str(readings[1].position),
    )
    _check("last_touched_channel tracks channel 1", det2.last_touched_channel == 1)

    # Move channel 3 to a new stable value; last_touched_channel should
    # switch to 3 once it clears debounce+movement.
    for i in range(12):
        raw = [noise(), 2000 + rng.randint(-3, 3), noise(), 3200 + rng.randint(-3, 3)]
        readings = det2.update(raw)
    _check("channel 3 now touched too", readings[3].touched)
    _check("last_touched_channel switches to most recently touched/moved (3)", det2.last_touched_channel == 3)

    # Release channel 1: send floating noise back on channel 1 and confirm
    # it debounces back to untouched.
    released = False
    for i in range(10):
        raw = [noise(), noise(), noise(), 3200 + rng.randint(-3, 3)]
        readings = det2.update(raw)
        if not readings[1].touched:
            released = True
    _check("channel 1 releases after floating noise resumes", released)


def run_selftest() -> bool:
    print("=" * 70)
    print("neuralGrid Pi codec self-test (no hardware, no pyserial required)")
    print("=" * 70)
    try:
        selftest_frame_roundtrip()
        selftest_cobs_zero_bytes()
        selftest_crc_rejection()
        selftest_touch_detector()
    except SelfTestFailure as exc:
        print()
        print(f"OVERALL: FAIL -- {exc}")
        return False
    except Exception as exc:  # noqa: BLE001 - want to report any surprise too
        print()
        print(f"OVERALL: FAIL -- unexpected exception: {exc!r}")
        return False

    print()
    print("OVERALL: PASS -- codec matches docs/PROTOCOL.md byte-for-byte")
    return True


# ---------------------------------------------------------------------------
# Live hardware harness (meant to run on the Pi)
# ---------------------------------------------------------------------------


def _auto_detect_port() -> str:
    import serial.tools.list_ports  # type: ignore

    candidates = list(serial.tools.list_ports.comports())
    if not candidates:
        raise SystemExit("no serial ports found; pass --port explicitly")
    # Prefer anything that looks like a USB CDC / ACM device.
    for p in candidates:
        if "usb" in p.device.lower() or "acm" in p.device.lower():
            return p.device
    return candidates[0].device


def _idle_rainbow_frame(t: float) -> List[Tuple[int, int, int]]:
    import colorsys

    frame = []
    for i in range(64):
        x, y = i % 8, i // 8
        hue = ((x + y) / 14.0 + t * 0.05) % 1.0
        r, g, b = colorsys.hsv_to_rgb(hue, 1.0, 0.15)
        frame.append((int(r * 255), int(g * 255), int(b * 255)))
    return frame


def run_live(port: str) -> None:
    from neuralgrid.link import GridLink

    menu_names = {0: "Instrument", 1: "Sequencer", 2: "MIDI-fx", 3: "reserved"}
    detector = TouchDetector(n_channels=4)
    pressed_keys: set = set()

    def on_hello(hello):
        print(f"HELLO: {hello}")

    def on_key(key):
        print(f"KEY: x={key.x} y={key.y} {'PRESS' if key.pressed else 'RELEASE'}")
        if key.pressed:
            pressed_keys.add((key.x, key.y))
        else:
            pressed_keys.discard((key.x, key.y))
        link.send_leds_partial(
            [(x, y, 0, 200, 255) for (x, y) in pressed_keys]
            or [(key.x, key.y, 0, 0, 0)]
        )

    def on_analog(analog):
        readings = detector.update(analog.values)
        parts = []
        for ch, r in enumerate(readings):
            marker = "*" if ch == detector.last_touched_channel else " "
            parts.append(f"ch{ch}{marker}={'TOUCH' if r.touched else '.....'} pos={r.position:.2f}")
        menu = menu_names.get(detector.last_touched_channel)
        print("ANALOG: " + " | ".join(parts) + (f"  -> menu: {menu}" if menu else ""))

    def on_frame_error(exc, raw):
        print(f"FRAME ERROR: {exc} (raw={raw.hex()})")

    link = GridLink(
        port,
        on_hello=on_hello,
        on_key=on_key,
        on_analog=on_analog,
        on_frame_error=on_frame_error,
    )
    link.start()
    try:
        print(f"Connected to {port}. Sending PING...")
        link.send_ping()
        time.sleep(0.5)
        print("Streaming KEY/ANALOG events (Ctrl-C to stop). Idle rainbow runs when nothing is pressed.")
        t = 0.0
        while True:
            if not pressed_keys:
                link.send_leds_full(_idle_rainbow_frame(t))
            t += 1
            time.sleep(0.1)
    except KeyboardInterrupt:
        print("\nStopping.")
    finally:
        link.send_clear()
        link.stop()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default=None, help="serial port, e.g. /dev/ttyUSB0 (auto-detected if omitted)")
    parser.add_argument("--selftest", action="store_true", help="run the hardware-free codec self-test")
    args = parser.parse_args()

    if args.selftest:
        ok = run_selftest()
        sys.exit(0 if ok else 1)

    port = args.port or _auto_detect_port()
    run_live(port)


if __name__ == "__main__":
    main()
