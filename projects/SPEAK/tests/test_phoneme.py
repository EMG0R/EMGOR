from engine.phoneme import to_arpabet


def test_known_word_returns_list():
    result = to_arpabet('hello')
    assert isinstance(result, list)
    assert len(result) > 0


def test_known_word_tokens_are_strings():
    result = to_arpabet('hello')
    for token in result:
        assert isinstance(token, str)


def test_cat_contains_ae():
    result = to_arpabet('cat')
    bases = [t.rstrip('012') for t in result]
    assert 'AE' in bases


def test_unknown_word_still_returns_list():
    result = to_arpabet('flurblezorp')
    assert isinstance(result, list)


def test_empty_returns_empty():
    assert to_arpabet('') == []


def test_uppercase_handled():
    lower = to_arpabet('hello')
    upper = to_arpabet('HELLO')
    assert lower == upper


def test_router_letter_fallback_when_phonemes_empty():
    from engine.router import route_word
    from unittest.mock import patch
    with patch('engine.allophone.synthesize', return_value=b''):
        result = route_word('hi')
        assert isinstance(result, bytes)
