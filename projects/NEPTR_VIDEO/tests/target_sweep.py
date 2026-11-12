#!/usr/bin/env python3
"""Which of the journey's TARGET POINTS actually have something at the bottom?

The dive aims at one of sixteen coordinates said to be "on the boundary,
where the detail is". That claim was never checked, and it is false for some
of them: a 48 s run of the real engine at the default rate travelled through
three dives and one of them arrived at a COMPLETELY BLACK frame, with another
sitting at an edge energy of 0.12 -- a flat wash.

That is not a precision failure and not a kernel failure. It is a bad
coordinate. A point that looks like it is on the boundary at 1e0 can be
comfortably inside the set by 1e4, and then the dive ends on solid interior.

So each target is flown, at MANDELBROT, to the full five decades, and scored
the same way tests/depth_sweep.py scores a kernel: live fraction (not
interior-black) and edge energy (not a flat wash). A target is KEPT only if
it holds structure the whole way down, because a dive is all-or-nothing --
there is no partial credit for going black in the last decade.

  ./tests/target_sweep.py [--build DIR] [--kernel N]
"""
import argparse
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from depth_sweep import score  # noqa: E402

# The whole descent, sampled every half decade near the bottom where the
# failures live. Depth 0 is the surface reference.
LADDER = [0.0, 1.0, 2.0, 3.0, 3.5, 4.0, 4.5, 5.0]

# Absolute floors, not relative ones. depth_sweep.py scores a kernel against
# its own surface because a kernel is allowed to be sparse by nature. A
# TARGET is not: every target renders the same map, so they are directly
# comparable, and "is there structure here" has one answer for all of them.
MIN_LIVE = 0.04    # below this the frame is essentially solid
MIN_EDGE = 0.60    # below this it is a smooth wash with no boundary in it


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--build', default=os.path.join(ROOT, 'build'))
    ap.add_argument('--kernel', default='0')
    ap.add_argument('--count', type=int, default=16)
    args = ap.parse_args()

    binary = os.path.join(args.build, 'neptr-video')
    if not os.path.exists(binary):
        print('no binary at %s' % binary)
        return 1

    good, bad = [], []
    for ti in range(args.count):
        marks = []
        for depth in LADDER:
            tmp = tempfile.mkdtemp()
            shot = os.path.join(tmp, 's.ppm')
            frame = max(1, int(round(depth * 20.0)))
            env = dict(os.environ)
            env.update({
                'NV_HOME': '0', 'NV_HEADLESS': '1', 'NV_CHAIN': 'none',
                'NV_KERNEL': args.kernel, 'NV_FIXED_DT': '0.0166666',
                'NV_SET': 'g.evolve=0,g.speed=1,g.journey=3.0',
                'NV_JOURNEY_DEPTH': '5.0', 'NV_JOURNEY_TARGET': str(ti),
                'NV_GOVERNOR': '0', 'NV_MOTION_PHASE': '0.0',
                'NV_SHOT': shot, 'NV_SHOT_FRAME': str(frame),
                'NV_MAX_SECONDS': '120',
            })
            try:
                subprocess.run([binary], env=env, capture_output=True,
                               text=True, timeout=180)
            except subprocess.TimeoutExpired:
                marks.append(None)
                continue
            p = shot if os.path.exists(shot) else shot.replace('.ppm',
                                                               '.0.ppm')
            marks.append(score(p) if os.path.exists(p) else None)

        ok = all(m is not None and m[0] >= MIN_LIVE and m[1] >= MIN_EDGE
                 for m in marks)
        (good if ok else bad).append(ti)
        cells = ' '.join('  --  ' if m is None else '%.2f/%.1f' % m
                         for m in marks)
        # Name the first depth that failed: that is the actionable half.
        why = ''
        if not ok:
            for depth, m in zip(LADDER, marks):
                if m is None or m[0] < MIN_LIVE or m[1] < MIN_EDGE:
                    why = '  DIES at %.1f' % depth
                    break
        print('target %2d  %s  %s%s' % (ti, cells, 'KEEP' if ok else 'drop',
                                        why))
        sys.stdout.flush()

    print('\ndepths: %s' % ' '.join('%.1f' % d for d in LADDER))
    print('KEEP %d/%d: %s' % (len(good), args.count,
                              ','.join(str(g) for g in good)))
    print('drop: %s' % ','.join(str(b) for b in bad))
    return 0


if __name__ == '__main__':
    sys.exit(main())
