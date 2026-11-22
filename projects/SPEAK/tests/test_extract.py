import importlib, sys, os

def test_words_dict_exists_and_is_nonempty():
    sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))
    from data import words
    assert isinstance(words.WORDS, dict)
    assert len(words.WORDS) > 10


def test_words_keys_are_uppercase_strings():
    from data import words
    for key in words.WORDS:
        assert key == key.upper(), f"Key not uppercase: {key}"


def test_words_values_are_bytes():
    from data import words
    for key, val in words.WORDS.items():
        assert isinstance(val, bytes), f"Value for {key} is not bytes"


def test_fourty_is_present():
    from data import words
    assert 'FOURTY' in words.WORDS  # ROM spells it FOURTY


def test_affirmative_is_present():
    from data import words
    assert 'AFFIRMATIVE' in words.WORDS
