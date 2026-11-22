"""
Formant-driven TMS5100 synthesis engine v3.

Pipeline:
  text → ARPABET phonemes (via CMU dict / g2p_en)
       → anchor K-vectors (vowels: real TI templates, consonants: formant synthesis)
       → per-frame K interpolation with locus-based C-V transitions
       → encode_frames() → playable LPC bytes

Vowel steady-state K-vectors come from real TI Speak&Spell recordings so the
vowel timbre is authentic.  Transitions and consonant frames use formant-derived
K-vectors so coarticulation is continuous.  Stop consonants get a silence frame
plus a noise-burst frame.
"""

import math
import re
from dataclasses import dataclass
from engine.lpc import K as K_TABLES, K_BITS, encode_frames
from engine.phoneme import to_arpabet

FS             = 8000
MKFRACT        = 512
DEFAULT_PITCH_IDX = 12

_F4, _B4 = 3300.0, 250.0
_F5, _B5 = 3700.0, 300.0


# ── Phoneme data ──────────────────────────────────────────────────────────────

@dataclass
class Ph:
    f1: float; f2: float; f3: float
    b1: float = 60.0; b2: float = 90.0; b3: float = 150.0
    voiced: bool = True
    energy: int = 9
    dur: int = 5

PH: dict[str, Ph] = {
    # Vowels — formant targets used only for consonant context
    'IY': Ph(270,  2290, 3010,  60,  90, 150, True,  9, 6),
    'IH': Ph(390,  1990, 2550,  70, 100, 160, True,  9, 5),
    'EH': Ph(530,  1840, 2480,  70, 100, 160, True,  9, 5),
    'AE': Ph(660,  1720, 2410,  80, 110, 165, True,  9, 6),
    'AA': Ph(730,  1090, 2440,  80, 110, 155, True,  9, 6),
    'AH': Ph(520,  1190, 2390,  75, 100, 155, True,  8, 5),
    'AO': Ph(570,   920, 2410,  75, 115, 155, True,  9, 5),
    'OW': Ph(300,  1020, 2240,  70, 120, 150, True,  9, 6),
    'UH': Ph(440,  1020, 2240,  75, 115, 160, True,  8, 5),
    'UW': Ph(300,  1000, 2240,  70, 120, 150, True,  8, 5),
    'ER': Ph(500,  1350, 1690,  75, 105, 165, True,  8, 6),
    'AX': Ph(430,  1170, 2530,  85, 115, 165, True,  7, 3),
    'AY': Ph(730,  1090, 2440,  80, 110, 155, True,  9, 8),
    'AW': Ph(730,  1090, 2440,  80, 110, 155, True,  9, 8),
    'OY': Ph(570,   920, 2410,  75, 115, 155, True,  9, 8),
    'EY': Ph(530,  1840, 2480,  75, 105, 155, True,  9, 7),
    # Nasals
    'M':  Ph(250,   950, 2200,  80, 250, 260, True,  7, 4),
    'N':  Ph(250,  1500, 2200,  80, 250, 260, True,  7, 4),
    'NG': Ph(250,  2300, 2500,  80, 250, 260, True,  7, 3),
    # Liquids — wide B2 approximates lateral anti-resonance for L
    'L':  Ph(360,  1100, 2600,  75, 320, 280, True,  8, 4),
    'R':  Ph(400,  1200, 1650,  75, 130, 240, True,  8, 4),
    # Glides
    'W':  Ph(300,   700, 2200,  70, 110, 210, True,  8, 5),
    'Y':  Ph(270,  2290, 3010,  65,  95, 160, True,  8, 3),
    # Aspirate
    'HH': Ph(500,  1500, 2500, 400, 600, 700, False, 6, 2),
    # Voiced stops (voice bar)
    'B':  Ph(200,   750, 2500, 200, 300, 400, True,  5, 2),
    'D':  Ph(200,  1800, 2500, 200, 300, 400, True,  5, 2),
    'G':  Ph(200,  2300, 2500, 200, 300, 400, True,  5, 2),
    # Unvoiced stops (silence + burst)
    'P':  Ph(200,   750, 2500, 400, 500, 600, False, 4, 2),
    'T':  Ph(200,  1800, 2500, 400, 500, 600, False, 4, 2),
    'K':  Ph(200,  2300, 2500, 400, 500, 600, False, 4, 2),
    # Fricatives
    'F':  Ph(500,  1500, 3000, 500, 700, 900, False, 6, 4),
    'TH': Ph(500,  1500, 3000, 500, 700, 900, False, 5, 3),
    'S':  Ph(500,  1500, 3500, 600, 800, 950, False, 7, 4),
    'SH': Ph(400,  2000, 3000, 500, 700, 800, False, 7, 4),
    'V':  Ph(500,  1500, 3000, 160, 270, 370, True,  6, 4),
    'DH': Ph(500,  1500, 3000, 160, 270, 370, True,  5, 3),
    'Z':  Ph(500,  1500, 3500, 160, 270, 480, True,  6, 4),
    'ZH': Ph(400,  2000, 3000, 160, 270, 370, True,  6, 4),
    # Affricates
    'CH': Ph(400,  2000, 3000, 500, 700, 800, False, 7, 4),
    'JH': Ph(400,  2000, 3000, 160, 270, 370, True,  6, 4),
}

ARPABET_MAP: dict[str, str] = {
    'AA': 'AA', 'AE': 'AE', 'AH': 'AH', 'AO': 'AO',
    'AW': 'AW', 'AY': 'AY', 'EH': 'EH', 'ER': 'ER',
    'EY': 'EY', 'IH': 'IH', 'IY': 'IY', 'OW': 'OW',
    'OY': 'OY', 'UH': 'UH', 'UW': 'UW', 'AX': 'AX',
    'B':  'B',  'CH': 'CH', 'D':  'D',  'DH': 'DH',
    'F':  'F',  'G':  'G',  'HH': 'HH', 'JH': 'JH',
    'K':  'K',  'L':  'L',  'M':  'M',  'N':  'N',
    'NG': 'NG', 'P':  'P',  'R':  'R',  'S':  'S',
    'SH': 'SH', 'T':  'T',  'TH': 'TH', 'V':  'V',
    'W':  'W',  'Y':  'Y',  'Z':  'Z',  'ZH': 'ZH',
    'AXR': 'ER', 'IX': 'IH', 'UX': 'UW', 'NX': 'N',
}

_STRESS_RE = re.compile(r'[012]$')
_VOWELS = {'IY','IH','EH','AE','AA','AH','AO','OW','UH','UW','ER','AX',
           'AY','AW','OY','EY'}
_STOPS = {'B', 'D', 'G', 'P', 'T', 'K'}


# ── Real TI Speak&Spell K-vector templates ────────────────────────────────────
# Extracted from wordbank recordings via longest stable voiced-frame run.
# These are K-TABLE INDICES (not raw K-values).

VOWEL_K = {
    'IY': [28, 8, 10, 12, 8, 7, 2, 3, 6, 1],    # THREE
    'IH': [17, 7, 14,  1, 3, 6, 3, 7, 5, 3],    # IN
    'EH': [26, 22, 9,  9, 2,13, 4, 3, 6, 6],    # LEFT
    'AE': [28, 20, 12,10,14,11, 6, 4, 1, 5],    # HAVE
    'AA': [11, 12, 11, 4,13, 7, 2, 6, 4, 4],    # ACKNOWLEDGE
    'AH': [25, 23,  5,15, 9,10,10, 2, 5, 3],    # ONE
    'AO': [10, 18,  2,11, 9, 2, 7, 0, 5, 5],    # FOUR
    'OW': [11,  1, 12,12,10,11,10, 3, 1, 1],    # SLOW
    'UH': [ 6, 27,  6, 4, 9,13, 9, 0, 6, 3],    # FULL
    'UW': [11, 29, 10,11, 1, 3, 3, 2, 1, 4],    # NEW
    'ER': [21,  8, 14, 4, 5, 8,11, 6, 6, 3],    # AFFIRMATIVE
    'AX': [12,  3,  7,13, 5,10, 9, 1, 3, 3],    # AND
}

# Diphthong: onset and endpoint K-vectors
DIPH_ONSET_K = {
    'AY': VOWEL_K['AA'],
    'AW': VOWEL_K['AA'],
    'OY': VOWEL_K['AO'],
    'EY': VOWEL_K['EH'],
}
DIPH_END_K = {
    'AY': VOWEL_K['IY'],
    'AW': VOWEL_K['UW'],
    'OY': VOWEL_K['IY'],
    'EY': VOWEL_K['IY'],
}

# Mid-table neutral K for sentence onset
_NEUTRAL_K = [16, 16, 8, 8, 8, 8, 8, 4, 4, 4]

# Clamp K-table index to valid range
def _clamp_k(k: list[int]) -> list[int]:
    return [max(0, min(len(K_TABLES[i]) - 1, k[i])) for i in range(10)]


# ── Formant → K (used for consonant anchor K-vectors) ────────────────────────

def _formants_to_parcor(freqs, bws, fs=FS):
    import cmath as cm
    nyq = fs * 0.5 - 30.0
    poly: list[complex] = [complex(1.0)]
    for f, b in zip(freqs, bws):
        f = max(30.0, min(nyq, f))
        r = math.exp(-math.pi * b / fs)
        theta = 2.0 * math.pi * f / fs
        z = cm.rect(r, theta)
        for pole in (z, z.conjugate()):
            new = [complex(0.0)] * (len(poly) + 1)
            for i, c in enumerate(poly):
                new[i]     += c
                new[i + 1] -= c * pole
            poly = new
    a = [c.real for c in poly[1:]]
    return _lpc_to_parcor(a)


def _lpc_to_parcor(a):
    p = len(a); k = [0.0] * p; cur = list(a)
    for i in range(p - 1, -1, -1):
        ki = max(-0.975, min(0.975, -cur[i])); k[i] = ki
        if i == 0: break
        denom = 1.0 - ki * ki
        if denom < 1e-6: break
        nxt = [(cur[j] + ki * cur[i - 1 - j]) / denom for j in range(i)]
        cur = nxt
    return k


def _quantise(ki, table_idx):
    scaled = ki * MKFRACT; tbl = K_TABLES[table_idx]
    best, best_d = 0, float('inf')
    for j, v in enumerate(tbl):
        d = abs(v - scaled)
        if d < best_d: best_d, best = d, j
    return best


def formants_to_k(f1, f2, f3, b1=60.0, b2=90.0, b3=150.0,
                  f4=_F4, b4=_B4, f5=_F5, b5=_B5) -> list[int]:
    parcor = _formants_to_parcor([f1,f2,f3,f4,f5], [b1,b2,b3,b4,b5])
    return [_quantise(parcor[i], i) for i in range(10)]


def _ph_to_k(ph: Ph) -> list[int]:
    return formants_to_k(ph.f1, ph.f2, ph.f3, ph.b1, ph.b2, ph.b3)


# ── K-vector interpolation ────────────────────────────────────────────────────

def _lerp_k(ka: list[int], kb: list[int], t: float) -> list[int]:
    return _clamp_k([round(ka[i] + (kb[i] - ka[i]) * t) for i in range(10)])


def _frame(energy: int, period_idx: int, k: list[int]) -> dict:
    return {'energy': energy, 'period': period_idx, 'k': list(k)}


# ── Frame generators ──────────────────────────────────────────────────────────

def _make_frames(anchor_k, n_frames, prev_k, next_k,
                 energy, period_idx, is_vowel, is_diph=False,
                 diph_end_k=None):
    """
    Generate n_frames with smooth K-vector interpolation.

    Onset: first 30% interpolates from prev_k → anchor_k.
    Offset: last 20% (vowels only) interpolates anchor_k → next_k.
    Diphthong: after onset, glides anchor_k → diph_end_k.
    """
    coart_in  = max(1, int(n_frames * 0.30))
    coart_out = max(1, int(n_frames * 0.20)) if (is_vowel and next_k) else 0

    steady_k = anchor_k  # non-diph steady state

    frames = []
    for j in range(n_frames):
        if j < coart_in:
            t = j / coart_in
            k = _lerp_k(prev_k, anchor_k, t)
        elif is_diph and diph_end_k:
            glide_len = n_frames - coart_in - coart_out
            if glide_len <= 0:
                k = list(anchor_k)
            else:
                gt = (j - coart_in) / glide_len
                k = _lerp_k(anchor_k, diph_end_k, min(gt, 1.0))
                steady_k = k  # track for offset start
        elif coart_out > 0 and j >= n_frames - coart_out and next_k:
            t = (j - (n_frames - coart_out)) / coart_out
            base = diph_end_k if (is_diph and diph_end_k) else anchor_k
            k = _lerp_k(base, next_k, t)
        else:
            k = list(anchor_k)

        e = energy
        if j == 0 and n_frames > 2:          e = max(1, e - 3)
        elif j == n_frames - 1 and n_frames > 2: e = max(1, e - 2)

        frames.append(_frame(e, period_idx, k))
    return frames


def _stop_frames(ph: Ph, voiced_period_idx: int) -> list[dict]:
    """Closure silence + aspiration/burst frame for stop consonants."""
    k_burst = _ph_to_k(ph)
    silence = _frame(0, 0, [0] * 10)
    if ph.voiced:
        burst = _frame(5, voiced_period_idx, k_burst)
    else:
        burst = _frame(11, 0, k_burst)   # high-energy noise burst
    return [silence, burst]


# ── Helpers ───────────────────────────────────────────────────────────────────

def _strip_stress(tok: str) -> tuple[str, int]:
    m = _STRESS_RE.search(tok)
    return (tok[:m.start()], int(m.group())) if m else (tok, 0)


# ── Public entry points ───────────────────────────────────────────────────────

def synthesize_text(
    text: str,
    pitch_mode: str = 'flat',
    base_pitch_idx: int = DEFAULT_PITCH_IDX,
) -> bytes:
    if not text.strip():
        return b''

    # 1. Text → ARPABET tokens
    words = re.sub(r"[^A-Za-z0-9' -]", '', text).upper().split()
    all_tokens: list[tuple[str, int]] = []
    for w in words:
        for tok in to_arpabet(w):
            bare, stress = _strip_stress(tok)
            ph_key = ARPABET_MAP.get(bare)
            if ph_key:
                all_tokens.append((ph_key, stress))

    if not all_tokens:
        return b''
    n = len(all_tokens)

    # 2. Durations with stress scaling
    durations: list[int] = []
    for ph_key, stress in all_tokens:
        ph = PH[ph_key]; d = ph.dur
        if ph_key in _VOWELS:
            if stress == 1:   d = max(d, int(d * 1.5))
            elif stress == 0: d = max(2, int(d * 0.75))
        durations.append(d)

    # 3. Pitch contour
    total_frames = sum(durations)
    pitch_indices: list[int] = []
    for (ph_key, stress), dur in zip(all_tokens, durations):
        ph = PH[ph_key]
        if not ph.voiced:
            pitch_indices.extend([0] * dur); continue
        if pitch_mode == 'flat':
            pitch_indices.extend([base_pitch_idx] * dur)
        else:
            base = base_pitch_idx
            if ph_key in _VOWELS and stress == 1:
                base = max(1, base_pitch_idx - 3)
            p_list = []
            for j in range(dur):
                pos = (len(pitch_indices) + j) / max(total_frames, 1)
                fall = int(max(0.0, (pos - 0.80) / 0.20) * 5)
                p_list.append(min(31, base + fall))
            pitch_indices.extend(p_list)

    # 4. Build anchor K-vectors for every phoneme
    anchor_k: list[list[int]] = []
    for ph_key, _ in all_tokens:
        if ph_key in VOWEL_K:
            anchor_k.append(VOWEL_K[ph_key])
        elif ph_key in DIPH_ONSET_K:
            anchor_k.append(DIPH_ONSET_K[ph_key])
        else:
            anchor_k.append(_ph_to_k(PH[ph_key]))

    # 5. Generate frames
    all_frames: list[dict] = []
    pitch_cursor = 0

    for i, ((ph_key, stress), dur) in enumerate(zip(all_tokens, durations)):
        ph  = PH[ph_key]
        pit = pitch_indices[min(pitch_cursor, len(pitch_indices) - 1)]

        prev_k = anchor_k[i - 1] if i > 0 else _NEUTRAL_K
        next_k = anchor_k[i + 1] if i < n - 1 else None
        this_k = anchor_k[i]

        is_vowel = ph_key in _VOWELS
        is_diph  = ph_key in DIPH_ONSET_K
        diph_end = DIPH_END_K.get(ph_key)

        if ph_key in _STOPS:
            frames = _stop_frames(ph, pit)
        else:
            frames = _make_frames(
                this_k, dur, prev_k, next_k,
                ph.energy, pit,
                is_vowel=is_vowel, is_diph=is_diph, diph_end_k=diph_end,
            )
            # Apply per-frame pitch from contour
            for j, fr in enumerate(frames):
                if ph.voiced:
                    fr['period'] = pitch_indices[
                        min(pitch_cursor + j, len(pitch_indices) - 1)]

        all_frames.extend(frames)
        pitch_cursor += dur

    return encode_frames(all_frames)


def synthesize_word(word: str, pitch_mode: str = 'flat') -> bytes:
    return synthesize_text(word, pitch_mode=pitch_mode)
