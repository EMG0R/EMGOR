from data import letters


def test_letters_dict_exists():
    assert isinstance(letters.LETTERS, dict)


def test_letter_a_present():
    assert 'A' in letters.LETTERS


def test_letter_b_present():
    assert 'B' in letters.LETTERS


def test_letter_c_present():
    assert 'C' in letters.LETTERS


def test_values_are_bytes():
    for k, v in letters.LETTERS.items():
        assert isinstance(v, bytes), f"{k} not bytes"


def test_digits_present_if_available():
    for k, v in letters.LETTERS.items():
        if k.isdigit():
            assert isinstance(v, bytes)
