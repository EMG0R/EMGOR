from __future__ import annotations
import threading

import sounddevice as sd

try:
    from openwakeword.model import Model
except ImportError:
    Model = None  # type: ignore[assignment,misc]

_CHUNK = 1280        # 80 ms at 16 kHz — openWakeWord minimum
_SAMPLE_RATE = 16000
_THRESHOLD = 0.5
_WAKE_WORD = "hey jarvis"


class WakeWordListener:
    def __init__(
        self,
        wake_event: threading.Event,
        wake_word: str = _WAKE_WORD,
    ) -> None:
        self._event = wake_event
        self._wake_word = wake_word
        self._running = False
        self._thread: threading.Thread | None = None
        if Model is None:
            raise RuntimeError(
                "openwakeword is not installed. Run: pip install openwakeword"
            )
        self._model = Model(wakeword_models=[wake_word], inference_framework="onnx")

    def start(self) -> None:
        self._running = True
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def stop(self) -> None:
        self._running = False

    def _run(self) -> None:
        with sd.InputStream(
            samplerate=_SAMPLE_RATE,
            channels=1,
            dtype="int16",
            blocksize=_CHUNK,
        ) as stream:
            while self._running:
                audio, _ = stream.read(_CHUNK)
                prediction = self._model.predict(audio.flatten())
                score = prediction.get(self._wake_word, 0.0)
                if score >= _THRESHOLD:
                    self._event.set()
