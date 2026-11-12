"""DEMIURGE latency engine — measurement math (no hardware, no I/O).

Pure functions operating on numpy arrays so they are unit-testable against
synthetic ground truth (see test_engine.py — the "testing-ground mode").
The shell orchestrator (latency.sh) handles JACK rewiring and feeds real
captures in here.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np
from scipy import signal


# ── probe signal ────────────────────────────────────────────────────────────

def make_sweep(sr: int, dur: float = 2.0, f0: float = 20.0,
               f1: float = 20000.0, amp: float = 0.5) -> np.ndarray:
    """Exponential sine sweep (ESS) 20 Hz → 20 kHz.

    An ESS deconvolves cleanly to an impulse response, separating linear
    response from harmonic distortion, and gives a sharp matched-filter
    peak for sample-accurate latency.
    """
    n = int(round(dur * sr))
    t = np.arange(n) / sr
    f1 = min(f1, 0.45 * sr)
    k = np.log(f1 / f0)
    phase = 2.0 * np.pi * f0 * dur / k * (np.exp(t / dur * k) - 1.0)
    return (amp * np.sin(phase)).astype(np.float64)


# ── latency ─────────────────────────────────────────────────────────────────

@dataclass
class LatencyResult:
    latency_ms: float | None
    confidence: float


@dataclass
class FreqResponse:
    low_db: float       # 20–300 Hz, relative to reference
    mid_db: float       # 300 Hz–3 kHz
    high_db: float      # 3 kHz–16 kHz


@dataclass
class GlitchResult:
    clicks: int            # broadband single-sample discontinuities
    dropout_windows: int   # 10 ms windows far below median RMS
    noise_floor_db: float  # RMS of the silent lead-in (pre-arrival), dBFS


@dataclass
class ShotResult:
    latency: LatencyResult
    passthrough: bool
    passthrough_reason: str
    gain_db: float
    freq_response: FreqResponse
    glitches: GlitchResult


@dataclass
class LatencyAggregate:
    typical_ms: float      # median across repeated sweeps
    jitter_ms: float       # standard deviation
    worst_ms: float        # max observed = the "guaranteed" bound
    n: int


def measure_latency(ref: np.ndarray, out: np.ndarray, sr: int,
                    max_latency_s: float = 0.2,
                    template_s: float = 1.0) -> LatencyResult:
    """Recover the delay of `out` vs `ref` by a SHORT windowed matched filter.

    A short reference template (default 1 s) is slid over `out` across the
    search range; latency is the peak of the *sliding-normalised* correlation
    and its value is the confidence (0..1). A short template is essential:
    real captures cross independent device clocks, so over many seconds the
    output drifts in time vs the reference and a full-length correlation
    decorrelates to ~0. Over ~1 s the drift is negligible (<0.05 ms at 200 ppm).
    """
    n = min(len(ref), len(out))
    if n < sr // 8:
        return LatencyResult(None, 0.0)
    ref = ref[:n].astype(np.float64)
    out = out[:n].astype(np.float64)

    max_lag = int(max_latency_s * sr)
    # Template starts a little in (skip startup transient) and is bounded.
    t0 = min(int(0.25 * sr), n // 8)
    L = min(int(template_s * sr), n - t0 - max_lag)
    if L < sr // 16:
        return LatencyResult(None, 0.0)

    templ = ref[t0: t0 + L]
    search = out[t0: t0 + L + max_lag]
    if search.size < L + 1:
        return LatencyResult(None, 0.0)

    xcorr = signal.correlate(search, templ, mode="valid")       # max_lag+1 long
    denom = np.linalg.norm(templ) * np.sqrt(
        np.convolve(search ** 2, np.ones(L), mode="valid"))
    denom = np.where(denom > 0, denom, 1e-30)
    ncorr = np.abs(xcorr) / denom

    best_lag = int(np.argmax(ncorr))
    confidence = float(np.clip(ncorr[best_lag], 0.0, 1.0))
    return LatencyResult(latency_ms=1000.0 * best_lag / sr, confidence=confidence)


# ── shot analysis ───────────────────────────────────────────────────────────

def _db(x: float) -> float:
    return float(20.0 * np.log10(x)) if x > 0 else -np.inf


PASSTHROUGH_MIN_RMS = 1e-4     # output below this = nothing got through
PASSTHROUGH_MIN_CONF = 0.3     # correlation below this = not our signal


def _empty_response() -> FreqResponse:
    return FreqResponse(low_db=-np.inf, mid_db=-np.inf, high_db=-np.inf)


def _empty_glitches() -> GlitchResult:
    return GlitchResult(clicks=0, dropout_windows=0, noise_floor_db=-np.inf)


def analyze_shot(ref: np.ndarray, out: np.ndarray, sr: int) -> ShotResult:
    """Full fingerprint of one captured sweep against its reference."""
    ref = np.asarray(ref, dtype=np.float64)
    out = np.asarray(out, dtype=np.float64)

    # Too short to analyse at all (recording/wiring failure) — fail cleanly,
    # never crash the band filters on a sub-padlen array.
    if min(ref.size, out.size) < sr // 4:
        return ShotResult(latency=LatencyResult(None, 0.0), passthrough=False,
                          passthrough_reason="capture too short / empty",
                          gain_db=-np.inf, freq_response=_empty_response(),
                          glitches=_empty_glitches())

    lat = measure_latency(ref, out, sr)

    out_rms = float(np.sqrt(np.mean(out ** 2)))
    if out_rms < PASSTHROUGH_MIN_RMS:
        # Silent output — report the failure, skip the spectral math.
        return ShotResult(latency=lat, passthrough=False,
                          passthrough_reason="output silent (nothing reached capture)",
                          gain_db=-np.inf, freq_response=_empty_response(),
                          glitches=_empty_glitches())
    # Passthrough = audio reached the capture point. Independent of whether
    # latency could be LOCKED: free-running clock domains (no interface) pass
    # audio but won't correlate — reported via low confidence, which routes
    # the latency to the computed estimate, not a passthrough failure.
    if lat.confidence < PASSTHROUGH_MIN_CONF:
        passthrough, reason = True, "ok (latency not locked — computed estimate)"
    else:
        passthrough, reason = True, "ok"

    ref_rms = float(np.sqrt(np.mean(ref ** 2)))
    gain_db = _db(out_rms) - _db(ref_rms)

    fr = _freq_response(ref, out, sr)
    gl = _glitches(out, sr)

    return ShotResult(latency=lat, passthrough=passthrough,
                      passthrough_reason=reason, gain_db=gain_db,
                      freq_response=fr, glitches=gl)


def _glitches(out: np.ndarray, sr: int) -> GlitchResult:
    """Count sample discontinuities and RMS dropouts; estimate noise floor.

    Detectors mirror the proven probe analyzer: a click is a single-sample
    jump far above the local median jump (robust MAD threshold); a dropout
    is a 10 ms window whose RMS collapses below the median.
    """
    diff = np.abs(np.diff(out))
    clicks = 0
    if diff.size > 256:
        # Local adaptive threshold: a real click is a jump far above the
        # MEDIAN jump in its own neighbourhood. This survives a clean sweep,
        # whose near-Nyquist tail has uniformly large (but legitimate) diffs.
        from scipy.ndimage import median_filter
        local_med = median_filter(diff, size=201, mode="nearest")
        floor = 10.0 * float(np.median(diff)) + 1e-9
        thr = np.maximum(8.0 * local_med, floor)
        peaks, _ = signal.find_peaks(diff, height=thr,
                                     distance=max(int(0.001 * sr), 1))
        clicks = int(peaks.size)

    # Find the active signal region so the silent lead-in (the latency
    # itself) and any trailing silence don't masquerade as dropouts.
    peak = float(np.max(np.abs(out))) if out.size else 0.0
    onset, offset = 0, len(out)
    if peak > 0:
        above = np.abs(out) > 0.02 * peak
        idx = np.flatnonzero(above)
        if idx.size:
            onset, offset = int(idx[0]), int(idx[-1]) + 1

    active = out[onset:offset]
    win = max(int(0.010 * sr), 32)
    nwin = len(active) // win
    dropouts = 0
    if nwin >= 4:
        framed = active[: nwin * win].reshape(nwin, win)
        rms = np.sqrt(np.mean(framed ** 2, axis=1))
        median = float(np.median(rms))
        if median > 0:
            dropouts = int(np.sum(rms < 0.25 * median))

    # Noise floor: RMS of the silent lead-in BEFORE the signal arrived.
    lead = out[: onset] if onset > sr // 1000 else out[: max(int(0.005 * sr), 1)]
    nf = float(np.sqrt(np.mean(lead ** 2))) if lead.size else 0.0
    return GlitchResult(clicks=clicks, dropout_windows=dropouts,
                        noise_floor_db=_db(nf))


def measure_latency_series(ref: np.ndarray, out: np.ndarray, sr: int,
                           n_segments: int = 4) -> list[float]:
    """Latency measured independently in each of `n_segments` time slices.

    Feeding the spread of these into aggregate_latency() yields jitter and
    the worst-case bound from a single capture of a looped sweep.
    """
    n = min(len(ref), len(out))
    seg = n // n_segments
    if seg < sr // 8:
        r = measure_latency(ref, out, sr)
        return [r.latency_ms] if r.latency_ms is not None else []
    out_ms: list[float] = []
    for i in range(n_segments):
        a, b = i * seg, (i + 1) * seg
        r = measure_latency(ref[a:b], out[a:b], sr)
        if r.latency_ms is not None:
            out_ms.append(r.latency_ms)
    return out_ms


def aggregate_latency(samples_ms: list[float]) -> LatencyAggregate:
    """Combine repeated sweep latencies into typical / jitter / worst-case.

    `worst_ms` (the max) is the GUARANTEED bound the user cares about.
    """
    arr = np.asarray([s for s in samples_ms if s is not None], dtype=np.float64)
    if arr.size == 0:
        return LatencyAggregate(float("nan"), float("nan"), float("nan"), 0)
    return LatencyAggregate(
        typical_ms=float(np.median(arr)),
        jitter_ms=float(np.std(arr)),
        worst_ms=float(np.max(arr)),
        n=int(arr.size),
    )


_BANDS = {"low": (20.0, 300.0), "mid": (300.0, 3000.0), "high": (3000.0, 16000.0)}


def _band_gain_db(ref: np.ndarray, out: np.ndarray, sr: int,
                  lo: float, hi: float) -> float:
    hi = min(hi, 0.45 * sr)
    sos = signal.butter(4, [lo, hi], btype="band", fs=sr, output="sos")
    r = signal.sosfiltfilt(sos, ref)
    o = signal.sosfiltfilt(sos, out)
    r_rms = float(np.sqrt(np.mean(r ** 2)))
    o_rms = float(np.sqrt(np.mean(o ** 2)))
    return _db(o_rms) - _db(r_rms)


def _freq_response(ref: np.ndarray, out: np.ndarray, sr: int) -> FreqResponse:
    return FreqResponse(
        low_db=_band_gain_db(ref, out, sr, *_BANDS["low"]),
        mid_db=_band_gain_db(ref, out, sr, *_BANDS["mid"]),
        high_db=_band_gain_db(ref, out, sr, *_BANDS["high"]),
    )
