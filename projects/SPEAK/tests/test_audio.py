import numpy as np
from unittest.mock import patch
from engine.audio import play


def test_play_calls_sounddevice_with_correct_args():
    samples = np.zeros(400, dtype=np.float32)
    with patch('engine.audio.sd.play') as mock_play, \
         patch('engine.audio.sd.wait') as mock_wait:
        play(samples)
        mock_play.assert_called_once_with(samples, samplerate=8000)
        mock_wait.assert_called_once()


def test_play_empty_array_does_nothing():
    samples = np.array([], dtype=np.float32)
    with patch('engine.audio.sd.play') as mock_play:
        play(samples)
        mock_play.assert_not_called()
