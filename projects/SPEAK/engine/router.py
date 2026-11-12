import re
from engine.lpc import concat_lpc
from engine.formant import synthesize_text, synthesize_word

_PUNCT = re.compile(r"[^A-Z0-9' -]")

# Trailing repeated vowel: 3+ identical vowels → elongation
_TRAIL_VOWEL = re.compile(r'^(.*?)([AEIOU])\2{2,}$')


def route_text(text: str, pitch_mode: str = 'flat') -> bytes:
    """
    Primary entry point.  Converts any English text to TMS5100 LPC bytes
    using continuous formant synthesis (SAM-style articulation, TI texture).

    pitch_mode: 'flat' = TI monotone (default) | 'natural' = rising/falling
    """
    if not text.strip():
        return b''
    cleaned = _PUNCT.sub('', text.upper())
    tokens  = cleaned.split()
    return concat_lpc(*[_route_token(t, pitch_mode) for t in tokens])


def _route_token(word: str, pitch_mode: str) -> bytes:
    w = word.upper().strip()
    if not w:
        return b''

    # ── Vowel elongation: "ooooo" / "noooo" ─────────────────────────────────
    m = _TRAIL_VOWEL.match(w)
    if m:
        stem  = m.group(1) + m.group(2)
        vowel = m.group(2)
        extra = len(w) - len(stem)   # extra repetitions of the vowel phoneme
        base  = synthesize_word(stem, pitch_mode)
        ext   = synthesize_word(vowel, pitch_mode)
        return concat_lpc(base, *([ext] * extra))

    return synthesize_word(w, pitch_mode)


# ── Word-bank access (hidden tier, always authentic TI recordings) ─────────────

def wordbank_text(text: str) -> bytes:
    """
    Speak using only pre-recorded TI LPC clips.  Falls back to formant
    synthesis for words not in the bank.  Useful for comparing quality.
    """
    from data.words import WORDS
    from data.letters import LETTERS
    cleaned = _PUNCT.sub('', text.upper())
    parts   = []
    for t in cleaned.split():
        if t in WORDS:
            parts.append(WORDS[t])
        elif len(t) == 1 and t in LETTERS:
            parts.append(LETTERS[t])
        else:
            parts.append(synthesize_word(t))
    return concat_lpc(*parts) if parts else b''


# Legacy shim kept for existing tests
def route_word(word: str) -> bytes:
    return _route_token(word.upper().strip(), 'flat')
