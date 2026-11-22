"""serial_bridge body mapping vs the measured 2026-10-22 captures.

The USB-MIDI side of each capture step is the engine's canonical numbering
(TEENSYY.ino tables); the bridge must emit exactly the same CC + direction.
"""
import json
import os
import sys

import pytest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "csound"))
pytest.importorskip("mido")
import serial_bridge as sb  # noqa: E402

DOCS = os.path.join(HERE, "..", "docs")


def _line(**kv):
    return "NEPTR2 t=1 " + " ".join(f"{k}={v}" for k, v in kv.items())


# serial index -> (position, rot CC, click CC), CW already normalised to 127
POSITIONS = {
    "e2": ("top-left  PREV",  26, 76), "e4": ("top-right NEXT", 24, 74),
    "e5": ("bottom1 slot4",   25, 75), "e1": ("bottom2 slot3", 21, 71),
    "e3": ("bottom3 slot2",   22, 72), "e6": ("bottom4 slot1", 23, 73),
    "e7": ("bottom5 slot0",   20, 70),
}


@pytest.mark.parametrize("e", sorted(POSITIONS))
def test_encoder_cw_ccw_click(e):
    _, rot, click = POSITIONS[e]
    n = e[1:]
    sgn = -1 if sb.ENCODER_FLIP[e] else 1      # raw serial delta is negative for CW on flipped encoders
    assert sb.encoder_events(sb.parse_frame(_line(**{e: 2 * sgn}))) == [(rot, 127)] * 2
    assert sb.encoder_events(sb.parse_frame(_line(**{e: -3 * sgn}))) == [(rot, 0)] * 3
    assert sb.ENC_BTN_CCS["eb" + n] == click


def test_buttons_firmware_order():
    # ARCADE, then UP DOWN LEFT RIGHT A B X Y == CC 60, 61..68
    exp = {"b1": 60, "b6": 61, "b9": 62, "b8": 63, "b7": 64,
           "b2": 65, "b5": 66, "b4": 67, "b3": 68}
    assert sb.BUTTON_CCS == exp


def test_idle_frame_emits_nothing():
    assert sb.encoder_events(sb.parse_frame(_line(e1=0, e2=0, e7=0))) == []


def _captures():
    for f in ("control_capture_partial_214416.json",
              "control_capture_20261022-214545.json"):
        for step in json.load(open(os.path.join(DOCS, f))):
            yield step


def test_matches_measured_usb_numbering():
    # Every measured step: what the Teensy sent on USB is what the bridge emits
    # for the same gesture. UP/RIGHT excluded (duplicate in the capture, TODO).
    for step in _captures():
        name = step["step"]
        if name in ("button UP", "button RIGHT"):
            continue
        usb = {k: v for k, v in step["events"].items() if k.startswith("Teensy")}
        for key, vals in usb.items():
            cc = int(key.rsplit("#", 1)[1])
            if cc in sb.ENCODER_CCS.values():
                e = next(f for f, c in sb.ENCODER_CCS.items() if c == cc)
                want = 0 if "COUNTER" in name else 127
                got = sb.encoder_events(
                    sb.parse_frame(_line(**{e: (-1 if sb.ENCODER_FLIP[e] else 1)
                                            * (-1 if want == 0 else 1)})))
                assert got == [(cc, want)], name
            else:
                assert cc in list(sb.BUTTON_CCS.values()) + list(sb.ENC_BTN_CCS.values()), name
