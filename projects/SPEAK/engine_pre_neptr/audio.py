import numpy as np
import sounddevice as sd

SAMPLE_RATE = 8000


def play(samples: np.ndarray, sr: int = SAMPLE_RATE) -> None:
    """Play float32 audio samples. Blocks until done."""
    if samples is None or len(samples) == 0:
        return
    sd.play(samples, samplerate=sr)
    sd.wait()
