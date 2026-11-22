#!/usr/bin/env python3
"""What does the intrinsic motion cost?

Two different questions, and the first cut of this conflated them and produced
nonsense (motion made the Phoenix 70% FASTER):

  instructions   the extra arithmetic in the inner loop. Measured with
                 EVOLVE at 0 against a build where uMotion is a CONSTANT and
                 the driver folds the whole thing away. Both draw the IDENTICAL
                 picture -- every kernel body is written to reduce to its
                 static self at EVOLVE 0 -- so the escape workload is the same
                 and the only difference is the added maths.

  workload       what a MOVING picture costs. Escape time is data-dependent:
                 a frame with more of the set on screen runs more iterations,
                 so a fractal that is breathing costs a little more on some
                 frames and a little less on others. This is not the motion's
                 overhead, it is the fractal being a different fractal, and it
                 is reported separately rather than folded in.

Vsync is off and there is a glFinish per frame, because otherwise the loop
measures how long it took to SUBMIT a frame.

  ./tests/motion_cost.py [--build DIR] [--kernels 0,4,10] [--detail N]
"""
import argparse
import os
import re
import statistics
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tests'))
from motion_probe import read_ppm                       # noqa: E402


def measure(binary, kernel, detail, frames, static, evolve, geom, shot=None):
    env = dict(os.environ)
    env.update({
        'NV_HOME': '0', 'NV_SINGLE_WINDOW': '1', 'NV_CHAIN': 'none',
        'NV_KERNEL': str(kernel), 'NV_GEOM': geom,
        'NV_MOTION_PHASE': '0.0', 'NV_FIXED_DT': '0.0166666',
        'NV_SET': 'g.detail=%d,g.evolve=%g,g.speed=1' % (detail, evolve),
        'NV_FRAMETIME': '1', 'NV_VSYNC': '0', 'NV_GOVERNOR': '0', 'NV_MAX_FRAMES': str(frames),
    })
    if static:
        env['NV_MOTION_STATIC'] = '1'
    if shot:
        env['NV_SHOT'] = shot
        env['NV_SHOT_FRAME'] = '60'
    out = subprocess.run([binary], env=env, capture_output=True, text=True,
                         timeout=600).stdout
    ms = [float(m.group(1)) for m in re.finditer(r'\[ft\] ([\d.]+)', out)]
    ms = sorted(ms[len(ms) // 4:])          # drop compile + cold cache
    return ms


def med(ms):
    return statistics.median(ms) if ms else float('nan')


def best_of(binary, k, detail, frames, static, evolve, geom, runs=3):
    """The MINIMUM of several medians.

    A laptop GPU clocks up and down and shares itself with the compositor, so
    a single median is an upper bound contaminated by whatever else was
    happening. The minimum across runs is the closest thing to the cost of the
    work itself, and it is the same estimator for both sides of the comparison.
    """
    return min(med(measure(binary, k, detail, frames, static, evolve, geom))
               for _ in range(runs))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--build', default=os.path.join(ROOT, 'build'))
    ap.add_argument('--kernels', default='0,4,10,13,20,28')
    ap.add_argument('--detail', type=int, default=256)
    ap.add_argument('--frames', type=int, default=600)
    ap.add_argument('--geom', default='1280x720')
    ap.add_argument('--tmp', default='/tmp/nvtest')
    args = ap.parse_args()
    binary = os.path.join(args.build, 'neptr-video')
    os.makedirs(args.tmp, exist_ok=True)

    print('detail %d  %s  %d frames  (median of the last three quarters)'
          % (args.detail, args.geom, args.frames))
    print('%-4s %10s %10s %9s %10s %9s  %s'
          % ('KERN', 'STATIC', 'EVOLVE0', 'INSTR', 'EVOLVE1', 'MOVING',
             'PICTURE AT EVOLVE 0'))
    instr = []
    for k in [int(x) for x in args.kernels.split(',')]:
        # The identity check gets its own short run: a shot exits the process
        # at that frame, so folding it into a timing run would time 60 frames
        # instead of 600.
        sa = os.path.join(args.tmp, 'cost_static_%d.ppm' % k)
        sb = os.path.join(args.tmp, 'cost_zero_%d.ppm' % k)
        measure(binary, k, args.detail, 80, True, 1.0, '320x240', sa)
        measure(binary, k, args.detail, 80, False, 0.0, '320x240', sb)
        same = '?'
        if os.path.exists(sa) and os.path.exists(sb):
            pa, pb = read_ppm(sa)[2], read_ppm(sb)[2]
            if pa == pb:
                same = 'bit-exact'
            else:
                n = sum(1 for i in range(len(pa)) if pa[i] != pb[i])
                same = '%.3f%% px' % (100.0 * n / len(pa))

        a = best_of(binary, k, args.detail, args.frames, True, 1.0, args.geom)
        b = best_of(binary, k, args.detail, args.frames, False, 0.0, args.geom)
        c = best_of(binary, k, args.detail, args.frames, False, 1.0, args.geom)
        d1 = (b - a) / a * 100.0
        d2 = (c - a) / a * 100.0
        instr.append(d1)
        print('%-4d %9.3fms %9.3fms %+8.1f%% %9.3fms %+8.1f%%  %s'
              % (k, a, b, d1, c, d2, same))

    print()
    print('instruction cost across those kernels: median %+.1f%%, worst %+.1f%%'
          % (statistics.median(instr), max(instr)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
