#!/usr/bin/env python3
"""POLY DLY v5 bench analysis (pure python): onset error + pitch per repeat.
usage: analyze_polydelay.py diff INTV s1..s4.wav u1..u4.wav   (see diff mode below; 24-bit WAV, bench scales out by 0.25)
       analyze_polydelay.py file.wav series              (pitch vs time, for A/D check)"""
import sys, wave, math, os
import math
def rms(x, a, b): return math.sqrt(sum(v*v for v in x[a:b])/max(1, b-a))
def zc(x, a, b):
    seg = x[a:b]; t = []
    for i in range(1, len(seg)):
        if seg[i-1] < 0 <= seg[i]:
            t.append(i - 1 + (-seg[i-1]) / (seg[i]-seg[i-1]))
    if len(t) < 3: return float('nan')
    return (len(t)-1) * 48000.0 / (t[-1]-t[0])
def ac_freq(x, a, b, fguess):
    # dominant frequency: Hann-windowed DFT magnitude scan, +-15% of the guess, 0.25 Hz steps, parabolic refine
    seg = x[a:b]; n = len(seg)
    wn = [seg[i]*(0.5-0.5*math.cos(2*math.pi*i/(n-1))) for i in range(n)]
    def mag(f):
        w = 2*math.pi*f/48000.0
        c = math.cos(w); s_ = math.sin(w)
        # recurrence for cos/sin sums
        re = im = 0.0; cr = 1.0; ci = 0.0
        for v in wn:
            re += v*cr; im += v*ci
            cr, ci = cr*c - ci*s_, cr*s_ + ci*c
        return re*re + im*im
    fs = [fguess*(0.85 + 0.30*i/120.0) for i in range(121)]
    m = [mag(f) for f in fs]
    j = max(range(len(fs)), key=lambda i: m[i])
    if 0 < j < len(fs)-1:
        den = m[j-1] - 2*m[j] + m[j+1]
        off = 0.5*(m[j-1]-m[j+1])/den if den else 0
        return fs[j] + off*(fs[1]-fs[0])
    return fs[j]
def load(fn):
    w = wave.open(fn); n = w.getnframes(); raw = w.readframes(n)
    assert w.getsampwidth() == 3
    d = [int.from_bytes(raw[i:i+3], 'little', signed=True)/8388608.0*4.0 for i in range(0, len(raw), 3)]
    return w.getframerate(), d[0::2], d[1::2]
def env100(x):
    # rectified + 1-pole low-pass (~100 Hz) envelope
    a = math.exp(-2*math.pi*100/48000.0); y = 0.0; o = []
    for v in x:
        y = a*y + (1-a)*abs(v); o.append(y)
    return o
def t10(x, exp):
    # first crossing of 10 % of the repeat's own peak, searched around the grid time
    e = env100(x); lo = int((exp-0.05)*48000); hi = int((exp+0.30)*48000)
    pk = max(e[lo:hi])
    for i in range(lo, hi):
        if e[i] > 0.1*pk: return i/48000.0
if sys.argv[1] == 'diff':
    # usage: analyze_polydelay.py diff INTV s1.wav s2.wav s3.wav s4.wav u1.wav u2.wav u3.wav u4.wav
    #   s* = renders at Interval INTV with REPS=1..4, u* = same with --omacro:INTV=0 (unshifted twin).
    # repeat k = render(k) - render(k-1) (isolates overlapping repeats exactly).
    # Reports onset(shifted) - onset(unshifted) by 10 %-of-peak crossing, and dominant frequency
    # (Hann-DFT scan) of the shifted repeat vs 220*2^(INTV*k/12).
    INTV = float(sys.argv[2]); n = (len(sys.argv)-3)//2
    sh = [load(f) for f in sys.argv[3:3+n]]; un = [load(f) for f in sys.argv[3+n:3+2*n]]; sr = sh[0][0]
    def iso(r, si, k):
        x = r[k-1][si]
        return x if k == 1 else [a-b for a, b in zip(x, r[k-2][si])]
    BURST = float(os.environ.get('BURST', '0.2'))   # burst length of the bench render (s)
    def energy(x, exp):
        return sum(v*v for v in x[int((exp-0.15)*48000):int((exp+BURST+0.35)*48000)])
    def centroid(x, exp):
        # energy centroid (s) around the grid time: timing metric free of phase-vocoder pre-echo
        lo = int((exp-0.10)*48000); hi = int((exp+0.40)*48000)
        e = sum(v*v for v in x[lo:hi])
        return sum(((lo+i)/48000.0)*v*v for i, v in enumerate(x[lo:hi]))/e if e > 0 else float('nan')
    print('k side  d_onset_ms(shifted-unshifted)   f_hz   f_exp   err%   lvl_plateau_dB  lvl_energy_dB  d_centroid_ms')
    ds = []
    for k in range(1, n+1):
        for si, name, T in ((1, 'L', 0.25), (2, 'R', 1/6.0)):
            xs = iso(sh, si, k); xu = iso(un, si, k); exp = k*T
            d = (t10(xs, exp) - t10(xu, exp))*1000; ds.append(d)
            fe = 220*2**(INTV*k/12.0)
            fw = max(0.05, 0.4*BURST)   # pitch window: mid-plateau (the first ~N*(pitch+1)/2 of a PV repeat is smeared)
            f = ac_freq(xs, int((exp+fw)*sr), int((exp+fw+0.1)*sr), fe)
            len_ = 10*math.log10(energy(xs, exp)/energy(xu, exp))
            # plateau window sits mid-burst (smearing of the PV edges is ~N*(pitch+1)/2 either side)
            a0 = int((exp+BURST*0.35)*sr); a1 = int((exp+BURST*0.65)*sr)
            lvl = 20*math.log10(rms(xs, a0, a1)/rms(xu, a0, a1))
            dc = (centroid(xs, exp) - centroid(xu, exp))*1000
            print('%d %s   %+7.2f                       %7.1f %7.1f %+6.2f   %+6.2f   %+6.2f   %+6.2f' % (k, name, d, f, fe, (f/fe-1)*100, lvl, len_, dc))
    m = sum(ds)/len(ds)
    print('onset diff mean %+.2f ms, sd %.2f, range %.2f..%.2f' % (m, (sum((d-m)**2 for d in ds)/len(ds))**.5, min(ds), max(ds)))
    sys.exit()
fn = sys.argv[1]
sr, L, R = load(fn)
def zc(x, a, b):
    seg = x[a:b]; t = []
    for i in range(1, len(seg)):
        if seg[i-1] < 0 <= seg[i]:
            t.append(i - 1 + (-seg[i-1]) / (seg[i]-seg[i-1]))   # interpolated crossing
    if len(t) < 3: return float('nan')
    return (len(t)-1) * sr / (t[-1]-t[0])
def rms(x, a, b): return math.sqrt(sum(v*v for v in x[a:b])/max(1, b-a))
if sys.argv[2:3] == ['series']:
    for name, x in (('L', L), ('R', R)):
        print(name, ' '.join('%.0f' % zc(x, i, i+int(0.04*sr)) for i in range(int(0.2*sr), int(1.2*sr), int(0.04*sr))))
    sys.exit()
INTV = float(sys.argv[2]) if len(sys.argv) > 2 else 7
REPS = int(sys.argv[3]) if len(sys.argv) > 3 else 4
blk = int(0.0005*sr)
print('k  side expected_ms onset_ms err_ms  f_hz  f_exp  err%')
for name, x, T in (('L', L, 0.25), ('R', R, 1/6.0)):
    for k in range(1, REPS+1):
        exp = k*T; on = None
        # plateau level = rms over exp+60..exp+120 ms; onset = 50% crossing of the
        # 2 ms rms envelope, minus 5 ms (half of the 10 ms linear fade-in)
        plat = rms(x, int((exp+0.03)*sr), int((exp+0.07)*sr))
        bw = int(0.002*sr)
        for i in range(int((exp-0.04)*sr), int((exp+0.12)*sr), 24):
            if rms(x, i, i+bw) > 0.5*plat: on = i + bw//2 - int(0.005*sr); break
        fe = 220*2**(INTV*k/12.0)
        f = zc(x, on+int(0.03*sr), on+int(0.08*sr)) if on else float('nan')
        print('%d  %s  %8.1f %8.1f %+6.2f  %7.1f %7.1f %+5.2f' % (k, name, exp*1000, on/sr*1000 if on else -1, (on/sr-exp)*1000 if on else float('nan'), f, fe, (f/fe-1)*100))
