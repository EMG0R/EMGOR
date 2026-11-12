#!/usr/bin/env python3
"""Find, for EACH kernel, the point on its own plane worth descending into.

The journey's seed targets are sixteen MANDELBROT coordinates. Exactly one
kernel in the bank is that map. For the other 21 escape kernels those numbers
are arbitrary, which is why tests/depth_sweep.py measured 16 of them dying
inside one decade -- not because the maps have no structure, but because the
dive was aimed somewhere they do not keep any.

Runtime steering does not fix this. It can refine an aim that is already in
roughly the right region; it cannot rescue one that is not, and measured, it
left BURNING SHIP exactly as dead while costing MANDELBROT its deepest decade.

So the aim is searched for OFFLINE, per kernel, and baked in as a constant --
which also means the live renderer keeps no feedback loop.

The method is progressive refinement, which is the only thing that can produce
a coordinate accurate enough to be worth having. A point picked from a wide
view is known to about a cell width, ~0.03 of the plane, which is exhausted by
1.5 decades of zoom. So the aim is re-measured at every half decade on the way
down: render at the current aim and depth, find the best boundary cell near
the centre, move the aim there, descend, repeat. Each step refines the
coordinate by roughly the factor it just zoomed.

Scoring is the same pair depth_sweep.py uses, and for the same reasons:

  live   fraction not interior-black -- catches the dive going solid.
  edge   mean |difference| between adjacent pixels -- catches the OTHER
         failure, a frame that is 100% live and still contains nothing.

A candidate is preferred by the depth it survives to, then by edge energy
there. Surviving is all-or-nothing: a dive that dies at four decades is worth
less than one that holds at three, however pretty its surface.

  ./tests/target_search.py [--build DIR] [--kernels 0,4,6] [--out FILE]
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

SPAN = 5.0          # Journey::kSpan
GEOM = (128, 128)


def render(binary, kernel, aim, travel, path, zoom=1.0):
    env = dict(os.environ)
    env.update({
        'NV_HOME': '0', 'NV_SINGLE_WINDOW': '1', 'NV_CHAIN': 'none',
        'NV_KERNEL': str(kernel), 'NV_FIXED_DT': '0.0166666',
        # SPEED 0 freezes travel, so the pinned value is what gets rendered.
        # EVOLVE 0 removes the intrinsic wobble, so the frame is a function of
        # the aim and the depth alone.
        'NV_SET': 'g.evolve=0,g.speed=0,g.detail=512,g.zoom=%.6g' % zoom,
        'NV_JOURNEY_AIM': '%.17g,%.17g' % aim,
        'NV_JOURNEY_TRAVEL': '%.17g' % travel,
        # Full span, so logZoom == travel exactly.
        'NV_JOURNEY_DEPTH': str(SPAN),
        'NV_SHOT': path, 'NV_SHOT_FRAME': '20',
        'NV_GEOM': '%dx%d' % GEOM, 'NV_MOTION_PHASE': '0.0',
        'NV_GOVERNOR': '0', 'NV_HEADLESS': '1', 'NV_MAX_SECONDS': '25',
    })
    subprocess.run([binary], env=env, capture_output=True, text=True,
                   timeout=60)
    # A single NV_SHOT_FRAME writes the plain path; a comma list writes
    # indexed variants. Accept either.
    for cand in (path, path.replace('.ppm', '.0.ppm')):
        if os.path.exists(cand):
            return cand
    return None


def analyse(path):
    """live fraction, edge energy, and the best boundary cell near the centre."""
    w, h, px = read_ppm(path)
    lum = [0] * (w * h)
    for i in range(w * h):
        lum[i] = px[3 * i] + px[3 * i + 1] + px[3 * i + 2]
    inter = [v <= 12 for v in lum]

    live = sum(0 if b else 1 for b in inter) / float(w * h)
    edge = 0.0
    for y in range(h):
        row = y * w
        for x in range(w - 1):
            edge += abs(lum[row + x + 1] - lum[row + x])
    edge /= float(h * (w - 1) * 3)

    # Best cell: most interior/exterior transitions in a window, restricted to
    # the central half so a measurement corrects the aim rather than replacing
    # it with whatever is at the frame's edge.
    # The WHOLE frame is a candidate, unlike the live steerer which is
    # restricted to the centre. The difference is deliberate: live steering is
    # a correction to an aim that is already close, so letting it jump across
    # the frame turns a descent into a random walk. This is a SEARCH, run
    # once, offline, and its whole job is to leave the region it started in --
    # restricting it to the centre is what made it stall at 1.5 decades
    # starting from the origin, which is inside the main cardioid.
    win, cands = 4, []
    # Effectively no interior: fall back to contrast scoring (see below).
    noInterior = live > 0.98
    lo_x, hi_x = win, w - win
    lo_y, hi_y = win, h - win
    for y in range(lo_y, hi_y):
        for x in range(lo_x, hi_x):
            t = 0
            for dy in range(-win, win + 1):
                yy = y + dy
                if yy < 0 or yy >= h:
                    continue
                prev = inter[yy * w + max(x - win, 0)]
                for dx in range(-win + 1, win + 1):
                    xx = x + dx
                    if xx < 0 or xx >= w:
                        continue
                    cur = inter[yy * w + xx]
                    if cur != prev:
                        t += 1
                    prev = cur
            if noInterior:
                # No interior anywhere in this frame, so there is no
                # black/lit frontier to find. That is not a degenerate case:
                # a Julia set at many parameter values is DUST, and a
                # convergent map like Newton has every point converging, so
                # neither has an interior in the escape sense. Their structure
                # lives in the escape-time GRADIENT instead, so score local
                # contrast. Without this both report no dive at any beam
                # width -- the search was looking for a frontier that cannot
                # exist for those maps.
                c = 0
                for dy in range(-win, win + 1, 2):
                    yy = y + dy
                    if yy < 0 or yy >= h:
                        continue
                    for dx in range(-win, win + 1, 2):
                        xx = x + dx
                        if xx < 0 or xx >= w:
                            continue
                        c += abs(lum[yy * w + xx] - lum[y * w + x])
                cands.append((c, x, y))
            else:
                cands.append((t, x, y))

    # Top candidates, with non-maximum suppression so they are genuinely
    # DIFFERENT regions rather than nine neighbours of one peak.
    cands.sort(reverse=True)
    picked = []
    for t, x, y in cands:
        if all(abs(x - px_) > w // 8 or abs(y - py_) > h // 8
               for _, px_, py_ in picked):
            picked.append((t, x, y))
        if len(picked) >= 6:
            break
    uv = [((x + 0.5) / w, (y + 0.5) / h) for _, x, y in picked]
    return live, edge, uv


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--build', default=os.path.join(ROOT, 'build'))
    ap.add_argument('--kernels', default='')
    ap.add_argument('--out', default='')
    ap.add_argument('--step', type=float, default=1.0)
    ap.add_argument('--beam', type=int, default=8)
    ap.add_argument('--expand', type=int, default=3)
    ap.add_argument('--aims', type=int, default=6)
    ap.add_argument('--sep', type=float, default=0.01)
    # Absolute floor as well as growth: 3x of nearly nothing is nothing.
    ap.add_argument('--minedge', type=float, default=2.0)
    # Decades to zoom OUT for the seed view. -0.7 spans about +/-5.
    # Zoom knob for the seed view. 0.22 spans about +/-4.5 of the plane.
    ap.add_argument('--seedzoom', type=float, default=0.22)
    args = ap.parse_args()

    binary = os.path.join(args.build, 'neptr-video')
    names = subprocess.run([binary], env={**os.environ, 'NV_LIST_KERNELS': '1'},
                           capture_output=True, text=True).stdout.strip().split('\n')
    rows = [ln.split('\t') for ln in names if ln.strip()]
    want = ([int(x) for x in args.kernels.split(',')] if args.kernels
            else list(range(len(rows))))

    tmp = tempfile.mkdtemp()
    out = []
    for k in want:
        if k >= len(rows):
            continue
        kid, kname, kind = rows[k][0], rows[k][1], rows[k][2]
        if kind != 'escape':
            print('%-3d %-16s FIELD -- holds its zoom still, no aim needed'
                  % (k, kid))
            continue

        # BEAM SEARCH down the depths.
        #
        # Branching only at the surface is not enough, and the failure is not
        # subtle: seeded at the origin this search found MANDELBROT a
        # five-decade dive, and seeded from a wide view it found the same map
        # only one decade -- because a single greedy path commits at every
        # step and one bad commit ends the descent. The structure worth
        # descending into is not always under the best-looking cell of the
        # frame above it.
        #
        # So several paths are carried down together and the weakest are
        # dropped at each level. Cost is beam * expand renders per half
        # decade, which is minutes per kernel, paid once, offline.
        # Seed at TWO scales and merge. One is not enough for a bank this
        # varied: BURNING SHIP's structure sits out near x = -1.8 and is
        # invisible from a view spanning +/-1, while MANDELBROT's best
        # candidates are washed out at a view spanning +/-4.5, where the whole
        # set is a handful of cells. Searching both costs two renders.
        beam = []
        floor, grow = 0.12, 0.0
        for sz in (args.seedzoom, 1.0):
            ps = render(binary, k, (0.0, 0.0), 0.0,
                        os.path.join(tmp, 'k%d_seed%g.ppm' % (k, sz)), zoom=sz)
            if not ps:
                continue
            _, e0, st = analyse(ps)
            floor = max(floor, 0.3 * e0)
            grow = max(grow, 1.8 * e0)
            beam += [(((su - 0.5) * 2.0 / sz, (sv - 0.5) * 2.0 / sz),
                      0.0, 0.0, []) for su, sv in st[:args.beam]]
        if not beam:
            print('%-3d %-16s NO FRAME' % (k, kid))
            continue
        best = ((0.0, 0.0), 0.0, 0.0, [])
        winners = []
        t = args.step
        n = 0
        while t <= SPAN + 1e-9 and beam:
            nxt = []
            for aim, _, _, trail in beam:
                n += 1
                p = render(binary, k, aim, t,
                           os.path.join(tmp, 'k%d_%d_%g.ppm' % (k, n, t)))
                if not p:
                    continue
                live, edge, uv = analyse(p)
                # Alive means BOTH: not solid, and not a flat wash -- but
                # judged RELATIVE to what this kernel scores at its own
                # surface, never against an absolute number. Kernels differ by
                # an order of magnitude in inherent contrast: MANDELBROT
                # measures 1.6 at the surface on this scale and JULIA 0.30, so
                # a fixed threshold of 0.5 rejected Julia's very first frame
                # and reported the map has no dive at any beam width. What is
                # being asked is whether structure SURVIVED the descent, which
                # is a question about the ratio.
                if live < 0.02 or edge < floor or not uv:
                    continue
                tr = trail + ['%.1f:%.2f/%.1f' % (t, live, edge)]
                # A depth only COUNTS if structure actually grew on the way
                # down. Surviving a floor is not enough: JULIA at its default
                # parameters holds a dead-flat edge energy of 0.30 from the
                # surface to five decades, which clears any relative floor and
                # is a smooth wash the whole way -- a false pass that would
                # bake in a target with nothing at the bottom. Descending into
                # real self-similar structure RAISES edge energy, often by
                # ten times or more.
                # Every qualifying path is kept, not just the best one.
                #
                # One aim per kernel would make every dive land in the SAME
                # place, so the instrument would loop with a period of one
                # dive -- which is the recurrence this whole design exists to
                # eliminate. A kernel needs several aims for the recycle to
                # mean anything.
                if edge >= grow and edge >= args.minedge:
                    winners.append((t, edge, aim))
                if edge >= grow and (t, edge) > (best[1], best[2]):
                    best = (aim, t, edge, tr)
                zoom = 10.0 ** t
                for u, v in uv[:args.expand]:
                    nxt.append(((aim[0] + (u - 0.5) * 2.0 / zoom,
                                 aim[1] + (v - 0.5) * 2.0 / zoom),
                                t, edge, tr))
            # Keep the most promising paths: deepest first, then liveliest.
            nxt.sort(key=lambda e: (e[1], e[2]), reverse=True)
            beam = nxt[:args.beam]
            t += args.step

        aim, depth, best_edge, trail = best

        # Up to args.aims well-separated aims, deepest first. Separation is in
        # plane units: two aims a thousandth apart are the same dive.
        winners.sort(reverse=True, key=lambda w: (w[0], w[1]))
        picked = []
        for d, e, a in winners:
            if all(abs(a[0] - q[0]) > args.sep or abs(a[1] - q[1]) > args.sep
                   for _, _, q in picked):
                picked.append((d, e, a))
            if len(picked) >= args.aims:
                break
        out.append((k, kid, aim, depth, best_edge, picked))
        print('%-3d %-16s depth %.1f  edge %.1f  aim %.15g,%.15g   %s'
              % (k, kid, depth, best_edge, aim[0], aim[1], ' '.join(trail)))

    if args.out:
        with open(args.out, 'w') as f:
            for k, kid, aim, depth, edge, picked in out:
                for d, e, a in picked:
                    f.write('%s\t%.17g\t%.17g\t%.1f\t%.2f\n'
                            % (kid, a[0], a[1], d, e))
        print('\nwrote %s' % args.out)
    return 0


if __name__ == '__main__':
    sys.exit(main())
