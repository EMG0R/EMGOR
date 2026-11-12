#!/usr/bin/env python3
"""PMOR USB-audio fidelity test.

Device side: send SINE1 over CDC (firmware replaces the USB capture content
with a locally generated 440Hz sine while the full instrument keeps
running). This script records the tone from the PMOR capture device and
measures transport fidelity — any defect it finds happened in the
ISR->ring->drain->USB path, not in the music.

Metrics (8s capture, first 0.5s discarded as open-transient):
  hard glitches : sample-to-sample jumps > 3x the sine's max slope
                  (clicks, dropped packets, discontinuities)
  zero runs     : >=0.5ms of exact digital zero (whole packets missing)
  bad envelope  : 10ms windows whose RMS deviates >20% from the median
  THD+N         : energy outside the 440Hz fundamental, in dB

PASS = 0 hard glitches, 0 zero runs, <=2 bad envelope windows, THD+N < -40dB.

Run with the app's venv python:
  PMOR.app/Contents/Resources/app/venv/bin/python sine_fidelity.py [seconds]
"""
import sys
import time

import numpy as np
import serial
import sounddevice as sd

SECONDS = float(sys.argv[1]) if len(sys.argv) > 1 else 8.0
SR = 48000
F0 = 440.0


def find_port():
    import glob
    ports = [p for p in glob.glob("/dev/cu.usbmodem*") if "R3GL" not in p]
    return ports[0] if ports else None


def cdc(port, cmd):
    s = serial.Serial(port, 115200, timeout=2)
    s.write((cmd + "\n").encode())
    time.sleep(0.3)
    r = s.read(200).decode(errors="replace").strip()
    s.close()
    return r


def main():
    port = find_port()
    if not port:
        print("FAIL: no CDC port (device not enumerated)")
        return 2
    devs = sd.query_devices()
    idx = next((i for i, d in enumerate(devs)
                if ("opgorator" in d["name"].lower() or "pmor" in d["name"].lower())
                and d["max_input_channels"] > 0), None)
    if idx is None:
        print("FAIL: PMOR capture device not bound")
        return 2

    print("tone on:", cdc(port, "SINE1"))
    try:
        rec = sd.rec(int((SECONDS + 0.5) * SR), samplerate=SR, channels=2,
                     device=idx, dtype="float32")
        sd.wait()
    finally:
        print("tone off:", cdc(port, "SINE0"))

    x = rec.mean(axis=1)[int(0.5 * SR):]
    n = len(x)

    # hard glitches: slope bound for 0.5-amplitude 440Hz sine
    max_slope = 0.5 * 2 * np.pi * F0 / SR
    d = np.abs(np.diff(x))
    glitches = int((d > 3 * max_slope).sum())

    # zero runs >= 0.5ms
    zero = (x == 0.0).astype(np.int8)
    runs = 0
    run = 0
    for z in zero:
        run = run + 1 if z else 0
        if run == int(0.0005 * SR):
            runs += 1
    # envelope stability, 10ms windows
    w = int(0.010 * SR)
    m = n // w
    env = np.sqrt((x[:m * w].reshape(m, w) ** 2).mean(axis=1))
    med = np.median(env)
    bad_env = int((np.abs(env - med) > 0.2 * med).sum()) if med > 1e-6 else m

    # THD+N via FFT
    win = np.hanning(n)
    X = np.abs(np.fft.rfft(x * win))
    freqs = np.fft.rfftfreq(n, 1 / SR)
    fund = (freqs > F0 - 10) & (freqs < F0 + 10)
    sig = (X[fund] ** 2).sum()
    noise = (X ** 2).sum() - sig
    thdn = 10 * np.log10(noise / sig) if sig > 0 else 0.0

    rms = float(np.sqrt((x ** 2).mean()))
    print(f"rms={rms:.4f}  glitches={glitches}  zero_runs={runs}  "
          f"bad_env={bad_env}/{m}  THD+N={thdn:.1f}dB")
    ok = (glitches == 0 and runs == 0 and bad_env <= 2 and thdn < -40 and rms > 0.1)
    print("PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
