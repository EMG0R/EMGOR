import platform
from unittest.mock import patch
import numpy as np
import pytest


@pytest.fixture(autouse=True)
def reset_tools():
    """Reset tools module state between tests."""
    import engine.tools as t
    t._voice_mode = "ti" if platform.system() != "Darwin" else "mix"
    t._ti_amount = 0.85
    yield


def test_speak_calls_synthesize_and_play():
    import engine.tools as t
    from engine.tools import speak
    fake_audio = np.zeros(100, dtype=np.float32)
    with patch("engine.tools.synthesize", return_value=(fake_audio, 22050)) as ms, \
         patch("engine.tools.play") as mp:
        speak("hello world")
    ms.assert_called_once_with("hello world", mode=t._voice_mode, ti_amount=t._ti_amount)
    mp.assert_called_once_with(fake_audio, 22050)


def test_set_voice_updates_state_and_saves(tmp_path, monkeypatch):
    import engine.memory as mem
    monkeypatch.setattr(mem, "MEMORY_DIR", tmp_path)
    from engine.tools import set_voice
    import engine.tools as t
    set_voice("ti", 0.95)
    assert t._voice_mode == "ti"
    assert t._ti_amount == 0.95
    cfg = mem.load_voice_config()
    assert cfg == {"mode": "ti", "ti_amount": 0.95}


def test_write_and_read_memory_roundtrip(tmp_path, monkeypatch):
    import engine.memory as mem
    monkeypatch.setattr(mem, "MEMORY_DIR", tmp_path)
    from engine.tools import write_memory, read_memory
    write_memory("mood", "chaotic")
    assert read_memory("mood") == "chaotic"


def test_preprocess_replaces_bro_with_dude():
    from engine.tools import _preprocess
    assert _preprocess("yo bro what's up") == "yo dude what's up"
    assert _preprocess("BRO chill") == "dude chill"
    assert _preprocess("bro!") == "dude!"


def test_preprocess_passes_bruh_through():
    from engine.tools import _preprocess
    assert _preprocess("bruh.") == "bruh."
    assert _preprocess("bruhhh") == "bruhhh"


def test_preprocess_does_not_letter_spell_neptr():
    from engine.tools import _preprocess
    # neptr is now a word, not letter-spelled
    assert _preprocess("hi neptr") == "hi neptr"
    assert _preprocess("NEPTR here") == "NEPTR here"


def test_sing_without_midi_calls_sing_lyrics():
    from engine.tools import sing
    fake_audio = np.zeros(200, dtype=np.float32)
    with patch("engine.tools.sing_lyrics", return_value=(fake_audio, 22050)) as ms, \
         patch("engine.tools.play") as mp:
        sing(["HEY", "THERE", "WORLD"], midi=None)
    ms.assert_called_once()
    score = ms.call_args[0][0]  # first positional arg
    assert len(score) == 3  # 3 syllables
    assert all(len(note) == 3 for note in score)  # each is (syllable, midi, dur_ms)
    assert score[0][0] == "HEY"
    mp.assert_called_once_with(fake_audio, 22050)


def test_sing_with_midi_calls_render_song():
    from engine.tools import sing
    fake_audio = np.zeros(200, dtype=np.float32)
    with patch("engine.tools.render_song", return_value=(fake_audio, 22050)) as ms, \
         patch("engine.tools.play") as mp:
        sing(["HEY", "THERE"], midi="songs/daisy.mid")
    ms.assert_called_once_with("songs/daisy.mid", ["HEY", "THERE"])
    mp.assert_called_once_with(fake_audio, 22050)


def test_write_song_saves_file(tmp_path, monkeypatch):
    import engine.memory as mem
    monkeypatch.setattr(mem, "MEMORY_DIR", tmp_path)
    from engine.tools import write_song
    write_song("test.song", "lyrics:\n  HI")
    assert (tmp_path / "songs" / "test.song").read_text() == "lyrics:\n  HI"
