"""
.song file parser + renderer.

Format:
    midi:      <relative path to .mid>
    transpose: <int semitones>
    intro:     <optional spoken intro>
    lyrics:
      SYLL SYLL SYLL ...

Lines starting with '#' are comments.
"""
from __future__ import annotations
import os
from dataclasses import dataclass, field


@dataclass
class Song:
    midi: str
    transpose: int = -12
    intro: str = ""
    syllables: list[str] = field(default_factory=list)
    source_path: str = ""


def load_song(path: str) -> Song:
    with open(path, "r") as f:
        raw = f.read()

    midi = ""
    transpose = -12
    intro = ""
    syllables: list[str] = []

    in_lyrics = False
    for line in raw.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        low = stripped.lower()
        if low.startswith("midi:"):
            midi = stripped.split(":", 1)[1].strip()
            in_lyrics = False
        elif low.startswith("transpose:"):
            transpose = int(stripped.split(":", 1)[1].strip())
            in_lyrics = False
        elif low.startswith("intro:"):
            intro = stripped.split(":", 1)[1].strip()
            in_lyrics = False
        elif low.startswith("lyrics:"):
            in_lyrics = True
            rest = stripped.split(":", 1)[1].strip()
            if rest:
                syllables.extend(rest.split())
        elif in_lyrics:
            syllables.extend(stripped.split())

    base = os.path.dirname(os.path.abspath(path))
    midi_abs = midi if os.path.isabs(midi) else os.path.join(base, midi)
    return Song(midi=midi_abs, transpose=transpose, intro=intro,
                syllables=syllables, source_path=os.path.abspath(path))


def render_song_file(path: str):
    """Load + render a .song file. Returns (samples, sr)."""
    from engine.singer import render_song, render_spoken_then_song
    song = load_song(path)
    if song.intro:
        return render_spoken_then_song(
            song.intro, song.midi, song.syllables, transpose=song.transpose,
        )
    return render_song(song.midi, song.syllables, transpose=song.transpose)
