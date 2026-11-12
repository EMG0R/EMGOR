# ---------------------------------------------------------------- LED ring dots (2026-10-15 audit)
# WS2812B 144/m strip, 113 LEDs cut from it, closed ring (firmware wraps: min(dRaw, N-dRaw)).
# Strip sits glued to the INNER shoulder surface, centreline = horizontal contour of the inner skin at z_c where the
# contour length = 113 * 1000/144. Dots = tiny through holes along the local surface normal, seeded random scatter
# (dart throwing in (arclength c, profile offset s)) with the same number of dots under every LED.
import bpy, bmesh, json, math
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

# v2 (2026-10-15 polish): thinner band. Same centreline (z_c), same 5 dots/LED, same sizes + seed, but 3 explicit ROWS at
# s = -row_pitch, 0, +row_pitch (+- jit_s jitter) instead of a +-3.8 scatter; spacing rule = hole-to-hole web >= web_min.
# v1 (scatter, band_half 3.8, gap_min 1.7) is kept in NEPTR_4.2_led_dots_v1.py/.json.
LED = dict(n_led=113, pitch=1000.0 / 144.0, seed=4242, k_per_led=5, band_half=3.8, d_choices=(0.9, 1.3),
           rows=3, row_pitch=1.2, jit_s=0.2, web_min=0.6, web_target=0.68,
           gap_min=1.7, cut_in=3.0, cut_out=5.5, seg=16, split_z=45.0, batch=13, order=140,
           start='contour point at +y (north centre), arclength increasing CCW seen from +z (ASSUMED pixel order)')


def _bvh_obj(o):
    me = o.data; me.calc_loop_triangles()
    V = np.empty(len(me.vertices) * 3); me.vertices.foreach_get('co', V); V = V.reshape(-1, 3)
    T = np.empty(len(me.loop_triangles) * 3, np.int64); me.loop_triangles.foreach_get('vertices', T)
    return BVHTree.FromPolygons(V.tolist(), T.reshape(-1, 3).tolist())


def inner_contour(b, z, N=3600, a0=math.pi / 2):
    """inner-skin contour at height z: rays from outside inward, 2nd hit. Returns (N,3) pts and (N,3) outward normals."""
    P = []; Nn = []
    for k in range(N):
        a = a0 + 2 * math.pi * k / N
        d = Vector((-math.cos(a), -math.sin(a), 0)); p = Vector((300 * math.cos(a), 300 * math.sin(a), z))
        h1 = b.ray_cast(p, d)
        h2 = b.ray_cast(h1[0] + d * 1e-3, d)
        P.append(h2[0][:])
        Nn.append((-Vector(h2[1]))[:])  # stored face normal points into the cavity; flip => points into the wall (outward)
    return np.array(P), np.array(Nn)


def contour_len(P):
    return float(np.linalg.norm(np.diff(np.vstack([P, P[:1]]), axis=0), axis=1).sum())


def solve_zc(b, target, lo=53.0, hi=58.5, N=3600):
    for _ in range(22):
        mid = (lo + hi) / 2
        L = contour_len(inner_contour(b, mid, N)[0])
        if L > target: lo = mid
        else: hi = mid
    return (lo + hi) / 2


def gen_dots_scatter_v1(b, cfg=LED):
    """returns dict(zc, L, dots=[...], stats)"""
    target = cfg['n_led'] * cfg['pitch']
    zc = solve_zc(b, target)
    Pc, Nc = inner_contour(b, zc, 7200)
    seg = np.linalg.norm(np.diff(np.vstack([Pc, Pc[:1]]), axis=0), axis=1)
    cum = np.concatenate([[0], np.cumsum(seg)]); L = cum[-1]
    p = L / cfg['n_led']  # exact cell pitch on this contour (target 6.944)
    rng = np.random.default_rng(cfg['seed'])
    # 2D dart throwing, wrap in c
    cs = []; ss = []; ds = []; fails = 0
    for i in range(cfg['n_led']):
        sizes = sorted(rng.choice(cfg['d_choices'], cfg['k_per_led']), reverse=True)
        for d in sizes:
            for _ in range(6000):
                c = (i + rng.random()) * p; s = rng.uniform(-cfg['band_half'], cfg['band_half'])
                if cs:
                    dc = np.abs(np.array(cs) - c); dc = np.minimum(dc, L - dc)
                    ds_ = np.array(ss) - s
                    need = (np.array(ds) + d) / 2 + cfg['gap_min']
                    if (np.hypot(dc, ds_) < need).any(): continue
                cs.append(c); ss.append(s); ds.append(float(d)); break
            else:
                fails += 1
    cs = np.array(cs); ss = np.array(ss); ds = np.array(ds)
    dots = []
    for c, s, d in zip(cs, ss, ds):
        j = int(np.searchsorted(cum, c) - 1) % len(Pc); f = (c - cum[j]) / seg[j]
        P0 = Pc[j] * (1 - f) + Pc[(j + 1) % len(Pc)] * f
        n0 = Nc[j] * (1 - f) + Nc[(j + 1) % len(Pc)] * f; n0 /= np.linalg.norm(n0)
        tc = Pc[(j + 1) % len(Pc)] - Pc[j]; tc /= np.linalg.norm(tc)
        q = np.cross(n0, tc)
        if q[2] < 0: q = -q
        q /= np.linalg.norm(q)
        Pt = P0 + s * q
        h = b.ray_cast(Vector((Pt - 4 * n0).tolist()), Vector(n0.tolist()))
        if h[0] is None: fails += 1000; continue
        n = -np.array(h[1])  # outward
        if n.dot(n0) < 0: n = -n
        Pin = np.array(h[0][:])
        h2 = b.ray_cast(Vector((Pin + n * 0.01).tolist()), Vector(n.tolist()))
        Pout = np.array(h2[0][:]) if h2[0] is not None else None
        dots.append(dict(c=float(c), s=float(s), d=float(d), pin=Pin.tolist(), n=n.tolist(),
                         pout=Pout.tolist() if Pout is not None else None,
                         wall=float(np.linalg.norm(Pout - Pin)) if Pout is not None else None,
                         phase=float(rng.random() * 2 * math.pi / cfg['seg'])))
    return dict(zc=zc, L=float(L), pitch=float(p), dots=dots, fails=fails)


def _R(d, seg=16): return d / 2 / math.cos(math.pi / seg)   # circumradius of the 16-gon cutter (flats >= nominal)


def gen_dots(b, cfg=LED, zc=None, contour=None):
    """rows version. Same RNG scheme as v1 (default_rng(seed); per LED sizes sorted desc), 5 dots per LED cell, rows
    chosen per cell as a shuffled (-1,-1,0,0,+1)-style multiset 2/2/1 (single row random), c uniform in the cell,
    s = row*row_pitch + U(-jit_s, jit_s); dart-thrown against the 2D metric with need = R_i + R_j + web_target."""
    target = cfg['n_led'] * cfg['pitch']
    if zc is None: zc = solve_zc(b, target)
    Pc, Nc = contour if contour is not None else inner_contour(b, zc, 7200)
    seg = np.linalg.norm(np.diff(np.vstack([Pc, Pc[:1]]), axis=0), axis=1)
    cum = np.concatenate([[0], np.cumsum(seg)]); L = cum[-1]
    p = L / cfg['n_led']
    rng = np.random.default_rng(cfg['seed'])
    nr = cfg['rows']; rr = [(k - (nr - 1) / 2) * cfg['row_pitch'] for k in range(nr)]
    cs = []; ss = []; ds = []; fails = 0
    for i in range(cfg['n_led']):
        sizes = sorted(rng.choice(cfg['d_choices'], cfg['k_per_led']), reverse=True)
        single = int(rng.integers(0, 3)); rows = [0, 1, 2] + [r for r in (0, 1, 2) if r != single]   # 2/2/1, single row random
        rows = list(rng.permutation(rows))
        for d, r in zip(sizes, rows):
            for it in range(12000):
                # first 3000 tries: own cell + own row; then neighbouring-cell spill (+-0.5 cell) and any row (keeps 5/LED on average)
                if it < 3000: c = (i + rng.random()) * p; rw = rr[r]
                else: c = (i + (rng.uniform(-0.5, 1.5) if it < 6000 else rng.uniform(-2.0, 3.0))) * p; rw = rr[int(rng.integers(0, nr))]
                c = c % L; s = rw + rng.uniform(-cfg['jit_s'], cfg['jit_s'])
                if cs:
                    dc = np.abs(np.array(cs) - c); dc = np.minimum(dc, L - dc)
                    need = _R(d) + _R(np.array(ds)) + cfg['web_target']
                    if (np.hypot(dc, np.array(ss) - s) < need).any(): continue
                cs.append(c); ss.append(s); ds.append(float(d)); break
            else:
                fails += 1
    return _dots_from(b, cfg, zc, Pc, Nc, cum, seg, L, p, np.array(cs), np.array(ss), np.array(ds), rng, fails)


def _dots_from(b, cfg, zc, Pc, Nc, cum, seg, L, p, cs, ss, ds, rng, fails):
    dots = []
    for c, s, d in zip(cs, ss, ds):
        j = int(np.searchsorted(cum, c) - 1) % len(Pc); f = (c - cum[j]) / seg[j]
        P0 = Pc[j] * (1 - f) + Pc[(j + 1) % len(Pc)] * f
        n0 = Nc[j] * (1 - f) + Nc[(j + 1) % len(Pc)] * f; n0 /= np.linalg.norm(n0)
        tc = Pc[(j + 1) % len(Pc)] - Pc[j]; tc /= np.linalg.norm(tc)
        q = np.cross(n0, tc)
        if q[2] < 0: q = -q
        q /= np.linalg.norm(q)
        Pt = P0 + s * q
        h = b.ray_cast(Vector((Pt - 4 * n0).tolist()), Vector(n0.tolist()))
        if h[0] is None: fails += 1000; continue
        n = -np.array(h[1])
        if n.dot(n0) < 0: n = -n
        Pin = np.array(h[0][:])
        h2 = b.ray_cast(Vector((Pin + n * 0.01).tolist()), Vector(n.tolist()))
        Pout = np.array(h2[0][:]) if h2[0] is not None else None
        dots.append(dict(c=float(c), s=float(s), d=float(d), pin=Pin.tolist(), n=n.tolist(),
                         pout=Pout.tolist() if Pout is not None else None,
                         wall=float(np.linalg.norm(Pout - Pin)) if Pout is not None else None,
                         phase=float(rng.random() * 2 * math.pi / cfg['seg'])))
    return dict(zc=zc, L=float(L), pitch=float(p), dots=dots, fails=fails)


def min_web(dots, seg=16, wall=2.4, reach=6.0, nt=13):
    """3D hole-to-hole web: min over neighbour pairs of (distance between the two axis segments (inner skin -> outer skin)
    - circumradius_i - circumradius_j). Conservative: circumradius, i.e. polygon vertices. Returns (min_web, worst_pair, n_pairs<0.6+0.05)"""
    P = np.array([d['pin'] for d in dots]); N = np.array([d['n'] for d in dots]); D = np.array([d['d'] for d in dots])
    t = np.linspace(0, wall, nt); best = (9e9, None); low = 0
    for i in range(len(P)):
        dd = np.linalg.norm(P - P[i], axis=1); nb = np.where((dd < reach) & (np.arange(len(P)) > i))[0]
        A = P[i][None] + t[:, None] * N[i][None]
        for j in nb:
            B = P[j][None] + t[:, None] * N[j][None]
            m = np.linalg.norm(A[:, None, :] - B[None, :, :], axis=2).min()
            w = m - _R(D[i], seg) - _R(D[j], seg)
            if w < best[0]: best = (float(w), (i, int(j)))
            if w < 0.65: low += 1
    return best[0], best[1], low


def cutter_mesh(name, dots, centre, cfg=LED):
    bm = bmesh.new(); S = cfg['seg']
    for dt in dots:
        n = np.array(dt['n']); P = np.array(dt['pin'])
        ref = np.array([0, 0, 1.0]) if abs(n[2]) < 0.9 else np.array([1.0, 0, 0])
        u = np.cross(n, ref); u /= np.linalg.norm(u); v = np.cross(n, u)
        R = dt['d'] / 2 / math.cos(math.pi / S)
        rings = []
        for t in (-cfg['cut_in'], cfg['cut_out']):
            ring = []
            for k in range(S):
                a = dt['phase'] + 2 * math.pi * k / S
                pt = P + n * t + R * (math.cos(a) * u + math.sin(a) * v) - np.array(centre)
                ring.append(bm.verts.new(pt.tolist()))
            rings.append(ring)
        bm.faces.new(rings[0][::-1]); bm.faces.new(rings[1])
        for k in range(S):
            j = (k + 1) % S
            bm.faces.new((rings[0][k], rings[0][j], rings[1][j], rings[1][k]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    return me


def create_led_features(b=None, cfg=LED, coll='FEATURES_4.2_CUT', g=None, phase_fix=None):
    """(re)creates library object dome_cut_led_dots from the live dome surface. Returns generator result."""
    if b is None: b = _bvh_obj(bpy.data.objects['neptr_top_dome_4.2'])
    if g is None: g = gen_dots(b, cfg)
    for i, ph in (phase_fix or {}).items(): g['dots'][int(i)]['phase'] = ph
    centre = (0.0, 0.0, round(g['zc'], 3))
    name = 'dome_cut_led_dots'
    old = bpy.data.objects.get(name)
    if old: bpy.data.objects.remove(old, do_unlink=True)
    o = bpy.data.objects.new(name, cutter_mesh(name, g['dots'], centre, cfg)); o.location = centre
    bpy.data.collections[coll].objects.link(o); o.hide_viewport = True; o.hide_render = True
    spec = dict(kind='led_dots', n_led=cfg['n_led'], led_pitch_mm=cfg['pitch'], contour_pitch_mm=g['pitch'], zc_inner=g['zc'],
                contour_len=g['L'], seed=cfg['seed'], k_per_led=cfg['k_per_led'], band_half=cfg['band_half'],
                d_choices=list(cfg['d_choices']), gap_min=cfg['gap_min'], rows=cfg.get('rows'), row_pitch=cfg.get('row_pitch'), jit_s=cfg.get('jit_s'), web_min=cfg.get('web_min'), web_target=cfg.get('web_target'), cut_in=cfg['cut_in'], cut_out=cfg['cut_out'],
                seg=cfg['seg'], n_dots=len(g['dots']), phase_fix={str(k): v for k, v in (phase_fix or {}).items()}, start=cfg['start'],
                axis='local inner-surface normal', hardware='WS2812B 5050 144/m strip, 10 wide, glued on inner shoulder, LEDs facing wall (ASSUMED mount)')
    for k, v in dict(part='dome', op='cut', order=cfg['order'], batch=cfg['batch'], group='led', spec=json.dumps(spec),
                     centre=json.dumps(list(centre)), applied=False, split_z=cfg['split_z']).items(): o[k] = v
    return g
