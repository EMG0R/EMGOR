"""v5.2 engraved logo: blocky angular capitals as stroke polylines, V-carved with convex wedge tools (diamond terminals), subtracted from the shell mesh.
Use:  python hypertrumpet_v5_logo.py   (reads stl_v4/shell_pebble_v4.stl, writes the same file + logo_info_v5.json)"""
import os, sys, json, math, numpy as np, trimesh
from scipy import ndimage as ndi
import manifold3d as mf
HERE = os.path.dirname(os.path.abspath(__file__)); FACE_Y = 568.4
# glyphs: polylines in a 0.7 x 1.0 box (x right, z up); chamfered/angular, no curves
G = {
 "T": [[(0, 1), (0.7, 1)], [(0.35, 1), (0.35, 0)]],
 "H": [[(0, 0), (0, 1)], [(0.7, 0), (0.7, 1)], [(0, 0.5), (0.7, 0.5)]],
 "E": [[(0.7, 1), (0, 1), (0, 0), (0.7, 0)], [(0, 0.5), (0.5, 0.5)]],
 "Y": [[(0, 1), (0.35, 0.5), (0.7, 1)], [(0.35, 0.5), (0.35, 0)]],
 "P": [[(0, 0), (0, 1), (0.52, 1), (0.7, 0.82), (0.7, 0.68), (0.52, 0.5), (0, 0.5)]],
 "R": [[(0, 0), (0, 1), (0.52, 1), (0.7, 0.82), (0.7, 0.68), (0.52, 0.5), (0, 0.5)], [(0.3, 0.5), (0.7, 0)]],
 "U": [[(0, 1), (0, 0.15), (0.15, 0), (0.55, 0), (0.7, 0.15), (0.7, 1)]],
 "M": [[(0, 0), (0, 1), (0.35, 0.45), (0.7, 1), (0.7, 0)]],
 "G": [[(0.7, 0.85), (0.55, 1), (0.15, 1), (0, 0.85), (0, 0.15), (0.15, 0), (0.7, 0), (0.7, 0.45), (0.4, 0.45)]],
 "O": [[(0.15, 1), (0.55, 1), (0.7, 0.85), (0.7, 0.15), (0.55, 0), (0.15, 0), (0, 0.15), (0, 0.85), (0.15, 1)]],
}
GW, GAP_L = 0.64, 0.2     # glyph width, letter gap (in units of glyph height)
SMALL, LINEGAP = 0.47, 0.5

def layout(Hbig):
    """returns list of (char, x_left, z_bottom, glyph_height) with block centred at (0,0); also block w,h"""
    Hs = SMALL * Hbig; lines = [("THE", Hs), ("HYPERTRUMPET", Hbig), ("EMGOR", Hs)]
    gap = LINEGAP * Hbig; Hb = Hs + gap + Hbig + gap + Hs
    z = Hb / 2; out = []; Wmax = 0
    for txt, H in lines:
        z -= H; w = len(txt) * GW * H + (len(txt) - 1) * GAP_L * H; Wmax = max(Wmax, w); x = -w / 2
        for ch in txt: out.append((ch, x, z, H)); x += (GW + GAP_L) * H
        z -= gap
    return out, Wmax, Hb

def tools(glyphs, cx, cz, hw_big, depth):
    res = []
    for ch, x0, z0, H in glyphs:
        hw = hw_big if H >= 4.0 else 0.4; depth = 1.35 if H >= 4.0 else 1.1
        for poly in G[ch]:
            P = [np.array([cx - (x0 + p[0] * H * (GW / 0.7)), cz + z0 + p[1] * H]) for p in poly]      # mirrored in x so it reads correctly for a viewer looking at the +y face
            for a, b in zip(P[:-1], P[1:]):
                L = np.linalg.norm(b - a); d = (b - a) / L; n = np.array([-d[1], d[0]]); e = min(hw, L / 2.2)
                top = FACE_Y + 0.6; k = 0.6 / depth; hwt = hw * (1 + k)               # keep the V slope above the surface
                pts = []
                for sgn in (-1, 1):
                    for end in (a, b):
                        q = end + n * sgn * hwt; pts.append([q[0], top, q[1]])           # top rectangle corners (full length)
                for end in (a + d * e, b - d * e):
                    pts.append([end[0], FACE_Y - depth, end[1]])                       # apex line, shortened by e: pyramidal (diamond) terminals
                h = trimesh.convex.convex_hull(np.array(pts)); res.append(h)
    return res

def free_region(shell, x_lim=(728, 806), z_lim=(-12, 48), margin=6.0, step=0.5):
    xs = np.arange(x_lim[0], x_lim[1], step); zs = np.arange(z_lim[0], z_lim[1], step); X, Z = np.meshgrid(xs, zs, indexing="ij")
    o = np.c_[X.ravel(), np.full(X.size, 620.0), Z.ravel()]; loc, ri, _ = shell.ray.intersects_location(o, np.tile([0, -1.0, 0], (len(o), 1)), multiple_hits=False)
    y = np.full(len(o), np.nan); y[ri] = loc[:, 1]; Y = y.reshape(X.shape); flat = np.abs(Y - FACE_Y) < 0.12
    e = ndi.distance_transform_edt(np.pad(flat, 1, constant_values=False))[1:-1, 1:-1] * step >= margin
    return xs, zs, e

def best_block(xs, zs, ok, step=0.5):
    S = ok.astype(np.int32).cumsum(0).cumsum(1); S = np.pad(S, ((1, 0), (1, 0)))
    best = None
    for Hbig in np.arange(9.0, 2.5, -0.1):
        _, W, Hb = layout(Hbig); nw, nh = int(math.ceil(W / step)), int(math.ceil(Hb / step))
        if nw >= ok.shape[0] or nh >= ok.shape[1]: continue
        cnt = S[nw:, nh:] - S[:-nw, nh:] - S[nw:, :-nh] + S[:-nw, :-nh]
        full = np.argwhere(cnt == nw * nh)
        if len(full): i, j = full[len(full) // 2]; best = (Hbig, W, Hb, xs[i] + W / 2, zs[j] + Hb / 2); break
    return best

if __name__ == "__main__":
    p = os.path.join(HERE, "stl_v4", "shell_pebble_v4.stl"); shell = trimesh.load(p, force="mesh")
    xs, zs, ok = free_region(shell); b = best_block(xs, zs, ok)
    if b is None: print("no region"); sys.exit(1)
    Hbig, W, Hb, cx, cz = b; print("block", round(W, 1), "x", round(Hb, 1), "cap height", round(Hbig, 2), "centre", round(cx, 1), round(cz, 1))
    glyphs, _, _ = layout(Hbig); hw = 0.5; depth = 1.35
    T = tools(glyphs, cx, cz, hw, depth)
    # thicken the wall locally from the inside (boss under the block), then carve
    boss = trimesh.creation.box(extents=[W + 4, 3.8, Hb + 4]); boss.apply_translation([cx, 563.5 + 1.9, cz])
    from hypertrumpet_v2_build import to_manifold, from_manifold
    base = to_manifold(shell); bm = to_manifold(boss); res = base + bm
    tm = [to_manifold(t) for t in T]
    cut = mf.Manifold.batch_boolean(tm, mf.OpType.Add); res = res - cut
    m = from_manifold(res); m.merge_vertices(); m = max(m.split(only_watertight=False), key=lambda q: len(q.faces)); print("watertight", m.is_watertight, "bodies", len(m.split(only_watertight=False)))
    m.export(p); info = dict(face="control face (+y), lower-left over the Daisy Seed housing", block_mm=[round(W, 1), round(Hb, 1)], big_cap_height_mm=round(Hbig, 2), small_cap_height_mm=round(Hbig * SMALL, 2), centre_xz=[round(cx, 1), round(cz, 1)], margin_mm=6.0, depth_mm=depth, stroke_half_width_mm=hw, wall_under_deepest_mm=round(568.4 - depth - 563.5, 2), faces=int(len(m.faces)), watertight=bool(m.is_watertight), euler=int(m.euler_number))
    json.dump(info, open(os.path.join(HERE, "logo_info_v5.json"), "w"), indent=1); print(info)
