from unittest.mock import MagicMock, patch
from engine.presence import Presence, PresenceState
from engine.idle import IdleLoop


def _make_loop(presence, **overrides):
    defaults = dict(
        presence=presence,
        speak_fn=MagicMock(),
        sing_fn=MagicMock(),
        verse_generator=MagicMock(return_value="silent stars hum"),
        sounds=["hmmm", "woah"],
        topics=["the silence between stars"],
        npc_cadence_seconds=180.0,
        npc_jitter_seconds=0.0,
        verse_cadence_seconds=1200.0,
        verse_jitter_seconds=0.0,
    )
    defaults.update(overrides)
    return IdleLoop(**defaults)


def test_decide_returns_nothing_when_active():
    p = Presence(active_window_seconds=900.0)
    p._state = PresenceState.ACTIVE  # force
    loop = _make_loop(p)
    sound, topic = loop._decide(now=10_000.0)
    assert sound is None and topic is None


def test_decide_fires_npc_sound_when_idle_and_due():
    p = Presence()
    loop = _make_loop(p)
    # Force the next-fire timestamps to "due"
    loop._next_npc_at = 100.0
    loop._next_verse_at = 1e12
    sound, topic = loop._decide(now=200.0)
    assert sound in ["hmmm", "woah"]
    assert topic is None
    # Timer rescheduled forward
    assert loop._next_npc_at > 200.0


def test_decide_fires_verse_when_due():
    p = Presence()
    loop = _make_loop(p)
    loop._next_npc_at = 1e12
    loop._next_verse_at = 100.0
    sound, topic = loop._decide(now=200.0)
    assert sound is None
    assert topic == "the silence between stars"
    assert loop._next_verse_at > 200.0


def test_decide_active_resets_timers_so_no_burst_after_returning_to_idle():
    p = Presence()
    p._state = PresenceState.ACTIVE
    loop = _make_loop(p)
    loop._next_npc_at = 50.0  # past
    loop._next_verse_at = 50.0  # past
    loop._decide(now=200.0)
    # Both pushed forward; nothing fires immediately on next IDLE tick
    assert loop._next_npc_at > 200.0
    assert loop._next_verse_at > 200.0


def test_apply_speaks_npc_sound():
    p = Presence()
    loop = _make_loop(p)
    loop._apply(sound="hmmm", topic=None)
    loop._speak_fn.assert_called_once_with("hmmm")
    loop._sing_fn.assert_not_called()


def test_apply_sings_verse_with_uppercased_lyrics():
    p = Presence()
    loop = _make_loop(p, verse_generator=MagicMock(return_value="silent  stars\nhum gently"))
    loop._apply(sound=None, topic="the silence between stars")
    loop._verse_generator.assert_called_once_with("the silence between stars")
    loop._sing_fn.assert_called_once()
    args = loop._sing_fn.call_args.args[0]
    assert args == ["SILENT", "STARS", "HUM", "GENTLY"]


def test_apply_swallows_verse_generator_errors():
    p = Presence()
    loop = _make_loop(p, verse_generator=MagicMock(side_effect=RuntimeError("llm busy")))
    # Should not raise
    loop._apply(sound=None, topic="the silence between stars")
    loop._sing_fn.assert_not_called()


def test_apply_strips_punctuation_before_singing():
    p = Presence()
    loop = _make_loop(p, verse_generator=MagicMock(return_value="silent, stars! hum—gently."))
    loop._apply(sound=None, topic="anything")
    args = loop._sing_fn.call_args.args[0]
    assert args == ["SILENT", "STARS", "HUM", "GENTLY"]


def test_apply_skips_when_verse_empty():
    p = Presence()
    loop = _make_loop(p, verse_generator=MagicMock(return_value=""))
    loop._apply(sound=None, topic="anything")
    loop._sing_fn.assert_not_called()
