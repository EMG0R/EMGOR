"""Tests for sources/teensy.py using a canned NEPTR2 frame sequence — no
real serial port. Verifies:
  - sensors are emitted at full raw resolution (not squashed to 7-bit MIDI)
  - encoder/button control events map to CC byte-compatibly with
    serial_bridge.py's ENCODER_CCS/BUTTON_CCS/ENC_BTN_CCS/JOY_BTN_CC tables
  - dropout sentinel (-1) passes through raw untouched (conditioning, not
    the source, is responsible for dropout policy)
"""
from demiurge_io.sources.teensy import (
    parse_frame, frame_to_readings, ControlState, ControlCC,
    SENSOR_DESCRIPTORS, CHANNEL,
)

# A short canned sequence of raw NEPTR2 lines, as the Teensy would emit them.
FRAME_SEQUENCE = [
    "NEPTR2 t=1000 e1=0 e2=0 e3=0 e4=0 e5=0 e6=0 e7=0 "
    "b1=0 b2=0 b3=0 b4=0 b5=0 b6=0 b7=0 b8=0 b9=0 "
    "eb1=0 eb2=0 eb3=0 eb4=0 eb5=0 eb6=0 eb7=0 jb=0 "
    "jx=2048 jy=1024 exp=2000 fsr1=0 fsr2=0 tof1=350 tof2=-1 "
    "ax=100 ay=-200 az=16000 ml=0 mr=0",
    "NEPTR2 t=1010 e1=2 e2=-1 e3=0 e4=0 e5=0 e6=0 e7=0 "
    "b1=1 b2=0 b3=0 b4=0 b5=0 b6=0 b7=0 b8=0 b9=0 "
    "eb1=0 eb2=0 eb3=0 eb4=0 eb5=0 eb6=0 eb7=0 jb=1 "
    "jx=4095 jy=0 exp=3992 fsr1=4095 fsr2=2500 tof1=-1 tof2=100 "
    "ax=-17000 ay=17000 az=0 ml=800 mr=400",
    "NEPTR2 t=1020 e1=0 e2=0 e3=0 e4=0 e5=0 e6=0 e7=0 "
    "b1=0 b2=0 b3=0 b4=0 b5=0 b6=0 b7=0 b8=0 b9=0 "
    "eb1=0 eb2=0 eb3=0 eb4=0 eb5=0 eb6=0 eb7=0 jb=0 "
    "jx=4095 jy=0 exp=3992 fsr1=4095 fsr2=2500 tof1=-1 tof2=100 "
    "ax=-17000 ay=17000 az=0 ml=800 mr=400",
]


def test_parse_frame_rejects_non_neptr2_lines():
    assert parse_frame("garbage") is None
    assert parse_frame("") is None


def test_parse_frame_extracts_all_fields():
    frame = parse_frame(FRAME_SEQUENCE[0])
    assert frame["jx"] == 2048
    assert frame["tof1"] == 350
    assert frame["tof2"] == -1
    assert frame["ax"] == 100
    assert frame["b1"] == 0


def test_sensors_emitted_at_full_raw_resolution_not_7bit():
    frame = parse_frame(FRAME_SEQUENCE[1])
    readings = frame_to_readings(frame, t=1.0)
    by_id = {r.source_id: r.raw for r in readings}
    # jx=4095 must stay 4095, not be squashed to a 7-bit 0..127 MIDI value.
    assert by_id["jx"] == 4095
    assert by_id["fsr1"] == 4095
    # exp raw range is 128..3992 per the firmware's calibrated ADC span —
    # a squashed MIDI value would top out at 127, this must not.
    assert by_id["exp"] == 3992
    # LiDAR: raw mm survives untouched (this is the whole point — 7-bit
    # MIDI would only give ~3.5mm/step over 2-46cm; raw mm is ~1mm/step).
    assert by_id["tof2"] == 100
    assert by_id["ax"] == -17000


def test_dropout_sentinel_passes_through_raw_unmodified():
    frame = parse_frame(FRAME_SEQUENCE[0])
    readings = frame_to_readings(frame, t=1.0)
    by_id = {r.source_id: r.raw for r in readings}
    assert by_id["tof2"] == -1
    assert SENSOR_DESCRIPTORS["tof2"].dropout_value == -1


def test_all_sensor_fields_have_descriptors():
    frame = parse_frame(FRAME_SEQUENCE[1])
    readings = frame_to_readings(frame, t=1.0)
    for r in readings:
        assert r.source_id in SENSOR_DESCRIPTORS


# ── control-event bypass path: byte-compatible with serial_bridge.py ──────

def test_encoder_delta_emits_one_cc_per_click_matching_serial_bridge_table():
    cs = ControlState()
    frame = parse_frame(FRAME_SEQUENCE[1])   # e1=2 e2=-1
    ccs = cs.frame_to_ccs(frame)
    e1_events = [c for c in ccs if c.cc == 20]   # e1 -> CC20 (serial_bridge ENCODER_CCS)
    e2_events = [c for c in ccs if c.cc == 21]   # e2 -> CC21
    assert len(e1_events) == 2 and all(c.value == 127 for c in e1_events)
    assert len(e2_events) == 1 and e2_events[0].value == 0
    assert all(c.channel == CHANNEL for c in ccs)


def test_button_edge_emits_cc_matching_serial_bridge_table():
    cs = ControlState()
    cs.frame_to_ccs(parse_frame(FRAME_SEQUENCE[0]))  # establishes b1=0, jb=0 baseline
    ccs = cs.frame_to_ccs(parse_frame(FRAME_SEQUENCE[1]))   # b1: 0->1, jb: 0->1
    b1 = [c for c in ccs if c.cc == 60]   # b1 -> CC60 (serial_bridge BUTTON_CCS)
    jb = [c for c in ccs if c.cc == 77]   # jb -> CC77 (serial_bridge JOY_BTN_CC)
    assert len(b1) == 1 and b1[0].value == 127
    assert len(jb) == 1 and jb[0].value == 127


def test_button_no_edge_no_repeat_cc():
    cs = ControlState()
    cs.frame_to_ccs(parse_frame(FRAME_SEQUENCE[1]))  # b1=1, jb=1
    ccs = cs.frame_to_ccs(parse_frame(FRAME_SEQUENCE[2]))  # b1=0, jb=0 (release edge)
    b1 = [c for c in ccs if c.cc == 60]
    assert len(b1) == 1 and b1[0].value == 0   # release -> 0, only once
    # A third identical frame (no change) must emit nothing for b1.
    ccs2 = cs.frame_to_ccs(parse_frame(FRAME_SEQUENCE[2]))
    assert not [c for c in ccs2 if c.cc == 60]


def test_full_canned_sequence_runs_without_error():
    cs = ControlState()
    total_ccs = 0
    total_readings = 0
    for line in FRAME_SEQUENCE:
        frame = parse_frame(line)
        total_readings += len(frame_to_readings(frame, t=0.0))
        total_ccs += len(cs.frame_to_ccs(frame))
    assert total_readings == 12 * len(FRAME_SEQUENCE)
    assert total_ccs > 0
