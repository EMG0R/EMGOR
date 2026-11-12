#!/usr/bin/env python3
"""Does every generator in the bank actually MOVE?

Not a unit test of a formula -- this drives the real binary, captures real
frames at two different scene times, and compares the pixels. A kernel whose
motion exists only in the source reads as a failure here, which is the point:
the whole class of bug being chased was "the code says it animates".

The effect chain is bypassed (NV_CHAIN=none) because FEEDBACK and GRAIN make
consecutive frames differ no matter what the generator does, and a test that
cannot fail is not a test.

  ./tests/motion_probe.py [--build DIR] [--frames A,B] [--evolve N]
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)


def read_ppm(path):
    with open(path, 'rb') as f:
        data = f.read()
    # P6 <w> <h> <max>\n<binary>. Header fields are whitespace separated and
    # may carry comments; the engine writes the plain form.
    parts = []
    i = 0
    while len(parts) < 4:
        while i < len(data) and data[i:i + 1].isspace():
            i += 1
        if data[i:i + 1] == b'#':
            while i < len(data) and data[i:i + 1] != b'\n':
                i += 1
            continue
        j = i
        while j < len(data) and not data[j:j + 1].isspace():
            j += 1
        parts.append(data[i:j])
        i = j
    i += 1
    w, h = int(parts[1]), int(parts[2])
    return w, h, data[i:i + w * h * 3]


def mean_abs_diff(a, b):
    _, _, pa = read_ppm(a)
    _, _, pb = read_ppm(b)
    n = min(len(pa), len(pb))
    if n == 0:
        return -1.0
    total = 0
    for i in range(0, n, 7):          # every 7th byte: plenty, and fast
        d = pa[i] - pb[i]
        total += d if d >= 0 else -d
    return total / float(len(range(0, n, 7)))


def kernel_names(out):
    """The banner every run prints is the list; there is no second one to
    keep in step with it."""
    names = {}
    for m in re.finditer(r'(\d+)\s+(\S[^\n]*?)\s{2,}', out):
        names[int(m.group(1))] = m.group(2).strip()
    return names


def run(binary, env, timeout=120):
    e = dict(os.environ)
    e.update(env)
    return subprocess.run([binary], env=e, capture_output=True, text=True,
                          timeout=timeout)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--build', default=os.path.join(ROOT, 'build'))
    ap.add_argument('--frames', default='40,220')
    ap.add_argument('--evolve', default='1.0')
    ap.add_argument('--speed', default='1.0')
    ap.add_argument('--threshold', type=float, default=0.5)
    ap.add_argument('--count', type=int, default=0)
    args = ap.parse_args()

    binary = os.path.join(args.build, 'neptr-video')
    tmp = tempfile.mkdtemp(prefix='nvmotion')
    names = {}
    n = args.count or 0
    failures = []
    print('%-4s %-24s %10s' % ('IDX', 'GENERATOR', 'MEAN|D|'))
    k = -1
    while True:
        k += 1
        if n and k >= n: break
        shot = os.path.join(tmp, 'k%d.ppm' % k)
        r = run(binary, {
            'NV_HOME': '0', 'NV_SINGLE_WINDOW': '1', 'NV_CHAIN': 'none',
            'NV_KERNEL': str(k), 'NV_FIXED_DT': '0.0166666',
            'NV_SET': 'g.evolve=%s,g.speed=%s' % (args.evolve, args.speed),
            'NV_SHOT': shot, 'NV_SHOT_FRAME': args.frames,
            'NV_GEOM': '320x240', 'NV_MOTION_PHASE': '0.0', 'NV_GOVERNOR': '0',
        })
        if not names:
            names = kernel_names(r.stdout)
            if not args.count:
                mm = re.search(r'\[nv\] kernels (\d+)', r.stdout)
                n = int(mm.group(1)) if mm else 30
        a, b = shot.replace('.ppm', '.0.ppm'), shot.replace('.ppm', '.1.ppm')
        if not (os.path.exists(a) and os.path.exists(b)):
            print('%-4d %-24s  NO FRAMES\n%s' % (k, names.get(k, '?'), r.stdout[-400:]))
            failures.append((k, names.get(k, '?'), 'no frames'))
            continue
        d = mean_abs_diff(a, b)
        flag = '' if d >= args.threshold else '   <-- STATIC'
        if d < args.threshold:
            failures.append((k, names.get(k, '?'), d))
        print('%-4d %-24s %10.4f%s' % (k, names.get(k, '?'), d, flag))

    print()
    if failures:
        print('FAIL: %d generator(s) did not move' % len(failures))
        for k, nm, d in failures:
            print('  %d %s (%s)' % (k, nm, d))
        return 1
    print('PASS: all %d generators moved' % n)
    return 0


if __name__ == '__main__':
    sys.exit(main())
