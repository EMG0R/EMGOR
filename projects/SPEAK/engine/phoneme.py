import pronouncing

_g2p = None


def _get_g2p():
    global _g2p
    if _g2p is not None:
        return _g2p
    try:
        from g2p_en import G2p
        _g2p = G2p()
    except Exception:
        _g2p = False   # mark as unavailable
    return _g2p


def to_arpabet(word: str) -> list[str]:
    """Convert a single word to a list of ARPABET phoneme tokens."""
    if not word:
        return []
    w = word.upper()

    # CMU Pronouncing Dictionary (best quality for known words)
    phones = pronouncing.phones_for_word(w.lower())
    if phones:
        return phones[0].split()

    # Neural G2P fallback for unknown/misspelled words
    g2p = _get_g2p()
    if g2p and g2p is not False:
        try:
            result = g2p(w)
            return [t for t in result if t.isalpha() or (len(t) > 1 and t[:-1].isalpha())]
        except Exception:
            pass

    return []
