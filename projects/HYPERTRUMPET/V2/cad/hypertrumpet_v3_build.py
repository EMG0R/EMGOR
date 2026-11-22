#!/usr/bin/env python3
"""
HYPERTRUMPET V3 parametric build. Controls are placed from the ORIGINAL hyperFRday2.blend comp/cutter objects
(see ORIGINAL_CONTROLS, measured in docs/V3_BUILD_NOTES.md); supports are the ORIGINAL v1 mount with minimal fixes.
Uses the v2 library (hypertrumpet_v2_build.py, untouched) for SDF / mesh tools.
Steps:  python hypertrumpet_v3_build.py mount shell drawer cap spiral mic rail caps coupons verify   (or: all)
Outputs: cad/stl_v3/*.stl, cad/params_v3.json, cad/verify_report_v3.json. Never writes into v2 outputs.
Frame: model frame (x along horn, y lateral, z valve axis), mm. [M]=measured, [G]=general, [A]=assumed.
"""
import os, sys, json, math, time, pickle
import numpy as np
from scipy import ndimage as ndi
import trimesh
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import hypertrumpet_v2_build as L
from hypertrumpet_v2_build import Grid, sd_box, sd_cyl, sd_capsule_poly, sd_sphere, occ_to_sd, morph_close, morph_open, largest_component, keep_component, to_manifold, from_manifold

STL3 = os.path.join(HERE, "stl_v3"); os.makedirs(STL3, exist_ok=True)
BELL_OFF = L.BELL_OFF
P = dict(L.P)

# ============================================== ORIGINAL CONTROL OPENINGS [M] ===
# from hyperFRday2.blend: _holes.001 cutter bodies + attemp3COMPshowHOLE.001 hole loops (x, z = centre, w, h = opening size in x, z)
TRELLIS_X = (830.6, 845.6, 860.7, 875.7); TRELLIS_Z = (67.6, 82.7, 97.7, 112.8); TRELLIS_WH = (11.0, 11.1)
FADER_POS = [(828.5, 23.3), (852.7, 23.3), (876.9, 23.4)]; FADER_WH = (4.1, 38.4)       # vertical slots; NAMED VARIABLES: EMGOR is sending new fader/lidar numbers
LIDAR_POS = (853.1, 52.9); LIDAR_WH = (6.4, 4.0)                                         # NAMED VARIABLE (awaiting new measurement)
ROUND_HOLES = [(749.7, 46.8), (749.7, 96.5), (806.7, 56.3), (806.7, 90.0)]; ROUND_WH = (8.7, 8.3)   # pots / encoders (which is which is not in the file)
JOY_POS = (776.7, 72.3); JOY_WH = (29.4, 28.0)
FACE = (533.29, 0.03507, -0.03660)       # original face plane: y = a + b*x + c*z  (least squares over 15 hole loops, max residual 0.56 mm) [M]
ORIGINAL_CONTROLS = (
    [("trellis_%d%d" % (i, j), (TRELLIS_X[i], TRELLIS_Z[j]), TRELLIS_WH) for i in range(4) for j in range(4)]
    + [("fader_%d" % i, FADER_POS[i], FADER_WH) for i in range(3)] + [("lidar", LIDAR_POS, LIDAR_WH)]
    + [("round_%d" % i, ROUND_HOLES[i], ROUND_WH) for i in range(4)] + [("joystick", JOY_POS, JOY_WH)])


def face_y(x, z):
    return FACE[0] + FACE[1] * x + FACE[2] * z


P.update(
    plate_front=541.0, wall=2.2, lobe_wall=2.6,
    rail_x=878.0, rail_z0=8.0, rail_z1=100.0, rail_h=6.0, dt_neck=14.0, dt_head=22.0, dt_clear=0.8,
    horn_cut=0.4, bridge_A=(892.0, 553.5, 52.0), crown=1.2,                          # clearance cut applied to the ORIGINAL mount against the standard-dimension horn
    # palm lobe (flows out of the shell BELOW the faders; leans toward +y = the player's left in the model frame [A])
    lobe_p0=(853.0, 549.0, -29.0), lobe_p1=(858.0, 600.0, -40.0), lobe_r0=31.0, lobe_r1=35.0,
    drw_bay=(47.6, 25.4, 82.0), drw=(46.4, 24.2, 80.0),
    # spiral: one band up the entire bell (bell tail -> rim), < 1 turn, open at the leadpipe/brace side
    sp3_s0=8.0, sp3_s1=215.0, sp3_theta0=-100.0, sp3_dtheta=220.0, sp3_gap=1.5,
    sp3_w=12.0, sp3_base=1.6, sp3_depth=6.4, sp3_inner=7.0,
    # mic
    mic_r=11.0, mic_len=75.0, mic_S_dx=25.0, mic_S_rho_add=4.4, mic_aim_deg=54.0, mic_tilts=(-4.0, 0.0, 6.0),
    arm_w=11.0, arm_h=8.0, boss_r=10.5, socket_d=15.6, socket_depth=12.0, plug_d=15.2,
    # valve caps (finger caps replace the finger buttons; magnet lobe over the Hall rail)
    cap_r=8.0, cap_h=7.0, stem_bore=3.9, magnet_pocket=(10.4, 3.3), magnet_off=9.5, lobe_cap_r=6.9, lobe_cap_h=5.4,
    hall3_z=(117.5, 119.5),
)
Pj = dict(P)


def smin(a, b, k):
    h = np.maximum(k - np.abs(a - b), 0) / k
    return np.minimum(a, b) - h * h * k / 4


def save3(m, name, offset=None):
    path = os.path.join(STL3, name + ".stl")
    if offset is not None:
        m = m.copy(); m.apply_translation(-np.asarray(offset))
        f = os.path.join(HERE, "stl_offsets_v3.json"); d = json.load(open(f)) if os.path.exists(f) else {}
        d[name] = [float(x) for x in offset]; json.dump(d, open(f, "w"), indent=1)
    m2 = L.finalize(m); m2.export(path)
    if not trimesh.load(path, force="mesh").is_watertight: print("WARNING not watertight:", name)
    return path


def load3(name):
    m = trimesh.load(os.path.join(STL3, name + ".stl"), force="mesh")
    f = os.path.join(HERE, "stl_offsets_v3.json")
    if os.path.exists(f):
        d = json.load(open(f))
        if name in d: m.apply_translation(np.asarray(d[name]))
    return m


# ===================================================================== MOUNT ===
def build_mount(save=True):
    """ORIGINAL v1 mount (main body only: floating lumps dropped) minus the standard-dimension horn + 0.4 mm; plus a dovetail rail on the plate front."""
    p = P; h = 0.5
    g = Grid((712, 478, -20), (948, 551, 140), h)
    t0 = time.time()
    occ_v1 = L.v1_arm_occ(g)
    H = L.horn_field(g, include_shell=False)                  # analytic standard-dimension horn only (casing 18, caps, slide legs 12.6, ring, hook, brace, leadpipe)
    occ = occ_v1 & ~(H < p["horn_cut"] + 0.35)
    # rail boss (additive): male dovetail along z on the plate front, shell slides onto it
    R = g.full()
    L.dovetail_z(g, R, p["rail_x"], p["plate_front"] + p["rail_h"], p["rail_z0"], p["rail_z1"], p["dt_neck"], p["dt_head"], p["rail_h"], 0.0, grow_back=0.0)
    # dovetail_z narrows toward +y: we need it narrow at the plate and wide at the tip -> build mirrored: head at the tip
    Rm = g.full()
    Yg = g.ax[1][None, :, None]; Xg = g.ax[0][:, None, None]; Zg = g.ax[2][None, None, :]
    t = (Yg - (p["plate_front"] - 2.0)) / (p["rail_h"] + 2.0)           # 0 at the plate (inside), 1 at the tip
    w = (p["dt_neck"] / 2) + (p["dt_head"] / 2 - p["dt_neck"] / 2) * np.clip(t, 0, 1)
    rail = (np.abs(Xg - p["rail_x"]) <= w) & (Yg >= p["plate_front"] - 2.0) & (Yg <= p["plate_front"] + p["rail_h"]) & (Zg >= p["rail_z0"]) & (Zg <= p["rail_z1"])
    occ |= rail
    occ = largest_component(occ)
    # rail latch notch (flex tab sits in the shell)
    sd = occ_to_sd(occ, h, smooth=0.9)
    m = L.sd_to_mesh(g, sd, taubin=14)
    stats = dict(v1_main_mm3=float(occ_v1.sum() * h ** 3), v3_mm3=float(occ.sum() * h ** 3), rail_mm3=float(rail.sum() * h ** 3))
    # change log: removed components and what they were near
    rem = occ_v1 & ~occ & ~rail
    lab, n = ndi.label(rem); changes = []
    items = L.horn_items_for_check()
    for i in range(1, n + 1):
        ii = np.argwhere(lab == i); vol = len(ii) * h ** 3
        if vol < 4: continue
        c = g.lo + ii.mean(0) * h; best = None
        for name, kind, data, r in items:
            if kind == "cyl":
                cx, cy, z0, z1 = data; rho, dz = L.cyl_d(c[None, :], cx, cy, z0, z1); d1 = rho[0] - r
                d = min(max(d1, dz[0]), 0) + math.hypot(max(d1, 0), max(dz[0], 0))
            else:
                d = float(L.poly_d(c[None, :], np.asarray(data, float))[0]) - r
            if best is None or d < best[1]: best = (name, d)
        changes.append(dict(removed_mm3=round(vol, 1), centre=[round(float(x), 1) for x in c], bbox_lo=[round(float(x), 1) for x in (g.lo + ii.min(0) * h)], bbox_hi=[round(float(x), 1) for x in (g.lo + ii.max(0) * h)], nearest_horn_item=best[0]))
    changes.sort(key=lambda c: -c["removed_mm3"]); stats["changes"] = changes[:25]
    json.dump(stats, open(os.path.join(HERE, "mount_stats_v3.json"), "w"), indent=1)
    if save: save3(m, "mount_original_arms_plate")
    print("mount", round(time.time() - t0, 1), "s")
    return m


# ===================================================================== SHELL ===
def d_circle(X, Z, c, R): return np.hypot(X - c[0], Z - c[1]) - R


def d_capsule(X, Z, a, b, R):
    ax, az = a; bx, bz = b; dx, dz = bx - ax, bz - az; L2 = dx * dx + dz * dz
    t = np.clip(((X - ax) * dx + (Z - az) * dz) / L2, 0, 1)
    return np.hypot(X - ax - t * dx, Z - az - t * dz) - R


def d_rbox(X, Z, c, half, r):
    qx = np.abs(X - c[0]) - (half[0] - r); qz = np.abs(Z - c[1]) - (half[1] - r)
    return np.hypot(np.maximum(qx, 0), np.maximum(qz, 0)) + np.minimum(np.maximum(qx, qz), 0) - r


def footprint2d(X, Z):
    """one blended (smooth-union) outline wrapped around every original control opening"""
    k = 16.0
    f = d_rbox(X, Z, (853.2, 90.2), (38.5, 38.5), 18.0)                       # Trellis block
    f = smin(f, d_rbox(X, Z, (852.7, 23.3), (40.0, 30.0), 18.0), k)            # fader bank
    f = smin(f, d_circle(X, Z, JOY_POS, 26.0), k)                              # joystick
    f = smin(f, d_capsule(X, Z, ROUND_HOLES[0], ROUND_HOLES[1], 13.5), k)      # left pot/encoder column
    f = smin(f, d_capsule(X, Z, ROUND_HOLES[2], ROUND_HOLES[3], 13.5), k)      # second column
    f = smin(f, d_capsule(X, Z, (853.1, 40.0), (853.1, 56.0), 11.0), k)        # lidar bridge between faders and keys
    return f


def build_shell(save=True):
    p = P; h = 0.6
    g = Grid((730, 538, -78), (906, 650, 134), h)
    t0 = time.time()
    X = g.ax[0][:, None, None]; Y = g.ax[1][None, :, None]; Z = g.ax[2][None, None, :]
    F2 = footprint2d(X[:, 0, :], Z[0, :, :].reshape(1, -1) if False else Z[:, 0, :]) if False else footprint2d(g.ax[0][:, None], g.ax[2][None, :])[:, None, :]
    yb = p["plate_front"] + 0.6
    xx = g.ax[0][:, None, None]; zz = g.ax[2][None, None, :]
    yf = face_y(xx, zz) + p["crown"] * np.clip(1 - ((xx - 800) / 95) ** 2 - ((zz - 62) / 95) ** 2, 0, 1)        # original face plane + a 1.2 mm crown
    mid = 0.5 * (yb + yf); Hh = 0.5 * (yf - yb)
    Rr = 8.0
    q1 = F2 + Rr; q2 = np.abs(Y - mid) - Hh + Rr
    body = np.sqrt(np.maximum(q1, 0) ** 2 + np.maximum(q2, 0) ** 2) + np.minimum(np.maximum(q1, q2), 0) - Rr
    # palm lobe: round cone sweeping out of the shell below the faders toward +y
    a = np.array(p["lobe_p0"]); b = np.array(p["lobe_p1"]); r0, r1 = p["lobe_r0"], p["lobe_r1"]
    ab = b - a; L2 = float(ab @ ab)
    px = X - a[0]; py = Y - a[1]; pz = Z - a[2]
    t = np.clip((px * ab[0] + py * ab[1] + pz * ab[2]) / L2, 0, 1)
    rr = r0 + (r1 - r0) * t
    lobe = np.sqrt((px - t * ab[0]) ** 2 + (py - t * ab[1]) ** 2 + (pz - t * ab[2]) ** 2) - rr
    lobe = np.maximum(lobe, (yb + 1.0) - Y)                                            # the lobe starts at the plate plane, never behind it
    sd_out = np.maximum(smin(body, lobe, 22.0), (yb - 0.1) - Y).astype(np.float32)           # the blend never goes behind the plate plane
    print("fields", round(time.time() - t0, 1))
    wall = np.where(lobe < body + 4, p["lobe_wall"], p["wall"])
    shell = np.maximum(sd_out, -(sd_out + wall))                                      # hollow skin
    # open back: remove the skin behind yb+2.8 (the mount plate closes it), keep a rail strip
    back_cut = (Y < yb + 2.9)
    strip = (np.abs(X - p["rail_x"]) < 21.0) & (Y < yb + 9.2) & (sd_out < 0) & (Z > p["rail_z0"] - 14) & (Z < p["rail_z1"] + 6)
    S = np.where(back_cut & ~strip, np.maximum(shell, 0.5), shell)
    S = np.where(strip, np.minimum(S, np.maximum(sd_out, yb - 0.4 - Y)), S)        # strip is solid inside the body, from the plate face forward
    S = np.where(strip & (Y > yb + 9.2), np.maximum(S, 0.5), S)
    # female dovetail channel (open at the bottom) for the mount rail
    C = g.full()
    rt = (Y - (p["plate_front"] - 0.2))                                              # depth from the plate face
    wch = (p["dt_neck"] / 2 + p["dt_clear"]) + ((p["dt_head"] - p["dt_neck"]) / 2) * np.clip(rt / p["rail_h"], 0, 1)
    ch = np.maximum(np.maximum((np.abs(X - p["rail_x"]) - wch) * 0.93, -rt - 1.0), np.maximum(rt - (p["rail_h"] + p["dt_clear"]), np.abs(Z - 0.5 * (p["rail_z0"] + p["rail_z1"] - 30)) - 0.5 * (p["rail_z1"] - p["rail_z0"] + 31.8) - 0))
    S = np.maximum(S, -ch)
    # --- control openings through the front skin at the ORIGINAL positions
    def opening(c, wh, shape="rbox", r=1.0, depth=6.5):
        cx, cz = c; yface = face_y(cx, cz)
        if shape == "ellipse":
            d2 = (np.sqrt(((X - cx) / (wh[0] / 2)) ** 2 + ((Z - cz) / (wh[1] / 2)) ** 2) - 1) * min(wh) / 2
        else:
            qx = np.abs(X - cx) - (wh[0] / 2 - r); qz = np.abs(Z - cz) - (wh[1] / 2 - r)
            d2 = np.hypot(np.maximum(qx, 0), np.maximum(qz, 0)) + np.minimum(np.maximum(qx, qz), 0) - r
        return np.maximum(d2, np.abs(Y - (yface + 1.0)) - (depth + 3.0))
    cut = g.full()
    for name, c, wh in ORIGINAL_CONTROLS:
        if name.startswith("trellis"): f_ = opening(c, wh, "rbox", 1.0)
        elif name.startswith("fader"): f_ = opening(c, wh, "rbox", 2.0)
        elif name == "lidar": f_ = opening(c, wh, "rbox", 1.0)
        else: f_ = opening(c, wh, "ellipse")
        cut = np.minimum(cut, f_)
    S = np.maximum(S, -cut)
    # --- bridge plug slot (oriented along the bridge tangent at the anchor)
    bz_, tg_, sd_, up_ = bridge_geometry(); iA = 10
    Ap = bz_[iA]; u_ = np.array(sd_[iA]); v_ = np.array(up_[iA]); w_ = np.array(tg_[iA])
    pu = (X - Ap[0]) * u_[0] + (Y - Ap[1]) * u_[1] + (Z - Ap[2]) * u_[2]; pv = (X - Ap[0]) * v_[0] + (Y - Ap[1]) * v_[1] + (Z - Ap[2]) * v_[2]; pw = (X - Ap[0]) * w_[0] + (Y - Ap[1]) * w_[1] + (Z - Ap[2]) * w_[2]
    slot = np.maximum(np.maximum(np.abs(pu) - 9.2, np.abs(pv) - 8.2), np.maximum(-pw - 18.0, pw - 4.0))
    S = np.maximum(S, -slot)
    # --- palm lobe: cell drawer bay (open at the dome end), FSR recess + tail hole
    cx0 = 0.5 * (p["lobe_p0"][0] + p["lobe_p1"][0]); cz0 = 0.5 * (p["lobe_p0"][2] + p["lobe_p1"][2])
    bx, bz, by = p["drw_bay"]
    bay = g.full(); sd_box(g, bay, (cx0 - bx / 2, 556.0, cz0 - bz / 2), (cx0 + bx / 2, 660.0, cz0 + bz / 2), rad=1.5)
    S = np.maximum(S, -bay)
    for yy in (cx0 - bx / 2 - 0.8, cx0 + bx / 2):                                   # detent notches (along y)
        nb = g.full(); sd_box(g, nb, (yy, 612.0, cz0 - 5), (yy + 0.8, 622.0, cz0 + 5)); S = np.maximum(S, -nb)
    # FSR: on the lobe surface where the palm presses (upper-lateral), recess 1.0 + tail hole to the bay  [A]
    fy = 0.5 * (p["lobe_p0"][1] + p["lobe_p1"][1]) + 8.0; dvec = np.array([0.0, 0.55, 0.83]); dvec /= np.linalg.norm(dvec)
    ctr = np.array([cx0, fy, cz0 + 0.0])
    rrs = 0.5 * (r0 + r1) + 0.0
    fsr_c = ctr + dvec * (rrs)
    fz = g.full(); sd_cyl(g, fz, 2, (fsr_c[0], fsr_c[1]), 9.8, fsr_c[2] - 2.0, fsr_c[2] + 6.0)
    S = np.maximum(S, -np.where((fz < 0) & (sd_out > -1.2), -1.0, 1.0))      # 1.2 mm recess in the outer skin only
    tail = g.full(); sd_cyl(g, tail, 2, (fsr_c[0], fsr_c[1]), 1.7, cz0 + 12.0, fsr_c[2] + 1.0); S = np.maximum(S, -tail)
    # --- slot in the shell wall where the spiral bridge plugs in (right side, near z = 88)
    occ = largest_component(S < 0)
    sdS = np.where(occ, S, np.maximum(S, 0.05))
    m = L.sd_to_mesh(g, ndi.gaussian_filter(sdS, 0.5), taubin=8)
    if save: save3(m, "shell_flowing_pod_palm")
    print("shell", round(time.time() - t0, 1), "s", m.is_watertight)
    return m


# ==================================================================== DRAWER ===
def build_drawer(save=True):
    """cartridge for the lobe bay, slides along +y; same keyed 2-cell design as v2 (cells along y)"""
    m = L.build_drawer(save=False)
    # v2 drawer is along x (length 80); rotate so its length runs along +y
    R = trimesh.transformations.rotation_matrix(math.pi / 2, [0, 0, 1])
    m = m.copy(); m.apply_transform(R)
    if save: save3(m, "battery_drawer")
    return m


def build_cap(save=True):
    m = L.build_elec_cap(save=False)
    if save: save3(m, "electronics_end_cap_ref")
    return m


# ============================================================= SPIRAL / MIC ====
def rm_frames(points, side0, up0, backward=False):
    """parallel-transport (side, up) along a polyline; backward=True starts from the LAST point (so the frame matches the helix start)"""
    pts = np.asarray(points); n = len(pts)
    tang = np.gradient(pts, axis=0); tang /= np.linalg.norm(tang, axis=1)[:, None]
    side = np.zeros((n, 3)); up = np.zeros((n, 3)); first = True; prev_t = None
    for i in (range(n - 1, -1, -1) if backward else range(n)):
        if first: s_, u_ = np.array(side0, float), np.array(up0, float); first = False
        else:
            a_ = prev_t; b_ = tang[i]; v = np.cross(a_, b_); c = a_ @ b_
            if np.linalg.norm(v) < 1e-9: Rm = np.eye(3)
            else:
                vx = np.array([[0, -v[2], v[1]], [v[2], 0, -v[0]], [-v[1], v[0], 0]]); Rm = np.eye(3) + vx + vx @ vx * (1 / (1 + c))
            s_ = Rm @ s_; u_ = Rm @ u_
        side[i] = s_; up[i] = u_; prev_t = tang[i]
    return tang, side, up


def bezier(p0, p1, p2, p3, n=60):
    t = np.linspace(0, 1, n)[:, None]
    return (1 - t) ** 3 * p0 + 3 * (1 - t) ** 2 * t * p1 + 3 * (1 - t) * t ** 2 * p2 + t ** 3 * p3


def helix_pts():
    p = P
    n = int((p["sp3_s1"] - p["sp3_s0"]) / 1.0) * 3 + 2
    s_arr = np.linspace(p["sp3_s1"], p["sp3_s0"], n)
    th = np.radians(p["sp3_theta0"] + p["sp3_dtheta"] * (p["sp3_s1"] - s_arr) / (p["sp3_s1"] - p["sp3_s0"]))
    C0, T, N, r = L.helix_frames(s_arr, th)
    return s_arr, th, C0, T, N, r


def band_frames():
    """ribbon frames: up = surface normal, side = N x travel direction (the band lies ALONG the bell, width circumferential)"""
    s_arr, th, C0, T, N, r = helix_pts()
    tg = np.gradient(C0, axis=0); tg /= np.linalg.norm(tg, axis=1)[:, None]
    Nn = N - (N * tg).sum(1)[:, None] * tg; Nn /= np.linalg.norm(Nn, axis=1)[:, None]
    sd = np.cross(Nn, tg); sd /= np.linalg.norm(sd, axis=1)[:, None]
    return s_arr, th, C0, tg, sd, Nn


def bridge_geometry():
    """centreline (profile-mid) points, tangent, side, up of the bridge from the shell anchor to the helix start"""
    p = P
    s_arr, th, C0, tgH, T, N = band_frames()
    A = np.array(p["bridge_A"]); mid = p["sp3_gap"] + 4.0
    Pj = C0[0] + N[0] * mid
    tj = (C0[3] + N[3] * mid) - Pj; tj /= np.linalg.norm(tj)           # helix travel direction at the start
    tA = np.array([1.0, -0.03, 0.18]); tA /= np.linalg.norm(tA)
    bz = bezier(A - tA * 7.0, A + tA * 34.0, Pj - tj * 50.0, Pj, 80)
    tang, side, up = rm_frames(bz, T[0], N[0], backward=True)
    return bz, tang, side, up


def obox(c, u, v, w, half):
    M = np.eye(4); M[:3, 0] = u; M[:3, 1] = v; M[:3, 2] = w; M[:3, 3] = c
    b = trimesh.creation.box(extents=2 * np.asarray(half)); b.apply_transform(M); return b


def build_spiral(save=True):
    p = P
    s_arr, th, C0, tgH, T, N = band_frames()
    prof = L.u_profile(p["sp3_w"], p["sp3_inner"], p["sp3_gap"], p["sp3_base"], p["sp3_depth"], groove=True)
    mid = p["sp3_gap"] + 4.0
    bz, tang_b, side_b, up_b = bridge_geometry()
    bring = [bz[i] - up_b[i] * mid + prof[:, 0:1] * side_b[i] + prof[:, 1:2] * up_b[i] for i in range(len(bz) - 1)]
    hring = [C0[i] + prof[:, 0:1] * T[i] + prof[:, 1:2] * N[i] for i in range(len(C0))]
    # ---- split for the X1C bed: scarf-lap joint with two pins, at s ~ 112
    kJ = int(np.argmin(np.abs(s_arr - 112.0))); step = np.linalg.norm(np.diff(C0, axis=0), axis=1).mean(); nlap = int(14.0 / step)
    ribA = L.sweep_profiles(bring + hring[:kJ + nlap + 1]); ribB = L.sweep_profiles(hring[kJ:])
    J = C0[kJ]; uJ = T[kJ]; vJ = N[kJ]; wJ = tgH[kJ]                                       # side, up, helix travel direction
    vmid = p["sp3_gap"] + 4.0
    cA = J + wJ * 7.5 + vJ * (vmid + 10.0)                                                  # remove the upper half of A over the lap
    cB = J + wJ * 7.5 + vJ * (vmid - 10.0)                                                  # remove the lower half of B over the lap
    cutA = obox(cA, uJ, vJ, wJ, (14, 10.0, 7.5)); cutB = obox(cB, uJ, vJ, wJ, (14, 10.0, 7.5))
    pins = []; holes = []
    for sgn in (-1, 1):
        for wv in (4.0, 11.0):
            c = J + uJ * (sgn * 5.2) + wJ * wv + vJ * (vmid + 0.8)
            pins.append(obox(c, uJ, vJ, wJ, (0.75, 1.7, 0.75))); holes.append(obox(J + uJ * (sgn * 5.2) + wJ * wv + vJ * (vmid + 1.9), uJ, vJ, wJ, (1.05, 1.8, 1.05)))
    # ---- mic arm from the helix end
    yb, zb = P["bell_axis"]; xr = P["bell_rim_x"]
    thE = th[-1]; er = np.array([0.0, math.cos(thE), math.sin(thE)]); ex = np.array([1.0, 0, 0]); r0 = L._BELL(0.0)
    S = np.array([xr + p["mic_S_dx"], yb, zb]) + er * (r0 + p["mic_S_rho_add"])
    a = math.radians(p["mic_aim_deg"]); aim = -ex * math.cos(a) - er * math.sin(a)
    Pe = C0[-1] + N[-1] * mid; tE = ex * 0.45 + er * 0.89; tE /= np.linalg.norm(tE)
    over = np.array([xr + 2.0, yb, zb]) + er * (r0 + 2.0 + 4.0 + 4.5)
    arm_path = np.vstack([bezier(Pe, Pe + tE * 12.0, over - ex * 14.0, over, 40)[:-1], bezier(over, over + ex * 9.0, S + er * 5.0 - ex * 6.0, S, 40)])
    tang = np.gradient(arm_path, axis=0); tang /= np.linalg.norm(tang, axis=1)[:, None]
    sdv = np.cross(er, ex); sdv /= np.linalg.norm(sdv)
    proa = L.solid_profile(p["arm_w"], p["arm_h"], -p["arm_h"] / 2, c=1.6); arm_rings = []
    for i in range(len(arm_path)):
        upv = np.cross(tang[i], sdv); upv /= np.linalg.norm(upv); sdv = np.cross(upv, tang[i]); sdv /= np.linalg.norm(sdv)
        arm_rings.append(arm_path[i] + proa[:, 0:1] * sdv + proa[:, 1:2] * upv)
    arm = L.sweep_profiles(arm_rings)
    Tal = trimesh.geometry.align_vectors([0, 0, 1], aim)
    boss = trimesh.creation.cylinder(radius=p["boss_r"], height=16.0, sections=67); boss.apply_transform(Tal); boss.apply_translation(S + aim * 4.0)
    pocket = trimesh.creation.cylinder(radius=p["socket_d"] / 2, height=p["socket_depth"] + 3.0, sections=61); pocket.apply_transform(Tal); pocket.apply_translation(S + aim * (12.0 - p["socket_depth"] / 2 + 1.5))
    groove = trimesh.creation.cylinder(radius=p["socket_d"] / 2 + 0.55, height=1.8, sections=59); groove.apply_transform(Tal); groove.apply_translation(S + aim * (12.0 - 7.0))
    for q in [ribA, ribB, arm, boss, pocket, groove, cutA, cutB] + pins + holes: q.apply_translation(-BELL_OFF)
    UA = to_manifold(ribA) - to_manifold(cutA)
    for q in pins: UA = UA + to_manifold(q)
    UB = to_manifold(ribB) + to_manifold(arm) + to_manifold(boss)
    UB = UB - to_manifold(cutB) - (to_manifold(pocket) + to_manifold(groove))
    for q in holes: UB = UB - to_manifold(q)
    resA = from_manifold(UA); resA.apply_translation(BELL_OFF); resB = from_manifold(UB); resB.apply_translation(BELL_OFF)
    mid2 = p["sp3_gap"] + p["sp3_base"] + p["sp3_depth"] / 2
    cl = np.vstack([bz[:-1] - up_b[:-1] * mid + up_b[:-1] * mid2, C0 + N * mid2]); length = float(np.linalg.norm(np.diff(cl, axis=0), axis=1).sum())
    info = dict(S=[float(x) for x in S], aim=[float(x) for x in aim], er=[float(x) for x in er], anchor=[float(x) for x in A_] if False else list(p["bridge_A"]), strip_path_mm=round(length, 1),
                strip_px_at_160_per_m=int(length / 6.25), helix_len_mm=round(float(np.linalg.norm(np.diff(C0 + N * mid2, axis=0), axis=1).sum()), 1), bridge_len_mm=round(float(np.linalg.norm(np.diff(bz, axis=0), axis=1).sum()), 1),
                turns=p["sp3_dtheta"] / 360.0, split_s_mm=112.0)
    json.dump(info, open(os.path.join(HERE, "spiral_info_v3.json"), "w"), indent=1)
    if save:
        save3(resA, "spiral_A_bridge_and_tail", offset=BELL_OFF); save3(resB, "spiral_B_bell_and_mic_arm", offset=BELL_OFF)
    return resA, resB, info


def build_diffuser(save=True):
    p = P
    s_arr, th, C0, tgH, T, N = band_frames()
    w = p["sp3_inner"] + 1.4; top = p["sp3_gap"] + p["sp3_base"] + p["sp3_depth"]
    v0 = top - 1.95; v1 = v0 + 0.9
    prof = np.array([(-w / 2 + .3, v0), (w / 2 - .3, v0), (w / 2, v0 + .3), (w / 2, v1 - .3), (w / 2 - .3, v1), (-w / 2 + .3, v1), (-w / 2, v1 - .3), (-w / 2, v0 + .3)])
    n_end = int(len(C0) * (1 - 14.0 / (p["sp3_s1"] - p["sp3_s0"])))
    rings = [C0[i] + prof[:, 0:1] * T[i] + prof[:, 1:2] * N[i] for i in range(0, n_end)]
    m = L.sweep_profiles(rings)
    if save: save3(m, "spiral_diffuser", offset=BELL_OFF)
    return m


def build_mic_inserts(save=True):
    p = P; out = []
    for a in p["mic_tilts"]:
        plug = trimesh.creation.cylinder(radius=p["plug_d"] / 2, height=p["socket_depth"], sections=60); plug.apply_translation([0, 0, -p["socket_depth"] / 2])
        rib = trimesh.creation.cylinder(radius=p["plug_d"] / 2 + 0.5, height=1.6, sections=58); rib.apply_translation([0, 0, -6.5])
        boss = trimesh.creation.cylinder(radius=10.2, height=14.0, sections=61)
        boss.apply_transform(trimesh.transformations.rotation_matrix(math.radians(a), [0, 1, 0])); boss.apply_translation([math.sin(math.radians(a)) * 6.5, 0, math.cos(math.radians(a)) * 6.5])
        hole = trimesh.creation.cylinder(radius=4.85, height=30.0, sections=48)
        hole.apply_transform(trimesh.transformations.rotation_matrix(math.radians(a), [0, 1, 0])); hole.apply_translation([math.sin(math.radians(a)) * 11.0, 0, math.cos(math.radians(a)) * 11.0])
        U = to_manifold(plug) + to_manifold(rib) + to_manifold(boss); U = U - to_manifold(hole)
        m = from_manifold(U)
        if save: save3(m, "mic_insert_tilt_%+03d" % int(a))
        out.append(m)
    return out


# ====================================================== HALL RAIL / VALVE CAPS ===
def build_hall_rail(save=True):
    """sensors on a rail between the top caps and the finger caps; finger cap travel >= 13 mm (see notes)"""
    p = P; h = 0.3
    g = Grid((818, 498, 108), (892, 530, 124), h)
    F = g.full(); cy = p["casing_y"]; z0, z1 = p["hall3_z"]
    sd_box(g, F, (p["casing_cx"][0] - 10.5, cy - 9.0, z0), (p["casing_cx"][2] + 10.5, cy + 13.5, z1), rad=0.9)
    for cx in p["casing_cx"]:
        sd_cyl(g, F, 2, (cx, cy), p["cap_od"] / 2 + 0.4 + 2.0, z0 - 6.5, z0 + 0.5)
    occ = F < 0
    N = g.full()
    for cx in p["casing_cx"]:
        sd_cyl(g, N, 2, (cx, cy), p["cap_od"] / 2 + 0.4, z0 - 8, z0 - 0.0)
        sd_box(g, N, (cx - 8.0, cy + 2.0, z0 - 8), (cx + 8.0, cy + 16.0, z0 - 0.0))
        sd_cyl(g, N, 2, (cx, cy), 3.4, z0 - 8, z1 + 2)
        sd_box(g, N, (cx - 2.8, cy + p["magnet_off"] - 2.8, z0 - 0.2), (cx + 2.8, cy + p["magnet_off"] + 2.8, z1 + 1))
    occ &= ~(N < 0); occ = largest_component(occ)
    m = L.sd_to_mesh(g, occ_to_sd(occ, h, smooth=0.5), simplify=1)
    if save: save3(m, "hall_rail_49E")
    return m


def build_valve_caps(save=True):
    """finger caps replacing the buttons: magnet pocket 10.4 x 3.3 (+glue relief groove + vent), snap bore for the stem, zig-zag band + polka dots"""
    p = P; h = 0.14
    g = Grid((-12, -12, -1.5), (12, 21, 9.5), h)
    X = g.ax[0][:, None, None]; Y = g.ax[1][None, :, None]; Z = g.ax[2][None, None, :]
    R = np.hypot(X, Y); TH = np.arctan2(Y, X)
    cr, ch = p["cap_r"], p["cap_h"]
    # zig-zag raised band (triangle wave in theta) on the side; ridge width 1.8, height 0.8
    Nz = 9; u = (TH / (2 * np.pi) * Nz) % 1.0; tri = 4 * np.abs(u - 0.5) - 1          # -1..1
    zc = 3.3 + 1.5 * tri
    ridge = np.clip((1.25 - np.abs(Z - zc)) / 0.5, 0, 1) * 1.0
    rside = cr + ridge
    # body: cylinder with rounded top edge (R2.2), flat smooth finger top
    q1 = R - (rside - 2.2); q2 = Z - (ch - 2.2)
    body = np.sqrt(np.maximum(q1, 0) ** 2 + np.maximum(q2, 0) ** 2) + np.minimum(np.maximum(q1, q2), 0) - 2.2
    body = np.maximum(body, -Z)
    # magnet lobe (offset +y), lower than the cap
    Rl = np.hypot(X, Y - p["magnet_off"]); lc = p["lobe_cap_r"]; lh = p["lobe_cap_h"]
    q1l = Rl - (lc - 1.6); q2l = Z - (lh - 1.6)
    lobe = np.sqrt(np.maximum(q1l, 0) ** 2 + np.maximum(q2l, 0) ** 2) + np.minimum(np.maximum(q1l, q2l), 0) - 1.6
    lobe = np.maximum(lobe, -Z)
    sd = smin(body, lobe, 1.6).astype(np.float32)
    # stem snap bore (undersize 0.1 vs the 4.0 stem) 5 mm deep with 3 relief slits
    bore = np.maximum(np.hypot(X, Y) - p["stem_bore"] / 2, Z - 5.0)
    sd = np.maximum(sd, -bore)
    for ang in (90, 210, 330):
        ca, sa = math.cos(math.radians(ang)), math.sin(math.radians(ang))
        along = X * ca + Y * sa; perp = -X * sa + Y * ca
        slit = np.maximum(np.maximum(np.abs(perp) - 0.4, -along + 1.0), np.maximum(along - 5.0, Z - 5.0))
        sd = np.maximum(sd, -slit)
    # magnet pocket from the underside, 10.4 x 3.3, glue groove ring + vent to the top
    mp = np.maximum(Rl - p["magnet_pocket"][0] / 2, Z - p["magnet_pocket"][1])
    sd = np.maximum(sd, -mp)
    groove = np.maximum(np.abs(Rl - 4.3) - 0.45, np.maximum(Z - (p["magnet_pocket"][1] + 0.5), -Z))
    sd = np.maximum(sd, -groove)
    vent = np.maximum(Rl - 0.65, np.maximum(Z - 6.5, p["magnet_pocket"][1] - Z - 0.2 + 0.0 * Z))
    sd = np.maximum(sd, -vent)
    # polka dots: shallow recessed dimples (d 2.4, 0.6 deep) on the rim ring of the finger top and the lobe top
    for k_ in range(8):
        a_ = 2 * math.pi * k_ / 8 + math.pi / 8; cx_, cy_ = 6.3 * math.cos(a_), 6.3 * math.sin(a_)
        dim = np.sqrt((X - cx_) ** 2 + (Y - cy_) ** 2 + (Z - (ch + 0.7)) ** 2) - 1.3
        sd = np.maximum(sd, -dim)
    for k_ in range(3):
        a_ = math.pi * (0.5 + 0.5 * k_ / 2 * 2) / 1.0 + k_ * 0.0
        cx_, cy_ = [-3.2, 0.0, 3.2][k_], p["magnet_off"] + [1.6, 3.4, 1.6][k_]
        dim = np.sqrt((X - cx_) ** 2 + (Y - cy_) ** 2 + (Z - (lh + 0.7)) ** 2) - 1.3
        sd = np.maximum(sd, -dim)
    occ = largest_component(sd < 0)
    m = L.sd_to_mesh(g, ndi.gaussian_filter(np.where(occ, sd, np.maximum(sd, 0.05)), 0.6), simplify=1, taubin=4)
    if save:
        for i in range(1, 5): save3(m, "valve_cap_%d%s" % (min(i, 3), "_spare" if i == 4 else ""))
    return m


def build_coupons(save=True):
    p = P
    for c in (0.2, 0.4, 0.6):
        idm = p["casing_od"] + 2 * c
        ring = trimesh.creation.annulus(r_min=idm / 2, r_max=idm / 2 + 3.2, height=14.0, sections=96)
        if save: save3(from_manifold(to_manifold(ring)), "coupon_grip_ring_c%02d" % int(c * 10))
    return None


# ===================================================================== VERIFY ===
ASSEMBLED3 = ["mount_original_arms_plate", "shell_flowing_pod_palm", "spiral_A_bridge_and_tail", "spiral_B_bell_and_mic_arm", "spiral_diffuser", "hall_rail_49E"]


def mic_envelopes():
    """mic body = cylinder (radius mic_r, length mic_len) on the insert axis. Per axial station: all ring points must be on one side of the bell wall (no crossing),
    before the rim plane the distance to the rim bead must be positive. Returns the minimum clearance (negative = wall/rim crossed)."""
    info = json.load(open(os.path.join(HERE, "spiral_info_v3.json")))
    S = np.array(info["S"]); er = np.array(info["er"]); ex = np.array([1.0, 0, 0]); xr = P["bell_rim_x"]; yb, zb = P["bell_axis"]
    out = {}
    for t in P["mic_tilts"]:
        a = math.radians(P["mic_aim_deg"] + t); ai = -ex * math.cos(a) - er * math.sin(a)
        tg = np.cross(er, ex); perp = np.cross(ai, tg); worst = 1e9
        for ss in np.linspace(0, P["mic_len"], 76):
            c = S + ai * (16.0 + ss)
            ring = np.array([c + P["mic_r"] * (math.cos(g) * tg + math.sin(g) * perp) for g in np.linspace(0, 2 * math.pi, 24, endpoint=False)] + [c])
            s_ = xr - ring[:, 0]; rho = np.hypot(ring[:, 1] - yb, ring[:, 2] - zb)
            rim = np.hypot(s_, rho - L._BELL(0.0)) - 2.0
            if (s_ > 0).all():
                dwall = (rho - L._BELL(np.clip(s_, 0, 230))) / np.sqrt(1 + L._BELLD(np.clip(s_, 0, 230)) ** 2)
                m_ = -min(dwall.max(), -dwall.min()) if (dwall.min() < 0 < dwall.max()) else np.abs(dwall).min()
            else:
                m_ = rim.min()
                inside = (s_ > 0)
                if inside.any():
                    dwall = (rho[inside] - L._BELL(np.clip(s_[inside], 0, 230))) / np.sqrt(1 + L._BELLD(np.clip(s_[inside], 0, 230)) ** 2)
                    m_ = min(m_, np.abs(dwall).min()) if not (dwall.min() < 0 < dwall.max()) else min(m_, -min(dwall.max(), -dwall.min()))
            worst = min(worst, m_)
        out[t] = round(float(worst), 2)
    return out


def verify(write=True):
    rep = {"parts": {}}
    for f in sorted(x[:-4] for x in os.listdir(STL3) if x.endswith(".stl")):
        m = load3(f)
        d = dict(watertight=bool(m.is_watertight), bodies=len(m.split(only_watertight=False)), volume_cm3=round(float(m.volume) / 1000, 2) if m.is_watertight else None,
                 bbox_mm=[round(float(x), 1) for x in (m.bounds[1] - m.bounds[0])], fits_bed=bool(np.all(np.sort(m.bounds[1] - m.bounds[0]) <= 252)))
        if f in ASSEMBLED3:
            res, ko, pts = L.clearance_report(m, n=80000)
            d["min_clear_by_item"] = {k: round(v, 2) for k, v in sorted(res.items(), key=lambda kv: kv[1])[:8]}
            cls = {'casings/caps/buttons': ('casing', 'cap', 'stem', 'button', 'botcap'), 'slide legs+crowns': ('slide',), 'finger ring': ('ring3',), 'thumb hook': ('saddle',), 'brace': ('brace',), 'leadpipe': ('leadpipe',), 'bell+rim': ('bell',)}
            d["clear_by_class"] = {c: round(min(v for k, v in res.items() if k.startswith(pre)), 2) for c, pre in cls.items()}
        rep["parts"][f] = d
    # control position diff (ray scan through the front skin: first hit of a ray from +y must be deeper than the skin for the opening)
    shell = load3("shell_flowing_pod_palm")
    ray = trimesh.ray.ray_triangle.RayMeshIntersector(shell)
    def open_at(xs, zs, yface):
        o = np.c_[xs, np.full(len(xs), 700.0), zs]; d = np.tile([0, -1.0, 0], (len(xs), 1))
        loc, idx_ray, _ = ray.intersects_location(o, d, multiple_hits=False)
        first = np.full(len(xs), 0.0); first[idx_ray] = loc[:, 1]
        return first < yface - 4.0            # opening if the first surface hit is below the skin (or none)
    diff = []
    for name, (cx, cz), (w, hh) in ORIGINAL_CONTROLS:
        yface = face_y(cx, cz) + P["crown"] * max(0.0, 1 - ((cx - 800) / 95) ** 2 - ((cz - 62) / 95) ** 2)
        xs = np.arange(cx - w / 2 - 5, cx + w / 2 + 5, 0.2); zs = np.arange(cz - hh / 2 - 5, cz + hh / 2 + 5, 0.2)
        ox = open_at(xs, np.full(len(xs), cz), yface); oz = open_at(np.full(len(zs), cx), zs, yface)
        if not ox.any() or not oz.any(): diff.append(dict(control=name, found=False)); continue
        # contiguous run containing the centre
        def run(a_, c, arr):
            i0 = int(np.argmin(np.abs(a_ - c)))
            if not arr[i0]: return None
            l = i0; r = i0
            while l > 0 and arr[l - 1]: l -= 1
            while r < len(arr) - 1 and arr[r + 1]: r += 1
            return a_[l], a_[r]
        rx = run(xs, cx, ox); rz = run(zs, cz, oz)
        if rx is None or rz is None: diff.append(dict(control=name, found=False)); continue
        ccx, ccz = 0.5 * (rx[0] + rx[1]), 0.5 * (rz[0] + rz[1])
        diff.append(dict(control=name, orig_centre=(cx, cz), v3_centre=(round(ccx, 2), round(ccz, 2)), d_centre_mm=round(math.hypot(ccx - cx, ccz - cz), 2), orig_size=(w, hh), v3_size=(round(rx[1] - rx[0], 2), round(rz[1] - rz[0], 2))))
    rep["control_diff"] = diff
    rep["mic_envelope_min_clearance_to_bell_mm"] = mic_envelopes()
    inter = {}
    ms = {n: load3(n) for n in ASSEMBLED3}; names = list(ms)
    for i in range(len(names)):
        for j in range(i + 1, len(names)):
            a, b = ms[names[i]], ms[names[j]]
            if np.any(a.bounds[1] < b.bounds[0]) or np.any(b.bounds[1] < a.bounds[0]): continue
            try:
                I = from_manifold(to_manifold(a) ^ to_manifold(b)); v = float(I.volume) if len(I.faces) else 0.0
            except Exception: v = -1.0
            if v != 0.0: inter[f"{names[i]} x {names[j]}"] = round(v, 2)
    rep["assembly_intersections_mm3"] = inter
    if write: json.dump(rep, open(os.path.join(HERE, "verify_report_v3.json"), "w"), indent=1)
    return rep


def run(steps):
    allsteps = ["mount", "shell", "drawer", "spiral", "mic", "rail", "caps", "coupons"]
    if "all" in steps: steps = allsteps + ["verify"]
    P2 = dict(P); P2["bell_prof_dense"] = [[float(x), float(L._BELL(x))] for x in np.linspace(0, 230, 116)]
    P2["controls"] = [dict(name=n, x=c[0], z=c[1], w=w, h=h) for n, c, (w, h) in ORIGINAL_CONTROLS]; P2["face_plane"] = FACE
    json.dump(P2, open(os.path.join(HERE, "params_v3.json"), "w"), indent=1, default=list)
    for st in steps:
        t = time.time()
        {"mount": build_mount, "shell": build_shell, "drawer": build_drawer, "cap": build_cap, "spiral": lambda: (build_spiral(), build_diffuser()), "mic": build_mic_inserts,
         "rail": build_hall_rail, "caps": build_valve_caps, "coupons": build_coupons, "verify": verify}[st]()
        print(st, "done", round(time.time() - t, 1), "s")


if __name__ == "__main__":
    run(sys.argv[1:] or ["all"])
