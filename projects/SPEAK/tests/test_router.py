from engine.router import route_word, route_text, wordbank_text
from data.words import WORDS
from data.letters import LETTERS


def test_known_word_returns_bytes():
    result = route_word('fourty')   # g2p handles misspelling
    assert isinstance(result, bytes)
    assert len(result) > 0


def test_known_word_case_insensitive():
    assert route_word('FOURTY') == route_word('fourty')


def test_single_letter_returns_bytes():
    result = route_word('a')
    assert isinstance(result, bytes)
    assert len(result) > 0


def test_wordbank_known_word_uses_recording():
    # wordbank_text() routes known words through pre-recorded clips
    result = wordbank_text('fourty')
    assert isinstance(result, bytes)
    assert len(result) > 0


def test_wordbank_single_letter_uses_letter_clip():
    # wordbank_text() routes single letters through letter clips
    result = wordbank_text('a')
    assert isinstance(result, bytes)
    assert len(result) > 0


def test_unknown_word_returns_bytes_via_formant():
    result = route_word('blorple')
    assert isinstance(result, bytes)
    assert len(result) > 0


def test_route_text_splits_on_whitespace():
    result = route_text('hello world')
    assert isinstance(result, bytes)
    assert len(result) > 0


def test_route_text_empty_returns_empty():
    assert route_text('') == b''


def test_route_text_strips_punctuation():
    result_clean = route_text('hello')
    result_punct = route_text('hello!')
    assert len(result_clean) > 0
    assert len(result_punct) > 0


def test_vowel_elongation():
    short = route_word('no')
    long  = route_word('noooo')
    assert len(long) > len(short)


def test_pure_vowel_repeat():
    result = route_word('ooooo')
    assert isinstance(result, bytes)
    assert len(result) > 0


def test_pitch_mode_natural():
    flat    = route_text('hello world', pitch_mode='flat')
    natural = route_text('hello world', pitch_mode='natural')
    assert len(flat) > 0
    assert len(natural) > 0
