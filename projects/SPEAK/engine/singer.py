"""
MIDI-addressable formant singer.

Generates clean held pitches by driving a formant filter bank with a
band-limited sawtooth at the target note frequency. This is a classic
source-filter singing-synthesis approach — no pitch-shifting artefacts,
no mid-note wobble, pitch is exact.

Public API:
    sing(notes) → (samples: float32, sr: int)

where `notes` is a list of (midi_note, vowel, duration_ms) tuples.
Vowel is one of VOWELS.keys(): AH, EH, IH, OH, UH, EE, OO, AE, ER.

Also provides:
    scale_degrees_to_midi(scale, root_midi) → [midi_notes]
    major_scale_udu(root_midi=60) → up-down C-major octave
"""
from __future__ import annotations
import numpy as np
from scipy.signal import butter, lfilter

from engine.lpc import CHIRP

SR = 22050

# Real TMS5100 chirp waveform, int8 → float32 in [-1, 1]
_CHIRP_WF = np.array(
    [(c - 256) if c > 127 else c for c in CHIRP], dtype=np.float32
) / 128.0

# Male-voice formant centers (Hz) and bandwidths for common English vowels.
# Based on Peterson & Barney averages + Klatt defaults.
VOWELS: dict[str, list[tuple[float, float]]] = {
    # vowel : [(F1, BW1), (F2, BW2), (F3, BW3)]
    "AH": [(730, 90), (1090, 110), (2440, 170)],   # father
    "EH": [(530, 80), (1840, 100), (2480, 160)],   # bed
    "IH": [(390, 70), (1990, 110), (2550, 170)],   # bit
    "OH": [(570, 80), (840,  90), (2410, 160)],    # boat
    "UH": [(440, 70), (1020, 90), (2240, 150)],    # book
    "EE": [(270, 60), (2290, 100), (3010, 170)],   # beet
    "OO": [(300, 70), (870,  90), (2240, 150)],    # boot
    "AE": [(660, 90), (1720, 110), (2410, 170)],   # cat
    "ER": [(490, 80), (1350, 110), (1690, 160)],   # bird
}

# Default solfege → vowel mapping
SOLFEGE_VOWEL = {
    "DO": "OO", "RE": "EH", "MI": "EE", "FA": "AH",
    "SO": "OH", "SOL": "OH", "LA": "AH", "TI": "EE",
}


# ── Helpers ────────────────────────────────────────────────────────────────

def midi_to_hz(m: float) -> float:
    return 440.0 * 2.0 ** ((m - 69) / 12.0)


def _bandpass(x: np.ndarray, sr: int, f: float, bw: float,
              order: int = 2) -> np.ndarray:
    ny = sr * 0.5
    lo = max(20.0, f - bw * 0.5) / ny
    hi = min(ny - 1, f + bw * 0.5) / ny
    b, a = butter(order, [lo, hi], btype="band")
    return lfilter(b, a, x).astype(np.float32)


def _sawtooth(freq: float, dur_s: float, sr: int = SR) -> np.ndarray:
    """Band-limited sawtooth-ish via phase-wrapped ramp."""
    n = int(dur_s * sr)
    t = np.arange(n, dtype=np.float64)
    phase = (t * freq / sr) % 1.0
    # simple sawtooth, DC removed
    saw = 2.0 * phase - 1.0
    return saw.astype(np.float32)


def _adsr(n: int, sr: int, a_ms: float = 15.0, d_ms: float = 20.0,
          sustain: float = 0.85, r_ms: float = 40.0) -> np.ndarray:
    a = max(1, int(sr * a_ms / 1000))
    d = max(1, int(sr * d_ms / 1000))
    r = max(1, int(sr * r_ms / 1000))
    s = max(0, n - a - d - r)
    env = np.concatenate([
        np.linspace(0.0, 1.0, a, dtype=np.float32),
        np.linspace(1.0, sustain, d, dtype=np.float32),
        np.full(s, sustain, dtype=np.float32),
        np.linspace(sustain, 0.0, r, dtype=np.float32),
    ])
    if len(env) < n:
        env = np.pad(env, (0, n - len(env)))
    return env[:n]


def _vibrato(n: int, sr: int, rate_hz: float = 5.5,
             depth_cents: float = 25.0) -> np.ndarray:
    """Gentle vibrato as a frequency-multiplier signal (1.0 ± depth)."""
    t = np.arange(n, dtype=np.float64) / sr
    depth = 2.0 ** (depth_cents / 1200.0) - 1.0
    return (1.0 + depth * np.sin(2 * np.pi * rate_hz * t)).astype(np.float32)


# ── Core single-note synthesis ────────────────────────────────────────────

def _stamp_chirp(x: np.ndarray, sr: int, hz: float,
                 gain: float = 0.55) -> np.ndarray:
    """Stamp real TMS5100 chirp pulses across x at frequency hz."""
    if hz <= 0 or len(x) < 64:
        return x
    period = max(8, int(sr / hz))
    ch = _CHIRP_WF
    if sr != 8000:
        # resample chirp to target sr (chirp was sampled at 8 kHz)
        from scipy.signal import resample_poly
        from math import gcd
        g = gcd(8000, sr)
        ch = resample_poly(ch, sr // g, 8000 // g).astype(np.float32)
    n = len(x)
    out = x.copy()
    env = np.abs(x)
    # smooth envelope
    w = max(1, period // 2)
    env = np.convolve(env, np.ones(w, dtype=np.float32) / w, mode="same")
    thr = 0.05 * float(np.max(env) or 1.0)
    i = 0
    ch_n = len(ch)
    while i + ch_n < n:
        if env[i] > thr:
            out[i:i + ch_n] += ch * gain * env[i]
            i += period
        else:
            i += period // 2
    return out


def render_note(midi_note: float, vowel: str, dur_ms: float,
                sr: int = SR, vibrato: bool = True,
                ti_chirp: float = 0.0) -> np.ndarray:
    """Render one sung note. Returns float32 mono."""
    vowel = vowel.upper()
    if vowel not in VOWELS:
        vowel = "AH"
    hz = midi_to_hz(midi_note)
    n = int(sr * dur_ms / 1000)
    if n <= 0:
        return np.zeros(0, dtype=np.float32)

    # build source: sawtooth with slight vibrato
    t = np.arange(n, dtype=np.float64)
    if vibrato:
        vib = _vibrato(n, sr)
        # integrate frequency to get phase
        inst_hz = hz * vib
        phase = np.cumsum(inst_hz / sr)
    else:
        phase = t * hz / sr
    phase = phase % 1.0
    source = (2.0 * phase - 1.0).astype(np.float32)

    # formant filtering
    formants = VOWELS[vowel]
    out = np.zeros(n, dtype=np.float32)
    weights = [1.0, 0.7, 0.4]
    for (f, bw), w in zip(formants, weights):
        out += _bandpass(source, sr, f, bw) * w

    # gentle lowshelf-ish rolloff above 4 kHz by simple 1st-order LPF
    from scipy.signal import butter as _bt
    cutoff = min(4000.0, sr * 0.5 - 100.0)
    b, a = _bt(2, cutoff / (sr * 0.5), btype="low")
    out = lfilter(b, a, out).astype(np.float32)

    # envelope
    env = _adsr(n, sr)
    out = out * env

    # layer OG TMS5100 chirp transients at the note's pitch
    if ti_chirp > 0:
        out = _stamp_chirp(out, sr, hz, gain=float(ti_chirp))

    # normalise
    m = float(np.max(np.abs(out)) or 1.0)
    return (out / m * 0.8).astype(np.float32)


# ── Public API ────────────────────────────────────────────────────────────

def sing(notes: list[tuple[float, str, float]],
         sr: int = SR,
         gap_ms: float = 20.0,
         vibrato: bool = True) -> tuple[np.ndarray, int]:
    """
    Render a sequence of (midi_note, vowel, duration_ms) tuples.

    Example:
        sing([(60,'OO',500), (62,'EH',500), (64,'EE',500), (65,'AH',500)])
    """
    if not notes:
        return np.zeros(0, dtype=np.float32), sr
    gap = np.zeros(int(sr * gap_ms / 1000), dtype=np.float32)
    parts = []
    for i, (midi, vowel, dur_ms) in enumerate(notes):
        if i:
            parts.append(gap)
        parts.append(render_note(midi, vowel, dur_ms, sr=sr, vibrato=vibrato))
    return np.concatenate(parts).astype(np.float32), sr


# ── Convenience: solfege & scales ─────────────────────────────────────────

_MAJOR_INTERVALS = [0, 2, 4, 5, 7, 9, 11, 12]   # C D E F G A B C
_SOLFEGE_NAMES   = ["DO", "RE", "MI", "FA", "SO", "LA", "TI", "DO"]


def do_re_mi(root_midi: int = 60, note_ms: float = 600.0,
             vibrato: bool = True) -> tuple[np.ndarray, int]:
    """Sing 'DO RE MI FA SO LA TI DO' on the major scale rooted at `root_midi`."""
    notes = [
        (root_midi + iv, SOLFEGE_VOWEL[name], note_ms)
        for iv, name in zip(_MAJOR_INTERVALS, _SOLFEGE_NAMES)
    ]
    return sing(notes, vibrato=vibrato)


def major_scale_udu(root_midi: int = 60, note_ms: float = 450.0,
                    vowel: str = "AH") -> tuple[np.ndarray, int]:
    """Major scale up then down on a single vowel."""
    up   = [(root_midi + iv, vowel, note_ms) for iv in _MAJOR_INTERVALS]
    down = [(root_midi + iv, vowel, note_ms) for iv in reversed(_MAJOR_INTERVALS[:-1])]
    return sing(up + down)


# ── Syllable → vowel mapping ──────────────────────────────────────────────

def _guess_vowel(syllable: str) -> str:
    """Pick the dominant vowel formant for a written English syllable."""
    s = syllable.upper()
    # ordered, most-specific first
    if "EE" in s or "EA" in s or s.endswith("Y"):       return "EE"
    if "OO" in s or s.endswith("OU") or "OU" in s:      return "OO"
    if "AI" in s or "AY" in s:                          return "AE"
    if "OA" in s or s.endswith("O") or "OW" in s:       return "OH"
    if "ER" in s or "IR" in s or "UR" in s:             return "ER"
    if "AR" in s or "AH" in s or "A" in s and "L" not in s: return "AH"
    if "I" in s:                                         return "IH"
    if "E" in s:                                         return "EH"
    if "O" in s:                                         return "OH"
    if "U" in s:                                         return "UH"
    if "A" in s:                                         return "AE"
    return "AH"


def sing_lyrics(score: list[tuple[str, int, float]],
                sr: int = SR,
                vibrato: bool = True,
                gap_ms: float = 20.0) -> tuple[np.ndarray, int]:
    """
    Render (syllable, midi, dur_ms) triples. Vowel is chosen automatically
    from the syllable's letters.
    """
    notes = [(m, _guess_vowel(s), d) for (s, m, d) in score]
    return sing(notes, sr=sr, vibrato=vibrato, gap_ms=gap_ms)


# ── Daisy Bell (chorus) ───────────────────────────────────────────────────
# Traditional key of G major. Syllables + midi + ms.

DAISY_BELL_CHORUS: list[tuple[str, int, float]] = [
    # "Dai-sy,  Dai-sy,"
    ("DAI", 74, 500), ("SY", 71, 400),
    ("DAI", 67, 500), ("SY", 64, 800),
    # "give me your an-swer do,"
    ("GIVE", 64, 300), ("ME", 67, 300),
    ("YOUR", 67, 300),
    ("AN", 64, 300), ("SWER", 67, 400),
    ("DO",  62, 900),
    # "I'm half cra-zy,"
    ("IM",  62, 400), ("HALF", 66, 400),
    ("CRA", 67, 400), ("ZY",  69, 700),
    # "all for the love of you."
    ("ALL", 69, 300), ("FOR", 71, 300),
    ("THE", 67, 300),
    ("LOVE",66, 400), ("OF",  64, 400),
    ("YOU", 67, 1000),
    # "It won't be a sty-lish mar-riage,"
    ("IT",  67, 300), ("WONT",69, 300),
    ("BE",  71, 300), ("A",   69, 300),
    ("STY", 67, 400), ("LISH",64, 400),
    ("MAR", 66, 400), ("RIAGE",62,700),
    # "I can't af-ford a car-riage,"
    ("I",   62, 300), ("CANT",66, 300),
    ("AF",  69, 300), ("FORD",66, 300),
    ("A",   64, 300),
    ("CAR", 67, 400), ("RIAGE",62,700),
    # "But you'll look sweet u-pon the seat"
    ("BUT", 62, 300), ("YOULL",66,300),
    ("LOOK",69, 300), ("SWEET",71,400),
    ("U",   69, 300), ("PON", 67, 300),
    ("THE", 66, 300), ("SEAT",64, 600),
    # "Of a bi-cy-cle built for two."
    ("OF",  62, 300), ("A",   66, 300),
    ("BI",  69, 400), ("CY",  67, 400),
    ("CLE", 66, 300),
    ("BUILT",64,300), ("FOR", 62, 300),
    ("TWO", 67, 1400),
]


def daisy_bell(note_ms_scale: float = 1.0,
               vibrato: bool = True) -> tuple[np.ndarray, int]:
    """Render the Daisy Bell chorus. note_ms_scale stretches/shrinks tempo."""
    score = [(s, m, d * note_ms_scale) for (s, m, d) in DAISY_BELL_CHORUS]
    return sing_lyrics(score, vibrato=vibrato)


# ── Hybrid: sing actual words using say + pitch/time shift ────────────────

# Approx MIDI baseline for Fred's speaking pitch (empirical)
_FRED_BASELINE_MIDI = 66    # Fred at [[pmod 0]] [[pbas 70]] (~378 Hz)
_FRED_PBAS          = 70

# Pronunciation hints for syllables `say` mis-pronounces
_SAY_FIX = {
    "DAI":   "dai",     "SY":    "see",
    "IM":    "I'm",     "CRA":   "kray",   "ZY":   "zee",
    "AN":    "ann",     "SWER":  "sir",
    "WONT":  "won't",   "STY":   "sty",    "LISH": "lish",
    "MAR":   "mar",     "RIAGE": "rij",
    "CANT":  "can't",   "AF":    "aff",    "FORD": "ford",
    "CAR":   "car",
    "YOULL": "you'll",  "U":     "you",    "PON":  "pon",
    "CY":    "sy",      "CLE":   "cull",
    "THE":   "thuh",
}


def _syllable_word(text: str) -> str:
    t = text.upper()
    return _SAY_FIX.get(t, text.lower())


def _warp_spectral_envelope(sp: np.ndarray, ratio: float) -> np.ndarray:
    """Shift spectral envelope along the frequency axis.
    ratio < 1 lowers formants (deeper voice), ratio > 1 raises."""
    if abs(ratio - 1.0) < 1e-3:
        return sp
    n_bins = sp.shape[1]
    src_idx = np.arange(n_bins, dtype=np.float64) / ratio
    src_idx = np.clip(src_idx, 0, n_bins - 1)
    lo = np.floor(src_idx).astype(int)
    hi = np.clip(lo + 1, 0, n_bins - 1)
    frac = (src_idx - lo).astype(np.float64)
    warped = sp[:, lo] * (1 - frac) + sp[:, hi] * frac
    return warped


# Fred's natural speaking pitch in MIDI (measured) — adaptive formant
# shift references this as "no-shift anchor"
_FRED_NATURAL_MIDI = 50.0


def _render_syllable_world(text: str, midi: float, dur_ms: float,
                           sr: int = SR,
                           formant_shift_semitones: float | None = None,
                           force_voiced: bool = False,
                           freeze_frame: bool = False) -> np.ndarray:
    """
    Proper vocoder-based singing: extract spectral envelope from say
    (so words are preserved), replace f0 with constant target hz
    (clean held pitch, no decay), resynthesise with pyworld.
    """
    from engine.tts import say_synth
    from scipy.signal import resample_poly
    from math import gcd
    import pyworld as pw

    word = _syllable_word(text)
    # natural pitch modulation keeps the vowel voiced robustly; we
    # overwrite f0 entirely anyway, so prosody of source doesn't matter.
    samples, ssr = say_synth(f"[[pbas 55]] {word}", voice="Fred", rate=150)
    if len(samples) < 128:
        return np.zeros(int(sr * dur_ms / 1000), dtype=np.float32)

    # pyworld expects float64 mono
    x = samples.astype(np.float64)

    # analyse at source sr (22050)
    f0, t = pw.dio(x, ssr, frame_period=5.0)
    f0 = pw.stonemask(x, f0, t, ssr)
    sp = pw.cheaptrick(x, f0, t, ssr)
    ap = pw.d4c(x, f0, t, ssr)

    # formant shift: auto-adapt if not specified.
    # formants follow the pitch shift partially (40%), clamped, so:
    #   - singing low (midi < Fred's natural): formants drop a bit but
    #     stay close to natural → preserves vowel clarity
    #   - singing high (midi > natural): formants rise slightly →
    #     avoids muddy "low formants on high note" sound
    if formant_shift_semitones is None:
        # Zero-shift default: preserve Fred's natural formants exactly
        # regardless of f0 pitch. Vowels stay crisp at any octave because
        # formants are untouched — only the fundamental moves. This is
        # the "true timbre" anchor the caller can override if needed.
        formant_shift_semitones = 0.0
    if abs(formant_shift_semitones) > 0.01:
        ratio = 2.0 ** (formant_shift_semitones / 12.0)
        sp = _warp_spectral_envelope(sp, ratio)

    # time-stretch frames to target duration
    target_samples_src_sr = int(ssr * dur_ms / 1000)
    # number of frames needed for target duration at 5ms hop
    target_frames = max(4, int(dur_ms / 5.0))
    src_frames = sp.shape[0]
    if freeze_frame:
        # pick a single steady middle frame (highest energy, voiced) and
        # replicate it → zero timbre wobble, zero f0 wobble = rock-solid tone
        voiced_mask = f0 > 0
        if voiced_mask.any():
            voiced_idx = np.where(voiced_mask)[0]
            # middle of the voiced span
            pick = voiced_idx[len(voiced_idx) // 2]
        else:
            pick = src_frames // 2
        sp = np.tile(sp[pick:pick + 1], (target_frames, 1))
        ap = np.tile(ap[pick:pick + 1], (target_frames, 1))
        idx_i = np.full(target_frames, pick, dtype=int)
    else:
        idx = np.linspace(0, src_frames - 1, target_frames)
        idx_i = np.clip(idx.astype(int), 0, src_frames - 1)
        sp = sp[idx_i]
        ap = ap[idx_i]

    # constant f0 at target pitch (0 = unvoiced)
    target_hz = float(midi_to_hz(midi))
    orig_f0 = f0[idx_i]
    if force_voiced:
        # every frame voiced at exact target pitch — rock-solid sustain,
        # no pitch wobble from spurious unvoiced frames
        new_f0 = np.full_like(orig_f0, target_hz)
    else:
        new_f0 = np.where(orig_f0 > 0, target_hz, 0.0)

    # synthesise
    y = pw.synthesize(new_f0, sp, ap, ssr, frame_period=5.0)
    y = y.astype(np.float32)

    # resample to target sr
    if ssr != sr:
        g = gcd(ssr, sr)
        y = resample_poly(y, sr // g, ssr // g).astype(np.float32)

    # pad/trim to exact target length
    target_n = int(sr * dur_ms / 1000)
    if len(y) < target_n:
        y = np.pad(y, (0, target_n - len(y)))
    else:
        y = y[:target_n]

    # short attack/release fades to avoid clicks at splice boundaries
    # (skipped in freeze-frame mode — the caller handles seams via crossfade)
    if not freeze_frame:
        n = len(y)
        fade = min(int(sr * 0.012), n // 6)
        if fade > 1:
            y[:fade]  *= np.linspace(0.0, 1.0, fade, dtype=np.float32)
            y[-fade:] *= np.linspace(1.0, 0.0, fade, dtype=np.float32)

    # normalise
    m = float(np.max(np.abs(y)) or 1.0)
    if m > 0:
        y = (y / m * 0.85).astype(np.float32)
    return y


def _render_syllable_hybrid(text: str, midi: float, dur_ms: float,
                            sr: int = SR,
                            attack_ms: float = 90.0,
                            xfade_ms: float = 35.0) -> np.ndarray:
    """
    Consonant attack from say + sustained vowel from formant synth.

    - say provides the leading consonant burst (clear enunciation)
    - formant singer holds the vowel at EXACT midi pitch (no decay)
    - the two are crossfaded → words + rock-solid pitch
    """
    from engine.tts import say_synth
    from scipy.signal import resample_poly
    from math import gcd

    word  = _syllable_word(text)
    vowel = _guess_vowel(text)
    target_n = max(int(sr * dur_ms / 1000), int(sr * 0.12))

    # ── 1. consonant attack ────────────────────────────────────────────
    samples, ssr = say_synth(
        f"[[pmod 0]] [[pbas {_FRED_PBAS}]] {word}",
        voice="Fred", rate=150,
    )
    if len(samples) and ssr != sr:
        g = gcd(ssr, sr)
        samples = resample_poly(samples, sr // g, ssr // g).astype(np.float32)

    # trim silence
    if len(samples):
        env = np.abs(samples)
        thr = 0.02 * float(np.max(env) or 1.0)
        v = env > thr
        if v.any():
            a = int(np.argmax(v))
            samples = samples[a:]
    # only keep the leading attack
    attack_n = int(sr * attack_ms / 1000)
    attack = samples[:attack_n] if len(samples) else np.zeros(0, dtype=np.float32)

    # ── 2. sustained vowel at exact MIDI pitch ─────────────────────────
    sustain_n = target_n
    sustain = render_note(midi, vowel, sustain_n / sr * 1000, sr=sr,
                          vibrato=False, ti_chirp=0.0)
    if len(sustain) > sustain_n:
        sustain = sustain[:sustain_n]
    elif len(sustain) < sustain_n:
        sustain = np.pad(sustain, (0, sustain_n - len(sustain)))

    # ── 3. crossfade attack → sustain ──────────────────────────────────
    out = np.zeros(target_n, dtype=np.float32)
    xfade_n = min(int(sr * xfade_ms / 1000), len(attack), target_n // 3)

    if len(attack) == 0:
        return sustain

    # place attack
    a_n = min(len(attack), target_n)
    out[:a_n] += attack[:a_n]

    # fade attack tail
    if xfade_n > 1 and a_n >= xfade_n:
        fade_out = np.linspace(1.0, 0.0, xfade_n, dtype=np.float32)
        out[a_n - xfade_n:a_n] *= fade_out

    # fade-in sustain from the attack/sustain boundary
    if xfade_n > 1:
        sustain_start = max(0, a_n - xfade_n)
        fade_in = np.concatenate([
            np.linspace(0.0, 1.0, xfade_n, dtype=np.float32),
            np.ones(target_n - sustain_start - xfade_n, dtype=np.float32),
        ])[:target_n - sustain_start]
        out[sustain_start:sustain_start + len(fade_in)] += \
            sustain[sustain_start:sustain_start + len(fade_in)] * fade_in
    else:
        out[a_n:] += sustain[a_n:target_n]

    # short release
    rel = min(int(sr * 0.03), target_n // 6)
    if rel > 1:
        out[-rel:] *= np.linspace(1.0, 0.0, rel, dtype=np.float32)

    # ── 4. short OG TI chirp burst at note onset (transient only) ──────
    ch = _CHIRP_WF
    if sr != 8000:
        from scipy.signal import resample_poly
        from math import gcd
        g = gcd(8000, sr)
        ch = resample_poly(ch, sr // g, 8000 // g).astype(np.float32)
    # 3 chirp pulses at the note's pitch, fading out fast (~80ms burst)
    hz = midi_to_hz(midi)
    period = max(8, int(sr / hz))
    burst_ms = 80.0
    burst_n = int(sr * burst_ms / 1000)
    burst = np.zeros(min(burst_n, target_n), dtype=np.float32)
    t = 0
    while t + len(ch) < len(burst):
        fade = 1.0 - (t / len(burst))           # linear decay
        burst[t:t + len(ch)] += ch * 0.55 * fade
        t += period
    out[:len(burst)] += burst

    m = float(np.max(np.abs(out)) or 1.0)
    if m > 0.0:
        out = (out / m * 0.85).astype(np.float32)
    return out


def sing_words(score: list[tuple[str, int, float]],
               sr: int = SR,
               gap_ms: float = 15.0) -> tuple[np.ndarray, int]:
    """
    Render actual spoken words at exact MIDI pitches.

    score: list of (syllable_text, midi, dur_ms)
    """
    if not score:
        return np.zeros(0, dtype=np.float32), sr
    gap = np.zeros(int(sr * gap_ms / 1000), dtype=np.float32)
    parts = []
    for i, (text, midi, dur) in enumerate(score):
        if i:
            parts.append(gap)
        parts.append(_render_syllable_world(text, midi, dur, sr=sr))
    out = np.concatenate(parts).astype(np.float32)
    m = float(np.max(np.abs(out)) or 1.0)
    if m > 0.95:
        out = out / m * 0.9
    return out, sr


def daisy_bell_words(note_ms_scale: float = 1.0) -> tuple[np.ndarray, int]:
    """Sing Daisy Bell with real pronounced syllables at exact pitches."""
    score = [(s, m, d * note_ms_scale) for (s, m, d) in DAISY_BELL_CHORUS]
    return sing_words(score)


# ── MIDI-driven ───────────────────────────────────────────────────────────

DAISY_BELL_SYLLABLES = [
    "DAI","SY","DAI","SY", "GIVE","ME","YOUR","AN","SWER","DO",
    "IM","HALF","CRA","ZY", "ALL","FOR","THE","LOVE","OF","YOU",
    "IT","WONT","BE","A","STY","LISH","MAR","RIAGE",
    "I","CANT","AF","FORD","A","CAR","RIAGE",
    "BUT","YOULL","LOOK","SWEET","U","PON","THE","SEAT",
    "OF","A","BI","CY","CLE","BUILT","FOR","TWO",
]


def load_midi_melody(path: str, track_idx: int = 0) -> list[tuple[float, float, float]]:
    """Return list of (midi_pitch, dur_ms, start_ms) for the given track."""
    import mido
    m = mido.MidiFile(path)
    tempo = 500000
    for msg in m:
        if msg.type == "set_tempo":
            tempo = msg.tempo
            break

    active: dict[int, float] = {}
    events: list[tuple[float, float, float]] = []
    t_sec = 0.0
    track = m.tracks[track_idx]
    for msg in track:
        t_sec += mido.tick2second(msg.time, m.ticks_per_beat, tempo)
        if msg.type == "set_tempo":
            tempo = msg.tempo
        elif msg.type == "note_on" and msg.velocity > 0:
            active[msg.note] = t_sec
        elif msg.type == "note_off" or (msg.type == "note_on" and msg.velocity == 0):
            if msg.note in active:
                start = active.pop(msg.note)
                events.append((float(msg.note), (t_sec - start) * 1000, start * 1000))
    events.sort(key=lambda x: x[2])
    return events


def sing_midi_with_lyrics(midi_path: str,
                          syllables: list[str],
                          sr: int = SR,
                          transpose: int = 0) -> tuple[np.ndarray, int]:
    """
    Drive the vocoder singer from a MIDI melody.
    transpose: semitones to shift (e.g. -12 = down an octave).
    Extra notes beyond `syllables` get a neutral hum.
    """
    events = load_midi_melody(midi_path)
    score: list[tuple[str, float, float]] = []
    for i, (pitch, dur_ms, _) in enumerate(events):
        syll = syllables[i] if i < len(syllables) else "HM"
        score.append((syll, pitch + transpose, dur_ms))
    return sing_words(score, sr=sr)


def daisy_bell_from_midi(midi_path: str,
                         transpose: int = -12) -> tuple[np.ndarray, int]:
    """Sing Daisy Bell from MIDI. Default = -12 (down one octave)."""
    return sing_midi_with_lyrics(midi_path, DAISY_BELL_SYLLABLES,
                                 transpose=transpose)


# ── Full song render pipeline ─────────────────────────────────────────────

def render_song(midi_path: str,
                syllables: list[str],
                transpose: int = -12,
                ti_amount: float = 1.0,
                voice_gain: float = 0.55,
                texture_gain: float = 0.85) -> tuple[np.ndarray, int]:
    """
    Render any MIDI + lyrics into the locked-in voice: clean pyworld
    vocoder (words + exact pitch) mixed with a tempo-locked TI LPC
    texture layer. Works for any song.

    syllables : one string per MIDI note, in order
    transpose : semitones shift (negative = lower)
    """
    from engine.ticolor import ti_color
    from scipy.signal import resample_poly
    from math import gcd

    song, ssr = sing_midi_with_lyrics(midi_path, syllables, transpose=transpose)
    textured, tsr = ti_color(song, ssr, amount=ti_amount, chirp_gain=0.0)
    if tsr != ssr:
        g = gcd(tsr, ssr)
        textured = resample_poly(textured, ssr // g, tsr // g).astype(np.float32)
    n = len(song)
    if len(textured) < n:
        textured = np.pad(textured, (0, n - len(textured)))
    else:
        textured = textured[:n]

    mix = voice_gain * song + texture_gain * textured

    # gentle lowpass to shave vocoder hash / bitcrush buzz above 6 kHz
    from scipy.signal import butter, lfilter
    b, a = butter(2, 6000.0 / (ssr * 0.5), btype="low")
    mix = lfilter(b, a, mix).astype(np.float32)

    m = float(np.max(np.abs(mix)) or 1.0)
    if m > 0.95:
        mix = mix / m * 0.9
    return mix.astype(np.float32), ssr


def render_spoken_then_song(intro_text: str,
                            midi_path: str,
                            syllables: list[str],
                            transpose: int = -12,
                            gap_ms: float = 500.0) -> tuple[np.ndarray, int]:
    """Speak `intro_text` in mix mode, then sing the MIDI song."""
    from engine.voice import synthesize
    from scipy.signal import resample_poly
    from math import gcd

    song, ssr = render_song(midi_path, syllables, transpose=transpose)
    intro, isr = synthesize(intro_text, mode='mix', ti_amount=0.85)
    if isr != ssr:
        g = gcd(isr, ssr)
        intro = resample_poly(intro, ssr // g, isr // g).astype(np.float32)
    gap = np.zeros(int(ssr * gap_ms / 1000), dtype=np.float32)
    out = np.concatenate([intro, gap, song]).astype(np.float32)
    m = float(np.max(np.abs(out)) or 1.0)
    if m > 0.95:
        out = out / m * 0.9
    return out, ssr
