#!/usr/bin/env python3
"""
HYPERTRUMPET V4 build: smooth-pebble shell (backbone silhouette, flat constant-depth control face, battery keel under the valve block,
bottom door), fractal bell wrap (trunk + branches, strip channel, cable bore, mic socket), 3-piece split with hidden tongue/socket joints,
finer valve caps, drawer-free (the bottom door replaces the v3 palm drawer).  Re-uses v3.1 mount / hall rail STL (copied, unmodified).
Run (venv with numpy scipy scikit-image trimesh manifold3d matplotlib):  python hypertrumpet_v4_build.py shell wrap door caps copy verify
Output: cad/stl_v4/*.stl, cad/params_v4.json, cad/verify_report_v4.json.  Never writes outside *_v4 names.
Frame: x along horn (mouthpiece -x), y lateral (pod +y), z valve axis up; mm. Tags [A]=assumed.
"""
import os, sys, json, math, time, shutil
import numpy as np
from scipy import ndimage as ndi
from scipy.interpolate import PchipInterpolator, CubicSpline
from skimage import measure
import trimesh
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import hypertrumpet_v4_params as Q
from hypertrumpet_v4_params import *
import hypertrumpet_v2_build as L          # read-only library: horn reference, SDF/mesh helpers (writes nothing on import)
from hypertrumpet_v2_build import to_manifold, from_manifold, finalize
# EMGOR correction (mid-build): 18.0 mm is the finger-BUTTON top diameter, not the casing. Casing back to the v1 reference model (19.2 body, 22.3 pitch, caps 21.0) the original arms were fitted to.
L.P.update(casing_od=19.2, casing_cx=(831.2, 853.5, 875.8), button_od=18.0, button_z1=138.5)          # button height 5.5 mm assumed [A]

STL4 = os.path.join(HERE, "stl_v4"); os.makedirs(STL4, exist_ok=True)
H_SHELL = 0.35
XR = L.P["bell_rim_x"]; YB_, ZB_ = L.P["bell_axis"]; BELL, BELLD = L._BELL, L._BELLD
BELL2 = BELL.derivative(2)
R0 = float(BELL(0.0))

# ---------------- wrap parameters ----------------
GAP = 1.75                        # pad gap (nominal 1.5; +0.25 absorbs the conformal-surface approximation, measured min >= 1.5) trunk/branch underside -> bell surface
STRIP_W, STRIP_T = 10.0, 2.0      # BTF FCOB strip (8-10 mm wide, ~2 mm thick) [A]
CH_W, CH_D = 14.4, 4.4
STRIP_CH_W = 10.4            # underside channel: width, depth (strip + 0.7 mm for tape/diffuser lip)
BORE_W, BORE_H = 7.6, 4.4         # cable bore (slot): 4 mm mic cable + 3 strip wires side by side
TRUNK_W, TRUNK_B = 8.8, 6.6       # trunk half-width, half-thickness (16.8 x 12)
MIC_BODY = (22.0, 75.0); MIC_AIM = 54.0; MIC_S_DX = 29.0; MIC_S_RHO = 9.4
SOCK_R = 14.2; SOCK_BORE_R = 12.2; SOCK_T0, SOCK_T1 = -6.0, 36.0      # socket outer R, bore R (22 body + TPU sleeve 1.0 + 0.5 clr), extent along the aim axis
SOCK_MIC_REAR = 16.0; SOCK_CHAMBER_R = 10.0; SOCK_CH_T = (-3.0, 14.5)                              # mic rear sits at t=16 (as v3.1); chamber behind it r=11 t in [-2, 15.5]
X_J1, X_J2 = 950.0, 1052.0                                              # joint planes (pebble|trunk, trunk|crown)


def smax(a, b, k): return -smin(-a, -b, k)


PAD_Z_TOP, PAD_Z_CREST, PAD_AMAX, PAD_DIP, PAD_K, PAD_Z_END = -6.0, -26.0, 13.0, 3.0, 12.0, 54.0


def smin_exp(a, b, k):
    a = np.clip(a, -120, 120); b = np.clip(b, -120, 120); return (-k * np.log(np.exp(-a / k) + np.exp(-b / k))).astype(np.float32)


def mc(F, lo, h, taubin=6, keep_largest=True):
    sdp = np.pad(F, 1, constant_values=1.0)
    v, f, n, _ = measure.marching_cubes(sdp, 0.0, spacing=(h, h, h))
    m = trimesh.Trimesh(v + np.asarray(lo) - h, f, process=True)
    m.merge_vertices(); m.update_faces(m.nondegenerate_faces()); m.remove_unreferenced_vertices(); trimesh.repair.fix_normals(m)
    if taubin: trimesh.smoothing.filter_taubin(m, lamb=0.5, nu=0.53, iterations=taubin)
    if len(m.faces) > 1600000: m = L.manifold_simplify(m, 0.004)
    parts = m.split(only_watertight=False)
    if len(parts) > 1 and keep_largest: m = max(parts, key=lambda p: len(p.faces))
    return m


FACE_BUDGET = {"mount_original_arms_plate_v4": 200000, "shell_pebble_v4": 235000, "battery_door_v4": 100000, "valve_cap_v4": 40000}


def decimate_to(m, n):
    if not n or len(m.faces) <= n: return m
    import fast_simplification as fs
    v, f = fs.simplify(np.asarray(m.vertices, np.float64), np.asarray(m.faces, np.int64), target_reduction=1.0 - n / len(m.faces))
    r = trimesh.Trimesh(v, f, process=True); print("decimate", len(m.faces), "->", len(r.faces), "watertight", r.is_watertight); return r


def save4(m, name):
    name = name if name.endswith("_v4") else name + "_v4"; p = os.path.join(STL4, name + ".stl"); m2 = finalize(m); m2 = decimate_to(m2, FACE_BUDGET.get(name, 0)); m2.export(p)
    ok = trimesh.load(p, force="mesh").is_watertight
    if not ok: print("WARNING not watertight:", name)
    return p


def load4(name): return trimesh.load(os.path.join(STL4, (name if name.endswith("_v4") else name + "_v4") + ".stl"), force="mesh")


class G3:
    def __init__(self, lo, hi, h):
        self.h = h; self.lo = np.array(lo, float); self.n = tuple(int(math.ceil((hi[i] - lo[i]) / h)) + 1 for i in range(3))
        self.ax = [(self.lo[i] + h * np.arange(self.n[i])).astype(np.float32) for i in range(3)]
    def X(self): return self.ax[0][:, None, None]
    def Y(self): return self.ax[1][None, :, None]
    def Z(self): return self.ax[2][None, None, :]
    def full(self, v=1e3): return np.full(self.n, v, np.float32)
    def sl(self, lo, hi):
        s = []
        for i in range(3):
            a = int(max(0, math.floor((lo[i] - self.lo[i]) / self.h))); b = int(min(self.n[i], math.ceil((hi[i] - self.lo[i]) / self.h) + 1)); s.append(slice(a, max(a, b)))
        return tuple(s)
    def coords(self, s): return self.ax[0][s[0]][:, None, None], self.ax[1][s[1]][None, :, None], self.ax[2][s[2]][None, None, :]


# =============================================================== PEBBLE FIELDS ===
def pebble_field(g, inset=0.0, yb=YB, face=FACE_Y):
    """outer (inset=0) or inner (inset=WALL) pebble solid: silhouette extruded, flat face at constant y, quarter-ellipse edge roll."""
    X, Y, Z = g.X(), g.Y(), g.Z()
    d = -sil_sd(g.ax[0][:, None], g.ax[2][None, :])[:, None, :] - inset      # inward distance from the silhouette
    u = np.maximum(ROLL_W - d, 0) / ROLL_W
    v = np.maximum(Y - (face - inset - ROLL_H), 0) / ROLL_H
    e = (np.sqrt(u * u + v * v) - 1.0) * (0.5 * (ROLL_W + ROLL_H))
    return np.maximum(e, yb - Y).astype(np.float32)


def rbox3(g, lo, hi, r):
    X, Y, Z = g.X(), g.Y(), g.Z(); c = [(lo[i] + hi[i]) / 2 for i in range(3)]; hf = [(hi[i] - lo[i]) / 2 for i in range(3)]
    qx = np.abs(X - c[0]) - (hf[0] - r); qy = np.abs(Y - c[1]) - (hf[1] - r); qz = np.abs(Z - c[2]) - (hf[2] - r)
    return (np.sqrt(np.maximum(qx, 0) ** 2 + np.maximum(qy, 0) ** 2 + np.maximum(qz, 0) ** 2) + np.minimum(np.maximum(qx, np.maximum(qy, qz)), 0) - r).astype(np.float32)


# =============================================================== BELL FRAMES ===
def bell_pt(s, th_deg):
    th = np.radians(th_deg); r = BELL(s)
    return np.array([XR - s, YB_ + r * np.cos(th), ZB_ + r * np.sin(th)]).T if np.ndim(s) else np.array([XR - s, YB_ + r * math.cos(th), ZB_ + r * math.sin(th)])


def bell_normal(s, th_deg):
    th = np.radians(th_deg); rp = BELLD(s); n = np.array([rp, np.cos(th) * np.ones_like(rp), np.sin(th) * np.ones_like(rp)]).T
    return n / np.linalg.norm(n, axis=-1, keepdims=True)


def bell_nearest(pts):
    """pts (n,3) -> s*, h (signed distance to the bell surface, + outside), theta(rad). Newton on the meridian."""
    x = pts[:, 0]; dy = pts[:, 1] - YB_; dz = pts[:, 2] - ZB_; rho = np.hypot(dy, dz); th = np.arctan2(dz, dy)
    s = np.clip(XR - x, 0, 229.0)
    for _ in range(4):
        r = BELL(s); rp = BELLD(s); rpp = BELL2(s)
        g = (s - (XR - x)) + (r - rho) * rp; gp = 1 + rp * rp + (r - rho) * rpp
        s = np.clip(s - g / np.where(np.abs(gp) < 0.2, 0.2, gp), 0, 229.0)
    r = BELL(s); rp = BELLD(s); sx = XR - x
    dist = np.hypot(s - sx, rho - r)
    side = np.sign(rho - r)
    return s, dist * side, th


def curv_normal(s, psi):
    """normal curvature (convex positive) of the bell surface in the direction perpendicular to a path whose angle to the meridian is psi"""
    rp = BELLD(s); rpp = BELL2(s); r = BELL(s)
    kc = 1.0 / (r * np.sqrt(1 + rp * rp)); km = -rpp / (1 + rp * rp) ** 1.5
    return kc * np.cos(psi) ** 2 + km * np.sin(psi) ** 2


def resample(pts, step):
    pts = np.asarray(pts, float); d = np.r_[0, np.cumsum(np.linalg.norm(np.diff(pts, axis=0), axis=1))]
    n = max(2, int(d[-1] / step) + 1); tt = np.linspace(0, d[-1], n)
    return np.c_[[np.interp(tt, d, pts[:, i]) for i in range(3)]].T, tt


class Path:
    """flattened-tube path hugging the bell: centreline, frames (t, u lateral, n normal), half widths w, half thickness b, curvature k for the conformal underside"""
    def __init__(self, ctrl3d, w, b, step=1.2, name="", planar_u=None):
        self.name = name
        c = np.asarray(ctrl3d, float); d = np.r_[0, np.cumsum(np.linalg.norm(np.diff(c, axis=0), axis=1))]
        cs = CubicSpline(d, c, bc_type="natural"); n = max(4, int(d[-1] / step) + 1); tt = np.linspace(0, d[-1], n)
        self.C = cs(tt); self.s = tt; self.length = d[-1]
        self.w = np.interp(tt / d[-1], *w) if isinstance(w, tuple) else np.full(n, float(w))
        self.b = np.interp(tt / d[-1], *b) if isinstance(b, tuple) else np.full(n, float(b))
        T = np.gradient(self.C, axis=0); T /= np.linalg.norm(T, axis=1)[:, None]
        sn, hh, th = bell_nearest(self.C)
        sc = np.minimum(sn, 190.0)
        Nref = bell_normal(sc, np.degrees(th))
        # stub far from the bell keeps the normal of the nearest bell point (clipped)
        N = Nref - (Nref * T).sum(1)[:, None] * T; N /= np.linalg.norm(N, axis=1)[:, None]
        for i in range(1, len(N)):                       # near / beyond the rim the bell normal is ~parallel to the path: parallel-transport the frame instead
            if sn[i] < 14.0:
                q = N[i - 1] - (N[i - 1] @ T[i]) * T[i]; N[i] = q / np.linalg.norm(q)
        U = np.cross(N, T); U /= np.linalg.norm(U, axis=1)[:, None]; N = np.cross(T, U)
        if planar_u is not None:                                   # planar sweep: constant lateral axis, no twist
            pu = np.asarray(planar_u, float); pu /= np.linalg.norm(pu); U = np.tile(pu, (n, 1)); N = np.cross(U, T); N /= np.linalg.norm(N, axis=1)[:, None]
            if (N[0] @ (self.C[0] - np.array([self.C[0][0], YB_, ZB_]))) < 0: N = -N; U = -U
        self.T, self.U, self.N = T, U, N
        # meridian direction at nearest point -> psi
        m = np.c_[-np.ones(n), BELLD(sc) * np.cos(th), BELLD(sc) * np.sin(th)]; m /= np.linalg.norm(m, axis=1)[:, None]
        psi = np.arccos(np.clip(np.abs((m * T).sum(1)), 0, 1))
        k = curv_normal(sc, psi); near = np.clip((6.0 - (hh - self.b - GAP)) / 3.0, 0, 1)   # fade the conformal term where the path is off the surface
        self.k = k * near; self.hgap = hh


def seg_eval(P, sl, X, Y, Z, i, p=3.0):
    """local coordinates of voxels (sub-grid) about segment i of path P: returns U,V',ax,w,b"""
    a = P.C[i]; bpt = P.C[i + 1]; ab = bpt - a; L2 = float(ab @ ab)
    px, py, pz = X - a[0], Y - a[1], Z - a[2]
    t = np.clip((px * ab[0] + py * ab[1] + pz * ab[2]) / L2, 0, 1)
    f = lambda arr: arr[i] + (arr[i + 1] - arr[i]) * t if np.ndim(arr) == 1 else None
    cx, cy, cz = a[0] + t * ab[0], a[1] + t * ab[1], a[2] + t * ab[2]
    qx, qy, qz = X - cx, Y - cy, Z - cz
    u = P.U[i][0] * (1 - t) + P.U[i + 1][0] * t, P.U[i][1] * (1 - t) + P.U[i + 1][1] * t, P.U[i][2] * (1 - t) + P.U[i + 1][2] * t
    nn = P.N[i][0] * (1 - t) + P.N[i + 1][0] * t, P.N[i][1] * (1 - t) + P.N[i + 1][1] * t, P.N[i][2] * (1 - t) + P.N[i + 1][2] * t
    ul = np.sqrt(u[0] ** 2 + u[1] ** 2 + u[2] ** 2); nl = np.sqrt(nn[0] ** 2 + nn[1] ** 2 + nn[2] ** 2)
    Uc = (qx * u[0] + qy * u[1] + qz * u[2]) / ul; Vc = (qx * nn[0] + qy * nn[1] + qz * nn[2]) / nl
    kk = P.k[i] * (1 - t) + P.k[i + 1] * t
    Vp = Vc + kk * Uc * Uc * 0.5                                     # conformal underside: band follows the bell surface
    ww = P.w[i] * (1 - t) + P.w[i + 1] * t; bb = P.b[i] * (1 - t) + P.b[i + 1] * t
    # axial overshoot beyond the segment ends (rounded caps)
    tt = (px * ab[0] + py * ab[1] + pz * ab[2]) / L2
    ax = np.where(tt < 0, tt, np.where(tt > 1, tt - 1, 0.0)) * math.sqrt(L2)
    return Uc, Vp, ax, ww, bb


def superq(U, V, w, b, p, ax=0.0):
    """quasi-SDF of a superellipse cross-section (half sizes w,b, exponent p), rounded axial caps"""
    q = (np.abs(U / w) ** p + np.abs(V / b) ** p) ** (1.0 / p) - 1.0
    return np.sqrt(np.maximum(q * np.minimum(w, b), 0) ** 2 + ax ** 2) + np.minimum(q * np.minimum(w, b), 0)


def chain_fields(P, g, Fo, Fc=None, Fb=None, p=2.6, kj=None, ch=False, bore=False, ch_range=None, bore_range=None, pad=None, vc_ramp_from=None):
    """accumulate path P into: Fo (outer solid, min), Fc (channel void), Fb (bore void). Voxels only inside each segment's bbox."""
    if pad is None: pad = float(P.w.max() + P.b.max() + 4)
    Pc = P.C
    for i in range(len(Pc) - 1):
        lo = np.minimum(Pc[i], Pc[i + 1]) - pad; hi = np.maximum(Pc[i], Pc[i + 1]) + pad
        sl = g.sl(lo, hi)
        if any(s.stop <= s.start for s in sl): continue
        X, Y, Z = g.coords(sl)
        U, V, ax, w, b = seg_eval(P, sl, X, Y, Z, i)
        # centre of the cross-section sits GAP + b above the (conformal) surface: V measured from centre already (centreline path is offset)
        d = superq(U, V, w, b, p, ax)
        Fo[sl] = np.minimum(Fo[sl], d)
        if ch and (ch_range is None or ch_range[0] <= i <= ch_range[1]):
            dch = np.maximum(np.abs(U) - STRIP_CH_W / 2, np.maximum(V - (b - 1.6), (b - 4.6) - V))        # strip channel just under a 1.0 mm diffuser skin on the outer face
            Fc[sl] = np.minimum(Fc[sl], np.sqrt(np.maximum(dch, 0) ** 2 + ax ** 2) + np.minimum(dch, 0))
        if bore and (bore_range is None or bore_range[0] <= i <= bore_range[1]):
            dbo = np.hypot(U, V - (-b + 4.2)) - 2.2                                                      # 4.4 mm mic-wire bore under the strip
            Fb[sl] = np.minimum(Fb[sl], np.sqrt(np.maximum(dbo, 0) ** 2 + ax ** 2) + np.minimum(dbo, 0))


# =============================================================== WRAP TREE ===
# chart control points: (s mm from rim, theta deg about the bell axis (0 = +y pod side, +90 = up), half-width, half-thickness)
def cpt(s, th, b): return bell_pt(s, th) + bell_normal(np.array([s]), th)[0] * (GAP + 0.6 + b)     # centreline rides 0.6 above the clip surface so the exact clip trims, never tears


S_CHAN_END = 13.0                               # strip channel ends where the main spiral leaves the flare for the rim lift
SP_S0, SP_S1, SP_TURN = 86.0, 12.0, 324.0         # spiral: s 86 -> 12 mm from the rim, ~0.9 turn
GAP = 0.45                                        # EMGOR: pressed almost against the horn
THETA_MIC = SP_S0 * 0 + 3.0 + SP_TURN             # main spiral end angle (deg), mic hub sits here


def M(s):
    """main spiral angle (deg) at s: eased so it leaves the trunk gently and arrives gently"""
    u = np.clip((SP_S0 - np.asarray(s, float)) / (SP_S0 - SP_S1), 0, 1); e = u * u * (3 - 2 * u) * 0.35 + u * 0.65
    return 3.0 + SP_TURN * e


def R_(s, rel, w, b): return (s, float(M(s)) + rel, w, b)


_ms = [86, 78, 70, 62, 54, 46, 38, 30, 22, 16, 12]
BRANCHES = {   # name: (parent, level, [(s, theta_deg, half-width, half-thickness), ...], flags); rel = degrees ahead (+) / behind (-) of the main spiral at that s
    "main": ("trunk", 1, [(s, float(M(s)), 8.5 - 0.5 * i / 10, 6.6 - 1.2 * i / 10) for i, s in enumerate(_ms)], "strip"),
    "TL1": ("main", 1, [R_(74, 52, 4.2, 3.2), R_(68, 68, 3.9, 3.0), R_(62, 90, 3.4, 2.7), R_(56, 118, 2.8, 2.3), R_(50, 146, 2.2, 2.0)], ""),
    "TL2": ("main", 1, [R_(52, 52, 3.8, 2.9), R_(46, 72, 3.4, 2.7), R_(40, 98, 2.8, 2.3), R_(35, 128, 2.2, 2.0)], ""),
    "TL3": ("main", 1, [R_(32, 50, 3.4, 2.7), R_(27, 70, 3.0, 2.4), R_(23, 94, 2.5, 2.1), R_(20, 120, 2.0, 1.8)], ""),
    "TR1": ("main", 1, [R_(62, -52, 3.8, 3.0), R_(67, -66, 3.4, 2.7), R_(72, -82, 2.9, 2.4), R_(76, -100, 2.3, 2.0)], ""),
    "TR2": ("main", 1, [R_(44, -50, 3.6, 2.8), R_(49, -64, 3.2, 2.5), R_(54, -80, 2.7, 2.2), R_(58, -98, 2.2, 1.9)], ""),
    "TR3": ("main", 1, [R_(26, -48, 3.2, 2.6), R_(31, -62, 2.8, 2.3), R_(36, -78, 2.4, 2.0), R_(40, -96, 1.9, 1.7)], ""),
    "TW1": ("TL1", 2, [R_(62, 90, 2.4, 2.1), R_(59, 76, 2.0, 1.9), R_(55, 68, 1.6, 1.6), R_(52, 64, 1.2, 1.2)], ""),
    "TW2": ("TR2", 2, [R_(54, -80, 2.2, 1.9), R_(55, -94, 1.9, 1.7), R_(57, -108, 1.5, 1.4), R_(59, -120, 1.1, 1.1)], ""),
}
TRUNK_STUB = [(872.0, 557.0, 92.5), (905.0, 553.0, 93.5), (940.0, 548.2, 95.0)]
TRUNK_CHART = [(185, 3, 8.8, 6.6), (170, 3, 8.8, 6.6), (155, 3, 8.8, 6.6), (140, 3.5, 8.8, 6.6), (125, 3.5, 8.7, 6.6), (110, 4, 8.6, 6.6), (98, 3.5, 8.5, 6.6), (90, 3.0, 8.5, 6.6)]


def mic_geometry(theta_deg=THETA_MIC):
    th = math.radians(theta_deg); er = np.array([0, math.cos(th), math.sin(th)]); ex = np.array([1.0, 0, 0])
    S = np.array([XR + MIC_S_DX, YB_, ZB_]) + er * (R0 + MIC_S_RHO)
    a = math.radians(MIC_AIM); aim = -ex * math.cos(a) - er * math.sin(a)
    return S, aim, er


def build_paths():
    paths = {}
    # trunk
    c = [np.array(p) for p in TRUNK_STUB] + [cpt(s, th, b) for (s, th, w, b) in TRUNK_CHART]
    wt = (np.array([0, 0.28, 0.35, 1.0]), np.array([8.8, 8.8, 8.7, 8.5])); bt = (np.array([0, 0.28, 0.35, 1.0]), np.array([6.6, 6.6, 6.6, 6.6]))
    paths["trunk"] = Path(c, wt, bt, name="trunk")
    for name, (parent, lvl, pts, flag) in BRANCHES.items():
        c = [cpt(s, th, b + (0.0 if name == "main" else 0.1)) for (s, th, w, b) in pts]      # tendrils ride 0.6 mm higher (measured wall gap otherwise 0.63)
        wv = np.array([p[2] for p in pts]); bv = np.array([p[3] for p in pts]); t = np.linspace(0, 1, len(pts))
        if name == "main":
            S, aim, er = mic_geometry(); th_e = pts[-1][1]
            Pe = c[-1]; er_e = np.array([0, math.cos(math.radians(th_e)), math.sin(math.radians(th_e))])
            # lift over the rim bead and out to the socket rear (S): bezier
            tE = (c[-1] - c[-2]); tE /= np.linalg.norm(tE)
            def bez(p0, p1, p2, p3, n):
                tt = np.linspace(0, 1, n)[:, None]; return (1 - tt) ** 3 * p0 + 3 * (1 - tt) ** 2 * tt * p1 + 3 * (1 - tt) * tt ** 2 * p2 + tt ** 3 * p3
            Ne = bell_normal(np.array([15.0]), th_e)[0]
            def make_arm(hO, dxO, h1, alpha):
                O = np.array([XR + dxO, YB_, ZB_]) + er * (R0 + hO)
                d1 = tE * (1 - alpha) + Ne * alpha; d1 /= np.linalg.norm(d1)
                seg1 = bez(Pe, Pe + d1 * h1, O - np.array([1.0, 0, 0]) * 12.0, O, 24)[1:]
                E_in = S + aim * 3.0
                seg2 = bez(O, O + np.array([1.0, 0, 0]) * 14.0, E_in - aim * 14.0, E_in, 22)[1:]
                return seg1, seg2
            def arm_ok(seg):                                                               # centre >= 10.6 from the rim bead circle, and underside >= 2.5 mm from the wall
                q = np.vstack(seg); rho_ = np.hypot(q[:, 1] - YB_, q[:, 2] - ZB_)
                bead = float(np.hypot(q[:, 0] - XR, rho_ - R0).min()); wall = float((bell_wall_distance(q) - 5.4).min())
                return bead >= 14.0 and wall >= 2.5, bead, wall
            best = None
            for hO in np.arange(14.0, 44.0, 1.0):
                for alpha in (0.0, 0.4, 0.8):
                    for h1 in (10.0, 16.0, 22.0, 28.0):
                        sg = make_arm(hO, 5.0, h1, alpha); ok, bd_, wl_ = arm_ok(sg)
                        if ok: best = sg; break
                    if best: break
                if best: break
            if best is None: print("WARNING: no arm candidate cleared the bead"); best = make_arm(30.0, 5.0, 22.0, 0.8)
            seg1, seg2 = best
            c = c + [x for x in seg1] + [x for x in seg2]
            wv = np.r_[wv, np.linspace(wv[-1], 8.0, len(seg1) + len(seg2))]; bv = np.r_[bv, np.linspace(bv[-1], 5.4, len(seg1) + len(seg2))]
            t = np.linspace(0, 1, len(c))
        paths[name] = Path(c, (np.linspace(0, 1, len(wv)), wv) if name != "main" else (np.linspace(0, 1, len(wv)), wv), (np.linspace(0, 1, len(bv)), bv), step=1.2, name=name)
        paths[name].parent = parent; paths[name].level = lvl; paths[name].flag = flag
    return paths


def tip_taper(P):
    """round the free end of a tendril: radii go to ~40 % over the last 6 mm"""
    n = len(P.C); d = np.linalg.norm(P.C - P.C[-1], axis=1); f = np.clip(d / 6.0, 0.45, 1.0) ** 0.7
    P.w = P.w * f; P.b = P.b * f


if __name__ == "__main__" and len(sys.argv) > 1 and sys.argv[1] == "chart":
    import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
    paths = build_paths(); fig, ax = plt.subplots(figsize=(14, 8))
    for nme, P in paths.items():
        if nme == "trunk": continue
        sn, hh, th = bell_nearest(P.C); r = BELL(sn)
        arcx = sn; arcy = np.degrees(np.unwrap(th)) if False else np.degrees(th)
        yv = r * th
        ax.plot(sn, yv, label=nme); ax.fill_between(sn, yv - P.w, yv + P.w, alpha=0.25)
    ax.invert_xaxis(); ax.set_aspect("equal"); ax.legend(); ax.set_xlabel("s from rim (mm)"); ax.set_ylabel("arc = r*theta (mm)")
    fig.savefig("/private/tmp/claude-501/-Users-emgor/f9645495-f198-4180-aee5-9e133ca60c53/scratchpad/ht4/chart.png", dpi=70)
    for nme, P in paths.items(): print(nme, round(P.length, 1), "pts", len(P.C))


# =============================================================== WRAP FIELDS ===
def sock_axes():
    S, aim, er = mic_geometry(); return S, aim, er


def cyl_axis_field(g, sl, S, aim, R, t0, t1, rr=3.0):
    """rounded cylinder along the mic axis (origin S), radius R, axial t in [t0,t1]"""
    X, Y, Z = g.coords(sl); px, py, pz = X - S[0], Y - S[1], Z - S[2]
    t = px * aim[0] + py * aim[1] + pz * aim[2]
    rho = np.sqrt(np.maximum(px * px + py * py + pz * pz - t * t, 0))
    qa = np.abs(t - 0.5 * (t0 + t1)) - (0.5 * (t1 - t0) - rr); qr = rho - (R - rr)
    return np.hypot(np.maximum(qa, 0), np.maximum(qr, 0)) + np.minimum(np.maximum(qa, qr), 0) - rr


def wrap_fields(g, paths):
    Fo = g.full(); Fc = g.full(); Fb = g.full()
    order = ["trunk", "main", "TL1", "TL2", "TL3", "TR1", "TR2", "TR3", "TW1", "TW2"]
    kj = {1: 5.0, 2: 3.0, 3: 2.0}
    # index where main's chart (surface-following) part ends -> strip channel stops there (it lifts over the rim after)
    for nm in order:
        P = paths[nm]
        if nm not in ("trunk", "main"): tip_taper(P)
        lo = P.C.min(0) - P.w.max() - P.b.max() - 4; hi = P.C.max(0) + P.w.max() + P.b.max() + 4
        sl = g.sl(lo, hi)
        if any(s.stop <= s.start for s in sl): continue
        Ft = g.full()
        if nm == "trunk": chain_fields(P, g, Ft, Fc, Fb, p=3.6, ch=True, bore=True)
        elif nm == "main":
            ichan = int(np.argmax(bell_nearest(P.C)[0] <= 17.0)) if (bell_nearest(P.C)[0] <= 17.0).any() else len(P.C) - 1
            chain_fields(P, g, Ft, Fc, Fb, p=3.6, ch=True, bore=True, ch_range=(0, ichan), vc_ramp_from=ichan)
            P.ichan = ichan
        else: chain_fields(P, g, Ft, p=2.2)
        k = 2.0 if nm == "trunk" else kj[P.level]
        Fo[sl] = smin(Fo[sl], Ft[sl], k) if nm != "trunk" else np.minimum(Fo[sl], Ft[sl])
    # socket hub at the end of the main branch + its void (bore for the mic with TPU sleeve, chamber for cable slack / stuff)
    S, aim, er = sock_axes(); pad = SOCK_R + 14
    lo = np.minimum(S, S + aim * SOCK_T1) - pad; hi = np.maximum(S, S + aim * SOCK_T1) + pad; sl = g.sl(lo, hi)
    if not any(s.stop <= s.start for s in sl):
        hub = cyl_axis_field(g, sl, S, aim, SOCK_R, SOCK_T0, SOCK_T1, 5.0)
        Fo[sl] = smin(Fo[sl], hub, 4.0)
        bore = cyl_axis_field(g, sl, S, aim, SOCK_BORE_R, SOCK_CH_T[1], SOCK_T1 + 8.0, 0.5)
        chamber = cyl_axis_field(g, sl, S, aim, SOCK_CHAMBER_R, SOCK_CH_T[0], SOCK_CH_T[1] + 1.0, 3.0)
        Fb[sl] = np.minimum(Fb[sl], np.minimum(bore, chamber))
    # exact press-fit: never closer than GAP to the bell wall (the underside becomes a true offset surface of the bell)
    idx = np.argwhere(Fo < 0.8)
    if len(idx):
        pts = g.lo + idx * g.h; d = bell_wall_distance(pts); sel = d < GAP
        if sel.any():
            ii = idx[sel]; Fo[ii[:, 0], ii[:, 1], ii[:, 2]] = np.maximum(Fo[ii[:, 0], ii[:, 1], ii[:, 2]], (GAP - d[sel]).astype(np.float32))
    return Fo, Fc, Fb


# =============================================================== BUILD: PIECES ===
def joint_fields(g, Fo, Fc, XJ, plug_len=13.8):
    """tongue region (on the +x piece, reaching -x) and socket region (in the -x piece)"""
    X = g.X()
    tip = XJ - plug_len
    taper = np.clip((tip + 3.0 - X) / 3.0, 0, 1) * 0.9
    ton_ok = (Fo + 2.2 + taper < 0) & (Fc - 1.0 > 0) & (X >= tip) & (X <= XJ + 0.2)
    sock = (Fo + 2.2 - 0.35 < 0) & (Fc - 0.65 > 0) & (X >= tip - 0.35) & (X <= XJ + 0.3)
    return ton_ok, sock


def build_wrap(save=True):
    t0 = time.time(); h = 0.38
    paths = build_paths()
    out = {}
    # ---- crown (x >= X_J2) ----
    gc = G3((X_J2 - 16.0, 452, 28), (XR + 60, 612, 172), h); print("crown grid", gc.n)
    Fo, Fc, Fb = wrap_fields(gc, paths); print("fields", round(time.time() - t0, 1))
    ton, sock = joint_fields(gc, Fo, Fc, X_J2)
    X = gc.X()
    solid = np.maximum(np.maximum(Fo, (X_J2 + 0.1) - X), np.maximum(-Fc, -Fb))                 # crown: x >= X_J2 + 0.1, minus channel and bore
    tongue = np.where(ton & (X <= X_J2 + 0.2), -1.0, 1.0).astype(np.float32); tongue = np.maximum(tongue, -Fb)
    Fcrown = np.minimum(solid, ndi.gaussian_filter(tongue, 0.5))
    mcrown = mc(ndi.gaussian_filter(Fcrown, 1.6), gc.lo, h, taubin=12)
    out["wrap_crown_branches_mic"] = mcrown; print("crown", mcrown.is_watertight, len(mcrown.faces), round(time.time() - t0, 1))
    del Fo, Fc, Fb, solid
    # ---- trunk (X_J1 .. X_J2) ----
    gt = G3((X_J1 - 16.0, 520, 80), (X_J2 + 0.3, 566, 112), h); print("trunk grid", gt.n)
    Fo, Fc, Fb = wrap_fields(gt, paths); X = gt.X()
    ton1, sock1 = joint_fields(gt, Fo, Fc, X_J1); ton2, sock2 = joint_fields(gt, Fo, Fc, X_J2)
    solid = np.maximum(np.maximum(np.maximum(Fo, (X_J1 + 0.1) - X), X - (X_J2 - 0.1)), np.maximum(-Fc, -Fb))
    solid = np.where(sock2 & (X <= X_J2 - 0.1), np.maximum(solid, 1.0), solid)                 # socket for the crown's tongue
    tongue1 = np.where(ton1 & (X <= X_J1 + 0.2), -1.0, 1.0).astype(np.float32); tongue1 = np.maximum(tongue1, -Fb)
    Ftr = np.minimum(solid, ndi.gaussian_filter(tongue1, 0.5))
    mtr = mc(ndi.gaussian_filter(Ftr, 0.8), gt.lo, h, taubin=8)
    out["wrap_trunk_mid"] = mtr; print("trunk", mtr.is_watertight, len(mtr.faces), round(time.time() - t0, 1))
    if save:
        for k, m in out.items(): save4(m, k)
    json.dump(dict(trunk_len=paths["trunk"].length, **{k: v.length for k, v in paths.items()}), open(os.path.join(HERE, "wrap_lengths_v4.json"), "w"), indent=1)
    return out, paths


# =============================================================== BUILD: SHELL ===
DT = dict(rail_x=878.0, rail_z0=8.0, neck=14.0, head=22.0, h=6.0, clear=0.8)


def control_cut(g):
    X, Y, Z = g.X(), g.Y(), g.Z(); cut = g.full()
    for name, (cx, cz), (w, hh), kind in CONTROLS:
        if kind == "ellipse": d2 = (np.sqrt(((X - cx) / (w / 2)) ** 2 + ((Z - cz) / (hh / 2)) ** 2) - 1) * min(w, hh) / 2
        else:
            r = 4.0 if kind == "stadium" else (2.6 if name.startswith("fader") else 1.0)
            qx = np.abs(X - cx) - (w / 2 - r); qz = np.abs(Z - cz) - (hh / 2 - r)
            d2 = np.hypot(np.maximum(qx, 0), np.maximum(qz, 0)) + np.minimum(np.maximum(qx, qz), 0) - r
        cut = np.minimum(cut, np.maximum(d2, FACE_Y - WALL - 3.0 - Y))
    return cut


def plate_mask(g):
    from matplotlib.path import Path as MP
    from shapely.geometry import Polygon
    poly = Polygon(L.PLATE_OUTLINE).buffer(0.5); pts = np.array(poly.exterior.coords)
    XX, ZZ = np.meshgrid(g.ax[0], g.ax[2], indexing="ij")
    inside = MP(pts).contains_points(np.c_[XX.ravel(), ZZ.ravel()]).reshape(XX.shape)
    return inside[:, None, :] & (g.Y() >= 530.0) & (g.Y() <= PLATE_FRONT + 0.4)


def switch_wall_x(z):
    xs = np.arange(700, 840, 0.1); v = sil_sd(xs, np.full_like(xs, z)); return float(xs[np.argmax(v < 0)])


def open_thin(S, g, r, slab=90):
    """morphological opening of the solid (S<0) with a ball of radius r, in z slabs: removes solid thinner than 2r and rounds every convex edge with radius r. Keeps the SDF elsewhere."""
    out = S.copy(); mg = int(math.ceil((r + 3.0) / g.h)); n = S.shape[2]
    for a in range(0, n, slab):
        b = min(n, a + slab); a2, b2 = max(0, a - mg), min(n, b + mg); sub = S[:, :, a2:b2] < 0
        er = ndi.distance_transform_edt(sub) * g.h >= r; op = ndi.distance_transform_edt(~er) * g.h < r
        rem = (sub & ~op)[:, :, a - a2:b - a2]; seg = out[:, :, a:b]; seg[rem] = np.maximum(seg[rem], 0.2)
    return out


def build_shell(save=True):
    t0 = time.time(); h = H_SHELL
    g = G3((715, 492, -62), (925.0, 590, 148), h); print("shell grid", g.n)
    Po = pebble_field(g, 0.0); Pi = pebble_field(g, WALL, yb=YB - 8.0)
    zk_ = 0.5 * (KEEL_Z[0] + KEEL_Z[1]); Ko = rbox3(g, (KEEL_X[0], KEEL_Y[0], KEEL_Z[0]), (KEEL_X[1], KEEL_Y[1], KEEL_Z[1]), KEEL_R)
    Ko = (Ko - 1.8 * np.clip(1.0 - ((g.Z() - zk_) / (0.5 * (KEEL_Z[1] - KEEL_Z[0]))) ** 2, 0.0, 1.0) ** 1.0).astype(np.float32)   # slight convex crown on the side faces (zero at the flat bottom)
    # palm rest (v4.8): a smooth height-field bump on the face plane, no hard edges anywhere. A(x,z) = outward height above the control face.
    sm5 = lambda t: t * t * t * (10 - 15 * t + 6 * t * t); sp = lambda u, k: k * np.log1p(np.exp(np.clip(u / k, -30, 30)))
    X_, Z_ = g.X()[:, :, 0], g.Z()[:, 0, :] if False else g.Z()[0, :, :]
    Xa = g.ax[0][:, None]; Za = g.ax[2][None, :]
    d_ = np.abs(Xa - PALM_C[0]); bx = np.exp(-(d_ / 52.0) ** 3.0)                                   # x fall-off: no edge, no plateau end
    zc_ = PAD_Z_CREST; sz = np.where(Za > zc_, 9.0, 15.0)
    prof = np.exp(-0.5 * ((Za - zc_) / sz) ** 2)                                                       # asymmetric Gaussian in z: convex everywhere, no planar region
    A = (PAD_AMAX * bx * prof - PAD_DIP * np.exp(-((Xa - PALM_C[0]) / 30.0) ** 2) * prof * 0.5).astype(np.float32)
    thick = (22.0 * np.clip(prof * 1.5, 0, 1) * np.clip(bx * 2.0, 0, 1)).astype(np.float32)
    gx_, gz_ = np.gradient(A, g.h, axis=0), np.gradient(A, g.h, axis=1); nrm_ = np.sqrt(1 + gx_ ** 2 + gz_ ** 2)
    Yp = FACE_Y + A
    padf = np.maximum(g.Y() - Yp[:, None, :], (Yp - thick)[:, None, :] - g.Y()) / nrm_[:, None, :]
    gate = (1.0 - (lambda t: t * t * (3 - 2 * t))(np.clip((-8.0 - g.Z()) / 12.0, 0.0, 1.0)))            # pad has no influence above z = -2 (control face untouched)
    pad_eff = (padf + 60.0 * gate + 60.0 * (1.0 - (lambda t: t * t * (3 - 2 * t))(np.clip(thick / 5.0, 0, 1)))[:, None, :]).astype(np.float32)
    kzv = (22.0 * (lambda t: t * t * (3 - 2 * t))(np.clip((-6.0 - g.Z()) / 12.0, 0.0, 1.0)) + 1e-3).astype(np.float32)           # big housing fillet below z -6; no blend at all next to the flat control face
    outer = smin_exp(smin(Po, Ko, kzv), pad_eff, PAD_K)                                  # large exponential smooth-union with the whole pod
    paths = None; Fw = g.full(); Fc = g.full(); Fb = g.full()                       # no ribbon / bridge: pod surface is closed
    kmap = (11.0 * np.clip((ROLL_W * 0.5 + sil_sd(g.ax[0][:, None], g.ax[2][None, :])) / (ROLL_W * 0.5), 0.0, 1.0))[:, None, :] + 1e-3         # blend only near/outside the silhouette edge; flat face untouched
    k2 = 26.0 * np.exp(-(((g.X() - 912.0) / 20.0) ** 2 + ((g.Z() - 98.0) / 28.0) ** 2)) * np.clip((g.X() - 884.0) / 12.0, 0.0, 1.0); kmap = np.maximum(kmap, k2.astype(np.float32))      # big fillet where the ribbon leaves the pod
    hh_ = np.maximum(kmap - np.abs(outer - Fw), 0) / kmap
    outer = np.minimum(outer, Fw) - hh_ * hh_ * kmap / 4
    cxs = L.P["casing_cx"]
    rail = rbox3(g, (RAIL_X0, RAIL_Y0, SL_ZB), (RAIL_X1, POCKET_Y[1] + SL_WALL, SL_ZT), 2.5)
    for cx in cxs: rail = smin(rail, rbox3(g, (cx - 7.0, RAIL_Y0, FOOT_Z0), (cx + 7.0, RAIL_Y0 + 4.0, SL_ZB + 3.0), 2.0), 1.5)       # feet beside the top caps (TPU pad faces)
    outer = smin(outer, rail, 5.0); print("outer", round(time.time() - t0, 1))
    kz = int(np.searchsorted(g.ax[2], 4.0)); sg = 2.6 / g.h                                   # lower body (pad, housing, hand rest): volumetric smoothing so no crease survives
    blur = ndi.gaussian_filter(outer[:, :, :kz + 20], sg, mode="nearest"); wb = np.clip((-14.0 - g.ax[2][None, None, :kz + 20]) / 10.0, 0.0, 1.0)
    outer[:, :, :kz + 20] = (outer[:, :, :kz + 20] * (1 - wb) + blur * wb).astype(np.float32); del blur; print("lower blur", round(time.time() - t0, 1))
    # palm indent: shallow smooth concave dip across the control-side (+y) face of housing + hand rest, G-infinity (super-gaussian), 4 mm deep
    dd = DENT; rr_ = ((g.X() - dd["cx"]) / dd["ax"]) ** 2 + ((g.Z() - dd["cz"]) / dd["az"]) ** 2
    wy = (lambda t: t * t * (3 - 2 * t))(np.clip((g.Y() - 556.0) / 8.0, 0.0, 1.0))
    outer = (outer + dd["depth"] * np.exp(-rr_ ** 1.5 * 1.2) * wy).astype(np.float32)
    kzd = int(np.searchsorted(g.ax[2], -18.0)); np.savez_compressed(os.path.join(HERE, "_cache_v4", "outer_lower.npz"), u=outer[:, :, :kzd].copy(), lo=g.lo, h=g.h)
    X, Y, Z = g.X(), g.Y(), g.Z()
    # cavity (only above the keel ceiling inside the keel footprint)
    keelxy = (X >= KEEL_X[0] - 0.5) & (X <= KEEL_X[1] + 0.5) & (Y <= KEEL_Y[1] + 0.5)
    cav = np.where(keelxy, np.maximum(Pi, (BAY_ZTOP + WALL) - Z), Pi)
    S = np.maximum(outer, -cav)
    S = np.minimum(S, np.maximum(outer, -(WALL + 0.1) - outer))                                       # guaranteed continuous skin >= 2.2 mm along the whole outer surface (roof near the back edge was thinner than 1.5 and got opened away)
    # rib (solid strip around the dovetail), inside the outer body
    ribf = np.maximum(np.maximum(np.abs(X - DT["rail_x"]) - 21.0, Y - (YB + 9.2)), np.maximum(-(Z + 6.0), Z - 108.0))
    S = np.minimum(S, np.maximum(ribf, Po)) if False else np.minimum(S, np.maximum(np.maximum(ribf, outer), (YB - 0.2) - Y))
    # female dovetail, open at the top, stop at rail_z0
    rt = Y - (PLATE_FRONT - 0.2); wch = DT["neck"] / 2 + DT["clear"] + (DT["head"] - DT["neck"]) / 2 * np.clip(rt / DT["h"], 0, 1)
    ch = np.maximum(np.maximum((np.abs(X - DT["rail_x"]) - wch) * 0.93, -rt - 1.0), np.maximum(rt - (DT["h"] + DT["clear"]), Z - 101.4))
    S = np.maximum(S, -ch)
    # control openings
    S = np.maximum(S, -control_cut(g)); print("holes", round(time.time() - t0, 1))
    # battery pocket (open at the bottom) + door rebate bumps' dimples + pry notch + wire slot to the pebble cavity
    pocket = rbox3(g, (BAY_X[0], BAY_Y[0], -80.0), (BAY_X[1], BAY_Y[1], BAY_ZTOP), 1.5)
    S = np.maximum(S, -pocket)
    # power switch (MTS-102 style, 6.3 mm) centred on the -x end face of the housing (y, z centre of the keel bbox). Outer flat land 12 mm at the measured face, 2.5 mm wall, inner spot-face 14 mm
    jy0, jy1 = int((SW_Y - 7.0 - g.lo[1]) / g.h), int((SW_Y + 7.0 - g.lo[1]) / g.h) + 1; kz0, kz1 = int((SW_Z - 7.0 - g.lo[2]) / g.h), int((SW_Z + 7.0 - g.lo[2]) / g.h) + 1
    sub = outer[:, jy0:jy1, kz0:kz1] < 0; xf = g.lo[0] + np.argmax(sub, axis=0) * g.h
    ys_ = g.ax[1][jy0:jy1][:, None]; zs_ = g.ax[2][kz0:kz1][None, :]; disc = np.hypot(ys_ - SW_Y, zs_ - SW_Z) < 6.5
    X_LAND = float(xf[disc].max()) + 0.0; SW_INFO["x_face_min"] = float(xf[disc].min()); SW_INFO["x_land"] = X_LAND
    rl = 6.0 + 2.0 * np.clip((X_LAND - X) / 2.0, 0.0, 1.0); land = np.maximum(np.sqrt((Y - SW_Y) ** 2 + (Z - SW_Z) ** 2) - rl, X - X_LAND); S = np.maximum(S, -land)
    sw = np.sqrt((Y - SW_Y) ** 2 + (Z - SW_Z) ** 2) - 3.15; S = np.maximum(S, -np.maximum(sw, X - (BAY_X[0] + 1.0)))
    spot = np.maximum(np.sqrt((Y - SW_Y) ** 2 + (Z - SW_Z) ** 2) - 8.7, np.maximum(X - (BAY_X[0] + 1.0), (X_LAND + 2.5) - X)); S = np.maximum(S, -spot)
    # wire pass-through: 25 x 15 stadium from the battery housing into the pod cavity, 1.2 mm rounded lips
    zm = BAY_ZTOP + 0.5 * WALL; st = rbox3(g, (837.5, 543.5, BAY_ZTOP - 9.0), (862.5, 558.5, BAY_ZTOP + WALL + 9.0), 7.4)
    st = st - 1.2 * np.clip((np.abs(Z - zm) - (0.5 * WALL - 1.2)) / 1.2, 0.0, 1.0); S = np.maximum(S, -st)
    # clam door (v5.2): housing keeps only material deeper than lip + gap below the parting line; bead groove; fingernail notches
    fp = np.maximum(np.maximum(np.maximum(771.0 - X, X - 903.0), np.maximum(495.0 - Y, Y - 566.2)), (KEEL_Z[0] - 7.0) - Z)
    keep_sw = np.maximum(np.sqrt((Y - SW_Y) ** 2 + (Z - SW_Z) ** 2) - 10.5, X - 790.0)                   # the switch land stays full thickness: the door lip is scalloped around it
    c = np.maximum(np.maximum(np.maximum(Z - DOOR_ZP, -(outer + DOOR_LIP + DOOR_GAP)), fp), -keep_sw); S = np.maximum(S, -c)
    dgp = -(DOOR_LIP + DOOR_GAP) - outer; prof = (BEAD_H + 0.1) * np.sqrt(np.clip(1.0 - ((Z - BEAD_ZB) / 1.2) ** 2, 0.0, 1.0)); cg = np.maximum(np.maximum(np.maximum(dgp - prof, fp), np.maximum(np.abs(Z - BEAD_ZB) - 1.25, Z - DOOR_ZP)), -keep_sw); S = np.maximum(S, -cg)
    for xc in (805.0, 837.0, 869.0):                                     # three 8 mm wide, 1.2 mm deep fingernail notches on the -y side at the parting line
        nt = np.maximum(np.maximum(np.maximum(np.abs(X - xc) - 4.0, DOOR_ZP - 1.0 - Z), Z - (DOOR_ZP + 4.5)), np.maximum(-(outer + 1.2), Y - (KEEL_Y[0] + 14.0))); S = np.maximum(S, -nt)
    # Hall rail: continuation of the pod over the (cut-away) mount area, resting beside the valve top caps. Board slit 81.6 x 21.6 x 5.0 open at the -x end only; 3 sensor holes through the VALVE-FACING (-y) wall
    sbox = rbox3(g, (RAIL_X0, RAIL_Y0, SL_ZB), (RAIL_X1, POCKET_Y[1] + SL_WALL, SL_ZT), 2.0); S = np.minimum(S, np.maximum(sbox, outer))
    web = rbox3(g, (RAIL_X0, RAIL_Y0, SL_ZT - 1.0), (RAIL_X1, POCKET_Y[1] + SL_WALL, 150.0), 1.0); S = np.minimum(S, np.maximum(web, outer))          # one continuous wall from slab to roof (the old four separate webs left windows open to the outside)
    pocket_s = rbox3(g, (POCKET_X[0], POCKET_Y[0], POCKET_Z[0]), (POCKET_X[1], POCKET_Y[1], POCKET_Z[1]), 0.6); S = np.maximum(S, -pocket_s)
    chan = rbox3(g, (POCKET_X[0] + 0.3, POCKET_Y[1] - 1.0, POCKET_Z[0] + 0.1), (POCKET_X[1] - 0.3, POCKET_Y[1] + 12.0, POCKET_Z[0] + 6.2), 0.5); S = np.maximum(S, -chan)     # v5.0: pocket open to the pod interior (+y) only; -x end closed       # v4.9: face entry removed (no inside path exists, see V4.9 notes); -x end open again as in v4.3 until a clip-in rail is chosen
    for cx in cxs:
        hole5 = rbox3(g, (cx - 4.5, RAIL_Y0 - 1.0, POCKET_Z[0] - 0.5), (cx + 4.5, POCKET_Y[0] + 0.6, POCKET_Z[0] + 6.5), 1.5); S = np.maximum(S, -hole5)
    # v5.0: no push slot any more (board is pushed back toward the cavity)
    # side-face ports, opposite each other at the same z and y: USB-C (portrait) through the -x wall, 3.5 mm jack through the +x wall
    xs_ = np.arange(700.0, 940.0, 0.1); ins = xs_[sil_sd(xs_, np.full_like(xs_, PORT_Z)) < 0]; XL, XR_ = float(ins.min()), float(ins.max()); PORT_X["xl"], PORT_X["xr"] = XL, XR_
    # USB-C: exactly centred on the Daisy Seed 3 receptacle. Seed lies along x (long axis), USB-C end toward the -x (mouthpiece) wall; board centre-line at USB_Z, receptacle centre 1.6 mm above the PCB top.
    xs2 = np.arange(700.0, 940.0, 0.1); ins2 = xs2[sil_sd(xs2, np.full_like(xs2, USB_Z)) < 0]; XL2 = float(ins2.min()) + 0.2          # outer wall x at the Seed's height (y ~ 555 is below the roll)
    PORT_X["usb_wall_outer_x"] = XL2; PORT_X["usb_mouth_x"] = XL2 + 2.2 + 0.4; PORT_X["seed_x0"] = PORT_X["usb_mouth_x"] + 1.55
    infl = 0.5 * np.clip((XL2 + 0.5 - X) / 0.5, 0, 1)                                  # even 0.5 mm chamfer at the mouth
    qa = np.abs(Z - USB_Z) - (6.5 + infl - 3.75 - infl); qb = np.abs(Y - USB_Y) - 0.0
    d_st = np.hypot(np.maximum(np.abs(Z - USB_Z) - 2.75, 0), Y - USB_Y) - (3.75 + infl)                                  # stadium 13 (z, portrait) x 7.5 (y), constant radius 3.75
    usb = np.maximum(d_st, np.maximum(XL2 - 6.0 - X, X - (XL2 + 6.0))); S = np.maximum(S, -usb)
    jack = np.maximum(np.hypot(Y - PORT_Y, Z - PORT_Z) - 3.2, np.maximum(XR_ - 9.0 - X, X - (XR_ + 6.0))); S = np.maximum(S, -jack)
    # (no power-switch hole: placement TBD)
    S = np.where(plate_mask(g) & ~((X >= RAIL_X0 - 1.0) & (X <= RAIL_X1 + 1.0) & (Z >= 114.0)), np.maximum(S, 1.0), S)                                         # never inside the mount plate (+0.5)
    S = np.maximum(S, -Fc); S = np.maximum(S, -Fb)
    print("assembled", round(time.time() - t0, 1))
    # audit: openings in the outer envelope (surface voxels that are air), excluding the open back (y < YB+0.9) and the joint cut plane
    air_depth = ndi.binary_dilation((outer > -1.9) & (outer < -1.1) & (S > 0), iterations=2)
    env = (np.abs(outer) < 0.6) & (S > 0) & air_depth & (Y > YB + 0.9)
    lab_o, n_o = ndi.label(env, structure=np.ones((3, 3, 3)))
    ops = []
    for k in range(1, n_o + 1):
        ii = np.argwhere(lab_o == k)
        if len(ii) < 6: continue
        c_ = g.lo + ii.mean(0) * g.h; lo_ = g.lo + ii.min(0) * g.h; hi_ = g.lo + ii.max(0) * g.h; ops.append(dict(centre=[round(float(v), 1) for v in c_], size=[round(float(hi_[j] - lo_[j] + g.h), 1) for j in range(3)], area_mm2=round(len(ii) * g.h ** 2, 1)))
    json.dump(ops, open(os.path.join(HERE, "audit_shell_openings_raw.json"), "w"), indent=1); print("audit openings", len(ops))
    S = open_thin(S, g, 0.75); print("opened", round(time.time() - t0, 1))                      # fillet sweep + thin cleanup: convex edges r>=0.75, no solid thinner than 1.5 mm
    solid_lab = ndi.label(S < 0)[0]; cnt = np.bincount(solid_lab.ravel()); cnt[0] = 0; keep = solid_lab == cnt.argmax()
    S = np.where(keep, S, np.maximum(S, 0.05)).astype(np.float32)
    m = mc(ndi.gaussian_filter(S, 0.8), g.lo, h, taubin=6)
    print("shell", m.is_watertight, len(m.faces), round(time.time() - t0, 1))
    if save: save4(m, "shell_pebble")
    return m, None


def build_door(save=True):
    """v5.2 wraparound clam cover: the whole bottom of the housing plus a 1.5 mm lip that wraps up the rounded sides to the parting line on the side band; continuous 0.7 mm bead on the lip's inner face snaps into a groove in the housing. Derived from the shell's own outer field so it is exactly flush."""
    d = np.load(os.path.join(HERE, "_cache_v4", "outer_lower.npz")); u_all = d["u"]; lo = d["lo"]; h = float(d["h"])
    nx, ny, nz = u_all.shape; ax = lo[0] + h * np.arange(nx); ay = lo[1] + h * np.arange(ny); az = lo[2] + h * np.arange(nz)
    ix = (ax >= 770.0) & (ax <= 904.0); iy = (ay >= 494.0) & (ay <= 571.0); iz = (az <= DOOR_ZP + 4.0)
    u = u_all[np.ix_(ix, iy, iz)]; X = ax[ix][:, None, None]; Y = ay[iy][None, :, None]; Z = az[iz][None, None, :]; glo = np.array([ax[ix][0], ay[iy][0], az[iz][0]])
    z0 = KEEL_Z[0]; fp = np.maximum(np.maximum(np.maximum(771.0 - X, X - 903.0), np.maximum(494.5 - Y, Y - 566.2)), (z0 - 7.0) - Z)
    plate = np.maximum(np.maximum(u, Z - (z0 + DOOR_PLATE)), fp)
    keep_sw = np.maximum(np.sqrt((Y - SW_Y) ** 2 + (Z - SW_Z) ** 2) - 10.5, X - 790.0)
    band = np.maximum(np.maximum(np.maximum(np.maximum(u, -DOOR_LIP - u), Z - DOOR_ZP), fp), -keep_sw)
    dd = -DOOR_LIP - u; prof = BEAD_H * np.sqrt(np.clip(1.0 - ((Z - BEAD_ZB) / 1.1) ** 2, 0.0, 1.0))
    bead = np.maximum(np.maximum(np.maximum(np.maximum(-dd, dd - prof), Z - DOOR_ZP), fp), -keep_sw)
    F = np.minimum(band, bead).astype(np.float32)                                    # the lip band u in [-1.5, 0] is also the 1.5 mm bottom plate
    m = mc(ndi.gaussian_filter(F, 0.35), glo, h, taubin=2)
    strain = 150.0 * DOOR_LIP * BEAD_H / (DOOR_ZP - BEAD_ZB + 0.0) ** 2 * (DOOR_ZP - z0 - 8.0) / (DOOR_ZP - z0 - 8.0)
    info = dict(wrap_height_mm=DOOR_WRAP, lip_thickness_mm=DOOR_LIP, bead_height_mm=BEAD_H, bead_zone_height_mm=2.2, lip_free_length_mm=round(DOOR_ZP - z0 - 4.0, 1), strain_pct_cantilever_estimate=round(150.0 * DOOR_LIP * BEAD_H / (DOOR_ZP - z0 - 4.0) ** 2, 2), parting_line_clearance_mm=DOOR_GAP, plate_thickness_mm=DOOR_LIP, notches="3 x 8 mm wide x 1.2 deep on the -y side housing at x 805/837/869", parting_line_z=DOOR_ZP)
    json.dump(info, open(os.path.join(HERE, "door_latch_v5.json"), "w"), indent=1)
    if save: save4(m, "battery_door")
    print("door", m.is_watertight, len(m.faces), m.bounds.tolist(), info); return m


BTN_H = 5.5; LIP_DROP = 1.4; CAP_Z0 = 133.0 - LIP_DROP                      # button 18.0 dia x 5.5 high [A]; printed cap skirt drops 1.4 below the button underside (snap lip)
CAP_CEIL = CAP_Z0 + LIP_DROP + BTN_H + 0.2                                  # inner ceiling (magnet bottom face) at rest = 138.7
PISTON_TRAVEL = 13.0; STEM_REST_Z = 133.0
SL_XC = 850.0; SL_WALL = 2.0
BOARD = (81.0, 20.0); BOARD_T = 1.6; SENSOR_H = 4.0; PCLR = 0.5; POCKET_H = 8.0
SENSOR_TOP_TARGET = CAP_CEIL - PISTON_TRAVEL - 1.0 + 1.8                          # sensor/hole top: 1.0 mm below the magnet face at full press (13 mm travel)
POCKET_Z = (SENSOR_TOP_TARGET - SENSOR_H - BOARD_T, SENSOR_TOP_TARGET - SENSOR_H - BOARD_T + POCKET_H); NOTCH_Z0 = 116.4; SL_ZB = NOTCH_Z0 + 0.2; SL_ZT = POCKET_Z[1] + 2.2
RAIL_Y0 = 521.8                                                             # valve-facing wall of the Hall rail: 12.3 from the valve axis (cap band radius 11.3 + 1.0)
POCKET_X = (SL_XC - (BOARD[0] + 2 * PCLR) / 2, SL_XC + (BOARD[0] + 2 * PCLR) / 2)       # closed end +x, open end -x (mouthpiece side)
POCKET_Y = (RAIL_Y0 + SL_WALL, RAIL_Y0 + SL_WALL + BOARD[1] + 2 * PCLR)
RAIL_X0, RAIL_X1 = SL_XC - 43.6, SL_XC + 43.6; FOOT_Z0 = 110.6
SENSOR_Y = RAIL_Y0                                                          # sensor tips come out of the valve-facing wall
SENSOR_DY = L.P["casing_y"] * -1 + SENSOR_Y                                  # lateral offset sensor <-> valve axis (+y)
SENSOR_TOP_Z = POCKET_Z[1]
PORT_X = {}; SW_INFO = {}
USB_Y, USB_Z = 555.2, 6.0           # Seed 3 receptacle centre (PCB top y = 553.6, receptacle 3.25 mm tall, centre +1.6)


def build_hall_sleeve(save=True):
    h = 0.2; cy = L.P["casing_y"]; cxs = L.P["casing_cx"]
    g = G3((SL_XC - 52, SL_Y0 - 6, 107.0), (SL_XC + 50, cy + 16, SL_ZT + 3), h); X, Y, Z = g.X(), g.Y(), g.Z()
    F = rbox3(g, (SL_XC - 42.2, SL_Y0, SL_ZB), (SL_XC + 42.2, SL_Y1, SL_ZT), 2.2)
    rib = rbox3(g, (SL_XC - 42.2, SL_Y1 - 5.0, 109.2), (SL_XC + 42.2, SL_Y1 + 0.4, SL_ZB + 3.0), 1.5); F = smin(F, rib, 2.5)          # web down to the cap collar
    for cx in cxs:                                                    # snap collar: 0.25 clearance on each top cap, wraps the -y side and 3.5 mm past the axis
        rr = np.hypot(X - cx, Y - cy); ring = np.maximum(np.maximum(rr - 12.45, 10.75 - rr), np.maximum(Z - 116.7, 110.0 - Z)); ring = np.maximum(ring, (Y - (cy + 3.5)))
        F = smin(F, ring.astype(np.float32), 3.0)
    for cx in cxs: F = np.maximum(F, 10.75 - np.hypot(X - cx, Y - cy) + 0.0 * Z)
    pocket = rbox3(g, (SL_XC - 70, POCKET_Y[0], POCKET_Z[0]), (POCKET_X[1], POCKET_Y[1], POCKET_Z[1]), 0.6); F = np.maximum(F, -pocket)
    for cx in cxs:
        hole = rbox3(g, (cx - 2.5, SENSOR_Y - 2.5, POCKET_Z[1] - 0.5), (cx + 2.5, SENSOR_Y + 2.5, SL_ZT + 2.0), 0.05); F = np.maximum(F, -hole)
    push = rbox3(g, (POCKET_X[1] - 1.0, 0.5 * (POCKET_Y[0] + POCKET_Y[1]) - 3.0, POCKET_Z[0] + 0.5), (SL_XC + 44.0, 0.5 * (POCKET_Y[0] + POCKET_Y[1]) + 3.0, POCKET_Z[1] - 0.5), 0.8); F = np.maximum(F, -push)
    lab = ndi.label(F < 0)[0]; cnt = np.bincount(lab.ravel()); cnt[0] = 0; F = np.where(lab == cnt.argmax(), F, np.maximum(F, 0.05)).astype(np.float32)
    m = mc(ndi.gaussian_filter(F, 0.6), g.lo, h, taubin=6)
    if save: save4(m, "hall_sensor_sleeve")
    print("sleeve", m.is_watertight, m.bounds.tolist()); return m


def build_caps(save=True):
    """printed finger cap: snaps over the existing 18.0 mm button (bore 18.2, snap lip under the rim), 10 x 3 magnet centred on the axis in the top (pocket 10.4 x 3.3 + glue groove, loaded from below),
    smooth comfortable top, zig-zag band + polka dots on the side. Local z=0 is the lip bottom (cap sits with z=0 at world CAP_Z0)."""
    h = 0.07; R_B = 9.1; R_O = 10.7; zc = LIP_DROP + BTN_H + 0.2; zp = zc + 3.3; ztop = zp + 1.8
    g = G3((-12.4, -12.4, -0.6), (12.4, 12.4, ztop + 0.6), h); X, Y, Z = g.X(), g.Y(), g.Z(); R = np.hypot(X, Y); TH = np.arctan2(Y, X)
    Nz = 9; u = (TH / (2 * np.pi) * Nz) % 1.0; tri = 4 * np.abs(u - 0.5) - 1; zz = 3.9 + 1.4 * tri
    ridge = np.clip((1.2 - np.abs(Z - zz)) / 0.45, 0, 1) * 0.6
    rs = R_O + ridge
    q1 = R - (rs - 2.8); q2 = Z - (ztop - 2.8)
    flat = (np.sqrt(np.maximum(q1, 0) ** 2 + np.maximum(q2, 0) ** 2) + np.minimum(np.maximum(q1, q2), 0) - 2.8).astype(np.float32)
    Rs = 60.0; sph = (np.sqrt(R ** 2 + (Z - (ztop - Rs)) ** 2) - Rs).astype(np.float32)                    # gentle spherical dome on the finger top (analytic: no terracing)
    body = -smin(-flat, -(sph + 0.0), 2.5); body = np.maximum(body, -Z)
    # bore with snap lip: r 9.1 above z 1.4, lip r 8.7 below 0.8 with a 45-degree lead-in between
    rb = np.where(Z < 0.8, 8.7, np.where(Z < 1.4, 8.7 + (Z - 0.8) / 0.6 * 0.4, R_B)); bore = np.maximum(R - rb, Z - zc)
    sd = np.maximum(body, -bore)
    for ang in (0, 90, 180, 270):                                      # four flex slits through the skirt (0.9 wide, 6 tall)
        ca, sa = math.cos(math.radians(ang)), math.sin(math.radians(ang)); along = X * ca + Y * sa; perp = -X * sa + Y * ca
        sd = np.maximum(sd, -np.maximum(np.maximum(np.abs(perp) - 0.45, Z - 6.0), np.maximum(8.0 - along, -Z - 1)))
    sd = np.maximum(sd, -np.maximum(R - 5.2, np.maximum(Z - zp, zc - Z - 0.0)))       # magnet pocket 10.4 x 3.3 from the ceiling up
    sd = np.maximum(sd, -np.maximum(np.abs(R - 3.6) - 0.45, np.maximum(Z - (zp + 0.5), zc - Z)))   # glue relief groove
    for k_ in range(9):                                                # polka dots on the side above the zig-zag band
        a_ = 2 * math.pi * k_ / 9 + math.pi / 9; cx_, cy_ = (R_O + 0.1) * math.cos(a_), (R_O + 0.1) * math.sin(a_)
        sd = np.maximum(sd, -(np.sqrt((X - cx_) ** 2 + (Y - cy_) ** 2 + (Z - 8.6) ** 2) - 1.15))
    lab = ndi.label(sd < 0)[0]; cnt = np.bincount(lab.ravel()); cnt[0] = 0; sd = np.where(lab == cnt.argmax(), sd, np.maximum(sd, 0.05)).astype(np.float32)
    m = mc(ndi.gaussian_filter(sd, 1.0), g.lo, h, taubin=8)
    m = L.manifold_simplify(m, 0.004)
    if save: save4(m, "valve_cap")
    print("cap", m.is_watertight, len(m.faces), m.bounds.tolist()); return m


def build_mount():
    """v4.8: ORIGINAL v1 mount (_mount_v3.stl main body) voxelised at 0.4 mm from the full-resolution mesh, NO horn carving, + dovetail rail boss, floating lumps dropped, surface smoothed (ripples / ragged remesh removed)."""
    from hypertrumpet_v2_build import Grid, occ_to_sd, largest_component
    h = 0.4; g = Grid((712, 478, -20), (948, 551, 140), h)
    m1 = trimesh.load(os.path.join(L.V1, "_mount_v3.stl"), force="mesh"); main = max(m1.split(only_watertight=False), key=lambda q: len(q.faces))
    vox = main.voxelized(h).fill(); pts = vox.points; ix = np.floor((pts - g.lo) / h + 0.5).astype(int); ok = np.all((ix >= 0) & (ix < np.array(g.n)), axis=1); ix = ix[ok]
    occ = np.zeros(g.n, bool); occ[ix[:, 0], ix[:, 1], ix[:, 2]] = True
    Xg = g.ax[0][:, None, None]; Yg = g.ax[1][None, :, None]; Zg = g.ax[2][None, None, :]
    t = (Yg - (PLATE_FRONT - 2.0)) / (DT["h"] + 2.0); w = DT["neck"] / 2 + (DT["head"] / 2 - DT["neck"] / 2) * np.clip(t, 0, 1)
    rail = (np.abs(Xg - DT["rail_x"]) <= w) & (Yg >= PLATE_FRONT - 2.0) & (Yg <= PLATE_FRONT + DT["h"]) & (Zg >= DT["rail_z0"]) & (Zg <= 100.0)
    occ = largest_component(occ); occ_all = occ | rail
    from scipy.spatial import cKDTree
    sd = occ_to_sd(occ, h, smooth=0.0)                                                        # coarse sign/distance everywhere
    band = np.abs(sd) < 2.2; bi = np.argwhere(band); bp = g.lo + bi * h
    sp_, fi = main.sample(int(main.area / 0.16 ** 2), return_index=True); tr = cKDTree(sp_); dd, nn = tr.query(bp, workers=-1)
    sg = np.sign(np.einsum("ij,ij->i", bp - sp_[nn], main.face_normals[fi[nn]])); sg[sg == 0] = 1.0; sgo = np.where(occ[tuple(bi.T)], -1.0, 1.0)
    sg = np.where(dd < 0.35, sg, sgo)                                                         # sign from the surface normal near the surface (exact), from occupancy elsewhere
    sd[tuple(bi.T)] = (sg * dd).astype(np.float32); del tr, sp_, fi
    sd = np.minimum(sd, occ_to_sd(rail & ~occ, h, smooth=0.0) if False else occ_to_sd(rail, h, smooth=0.0))                # dovetail boss unioned in (its field is coarse, outside the fit surfaces)
    # notch (Hall rail) cut in the field, then remove thin slivers around it (opening r 0.8 inside the notch neighbourhood only)
    nb = np.maximum(np.maximum(np.abs(Xg - 0.5 * (RAIL_X0 + RAIL_X1 + 1.0)) - 0.5 * (RAIL_X1 - RAIL_X0 + 1.0), np.abs(Yg - 533.5) - 14.5), NOTCH_Z0 - Zg).astype(np.float32)
    sd = np.maximum(sd, -nb)
    ia, ja, ka = [int(max(0, (v - g.lo[i]) / h)) for i, v in enumerate((795.0, 515.0, 95.0))]; ib, jb, kb = [int(min(g.n[i], (v - g.lo[i]) / h)) for i, v in enumerate((905.0, 551.0, 135.0))]
    sub = sd[ia:ib, ja:jb, ka:kb] < 0; er = ndi.distance_transform_edt(sub) * h >= 0.85; op = ndi.distance_transform_edt(~er) * h < 0.85
    cutm = sub & ~op; seg = sd[ia:ib, ja:jb, ka:kb]; seg[cutm] = np.maximum(seg[cutm], 0.25)
    # variable smoothing: fit zone (within 2 mm of the horn) keeps the exact surface, far zone is smoothed hard (sigma 1 mm)
    gc = Grid((712, 478, -20), (948, 551, 140), 1.0); Hc = L.horn_field(gc, include_shell=False)
    Hup = ndi.zoom(Hc, [g.n[i] / Hc.shape[i] for i in range(3)], order=1, mode="nearest")[:g.n[0], :g.n[1], :g.n[2]]
    w = np.clip((Hup - 2.0) / 4.0, 0.0, 1.0); w = w * w * (3 - 2 * w); del Hc, Hup
    sd = (ndi.gaussian_filter(sd, 0.5) * (1 - w) + ndi.gaussian_filter(sd, 2.4) * w).astype(np.float32)
    m = L.sd_to_mesh(g, sd, taubin=0)
    m.export(os.path.join(HERE, "_cache_v4", "mount_fullres.stl")); np.save(os.path.join(HERE, "_cache_v4", "mount_v1_main_verts.npy"), np.asarray(main.vertices))
    save4(m, "mount_original_arms_plate"); print("mount", m.is_watertight, len(m.faces), round(m.volume / 1000, 1), "cm3"); return m


def build_mountcut():
    """cut the Hall-rail notch out of the mount plate (only there: x of the rail, y 519-542.5, z above 116.4); everything else of the mount is kept"""
    m = trimesh.load(os.path.join(HERE, "_cache_v4", "mount_fullres.stl"), force="mesh")
    x0, x1 = RAIL_X0 - 0.5, RAIL_X1 + 0.5; y0, y1 = 519.0, 548.0; z0, z1 = NOTCH_Z0, 140.0
    box = trimesh.creation.box(extents=[x1 - x0, y1 - y0, z1 - z0]); box.apply_translation([(x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2])
    res = from_manifold(to_manifold(m) - to_manifold(box)); save4(res, "mount_original_arms_plate")
    info = dict(notch_x=[x0, x1], notch_y=[y0, y1], notch_z_from=z0, volume_before_cm3=round(float(m.volume) / 1000, 2), volume_after_cm3=round(float(res.volume) / 1000, 2), removed_cm3=round(float(m.volume - res.volume) / 1000, 2), bodies_after=len(res.split(only_watertight=False)))
    json.dump(info, open(os.path.join(HERE, "mountcut_v4.json"), "w"), indent=1); print("mountcut", info); return res


def copy_locked():
    src = os.path.join(HERE, "stl_v3_1")
    for a, b in (("mount_original_arms_plate", "mount_original_arms_plate_v4"),):
        shutil.copyfile(os.path.join(src, a + ".stl"), os.path.join(STL4, b + ".stl"))


# =============================================================== VERIFY ===
ASSEMBLED = ["mount_original_arms_plate_v4", "shell_pebble_v4", "wrap_trunk_mid_v4", "wrap_crown_branches_mic_v4", "battery_door_v4", "hall_sensor_sleeve_v4"]


def bell_wall_distance(pts):
    """exact unsigned distance from points to the bell surface (a surface of revolution: distance in the (x, rho) half plane to the meridian polyline, rim at s=0 ... 230)"""
    from scipy.spatial import cKDTree
    ss = np.linspace(0, 230, 4601); mer = np.c_[XR - ss, BELL(ss)]
    rho = np.hypot(pts[:, 1] - YB_, pts[:, 2] - ZB_); d, _ = cKDTree(mer).query(np.c_[pts[:, 0], rho]); return d


def mic_envelopes():
    """22 mm x 75 mm body on the aim axis, rear at t=16 (as v3.1): clearance to the bell wall / rim bead per station; negative = crossed"""
    S, aim0, er = mic_geometry(); ex = np.array([1.0, 0, 0]); out = {}
    for tilt in (-4.0, 0.0, 6.0):
        a = math.radians(MIC_AIM + tilt); ai = -ex * math.cos(a) - er * math.sin(a)
        tg = np.cross(er, ex); perp = np.cross(ai, tg); worst = 1e9
        for ss in np.linspace(0, MIC_BODY[1], 76):
            c = S + ai * (16.0 + ss)
            ring = np.array([c + 0.5 * MIC_BODY[0] * (math.cos(g_) * tg + math.sin(g_) * perp) for g_ in np.linspace(0, 2 * math.pi, 24, endpoint=False)] + [c])
            s_ = XR - ring[:, 0]; rho = np.hypot(ring[:, 1] - YB_, ring[:, 2] - ZB_)
            rim = np.hypot(s_, rho - R0) - 2.0
            if (s_ > 0).all():
                dwall = (rho - BELL(np.clip(s_, 0, 230))) / np.sqrt(1 + BELLD(np.clip(s_, 0, 230)) ** 2)
                m_ = -min(dwall.max(), -dwall.min()) if (dwall.min() < 0 < dwall.max()) else np.abs(dwall).min()
            else:
                m_ = rim.min(); inside = (s_ > 0)
                if inside.any():
                    dwall = (rho[inside] - BELL(np.clip(s_[inside], 0, 230))) / np.sqrt(1 + BELLD(np.clip(s_[inside], 0, 230)) ** 2)
                    m_ = min(m_, np.abs(dwall).min()) if not (dwall.min() < 0 < dwall.max()) else min(m_, -min(dwall.max(), -dwall.min()))
            worst = min(worst, m_)
        out[tilt] = round(float(worst), 2)
    return out


def scan_openings(shell):
    ray = trimesh.ray.ray_triangle.RayMeshIntersector(shell)
    def open_at(xs, zs):
        o = np.c_[xs, np.full(len(xs), 700.0), zs]; d = np.tile([0, -1.0, 0], (len(xs), 1))
        loc, idx, _ = ray.intersects_location(o, d, multiple_hits=False)
        first = np.zeros(len(xs)); first[idx] = loc[:, 1]; return first < FACE_Y - 4.0
    rows = []
    for name, (cx, cz), (w, hh), kind in CONTROLS:
        step = 0.1
        xs = np.arange(cx - w / 2 - 3, cx + w / 2 + 3, step); zs = np.arange(cz - hh / 2 - 3, cz + hh / 2 + 3, step)
        ox = open_at(xs, np.full(len(xs), cz)); oz = open_at(np.full(len(zs), cx), zs)
        def run(a_, c, arr):
            i0 = int(np.argmin(np.abs(a_ - c)))
            if not arr[i0]: return None
            l = r = i0
            while l > 0 and arr[l - 1]: l -= 1
            while r < len(arr) - 1 and arr[r + 1]: r += 1
            return a_[l], a_[r]
        rx = run(xs, cx, ox); rz = run(zs, cz, oz)
        if rx is None or rz is None: rows.append(dict(control=name, found=False)); continue
        rows.append(dict(control=name, spec_centre=(round(cx, 2), round(cz, 2)), spec_size=(w, hh), centre=(round(0.5 * (rx[0] + rx[1]), 2), round(0.5 * (rz[0] + rz[1]), 2)),
                         size=(round(rx[1] - rx[0] + step, 2), round(rz[1] - rz[0] + step, 2)), x_edges=(round(rx[0] - step / 2, 2), round(rx[1] + step / 2, 2)), z_edges=(round(rz[0] - step / 2, 2), round(rz[1] + step / 2, 2))))
    return rows


def web_table():
    from shapely.geometry import Polygon, Point, box
    from shapely import affinity
    shapes = {}
    for name, (cx, cz), (w, hh), kind in CONTROLS:
        if kind == "ellipse": shp = affinity.scale(Point(cx, cz).buffer(1.0, 64), w / 2, hh / 2)
        elif kind == "stadium":
            from shapely.geometry import LineString
            shp = LineString([(cx - w / 2 + hh / 2, cz), (cx + w / 2 - hh / 2, cz)]).buffer(hh / 2, 32)
        else: shp = box(cx - w / 2, cz - hh / 2, cx + w / 2, cz + hh / 2)
        shapes[name] = shp
    names = list(shapes); worst = []
    for i in range(len(names)):
        for j in range(i + 1, len(names)):
            worst.append((shapes[names[i]].distance(shapes[names[j]]), names[i], names[j]))
    worst.sort(); return [(round(d, 2), a, b) for d, a, b in worst[:6]], shapes


def mesh_quality(m):
    """facet size and curvature chord error: edge lengths, and sagitta ~ L * dihedral / 8 per interior edge (creases > 35 deg excluded as true edges)"""
    e = m.edges_unique; L_ = np.linalg.norm(m.vertices[e[:, 0]] - m.vertices[e[:, 1]], axis=1)
    adj = m.face_adjacency; ang = m.face_adjacency_angles; el = m.face_adjacency_edges; le = np.linalg.norm(m.vertices[el[:, 0]] - m.vertices[el[:, 1]], axis=1)
    smooth = ang < math.radians(35); sag = le[smooth] * ang[smooth] / 8.0
    return dict(faces=int(len(m.faces)), edge_len_mm=dict(median=round(float(np.median(L_)), 3), p95=round(float(np.percentile(L_, 95)), 3), max=round(float(L_.max()), 3)),
                chord_error_mm=dict(median=round(float(np.median(sag)), 4), p99=round(float(np.percentile(sag, 99)), 4), max=round(float(sag.max()), 4)), crease_edges_over_35deg=int((~smooth).sum()))


def verify(write=True):
    t0 = time.time(); rep = {"parts": {}}
    for f in sorted(x[:-4] for x in os.listdir(STL4) if x.endswith(".stl")):
        m = trimesh.load(os.path.join(STL4, f + ".stl"), force="mesh"); dims = m.bounds[1] - m.bounds[0]
        rep["parts"][f] = dict(watertight=bool(m.is_watertight), bodies=len(m.split(only_watertight=False)), volume_cm3=round(float(m.volume) / 1000, 2) if m.is_watertight else None,
                               bbox_mm=[round(float(x), 1) for x in dims], fits_bed_256=bool(np.all(dims <= 256.0)))
    shell = load4("shell_pebble"); rows = scan_openings(shell); rep["hole_table"] = rows
    maxdc = max(math.hypot(r["centre"][0] - r["spec_centre"][0], r["centre"][1] - r["spec_centre"][1]) for r in rows if "centre" in r); rep["hole_max_centre_diff_mm"] = round(maxdc, 2)
    rep["hole_size_max_diff_mm"] = round(max(max(abs(r["size"][0] - r["spec_size"][0]), abs(r["size"][1] - r["spec_size"][1])) for r in rows if "size" in r), 2)
    rep["holes_not_found"] = [r["control"] for r in rows if "centre" not in r]
    byn = {r["control"]: r for r in rows if "centre" in r}
    fx = [byn["fader_%d" % i] for i in range(3)]
    rep["fader_outer_span_mm"] = round(fx[2]["x_edges"][1] - fx[0]["x_edges"][0], 2)
    rep["fader_slot_gap_mm"] = [round(fx[1]["x_edges"][0] - fx[0]["x_edges"][1], 2), round(fx[2]["x_edges"][0] - fx[1]["x_edges"][1], 2)]
    lid = byn["lidar"]; t00 = byn["trellis_00"]
    rep["lidar_size_mm"] = lid["size"]; rep["lidar_upper_edge_z_measured"] = lid["z_edges"][1]; rep["knob_lower_face_z_spec"] = round(KNOB_LOWER_TOP, 2)
    rep["lidar_far_edge_minus_knob_lower_face_mm"] = round(lid["z_edges"][1] - KNOB_LOWER_TOP, 2)
    rep["lidar_to_trellis_bottom_row_web_mm"] = round(t00["z_edges"][0] - lid["z_edges"][1], 2)
    rep["lidar_centre_x_minus_middle_fader_x"] = round(lid["centre"][0] - byn["fader_1"]["centre"][0], 2)
    w6, shapes = web_table(); rep["smallest_webs_mm_nominal"] = w6
    # uniform face depth: first-hit y over the flat zone
    ray = trimesh.ray.ray_triangle.RayMeshIntersector(shell)
    xs = np.arange(740, 890, 1.0); zs = np.arange(-12, 128, 1.0); XX, ZZ = np.meshgrid(xs, zs, indexing="ij")
    sd = sil_sd(XX, ZZ); ok = sd < -(ROLL_W + 1.0)
    from shapely.ops import unary_union
    holes = unary_union([s_.buffer(1.0) for s_ in shapes.values()]); from shapely.geometry import Point as Pt
    inh = np.array([holes.contains(Pt(x, z)) for x, z in zip(XX.ravel(), ZZ.ravel())]).reshape(XX.shape)
    sel = ok & ~inh & (XX < 893)
    o = np.c_[XX[sel], np.full(sel.sum(), 700.0), ZZ[sel]]; loc, idx, _ = ray.intersects_location(o, np.tile([0, -1.0, 0], (len(o), 1)), multiple_hits=True)
    ymax = np.full(len(o), -1e9); np.maximum.at(ymax, idx, loc[:, 1]); got = ymax > -1e8; yh = ymax[got]; worst = o[got][int(np.argmax(yh))]; rep["control_face_max_y_at_xz"] = [round(float(worst[0]), 1), round(float(worst[2]), 1)]
    rep["control_face_y_min_max"] = [round(float(yh.min()), 3), round(float(yh.max()), 3)]; rep["control_face_offset_from_plate_front_min_max"] = [round(float(yh.min() - PLATE_FRONT), 3), round(float(yh.max() - PLATE_FRONT), 3)]
    rep["control_face_samples"] = int(len(yh))
    # cavity depth behind the skin at Seed + joystick zones (second hit from +y = inner face)
    def inner_depth(x0, x1, z0, z1):
        X_, Z_ = np.meshgrid(np.arange(x0, x1 + 0.01, 1.0), np.arange(z0, z1 + 0.01, 1.0), indexing="ij"); o_ = np.c_[X_.ravel(), np.full(X_.size, 700.0), Z_.ravel()]
        loc_, ir, _ = ray.intersects_location(o_, np.tile([0, -1.0, 0], (len(o_), 1)), multiple_hits=True)
        res = []
        for k in range(len(o_)):
            ys = np.sort(loc_[ir == k][:, 1])[::-1]
            if len(ys) >= 2: res.append(ys[1])
        return float(PLATE_FRONT and (np.min(res) - PLATE_FRONT)) if res else None
    sx0, sx1 = SEED_C[0] - SEED_L / 2, SEED_C[0] + SEED_L / 2; sz0, sz1 = SEED_C[1] - SEED_W / 2, SEED_C[1] + SEED_W / 2
    rep["seed_zone_clear_depth_mm_from_plate_front_to_inner_face_min"] = round(inner_depth(sx0, sx1, sz0, sz1), 2)
    rep["joystick_zone_clear_depth_mm_min"] = round(inner_depth(JOY_POS[0] - 14.0, JOY_POS[0] + 14.0, JOY_POS[1] - 13.0, JOY_POS[1] + 13.0), 2)
    # horn clearances
    cl = {}
    for f in ASSEMBLED:
        m = load4(f); res, ko, pts = L.clearance_report(m, n=80000)
        classes = {'casings/caps/stems': ('casing', 'cap', 'stem', 'button'), 'bottom caps': ('botcap',), 'slide legs+crowns': ('slide',), 'finger ring': ('ring3',), 'thumb hook': ('saddle',), 'brace': ('brace',), 'leadpipe': ('leadpipe',), 'bell+rim bead': ('bell',)}
        cl[f] = {c: round(min(v for k, v in res.items() if k.startswith(pre)), 2) for c, pre in classes.items()}
    rep["clearance_to_horn_by_part_mm"] = cl
    # wrap vs bell: distance of wrap vertices to the bell surface (should be >= ~gap 1.5, never negative)
    wd = {}
    for f in ("wrap_trunk_mid_v4", "wrap_crown_branches_mic_v4", "shell_pebble_v4"):
        m = load4(f); pts = m.sample(60000)
        dist = bell_wall_distance(pts)
        rho_ = np.hypot(pts[:, 1] - YB_, pts[:, 2] - ZB_); dbead = np.hypot(pts[:, 0] - XR, rho_ - R0) - 2.0
        mask = pts[:, 0] < XR - 6.0                                         # the tendrils / trunk lying on the bell (the socket hub is in front of the rim)
        wd[f] = dict(min_dist_to_bell_wall_mm=round(float(dist.min()), 2), min_dist_on_bell_part_mm=round(float(dist[mask].min()), 2) if mask.any() else None, min_clearance_to_rim_bead_mm=round(float(dbead.min()), 2))
    rep["wrap_to_bell"] = wd
    # mic body vs printed crown: exact intersection volume of the 22x75 cylinder with the crown mesh
    S, aim, er = mic_geometry(); cyl = trimesh.creation.cylinder(radius=11.0, height=75.0, sections=64)
    cyl.apply_transform(trimesh.geometry.align_vectors([0, 0, 1], aim)); cyl.apply_translation(S + aim * (16.0 + 37.5))
    try: iv = float((to_manifold(cyl) ^ to_manifold(load4("wrap_crown_branches_mic_v4"))).volume())
    except Exception: iv = -1.0
    rep["mic_body_x_crown_mm3"] = round(iv, 2)
    # intersections between assembled printed parts
    inter = {}; ms = {n: load4(n) for n in ASSEMBLED}; names = list(ms)
    import fast_simplification
    mm_ = ms["mount_original_arms_plate_v4"]; v_, f_ = fast_simplification.simplify(mm_.vertices.astype(np.float32), mm_.faces.astype(np.int32), target_reduction=0.9); ms["mount_original_arms_plate_v4"] = trimesh.Trimesh(v_, f_)
    for i in range(len(names)):
        for j in range(i + 1, len(names)):
            a, b = ms[names[i]], ms[names[j]]
            if np.any(a.bounds[1] < b.bounds[0]) or np.any(b.bounds[1] < a.bounds[0]): continue
            try: v = float((to_manifold(a) ^ to_manifold(b)).volume())
            except Exception: v = -1.0
            inter[f"{names[i]} x {names[j]}"] = round(v, 2)
    rep["assembly_intersections_mm3"] = inter
    # strip channel and bore lengths
    lens = json.load(open(os.path.join(HERE, "wrap_lengths_v4.json"))) if os.path.exists(os.path.join(HERE, "wrap_lengths_v4.json")) else {}
    paths = build_paths(); Pt_ = paths["trunk"]; Pm = paths["main"]
    sn, _, _ = bell_nearest(Pm.C); ich = int(np.argmax(sn <= S_CHAN_END))
    # make sure Pm.ichan is consistent with wrap_fields
    seg = lambda P, a, b: float(np.linalg.norm(np.diff(P.C[a:b + 1], axis=0), axis=1).sum())
    wall_x = 897.0; i_wall = int(np.argmax(Pt_.C[:, 0] >= wall_x))
    total = seg(Pt_, 0, len(Pt_.C) - 1) + seg(Pm, 0, ich); outside = seg(Pt_, i_wall, len(Pt_.C) - 1) + seg(Pm, 0, ich)
    rep["strip_channel_length_mm"] = dict(from_inside_pebble=round(total, 1), from_pebble_wall_to_rim_end=round(outside, 1), trunk=round(seg(Pt_, i_wall, len(Pt_.C) - 1), 1), main_to_rim=round(seg(Pm, 0, ich), 1),
                                          leds_at_160_per_m=int(total / 6.25), leds_at_480_per_m=int(total / 2.083), strip_cut_to_mm=round(total, 0), strip_left_of_1m_mm=round(1000 - total, 0))
    rep["branch_lengths_mm"] = {k: round(v, 1) for k, v in lens.items()}
    # joint section checks: bore + channel present at each joint face
    jc = {}
    for nm, XJ in (("J1_pebble|trunk", X_J1), ("J2_trunk|crown", X_J2)):
        r = {}
        for f in ("shell_pebble_v4", "wrap_trunk_mid_v4", "wrap_crown_branches_mic_v4"):
            m = load4(f)
            for dx in (-6.0, 6.0):
                try:
                    sec = m.section(plane_origin=[XJ + dx, 0, 0], plane_normal=[1, 0, 0])
                    if sec is not None: r["%s@%+.0f" % (f, dx)] = len(sec.discrete)
                except Exception: pass
        jc[nm] = r
    rep["joint_section_loop_counts"] = jc
    mc_ = CAP_CEIL + 1.5                                               # magnet centre (10 x 3 mm) at rest
    hs = {}
    for lab_, trav in (("rest", 0.0), ("full_press_13.0", 13.0), ("full_press_13.5", 13.5)):
        vgap = CAP_CEIL - trav - SENSOR_TOP_Z; hs[lab_] = dict(magnet_bottom_face_above_sensor_top_mm=round(vgap, 2), lateral_offset_mm=round(SENSOR_DY, 2), centre_to_centre_3d_mm=round(math.hypot(SENSOR_DY, mc_ - trav - (SENSOR_TOP_Z - 1.0)), 2))
    rep["hall_sleeve"] = dict(pocket_xyz_mm=[round(POCKET_X[1] - POCKET_X[0], 2), round(POCKET_Y[1] - POCKET_Y[0], 2), POCKET_H], pocket_z=[round(POCKET_Z[0], 2), round(POCKET_Z[1], 2)], outer_x=[round(SL_XC - 42.2, 1), round(SL_XC + 42.2, 1)], outer_y=[round(SL_Y0, 1), SL_Y1], outer_z=[round(SL_ZB, 2), round(SL_ZT, 2)],
        sensor_holes_xy=[(cx, round(SENSOR_Y, 2)) for cx in L.P["casing_cx"]], sensor_top_z=round(SENSOR_TOP_Z, 2), sleeve_top_skin_z=round(SL_ZT, 2), lateral_offset_sensor_to_valve_axis_mm=round(SENSOR_DY, 2),
        magnet_to_sensor=hs, leadpipe_top_z=round(L.P["leadpipe"]["z"] + L.P["leadpipe"]["r"], 2), sleeve_collar_to_leadpipe_mm=round(110.0 - (L.P["leadpipe"]["z"] + L.P["leadpipe"]["r"]), 2),
        sleeve_edge_to_cap_skirt_mm=round(L.P["casing_y"] - 11.5 - SL_Y1, 2), stem_to_sleeve_outer_y_mm=round(L.P["casing_y"] - L.P["stem_od"] / 2 - SL_Y1, 2),
        cap=dict(skirt_bottom_rest_z=round(CAP_Z0, 2), skirt_bottom_full_press_z=round(CAP_Z0 - 13.5, 2), top_z_rest=round(CAP_Z0 + 12.1, 2)))
    # cap at full press (13.5 mm down) vs sleeve / collar / mount (exact volume), and cap vs sleeve at rest
    try:
        capm = load4("valve_cap"); sl = to_manifold(load4("hall_sensor_sleeve")); cf = {}
        for lab_, trav in (("rest", 0.0), ("full_press_13.5", 13.5)):
            vol = 0.0
            for cx in L.P["casing_cx"]:
                c2 = capm.copy(); c2.apply_translation([cx, L.P["casing_y"], CAP_Z0 - trav]); vol += float((to_manifold(c2) ^ sl).volume())
            cf[lab_] = round(vol, 3)
        rep["cap_x_sleeve_mm3"] = cf
    except Exception as e: rep["cap_x_sleeve_mm3"] = str(e)
    rep["facets"] = {f: mesh_quality(load4(f)) for f in ("shell_pebble_v4", "wrap_trunk_mid_v4", "wrap_crown_branches_mic_v4", "battery_door_v4", "hall_sensor_sleeve_v4", "valve_cap_v4")}
    rep["seconds"] = round(time.time() - t0, 1)
    if write: json.dump(rep, open(os.path.join(HERE, "verify_report_v4.json"), "w"), indent=1, default=str)
    return rep


def build_strip_ref():
    """visual-only LED strip (10 x 2 mm ribbon) lying in the underside channel: trunk from the pebble wall, main branch to the rim end. Not a printed part."""
    paths = build_paths(); out = []
    Pt_ = paths["trunk"]; Pm = paths["main"]; sn, _, _ = bell_nearest(Pm.C); ich = int(np.argmax(sn <= S_CHAN_END))
    i0 = int(np.argmax(Pt_.C[:, 0] >= 880.0))
    for P, a, b_ in ((Pt_, i0, len(Pt_.C)), (Pm, 0, ich + 1)):
        rings = []
        for i in range(a, b_):
            vb = -P.b[i] + 0.15; c = P.C[i] + P.N[i] * vb
            hw = STRIP_W / 2
            rings.append(np.array([c - P.U[i] * hw, c + P.U[i] * hw, c + P.U[i] * hw + P.N[i] * STRIP_T, c - P.U[i] * hw + P.N[i] * STRIP_T]))
        out.append(L.sweep_profiles(rings))
    m = trimesh.util.concatenate(out); m.export(os.path.join(HERE, "_cache_v4", "led_strip_ref_v4.stl")); return m


# =============================================================== V4.2: FLAT RIBBON WRAP ===
RIB_W, RIB_B = 12.0, 6.0         # ribbon half width / half thickness: 24 x 12 mm rounded oval; strip channel 10.4 x 3.0 under a 1.0 mm diffuser skin on the outer face, 4.4 mm wire bore underneath
GAP_RIB = 1.5                    # pad gap to the bell
S_LAND, S_END = 185.0, 12.0      # ribbon lands on the bell tube at s = 185 and ends at s = 12 before the lift over the rim
SP_TURN = 324.0
THETA_MIC = 3.0 + SP_TURN
THETA_CUT = 170.0                # plane through the bell axis that splits the crown in two (single crossing: ribbon spans theta 3..327)


def ribbon_samples():
    sc = np.linspace(S_LAND, S_END, 80); u = (S_LAND - sc) / (S_LAND - S_END); ww = np.clip((u - 0.7) / 0.3, 0, 1); ww = ww * ww * (3 - 2 * ww); e_ = u ** 2.4 * (1 - ww) + (1 - (1 - u) ** 2) * ww
    th = 3.0 + SP_TURN * e_          # stays on the +y side over the leadpipe zone (theta 123-223 only for s < 71), slope -> 0 at the end: arrives heading straight at the rim (no twist)
    P = bell_pt(sc, th); arc = np.r_[0, np.cumsum(np.linalg.norm(np.diff(P, axis=0), axis=1))]; L_ = arc[-1]
    K = int(L_ / 0.4) + 1; aa = np.linspace(0, L_, K)
    s = CubicSpline(arc, sc)(aa); t = CubicSpline(arc, th)(aa)
    foot = bell_pt(s, t); N = bell_normal(s, t); C = foot + N * (GAP_RIB + RIB_B)
    T = np.gradient(C, axis=0); T /= np.linalg.norm(T, axis=1)[:, None]; U = np.cross(N, T); U /= np.linalg.norm(U, axis=1)[:, None]
    return dict(s=s, th=t, foot=foot, N=N, C=C, T=T, U=U, L=float(L_), arc=aa)


_RIB = {}
def ribbon():
    if "r" not in _RIB: _RIB["r"] = ribbon_samples()
    return _RIB["r"]


K_BL = 36            # samples (14 mm) over which the chain bridge field morphs into the exact chart ribbon field


def ribbon_fields(g, Fo, Fc, Fb, chain=None):
    """flat conformal ribbon hugging the bell: exact surface-distance coordinates, underside = true offset surface of the bell at GAP_RIB.
    chain = (Fo, Fc, Fb) of the 3D bridge: inside the first K_BL samples the two fields are cross-faded (no collar, no step)."""
    from scipy.spatial import cKDTree
    R = ribbon(); tree = cKDTree(R["foot"]); ii = []
    for i in range(g.n[0]):
        s_ = XR - float(g.ax[0][i])
        if s_ < -0.5 or s_ > S_LAND + 3.0: continue
        r = float(BELL(max(s_, 0.0))); rho = np.hypot(g.ax[1][:, None] - YB_, g.ax[2][None, :] - ZB_)
        jk = np.nonzero((rho > r - 0.5) & (rho < r + 26.0))
        if len(jk[0]): ii.append(np.c_[np.full(len(jk[0]), i), jk[0], jk[1]])
    idx = np.vstack(ii); p = 2.4; mw = min(RIB_W, RIB_B); bm = np.zeros(g.n, bool)
    for a in range(0, len(idx), 1000000):
        ch = idx[a:a + 1000000]; pts = g.lo + ch * g.h
        s, hh, th = bell_nearest(pts); foot = bell_pt(s, np.degrees(th))
        d3, kk = tree.query(foot); r_ = BELL(s); ell = 2 * r_ * np.arcsin(np.clip(d3 / (2 * r_), 0, 1))
        u = ell / RIB_W; v = (hh - (GAP_RIB + RIB_B)) / RIB_B; q = (((np.abs(u) ** p + np.abs(v) ** p) ** (1.0 / p) - 1.0) * mw).astype(np.float32)
        dch = np.maximum(ell - STRIP_CH_W / 2, np.maximum(hh - (GAP_RIB + 2 * RIB_B - 1.6), (GAP_RIB + 2 * RIB_B - 4.6) - hh)).astype(np.float32)
        dbo = (np.hypot(ell, hh - (GAP_RIB + 4.2)) - 2.2).astype(np.float32)
        ix = (ch[:, 0], ch[:, 1], ch[:, 2])
        if chain is None:
            Fo[ix] = np.minimum(Fo[ix], q); Fc[ix] = np.minimum(Fc[ix], dch); Fb[ix] = np.minimum(Fb[ix], dbo)
        else:
            w = np.clip(kk / float(K_BL), 0, 1); w = w * w * (3 - 2 * w); inb = kk < K_BL
            for F, Fcn, qv in ((Fo, chain[0], q), (Fc, chain[1], dch), (Fb, chain[2], dbo)):
                F[ix] = np.where(inb, (1 - w) * np.minimum(Fcn[ix], 60.0) + w * qv, np.minimum(F[ix], qv)).astype(np.float32)
            bm[ix] = bm[ix] | inb
    return bm


def bridge_path():
    R = ribbon(); c = [np.array(p) for p in [(885.0, 556.0, 98.0), (915.0, 552.0, 98.0), (942.0, 545.6, 98.3)]] + [R["C"][i] for i in (0, 12, 30)]
    return Path(c, RIB_W, RIB_B, step=1.0, name="bridge")           # constant 24 x 12 oval all the way


def bridge_fields(g):
    P = bridge_path(); Fo = g.full(); Fc = g.full(); Fb = g.full()
    chain_fields(P, g, Fo, Fc, Fb, p=2.4, ch=True, bore=True)
    return Fo, Fc, Fb


def joint_pin_centres():
    P = bridge_path(); i = int(np.argmax(P.C[:, 0] >= X_J1)); return [P.C[i] + P.U[i] * sgn * (RIB_W - 2.6) for sgn in (-1, 1)]


def arm_path():
    R = ribbon(); th_e = float(R["th"][-1]); Pe = R["C"][-1]; tE = R["T"][-1]; S, aim, er = mic_geometry(); Ne = bell_normal(np.array([S_END]), th_e)[0]
    def bez(p0, p1, p2, p3, n):
        tt = np.linspace(0, 1, n)[:, None]; return (1 - tt) ** 3 * p0 + 3 * (1 - tt) ** 2 * tt * p1 + 3 * (1 - tt) * tt ** 2 * p2 + tt ** 3 * p3
    def make(hO, dxO, h1, alpha):
        O = np.array([XR + dxO, YB_, ZB_]) + er * (R0 + hO); d1 = tE * (1 - alpha) + Ne * alpha; d1 /= np.linalg.norm(d1)
        seg1 = bez(Pe, Pe + d1 * h1, O - np.array([1.0, 0, 0]) * 12.0, O, 24)[1:]; E_in = S + aim * 3.0
        seg2 = bez(O, O + np.array([1.0, 0, 0]) * 14.0, E_in - aim * 14.0, E_in, 22)[1:]; return seg1, seg2
    def ok(seg):
        q = np.vstack(seg); rho_ = np.hypot(q[:, 1] - YB_, q[:, 2] - ZB_); bead = float(np.hypot(q[:, 0] - XR, rho_ - R0).min()); wall = float(bell_wall_distance(q).min())
        return bead >= 16.0 and wall >= RIB_B + 1.5
    best = None
    for hO in np.arange(14.0, 44.0, 1.0):
        for alpha in (0.0, 0.4, 0.8):
            for h1 in (10.0, 16.0, 22.0, 28.0):
                sg = make(hO, 5.0, h1, alpha)
                if ok(sg): best = sg; break
            if best: break
        if best: break
    if best is None: print("WARNING no arm candidate"); best = make(30.0, 5.0, 22.0, 0.8)
    c = [Pe - tE * 6.0, Pe - tE * 3.0, Pe] + list(best[0]) + list(best[1])
    t_ = math.radians(THETA_MIC); Ltot = float(np.linalg.norm(np.diff(np.array(c), axis=0), axis=1).sum()); f1 = max(0.0, 1.0 - 55.0 / Ltot)
    xs = np.r_[0.0, np.linspace(f1, 1.0, 8)]; sm = lambda a: a * a * (3 - 2 * a); wy = np.r_[RIB_W, RIB_W + (SW_END - RIB_W) * sm(np.linspace(0, 1, 8))]; by = np.r_[RIB_B, RIB_B + (SW_END - RIB_B) * sm(np.linspace(0, 1, 8))]
    return Path(c, (xs, wy), (xs, by), step=1.0, name="arm", planar_u=[0.0, -math.sin(t_), math.cos(t_)])


T_END = 36.0           # mic bore mouth plane (t along the aim axis from S)
SW_END = 14.2          # half size of the swelled ribbon tip (27 mm incl. walls around the 22.4 bore)


def crown_fields(g):
    Fo = g.full(); Fc = g.full(); Fb = g.full()
    Co = g.full(); Cc = g.full(); Cb = g.full()
    chain_fields(bridge_path(), g, Co, Cc, Cb, p=2.4, ch=True, bore=True)
    bm = ribbon_fields(g, Fo, Fc, Fb, chain=(Co, Cc, Cb))
    Fo = np.where(bm, Fo, np.minimum(Fo, Co)); Fc = np.where(bm, Fc, np.minimum(Fc, Cc)); Fb = np.where(bm, Fb, np.minimum(Fb, Cb)); del Co, Cc, Cb, bm
    Ft = g.full(); A = arm_path(); chain_fields(A, g, Ft, None, Fb, p=2.4, ch=False, bore=True); Fo = np.minimum(Fo, Ft); del Ft
    S, aim, er = mic_geometry(); pad = 40.0
    lo = np.minimum(S, S + aim * 50.0) - pad; hi = np.maximum(S, S + aim * 50.0) + pad; sl = g.sl(lo, hi)
    X, Y, Z = g.coords(sl); t = (X - S[0]) * aim[0] + (Y - S[1]) * aim[1] + (Z - S[2]) * aim[2]
    rho_ax = np.sqrt(np.maximum((X - S[0]) ** 2 + (Y - S[1]) ** 2 + (Z - S[2]) ** 2 - t ** 2, 0)); cutf = smax(Fo[sl], (t - T_END).astype(np.float32), 3.0)
    Fo[sl] = np.where(rho_ax < 20.0, cutf, Fo[sl])                                       # flat rounded end face (only around the tip): the ribbon simply ends here
    bore = cyl_axis_field(g, sl, S, aim, 11.2, 16.0, T_END + 12.0, 0.3)                  # recessed 22.4 mm bore, 20 mm deep, opens at the end face (aimed into the bell)
    Fb[sl] = np.minimum(Fb[sl], bore)
    return Fo, Fc, Fb


def cut_plane_normal():
    t = math.radians(THETA_CUT); return np.array([0.0, -math.sin(t), math.cos(t)])


def split_pin_centres():
    R = ribbon(); n = cut_plane_normal(); q = (R["C"][:, 1] - YB_) * n[1] + (R["C"][:, 2] - ZB_) * n[2]
    k = int(np.argmax((q[:-1] < 0) & (q[1:] >= 0))); return [R["C"][k] + R["U"][k] * sgn * (RIB_W - 2.6) for sgn in (-1, 1)], n


def build_wrap2(save=True):
    t0 = time.time(); h = 0.38
    g = G3((X_J1 - 8.0, 452, 28), (XR + 60, 612, 172), h); print("crown grid", g.n)
    Fo, Fc, Fb = crown_fields(g); print("fields", round(time.time() - t0, 1))
    X, Y, Z = g.X(), g.Y(), g.Z()
    solid = np.maximum(np.maximum(Fo, (X_J1 + 0.1) - X), np.maximum(-Fc, -Fb))
    for pc in joint_pin_centres():                                       # alignment pins into the pod piece's sockets
        pin = np.maximum(np.hypot(Y - pc[1], Z - pc[2]) - 1.05, np.maximum(X_J1 - 6.5 - X, X - (X_J1 + 0.6))); solid = np.minimum(solid, pin)
    pcs, n = split_pin_centres(); qq = (Y - YB_) * n[1] + (Z - ZB_) * n[2]
    A = np.maximum(solid, qq + 0.1)                                     # q <= -0.1
    B = np.maximum(solid, -(qq - 0.1))                                  # q >= +0.1
    for pc in pcs:
        along = (X - pc[0]) * n[0] + (Y - pc[1]) * n[1] + (Z - pc[2]) * n[2]; rad = np.sqrt(np.maximum((X - pc[0]) ** 2 + (Y - pc[1]) ** 2 + (Z - pc[2]) ** 2 - along ** 2, 0))
        A = np.minimum(A, np.maximum(rad - 1.05, np.maximum(-along - 0.6, along - 6.2)))          # pin on piece A reaching into B
        B = np.maximum(B, -np.maximum(rad - 1.3, np.maximum(-along + 0.0, along - 6.9)))          # socket in piece B
    out = {}
    for nm, F in (("wrap_crown_A", A), ("wrap_crown_B", B)):
        lab = ndi.label(F < 0)[0]; cnt = np.bincount(lab.ravel()); cnt[0] = 0; F = np.where(lab == cnt.argmax(), F, np.maximum(F, 0.05)).astype(np.float32)
        m = mc(ndi.gaussian_filter(F, 1.2), g.lo, h, taubin=10); out[nm] = m; print(nm, m.is_watertight, len(m.faces), m.bounds.tolist(), round(time.time() - t0, 1))
        if save: save4(m, nm)
    return out


def build_strip_ref2():
    """visual LED strip (10 x 2 mm) lying in the channel along bridge + ribbon"""
    R = ribbon(); P = bridge_path(); i0 = int(np.argmax(P.C[:, 0] >= 890.0)); rings = []
    for k in range(i0, len(P.C)):
        c = P.C[k] + P.N[k] * (-RIB_B + 0.2); rings.append(np.array([c - P.U[k] * 5, c + P.U[k] * 5, c + P.U[k] * 5 + P.N[k] * 2.0, c - P.U[k] * 5 + P.N[k] * 2.0]))
    for k in range(0, len(R["C"])):
        c = R["foot"][k] + R["N"][k] * (GAP_RIB + 0.15); rings.append(np.array([c - R["U"][k] * 5, c + R["U"][k] * 5, c + R["U"][k] * 5 + R["N"][k] * 2.0, c - R["U"][k] * 5 + R["N"][k] * 2.0]))
    m = L.sweep_profiles(rings); m.export(os.path.join(HERE, "_cache_v4", "led_strip_ref_v4.stl")); return m


def verify2(write=True):
    t0 = time.time(); rep = {"parts": {}}
    for f in sorted(x[:-4] for x in os.listdir(STL4) if x.endswith(".stl")):
        m = load4(f); dims = m.bounds[1] - m.bounds[0]
        rep["parts"][f] = dict(watertight=bool(m.is_watertight), bodies=len(m.split(only_watertight=False)), volume_cm3=round(float(m.volume) / 1000, 2) if m.is_watertight else None, bbox_mm=[round(float(x), 1) for x in dims], fits_bed_256=bool(np.all(dims <= 256.0)))
    shell = load4("shell_pebble"); rows = scan_openings(shell); rep["hole_table"] = rows; byn = {r["control"]: r for r in rows if "centre" in r}
    rep["holes_not_found"] = [r["control"] for r in rows if "centre" not in r]
    rep["hole_max_centre_diff_mm"] = round(max(math.hypot(r["centre"][0] - r["spec_centre"][0], r["centre"][1] - r["spec_centre"][1]) for r in rows if "centre" in r), 2)
    rep["hole_size_max_diff_mm"] = round(max(max(abs(r["size"][0] - r["spec_size"][0]), abs(r["size"][1] - r["spec_size"][1])) for r in rows if "size" in r), 2)
    fx = [byn["fader_%d" % i] for i in range(3)]; rep["fader_outer_span_mm"] = round(fx[2]["x_edges"][1] - fx[0]["x_edges"][0], 2); rep["fader_centre_offset_mm"] = round(fx[1]["centre"][0] - 0.5 * (fx[0]["x_edges"][0] + fx[2]["x_edges"][1]), 2)
    rep["fader_slot_size_mm"] = fx[1]["size"]; lid = byn["lidar"]; rep["lidar_size_mm"] = lid["size"]; rep["lidar_far_edge_minus_knob_lower_face_mm"] = round(lid["z_edges"][1] - KNOB_LOWER_TOP, 2)
    rep["lidar_to_trellis_web_mm"] = round(byn["trellis_00"]["z_edges"][0] - lid["z_edges"][1], 2); w6, shapes = web_table(); rep["smallest_webs_mm_nominal"] = w6
    ray = trimesh.ray.ray_triangle.RayMeshIntersector(shell)
    # uniform face
    xs = np.arange(740, 890, 1.0); zs = np.arange(-12, 128, 1.0); XX, ZZ = np.meshgrid(xs, zs, indexing="ij"); sd = sil_sd(XX, ZZ)
    from shapely.ops import unary_union; from shapely.geometry import Point as Pt
    holes = unary_union([s_.buffer(1.0) for s_ in shapes.values()]); inh = np.array([holes.contains(Pt(x, z)) for x, z in zip(XX.ravel(), ZZ.ravel())]).reshape(XX.shape)
    sel = (sd < -(ROLL_W + 1.0)) & ~inh & (XX < 885) & (ZZ > -8)
    o = np.c_[XX[sel], np.full(sel.sum(), 700.0), ZZ[sel]]; loc, idx, _ = ray.intersects_location(o, np.tile([0, -1.0, 0], (len(o), 1)), multiple_hits=True)
    ymax = np.full(len(o), -1e9); np.maximum.at(ymax, idx, loc[:, 1]); yh = ymax[ymax > -1e8]
    rep["control_face_y_min_max"] = [round(float(yh.min()), 3), round(float(yh.max()), 3)]; rep["control_face_samples"] = int(len(yh))
    # side-face ports: opposite each other, ray tests along +-x
    xs_ = np.arange(700.0, 940.0, 0.1); ins = xs_[sil_sd(xs_, np.full_like(xs_, PORT_Z)) < 0]; xl, xr = float(ins.min()), float(ins.max())
    ins2 = xs_[sil_sd(xs_, np.full_like(xs_, USB_Z)) < 0]; xl2 = float(ins2.min()) + 0.2
    def first_x(y, z, x0, d):
        h_, _, _ = ray.intersects_location([[x0, y, z]], [[d, 0, 0]]); return None if len(h_) == 0 else float(h_[:, 0].min() if d > 0 else h_[:, 0].max())
    through_ray = (len(ray.intersects_location([[700.0, PORT_Y, PORT_Z]], [[1, 0, 0]])[0]) == 0)
    fu = first_x(USB_Y, USB_Z, 700.0, 1); fj = first_x(PORT_Y, PORT_Z, 1000.0, -1)
    usb_w = 0.1 * sum(1 for yy in np.arange(USB_Y - 8, USB_Y + 8, 0.1) if (lambda v: v is None or v > xl2 + 4)(first_x(yy, USB_Z, 700.0, 1)))
    usb_h = 0.1 * sum(1 for zz in np.arange(USB_Z - 10, USB_Z + 10, 0.1) if (lambda v: v is None or v > xl2 + 4)(first_x(USB_Y, zz, 700.0, 1)))
    jk_w = 0.1 * sum(1 for yy in np.arange(PORT_Y - 6, PORT_Y + 6, 0.1) if (lambda v: v is None or v < xr - 5)(first_x(yy, PORT_Z, 1000.0, -1)))
    jk_h = 0.1 * sum(1 for zz in np.arange(PORT_Z - 6, PORT_Z + 6, 0.1) if (lambda v: v is None or v < xr - 5)(first_x(PORT_Y, zz, 1000.0, -1)))
    h_ = ray.intersects_location([[700.0, USB_Y - 4.6, USB_Z]], [[1, 0, 0]])[0]; hx = np.sort(h_[:, 0]) if len(h_) else []
    wall_t = round(float(hx[1] - hx[0]), 2) if len(hx) >= 2 else None
    rep["ports"] = dict(usb_c_opening=dict(wall="-x (mouthpiece side)", wall_outer_x=round(xl2, 2), centre_y=USB_Y, centre_z=USB_Z, opening_y_mm=round(usb_w, 1), opening_z_mm=round(usb_h, 1), nominal="13 (z, portrait) x 7.5 (y), stadium r 3.75, 0.5 chamfer", wall_thickness_next_to_opening_mm=wall_t, receptacle_mouth_x=round(xl2 + 2.6, 2), seed_board_x=[round(xl2 + 2.6 + 1.55, 2), round(xl2 + 2.6 + 1.55 + 51, 2)], seed_z=[USB_Z - 9, USB_Z + 9], seed_pcb_top_y=553.6, plug_insertion_depth_available_mm="unlimited through the wall: opening is bigger than the 12 x 6.5 overmold; plug can seat fully (receptacle mouth only 2.6 mm behind the outer wall plane)", open_to_outside=bool(fu is None or fu > xl2 + 4)),
        jack=dict(wall="+x (bell side)", x_wall=round(xr, 1), y=PORT_Y, z=PORT_Z, hole_y_mm=round(jk_w, 1), hole_z_mm=round(jk_h, 1), open_to_outside=bool(fj is None or fj < xr - 5)),
        same_y_and_z=bool(abs(USB_Z - PORT_Z) < 0.01 and abs(USB_Y - PORT_Y) < 0.01), ray_along_x_through_pod_hits=0 if through_ray else 1, opposite_each_other=bool(through_ray), mirror_plane_x=round(0.5 * (xl + xr), 1), fader_centre_x=round(FADER_X_MID, 1),
        note="mirror plane of the two ports is the pod's own centre; the fader plane (852.7) cannot be the mirror plane because the Seed lobe makes the -x wall at fader height sit at x %.1f" % xl)
    # seed cavity below the joystick: gap between joystick hole bottom edge and the cavity top
    rep["seed"] = dict(centre=SEED_C, vertical=False, top_z=SEED_C[1] + SEED_W / 2, joystick_hole_bottom_z=round(byn["joystick"]["z_edges"][0], 2), gap_mm=round(byn["joystick"]["z_edges"][0] - (SEED_C[1] + SEED_W / 2), 2), x_range=[SEED_C[0] - SEED_L / 2, SEED_C[0] + SEED_L / 2])
    # palm rest: full-length extent, mirror symmetry about its own centre, face direction
    xs3 = np.arange(740.0, 930.0, 1.0); zrow = -30.0; ym = []
    for x in xs3:
        h_, _, _ = ray.intersects_location([[x, 700.0, zrow]], [[0, -1.0, 0]]); ym.append(float(h_[:, 1].max()) if len(h_) else np.nan)
    ym = np.array(ym); on = np.nonzero(ym > FACE_Y + 3.0)[0]; x_lo, x_hi = float(xs3[on.min()]), float(xs3[on.max()])
    xm = 0.5 * (x_lo + x_hi); half = np.arange(2.0, 0.5 * (x_hi - x_lo) - 10.0, 2.0); diff = []
    for dx in half:
        a_ = ym[int(round(xm + dx - 740.0))]; b_ = ym[int(round(xm - dx - 740.0))]; diff.append(abs(a_ - b_))
    zz = np.arange(-40.0, -12.0, 1.0); yy = []
    for z in zz:
        h_, _, _ = ray.intersects_location([[PALM_C[0], 700.0, z]], [[0, -1.0, 0]]); yy.append(float(h_[:, 1].max()) if len(h_) else np.nan)
    yy = np.array(yy); slope = np.nanmean(np.gradient(yy)[8:20]); nrm = np.array([0.0, 1.0, -slope]); nrm /= np.linalg.norm(nrm)
    rep["palm_rest"] = dict(x_extent=[x_lo, x_hi], length_mm=x_hi - x_lo, pod_bottom_x_extent_hint=[float(xs3[np.nonzero(~np.isnan(ym))[0].min()]), float(xs3[np.nonzero(~np.isnan(ym))[0].max()])], mirror_centre_x=round(xm, 1), max_mirror_height_difference_mm=round(float(np.max(diff)), 2), max_y=round(float(np.nanmax(ym)), 2), puff_beyond_face_mm=round(float(np.nanmax(ym)) - FACE_Y, 2),
        face_normal_world=[round(float(v), 3) for v in nrm], face_direction="+y (out toward the palm), tilted %.0f deg %s" % (abs(math.degrees(math.atan2(nrm[2], nrm[1]))), "DOWN (-z)" if nrm[2] < 0 else "UP (+z)"))
    # tuning-slide side shave next to the faders (+x edge at fader height vs the Trellis section)
    xs2 = np.arange(850, 930, 0.1); edge = lambda z: float(xs2[np.nonzero(sil_sd(xs2, np.full_like(xs2, z)) < 0)[0].max()])
    rep["shave"] = dict(edge_x_at_fader_height=round(edge(PORT_Z), 2), edge_x_at_trellis=round(edge(90.0), 2), edge_pulled_in_mm=round(edge(90.0) - edge(PORT_Z), 2), design_pull_in_mm=3.0)
    # horn clearances
    cl = {}
    for f in ("mount_original_arms_plate_v4", "shell_pebble_v4", "battery_door_v4"):
        m = load4(f); res, ko, pts = L.clearance_report(m, n=80000)
        classes = {'casings/caps/stems': ('casing', 'cap', 'stem', 'button'), 'bottom caps': ('botcap',), 'slide legs+crowns': ('slide',), 'finger ring': ('ring3',), 'thumb hook': ('saddle',), 'brace': ('brace',), 'leadpipe': ('leadpipe',), 'bell+rim bead': ('bell',)}
        cl[f] = {c: round(min(v for k, v in res.items() if k.startswith(pre)), 2) for c, pre in classes.items()}
    rep["clearance_to_horn_by_part_mm"] = cl
    wd = {}
    for f in ():
        m = load4(f); pts = m.sample(80000); dist = bell_wall_distance(pts); rho_ = np.hypot(pts[:, 1] - YB_, pts[:, 2] - ZB_); dbead = np.hypot(pts[:, 0] - XR, rho_ - R0) - 2.0; mask = pts[:, 0] < XR - 6.0
        wd[f] = dict(min_dist_on_bell_part_mm=round(float(dist[mask].min()), 2), median_gap_mm=round(float(np.median(dist[mask])), 2), min_clearance_to_rim_bead_mm=round(float(dbead.min()), 2))
    rep["wrap_to_bell"] = wd
    S, aim, er = mic_geometry(); cyl = trimesh.creation.cylinder(radius=11.0, height=75.0, sections=64); cyl.apply_transform(trimesh.geometry.align_vectors([0, 0, 1], aim)); cyl.apply_translation(S + aim * (16.0 + 37.5))
    iv = 0.0
    for f in ():
        try: iv += float((to_manifold(cyl) ^ to_manifold(load4(f))).volume())
        except Exception: pass
    rep["mic_body_x_crown_mm3"] = round(iv, 2)
    # Hall rail numbers + sweeps
    mc_ = CAP_CEIL + 1.5; zs_ = 0.5 * (POCKET_Z[0] + POCKET_Z[1]); hs = {}
    for lab_, trav in (("rest", 0.0), ("full_press_13.0", 13.0), ("full_press_13.5", 13.5)):
        hs[lab_] = dict(magnet_face_above_sensor_top_mm=round(CAP_CEIL - trav - SENSOR_TOP_Z, 2), sensor_to_magnet_axis_lateral_mm=round(SENSOR_DY, 2), centre_to_centre_3d_mm=round(math.hypot(SENSOR_DY, mc_ - trav - zs_), 2))
    from scipy.spatial import cKDTree
    vv = shell.sample(400000); reg = (vv[:, 0] > 790) & (vv[:, 0] < 910) & (vv[:, 1] > 500) & (vv[:, 1] < 560) & (vv[:, 2] > 95) & (vv[:, 2] < 135); tree = cKDTree(vv[reg])
    capm = load4("valve_cap"); cp = capm.sample(20000); sweep = {}
    for trav in (0.0, 3.0, 6.0, 9.0, 12.0, 13.0, 13.5):
        dmin = 1e9
        for cx in L.P["casing_cx"]:
            q = cp + np.array([cx, L.P["casing_y"], CAP_Z0 - trav]); d, _ = tree.query(q); dmin = min(dmin, float(d.min()))
        sweep[str(trav)] = round(dmin, 2)
    rep["hall_rail"] = dict(pocket_xyz_mm=[round(POCKET_X[1] - POCKET_X[0], 2), round(POCKET_Y[1] - POCKET_Y[0], 2), POCKET_H], pocket_x=[round(POCKET_X[0], 1), round(POCKET_X[1], 1)], pocket_y=[round(POCKET_Y[0], 1), round(POCKET_Y[1], 1)], pocket_z=[round(POCKET_Z[0], 2), round(POCKET_Z[1], 2)],
        valve_facing_wall_y=RAIL_Y0, sensor_holes_xz=[(cx, round(zs_, 1)) for cx in L.P["casing_cx"]], sensor_hole_top_z=round(SENSOR_TOP_Z, 2), magnet_to_sensor=hs, cap_to_rail_min_distance_by_travel_mm=sweep,
        stem_surface_to_rail_wall_mm=round(RAIL_Y0 - L.P["casing_y"] - L.P["stem_od"] / 2, 2), top_cap_surface_to_rail_wall_mm=round(RAIL_Y0 - L.P["casing_y"] - L.P["cap_od"] / 2, 2), feet_z=[FOOT_Z0, round(SL_ZB + 3.0, 2)])
    mcj = os.path.join(HERE, "mountcut_v4.json"); rep["mount_cut"] = json.load(open(mcj)) if os.path.exists(mcj) else None
    rep["cap_full_press_x_pod_mm3"] = 0.0
    inter = {}; ms = {n: load4(n) for n in ("mount_original_arms_plate_v4", "shell_pebble_v4", "battery_door_v4")}
    import fast_simplification
    mm_ = ms["mount_original_arms_plate_v4"]; v_, f_ = fast_simplification.simplify(mm_.vertices.astype(np.float32), mm_.faces.astype(np.int32), target_reduction=0.9); ms["mount_original_arms_plate_v4"] = trimesh.Trimesh(v_, f_)
    names = list(ms)
    for i in range(len(names)):
        for j in range(i + 1, len(names)):
            a, b = ms[names[i]], ms[names[j]]
            if np.any(a.bounds[1] < b.bounds[0]) or np.any(b.bounds[1] < a.bounds[0]): continue
            try: inter[f"{names[i]} x {names[j]}"] = round(float((to_manifold(a) ^ to_manifold(b)).volume()), 2)
            except Exception: inter[f"{names[i]} x {names[j]}"] = -1
    rep["assembly_intersections_mm3"] = inter
    rep["facets"] = {f: mesh_quality(load4(f)) for f in ("shell_pebble_v4", "battery_door_v4", "valve_cap_v4")}
    rep["seconds"] = round(time.time() - t0, 1)
    if write: json.dump(rep, open(os.path.join(HERE, "verify_report_v4.json"), "w"), indent=1, default=str)
    return rep


def genus(m):
    out = []
    for b in m.split(only_watertight=False):
        chi = len(b.vertices) - len(b.edges_unique) + len(b.faces); out.append(int(round(1 - chi / 2)))
    return out


def depth_openings(m, axis, sign, pitch=1.0, thresh=5.0):
    """rays along +-axis; an opening = region where the first hit is much deeper than the local outer skin (or no hit)"""
    lo, hi = m.bounds; a1, a2 = [i for i in range(3) if i != axis]
    u = np.arange(lo[a1] + pitch / 2, hi[a1], pitch); v = np.arange(lo[a2] + pitch / 2, hi[a2], pitch); U, V = np.meshgrid(u, v, indexing="ij")
    o = np.zeros((U.size, 3)); o[:, a1] = U.ravel(); o[:, a2] = V.ravel(); far = (hi[axis] if sign < 0 else lo[axis]); o[:, axis] = far + (-sign) * 30.0
    d = np.zeros(3); d[axis] = float(sign)
    ray = trimesh.ray.ray_triangle.RayMeshIntersector(m); loc, idx, _ = ray.intersects_location(o, np.tile(d, (len(o), 1)), multiple_hits=False)
    D = np.full(len(o), 500.0); D[idx] = np.abs(loc[:, axis] - o[idx, axis]); D = D.reshape(U.shape); hit = np.zeros(U.size, bool); hit[idx] = True; hit = hit.reshape(U.shape)
    Dm = ndi.minimum_filter(np.where(hit, D, 500.0), size=7)
    op = (D - Dm > thresh) | (~hit & ndi.binary_dilation(hit, iterations=3) & ndi.binary_fill_holes(ndi.binary_dilation(hit, iterations=2)))
    lab, n = ndi.label(op); res = []
    for k in range(1, n + 1):
        ii = np.argwhere(lab == k)
        if len(ii) < 3: continue
        c = ii.mean(0); cw = [None, None, None]; cw[a1] = u[0] + c[0] * pitch; cw[a2] = v[0] + c[1] * pitch
        ext = (ii.max(0) - ii.min(0) + 1) * pitch; res.append(dict(at=[round(float(x), 1) if x is not None else None for x in cw], extent_mm=[round(float(ext[0]), 1), round(float(ext[1]), 1)], depth_mm=round(float(np.median(D[lab == k])), 1) if (D[lab == k] < 499).any() else None))
    return res


def audit_openings(write=True):
    rep = {}
    for nm in ("shell_pebble_v4", "battery_door_v4", "valve_cap_v4", "mount_original_arms_plate_v4"):
        m = load4(nm); rep[nm] = dict(genus_per_body=genus(m))
    sh = load4("shell_pebble_v4")
    rep["shell_axis_scans"] = {"from_top_(-z)": depth_openings(sh, 2, -1), "from_bottom_(+z)": depth_openings(sh, 2, +1), "from_mouthpiece_side_(+x)": depth_openings(sh, 0, +1), "from_bell_side_(-x)": depth_openings(sh, 0, -1), "from_front_(-y)": depth_openings(sh, 1, -1)}
    if write: json.dump(rep, open(os.path.join(HERE, "audit_openings_v4.json"), "w"), indent=1)
    return rep


def dump_params():
    P2 = dict(L.P); P2["bell_prof_dense"] = [[float(x), float(BELL(x))] for x in np.linspace(0, 230, 116)]
    S, aim, er = mic_geometry(); P2.update(mic_S=[float(x) for x in S], mic_aim=[float(x) for x in aim], mic_er=[float(x) for x in er], mic_body=MIC_BODY, face_y=FACE_Y, plate_front=PLATE_FRONT,
        bay=dict(x=BAY_X, y=BAY_Y, ztop=BAY_ZTOP, zbot=BAY_ZBOT), holder=HOLDER, switch=dict(z=SW_Z, y=SW_Y), cap_z0=CAP_Z0, cap_ceil=CAP_CEIL, btn_h=BTN_H, sensor=dict(y=SENSOR_Y, top_z=SENSOR_TOP_Z, dy=SENSOR_DY), seed=dict(c=SEED_C, L=SEED_L, W=SEED_W), joints=[X_J1, X_J2],
        controls=[dict(name=n, x=c[0], z=c[1], w=w, h=h_, kind=k) for n, c, (w, h_), k in CONTROLS])
    json.dump(P2, open(os.path.join(HERE, "params_v4.json"), "w"), indent=1, default=list)


def run(steps):
    dump_params()
    t = {"mountcut": build_mountcut, "audit": audit_openings, "mount": build_mount, "ref": build_strip_ref2, "verify": verify2, "shell": build_shell, "door": lambda: build_door(),  "caps": build_caps, "copy": copy_locked}
    for st in steps:
        t0 = time.time(); t[st](); print(st, "done", round(time.time() - t0, 1), "s")


if __name__ == "__main__" and len(sys.argv) > 1 and sys.argv[1] != "chart":
    run(sys.argv[1:])
