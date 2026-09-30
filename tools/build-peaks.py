#!/usr/bin/env python3
"""Precompute waveform peaks for the in-site player.

    python3 tools/build-peaks.py resources/peaks.json resources/apk/*.wav resources/apk/*.m4a resources/*.wav

Writes {"<src path>": [0..1 x 160], ...}. Needs ffmpeg. Re-run when tracks change.
"""
import json, os, subprocess, sys
import numpy as np

BARS = 160
out_path = sys.argv[1]
files = sys.argv[2:]
peaks = json.load(open(out_path)) if os.path.exists(out_path) else {}
for f in files:
    raw = subprocess.run(['ffmpeg', '-v', 'quiet', '-i', f, '-ac', '1', '-ar', '8000', '-f', 's16le', '-'], capture_output=True).stdout
    x = np.frombuffer(raw, dtype=np.int16).astype(np.float32) / 32768.0
    if not len(x):
        print('skip', f); continue
    n = len(x) // BARS
    seg = x[:n * BARS].reshape(BARS, n)
    rms = np.sqrt((seg ** 2).mean(axis=1))
    rms = rms / (rms.max() or 1)
    rms = np.power(rms, 0.7)  # lift quiet parts a little, soundcloud-ish
    peaks[f] = [round(float(v), 3) for v in rms]
    print('ok', f)
json.dump(peaks, open(out_path, 'w'), separators=(',', ':'))
