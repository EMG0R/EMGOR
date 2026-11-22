from unittest.mock import MagicMock, patch
import numpy as np
import sys


def _reset_stt_module():
    """Reset engine.stt module to clear cached _model."""
    if "engine.stt" in sys.modules:
        del sys.modules["engine.stt"]


def test_transcribe_audio_calls_whisper():
    fake_segment = MagicMock()
    fake_segment.text = " hello world"
    mock_model = MagicMock()
    mock_model.transcribe.return_value = ([fake_segment], MagicMock())

    # Patch faster_whisper before importing stt
    with patch.dict(sys.modules, {"faster_whisper": MagicMock(WhisperModel=MagicMock(return_value=mock_model))}):
        _reset_stt_module()
        from engine.stt import transcribe_audio
        audio = np.zeros(16000, dtype=np.float32)
        result = transcribe_audio(audio)

    assert result == "hello world"
    mock_model.transcribe.assert_called_once()


def test_transcribe_audio_joins_multiple_segments():
    seg1 = MagicMock()
    seg1.text = " one two"
    seg2 = MagicMock()
    seg2.text = " three"
    mock_model = MagicMock()
    mock_model.transcribe.return_value = ([seg1, seg2], MagicMock())

    with patch.dict(sys.modules, {"faster_whisper": MagicMock(WhisperModel=MagicMock(return_value=mock_model))}):
        _reset_stt_module()
        from engine.stt import transcribe_audio
        result = transcribe_audio(np.zeros(16000, dtype=np.float32))

    assert result == "one two three"


def test_transcribe_audio_returns_empty_on_no_segments():
    mock_model = MagicMock()
    mock_model.transcribe.return_value = ([], MagicMock())

    with patch.dict(sys.modules, {"faster_whisper": MagicMock(WhisperModel=MagicMock(return_value=mock_model))}):
        _reset_stt_module()
        from engine.stt import transcribe_audio
        result = transcribe_audio(np.zeros(16000, dtype=np.float32))

    assert result == ""
