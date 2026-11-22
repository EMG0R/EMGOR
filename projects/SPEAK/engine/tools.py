from __future__ import annotations
import platform
import re
import threading

from engine.voice import synthesize
from engine.audio import play
from engine.singer import sing_lyrics, render_song
from engine import memory

# Default voice: ti mode on Pi (no `say`), mix on Mac
_voice_mode: str = "ti" if platform.system() != "Darwin" else "mix"
_ti_amount: float = 0.85

# Pentatonic C major across two octaves
_PENTATONIC: list[int] = [60, 62, 64, 67, 69, 72, 74, 76, 79, 81]

_audio_lock = threading.Lock()


_EMOJI = re.compile("["
    u"\U0001F600-\U0001F64F"
    u"\U0001F300-\U0001F5FF"
    u"\U0001F680-\U0001F6FF"
    u"\U0001F1E0-\U0001F1FF"
    u"\U00002700-\U000027BF"
    u"\U0001F900-\U0001F9FF"
    u"\U00002600-\U000026FF"
    "]+", flags=re.UNICODE)


def _preprocess(text: str) -> str:
    text = _EMOJI.sub("", text)
    # strip markdown
    text = re.sub(r"\*\*(.+?)\*\*", r"\1", text)   # **bold**
    text = re.sub(r"\*(.+?)\*", r"\1", text)         # *italic*
    text = re.sub(r"^#+\s*", "", text, flags=re.MULTILINE)  # # headers
    text = re.sub(r"^-\s+", "", text, flags=re.MULTILINE)   # - bullets
    text = re.sub(r"\[.+?\]\(.+?\)", "", text)       # [links](url)
    text = re.sub(r"\bbro\b", "dude", text, flags=re.IGNORECASE)
    text = re.sub(r"\bneptr\b", "nepter", text, flags=re.IGNORECASE)
    return text.strip()


def speak(text: str) -> None:
    samples, sr = synthesize(_preprocess(text), mode=_voice_mode, ti_amount=_ti_amount)
    with _audio_lock:
        play(samples, sr)


def set_voice(mode: str | None, ti_amount: float | None) -> None:
    global _voice_mode, _ti_amount
    if mode is not None:
        _voice_mode = mode
    if ti_amount is not None:
        _ti_amount = ti_amount
    memory.save_voice_config(_voice_mode, _ti_amount)


def sing(lyrics: list[str], midi: str | None = None) -> None:
    if midi:
        samples, sr = render_song(midi, lyrics)
    else:
        dur_ms = 400.0
        score = [
            (syl, _PENTATONIC[i % len(_PENTATONIC)], dur_ms)
            for i, syl in enumerate(lyrics)
        ]
        samples, sr = sing_lyrics(score)
    with _audio_lock:
        play(samples, sr)


def write_memory(key: str, value: str) -> None:
    memory.write_memory(key, value)


def read_memory(key: str) -> str:
    return memory.read_memory(key)


def delete_memory(key: str) -> None:
    memory.delete_memory(key)


def list_memory() -> list[str]:
    return memory.list_memory()


def write_lore(entry: str) -> None:
    memory.write_lore(entry)


def write_song(filename: str, content: str) -> None:
    memory.write_song(filename, content)
