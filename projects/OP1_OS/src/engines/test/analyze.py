#!/usr/bin/env python3
"""Print peak / rms / DC / stereo correlation / spectral centroid for demo WAVs (numpy, run in .venv)."""
import sys, wave, numpy as np
for p in sys.argv[1:]:
    w = wave.open(p); n = w.getnframes(); sr = w.getframerate()
    x = np.frombuffer(w.readframes(n), dtype="<i2").reshape(-1, 2).astype(float) / 32768
    m = x.mean(1)
    spec = np.abs(np.fft.rfft(m * np.hanning(len(m)))); f = np.fft.rfftfreq(len(m), 1 / sr)
    cen = (spec * f).sum() / max(spec.sum(), 1e-9)
    c = np.corrcoef(x[:, 0], x[:, 1])[0, 1] if x[:, 0].std() > 0 and x[:, 1].std() > 0 else 1
    clip = (np.abs(x) > 0.999).mean() * 100
    print(f"{p.split('/')[-1]:28s} {n/sr:5.1f}s peak {np.abs(x).max():.2f} rms {x.std():.3f} dc {m.mean():+.4f} "
          f"LRcorr {c:.2f} centroid {cen:6.0f} Hz clip {clip:.2f}%")
