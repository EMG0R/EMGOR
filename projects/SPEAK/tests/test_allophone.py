from engine.allophone import synthesize
import numpy as np


def test_synthesize_returns_bytes():
    # Returns bytes regardless of whether allophone data is populated
    result = synthesize('hello')
    assert isinstance(result, bytes)


def test_synthesize_unknown_word_returns_bytes():
    result = synthesize('xylophone')
    assert isinstance(result, bytes)


def test_synthesize_empty_string_returns_empty():
    result = synthesize('')
    assert result == b''


def test_synthesize_produces_decodable_lpc():
    from engine.lpc import decode
    data = synthesize('hello')
    samples = decode(data)
    assert isinstance(samples, np.ndarray)
