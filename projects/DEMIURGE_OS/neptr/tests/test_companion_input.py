"""Tests for companion_input.InputState — the MIDI-CC -> input-snapshot map.

CC numbers come from TEENSYY/TEENSYY.ino (mirrored by serial_bridge.py):
  joy btn CC77, joy X CC27, joy Y CC28, arcade b1 CC60,
  tof1 CC30, tof2 CC36, accel CC31/32/33, fsr CC34/35, mics CC78/79.
"""

from companion_input import InputState


def test_defaults_are_centred_and_unpressed():
    s = InputState()
    assert s.joy_btn is False
    assert s.arcade is False
    assert s.joy_x == 64          # centre of 0..127
    assert s.joy_y == 64


def test_joystick_button_threshold():
    s = InputState()
    s.feed(77, 127)
    assert s.joy_btn is True
    s.feed(77, 0)
    assert s.joy_btn is False


def test_arcade_button_threshold():
    s = InputState()
    s.feed(60, 127)
    assert s.arcade is True
    s.feed(60, 0)
    assert s.arcade is False


def test_joystick_axes():
    s = InputState()
    s.feed(27, 10)
    s.feed(28, 120)
    assert s.joy_x == 10
    assert s.joy_y == 120


def test_snapshot_tuple_matches_controller_signature():
    s = InputState()
    s.feed(77, 127); s.feed(27, 30); s.feed(28, 90); s.feed(60, 127)
    assert s.snapshot() == dict(joy_btn=True, joy_x=30, joy_y=90, arcade=True)


def test_unrelated_cc_is_stored_for_sensors_not_controls():
    s = InputState()
    s.feed(31, 100)               # accel X
    assert s.joy_btn is False     # control snapshot unaffected
    assert s.sensors()["accel_x"] == 100


def test_sensors_exposes_named_telemetry():
    s = InputState()
    s.feed(30, 64)   # tof1
    s.feed(34, 12)   # fsr1
    s.feed(78, 7)    # mic L
    sens = s.sensors()
    assert sens["tof1"] == 64
    assert sens["fsr1"] == 12
    assert sens["mic_l"] == 7


def test_telemetry_is_ordered_label_value_pairs():
    s = InputState()
    s.feed(27, 70)   # joystick X
    s.feed(36, 25)   # tof2
    s.feed(60, 127)  # button 1
    s.feed(20, 127)  # encoder 1
    tel = dict(s.telemetry())
    assert tel["jstX"] == 70
    assert tel["ldr2"] == 25
    assert tel["btn1"] == 127
    assert tel["enc1"] == 127
    # ordered list, every channel present (value None until first CC seen)
    pairs = s.telemetry()
    assert isinstance(pairs, list) and isinstance(pairs[0], tuple)
    labels = [p[0] for p in pairs]
    assert "jstY" in labels and "accX" in labels and "btn9" in labels
