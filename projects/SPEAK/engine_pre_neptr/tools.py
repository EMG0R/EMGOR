from __future__ import annotations
import platform
import re

from engine.voice import synthesize
from engine.audio import play
from engine.singer import sing_lyrics, render_song
from engine import memory

# Default voice: ti mode on Pi (no `say`), mix on Mac
_voice_mode: str = "ti" if platform.system() != "Darwin" else "mix"
_ti_amount: float = 0.85

# Pentatonic C major across two octaves
_PENTATONIC: list[int] = [60, 62, 64, 67, 69, 72, 74, 76, 79, 81]


def _preprocess(text: str) -> str:
    # Ensure NEPTR is always spoken as letters, however the LLM writes it
    return re.sub(r"\bN\.?E\.?P\.?T\.?R\.?\b", "N. E. P. T. R.", text, flags=re.IGNORECASE)


def speak(text: str) -> None:
    samples, sr = synthesize(_preprocess(text), mode=_voice_mode, ti_amount=_ti_amount)
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
    play(samples, sr)


def write_memory(key: str, value: str) -> None:
    memory.write_memory(key, value)


def read_memory(key: str) -> str:
    return memory.read_memory(key)


def write_song(filename: str, content: str) -> None:
    memory.write_song(filename, content)
