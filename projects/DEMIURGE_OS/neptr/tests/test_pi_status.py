"""Tests for pi_status — the dashboard's system-metrics helpers.

Only the pure parsers are unit-tested. The live gather() shells out to
vcgencmd / sysfs and degrades gracefully off-Pi; that path is exercised on the
device, not here.

vcgencmd get_throttled bit meanings:
  bit 0  under-voltage now        bit 16 under-voltage occurred
  bit 1  arm freq capped now      bit 17 arm freq cap occurred
  bit 2  currently throttled      bit 18 throttling occurred
  bit 3  soft temp limit now      bit 19 soft temp limit occurred
"""

import pi_status


def test_parse_throttled_all_clear():
    f = pi_status.parse_throttled(0x0)
    assert f["under_voltage_now"] is False
    assert f["throttled_now"] is False
    assert f["under_voltage_occurred"] is False
    assert f["throttled_occurred"] is False
    assert f["ok"] is True


def test_parse_throttled_past_undervoltage_and_throttle():
    # 0x50000 = bits 16 and 18 => under-voltage + throttling have OCCURRED,
    # but nothing is happening right now.
    f = pi_status.parse_throttled(0x50000)
    assert f["under_voltage_now"] is False
    assert f["throttled_now"] is False
    assert f["under_voltage_occurred"] is True
    assert f["throttled_occurred"] is True
    assert f["ok"] is False


def test_parse_throttled_currently_undervolted():
    # bit 0 set => under-voltage happening now
    f = pi_status.parse_throttled(0x1)
    assert f["under_voltage_now"] is True
    assert f["ok"] is False


def test_parse_throttled_accepts_hex_string():
    # Real vcgencmd output is "throttled=0x50000"; the value passed in may be
    # the raw hex string.
    f = pi_status.parse_throttled("0x50000")
    assert f["throttled_occurred"] is True


def test_summarize_throttled_ok():
    assert pi_status.summarize_throttled(0x0) == "ok"


def test_summarize_throttled_lists_active_then_past_flags():
    # Currently under-voltage AND past throttling -> active flag shown, concise.
    s = pi_status.summarize_throttled(0x1)
    assert "under-volt" in s.lower()
    assert s != "ok"


def test_power_label_matches_cli_tiers():
    # Mirrors the demiurge CLI plabel(): <=1600 LOW, <=2400 MED, else HIGH.
    assert pi_status.label_for_maxfreq(1500) == "LOW"
    assert pi_status.label_for_maxfreq(1600) == "LOW"
    assert pi_status.label_for_maxfreq(1800) == "MED"
    assert pi_status.label_for_maxfreq(2400) == "MED"
    assert pi_status.label_for_maxfreq(2800) == "HIGH"


def test_power_label_none_when_unknown():
    assert pi_status.label_for_maxfreq(None) is None
    assert pi_status.label_for_maxfreq(0) is None
