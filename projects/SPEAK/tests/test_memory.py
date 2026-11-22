import json
import pytest
from pathlib import Path


@pytest.fixture(autouse=True)
def mem_dir(tmp_path, monkeypatch):
    """Redirect MEMORY_DIR to a temp directory for every test."""
    import engine.memory as mem
    monkeypatch.setattr(mem, "MEMORY_DIR", tmp_path)
    return tmp_path


def test_write_and_read_memory(mem_dir):
    from engine.memory import write_memory, read_memory
    write_memory("user_name", "Emgor")
    assert read_memory("user_name") == "Emgor"


def test_read_missing_key_returns_empty(mem_dir):
    from engine.memory import read_memory
    assert read_memory("nonexistent") == ""


def test_save_and_load_voice_config(mem_dir):
    from engine.memory import save_voice_config, load_voice_config
    save_voice_config("ti", 0.9)
    cfg = load_voice_config()
    assert cfg == {"mode": "ti", "ti_amount": 0.9}


def test_load_voice_config_returns_none_when_missing(mem_dir):
    from engine.memory import load_voice_config
    assert load_voice_config() is None


def test_load_all_merges_facts_and_voice_config(mem_dir):
    from engine.memory import write_memory, save_voice_config, load_all
    write_memory("color", "green")
    save_voice_config("mix", 0.85)
    ctx = load_all()
    assert "voice_config" in ctx
    assert ctx["voice_config"]["mode"] == "mix"
    assert "facts" in ctx
    assert "color" in ctx["facts"]


def test_write_song_creates_file(mem_dir):
    from engine.memory import write_song
    write_song("test.song", "midi: none\nlyrics:\n  HI THERE")
    p = mem_dir / "songs" / "test.song"
    assert p.exists()
    assert "HI THERE" in p.read_text()


def test_read_memory_no_key_collision(mem_dir):
    from engine.memory import write_memory, read_memory
    write_memory("user_name", "Emgor")
    assert read_memory("user") == ""  # short key must NOT match user_name
