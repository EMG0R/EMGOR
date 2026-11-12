#!/usr/bin/env python3
"""How transparent is BINCRUSH on a guitar-like source? Compares DRY (L) vs
WET (R) from bincrush_diag.csd across pedal positions.

Renders each scenario then reports, per pedal:
  * spectral difference (dB) between dry and wet average spectra
  * fraction of dry spectral energy that survives in the wet
A tiny difference at high pedal = the effect is transparent (the bug).
"""
import subprocess
import numpy as np
from scipy.io import wavfile

FFT = 2048


def render(scn):
    subprocess.run(
        ["csound", "bincrush_diag.csd", "-o", f"/tmp/diag_{scn}.wav",
         f"--omacro:SCN={scn}"],
        capture_output=True, check=True)
    sr, x = wavfile.read(f"/tmp/diag_{scn}.wav")
    x = x.astype(np.float64) / 32768.0
    dry, wet = x[:, 0], x[:, 1]
    return sr, dry[sr//2:], wet[sr//2:]     # skip attack transient


def avg_spec(sig):
    win = np.hanning(FFT)
    frames = [np.abs(np.fft.rfft(sig[i:i+FFT]*win))
              for i in range(0, len(sig)-FFT, FFT//2)]
    return np.mean(frames, axis=0)


print(f"{'pedal':>6} {'spec_diff_dB':>12} {'energy_kept':>12}")
for scn in range(5):
    sr, dry, wet = render(scn)
    ds, ws = avg_spec(dry), avg_spec(wet)
    # normalize both to unit energy (isolate SHAPE change, not level)
    ds /= np.linalg.norm(ds) or 1
    ws /= np.linalg.norm(ws) or 1
    diff_db = 20*np.log10(np.linalg.norm(ws-ds) + 1e-9)
    # energy overlap: how much of dry's shape remains
    kept = float(np.dot(ds, ws) / ((np.linalg.norm(ds)*np.linalg.norm(ws)) or 1))
    print(f"{scn*0.25:>6.2f} {diff_db:>12.1f} {kept:>12.3f}")

print()
print("Reading: energy_kept near 1.0 = wet looks like dry = TRANSPARENT (bad).")
print("We want energy_kept to drop clearly as the pedal pushes in.")
