#!/usr/bin/env python3
"""
Discontinuity detector.

For each output sample: first difference d1 and second difference d2, each
compared against a LOCAL scale derived from medians of |d1| / |d2| over a
window. The window is split in two halves and the scale is the MAX of the
two half-window medians - so a genuine note attack, where the half AFTER
the edge is legitimately loud, is scaled by that loud half and is NOT
flagged, while a click sitting inside otherwise-quiet or steady audio is.
"""
import sys, numpy as np

def counts(x, W=1024, K1=25.0, K2=25.0):
    d1 = np.abs(np.diff(x, 1))
    d2 = np.abs(np.diff(x, 2))
    n = len(d2)
    d1 = d1[1:n+1]
    H = W // 2
    nb = n // H
    out = {}
    for name, d, K in (("d1", d1, K1), ("d2", d2, K2)):
        med = np.array([np.median(d[i*H:(i+1)*H]) for i in range(nb)])
        # scale for half-block i = max(median of this half, median of the
        # neighbouring half) -> the louder side wins
        sc = np.maximum(med, np.maximum(
            np.concatenate(([med[0]], med[:-1])),
            np.concatenate((med[1:], [med[-1]]))))
        floor = max(1e-7, np.median(med) * 1e-3)
        sc = np.maximum(sc, floor)
        hits = 0
        worst = 0.0
        for i in range(nb):
            seg = d[i*H:(i+1)*H]
            r = seg / sc[i]
            hits += int((r > K).sum())
            if len(r) and r.max() > worst: worst = float(r.max())
        out[name] = (hits, worst)
    return out

def load(path, n=None):
    a = np.fromfile(path, dtype=np.float32)
    h = len(a)//2
    return a[:h], a[h:]

if __name__ == "__main__":
    for p in sys.argv[1:]:
        L, R = load(p)
        cl, cr = counts(L), counts(R)
        print("%-28s  L: d1 %5d (worst %7.1fx)  d2 %5d (worst %7.1fx)"
              "   R: d1 %5d (worst %7.1fx)  d2 %5d (worst %7.1fx)" % (
              p.split('/')[-1],
              cl["d1"][0], cl["d1"][1], cl["d2"][0], cl["d2"][1],
              cr["d1"][0], cr["d1"][1], cr["d2"][0], cr["d2"][1]))
