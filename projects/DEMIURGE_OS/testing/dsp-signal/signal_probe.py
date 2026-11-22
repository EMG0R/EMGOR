#!/usr/bin/env python3
"""signal_probe.py — inject a known test signal directly into csound's
input ports and capture/measure what comes back out, via PipeWire
(pw-cat), with no hardware loopback and no human playing anything.
Runs ON THE PI.

The live PipeWire graph on this rig wires the HiFiBerry ADC straight to
csound6:input1/2 and csound6:output1/2 straight to the HiFiBerry DAC (no
demiurge-sink hub in sync_layer=on + single-stage chain mode — verified
with `pw-link -l`). pw-cat --target=csound6 links an *additional*,
temporary stream directly to those same ports for the measurement
window, then tears itself down when the process exits — the permanent
hardware links are left untouched throughout (same "always restores"
philosophy as testing/latency/).

Usage:
    signal_probe.py tone   --freq 220 --dur 2.0 --out capture.wav
    signal_probe.py impulse --dur 5.0 --out capture.wav
    signal_probe.py noise  --dur 2.0 --out capture.wav
    signal_probe.py analyze capture.wav          # just measure an existing wav
"""
import argparse
import json
import math
import struct
import subprocess
import sys
import time
import wave

SR = 48000
TARGET = "csound6"
TAIL = 1.0  # extra seconds of capture after playback ends, to catch tails


def gen_tone(freq, dur, amp=0.3, fade=0.01):
    n = int(dur * SR)
    fadeN = int(fade * SR)
    out = []
    for i in range(n):
        s = amp * math.sin(2 * math.pi * freq * i / SR)
        if i < fadeN:
            s *= i / fadeN
        elif i > n - fadeN:
            s *= (n - i) / fadeN
        out.append(s)
    return out, out  # mono-derived stereo (L, R identical dry stimulus)


def gen_impulse(dur, amp=0.9):
    n = int(dur * SR)
    L = [0.0] * n
    L[int(0.05 * SR)] = amp
    return L, list(L)


def gen_noise(dur, amp=0.2, seed=12345):
    import random
    r = random.Random(seed)
    n = int(dur * SR)
    L = [amp * (r.random() * 2 - 1) for _ in range(n)]
    R = [amp * (r.random() * 2 - 1) for _ in range(n)]
    return L, R


def write_wav_s16(path, L, R):
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR)
        frames = bytearray()
        for l, r in zip(L, R):
            li = max(-32768, min(32767, int(l * 32767)))
            ri = max(-32768, min(32767, int(r * 32767)))
            frames += struct.pack("<hh", li, ri)
        w.writeframes(bytes(frames))


def read_wav_s16(path):
    with wave.open(path, "rb") as w:
        assert w.getnchannels() == 2, "expected stereo capture"
        n = w.getnframes()
        raw = w.readframes(n)
    vals = struct.unpack(f"<{2*n}h", raw)
    L = [v / 32768.0 for v in vals[0::2]]
    R = [v / 32768.0 for v in vals[1::2]]
    return L, R


def tail_rms(L, R, start_s, end_s):
    """RMS over an exact [start_s, end_s) window — use this (not whole-file
    RMS) to check for a reverb/decay TAIL after a stimulus ends: whole-file
    RMS is dominated by the stimulus itself and buries a quiet wet tail."""
    a, b = int(start_s * SR), int(end_s * SR)
    segL, segR = L[a:b], R[a:b]
    def rms(x):
        return math.sqrt(sum(v * v for v in x) / len(x)) if x else 0.0
    return rms(segL), rms(segR)


def analyze(L, R):
    n = len(L)
    if n == 0:
        return {"error": "empty capture"}
    def rms(x):
        return math.sqrt(sum(v * v for v in x) / len(x)) if x else 0.0
    rmsL, rmsR = rms(L), rms(R)
    peakL, peakR = max((abs(v) for v in L), default=0), max((abs(v) for v in R), default=0)
    clipL = sum(1 for v in L if abs(v) >= 0.999)
    clipR = sum(1 for v in R if abs(v) >= 0.999)
    def dbfs(x):
        return 20 * math.log10(x) if x > 1e-9 else -math.inf
    # L/R correlation (Pearson), only meaningful if both have energy
    def corr(a, b):
        ma = sum(a) / len(a); mb = sum(b) / len(b)
        va = sum((x - ma) ** 2 for x in a); vb = sum((x - mb) ** 2 for x in b)
        if va < 1e-12 or vb < 1e-12:
            return None
        cov = sum((x - ma) * (y - mb) for x, y in zip(a, b))
        return cov / math.sqrt(va * vb)
    return {
        "n_samples": n,
        "duration_s": round(n / SR, 3),
        "rms_L": rmsL, "rms_R": rmsR,
        "rms_dbfs_L": dbfs(rmsL), "rms_dbfs_R": dbfs(rmsR),
        "peak_L": peakL, "peak_R": peakR,
        "clipped_samples_L": clipL, "clipped_samples_R": clipR,
        "lr_correlation": corr(L, R),
        "has_signal": (rmsL > 1e-4 or rmsR > 1e-4),
    }


def capture_and_inject(stim_L, stim_R, out_path, extra_tail=TAIL):
    stim_dur = len(stim_L) / SR
    cap_dur = stim_dur + extra_tail
    stim_path = "/tmp/_dsp_probe_stim.wav"
    write_wav_s16(stim_path, stim_L, stim_R)

    cap_proc = subprocess.Popen(
        ["pw-cat", "--record", f"--target={TARGET}",
         "--rate", str(SR), "--channels", "2", "--format", "s16",
         out_path],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )
    time.sleep(0.35)  # let the capture stream actually link before we play

    play_proc = subprocess.run(
        ["pw-cat", "--playback", f"--target={TARGET}",
         "--rate", str(SR), "--channels", "2", "--format", "s16",
         stim_path],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )

    time.sleep(extra_tail)
    cap_proc.terminate()
    try:
        cap_proc.wait(timeout=3)
    except subprocess.TimeoutExpired:
        cap_proc.kill()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=["tone", "impulse", "noise", "analyze"])
    ap.add_argument("--freq", type=float, default=220.0)
    ap.add_argument("--dur", type=float, default=2.0)
    ap.add_argument("--amp", type=float, default=0.3)
    ap.add_argument("--out", default="/tmp/_dsp_probe_capture.wav")
    args = ap.parse_args()

    if args.mode == "analyze":
        L, R = read_wav_s16(args.out if args.out != "/tmp/_dsp_probe_capture.wav" else sys.argv[-1])
        print(json.dumps(analyze(L, R), indent=2))
        return

    if args.mode == "tone":
        L, R = gen_tone(args.freq, args.dur, args.amp)
    elif args.mode == "impulse":
        L, R = gen_impulse(args.dur, args.amp)
    else:
        L, R = gen_noise(args.dur, args.amp)

    capture_and_inject(L, R, args.out)
    capL, capR = read_wav_s16(args.out)
    result = analyze(capL, capR)
    result["stimulus"] = args.mode
    result["out_file"] = args.out
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
