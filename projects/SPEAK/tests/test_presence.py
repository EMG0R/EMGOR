from unittest.mock import patch
from engine.presence import Presence, PresenceState


def test_starts_idle():
    p = Presence(active_window_seconds=900.0)
    assert p.state == PresenceState.IDLE


def test_wake_transitions_to_active():
    p = Presence(active_window_seconds=900.0)
    with patch("engine.presence.time.monotonic", return_value=100.0):
        p.wake()
    assert p.state == PresenceState.ACTIVE


def test_tick_drops_to_idle_after_window():
    p = Presence(active_window_seconds=10.0)
    with patch("engine.presence.time.monotonic", return_value=100.0):
        p.wake()
    with patch("engine.presence.time.monotonic", return_value=111.0):
        p.tick()
    assert p.state == PresenceState.IDLE


def test_tick_keeps_active_within_window():
    p = Presence(active_window_seconds=10.0)
    with patch("engine.presence.time.monotonic", return_value=100.0):
        p.wake()
    with patch("engine.presence.time.monotonic", return_value=105.0):
        p.tick()
    assert p.state == PresenceState.ACTIVE


def test_mark_utterance_resets_timer():
    p = Presence(active_window_seconds=10.0)
    with patch("engine.presence.time.monotonic", return_value=100.0):
        p.wake()
    with patch("engine.presence.time.monotonic", return_value=108.0):
        p.mark_utterance()
    # Now at t=115 (15s after wake but only 7s after utterance), still ACTIVE
    with patch("engine.presence.time.monotonic", return_value=115.0):
        p.tick()
    assert p.state == PresenceState.ACTIVE


def test_mark_utterance_does_nothing_when_idle():
    p = Presence(active_window_seconds=10.0)
    with patch("engine.presence.time.monotonic", return_value=100.0):
        p.mark_utterance()
    assert p.state == PresenceState.IDLE
