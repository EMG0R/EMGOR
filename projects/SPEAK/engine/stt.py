from __future__ import annotations
import re
import threading
import collections
import numpy as np
import sounddevice as sd

try:
    from faster_whisper import WhisperModel
except ImportError:
    WhisperModel = None  # type: ignore[assignment,misc]

_SAMPLE_RATE = 16000
_SPEECH_ENERGY_MIN = 0.02
_SILENCE_ENERGY = 0.008          # below this = silence (low = only truly quiet)
_SILENCE_FRAMES = int(_SAMPLE_RATE * 0.7)   # 0.7s of smoothed quiet closes utterance
_RMS_SMOOTH = 8                  # smooth RMS over this many blocks to ignore brief gaps
_PREROLL_FRAMES = int(_SAMPLE_RATE * 0.2)   # 0.2s pre-roll captures speech start
_MAX_UTTERANCE_S = 30.0          # generous cap — only forces close on truly long monologues
_CONTAMINATION_RATIO = 0.4       # fraction of utterance with playback before we drop it
_NO_SPEECH_THRESHOLD = 0.5
_NO_SPEECH_SINGLE_WORD = 0.35
_GAIN = 2.5

_NEPTR_VARIANTS = re.compile(
    r"\b(nep[a-z]*r|nept[a-z]+|nephew|neptune|n[ée]ptr?|inepter?|innepter?|"
    r"ineptr?|neptor|neptur|neptir|neptyr|neper|nepper|netar|neptar)\b",
    re.IGNORECASE,
)

_model: "WhisperModel | None" = None
_model_lock = threading.Lock()


def _get_model() -> "WhisperModel":
    global _model
    with _model_lock:
        if _model is None:
            if WhisperModel is None:
                raise RuntimeError("faster-whisper is not installed")
            _model = WhisperModel("small.en", device="cpu", compute_type="int8")
        return _model


def _normalize(text: str) -> str:
    return _NEPTR_VARIANTS.sub("nepter", text)


def _preprocess(audio: np.ndarray) -> np.ndarray:
    from scipy.signal import butter, sosfilt
    # low-cut only: kill rumble/HVAC, keep full speech band intact
    sos = butter(4, 80, btype="highpass", fs=_SAMPLE_RATE, output="sos")
    filtered = sosfilt(sos, audio).astype(np.float32)
    frame = 512
    n_frames = len(filtered) // frame
    if n_frames > 0:
        frames = filtered[:n_frames * frame].reshape(n_frames, frame)
        rms_per = np.sqrt(np.mean(frames ** 2, axis=1))
        noise_floor = float(np.nanpercentile(rms_per, 10))
        gate = noise_floor * 4.0
        if gate > 0 and np.isfinite(gate):
            mask = np.repeat(rms_per >= gate, frame).astype(np.float32)
            filtered[:n_frames * frame] *= mask
    return filtered


def _transcribe(audio: np.ndarray) -> str:
    model = _get_model()
    boosted = np.clip(audio * _GAIN, -1.0, 1.0)
    clean = _preprocess(boosted)
    rms = float(np.sqrt(np.mean(clean ** 2)))
    if rms < _SPEECH_ENERGY_MIN:
        return ""
    segments, info = model.transcribe(
        clean, language="en", beam_size=1, best_of=1,
        condition_on_previous_text=False,
        vad_filter=True,
        vad_parameters={"min_silence_duration_ms": 250},
        initial_prompt="neptr, Taiwan, okay, yeah.",
    )
    if info.language_probability < 0.5:
        return ""
    seen: set[str] = set()
    words: list[str] = []
    for seg in segments:
        text = seg.text.strip()
        if text in seen:
            continue  # deduplicate repeated segments
        seen.add(text)
        seg_words = text.split()
        threshold = _NO_SPEECH_SINGLE_WORD if len(seg_words) == 1 else _NO_SPEECH_THRESHOLD
        if seg.no_speech_prob > threshold:
            continue
        words.extend(seg_words)
    if not words:
        return ""
    return _normalize(" ".join(words))


# ── Streaming listener ────────────────────────────────────────────────────────

class StreamingListener:
    """Callback-based InputStream so it coexists with sd.play() on the same device."""

    def __init__(
        self,
        on_text: "callable[[str], None]",
        on_speech_start: "callable[[], None] | None" = None,
    ) -> None:
        self._on_text = on_text
        self._on_speech_start = on_speech_start
        self._stop_event = threading.Event()
        self._thread: threading.Thread | None = None
        self._buf: collections.deque[np.ndarray] = collections.deque()
        self._buf_lock = threading.Lock()

    def start(self) -> None:
        _get_model()
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def stop(self) -> None:
        self._stop_event.set()
        if self._thread:
            self._thread.join(timeout=4.0)

    def _run(self) -> None:
        from engine.audio import is_playing
        import collections as col

        preroll: col.deque[np.ndarray] = col.deque()
        preroll_total = 0
        utterance: list[np.ndarray] = []
        utterance_total = 0
        silence_total = 0
        playback_total = 0
        in_speech = False
        max_frames = int(_SAMPLE_RATE * _MAX_UTTERANCE_S)
        rms_history: col.deque[float] = col.deque(maxlen=_RMS_SMOOTH)

        q: col.deque[np.ndarray] = col.deque()
        q_lock = threading.Lock()

        def callback(indata: np.ndarray, frames: int, time, status) -> None:
            with q_lock:
                q.append(indata.copy().flatten())

        with sd.InputStream(
            samplerate=_SAMPLE_RATE,
            channels=1,
            dtype="float32",
            blocksize=256,
            callback=callback,
        ):
            while not self._stop_event.is_set():
                sd.sleep(10)
                with q_lock:
                    chunks = list(q)
                    q.clear()

                try:
                    for block in chunks:
                        playing = is_playing()
                        raw_rms = float(np.sqrt(np.mean(block ** 2)))
                        rms_history.append(raw_rms)
                        smoothed_rms = float(np.mean(rms_history))

                        if not in_speech:
                            preroll.append(block)
                            preroll_total += len(block)
                            while preroll_total > _PREROLL_FRAMES:
                                old = preroll.popleft()
                                preroll_total -= len(old)

                            # Allow speech start even during TTS playback so
                            # the user can wake/interrupt mid-utterance. The
                            # `during_playback` flag downstream filters echo.
                            if smoothed_rms >= _SPEECH_ENERGY_MIN:
                                in_speech = True
                                playback_total = len(block) if playing else 0
                                silence_total = 0
                                utterance = list(preroll) + [block]
                                utterance_total = preroll_total + len(block)
                        else:
                            if playing:
                                playback_total += len(block)
                            utterance.append(block)
                            utterance_total += len(block)

                            # use smoothed RMS so brief inter-word gaps don't close early
                            if smoothed_rms < _SILENCE_ENERGY:
                                silence_total += len(block)
                            else:
                                silence_total = 0

                            closed = (silence_total >= _SILENCE_FRAMES or
                                      utterance_total >= max_frames)
                            if closed:
                                in_speech = False
                                ratio = playback_total / max(1, utterance_total)
                                during_playback = ratio >= _CONTAMINATION_RATIO
                                audio = np.concatenate(utterance)
                                threading.Thread(
                                    target=self._transcribe_and_fire,
                                    args=(audio, during_playback),
                                    daemon=True,
                                ).start()
                                utterance = []
                                utterance_total = 0
                                silence_total = 0
                                playback_total = 0
                                rms_history.clear()
                except Exception as exc:
                    print(f"[stt] listener error: {exc!r}")
                    in_speech = False
                    utterance = []
                    utterance_total = 0
                    silence_total = 0
                    playback_total = 0
                    rms_history.clear()

    def _transcribe_and_fire(self, audio: np.ndarray,
                             during_playback: bool = False) -> None:
        try:
            text = _transcribe(audio)
        except Exception as exc:
            print(f"[stt] transcribe error: {exc!r}")
            return
        if not text:
            return
        try:
            # Pass `during_playback` if the callback accepts it; otherwise
            # fall back to text-only for backwards compat.
            try:
                self._on_text(text, during_playback)
            except TypeError:
                self._on_text(text)
        except Exception as exc:
            print(f"[stt] on_text error: {exc!r}")


# ── Single-shot for test scripts ──────────────────────────────────────────────

def listen_and_transcribe() -> str:
    """Single-shot VAD-based record then transcribe."""
    import collections as col
    preroll: col.deque = col.deque()
    preroll_total = 0
    utterance: list[np.ndarray] = []
    utterance_total = 0
    silence_total = 0
    in_speech = False
    done = threading.Event()
    q: col.deque = col.deque()
    q_lock = threading.Lock()
    max_frames = int(_SAMPLE_RATE * _MAX_UTTERANCE_S)

    def callback(indata, frames, t, status):
        with q_lock:
            q.append(indata.copy().flatten())

    with sd.InputStream(samplerate=_SAMPLE_RATE, channels=1,
                        dtype="float32", blocksize=512, callback=callback):
        while not done.is_set():
            sd.sleep(30)
            with q_lock:
                blocks = list(q); q.clear()
            for block in blocks:
                rms = float(np.sqrt(np.mean(block ** 2)))
                if not in_speech:
                    preroll.append(block); preroll_total += len(block)
                    while preroll_total > _PREROLL_FRAMES:
                        old = preroll.popleft(); preroll_total -= len(old)
                    if rms >= _SPEECH_ENERGY_MIN:
                        in_speech = True
                        utterance = list(preroll) + [block]
                        utterance_total = preroll_total + len(block)
                else:
                    utterance.append(block); utterance_total += len(block)
                    silence_total = (silence_total + len(block)) if rms < _SILENCE_ENERGY else 0
                    if silence_total >= _SILENCE_FRAMES or utterance_total >= max_frames:
                        done.set()

    if not utterance:
        return ""
    return _transcribe(np.concatenate(utterance))
