"""NEPTR 4.2 housing -- analytic outer form + structured mesh generator.

Reproduces dome + cap bases from NEPTR_4.2_form_params.json (numpy + scipy only).
Run:  python3 NEPTR_4.2_form.py [params.json] [out.npz]

FORM
  plan outline C0: closed, mirror-symmetric in x. Half curve = flat top (0..x1), a curvature-
    spline curve (kappa = cubic B-spline in arclength, so G3), flat bottom. Built by integrating
    the tangent angle: phi' = kappa.
  inset profile: quintic Bezier d(z) (D at the flat bottom/top, 0 on the vertical wall), zero
    curvature at both ends (G2 into the flats), mirrored about z = 31.
  outer skin  P(S,q) = C0(S) + rho(q)*n(S) + z(q) ez      (exact parallel offset of C0 by d = -rho)
  inner skin  P_in   = P - wall * N3                       (N3 = exact analytic unit normal)
MESH
  rows = profile nodes, columns = C0 nodes; node spacing from a max turning angle per step (dphi_deg)
  so the dihedral between neighbouring faces is <= dphi_deg on the skin, capped by hmax.
  Flat top/bottom/floor/ceiling = Delaunay fill of the ring + interior lattice.
"""
import json, sys
import numpy as np
from scipy.spatial import Delaunay, cKDTree


# ----------------------------------------------------------------------------- B-spline
def bspline_eval(t, c, deg, x):
    """Cox-de Boor, vectorised in x. t clamped knot vector, returns sum c_i B_i(x)."""
    x = np.asarray(x, float)
    n = len(c)
    xx = np.clip(x, t[0], t[-1] - 1e-12)
    B = np.zeros((len(x), len(t) - 1))
    for i in range(len(t) - 1):
        B[:, i] = (xx >= t[i]) & (xx < t[i + 1])
    for k in range(1, deg + 1):
        Bn = np.zeros((len(x), len(t) - 1 - k))
        for i in range(len(t) - 1 - k):
            a = 0.0
            d1 = t[i + k] - t[i]
            d2 = t[i + k + 1] - t[i + 1]
            term = np.zeros(len(x))
            if d1 > 0:
                term += (xx - t[i]) / d1 * B[:, i]
            if d2 > 0:
                term += (t[i + k + 1] - xx) / d2 * B[:, i + 1]
            Bn[:, i] = term
        B = Bn
    return B[:, :n] @ np.asarray(c, float)


def cumtrapz0(y, x):
    return np.r_[0.0, np.cumsum((y[1:] + y[:-1]) * 0.5 * np.diff(x))]


# ----------------------------------------------------------------------------- form
class Form:
    def __init__(self, J):
        self.J = J
        self.D = J["D"]
        self.H = J["height"]
        self.wall = J["wall"]
        self.seam = J["seam_z"]
        self.floor = J["floor"]
        self._outline()
        self._profile()

    # ---- plan outline (half curve, dense table) ----
    def _outline(self, ds=0.004):
        c = self.J["c0"]
        K, deg, L, x1, yT = c["K"], c["deg"], c["L"], c["x1"], c["yT"]
        coefs = np.asarray(c["coefs_full"], float)
        t = np.r_[[0.0] * deg, np.linspace(0, 1, K - deg + 1), [1.0] * deg] * L
        s = np.linspace(0, L, int(L / ds) + 1)
        kap = bspline_eval(t, coefs, deg, s)
        phi = cumtrapz0(kap, s)
        x = x1 + cumtrapz0(np.cos(phi), s)
        y = yT + cumtrapz0(np.sin(phi), s)
        xe, ye, phie = x[-1], y[-1], phi[-1]
        # closure: heading must be exactly -pi at the end; rotate residual away is NOT done (fit enforces it)
        self.phi_end_err = float(phie + np.pi)
        # flats
        n1 = int(x1 / ds) + 1
        n2 = int(xe / ds) + 1
        sx = np.r_[np.linspace(0, x1, n1)[:-1], x1 + s[:-1], x1 + L + np.linspace(0, xe, n2)]
        X = np.r_[np.linspace(0, x1, n1)[:-1], x[:-1], np.linspace(xe, 0, n2)]
        Y = np.r_[np.full(n1 - 1, yT), y[:-1], np.full(n2, ye)]
        PH = np.r_[np.zeros(n1 - 1), phi[:-1], np.full(n2, -np.pi)]
        KA = np.r_[np.zeros(n1 - 1), kap[:-1], np.zeros(n2)]
        self.hs, self.hx, self.hy, self.hphi, self.hk = sx, X, Y, PH, KA   # half-curve table, hs in [0, Lh]
        self.Lh = sx[-1]
        self.x1, self.xe, self.yT, self.ye = x1, xe, yT, ye

    def u_nodes(self, dphi_deg=1.5, hmax=1.2):
        """C0 half nodes (top centre -> bottom centre), returns arrays hx, hy, nx, ny, kappa, S; both ends included."""
        dphi = np.radians(dphi_deg)
        h = 1.0 / np.sqrt(1.0 / hmax ** 2 + (self.hk / dphi) ** 2)
        n = cumtrapz0(1.0 / h, self.hs)
        M = int(np.ceil(n[-1]))
        if M % 2: M += 1
        targets = np.linspace(0, n[-1], M + 1)
        S = np.interp(targets, n, self.hs)
        X = np.interp(S, self.hs, self.hx)
        Y = np.interp(S, self.hs, self.hy)
        PH = np.interp(S, self.hs, self.hphi)
        KA = np.interp(S, self.hs, self.hk)
        # full ring: right half 0..M then mirrored M-1..1
        Xf = np.r_[X, -X[-2:0:-1]]
        Yf = np.r_[Y, Y[-2:0:-1]]
        # outward normal = left of heading: (-sin phi, cos phi); mirror flips x
        NX = -np.sin(PH); NY = np.cos(PH)
        NXf = np.r_[NX, -NX[-2:0:-1]]
        NYf = np.r_[NY, NY[-2:0:-1]]
        KAf = np.r_[KA, KA[-2:0:-1]]
        return Xf, Yf, NXf, NYf, KAf

    # ---- profile (rho outward-positive, z) ----
    def _profile(self):
        p = self.J["profile"]; D = self.D; H = self.H
        a1, a2, h, c1, c2 = p["a1"], p["a2"], p["h"], p["c1"], p["c2"]
        C = np.array([[D, 0], [D - a1, 0], [D - a2, 0], [0, h - c2], [0, h - c1], [0, h]], float)
        t = np.linspace(0, 1, 6001)
        from math import comb
        B = np.array([comb(5, k) * t ** k * (1 - t) ** (5 - k) for k in range(6)]).T
        pts = B @ C
        d = pts[:, 0]; z = pts[:, 1]
        # full path bottom edge -> top edge, in (rho=-d, z)
        lo = np.c_[-d, z]
        vert = np.array([[0.0, h], [0.0, H - h]])
        up = np.c_[-d, H - z][::-1]
        P = np.vstack([lo, vert[1:-1] if False else vert, up])
        # dedupe consecutive identical
        keep = np.r_[True, np.hypot(*np.diff(P, axis=0).T) > 1e-9]
        P = P[keep]
        # resample densely by arclength (vertical gets plenty of points)
        q = np.r_[0, np.cumsum(np.hypot(*np.diff(P, axis=0).T))]
        qq = np.linspace(0, q[-1], int(q[-1] / 0.01) + 1)
        rho = np.interp(qq, q, P[:, 0]); zz = np.interp(qq, q, P[:, 1])
        self.pq, self.prho, self.pz = qq, rho, zz
        self._profile_derived()

    def _profile_derived(self):
        q, rho, z = self.pq, self.prho, self.pz
        # smooth tangent via central differences
        tr = np.gradient(rho, q); tz = np.gradient(z, q)
        nrm = np.hypot(tr, tz); tr /= nrm; tz /= nrm
        self.ptr, self.ptz = tr, tz
        self.pNr, self.pNz = tz, -tr           # outward normal (rho, z)
        self.pang = np.unwrap(np.arctan2(tz, tr))   # tangent angle psi
        self.pcurv = np.gradient(self.pang, q)
        # inner offset
        w = self.wall
        self.irho = rho - w * self.pNr
        self.iz = z - w * self.pNz

    def profile_nodes(self, which, z_start, z_end, dpsi_deg=1.5, hmax=1.2, hit=None):
        """Node indices (float q values) along profile 'outer' or 'inner' between heights (inclusive).
        z_start/z_end may be None => profile ends. Rows are placed so the turning per step <= dpsi."""
        rho, z = (self.prho, self.pz) if which == "outer" else (self.irho, self.iz)
        q = self.pq
        # arclength of the chosen polyline
        qa = np.r_[0, np.cumsum(np.hypot(np.diff(rho), np.diff(z)))]
        ang = self.pang
        curv = np.gradient(ang, qa)
        # z monotone => locate by interpolation
        zq = lambda zv: float(np.interp(zv, z, qa))
        a0 = 0.0 if z_start is None else zq(z_start)
        a1 = qa[-1] if z_end is None else zq(z_end)
        dpsi = np.radians(dpsi_deg)
        hh = 1.0 / np.sqrt(1.0 / hmax ** 2 + (curv / dpsi) ** 2)
        m = (qa >= a0 - 1e-9) & (qa <= a1 + 1e-9)
        n = cumtrapz0(1.0 / hh[m], qa[m])
        M = max(int(np.ceil(n[-1])), 1)
        tg = np.linspace(0, n[-1], M + 1)
        aq = np.interp(tg, n, qa[m])
        aq[0] = a0; aq[-1] = a1
        return aq, qa, rho, z

    def profile_at(self, aq, qa, rho, z):
        return np.interp(aq, qa, rho), np.interp(aq, qa, z)


# ----------------------------------------------------------------------------- mesh helpers
def pip(poly, pts):
    """even-odd point in polygon (numpy)."""
    x, y = pts[:, 0], pts[:, 1]
    inside = np.zeros(len(pts), bool)
    n = len(poly)
    for i in range(n):
        x1, y1 = poly[i]; x2, y2 = poly[(i + 1) % n]
        c = ((y1 > y) != (y2 > y))
        with np.errstate(divide="ignore", invalid="ignore"):
            xi = (x2 - x1) * (y - y1) / (y2 - y1) + x1
        inside ^= c & (x < xi)
    return inside


def flat_fill(ring_xy, hint=1.6, margin=0.9):
    """Triangulate the polygon ring_xy (N,2, ccw or cw) with interior lattice. Returns (pts, tris) where
    the first N pts are the ring. Orientation of tris: ccw (normal +z)."""
    N = len(ring_xy)
    xmin, ymin = ring_xy.min(0); xmax, ymax = ring_xy.max(0)
    gx = np.arange(xmin, xmax, hint)
    gy = np.arange(ymin, ymax, hint * 0.866)
    G = []
    for j, yv in enumerate(gy):
        xs = gx + (hint * 0.5 if j % 2 else 0)
        G.append(np.c_[xs, np.full(len(xs), yv)])
    G = np.vstack(G)
    ins = pip(ring_xy, G)
    # distance to boundary (dense sampling of the ring)
    seg = np.concatenate([np.linspace(ring_xy[i], ring_xy[(i + 1) % N], 5, endpoint=False) for i in range(N)])
    dist, _ = cKDTree(seg).query(G)
    G = G[ins & (dist > margin)]
    pts = np.vstack([ring_xy, G])
    tri = Delaunay(pts).simplices
    cen = pts[tri].mean(1)
    tri = tri[pip(ring_xy, cen)]
    # orient ccw
    a = pts[tri]
    cr = (a[:, 1, 0] - a[:, 0, 0]) * (a[:, 2, 1] - a[:, 0, 1]) - (a[:, 1, 1] - a[:, 0, 1]) * (a[:, 2, 0] - a[:, 0, 0])
    tri[cr < 0] = tri[cr < 0][:, ::-1]
    cr = np.abs(cr)
    keep = cr > 1e-9
    return pts, tri[keep]


def build(J, log=print):
    F = Form(J)
    cfg = J["mesh"]
    X, Y, NX, NY, KA = F.u_nodes(cfg["dphi_deg"], cfg["hmax"])
    N = len(X)
    log("u nodes", N)
    seam = F.seam; W = F.wall; H = F.H; fl = F.floor

    def ring(rho, z):
        return np.c_[X + rho * NX, Y + rho * NY, np.full(N, z)]

    # outer rows
    oq_lo, oqa, orho, oz = F.profile_nodes("outer", None, seam, cfg["dphi_deg"], cfg["hmax"])
    oq_hi, _, _, _ = F.profile_nodes("outer", seam, None, cfg["dphi_deg"], cfg["hmax"])
    iq_lo, iqa, irho, iz = F.profile_nodes("inner", None, seam, cfg["dphi_deg"], cfg["hmax"])
    iq_hi, _, _, _ = F.profile_nodes("inner", seam, None, cfg["dphi_deg"], cfg["hmax"])

    def rows(aq, qa, rho, z):
        r, zz = F.profile_at(aq, qa, rho, z)
        return np.stack([ring(r[i], zz[i]) for i in range(len(aq))])   # (R,N,3)

    G = {}
    G["out_lo"] = rows(oq_lo, oqa, orho, oz)
    G["out_hi"] = rows(oq_hi, oqa, orho, oz)
    G["in_lo"] = rows(iq_lo, iqa, irho, iz)
    G["in_hi"] = rows(iq_hi, iqa, irho, iz)
    # flat polygon
    flat_ring = ring(-F.D, 0.0)[:, :2]
    pts2, tri2 = flat_fill(flat_ring, cfg["interior_h"])
    G["flat_pts"] = pts2
    G["flat_tris"] = tri2
    G["N"] = N
    G["nodes"] = np.c_[X, Y, NX, NY, KA]
    G["F"] = F
    return G


if __name__ == "__main__":
    pj = sys.argv[1] if len(sys.argv) > 1 else "NEPTR_4.2_form_params.json"
    J = json.load(open(pj))
    G = build(J)
    for k in ("out_lo", "out_hi", "in_lo", "in_hi"):
        print(k, G[k].shape)
    print("flat", G["flat_pts"].shape, G["flat_tris"].shape)


# ----------------------------------------------------------------------------- assemble closed parts
def _grid_faces(base, R, N, flip=False):
    """quad faces between consecutive rows of a (R,N) vertex grid starting at index 'base'. Rows run up,
    columns run clockwise (seen from +z): [i,j],[i+1,j],[i+1,j+1],[i,j+1] gives the outward normal."""
    i = np.arange(R - 1)[:, None]; j = np.arange(N)[None, :]
    a = base + i * N + j; b = base + (i + 1) * N + j
    c = base + (i + 1) * N + (j + 1) % N; d = base + i * N + (j + 1) % N
    q = np.stack([a, b, c, d], -1).reshape(-1, 4)
    return q[:, ::-1] if flip else q


def assemble(G, part):
    """part = 'dome' or 'cap'. returns V (n,3 float), quads (m,4), tris (k,3), closed outward-facing."""
    N = G["N"]; F = G["F"]; seam = F.seam; H = F.H; fl = F.floor; W = F.wall
    pts2 = G["flat_pts"]; tri2 = G["flat_tris"]
    nint = len(pts2) - N
    V = []; off = 0
    def add(a):
        nonlocal off
        V.append(a.reshape(-1, 3)); s = off; off += len(a.reshape(-1, 3)); return s
    quads = []; tris = []
    if part == "dome":
        O = G["out_hi"]; I = G["in_hi"]
        so = add(O); si = add(I)
        quads.append(_grid_faces(so, len(O), N)); quads.append(_grid_faces(si, len(I), N, flip=True))
        top_ring = so + (len(O) - 1) * N + np.arange(N)
        cei_ring = si + (len(I) - 1) * N + np.arange(N)
        zt = O[-1, 0, 2]; zc = I[-1, 0, 2]
        ti = add(np.c_[pts2[N:], np.full(nint, zt)])
        ci = add(np.c_[pts2[N:], np.full(nint, zc)])
        def mapidx(ring, inter):
            m = np.r_[ring, inter + np.arange(nint)]
            return m[tri2]
        tris.append(mapidx(top_ring, ti))
        tris.append(mapidx(cei_ring, ci)[:, ::-1])
        # rim annulus (faces down): outer ring row 0 and inner ring row 0
        ro = so + np.arange(N); ri = si + np.arange(N)
        jn = (np.arange(N) + 1) % N
        # outer ring clockwise; for -z normal use [ro_j, ro_j+1, ri_j+1, ri_j]? decide by sign check below
        quads.append(np.stack([ro, ro[jn], ri[jn], ri], -1))
    else:
        O = G["out_lo"]; I = G["in_lo"]
        so = add(O); si = add(I)
        quads.append(_grid_faces(so, len(O), N)); quads.append(_grid_faces(si, len(I), N, flip=True))
        bot_ring = so + np.arange(N)
        flo_ring = si + np.arange(N)
        zb = O[0, 0, 2]; zf = I[0, 0, 2]
        bi = add(np.c_[pts2[N:], np.full(nint, zb)])
        fi = add(np.c_[pts2[N:], np.full(nint, zf)])
        def mapidx(ring, inter):
            m = np.r_[ring, inter + np.arange(nint)]
            return m[tri2]
        tris.append(mapidx(bot_ring, bi)[:, ::-1])    # bottom faces -z
        tris.append(mapidx(flo_ring, fi))              # floor top faces +z
        ro = so + (len(O) - 1) * N + np.arange(N); ri = si + (len(I) - 1) * N + np.arange(N)
        jn = (np.arange(N) + 1) % N
        quads.append(np.stack([ro, ro[jn], ri[jn], ri], -1)[:, ::-1])   # faces +z
    V = np.vstack(V)
    Q = np.vstack(quads); T = np.vstack(tris)
    # fix rim orientation by signed volume: compute and, if the rim quads are inverted, flip them
    return V, Q, T


def signed_volume(V, Q, T):
    tri = np.vstack([T, Q[:, [0, 1, 2]], Q[:, [0, 2, 3]]])
    a, b, c = V[tri[:, 0]], V[tri[:, 1]], V[tri[:, 2]]
    return float(np.einsum("ij,ij->i", a, np.cross(b, c)).sum() / 6.0)


def topology(V, Q, T):
    """boundary / non-manifold edge counts."""
    E = np.vstack([Q[:, [0, 1]], Q[:, [1, 2]], Q[:, [2, 3]], Q[:, [3, 0]], T[:, [0, 1]], T[:, [1, 2]], T[:, [2, 0]]])
    key = np.sort(E, 1)
    k = key[:, 0].astype(np.int64) * len(V) + key[:, 1]
    u, c = np.unique(k, return_counts=True)
    # directed consistency: each undirected edge must appear once in each direction
    kd = E[:, 0].astype(np.int64) * len(V) + E[:, 1]
    ud, cd = np.unique(kd, return_counts=True)
    return {"edges": len(u), "boundary(1)": int((c == 1).sum()), "nonmanifold(>2)": int((c > 2).sum()),
            "dup_directed": int((cd > 1).sum())}


# ----------------------------------------------------------------------------- envelope (solid filled body, optionally inset)
def envelope(G, inset=0.0, z_from=10.0, cfg=None):
    """closed SOLID of the outer skin from z_from up to the top, offset inward by 'inset' along the analytic
    normal. Used to clip fastener blocks so they never poke through the skin."""
    F = G["F"]; N = G["N"]
    X, Y, NX, NY = G["nodes"][:, 0], G["nodes"][:, 1], G["nodes"][:, 2], G["nodes"][:, 3]
    cfg = cfg or F.J["mesh"]
    aq, qa, rho, z = F.profile_nodes("outer", z_from, None, cfg["dphi_deg"], cfg["hmax"])
    r = np.interp(aq, qa, rho); zz = np.interp(aq, qa, z)
    # offset along the profile normal
    Nr = np.interp(aq, qa, F.pNr); Nz = np.interp(aq, qa, F.pNz)
    r = r - inset * Nr; zz = zz - inset * Nz
    rows = np.stack([np.c_[X + r[i] * NX, Y + r[i] * NY, np.full(N, zz[i])] for i in range(len(aq))])
    pts2, tri2 = flat_fill(rows[-1][:, :2], 3.0)
    # bottom polygon at row 0
    bpts, btri = flat_fill(rows[0][:, :2], 3.0)
    V = []; off = 0
    def add(a):
        nonlocal off
        a = a.reshape(-1, 3); V.append(a); s = off; off += len(a); return s
    so = add(rows)
    nint = len(pts2) - N
    ti = add(np.c_[pts2[N:], np.full(nint, rows[-1][0, 2])])
    nb = len(bpts) - N
    bi = add(np.c_[bpts[N:], np.full(nb, rows[0][0, 2])])
    Q = _grid_faces(so, len(rows), N)
    top_ring = so + (len(rows) - 1) * N + np.arange(N)
    t1 = np.r_[top_ring, ti + np.arange(nint)][tri2]
    bot_ring = so + np.arange(N)
    t2 = np.r_[bot_ring, bi + np.arange(nb)][btri][:, ::-1]
    return np.vstack(V), Q, np.vstack([t1, t2])
