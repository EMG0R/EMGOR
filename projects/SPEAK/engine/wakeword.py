from __future__ import annotations
import threading

import numpy as np
import sounddevice as sd

try:
    from faster_whisper import WhisperModel
except ImportError:
    WhisperModel = None  # type: ignore[assignment,misc]

_SAMPLE_RATE = 16000
_CHUNK_SECONDS = 2.0  # length of each listen window
_CHUNK_FRAMES = int(_SAMPLE_RATE * _CHUNK_SECONDS)

_DEFAULT_WAKE_WORDS = ["neptr", "computer", "computah", "compua"]

_whisper: "WhisperModel | None" = None
_whisper_lock = threading.Lock()


def _get_whisper() -> "WhisperModel":
    global _whisper
    with _whisper_lock:
        if _whisper is None:
            if WhisperModel is None:
                raise RuntimeError("faster-whisper is not installed")
            _whisper = WhisperModel("base", device="cpu", compute_type="int8")
        return _whisper


def _transcribe_chunk(audio: np.ndarray) -> str:
    model = _get_whisper()
    segments, _ = model.transcribe(audio, language="en", beam_size=1, best_of=1)
    return " ".join(s.text.strip() for s in segments).lower()


class WakeWordListener:
    def __init__(
        self,
        wake_event: threading.Event,
        wake_words: list[str] | None = None,
    ) -> None:
        self._wake_words = [w.lower() for w in (wake_words or _DEFAULT_WAKE_WORDS)]
        self._event = wake_event
        self._stop_event = threading.Event()
        self._thread: threading.Thread | None = None
        print(f"[wakeword] listening for: {self._wake_words}")

    def start(self) -> None:
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def stop(self) -> None:
        self._stop_event.set()
        if self._thread is not None:
            self._thread.join(timeout=4.0)
            self._thread = None

    def _run(self) -> None:
        while not self._stop_event.is_set():
            try:
                audio = sd.rec(
                    _CHUNK_FRAMES,
                    samplerate=_SAMPLE_RATE,
                    channels=1,
                    dtype="float32",
                )
                sd.wait()
                if self._stop_event.is_set():
                    break
                text = _transcribe_chunk(audio.flatten())
                if any(w in text for w in self._wake_words):
                    self._event.set()
            except Exception as exc:
                if not self._stop_event.is_set():
                    print(f"[wakeword] error: {exc}")
