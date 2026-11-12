#!/usr/bin/env python3
"""Numeric checks for habit_bench.csd renders (see plan Task 5)."""
import sys
import wave
import numpy as np

def load(path="/tmp/habit_bench.wav"):
    w = wave.open(path)
    sr = w.getframerate()
    x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16)
    x = x.reshape(-1, w.getnchannels())[:, 0].astype(float) / 32768.0
    return sr, x

def envelope(x, sr, win_ms=20):
    n = int(sr * win_ms / 1000)
    pad = (-len(x)) % n
    e = np.abs(np.pad(x, (0, pad))).reshape(-1, n).max(axis=1)
    t = np.arange(len(e)) * n / sr
    return t, e

def bursts(t, e, thresh=0.02):
    """Rising-edge times of energy bursts."""
    on = e > thresh
    edges = np.flatnonzero(on[1:] & ~on[:-1]) + 1
    return t[edges]

def peak_freq(x, sr, t0, dur=0.15):
    seg = x[int(t0 * sr):int((t0 + dur) * sr)]
    if len(seg) < 256 or np.abs(seg).max() < 0.01:
        return None
    f = np.fft.rfft(seg * np.hanning(len(seg)))
    return np.argmax(np.abs(f)) * sr / len(seg)

sr, x = load()
t, e = envelope(x, sr)
scn = int(sys.argv[1]) if len(sys.argv) > 1 else 1

if scn == 1:
    b = bursts(t, e)
    pre = np.diff([v for v in b if 2.2 < v < 7.8])
    post = np.diff([v for v in b if 9.0 < v < 29])
    print(f"burst times: {np.round(b, 2)}")
    print(f"echo spacing before hop: {np.round(pre, 2)} (expect ~0.98 s or blip-pattern multiples)")
    print(f"echo spacing after  hop: {np.round(post, 2)} (expect ~0.42 s multiples)")
    # pitch integrity: every energetic window's peak freq near 220/330/440
    bad = []
    for t0 in np.arange(0.05, 29, 0.1):
        f = peak_freq(x, sr, t0)
        if f and not any(abs(f - ref) / ref < 0.04 for ref in (220, 330, 440)):
            bad.append((round(t0, 1), round(f, 1)))
    print(f"off-pitch windows (warp check): {bad[:10] if bad else 'NONE - no pitch warp'}")
elif scn == 2:
    seg = lambda a, b: np.sqrt(np.mean(x[int(a*sr):int(b*sr)] ** 2))
    r10, r20, r28 = seg(9, 11), seg(19, 21), seg(27, 29)
    print(f"RMS t=10:{r10:.4f} t=20:{r20:.4f} t=28:{r28:.4f}")
    print("stable infinite" if r28 > 0.3 * r10 and r28 < 3 * r10 and r28 > 0.005
          else "FAIL: runaway or died")
    print(f"peak overall: {np.abs(x).max():.3f} (must be < 1.0, no clipping wall)")
elif scn == 3:
    seg = lambda a, b: np.sqrt(np.mean(x[int(a*sr):int(b*sr)] ** 2))
    quiet, ghosts = seg(7.8, 8.8), seg(10, 28)
    print(f"RMS quiet(7.8-8.8s): {quiet:.5f}   ghosts(10-28s): {ghosts:.5f}")
    print("ghosts resurrect old phrase" if ghosts > 3 * quiet and ghosts > 0.003
          else "FAIL: no scan ghosts")
elif scn == 4:
    seg = lambda a, b: np.sqrt(np.mean(x[int(a*sr):int(b*sr)] ** 2))
    before, after = seg(9, 14), seg(17, 29.5)
    print(f"RMS ghosts before restart(9-14s): {before:.5f}  after restart(17-29.5s): {after:.5f}")
    print("memory cleared on re-engage" if after < 0.1 * max(before, 0.01)
          else "FAIL: old material survived re-engage")
