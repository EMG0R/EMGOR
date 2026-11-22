#!/usr/bin/env python3
"""How far does each generator reward being zoomed into?

The journey descends toward a point on the MANDELBROT boundary, because that
is where structure is known to regenerate forever. Only one kernel in the bank
actually IS that boundary. For every other map the question "is there anything
down there" has an answer, and it is not obtainable by reading the source --
a Julia set, a Worley field and a metaball blob all look perfectly healthy at
the surface and fail at depth in three different ways.

So this measures it. Each kernel is rendered at the dive-0 target at a ladder
of depths, and each frame is scored on whether it still contains STRUCTURE:

  live   fraction of pixels that are neither interior-black nor blown-out.
         Catches the escape-time failure: iterations run out, every sample
         hits the ceiling, the frame goes solid.

  edge   mean absolute difference between horizontally adjacent pixels.
         Catches the FIELD failure, which black-fraction cannot see: value
         noise and Voronoi have a finite number of octaves, so past them the
         frame is not black at all, it is a smooth wash of colour. A frame can
         be 100% "live" and contain nothing.

A depth passes when both stay above a fraction of what that same kernel scored
at the surface -- relative, because a kernel is allowed to be sparse or dark
by nature, and the question is whether it LOSES structure as it descends.

  ./tests/depth_sweep.py [--build DIR] [--kernels N,N,..] [--out FILE]
"""
import argparse
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from motion_probe import read_ppm  # noqa: E402

# Depths to test, in decades. 0 is the surface and is the reference the rest
# are scored against.
LADDER = [0.0, 1.0, 2.0, 3.0, 4.0, 5.0]

# A depth keeps its structure if it holds this fraction of the surface score.
# Loose on purpose: the question is "did the picture die", not "did it change".
KEEP = 0.35


def score(path):
    w, h, d = read_ppm(path)
    # Subsampled on a stride: this runs on the Pi, in CPython, 180 times.
    # A 2-pixel stride still resolves both failure modes -- a frame that has
    # gone solid or gone smooth is uniform at every scale, so undersampling
    # cannot hide it.
    step = 2
    n = 0
    live = 0
    edge = 0
    for y in range(0, h, step):
        row = y * w
        for x in range(0, w - 1, step):
            n += 1
            i = 3 * (row + x)
            r, g, b = d[i], d[i + 1], d[i + 2]
            m = max(r, g, b)
            if 6 < m < 250:
                live += 1
            j = i + 3
            edge += abs(r - d[j]) + abs(g - d[j + 1]) + abs(b - d[j + 2])
    return live / n, edge / (n * 3.0)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--build', default=os.path.join(ROOT, 'build'))
    ap.add_argument('--geom', default='192x192')
    ap.add_argument('--kernels', default='')
    ap.add_argument('--out', default='')
    args = ap.parse_args()

    binary = os.path.join(args.build, 'neptr-video')
    if not os.path.exists(binary):
        print('no binary at %s' % binary)
        return 1

    names = subprocess.run([binary], env=dict(os.environ, NV_LIST_KERNELS='1',
                                              NV_HEADLESS='1'),
                           capture_output=True, text=True, timeout=60).stdout
    bank = [l.split('\t') for l in names.strip().splitlines() if '\t' in l]
    if not bank:
        print('could not list the bank; is NV_LIST_KERNELS wired up?')
        return 1

    want = ([int(x) for x in args.kernels.split(',')] if args.kernels
            else list(range(len(bank))))

    rows = []
    for ki in want:
        kid, kname = bank[ki][0], bank[ki][1]
        # One run per depth. Travel is integrated, so the frame to shoot is
        # the one where travel has reached the depth asked for: at the fixed
        # dt below and JOURNEY 3.0, that is 20 frames per decade. Frame 1 for
        # depth 0, because frame 0 predates the first advance.
        marks = []
        for depth in LADDER:
            tmp = tempfile.mkdtemp()
            shot = os.path.join(tmp, 's.ppm')
            # The depth is reached by TRAVELLING there, not by jumping: the
            # override forces the full five-decade dive and the shot frame
            # picks the point along it. Jumping would skip the detail-boost
            # ramp that a real dive climbs, and so would measure a frame the
            # instrument never actually shows.
            frame = max(1, int(round(depth * 20.0)))
            env = dict(os.environ)
            env.update({
                'NV_HOME': '0', 'NV_HEADLESS': '1', 'NV_CHAIN': 'none',
                'NV_KERNEL': str(ki), 'NV_FIXED_DT': '0.0166666',
                'NV_SET': 'g.evolve=0,g.speed=1,g.journey=3.0',
                'NV_JOURNEY_DEPTH': '5.0',
                'NV_GOVERNOR': '0', 'NV_GEOM': args.geom,
                'NV_MOTION_PHASE': '0.0',
                'NV_SHOT': shot, 'NV_SHOT_FRAME': str(frame),
                'NV_MAX_SECONDS': '120',
            })
            try:
                subprocess.run([binary], env=env, capture_output=True,
                               text=True, timeout=180)
            except subprocess.TimeoutExpired:
                marks.append(None)
                continue
            # A single NV_SHOT_FRAME writes the bare path; a list writes
            # indexed ones. Accept either rather than assuming, because
            # assuming silently scores nothing and reports every kernel as
            # dead -- which is exactly what the first run of this did.
            p = shot if os.path.exists(shot) else shot.replace('.ppm',
                                                               '.0.ppm')
            marks.append(score(p) if os.path.exists(p) else None)

        base = marks[0]
        if base is None or (base[0] < 0.02 and base[1] < 0.5):
            # Dead at the surface too: that is a kernel problem, not a depth
            # answer, and parking it is the honest result.
            keep = 0.0
        else:
            keep = 0.0
            for depth, m in zip(LADDER, marks):
                if m is None:
                    break
                if m[0] >= base[0] * KEEP and m[1] >= base[1] * KEEP:
                    keep = depth
                else:
                    break
        rows.append((ki, kid, kname, marks, keep))
        cells = ' '.join('%s' % ('  --  ' if m is None
                                 else '%.2f/%.1f' % m) for m in marks)
        print('%2d %-14s %-22s %s  ->  keep %.0f' % (ki, kid, kname[:22],
                                                     cells, keep))
        sys.stdout.flush()

    if args.out:
        with open(args.out, 'w') as f:
            for ki, kid, kname, marks, keep in rows:
                f.write('%s\t%.1f\n' % (kid, keep))
        print('\nwrote %s' % args.out)
    return 0


if __name__ == '__main__':
    sys.exit(main())
