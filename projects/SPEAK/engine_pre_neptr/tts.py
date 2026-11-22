"""
Modern high-quality TTS backends.

Returns (samples: float32 mono, sample_rate: int).

Backends:
  - `say`      : Apple neural TTS (macOS built-in, highest quality offline)
  - `espeak`   : espeak-ng formant synth (robotic, Speak&Spell-adjacent)
"""
from __future__ import annotations
import subprocess
import tempfile
import shutil
import os
import numpy as np
import soundfile as sf

SAY_VOICE_DEFAULT = "Fred"         # retro male, pairs well with TI crunch
SAY_RATE          = 160            # words per minute
ESPEAK_VOICE      = "en-us"
ESPEAK_RATE       = 160


def _read_wav(path: str) -> tuple[np.ndarray, int]:
    data, sr = sf.read(path, dtype="float32", always_2d=False)
    if data.ndim > 1:
        data = data.mean(axis=1).astype(np.float32)
    return data, sr


def say_synth(text: str, voice: str = SAY_VOICE_DEFAULT,
              rate: int = SAY_RATE) -> tuple[np.ndarray, int]:
    """Apple `say` → float32 mono samples + sample rate."""
    if not text.strip() or shutil.which("say") is None:
        return np.zeros(0, dtype=np.float32), 22050
    with tempfile.TemporaryDirectory() as d:
        wav = os.path.join(d, "out.wav")
        subprocess.run(
            ["say", "-v", voice, "-r", str(rate), "-o", wav,
             "--data-format=LEI16@22050", "--file-format=WAVE", text],
            check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        return _read_wav(wav)


def espeak_synth(text: str, voice: str = ESPEAK_VOICE,
                 rate: int = ESPEAK_RATE) -> tuple[np.ndarray, int]:
    """espeak-ng → float32 mono samples + sample rate."""
    bin_name = "espeak-ng" if shutil.which("espeak-ng") else "espeak"
    if not text.strip() or shutil.which(bin_name) is None:
        return np.zeros(0, dtype=np.float32), 22050
    with tempfile.TemporaryDirectory() as d:
        wav = os.path.join(d, "out.wav")
        subprocess.run(
            [bin_name, "-v", voice, "-s", str(rate), "-w", wav, text],
            check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        return _read_wav(wav)
