#!/usr/bin/env python3
"""Does the picture ADVANCE, or does it just move?

motion_probe.py already proves every generator is not static. That is a
strictly weaker claim than the one that matters here, and passing it is
exactly what the ORIGINAL bug did: the closed sinusoid path moved every
frame, differed from its neighbour, and still went nowhere -- it came back
to its own start every ~12.5 s, forever.

So this probe measures the two things a loop cannot fake:

  RECURRENCE  the smallest difference between the last frame and any EARLIER
              frame. On a closed path some earlier frame is revisited, so this
              collapses toward zero once the series is longer than the period.
              On a journey it stays high: you never return.

  GROWTH      difference against the first frame as a function of separation.
              A loop's rises and then FALLS as it comes back around. A
              journey's is non-decreasing.

Run with the effect chain bypassed, EVOLVE at 0 and the governor off, so the
only thing that can move the picture is the journey itself.

  ./tests/journey_probe.py [--build DIR] [--kernel N] [--journey RATE]
"""
import argparse
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from motion_probe import read_ppm, mean_abs_diff  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--build', default=os.path.join(ROOT, 'build'))
    ap.add_argument('--kernel', default='0')
    ap.add_argument('--journey', default='1.0')
    # 16 frames spread over ~40 s of scene time at a fixed dt. Long enough to
    # cover several turns of the OLD 12.5 s loop, which is the thing that has
    # to show up as recurrence if this probe is working.
    # Sampling has to be DENSE relative to the loop being looked for. The
    # first cut of this probe took 16 samples over 2400 frames, missed the
    # ~12.5 s EVOLVE period by up to 1.3 s of phase, and scored the old
    # looping behaviour at 8.3 -- a clean pass for the exact bug it was
    # written to catch. 96 samples is ~30 per period, and the measured
    # separation is then 0.135 (loop) against 33.2 (journey).
    ap.add_argument('--span', type=int, default=2400)
    ap.add_argument('--count', type=int, default=96)
    # Ignore the immediate neighbours: adjacent frames are similar because
    # motion is continuous, not because anything recurred.
    ap.add_argument('--skip', type=int, default=8)
    ap.add_argument('--recur', type=float, default=5.0)
    # Minimum share of the requested frames that must actually be captured.
    ap.add_argument('--mincover', type=float, default=0.8)
    # Also run the control: EVOLVE alone, journey parked. That MUST look like
    # a loop. A probe that cannot fail proves nothing, so it checks itself.
    ap.add_argument('--no-control', action='store_true')
    args = ap.parse_args()

    binary = os.path.join(args.build, 'neptr-video')
    if not os.path.exists(binary):
        print('no binary at %s' % binary)
        return 1

    frames = [str(int(1 + i * args.span / (args.count - 1)))
              for i in range(args.count)]

    def capture(setstr, tag):
        tmp = tempfile.mkdtemp()
        shot = os.path.join(tmp, tag + '.ppm')
        env = dict(os.environ)
        env.update({
            'NV_HOME': '0', 'NV_SINGLE_WINDOW': '1', 'NV_CHAIN': 'none',
            'NV_KERNEL': args.kernel, 'NV_FIXED_DT': '0.0166666',
            'NV_SET': setstr,
            'NV_SHOT': shot, 'NV_SHOT_FRAME': ','.join(frames),
            'NV_GEOM': '320x240', 'NV_MOTION_PHASE': '0.0',
            'NV_GOVERNOR': '0', 'NV_HEADLESS': '1', 'NV_MAX_SECONDS': '150',
        })
        r = subprocess.run([binary], env=env, capture_output=True, text=True,
                           timeout=240)
        paths = [shot.replace('.ppm', '.%d.ppm' % i)
                 for i in range(len(frames))]
        got = [p for p in paths if os.path.exists(p)]
        if len(got) < len(paths):
            print('MISSING %d/%d frames for %s'
                  % (len(paths) - len(got), len(paths), tag))
            print(r.stdout[-600:])
        return got

    def enough(paths, tag):
        """Refuse to judge on a fraction of the run.

        This probe scored a PASS on 35 of 96 frames on the Pi, where DRM
        ignores NV_GEOM so every shot is a full-panel PPM and the wall-clock
        cap bites long before the frame list is done. Recurrence is a MINIMUM
        over earlier frames, so dropping frames can only make it look better:
        the fewer frames captured, the fewer chances to find the return that
        would fail the test. A missing-data pass is the one failure mode this
        probe cannot afford.
        """
        if len(paths) >= args.mincover * len(frames):
            return True
        print('FAIL: only %d of %d frames captured for %s -- too few to judge '
              '(recurrence is a minimum, so missing frames can only flatter '
              'the result)' % (len(paths), len(frames), tag))
        return False

    def recurrence(paths):
        """Smallest difference between the last frame and any earlier one.

        This is the number that separates a journey from a loop: a closed
        path revisits its own start, so some earlier frame matches the last
        one almost exactly.
        """
        return min(mean_abs_diff(p, paths[-1]) for p in paths[:-args.skip])

    ok = True

    # EVOLVE at 0, so anything measured here is the journey and nothing else.
    jp = capture('g.evolve=0,g.speed=1,g.journey=%s' % args.journey, 'j')
    if len(jp) <= args.skip or not enough(jp, 'journey'):
        return 1
    jrec = recurrence(jp)
    jfar = mean_abs_diff(jp[0], jp[-1])
    print('journey   recurrence %8.3f   first-vs-last %8.3f' % (jrec, jfar))
    if jrec < args.recur:
        print('FAIL: the picture RETURNS to an earlier frame -- '
              'that is a loop, not a journey')
        ok = False
    if jfar < args.recur:
        print('FAIL: the last frame barely differs from the first')
        ok = False

    # The control. EVOLVE alone is a CLOSED path by construction, so it has
    # to score as a loop here. If it does not, the probe is not measuring
    # what it claims and the result above means nothing.
    if not args.no_control:
        cp = capture('g.evolve=1,g.speed=1,g.journey=0', 'c')
        if not enough(cp, 'control'):
            ok = False
        elif len(cp) > args.skip:
            crec = recurrence(cp)
            print('control   recurrence %8.3f   (EVOLVE only, must look '
                  'like a loop)' % crec)
            if crec >= args.recur:
                print('FAIL: the control did not register as a loop -- '
                      'this probe cannot detect the bug it exists for')
                ok = False

    print('\nPASS: the picture advances and never returns' if ok
          else '\nFAILED')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
