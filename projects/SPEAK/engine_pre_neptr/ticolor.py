"""
TI TMS5100-flavor post-processor.

Takes clean modern TTS audio and imprints it with the Speak & Spell
character:
  - decimate to 8 kHz (native TMS5100 rate)
  - bandpass ~200 Hz .. 3.6 kHz (LPC-10 telephony-ish band)
  - bit-crush to ~8-bit quantisation (DAC grit)
  - short-frame LPC re-analysis + resynthesis with coarse quantised
    reflection coefficients (the real source of TI "buzz")
  - soft clip to get that saturated speaker edge
"""
from __future__ import annotations
import numpy as np
from scipy.signal import resample_poly, butter, lfilter, lfilter_zi

from engine.lpc import CHIRP

TI_SR = 8000

# Real TMS5100 chirp waveform, normalised to [-1, 1], int8 → float32
_CHIRP_WF = np.array(
    [(c - 256) if c > 127 else c for c in CHIRP], dtype=np.float32
) / 128.0


# ── Helpers ────────────────────────────────────────────────────────────────

def _bandpass(x: np.ndarray, sr: int, lo: float = 200.0,
              hi: float = 3600.0, order: int = 4) -> np.ndarray:
    ny = sr * 0.5
    b, a = butter(order, [lo / ny, min(hi, ny - 1) / ny], btype="band")
    return lfilter(b, a, x).astype(np.float32)


def _bitcrush(x: np.ndarray, bits: int = 8) -> np.ndarray:
    levels = (1 << (bits - 1)) - 1
    return (np.round(x * levels) / levels).astype(np.float32)


def _soft_clip(x: np.ndarray, drive: float = 1.4) -> np.ndarray:
    return np.tanh(x * drive).astype(np.float32)


# ── LPC analysis/resynthesis (autocorrelation + Levinson) ─────────────────

def _autocorr(frame: np.ndarray, order: int) -> np.ndarray:
    r = np.correlate(frame, frame, mode="full")
    mid = len(frame) - 1
    return r[mid:mid + order + 1]


def _levinson(r: np.ndarray, order: int) -> tuple[np.ndarray, np.ndarray]:
    """Return (a: lpc coeffs incl leading 1, k: reflection coeffs)."""
    a = np.zeros(order + 1, dtype=np.float64)
    a[0] = 1.0
    k = np.zeros(order, dtype=np.float64)
    e = r[0] if r[0] > 1e-10 else 1e-10
    for i in range(1, order + 1):
        acc = r[i] + np.dot(a[1:i], r[i - 1:0:-1]) if i > 1 else r[i]
        ki = -acc / e
        k[i - 1] = ki
        new_a = a.copy()
        new_a[1:i] = a[1:i] + ki * a[i - 1:0:-1]
        new_a[i] = ki
        a = new_a
        e = e * (1.0 - ki * ki)
        if e <= 1e-12:
            break
    return a, k


def _quantise_k(k: np.ndarray, bits: int = 4) -> np.ndarray:
    """Quantise reflection coeffs (in (-1, 1)) to `bits` bits — the
    dominant source of TI voice-banding artefacts."""
    k = np.clip(k, -0.999, 0.999)
    levels = (1 << (bits - 1)) - 1
    return np.round(k * levels) / levels


def _k_to_a(k: np.ndarray) -> np.ndarray:
    order = len(k)
    a = np.zeros(order + 1)
    a[0] = 1.0
    for i in range(1, order + 1):
        new_a = a.copy()
        new_a[1:i] = a[1:i] + k[i - 1] * a[i - 1:0:-1]
        new_a[i] = k[i - 1]
        a = new_a
    return a


def _lpc_recolor(x: np.ndarray, sr: int,
                 order: int = 10,
                 frame_ms: float = 25.0,
                 hop_ms: float = 10.0,
                 k_bits: int = 4) -> np.ndarray:
    """Frame-wise LPC analysis → quantise reflection coeffs → resynthesise
    with the ORIGINAL residual (preserves intelligibility but colours the
    formants with TI-style quantisation bands)."""
    n = len(x)
    if n < int(sr * frame_ms / 1000):
        return x
    frame = int(sr * frame_ms / 1000)
    hop   = int(sr * hop_ms   / 1000)
    win   = np.hanning(frame).astype(np.float64)

    out = np.zeros(n, dtype=np.float64)
    norm = np.zeros(n, dtype=np.float64)

    for start in range(0, n - frame, hop):
        seg = x[start:start + frame].astype(np.float64) * win
        if np.max(np.abs(seg)) < 1e-5:
            continue
        r = _autocorr(seg, order)
        a, k = _levinson(r, order)
        kq = _quantise_k(k, k_bits)
        aq = _k_to_a(kq)
        # residual via original a, then resynth via quantised aq
        resid = lfilter(a, [1.0], seg)
        try:
            recolored = lfilter([1.0], aq, resid)
        except ValueError:
            recolored = seg
        out[start:start + frame]  += recolored * win
        norm[start:start + frame] += win * win

    norm[norm < 1e-6] = 1.0
    return (out / norm).astype(np.float32)


# ── Chirp transient overlay ────────────────────────────────────────────────

# C major scale — semitone offsets, full ascending then descending octave
_C_MAJOR_DEGREES = [0, 2, 4, 5, 7, 9, 11, 12, 12, 11, 9, 7, 5, 4, 2, 0]

# C major scale frequencies (C3..C5) — still used for chirp-period mode
_C_MAJOR_HZ = [130.81, 146.83, 164.81, 174.61, 196.00, 220.00, 246.94,
               261.63, 293.66, 329.63, 349.23, 392.00, 440.00, 493.88,
               523.25]


def _vowel_regions(x: np.ndarray, sr: int,
                   min_ms: float = 60.0) -> list[tuple[int, int]]:
    """Segment audio into voiced/vowel regions via energy gating."""
    n = len(x)
    frame = max(1, int(sr * 0.010))     # 10 ms frames
    energy = np.array([
        float(np.sqrt(np.mean(x[i:i + frame] ** 2)))
        for i in range(0, n - frame, frame)
    ])
    if energy.size == 0:
        return []
    thr = 0.18 * float(np.max(energy))
    voiced = energy > thr

    regions: list[tuple[int, int]] = []
    i = 0
    while i < len(voiced):
        if voiced[i]:
            j = i
            while j < len(voiced) and voiced[j]:
                j += 1
            start = i * frame
            end   = min(n, j * frame)
            if end - start >= int(sr * min_ms / 1000):
                regions.append((start, end))
            i = j
        else:
            i += 1
    return regions


def _pitch_shift_region(x: np.ndarray, sr: int, semitones: float) -> np.ndarray:
    """Pitch-shift a chunk by `semitones`, preserving length."""
    if abs(semitones) < 0.01 or len(x) < 64:
        return x
    import librosa
    # at 8 kHz short segments, default n_fft=2048 is too large
    n_fft = 512 if len(x) >= 512 else 256
    y = librosa.effects.pitch_shift(y=x.astype(np.float32),
                                    sr=sr, n_steps=semitones,
                                    n_fft=n_fft)
    if len(y) > len(x):
        y = y[:len(x)]
    elif len(y) < len(x):
        y = np.pad(y, (0, len(x) - len(y)))
    return y.astype(np.float32)


def _subdivide(regions: list[tuple[int, int]], sr: int,
               target_ms: float = 180.0) -> list[tuple[int, int]]:
    """Chop long regions into ~target_ms chunks so each gets its own note."""
    step = max(1, int(sr * target_ms / 1000))
    out: list[tuple[int, int]] = []
    for a, b in regions:
        if b - a <= step:
            out.append((a, b))
        else:
            i = a
            while i < b:
                j = min(b, i + step)
                out.append((i, j))
                i = j
    return out


def _sing_pitch_shift(x: np.ndarray, sr: int,
                      scale: list[int] | None = None) -> np.ndarray:
    """Oscillate pitch across vowels following a major-scale sequence."""
    scale = scale or _C_MAJOR_DEGREES
    regions = _subdivide(_vowel_regions(x, sr), sr, target_ms=260.0)
    if not regions:
        return x
    out = x.copy()
    for idx, (a, b) in enumerate(regions):
        semis = float(scale[idx % len(scale)])
        out[a:b] = _pitch_shift_region(x[a:b], sr, semis)
    return out


def _chirp_overlay(x: np.ndarray, period_samples: int = 80,
                   gain: float = 0.35,
                   sing: bool = False,
                   scale_hz: list[float] | None = None) -> np.ndarray:
    """Stamp the real TMS5100 chirp waveform on voiced regions.

    period_samples : base chirp period (8000/period → pitch)
    sing           : if True, chirp period walks `scale_hz` across the
                     utterance — makes the voice "sing" the scale.
    """
    if len(x) < period_samples * 2:
        return x
    out = x.copy()
    env = np.abs(x)
    win = max(1, period_samples // 4)
    kernel = np.ones(win, dtype=np.float32) / win
    env = np.convolve(env, kernel, mode="same")
    thr = 0.05 * float(np.max(env) or 1.0)

    scale_hz = scale_hz or _C_MAJOR_HZ
    n = len(out)

    chirp = _CHIRP_WF
    ch_n  = len(chirp)
    i = 0
    while i + ch_n < n:
        if env[i] > thr:
            if sing:
                # walk the scale across the utterance
                step = int(len(scale_hz) * (i / n))
                step = min(step, len(scale_hz) - 1)
                period = max(12, int(8000 / scale_hz[step]))
            else:
                period = period_samples
            out[i:i + ch_n] += chirp * gain * env[i]
            i += period
        else:
            i += period_samples // 2
    return out


# ── Public API ─────────────────────────────────────────────────────────────

def ti_color(samples: np.ndarray, sr: int,
             amount: float = 1.0,
             sing: bool = False,
             chirp_gain: float | None = None) -> tuple[np.ndarray, int]:
    """
    Imprint TI TMS5100 character on clean audio.

    amount: 0.0 = no coloration, 1.0 = full TI crunch.
    Returns (samples, 8000).
    """
    if samples is None or len(samples) == 0:
        return np.zeros(0, dtype=np.float32), TI_SR

    x = samples.astype(np.float32)
    peak = float(np.max(np.abs(x)) or 1.0)
    x = x / peak * 0.9

    # singing mode: pitch-shift at ORIGINAL rate (higher quality)
    # before decimation, so Fred's voice clearly walks the scale
    if sing:
        x = _sing_pitch_shift(x, sr)

    # decimate to 8 kHz
    if sr != TI_SR:
        from math import gcd
        g = gcd(sr, TI_SR)
        x = resample_poly(x, TI_SR // g, sr // g).astype(np.float32)

    # LPC re-color — 3-bit k-coeffs, chunky TI banding
    colored = _lpc_recolor(x, TI_SR, k_bits=3)

    # cardboard-speaker bandpass
    colored = _bandpass(colored, TI_SR, lo=300.0, hi=3400.0)

    # stamp real TMS5100 chirp transients on voiced regions
    # explicit chirp_gain=0 disables the pitched buzz entirely
    if chirp_gain is None:
        chirp_gain = 0.30 if sing else 0.55
    if chirp_gain > 0:
        colored = _chirp_overlay(colored, period_samples=80, gain=chirp_gain)

    # bit-crush + soft clip
    colored = _bitcrush(colored, bits=6)
    colored = _soft_clip(colored, drive=1.8)

    # mix with dry (resampled) per `amount`
    dry = x if len(x) == len(colored) else np.resize(x, len(colored))
    mix = (amount * colored + (1.0 - amount) * dry).astype(np.float32)

    # final normalise
    m = float(np.max(np.abs(mix)) or 1.0)
    return (mix / m * 0.85).astype(np.float32), TI_SR
