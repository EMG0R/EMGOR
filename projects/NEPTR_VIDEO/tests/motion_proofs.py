#!/usr/bin/env python3
"""Reverse, freeze and continuity -- proved against the running binary.

Every claim here is about TWO FRAMES, so every one of them is checked by
capturing both from the real engine and comparing the pixels. Nothing is
inferred from the source.

  reverse    a frame reached by running forward to T is the same frame as one
             reached by overshooting T and rewinding back to it -- captured
             from the SAME process, so it is the identical GL state and the
             only difference is the route through time
  freeze     at SPEED 0 consecutive frames are byte-identical
  crossing   passing through 0 into reverse does not jump: the frame-to-frame
             difference across the crossing is no larger than it is either side
  continuity a dense run of consecutive frames has no outlier step
  long run   the same, after hours of simulated scene time, which is where a
             float clock's ulp would start to quantise the motion

  ./tests/motion_proofs.py [--build DIR]
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from motion_probe import read_ppm, mean_abs_diff        # noqa: E402

DT = 0.0166666
BASE = {
    'NV_HOME': '0',
    'NV_SINGLE_WINDOW': '1',
    'NV_CHAIN': 'none',          # FEEDBACK/GRAIN would differ on their own
    'NV_GEOM': '320x240',
    'NV_MOTION_PHASE': '0.0',    # the start phase is random per run by design
    'NV_FIXED_DT': str(DT),
    # The governor re-plans detail from measured load, which is exactly the
    # sort of run-to-run difference that would make two frames disagree for a
    # reason that has nothing to do with time.
    'NV_GOVERNOR': '0',
}

FAIL = []


def check(ok, what, detail=''):
    print('  %-6s %s%s' % ('ok' if ok else 'FAIL', what,
                           ('   ' + detail) if detail else ''))
    if not ok:
        FAIL.append(what)


def run(binary, env, tmp, name, frames, extra=None):
    shot = os.path.join(tmp, name + '.ppm')
    e = dict(os.environ)
    e.update(BASE)
    e.update(env)
    e['NV_SHOT'] = shot
    e['NV_SHOT_FRAME'] = ','.join(str(f) for f in frames)
    if extra:
        e.update(extra)
    r = subprocess.run([binary], env=e, capture_output=True, text=True,
                       timeout=600)
    stem = shot[:-4]
    paths = ([shot] if len(frames) == 1
             else ['%s.%d.ppm' % (stem, i) for i in range(len(frames))])
    times = {}
    for m in re.finditer(r'wrote (\S+) \(\d+x\d+\) frame (\d+) t=([-\d.]+) '
                         r'phase=([-\d.]+)', r.stdout):
        times[int(m.group(2))] = (float(m.group(3)), float(m.group(4)))
    return paths, times, r.stdout


def identical(a, b):
    return read_ppm(a)[2] == read_ppm(b)[2]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--build', default=os.path.join(ROOT, 'build'))
    ap.add_argument('--kernel', default='0')
    args = ap.parse_args()
    binary = os.path.join(args.build, 'neptr-video')
    tmp = tempfile.mkdtemp(prefix='nvproof')
    K = {'NV_KERNEL': args.kernel}

    # ---- reverse ----------------------------------------------------------
    # Forward to frame 60 (scene time 61*dt), keep going to frame 120, flip
    # SPEED to -1 at frame 121, and arrive back at 61*dt on frame 180:
    #   t(N) = (2*S - N - 1)*dt  with S = 121, N = 180  ->  61*dt.
    # One process, so the two frames differ only in the route.
    print('reverse (kernel %s)' % args.kernel)
    paths, times, out = run(binary, K, tmp, 'rev', [60, 180],
                            {'NV_SCRIPT': '121:set:g.speed=-1'})
    if len(times) == 2:
        t_fwd = times[60][0]
        t_back = times[180][0]
        check(abs(t_fwd - t_back) < 1e-6,
              'rewind lands on the same scene time',
              'forward t=%.6f  rewound t=%.6f' % (t_fwd, t_back))
        check(abs(times[60][1] - times[180][1]) < 1e-6,
              'and on the same motion phase',
              '%.6f vs %.6f' % (times[60][1], times[180][1]))
        d = mean_abs_diff(paths[0], paths[1])
        check(identical(paths[0], paths[1]),
              'the two frames are byte-identical', 'mean|d| %.6f' % d)
    else:
        check(False, 'captured both frames', out[-400:])

    # A control: the overshoot frame really was somewhere else, so the test
    # above is not passing because nothing ever moves.
    paths2, times2, _ = run(binary, K, tmp, 'ctrl', [60, 120])
    check(not identical(paths2[0], paths2[1]),
          'control: the overshoot frame is a different picture',
          'mean|d| %.4f' % mean_abs_diff(paths2[0], paths2[1]))

    # ---- freeze -----------------------------------------------------------
    print('freeze')
    paths, times, out = run(binary, K, tmp, 'frz', [90, 91, 150],
                            {'NV_SCRIPT': '60:set:g.speed=0'})
    if len(times) == 3:
        check(times[90][0] == times[150][0],
              'the scene clock stops dead',
              't=%.9f at both frame 90 and frame 150' % times[90][0])
        check(identical(paths[0], paths[1]),
              'consecutive frames are byte-identical')
        check(identical(paths[0], paths[2]),
              'and so are frames 60 apart')
    else:
        check(False, 'captured the freeze frames', out[-400:])

    # ---- crossing zero ----------------------------------------------------
    # Sweep SPEED down through 0 and out the other side, one step per frame,
    # capturing every frame across the crossing. A jump would show as one
    # frame pair whose difference dwarfs its neighbours.
    print('crossing zero')
    script = ';'.join('%d:set:g.speed=%.3f' % (100 + i, 0.6 - i * 0.06)
                      for i in range(21))
    frames = list(range(100, 121))
    paths, times, out = run(binary, K, tmp, 'cross', frames,
                            {'NV_SCRIPT': script})
    if len(paths) == len(frames) and all(os.path.exists(p) for p in paths):
        steps = [mean_abs_diff(paths[i], paths[i + 1])
                 for i in range(len(paths) - 1)]
        worst = max(steps)
        typical = sorted(steps)[len(steps) // 2]
        # Everything here is one frame of motion at |speed| <= 0.6, so no step
        # can be much bigger than the biggest legitimate one. 4x the median is
        # generous; a discontinuity is orders of magnitude, not a factor.
        check(worst <= max(4.0 * typical, 0.05),
              'no step across the crossing is an outlier',
              'median %.4f worst %.4f' % (typical, worst))
        check(min(steps) >= 0.0, 'captured %d consecutive frames' % len(paths))
    else:
        check(False, 'captured the crossing frames', out[-400:])

    # ---- dense continuity --------------------------------------------------
    print('continuity, 40 consecutive frames')
    frames = list(range(80, 120))
    paths, times, out = run(binary, K, tmp, 'cont', frames)
    if all(os.path.exists(p) for p in paths):
        steps = [mean_abs_diff(paths[i], paths[i + 1])
                 for i in range(len(paths) - 1)]
        worst, typical = max(steps), sorted(steps)[len(steps) // 2]
        check(worst <= max(3.0 * typical, 0.05),
              'frame-to-frame difference is smooth',
              'median %.4f worst %.4f' % (typical, worst))
    else:
        check(False, 'captured the continuity run', out[-400:])

    # ---- across a generator change ----------------------------------------
    # The PICTURE is meant to change completely -- that is what selecting a
    # different generator means. What must not change is the clock: if scene
    # time or the motion phase restarted on a recompile, every generator
    # change would also be a lurch in whatever was moving.
    print('across a generator change')
    paths, times, out = run(binary, K, tmp, 'kswitch', [100, 101, 140],
                            {'NV_SCRIPT': '100:key:]'})
    if len(times) == 3:
        expected = times[100][0] + DT
        check(abs(times[101][0] - expected) < 1e-6,
              'scene time does not restart on a kernel change',
              't %.6f -> %.6f' % (times[100][0], times[101][0]))
        check(times[140][1] > times[101][1],
              'the motion phase keeps running through the recompile')
    else:
        check(False, 'captured the generator change', out[-400:])

    # ---- after hours of scene time ----------------------------------------
    # dt of a whole second at SPEED 8 gets 8 hours of scene time into 3600
    # frames. This is where a float clock's ulp coarsens: if uTime were being
    # quantised, the last frames would either stop moving or step unevenly.
    print('after ~8 hours of scene time')
    long_script = '3600:set:g.speed=1'
    paths, times, out = run(
        binary, {'NV_KERNEL': args.kernel}, tmp, 'long',
        [3601, 3602, 3603, 3604],
        # JOURNEY parked. What this proof is about is the CLOCK -- whether a
        # float uTime coarsens after eight hours and starts quantising the
        # motion. It gets there by running dt=1.0 at SPEED 8, i.e. EIGHT
        # SCENE-SECONDS PER FRAME, which is fine for a bounded motion phase
        # and meaningless for a zoom: at the default rate that is two decades
        # of travel between consecutive frames, so a dive begins and ends
        # inside three samples and 'steps evenly' is not a property those
        # frames can have. Leaving it running measured 17.14, 0.41, 0.00 --
        # a real observation about a 2-decade-per-frame fast-forward, and
        # nothing at all about the clock this test exists to check.
        {'NV_FIXED_DT': '1.0', 'NV_SET': 'g.speed=8,g.journey=0',
         'NV_SCRIPT': long_script})
    if len(times) == 4:
        check(times[3601][0] > 25000.0, 'reached deep scene time',
              't=%.1f s' % times[3601][0])
        check(0.0 <= times[3603][1] < 38.0,
              'the motion phase is still inside its period',
              'phase=%.6f' % times[3603][1])
        steps = [mean_abs_diff(paths[i], paths[i + 1]) for i in range(3)]
        check(min(steps) > 0.0, 'still moving out there',
              'steps %s' % ' '.join('%.4f' % s for s in steps))
        check(max(steps) <= max(3.0 * sorted(steps)[1], 0.05),
              'and still stepping evenly',
              'steps %s' % ' '.join('%.4f' % s for s in steps))
    else:
        check(False, 'captured the long-run frames', out[-400:])

    print()
    if FAIL:
        print('FAIL: %d' % len(FAIL))
        for f in FAIL:
            print('  ' + f)
        return 1
    print('PASS')
    return 0


if __name__ == '__main__':
    sys.exit(main())
