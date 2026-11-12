"""
High-level voice router.

Returns (samples: np.float32, sample_rate: int) for any text.

Voice modes:
  modern  -> Apple `say` (clean, highest intelligibility)
  espeak  -> espeak-ng (robotic, retro-adjacent)
  ti      -> original TMS5100 LPC formant synth (authentic S&S)
  mix     -> `say` run through TI-color post-processor (intelligible + TI bite)

Single letters are ALWAYS rendered through the TI engine when mode is
`mix` or `ti`, and through `say` when mode is `modern`/`espeak` — giving
that recognisable Speak & Spell letter feel for alphabet reading.
"""
from __future__ import annotations
import re
import numpy as np

from data.letters import LETTERS
from engine.lpc import decode, concat_lpc
from engine.formant import synthesize_word
from engine.tts import say_synth, espeak_synth
from engine.ticolor import ti_color, TI_SR

# Letters that are also whole English words — route via `say`, not the
# single-letter TI blob. The pre-recorded "I" blob ("eye") in particular
# degrades into raw LPC transient nonsense; sending it through say+ti_color
# keeps it intelligible while preserving TI bite.
_LETTERS_AS_WORDS = {"I", "A"}

_PUNCT = re.compile(r"[^A-Z0-9' -]")
_TRAIL_VOWEL = re.compile(r'^(.*?)([AEIOU])\2{2,}$')

# Any 3+ repetition of a letter triggers sustained-tone mode
_ELONG = re.compile(r"^(.*?)([A-Z])\2{2,}(.*)$")

VALID_MODES = ("modern", "ti", "mix", "espeak")

# Central pitch for held-vowel oscillation. Consistent across utterances
# so "ooooo" never wobbles into a melody.
_SUSTAIN_MIDI = 43          # ~G2, matches Fred's perceived speaking pitch
_SUSTAIN_MS_PER_LETTER = 180

# Letter → formant vowel key understood by engine.singer.VOWELS
_VOWEL_MAP = {
    "A": "AH", "E": "EH", "I": "IH", "O": "OH", "U": "UH",
}


def _has_elongation(word: str) -> bool:
    return bool(_ELONG.match(word))


def _elongation_segments(word: str) -> list:
    """
    Walk the word and split into ordered segments:
        ("say",  text)                  -> speak via say
        ("hold", letter, count)         -> sustain that letter

    Each 'hold' absorbs all but one copy of its letter's run — the one
    copy lives in the preceding 'say' segment so Fred pronounces it
    naturally, then the hold extends it.
    """
    segs: list = []
    i = 0
    buf = ""
    while i < len(word):
        j = i
        while j < len(word) and word[j] == word[i]:
            j += 1
        run_len = j - i
        if run_len >= 3:
            buf += word[i]                       # one copy for pronunciation
            segs.append(("say", buf))
            buf = ""
            segs.append(("hold", word[i], run_len - 1))
            i = j
        else:
            buf += word[i:j]
            i = j
    if buf:
        segs.append(("say", buf))
    return segs


# ── TI engine -> float32 samples at 8 kHz ─────────────────────────────────

def _ti_word(word: str, pitch_mode: str = "flat") -> np.ndarray:
    w = word.upper().strip()
    if not w:
        return np.zeros(0, dtype=np.float32)

    # single letter? prefer the pre-recorded TI LPC blob
    if len(w) == 1 and w in LETTERS:
        return decode(LETTERS[w])

    # vowel elongation
    m = _TRAIL_VOWEL.match(w)
    if m:
        stem  = m.group(1) + m.group(2)
        vowel = m.group(2)
        extra = len(w) - len(stem)
        base  = synthesize_word(stem, pitch_mode)
        ext   = synthesize_word(vowel, pitch_mode)
        blob  = concat_lpc(base, *([ext] * extra))
        return decode(blob)

    return decode(synthesize_word(w, pitch_mode))


def _ti_text(text: str, pitch_mode: str = "flat") -> tuple[np.ndarray, int]:
    cleaned = _PUNCT.sub('', text.upper())
    tokens = cleaned.split()
    if not tokens:
        return np.zeros(0, dtype=np.float32), TI_SR
    parts = [_ti_word(t, pitch_mode) for t in tokens]
    # small silence between words
    gap = np.zeros(int(TI_SR * 0.08), dtype=np.float32)
    out = []
    for i, p in enumerate(parts):
        if i: out.append(gap)
        out.append(p)
    return np.concatenate(out), TI_SR


# ── Mix mode: modern words + TI color; authentic TI for single letters ───

def _split_letters(text: str) -> list[tuple[str, str]]:
    """Group text into ('letter', 'X') / ('word', 'HELLO') tokens."""
    cleaned = _PUNCT.sub('', text.upper())
    out = []
    for tok in cleaned.split():
        if len(tok) == 1 and tok in LETTERS and tok not in _LETTERS_AS_WORDS:
            out.append(("letter", tok))
        else:
            out.append(("word", tok))
    return out


def _resample_to(samples: np.ndarray, src_sr: int, dst_sr: int) -> np.ndarray:
    if src_sr == dst_sr or len(samples) == 0:
        return samples
    from scipy.signal import resample_poly
    from math import gcd
    g = gcd(src_sr, dst_sr)
    return resample_poly(samples, dst_sr // g, src_sr // g).astype(np.float32)


def _fade_edges(samples: np.ndarray, sr: int,
                fade_in_ms: float = 8.0,
                fade_out_ms: float = 30.0) -> np.ndarray:
    """Apply linear fades at both edges to suppress clicks."""
    if len(samples) < 64:
        return samples
    out = samples.astype(np.float32).copy()
    fi = min(int(sr * fade_in_ms / 1000), len(out) // 4)
    fo = min(int(sr * fade_out_ms / 1000), len(out) // 4)
    if fi > 1:
        out[:fi] *= np.linspace(0.0, 1.0, fi, dtype=np.float32)
    if fo > 1:
        out[-fo:] *= np.linspace(1.0, 0.0, fo, dtype=np.float32)
    return out


_VOWEL_SAY_WORD = {
    "A": "ahh",  "E": "ehh",  "I": "eee",  "O": "ohh",  "U": "ooo",
}

# per-letter word fed to say to generate a sustainable sample
_LETTER_SAY_WORD = {
    "A": "ahhhh", "E": "ehhhh", "I": "eeeee", "O": "ohhhh", "U": "oooo",
    "M": "mmmmm", "N": "nnnnn", "L": "lllll", "R": "rrrrr",
    "S": "sssss", "F": "fffff", "V": "vvvvv", "Z": "zzzzz",
    "H": "hhhhh", "W": "wooo",  "Y": "yeeee",
    "B": "bbuuuuh", "P": "ppuuuh", "T": "tuuh", "D": "dduuuh",
    "K": "kkuuuh", "G": "gguuh",  "C": "sssss", "J": "juuuh",
    "Q": "kwuuh",   "X": "ksss",
}


def _natural_pitch_hz(samples: np.ndarray, sr: int,
                      tail_ms: float = 200.0,
                      fallback: float = 98.0) -> float:
    """Median f0 of the last `tail_ms` of `samples`. Fallback if unvoiced."""
    if len(samples) < 256:
        return fallback
    try:
        import pyworld as pw
        x = samples.astype(np.float64)
        f0, t = pw.dio(x, sr, frame_period=5.0)
        f0 = pw.stonemask(x, f0, t, sr)
        if len(f0) == 0:
            return fallback
        n_tail = max(10, int(len(f0) * tail_ms / (len(x) / sr * 1000)))
        tail = f0[-n_tail:]
        voiced = tail[tail > 0]
        if len(voiced) == 0:
            voiced = f0[f0 > 0]
        if len(voiced) == 0:
            return fallback
        return float(np.median(voiced))
    except Exception:
        return fallback


def _trim_silence(x: np.ndarray, thr_frac: float = 0.01,
                  head: bool = True, tail: bool = True) -> np.ndarray:
    """Strip near-silent leading/trailing samples."""
    if len(x) < 8:
        return x
    env = np.abs(x)
    peak = float(np.max(env) or 1.0)
    above = env > thr_frac * peak
    if not above.any():
        return x
    a = int(np.argmax(above)) if head else 0
    b = len(x) - int(np.argmax(above[::-1])) if tail else len(x)
    return x[a:b].astype(np.float32)


def _crossfade_concat(prev: np.ndarray, nxt: np.ndarray, sr: int,
                      xfade_ms: float = 35.0) -> np.ndarray:
    """Overlap-and-ramp join so there's no audible gap or click."""
    # trim silence at the splice edges so crossfade happens on real signal
    prev = _trim_silence(prev, head=False, tail=True)
    nxt  = _trim_silence(nxt,  head=True,  tail=False)
    if len(prev) == 0:
        return nxt.astype(np.float32)
    if len(nxt) == 0:
        return prev.astype(np.float32)
    n = min(int(sr * xfade_ms / 1000), len(prev), len(nxt))
    if n < 4:
        return np.concatenate([prev, nxt]).astype(np.float32)
    fade_out = np.linspace(1.0, 0.0, n, dtype=np.float32)
    fade_in  = np.linspace(0.0, 1.0, n, dtype=np.float32)
    overlap = prev[-n:] * fade_out + nxt[:n] * fade_in
    return np.concatenate([prev[:-n], overlap, nxt[n:]]).astype(np.float32)


def _sustain_tone_raw(letter: str, dur_ms: float, sr: int,
                      target_hz: float) -> np.ndarray:
    """
    Rock-solid held letter: pyworld freeze-frame resynth.
    Takes Fred's natural vowel/continuant sample, extracts spectral
    envelope at one steady middle frame, forces every output frame to
    that same frame at a constant target pitch → zero wobble, zero
    retrigger. RAW (no TI color, no fades — caller handles both).
    """
    from engine.singer import _render_syllable_world
    word = _LETTER_SAY_WORD.get(letter.upper(), letter.lower() * 3)
    midi_equiv = 69 + 12 * float(np.log2(max(40.0, target_hz) / 440.0))
    return _render_syllable_world(word, midi_equiv, dur_ms, sr=sr,
                                  force_voiced=True, freeze_frame=True)


# ── Public API ────────────────────────────────────────────────────────────

# Punctuation → silence (ms) inserted between clauses so the ear can hear
# sentence and clause boundaries. Period/!/? give a real beat; comma/;/: a
# breath; newline a paragraph break.
_PAUSE_MS = {
    ".": 380, "!": 380, "?": 380,
    ",": 180, ";": 220, ":": 200,
    "\n": 380,
}
_CLAUSE_SPLIT = re.compile(r"([.!?,;:\n])")


def synthesize(text: str, mode: str = "mix",
               pitch_mode: str = "flat",
               ti_amount: float = 0.85,
               sing: bool = False) -> tuple[np.ndarray, int]:
    """Synthesize text → (float32 samples, sample_rate)."""
    if mode not in VALID_MODES:
        mode = "mix"
    if not text.strip():
        return np.zeros(0, dtype=np.float32), TI_SR

    # If the text has clause-level punctuation, render each clause separately
    # and stitch with silence gaps so periods/commas/etc. are audible.
    if _CLAUSE_SPLIT.search(text):
        parts = _CLAUSE_SPLIT.split(text)
        clauses: list[tuple[str, int]] = []  # (clause_text, trailing_pause_ms)
        buf = ""
        for tok in parts:
            if tok in _PAUSE_MS:
                if buf.strip():
                    clauses.append((buf, _PAUSE_MS[tok]))
                    buf = ""
            else:
                buf += tok
        if buf.strip():
            clauses.append((buf, 0))

        if len(clauses) > 1 or (clauses and clauses[0][1] > 0):
            chunks: list[np.ndarray] = []
            sr_out = TI_SR
            for clause_text, pause_ms in clauses:
                s, sr = synthesize(clause_text, mode=mode,
                                   pitch_mode=pitch_mode,
                                   ti_amount=ti_amount, sing=sing)
                if len(s):
                    chunks.append(s.astype(np.float32))
                    sr_out = sr
                if pause_ms > 0:
                    chunks.append(np.zeros(int(sr_out * pause_ms / 1000),
                                           dtype=np.float32))
            if not chunks:
                return np.zeros(0, dtype=np.float32), sr_out
            return np.concatenate(chunks).astype(np.float32), sr_out

    if mode == "ti":
        s, sr = _ti_text(text, pitch_mode)
        s = _fade_edges(s, sr, fade_in_ms=4.0, fade_out_ms=40.0)
        peak = float(np.max(np.abs(s)) or 1.0)
        if peak > 0.95:
            s = (s / peak * 0.9).astype(np.float32)
        return s, sr

    if mode == "modern":
        return _modern_with_elongation(text)

    if mode == "espeak":
        return espeak_synth(text)

    # mix: letter-by-letter routing, TI color on word spans
    tokens = _split_letters(text)
    if not tokens:
        return np.zeros(0, dtype=np.float32), TI_SR

    # group consecutive word tokens to feed `say` as a phrase (prosody)
    out_chunks: list[np.ndarray] = []
    out_sr = TI_SR
    i = 0
    gap = np.zeros(int(TI_SR * 0.08), dtype=np.float32)
    while i < len(tokens):
        kind, val = tokens[i]
        if kind == "letter":
            chunk = _ti_word(val, pitch_mode)
            if out_chunks: out_chunks.append(gap)
            out_chunks.append(chunk)
            i += 1
        else:
            # collect a run of non-elongated words; flush on elongation
            j = i
            buf: list[str] = []
            while j < len(tokens) and tokens[j][0] == "word":
                w = tokens[j][1]
                if _has_elongation(w):
                    break
                buf.append(w)
                j += 1

            if buf:
                phrase = " ".join(buf)
                samples, sr = say_synth(phrase)
                colored, csr = ti_color(samples, sr, amount=ti_amount, sing=sing)
                colored = _resample_to(colored, csr, TI_SR)
                if out_chunks: out_chunks.append(gap)
                out_chunks.append(colored)

            # if the next token is an elongated word, walk its segments
            if j < len(tokens) and tokens[j][0] == "word":
                w = tokens[j][1]
                if _has_elongation(w):
                    # build RAW (uncolored) word via crossfade across segments,
                    # then TI-color the whole word in ONE pass (no per-segment
                    # tail attenuation = no gaps).
                    say_sr_out = 22050
                    word_raw = np.zeros(0, dtype=np.float32)
                    last_hz = 98.0
                    for seg in _elongation_segments(w):
                        if seg[0] == "say":
                            s_raw, srs = say_synth(seg[1])
                            last_hz = _natural_pitch_hz(s_raw, srs,
                                                        fallback=last_hz)
                            chunk = _resample_to(s_raw, srs, say_sr_out)
                            word_raw = _crossfade_concat(
                                word_raw, chunk, say_sr_out, xfade_ms=110.0)
                        else:
                            _, letter, n = seg
                            dur_ms = n * _SUSTAIN_MS_PER_LETTER
                            # sustain sits a fifth below the detected natural
                            # pitch — smoother, less strident on R/M/etc.
                            sustain_hz = last_hz * (2 ** (-7 / 12.0))
                            tone = _sustain_tone_raw(letter, dur_ms,
                                                     say_sr_out, sustain_hz)
                            tone = (tone * 0.80).astype(np.float32)
                            word_raw = _crossfade_concat(
                                word_raw, tone, say_sr_out, xfade_ms=110.0)

                    # single TI-color pass over the whole word
                    colored, csr = ti_color(word_raw, say_sr_out,
                                            amount=ti_amount, sing=sing)
                    colored = _resample_to(colored, csr, TI_SR)

                    if out_chunks: out_chunks.append(gap)
                    out_chunks.append(colored)
                    j += 1

            i = j

    if not out_chunks:
        return np.zeros(0, dtype=np.float32), out_sr
    result = np.concatenate(out_chunks).astype(np.float32)

    # anti-click: short fade at the very end of the utterance
    result = _fade_edges(result, out_sr, fade_in_ms=4.0, fade_out_ms=40.0)

    peak = float(np.max(np.abs(result)) or 1.0)
    if peak > 0.95:
        result = result / peak * 0.9
    return result, out_sr


def _modern_with_elongation(text: str) -> tuple[np.ndarray, int]:
    """`modern` voice with 3+ vowel sustained-tone support + end fade."""
    cleaned = _PUNCT.sub('', text.upper())
    tokens = cleaned.split()
    if not tokens:
        return np.zeros(0, dtype=np.float32), 22050

    chunks: list[np.ndarray] = []
    buf: list[str] = []
    sr_out = 22050

    def flush():
        nonlocal buf
        if not buf:
            return
        phrase = " ".join(buf)
        samples, sr = say_synth(phrase)
        chunks.append(_resample_to(samples, sr, sr_out))
        buf = []

    for tok in tokens:
        if not _has_elongation(tok):
            buf.append(tok)
            continue
        flush()
        word_wav = np.zeros(0, dtype=np.float32)
        last_hz = 110.0
        from engine.singer import _render_syllable_world
        for seg in _elongation_segments(tok):
            if seg[0] == "say":
                s_raw, sr = say_synth(seg[1])
                last_hz = _natural_pitch_hz(s_raw, sr, fallback=last_hz)
                chunk = _resample_to(s_raw, sr, sr_out)
                word_wav = _crossfade_concat(word_wav, chunk, sr_out,
                                             xfade_ms=110.0)
            else:
                _, letter, n = seg
                dur_ms = n * _SUSTAIN_MS_PER_LETTER
                word = _LETTER_SAY_WORD.get(letter, letter.lower() * 3)
                sustain_hz = last_hz * (2 ** (-7 / 12.0))
                midi_equiv = 69 + 12 * float(np.log2(max(40.0, sustain_hz) / 440.0))
                tone = _render_syllable_world(word, midi_equiv, dur_ms,
                                              sr=sr_out, force_voiced=True,
                                              freeze_frame=True)
                tone = (tone * 0.80).astype(np.float32)
                word_wav = _crossfade_concat(word_wav, tone, sr_out,
                                             xfade_ms=110.0)
        chunks.append(word_wav)
    flush()

    if not chunks:
        return np.zeros(0, dtype=np.float32), sr_out
    out = np.concatenate(chunks).astype(np.float32)
    out = _fade_edges(out, sr_out, fade_in_ms=4.0, fade_out_ms=40.0)
    peak = float(np.max(np.abs(out)) or 1.0)
    if peak > 0.95:
        out = out / peak * 0.9
    return out, sr_out
