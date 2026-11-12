#!/usr/bin/env python3
"""Turn live.txt / seed.txt benchmark output into stats + pgfplots coordinates."""
import statistics, sys, os

d = os.path.dirname(os.path.abspath(__file__))

def load(fn):
    rows = {}
    for line in open(os.path.join(d, fn)):
        parts = line.strip().split('\t')
        if len(parts) != 3:
            continue
        idx, name, err = int(parts[0]), parts[1], float(parts[2])
        if idx < 25:
            rows[idx] = (name, err)  # duplicate final segment overwrites -> same model, fine
    return rows

live, seed = load('live.txt'), load('seed.txt')
assert sorted(live) == sorted(seed) == list(range(25)), (sorted(live), sorted(seed))
for i in range(25):
    assert live[i][0] == seed[i][0], (i, live[i][0], seed[i][0])

order = sorted(range(25), key=lambda i: -seed[i][1])

def stats(rows):
    v = [rows[i][1] for i in range(25)]
    return (statistics.pstdev(v), max(v) - min(v), max(v, key=abs))

for label, rows in (('seed-only', seed), ('live', live)):
    sd, spread, worst = stats(rows)
    print(f"{label:10s} stdev={sd:.2f} dB  spread={spread:.1f} dB  worst-abs={worst:+.2f} dB")

n_out_live = sum(1 for i in range(25) if abs(live[i][1]) > 2.0)
print(f"live captures outside +/-2 dB: {n_out_live}")

with open(os.path.join(d, 'coords.tex'), 'w') as f:
    f.write("% x = rank (sorted by seed error desc), y = error dB\n")
    f.write("\\def\\seedcoords{" +
            " ".join(f"({r+1},{seed[i][1]:.2f})" for r, i in enumerate(order)) + "}\n")
    f.write("\\def\\livecoords{" +
            " ".join(f"({r+1},{live[i][1]:.2f})" for r, i in enumerate(order)) + "}\n")
print("coords.tex written")
