#!/usr/bin/env python3
"""
Extract TI TMS5100 allophone LPC clips from WORDS / LETTERS recordings.
Run from project root:  python tools/extract_allophones.py
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))

from data.words import WORDS
from data.letters import LETTERS
from engine.lpc import decode_frames, encode_frames

# ── Carve helpers ────────────────────────────────────────────────────────────

def carve(source: bytes, start: int, end: int) -> bytes:
    frames = decode_frames(source)
    if not frames:
        return b''
    sliced = frames[max(0, start):min(end, len(frames))]
    return encode_frames(sliced) if sliced else b''


def W(word, s, e):   return carve(WORDS.get(word, b''), s, e)
def L(letter, s, e): return carve(LETTERS.get(letter, b''), s, e)

def Wlast(word, n):
    src = WORDS.get(word, b'')
    f = decode_frames(src)
    return encode_frames(f[-n:]) if len(f) >= n else encode_frames(f)

def Llast(letter, n):
    src = LETTERS.get(letter, b'')
    f = decode_frames(src)
    return encode_frames(f[-n:]) if len(f) >= n else encode_frames(f)

def silence(n):
    return encode_frames([{'energy': 0, 'period': 0, 'k': [0]*10}] * n)

# ── Allophone source map ──────────────────────────────────────────────────────
# Frame selections verified against actual frame analysis.
# Notation:  W(word, start_frame, end_frame_exclusive)

ALLOPHONE_SOURCES = {

    # ── Vowels ────────────────────────────────────────────────────────────────
    # IY (as in "see"): letter E = pure IY, skip onset frame
    'IY':   L('E', 1, 5),
    # IH (as in "in"): IN starts with IH vowel (all voiced)
    'IH':   W('IN', 0, 3),
    # EH (as in "ten"): TEN frame0=T(U), frames 1-4 = EH
    'EH':   W('TEN', 1, 4),
    # AE (as in "and"): AND starts with AE
    'AE':   W('AND', 0, 3),
    # AA (as in "calm"): use CALM or ALL/AO approximation
    'AA':   W('ALL', 1, 4),          # AO ≈ AA
    # AH (as in "above"): ABOVE starts with AH
    'AH':   W('ABOVE', 0, 3),
    # AO (as in "all"): ALL frame0=onset, frames 1-4 = AO
    'AO':   W('ALL', 1, 4),
    # AW (as in "down"): DOWN frame0=D onset, frames 2-6 = AW+N
    'AW':   W('DOWN', 2, 5),
    # AX (unstressed schwa): THE frame0=DH(U), frame1=silence, frames 3-6 = AX
    'AX':   W('THE', 3, 6),
    # AY (as in "five"): FIVE frame0=F(U), frames 1-4 = AY
    'AY':   W('FIVE', 1, 4),
    # EY (as in "eight"): EIGHT starts with EY (all voiced)
    'EY':   W('EIGHT', 0, 3),
    # ER (as in "tower"): TOWER last 4 frames = ER
    'ER1':  Wlast('TOWER', 4),
    # OW (as in "slow"): SLOW frames 8-13 = steady OW vowel
    'OW':   W('SLOW', 8, 13),
    # OY: rough approximation from ROGER middle
    'OY':   W('ROGER', 2, 5),
    # UH (as in "full"): FULL frame0-2=F(U), frames 3-6 = UH
    'UH':   W('FULL', 3, 6),
    # UW (as in "two"): TWO frame0-1=T(U), frames 2-5 = UW
    'UW1':  W('TWO', 2, 5),
    'UW2':  W('YOU', 3, 6),

    # ── Stops ─────────────────────────────────────────────────────────────────
    # BB1: letter B = "bee", first 2 frames = B burst
    'BB1':  L('B', 0, 2),
    'BB2':  L('B', 0, 2),
    # DD1: letter D = "dee", first 2 frames = D burst
    'DD1':  L('D', 0, 2),
    'DD2':  L('D', 0, 2),
    # GG1: GEAR starts with G (voiced, frame 0)
    'GG1':  W('GEAR', 0, 2),
    'GG2':  W('GEAR', 0, 2),
    'GG3':  W('GEAR', 0, 2),
    # PP: letter P = "pee", first 2 frames = P burst
    'PP':   L('P', 0, 2),
    # TT1: TEN frame0 = T (unvoiced burst)
    'TT1':  W('TEN', 0, 2),
    'TT2':  W('TEN', 0, 2),
    # KK1: letter K = "kay", first 2 frames = K burst
    'KK1':  L('K', 0, 2),
    'KK2':  L('K', 0, 2),
    'KK3':  L('K', 0, 2),

    # ── Fricatives ────────────────────────────────────────────────────────────
    # FF: FIVE frames 0-1 = F (unvoiced)
    'FF':   W('FIVE', 0, 2),
    # VV: letter V = "vee", first 2 frames = V (voiced fricative)
    'VV':   L('V', 0, 3),
    # SS: SIX frames 0-1 = S (unvoiced)
    'SS':   W('SIX', 0, 3),
    # ZZ: ZERO frames 0-2 = Z (voiced, all voiced from start)
    'ZZ':   W('ZERO', 0, 3),
    # SH: SHORT frames 0-1 = SH (unvoiced)
    'SH':   W('SHORT', 0, 3),
    # ZH: no good source — use SH approximation
    'ZH':   W('SHORT', 0, 2),
    # TH: THREE frames 0-1 = TH (unvoiced)
    'TH':   W('THREE', 0, 3),
    # DH1: THE frame 0 = DH (unvoiced/semi-voiced)
    'DH1':  W('THE', 0, 2),
    'DH2':  W('THE', 0, 2),

    # ── Affricates ────────────────────────────────────────────────────────────
    # CH: CHECK frame 0 unvoiced (CH stop+friction), frames 0-2
    'CH':   W('CHECK', 0, 3),
    # JH: letter J = "jay", first 3 frames = JH
    'JH':   L('J', 0, 3),

    # ── Nasals ────────────────────────────────────────────────────────────────
    # MM: MAYDAY starts with M (all voiced)
    'MM':   W('MAYDAY', 0, 3),
    # NN1: NINE starts with N (all voiced, frame 0-1)
    'NN1':  W('NINE', 0, 2),
    'NN2':  W('NINE', 0, 2),
    # NG: LANDING last 3 frames = NG coda
    'NG':   Wlast('LANDING', 3),
    'NM':   Wlast('LANDING', 3),

    # ── Liquids ───────────────────────────────────────────────────────────────
    # LL: ALL last 2 frames = L coda (voiced)
    'LL':   Wlast('ALL', 2),
    # RR1: RIGHT starts with R (voiced)
    'RR1':  W('RIGHT', 0, 3),
    'RR2':  W('RIGHT', 0, 3),

    # ── Glides ────────────────────────────────────────────────────────────────
    # WW: WEATHER starts with W (all voiced)
    'WW':   W('WEATHER', 0, 2),
    'WH':   W('WEATHER', 0, 2),
    # YY1: YELLOW starts with Y (all voiced)
    'YY1':  W('YELLOW', 0, 2),
    'YY2':  W('YELLOW', 0, 2),

    # ── Aspirate ──────────────────────────────────────────────────────────────
    # HH1: HAVE frames 0-1 = HH (unvoiced aspiration)
    'HH1':  W('HAVE', 0, 2),
    'HH2':  W('HAVE', 0, 2),

    # ── Syllabic consonants ───────────────────────────────────────────────────
    'EL':   Wlast('ALL', 3),
    'EM':   W('MAYDAY', 0, 3),
    'EN':   W('NINE', 0, 2),

    # ── Pauses ────────────────────────────────────────────────────────────────
    'PA1':  silence(1),
    'PA2':  silence(2),
    'PA3':  silence(3),
    'PA4':  silence(4),
    'PA5':  silence(5),
}

ALLOPHONE_SOURCES = {k: v for k, v in ALLOPHONE_SOURCES.items() if v}


def generate() -> None:
    lines = [
        '# Auto-generated by tools/extract_allophones.py',
        '# TI TMS5100 allophone LPC clips carved from Vocab_US_TI99 / Vocab_US_Large',
        '',
        'ALLOPHONES: dict[str, bytes] = {',
    ]
    for name, data in sorted(ALLOPHONE_SOURCES.items()):
        hex_str = ', '.join(f'0x{b:02X}' for b in data)
        lines.append(f'    {name!r}: bytes([{hex_str}]),')
    lines.append('}')
    lines.append('')

    out = os.path.join(os.path.dirname(os.path.dirname(__file__)), 'data', 'allophones.py')
    with open(out, 'w') as f:
        f.write('\n'.join(lines))

    present = sum(1 for v in ALLOPHONE_SOURCES.values() if v)
    print(f'Wrote {present} allophones → {out}')
    missing = [k for k, v in ALLOPHONE_SOURCES.items() if not v]
    if missing:
        print(f'Skipped (no source): {missing}')


if __name__ == '__main__':
    generate()
