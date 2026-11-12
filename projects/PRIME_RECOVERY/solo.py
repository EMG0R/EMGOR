#!/usr/bin/env python3
"""Per-channel loopback check: solo ONE output at a time, measure every input.

ioaudit.py plays all 8 tones at once. This plays a 1 kHz tone (-12 dBFS) on one
output only, captures all 8 inputs, and reports each input's level at 1 kHz,
so each channel is proven on its own. Stdlib only.

    python3 -I solo.py            # outputs 1..8 in turn
    python3 -I solo.py 8          # just out8
"""
import math, os, struct, subprocess, sys, tempfile, time, wave

DEV = "hw:sndrpihifiberry"
R, NCH, F, A, SEC = 48000, 8, 1000.0, 0.25, 3


def db(x):
    return -140.0 if x <= 1e-7 else 20 * math.log10(x)


def solo(out, tmp):
    play, cap = os.path.join(tmp, "play.wav"), os.path.join(tmp, "cap.wav")
    w = wave.open(play, "wb"); w.setnchannels(NCH); w.setsampwidth(4); w.setframerate(R)
    fr = bytearray(); z = [0] * NCH
    for i in range(R * SEC):
        z[out - 1] = int(A * math.sin(2 * math.pi * F * i / R) * 2**31)
        fr += struct.pack("<8i", *z)
    w.writeframes(bytes(fr)); w.close()
    rec = subprocess.Popen(["arecord", "-q", "-D", DEV, "-c8", "-r48000", "-fS32_LE", "-d", str(SEC), cap])
    time.sleep(0.2)
    subprocess.run(["aplay", "-q", "-D", DEV, play], check=True)
    rec.wait()
    c = wave.open(cap); n = c.getnframes()
    v = struct.unpack("<%di" % (n * NCH), c.readframes(n))
    lv = []
    for ch in range(NCH):
        x = v[ch::NCH][R // 2: R // 2 + R]       # 1 s from the middle of the take
        re = sum(s * math.cos(2 * math.pi * F * i / R) for i, s in enumerate(x))
        im = sum(s * math.sin(2 * math.pi * F * i / R) for i, s in enumerate(x))
        lv.append(db(2 * math.hypot(re, im) / len(x) / 2**31))
    return lv


def main():
    outs = [int(a) for a in sys.argv[1:]] or list(range(1, NCH + 1))
    with tempfile.TemporaryDirectory() as tmp:
        print("        " + "".join("   in%d" % (i + 1) for i in range(NCH)))
        for o in outs:
            lv = solo(o, tmp)
            best = max(range(NCH), key=lambda i: lv[i])
            verdict = ("-> in%d" % (best + 1)) if lv[best] > -60 else "SILENT everywhere"
            flag = "ok" if best == o - 1 and lv[best] > -60 else "FAIL"
            print("out%d  " % o + "".join("%6.0f" % l for l in lv) + "   %s  %s" % (verdict, flag))


if __name__ == "__main__":
    main()
