from __future__ import annotations
import numpy as np
import sounddevice as sd

try:
    from faster_whisper import WhisperModel
except ImportError:
    WhisperModel = None  # type: ignore[assignment,misc]

_SAMPLE_RATE = 16000
_SILENCE_THRESHOLD = 0.01
_SILENCE_DURATION_S = 1.2

_model: "WhisperModel | None" = None


def _get_model():
    global _model
    if _model is None:
        if WhisperModel is None:
            raise RuntimeError(
                "faster-whisper is not installed. Run: pip install faster-whisper"
            )
        _model = WhisperModel("base", device="cpu", compute_type="int8")
    return _model


def transcribe_audio(audio: np.ndarray) -> str:
    model = _get_model()
    segments, _ = model.transcribe(audio, language="en")
    return " ".join(s.text.strip() for s in segments)


def record_until_silence(
    sample_rate: int = _SAMPLE_RATE,
    silence_duration: float = _SILENCE_DURATION_S,
) -> np.ndarray:
    chunks: list[np.ndarray] = []
    silence_frames = int(silence_duration * sample_rate)
    consecutive_silence = 0

    def callback(indata: np.ndarray, frames: int, time, status) -> None:
        nonlocal consecutive_silence
        chunks.append(indata.copy())
        rms = float(np.sqrt(np.mean(indata ** 2)))
        if rms < _SILENCE_THRESHOLD:
            consecutive_silence += frames
        else:
            consecutive_silence = 0

    with sd.InputStream(
        samplerate=sample_rate, channels=1, dtype="float32", callback=callback
    ):
        while consecutive_silence < silence_frames:
            sd.sleep(100)

    return np.concatenate(chunks, axis=0).flatten()


def listen_and_transcribe() -> str:
    audio = record_until_silence()
    return transcribe_audio(audio)
