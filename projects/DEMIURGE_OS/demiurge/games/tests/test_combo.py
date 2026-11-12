"""Tests for the secret-handshake detector and frame parsing — no hardware."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from lib.combo import ComboDetector
from lib.config import Config
from lib.teensy import parse_frame


def test_fires_once_after_hold():
    c = ComboDetector(hold_seconds=1.0)
    assert c.update(False, -0.1) is False          # arm (saw a release)
    assert c.update(True, 0.0) is False            # rising edge, clock starts
    assert c.update(True, 0.5) is False            # not long enough
    assert c.update(True, 0.99) is False           # still short
    assert c.update(True, 1.0) is True             # threshold crossed -> fire
    assert c.update(True, 1.5) is False            # one-shot: no re-fire while held
    assert c.update(True, 3.0) is False


def test_release_resets():
    c = ComboDetector(hold_seconds=1.0)
    assert c.update(False, -0.1) is False          # arm
    assert c.update(True, 0.0) is False
    assert c.update(False, 0.5) is False           # released early
    assert c.update(True, 0.6) is False            # new hold starts fresh
    assert c.update(True, 1.59) is False           # 0.99s in
    assert c.update(True, 1.6) is True             # 1.0s in -> fire


def test_toggle_pattern_two_separate_holds():
    """Two distinct 1s holds = two toggles (open then close)."""
    c = ComboDetector(hold_seconds=1.0)
    c.update(False, -0.1)                           # arm
    # first hold
    c.update(True, 0.0)
    assert c.update(True, 1.0) is True
    # release
    c.update(False, 1.2)
    # second hold
    c.update(True, 2.0)
    assert c.update(True, 3.0) is True


def test_not_both_pressed_never_fires():
    c = ComboDetector(hold_seconds=1.0)
    for t in range(10):
        assert c.update(False, float(t)) is False


def test_stale_held_on_startup_does_not_fire():
    """A held b1+jb present from the very first frame (stale serial frame on
    port-open) must NOT auto-fire — the detector arms only after a release."""
    c = ComboDetector(hold_seconds=1.0)
    # held the whole time, never released first
    assert c.update(True, 0.0) is False
    assert c.update(True, 1.0) is False
    assert c.update(True, 5.0) is False
    # now a real release + fresh hold DOES fire
    assert c.update(False, 5.1) is False            # arm
    c.update(True, 5.2)
    assert c.update(True, 6.2) is True


def test_parse_frame_basic():
    f = parse_frame("NEPTR2 t=123 b1=1 jb=1 jx=2048 jy=100 e1=-2")
    assert f == {"t": 123, "b1": 1, "jb": 1, "jx": 2048, "jy": 100, "e1": -2}


def test_parse_frame_rejects_non_neptr():
    assert parse_frame("garbage line") is None
    assert parse_frame("") is None


def test_parse_frame_skips_bad_tokens():
    f = parse_frame("NEPTR2 b1=1 junk jx=oops jy=50")
    assert f == {"b1": 1, "jy": 50}


def test_config_defaults_and_coercion():
    cfg = Config.parse_text("enabled = on\ncombo_hold_ms = 1500\ndefault_game = galaga.zip\n")
    assert cfg["enabled"] is True
    assert cfg.combo_hold_seconds == 1.5
    assert cfg["default_game"] == "galaga.zip"
    # default filled in for an absent key
    assert cfg["serial_port"] == "/dev/ttyACM0"


def test_config_comments_ignored():
    cfg = Config.parse_text("# a comment\nenabled = off  # trailing\n")
    assert cfg["enabled"] is False
