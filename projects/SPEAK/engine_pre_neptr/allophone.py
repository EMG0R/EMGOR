import re
from data.allophones import ALLOPHONES
from engine.phoneme import to_arpabet
from engine.lpc import decode_frames, encode_frames

# ARPABET → TI allophone name
ARPABET_TO_TI: dict[str, str] = {
    'AA': 'AA',  'AE': 'AE',  'AH': 'AH',  'AO': 'AO',
    'AW': 'AW',  'AY': 'AY',  'EH': 'EH',  'ER': 'ER1',
    'EY': 'EY',  'IH': 'IH',  'IY': 'IY',  'OW': 'OW',
    'OY': 'OY',  'UH': 'UH',  'UW': 'UW1',
    'B':  'BB1', 'CH': 'CH',  'D':  'DD1', 'DH': 'DH1',
    'F':  'FF',  'G':  'GG1', 'HH': 'HH1', 'JH': 'JH',
    'K':  'KK1', 'L':  'LL',  'M':  'MM',  'N':  'NN1',
    'NG': 'NG',  'P':  'PP',  'R':  'RR1', 'S':  'SS',
    'SH': 'SH',  'T':  'TT1', 'TH': 'TH',  'V':  'VV',
    'W':  'WW',  'Y':  'YY1', 'Z':  'ZZ',  'ZH': 'ZH',
}

_STRESS = re.compile(r'[012]$')


def _strip_stress(token: str) -> str:
    return _STRESS.sub('', token)


def _lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * t


def _crossfade(clips: list[bytes], blend: int = 1) -> bytes:
    """Concatenate LPC clips with `blend` frames of linear K-interpolation at junctions."""
    if not clips:
        return b''
    if len(clips) == 1:
        return clips[0]

    all_frames: list[dict] = []
    for i, clip in enumerate(clips):
        frames = decode_frames(clip)
        if not frames:
            continue
        if i == 0 or not all_frames:
            all_frames.extend(frames)
            continue

        # Blend last `blend` frames of previous clip into first `blend` of this clip
        n = min(blend, len(all_frames), len(frames))
        for j in range(n):
            t = (j + 1) / (n + 1)
            prev = all_frames[-(n - j)]
            curr = frames[j]
            blended_k = [round(_lerp(prev['k'][ki], curr['k'][ki], t)) for ki in range(10)]
            blended_e = max(1, round(_lerp(prev['energy'], curr['energy'], t)))
            blended_p = round(_lerp(prev['period'], curr['period'], t))
            all_frames[-(n - j)] = {
                'energy': blended_e,
                'period': blended_p,
                'k': blended_k,
            }
        all_frames.extend(frames[n:])

    return encode_frames(all_frames) if all_frames else b''


def synthesize(word: str) -> bytes:
    """Convert a single word to TI allophone LPC bytes via ARPABET pipeline."""
    if not word:
        return b''
    if not ALLOPHONES:
        return b''

    phonemes = to_arpabet(word)
    clips: list[bytes] = []
    for p in phonemes:
        base = _strip_stress(p)
        ti_name = ARPABET_TO_TI.get(base)
        if ti_name and ti_name in ALLOPHONES:
            clips.append(ALLOPHONES[ti_name])

    return _crossfade(clips, blend=1) if clips else b''
