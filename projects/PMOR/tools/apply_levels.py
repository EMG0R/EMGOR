#!/usr/bin/env python3
"""Apply the JSON exported by the "PMOR Drum Levels" page to engine/drum_sounds.cpp.
Usage: python3 tools/apply_levels.py levels.json   (multiplies every .gain by the exported multiplier)"""
import json, re, sys
src = 'engine/drum_sounds.cpp'
j = json.load(open(sys.argv[1]))
order = [('kick', 0), ('snare', 1), ('hat', 2), ('perc', 3)]
lines = open(src).read().split('\n')
rows = []; gi = -1
for i, ln in enumerate(lines):
    r = re.match(r'\s*/\*\s*(\d+)\s+(\w+)\s*\*/', ln)
    if r:
        n = int(r[1]); gi += 1 if n == 0 else 0; cur = (gi, n)
    if '.gain =' in ln: rows.append((i, cur))
assert len(rows) == 64, len(rows)
for i, (g, n) in rows:
    key = order[g][0]
    mult = float(j[key][n])
    old = float(re.search(r'\.gain = ([\d.]+)f', lines[i])[1])
    lines[i] = re.sub(r'\.gain = [\d.]+f', '.gain = %.3ff' % (old * mult), lines[i])
open(src, 'w').write('\n'.join(lines))
print('applied')
