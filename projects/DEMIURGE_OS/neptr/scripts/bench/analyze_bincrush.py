#!/usr/bin/env python3
"""Analyze BINCRUSH bench renders (/tmp/bin_0..4.wav).

Render first:
  cd scripts/bench
  for n in 0 1 2 3 4; do csound bincrush_bench.csd -o /tmp/bin_$n.wav --omacro:SCN=$n; done
  python3 analyze_bincrush.py

Checks, per pedal scenario (0=heel/all bins .. 4=toe/sparse):
  * surviving bin count  -> must DECREASE as the pedal pushes in
  * RMS level            -> should stay roughly constant (balance vs dry working)
  * flicker (birdie)     -> per-bin temporal coeff-of-variation; LOW = soft/smooth
"""
import numpy as np
from scipy.io import wavfile

FFT = 256
HOP = 128
SETTLE_S = 0.8          # skip transient: portk ramp + threshold/balance settle


def load(n):
    sr, x = wavfile.read(f"/tmp/bin_{n}.wav")
    if x.ndim > 1:
        x = x[:, 0]                       # left channel
    x = x.astype(np.float64)
    x /= (np.abs(x).max() or 1.0)         # normalize for shape analysis
    return sr, x[int(sr * SETTLE_S):]


def stft_mag(x):
    win = np.hanning(FFT)
    frames = [np.abs(np.fft.rfft(x[i:i + FFT] * win))
              for i in range(0, len(x) - FFT, HOP)]
    return np.array(frames)              # (nframes, 129)


def analyze(n):
    sr, x = load(n)
    rms = float(np.sqrt(np.mean(x ** 2)))
    mags = stft_mag(x)
    avg = mags.mean(axis=0)
    peak = avg.max() or 1.0
    sig = avg > peak * 10 ** (-40 / 20)   # bins within 40 dB of the strongest
    n_bins = int(sig.sum())
    # flicker: temporal coeff-of-variation of the significant bins
    cols = mags[:, sig]
    cov = (cols.std(axis=0) / (cols.mean(axis=0) + 1e-12)).mean() if n_bins else 0.0
    return n_bins, rms, float(cov)


print(f"{'pedal':>6} {'bins':>5} {'rms':>8} {'flicker':>8}")
prev_bins = None
ok = True
for n in range(5):
    bins, rms, cov = analyze(n)
    print(f"{n*0.25:>6.2f} {bins:>5} {rms:>8.4f} {cov:>8.3f}")
    if prev_bins is not None and bins > prev_bins:
        ok = False
    prev_bins = bins

print()
print("bin count monotonically decreasing toward toe:", "PASS" if ok else "FAIL")
