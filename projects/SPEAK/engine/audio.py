import numpy as np
import sounddevice as sd

SAMPLE_RATE = 8000

_playing = False


def is_playing() -> bool:
    return _playing


def play(samples: np.ndarray, sr: int = SAMPLE_RATE) -> None:
    global _playing
    if samples is None or len(samples) == 0:
        return
    _playing = True
    try:
        sd.play(samples, samplerate=sr)
        sd.wait()
    except Exception as exc:
        print(f"[audio] play error: {exc!r}")
    finally:
        _playing = False


def stop() -> None:
    """Interrupt currently-playing audio. Safe to call from any thread."""
    global _playing
    try:
        sd.stop()
    except Exception:
        pass
    _playing = False
