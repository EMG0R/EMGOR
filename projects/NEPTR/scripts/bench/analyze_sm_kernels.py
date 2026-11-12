#!/usr/bin/env python3
"""Render + analyse the SoundMagic kernel bench. numpy only.
Run from anywhere:  neptr/.venv/bin/python scripts/bench/analyze_sm_kernels.py [--no-render] [scn ...]
Renders sm_kernels_bench.csd per scenario into the session scratchpad (never /tmp), prints PASS/FAIL."""
import os, struct, subprocess, sys, time
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = "/private/tmp/claude-501/-Users-emgor-Documents--EMGOR-SYNTH-------2026NEW-DEMIURGE-OS/1188d03a-b110-4e08-ad94-f7501642fdc7/scratchpad"
CSD = os.path.join(HERE, "sm_kernels_bench.csd")
SR = 48000
N = 2048
BIN = SR / N


def read_wav(p):
    b = open(p, "rb").read()
    assert b[:4] == b"RIFF", "not a WAV: " + p
    i = 12
    fmt = None
    while i < len(b):
        cid = b[i:i + 4]
        sz = struct.unpack("<I", b[i + 4:i + 8])[0]
        if cid == b"fmt ":
            fmt = struct.unpack("<HHIIHH", b[i + 8:i + 24])
        elif cid == b"data":
            tag, ch, sr, _, _, bits = fmt
            raw = b[i + 8:i + 8 + sz]
            if tag == 3 and bits == 32:
                a = np.frombuffer(raw, "<f4")
            elif bits == 16:
                a = np.frombuffer(raw, "<i2") / 32768.0
            else:
                raise ValueError("fmt %r" % (fmt,))
            a = a.astype(np.float64).reshape(-1, ch)
            return a[:, 0], a[:, 1]
        i += 8 + sz + (sz & 1)


def seg(x, a, b):
    return x[int(a * SR):int(b * SR)]


def rms(x):
    return float(np.sqrt(np.mean(x ** 2) + 1e-30))


def db(x):
    return 20 * np.log10(x + 1e-30)


def spec(x):
    w = np.hanning(len(x))
    return np.abs(np.fft.rfft(x * w, 8 * len(x)))


def centroid(x):
    s = spec(x)
    f = np.fft.rfftfreq(8 * len(x), 1 / SR)
    return float((s * f).sum() / (s.sum() + 1e-30))


def peak_hz(x):
    s = spec(x)
    k = int(np.argmax(s))
    f = np.fft.rfftfreq(8 * len(x), 1 / SR)
    if 0 < k < len(s) - 1:
        a, b, c = np.log(s[k - 1] + 1e-30), np.log(s[k] + 1e-30), np.log(s[k + 1] + 1e-30)
        d = 0.5 * (a - c) / (a - 2 * b + c)
        return float(f[k] + d * (f[1] - f[0]))
    return float(f[k])


NAMES = {1: "AVG", 2: "BLUR", 3: "FRZ", 4: "SHUF", 5: "BSHIFT", 6: "PITCH"}


def neutral(o, d):
    a, b = (2.0, 5.0)
    r = db(rms(seg(o, a, b)) / rms(seg(d, a, b)))
    c = centroid(seg(o, a, b)) / centroid(seg(d, a, b))
    ok = abs(r) <= 1.0 and abs(c - 1) <= 0.05
    return ok, "rms %+.2f dB, centroid ratio %.3f" % (r, c)


def t_freeze(o, d):
    a = rms(seg(o, 1.2, 1.9)); b = rms(seg(o, 3.5, 5.5))
    r = db(b / a)
    return abs(r) <= 3.0, "RMS held after input stop: %+.2f dB vs while playing (need within 3)" % r


def t_freeze0(o, d):
    a = rms(seg(o, 1.2, 1.9)); b = rms(seg(o, 3.5, 5.5))
    r = db(b / a)
    return r < -40, "control (factor 0): RMS after stop %+.1f dB (need < -40)" % r


def t_blur(o, d):
    early = rms(seg(o, 1.1, 1.6)); ref = rms(seg(d, 1.1, 1.6)); late = rms(seg(o, 5.0, 5.9))
    ok = early < 0.5 * ref and late > early * 1.3
    return ok, "after step: out %.3f vs dry %.3f at 0.1-0.6 s, still rising to %.3f at 5 s" % (early, ref, late)


def t_avg(o, d):
    early = rms(seg(o, 1.1, 1.25)); ref = rms(seg(d, 1.1, 1.25)); late = rms(seg(o, 5.0, 5.9)); fin = rms(seg(d, 5.0, 5.9))
    ok = early < 0.5 * ref and late > 0.85 * fin
    return ok, "after step: out %.3f vs dry %.3f at 0.1-0.25 s; settles to %.3f of %.3f" % (early, ref, late, fin)


def t_shuf(o, d):
    # R channel of scn 14 = kernel-domain sqrt(E_out/E_in) of the magnitude arrays (exact permutation test).
    # Time-domain RMS is NOT a valid energy test: a shuffled STFT is inconsistent, so Hann overlap-add
    # of the (incoherent) frames loses ~5.5 dB regardless of group size (measured). Reported as info.
    a, b = (2.0, 5.0)
    k = db(float(d[int(b * SR)]))
    e = db(rms(seg(o, a, b)) / 0.5)
    so = spec(seg(o, a, b))
    f = np.fft.rfftfreq(8 * int((b - a) * SR), 1 / SR)
    m = (f > 300) & (f < 600)
    left = np.sum(so[m] ** 2) / np.sum(so ** 2)
    ok = abs(k) <= 1.0 and left < 0.5
    return ok, "kernel-domain mag energy %+.3f dB; output energy fraction left in the 300-600 Hz source band %.2f (dry ~0.9+); time-domain loss info only" % (k, left)


def t_bs(o, d):
    p = peak_hz(seg(o, 2, 5)); exp = 440 + 10 * BIN
    return abs(p - exp) <= 0.5 * BIN, "peak %.1f Hz, expected %.1f (+10 bins = %.1f Hz)" % (p, exp, 10 * BIN)


def t_ps(o, d):
    p = peak_hz(seg(o, 2, 5))
    return abs(p / 880 - 1) <= 0.03, "peak %.1f Hz, expected 880 (+-3%%)" % p


TESTS = [
    (1, "AVG neutral", neutral), (2, "BLUR neutral", neutral), (3, "FRZ neutral", neutral),
    (4, "SHUF neutral", neutral), (5, "BSHIFT neutral", neutral), (6, "PITCH neutral", neutral),
    (11, "FRZ 100% holds", t_freeze), (21, "FRZ 0% control", t_freeze0), (12, "BLUR 100% smooths step", t_blur),
    (13, "AVG 50 frames smooths step", t_avg), (14, "SHUF 100% moves bins, keeps energy", t_shuf),
    (15, "BSHIFT +10 bins", t_bs), (16, "PITCH +1200 cents", t_ps),
]


def render(n):
    out = os.path.join(OUT, "sm_scn%d.wav" % n)
    t = time.time()
    r = subprocess.run(["csound", "-d", "--omacro:SCN=%d" % n, "-o", out, CSD], cwd=HERE,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    el = time.time() - t
    if r.returncode != 0 or "error:" in r.stdout:
        print("RENDER FAIL scn %d\n%s" % (n, "\n".join(l for l in r.stdout.splitlines() if "error:" in l)[:800]))
    return out, el


def main():
    args = sys.argv[1:]
    norender = "--no-render" in args
    sel = [int(a) for a in args if a.isdigit()]
    fails = 0
    for n, name, fn in TESTS:
        if sel and n not in sel:
            continue
        p = os.path.join(OUT, "sm_scn%d.wav" % n)
        if not norender:
            p, _ = render(n)
        try:
            o, d = read_wav(p)
            ok, msg = fn(o, d)
        except Exception as e:
            ok, msg = False, "error %r" % e
        fails += (not ok)
        print("%-4s scn %-3d %-36s %s" % ("PASS" if ok else "FAIL", n, name, msg))
    if not sel or 99 in sel:
        if not norender:
            p, el = render(99)
            o, d = read_wav(p)
            print("INFO scn 99  all six kernels, N=2048, 10 s: render %.2f s wall (%.1fx realtime), peak %.2f"
                  % (el, 10 / el, np.max(np.abs(o))))
    print("%d failed" % fails)
    sys.exit(1 if fails else 0)


main()
