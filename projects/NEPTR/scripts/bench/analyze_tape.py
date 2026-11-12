#!/usr/bin/env python3
"""Numeric checks for tape_bench renders. Usage: analyze_tape.py <scn> <wav> [<wav_amt0>]
scn 1: per-1.5s-segment out RMS in dB -> PASS if every AMOUNT step within +-0.5 dB
       of segment 0 (AMOUNT=0 reference)
scn 2: impulse group delay: argmax(|x|) - 48000 samples -> PASS if <= 144 (3 ms)
scn 3: (a) STFT f0 track of 440 Hz falls monotonically during the 2..3 s dive;
       (b) f0 back to 440 +-2 Hz by 6.5 s (<= 1.5 s wind-up);
       (c) click check: max |diff| in 4.5..7 s < 8x max |diff| of steady 1..2 s
scn 4: f0 deviation events (>30 cents from median): AMOUNT=1 wav >= 5 events in
       30 s; AMOUNT=0 wav 0 events and f0 std < 6 cents
"""
import sys
import numpy as np
from scipy.io import wavfile
from scipy.signal import stft


def load(path):
    sr, x = wavfile.read(path)
    if x.ndim > 1:
        x = x[:, 0]
    x = x.astype(float)
    if np.max(np.abs(x)) > 10:          # int PCM
        x /= 32768.0
    return sr, x


def f0_track(x, sr):
    f, t, Z = stft(x, sr, nperseg=8192, noverlap=6144)
    mag = np.abs(Z)
    band = (f > 100) & (f < 1500)
    fb = f[band]
    pk = fb[np.argmax(mag[band, :], axis=0)]
    return t, pk


def main():
    scn = int(sys.argv[1])
    sr, x = load(sys.argv[2])
    if scn == 1:
        segs = [x[int((1.5 * i + 0.4) * sr):int((1.5 * i + 1.4) * sr)] for i in range(5)]
        rms = [20 * np.log10(np.sqrt(np.mean(s ** 2))) for s in segs]
        rel = [r - rms[0] for r in rms]
        ok = all(abs(r) <= 0.5 for r in rel)
        print(f"scn1 rel dB per AMOUNT step: {[f'{r:+.2f}' for r in rel]} -> {'PASS' if ok else 'FAIL'}")
    elif scn == 2:
        d = np.argmax(np.abs(x)) - 3 * sr  # impulse fired at t=3
        # adaptive headroom: AMOUNT=0 rest gap is ~32 smp + filters
        print(f"scn2 group delay {d} samples ({1000 * d / sr:.2f} ms) -> {'PASS' if 0 <= d <= 96 else 'FAIL'}")
    elif scn == 3:
        t, f0 = f0_track(x, sr)
        dive = f0[(t > 2.1) & (t < 2.9)]
        mono = bool(np.all(np.diff(dive) < 5))
        back = f0[(t > 6.5) & (t < 8)]
        # one STFT bin (48000/8192 = 5.86 Hz) is the tracker's resolution floor
        ret = bool(np.all(np.abs(back - 440) < 6))
        w = lambda a, b: np.max(np.abs(np.diff(x[int(a * sr):int(b * sr)])))
        click_ok = bool(w(4.5, 7) < 8 * w(1, 2))
        print(f"scn3 dive-monotonic {mono}, return-by-6.5s {ret}, no-click {click_ok} -> "
              f"{'PASS' if (mono and ret and click_ok) else 'FAIL'}")
    elif scn == 4:
        def events(path):
            sr2, y = load(path)
            t, f0 = f0_track(y, sr2)
            # skip t<1.5 s: kAmt ports down from its 0.4 toggle-on default at
            # bench start (seed 0 => a random snag may fire in that window)
            keep = t > 1.5
            t, f0 = t[keep], f0[keep]
            cents = 1200 * np.log2(np.maximum(f0, 1) / np.median(f0))
            dev = np.abs(cents) > 30
            return int(np.sum(np.diff(dev.astype(int)) == 1)), float(np.std(cents))
        n1, _ = events(sys.argv[2])
        n0, sd0 = events(sys.argv[3])
        ok = n1 >= 5 and n0 == 0 and sd0 < 6
        print(f"scn4 events@AMT1={n1} events@AMT0={n0} std@AMT0={sd0:.1f}c -> {'PASS' if ok else 'FAIL'}")


main()
