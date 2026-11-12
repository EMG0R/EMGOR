#!/usr/bin/env python3
"""DEMIURGE probe — analysis of injected-signal capture.

The capture WAV is laid out as:
  channel 0           : inject:out1     (reference, what was emitted)
  channel 1..N-1      : chain output    (post-loopback, post-ALSA, pre-DAC)

Both channels are clocked by the same JACK driver, so the only offset
between them is the chain's processing latency — recovered by
cross-correlating the envelopes. Sample-accurate regardless of when
the capture started.

Reference signal (see inject.csd):
    multi-tone  : 87 / 240 / 661 / 1820 / 5010 Hz, each at -27 dBFS
    impulse train: amplitude 0.5, every 0.25 s, sample-accurate

The multi-tone leaves the spectrum BETWEEN the fundamentals essentially
silent, so any digital artifact (click, crackle, aliasing, buffer-stall
hash) appears in the gaps and stands out cleanly. The 7-18 kHz band is
quiet (above all 5 fundamentals + their main 2nd harmonics) and is
where the HF-click detector listens.
"""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy import signal


IMPULSE_AMP = 0.5
IMPULSE_PERIOD_S = 0.25

# Multi-tone fundamentals (Hz) — must match inject.csd exactly
TONE_FREQS = (87.0, 240.0, 661.0, 1820.0, 5010.0)
TONE_BIN_TOL_HZ = 5.0          # bin half-width around each tone (FFT smearing)

MAX_LATENCY_S = 0.200          # latency search window
ENVELOPE_CUTOFF_HZ = 200.0     # envelope LPF for cross-correlation
HF_BAND = (7000.0, 18000.0)    # click/crackle detection band — above all
                               # 5 fundamentals + their main 2nd harmonics


# ── data plumbing ──────────────────────────────────────────────────────────────

@dataclass
class LatencyResult:
    latency_ms: float | None
    confidence: float
    jitter_ms: float | None
    impulses_detected: int
    impulses_expected: int


@dataclass
class GlitchResult:
    rms_dbfs: float
    hf_clicks:           int      # broadband click events in HF band
    crackle_windows:     int      # 10ms windows where HF/total ratio spikes
    repeated_samples:    int      # consecutive identical samples (buffer stall)
    sample_discontinuities: int   # |Δx| > median + 3·MAD, impulse times masked
    dropout_windows:     int      # sliding RMS far below median
    spike_windows:       int      # sliding RMS far above median
    tone_purity_db:      float    # energy at the 5 known tone bins vs total
                                  # spectral energy, in dB. Higher = cleaner.
                                  # Healthy chain ≥ -10 dB; <-20 dB = garbage


def db(x: float) -> float:
    if x <= 0:
        return -np.inf
    return float(20.0 * np.log10(x))


def envelope(x: np.ndarray, sr: int, cutoff: float = ENVELOPE_CUTOFF_HZ) -> np.ndarray:
    sos = signal.butter(2, cutoff, fs=sr, output="sos")
    return signal.sosfiltfilt(sos, np.abs(x))


def bandpass(x: np.ndarray, sr: int, lo: float, hi: float) -> np.ndarray:
    sos = signal.butter(4, [lo, hi], btype="band", fs=sr, output="sos")
    return signal.sosfiltfilt(sos, x)


# ── latency ────────────────────────────────────────────────────────────────────

def measure_latency(ref: np.ndarray, out: np.ndarray, sr: int) -> LatencyResult:
    n = min(len(ref), len(out))
    if n < sr // 4:
        return LatencyResult(None, 0.0, None, 0, 0)

    env_ref = envelope(ref[:n], sr)
    env_out = envelope(out[:n], sr)

    max_lag = int(MAX_LATENCY_S * sr)
    seg = env_ref[: n - max_lag]
    target = env_out[: n]
    if seg.size < 1024 or target.size < seg.size + max_lag:
        return LatencyResult(None, 0.0, None, 0, 0)

    xcorr = signal.correlate(target, seg, mode="valid")
    denom = np.linalg.norm(seg) * np.sqrt(
        np.convolve(target ** 2, np.ones(seg.size), mode="valid")
    )
    denom = np.where(denom > 0, denom, 1e-30)
    norm = xcorr / denom
    best_lag = int(np.argmax(norm))
    confidence = float(np.clip(norm[best_lag], 0.0, 1.0))
    latency_ms = 1000.0 * best_lag / sr

    period_samples = int(round(IMPULSE_PERIOD_S * sr))
    duration_s = n / sr
    impulses_expected = int(np.floor(duration_s / IMPULSE_PERIOD_S))

    thresh = max(0.05, 4.0 * float(np.median(env_out)))
    peaks, _ = signal.find_peaks(
        env_out, height=thresh, distance=int(0.8 * period_samples)
    )
    jitter_ms: float | None
    if peaks.size >= 2:
        intervals_ms = np.diff(peaks).astype(np.float64) / sr * 1000.0
        jitter_ms = float(np.std(intervals_ms))
    else:
        jitter_ms = None

    return LatencyResult(
        latency_ms=latency_ms,
        confidence=confidence,
        jitter_ms=jitter_ms,
        impulses_detected=int(peaks.size),
        impulses_expected=impulses_expected,
    )


# ── glitch detectors ───────────────────────────────────────────────────────────

def hf_click_count(out: np.ndarray, sr: int, peaks: np.ndarray) -> int:
    """Bandpass 5-15 kHz, look for peaks well above the HF floor.

    Pink noise has its lowest energy in this band, so any digital click
    (which is broadband) shows up as a clear spike here. We mask out the
    injected impulse arrivals so we don't count our own clicks.
    """
    if out.size < sr // 4:
        return 0
    hf = bandpass(out, sr, *HF_BAND)
    env = envelope(hf, sr, cutoff=500.0)
    if env.size < 64:
        return 0
    # Mask impulse arrivals ±10 ms (the impulse propagates through FX as
    # a click; that's not the glitch we're chasing)
    mask = np.ones(env.size, dtype=bool)
    guard = int(0.010 * sr)
    for t in peaks:
        lo = max(int(t) - guard, 0)
        hi = min(int(t) + guard, env.size)
        mask[lo:hi] = False
    masked = env[mask]
    if masked.size < 64:
        return 0
    med = float(np.median(masked))
    mad = float(np.median(np.abs(masked - med)))
    if mad <= 0:
        return 0
    threshold = med + 8.0 * mad      # high bar — only count strong clicks
    # peaks separated by ≥ 1 ms to avoid double-counting ringing
    click_peaks, _ = signal.find_peaks(env * mask, height=threshold,
                                       distance=max(int(0.001 * sr), 1))
    return int(click_peaks.size)


def crackle_window_count(out: np.ndarray, sr: int) -> int:
    """Count 10 ms windows where the HF-to-total energy ratio is far above
    the median. Pink noise has a stable, low ratio; crackle is dense
    HF activity that spikes that ratio."""
    hf = bandpass(out, sr, *HF_BAND)
    win = max(int(0.010 * sr), 32)
    n = len(out) // win
    if n < 4:
        return 0
    out_f = out[: n * win].reshape(n, win)
    hf_f  = hf [: n * win].reshape(n, win)
    out_pow = np.mean(out_f ** 2, axis=1) + 1e-30
    hf_pow  = np.mean(hf_f  ** 2, axis=1)
    ratio = hf_pow / out_pow
    med = float(np.median(ratio))
    mad = float(np.median(np.abs(ratio - med)))
    if mad <= 0:
        return 0
    threshold = med + 6.0 * mad
    return int(np.sum(ratio > threshold))


def repeated_sample_runs(out: np.ndarray, min_run: int = 8) -> int:
    """Count runs of `min_run` or more consecutive identical samples.

    A healthy DSP output is essentially never exactly repeated for that
    long. Buffer-stuck audio (the JACK callback re-emitted the previous
    period because csound was late) shows up as exact-zero or
    exact-last-value runs."""
    if out.size < min_run + 1:
        return 0
    diff = np.diff(out)
    is_same = diff == 0
    # find runs of True in is_same of length >= min_run-1
    runs = 0
    i = 0
    n = is_same.size
    while i < n:
        if is_same[i]:
            j = i
            while j < n and is_same[j]:
                j += 1
            if j - i + 1 >= min_run:
                runs += 1
            i = j
        else:
            i += 1
    return runs


def sample_discontinuities(out: np.ndarray, sr: int, peaks: np.ndarray,
                           mad_factor: float = 3.0) -> int:
    """Single-sample jumps above noise floor, with our own impulses masked.

    Tightened from 6·MAD to 3·MAD. False-positive rate is higher but the
    user's symptom is consistent clicks on FX-engage that the 6·MAD
    threshold missed entirely."""
    diff = np.abs(np.diff(out))
    if diff.size == 0:
        return 0
    mask = np.ones(diff.size, dtype=bool)
    guard = int(0.005 * sr)
    for t in peaks:
        lo = max(int(t) - guard, 0)
        hi = min(int(t) + guard, diff.size)
        mask[lo:hi] = False
    masked = diff[mask]
    if masked.size < 64:
        return 0
    med = float(np.median(masked))
    mad = float(np.median(np.abs(masked - med)))
    if mad <= 0:
        return 0
    threshold = med + mad_factor * mad
    return int(np.sum(masked > threshold))


def sliding_rms_anomalies(out: np.ndarray, sr: int) -> tuple[int, int]:
    win = max(int(0.010 * sr), 32)
    n = len(out) // win
    if n < 4:
        return 0, 0
    framed = out[: n * win].reshape(n, win)
    rms = np.sqrt(np.mean(framed ** 2, axis=1))
    median = float(np.median(rms))
    if median <= 0:
        return 0, 0
    dropouts = int(np.sum(rms < 0.5 * median))
    spikes   = int(np.sum(rms > 2.0 * median))
    return dropouts, spikes


def tone_purity_db(out: np.ndarray, sr: int) -> float:
    """Energy in the 5 known tone bins (±TONE_BIN_TOL_HZ each) vs total
    spectral energy, in dB. Higher = cleaner chain.

    Healthy passive chain (no FX coloring): ≥ 0 dB (most energy is in
    the tones; little elsewhere because we put nothing else in).
    With heavy FX (reverb tails, delay smears, pitch shifters): typical
    -5 to -15 dB — FX legitimately spread energy off the fundamentals.
    < -25 dB starts to indicate that the chain isn't passing the tones
    intact — they're being shredded by glitches, dropouts, or aliasing
    that turns the clean spectrum into noise."""
    win_len = min(sr * 2, len(out))
    if win_len < 4096:
        return float("nan")
    start = (len(out) - win_len) // 2
    seg = out[start : start + win_len].copy()
    seg *= np.hanning(seg.size)
    spec = np.fft.rfft(seg)
    pow_ = np.abs(spec) ** 2
    freqs = np.fft.rfftfreq(seg.size, d=1.0 / sr)
    total_pow = float(np.sum(pow_))
    if total_pow <= 0:
        return float("nan")
    tone_pow = 0.0
    for f0 in TONE_FREQS:
        mask = (freqs >= f0 - TONE_BIN_TOL_HZ) & (freqs <= f0 + TONE_BIN_TOL_HZ)
        tone_pow += float(np.sum(pow_[mask]))
    if tone_pow <= 0:
        return float("nan")
    non_tone = max(total_pow - tone_pow, 1e-30)
    return db(np.sqrt(tone_pow) / np.sqrt(non_tone))


def measure_glitches(out: np.ndarray, sr: int) -> GlitchResult:
    rms = float(np.sqrt(np.mean(out ** 2)))
    rms_dbfs = db(rms)

    env_out = envelope(out, sr)
    period_samples = int(round(IMPULSE_PERIOD_S * sr))
    thresh = max(0.05, 4.0 * float(np.median(env_out)))
    peaks, _ = signal.find_peaks(env_out, height=thresh,
                                  distance=int(0.8 * period_samples))

    dropouts, spikes = sliding_rms_anomalies(out, sr)
    hf_clicks   = hf_click_count(out, sr, peaks)
    crackles    = crackle_window_count(out, sr)
    rep_runs    = repeated_sample_runs(out)
    discont     = sample_discontinuities(out, sr, peaks, mad_factor=3.0)
    purity_db   = tone_purity_db(out, sr)

    return GlitchResult(
        rms_dbfs=rms_dbfs,
        hf_clicks=hf_clicks,
        crackle_windows=crackles,
        repeated_samples=rep_runs,
        sample_discontinuities=discont,
        dropout_windows=dropouts,
        spike_windows=spikes,
        tone_purity_db=purity_db,
    )


# ── rendering ──────────────────────────────────────────────────────────────────

def render_report(
    capture: Path,
    sr: int,
    duration_s: float,
    n_channels: int,
    latency: LatencyResult,
    glitch: GlitchResult,
) -> str:
    lat = f"{latency.latency_ms:7.3f} ms" if latency.latency_ms is not None else "  n/a   "
    jit = f"{latency.jitter_ms:7.3f} ms" if latency.jitter_ms is not None else "  n/a   "
    conf = f"{latency.confidence:.3f}"

    def health(x: int, warn: int, bad: int) -> str:
        if x >= bad:  return "  BAD"
        if x >= warn: return " WARN"
        return "   ok"

    # Tone purity health: ≥0 dB is great, -10 dB is OK with heavy FX,
    # -25 dB suggests chain shredding the tones.
    def purity_health(x: float) -> str:
        if np.isnan(x):     return "    n/a"
        if x <= -25:        return "    BAD"
        if x <= -10:        return "   WARN"
        return "     ok"

    return (
        f"capture        : {capture}\n"
        f"duration       : {duration_s:.2f} s  @ {sr} Hz, {n_channels} ch (ch0 ref, rest chain)\n"
        f"chain RMS      : {glitch.rms_dbfs:7.2f} dBFS\n"
        f"\n"
        f"-- LATENCY (csound input → Scarlett :monitor, software only) --\n"
        f"  round-trip   : {lat}    (confidence {conf})\n"
        f"  jitter (σ)   : {jit}\n"
        f"  impulses     : {latency.impulses_detected}/{latency.impulses_expected} detected\n"
        f"\n"
        f"-- SPECTRAL --\n"
        f"  tone purity  (energy in 5 fundamentals vs everywhere else)\n"
        f"               : {glitch.tone_purity_db:7.2f} dB  {purity_health(glitch.tone_purity_db)}\n"
        f"\n"
        f"-- GLITCH FINGERPRINT --\n"
        f"  HF clicks    (7-18 kHz envelope peaks > med+8·MAD)     : {glitch.hf_clicks:5d}  {health(glitch.hf_clicks, 1, 5)}\n"
        f"  crackle win  (10ms HF/total ratio spikes > med+6·MAD)  : {glitch.crackle_windows:5d}  {health(glitch.crackle_windows, 1, 5)}\n"
        f"  repeated-sample runs (≥8 consecutive identical)        : {glitch.repeated_samples:5d}  {health(glitch.repeated_samples, 1, 5)}\n"
        f"  sample discontinuities (|Δ| > med + 3·MAD)             : {glitch.sample_discontinuities:5d}  {health(glitch.sample_discontinuities, 25, 200)}\n"
        f"  RMS dropouts (>6 dB below median, 10 ms win)           : {glitch.dropout_windows:5d}  {health(glitch.dropout_windows, 1, 5)}\n"
        f"  RMS spikes   (>6 dB above median, 10 ms win)           : {glitch.spike_windows:5d}  {health(glitch.spike_windows, 1, 5)}\n"
    )


# ── main ───────────────────────────────────────────────────────────────────────

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--capture", required=True, type=Path)
    ap.add_argument("--report",  required=True, type=Path)
    args = ap.parse_args()

    data, sr = sf.read(str(args.capture), always_2d=True)
    data = data.astype(np.float64)
    n, ch = data.shape
    duration_s = n / sr

    if ch < 2:
        print(f"[analyze] capture has only {ch} channel(s); need ≥2.", file=sys.stderr)
        return 1

    ref = data[:, 0]
    out = data[:, 1:].mean(axis=1)

    if float(np.sqrt(np.mean(out ** 2))) < 1e-6:
        print("[analyze] chain output is essentially silent. wiring problem?",
              file=sys.stderr)

    latency = measure_latency(ref, out, sr)
    glitch  = measure_glitches(out, sr)

    text = render_report(args.capture, sr, duration_s, ch, latency, glitch)
    args.report.write_text(text)
    print(text, end="")
    return 0


if __name__ == "__main__":
    sys.exit(main())
