#!/usr/bin/env python3
"""
HYPERTRUMPET V2 parametric build (EMGOR). Run with a venv python that has:
    numpy scipy scikit-image trimesh manifold3d shapely fast-simplification matplotlib
Usage:  python hypertrumpet_v2_build.py <step> [step ...]
  steps: mount pod rest drawer spiral mic rail caps pads coupons verify all
Model frame = v1 / Sketchfab frame: x along horn (player end 620 -> bell rim 1150), y lateral
(pod on +y), z valve axis up. 1 unit = 1 mm. All dimensions are named below.
Source files are READ ONLY: cad/v1_reference/_mount_v3.stl and the Sketchfab model inside
cad/v1_reference/hyperFRday2.blend (exported once to cad/_cache/allobjs.pkl by export_ref_objs()).
Every STL written is a single watertight body (checked in verify).
Tags used in comments: [M]=measured on the v1 files, [W]=web, [G]=general knowledge/unverified,
[A]=assumed (flagged in docs/V2_BUILD_NOTES.md).
"""
import os, sys, json, time, pickle, math
import numpy as np
from scipy import ndimage as ndi
from skimage import measure
import trimesh

HERE = os.path.dirname(os.path.abspath(__file__))
BELL_OFF = np.array([1090.0, 525.0, 110.0])   # spiral parts are exported centred on this point (float32 precision), see stl_offsets.json
STL = os.path.join(HERE, "stl_v2")
CACHE = os.path.join(HERE, "_cache")
V1 = os.path.join(HERE, "v1_reference")
os.makedirs(STL, exist_ok=True); os.makedirs(CACHE, exist_ok=True)

# =============================================================== PARAMETERS ====
P = dict(
    # ---- standard horn (r2 spec) ------------------------------------------------
    casing_od=18.0,            # EMGOR measurement (casing body at the top) [given]
    casing_pitch=21.5,         # [A] model pitch 22.3 minus the casing change
    casing_cx=(832.0, 853.5, 875.0),
    casing_y=509.5,            # [M]
    casing_z0=5.0, casing_z1=109.0,    # [M]
    cap_od=21.0, cap_z0=109.0, cap_z1=117.0,   # [M model 20.3-21.2]
    stem_od=4.0, stem_z1=133.0,                # [M]
    button_od=13.9, button_z1=137.0,           # [M]
    botcap_od=19.5, botcap_z0=-4.0,            # [M model 18.6] + 0.9 for std casing
    slide_leg_od=12.6,         # [G] bore 11.66 + wall
    slide1_y=509.5, slide3_y=500.0,    # [M] tube plane y of 1st / 3rd valve slide
    slide_leg_z=(28.1, 46.9),          # [M]
    slide1_crown_x=995.0, slide3_crown_x=763.0,   # [M]
    slide_pull=100.0,          # [A] slide travel used for keep-out
    ring3=dict(cx=801.0, cz=67.0, a=17.2, b=14.2, y=498.0, tube_r=1.8),   # [M] 3rd-slide finger ring
    saddle=dict(cx=916.5, cz=116.5, r=11.5, y=494.5, tube_r=1.8),         # [M] 1st-slide thumb hook (half ring)
    brace=dict(x=993.0, y=494.5, z0=37.0, z1=100.0, r=1.85),              # [M]
    leadpipe=dict(y=494.0, z=101.5, r=6.35, x0=620.0, x1=1050.0),         # [M]/[G]
    bell_axis=(525.2, 97.7),   # (y,z) [M]
    bell_rim_x=1150.0,         # [M]
    bell_prof_s=(0, 10, 20, 34, 50, 65, 83, 150, 230),
    bell_prof_r=(61.6, 44.4, 32.3, 24.6, 19.5, 16.2, 14.0, 10.5, 7.2),
    # ---- clearances / arm rules ---------------------------------------------------
    rigid_clear=0.4,           # EMGOR: 0.4 mm rigid clearance
    cut_margin=-0.25,           # extra voxel/smoothing margin used when cutting so the measured clearance stays >= rigid_clear
    arm_thicken=1.2,           # dilation of v1 arms: min blade 3.6 -> 6.0 mm (no thin blades)
    arm_min_thick=5.0,
    root_fillet=3.0,           # R3 closing at roots
    edge_round=1.2,            # convex edge rounding (opening)
    pad_thk=1.6, pad_interf=0.2, pad_recess=1.0,
    open_air=6.0,              # margin around slides/rings/saddle/water keys
    ring_air=10.0,
    # ---- plate / joints -----------------------------------------------------------
    plate_y0=531.0, plate_y1=541.0,           # plate rear / front face
    dt_neck=14.0, dt_head=22.0, dt_h=6.0, dt_clear=0.4,   # dovetail rails
    pod_rail_x=878.0, pod_rail_z0=-8.0, pod_rail_z1=118.0,
    latch_t=2.0, latch_w=7.0, latch_len=32.0, latch_bump=1.2,
    # ---- pod / face panel ---------------------------------------------------------
    pod_x0=818.0, pod_x1=940.0, pod_z0=-8.0, pod_z1=127.0, pod_r=14.0,
    pod_back=2.0, pod_cav=8.0, pod_panel=2.0, pod_wall=2.4,
    trellis_cx=853.5, trellis_cz=76.35, trellis_pitch=15.3, trellis_win=13.6,
    trellis_board=61.7, trellis_hole_off=27.4,        # [G] M3 hole offset from centre
    joy_xy=(912.0, 108.0), joy_hole=20.0,             # KY-023 stick through-hole [G]
    pot_xz=((904.0, 80.0), (926.0, 80.0)), pot_hole=7.2,     # M7x0.75 bushing [G]
    enc_xz=((904.0, 50.0), (926.0, 50.0)), enc_hole=7.2,     # KY-040 M7 bushing [G]
    fader_x=853.5, fader_z=(33.0, 18.0, 3.0), fader_slot=(47.0, 3.4),   # 45 mm travel [G]
    # ---- palm rest / battery ------------------------------------------------------
    rest_x0=800.0, rest_x1=912.0, rest_y0=489.0, rest_y1=576.0,
    rest_top=-13.6, rest_depth=50.0, rest_R_long=220.0, rest_R_trans=120.0, rest_round=12.0,
    cell_d=21.8, cell_l=70.6,                         # 21700 [G]
    bay_y=47.6, bay_z=25.4, bay_x=82.0,               # drawer cavity
    drw_y=46.4, drw_z=24.2, drw_x=80.0,
    fsr_d=18.3, fsr_pocket_d=19.0,                    # FSR402 [G]
    # ---- spiral / mic -------------------------------------------------------------
    sp_s0=20.0, sp_s1=140.0, sp_pitch=18.7,
    sp_gap_deg=100.0, sp_gap_center_deg=173.0,        # C-gap faces the leadpipe
    sp_w=12.0, sp_pad_gap=2.0, sp_base=1.6, sp_depth=6.4, sp_inner_w=7.0,
    sp_flex_h=2.4,                                    # PETG flexure thickness in the gap
    sp_end_theta_deg=90.0,                            # helix end (rim side) at top of bell
    strip_w=5.0, diffuser_t=0.9,
    mic_arm_w=14.2, mic_arm_h=11.3,
    mic_socket=20.0, mic_socket_depth=14.0, mic_insert_angles=(0, 10, 20, 30),
    mic_thread_hole=9.7,                              # for a 3/8"-16 brass insert
    # ---- hall rail / caps ---------------------------------------------------------
    hall_rail_z=(118.0, 123.0), magnet_d=10.2, magnet_t=3.3, hall_pocket=5.6,
    # ---- print -------------------------------------------------------------------
    bed=256.0,
)
G_H_MOUNT = 0.5     # voxel size for mount/pod (mm)


from scipy.interpolate import PchipInterpolator
_BELL = PchipInterpolator(P["bell_prof_s"], P["bell_prof_r"])    # smooth (monotone) bell flare through the profile points
_BELLD = _BELL.derivative()


def _bell_r(s):
    return float(_BELL(s))


# =============================================================== SDF LIBRARY ===
class Grid:
    def __init__(self, lo, hi, h):
        self.h = float(h); self.lo = np.array(lo, float)
        self.n = tuple(int(math.ceil((hi[i] - lo[i]) / h)) + 1 for i in range(3))
        self.ax = [(self.lo[i] + self.h * np.arange(self.n[i])).astype(np.float32) for i in range(3)]

    def full(self, v=1e3):
        return np.full(self.n, v, np.float32)

    def idx(self, lo, hi):
        sl = []
        for i in range(3):
            a = int(max(0, math.floor((lo[i] - self.lo[i]) / self.h)))
            b = int(min(self.n[i], math.ceil((hi[i] - self.lo[i]) / self.h) + 1))
            sl.append(slice(a, max(a, b)))
        return tuple(sl)

    def coords(self, sl):
        X = self.ax[0][sl[0]][:, None, None]; Y = self.ax[1][sl[1]][None, :, None]; Z = self.ax[2][sl[2]][None, None, :]
        return X, Y, Z


def _apply(g, F, lo, hi, fn, op=np.minimum):
    sl = g.idx(lo, hi)
    if any(s.stop <= s.start for s in sl): return
    X, Y, Z = g.coords(sl)
    F[sl] = op(F[sl], fn(X, Y, Z).astype(np.float32))


def sd_capsule_poly(g, F, pts, r, op=np.minimum):
    pts = np.asarray(pts, float)
    for a, b in zip(pts[:-1], pts[1:]):
        lo = np.minimum(a, b) - r - 2 * g.h; hi = np.maximum(a, b) + r + 2 * g.h
        ab = b - a; L2 = float(ab @ ab)

        def fn(X, Y, Z, a=a, ab=ab, L2=L2):
            px = X - a[0]; py = Y - a[1]; pz = Z - a[2]
            t = np.clip((px * ab[0] + py * ab[1] + pz * ab[2]) / L2, 0, 1)
            return np.sqrt((px - t * ab[0]) ** 2 + (py - t * ab[1]) ** 2 + (pz - t * ab[2]) ** 2) - r
        _apply(g, F, lo, hi, fn, op)


def sd_cyl(g, F, axis, c, r, a0, a1, op=np.minimum, rounded=0.0):
    """capped cylinder along axis (0,1,2); c = centre coords on the other two axes (in axis order)."""
    o = [i for i in range(3) if i != axis]
    lo = np.zeros(3); hi = np.zeros(3)
    lo[o[0]] = c[0] - r - 2 * g.h; hi[o[0]] = c[0] + r + 2 * g.h
    lo[o[1]] = c[1] - r - 2 * g.h; hi[o[1]] = c[1] + r + 2 * g.h
    lo[axis] = a0 - 2 * g.h; hi[axis] = a1 + 2 * g.h
    am = 0.5 * (a0 + a1); hl = 0.5 * (a1 - a0)

    def fn(X, Y, Z):
        V = (X, Y, Z)
        d1 = np.sqrt((V[o[0]] - c[0]) ** 2 + (V[o[1]] - c[1]) ** 2) - r
        d2 = np.abs(V[axis] - am) - hl
        return np.minimum(np.maximum(d1, d2), 0) + np.sqrt(np.maximum(d1, 0) ** 2 + np.maximum(d2, 0) ** 2)
    _apply(g, F, lo, hi, fn, op)


def sd_box(g, F, lo, hi, rad=0.0, op=np.minimum, pad=0.0):
    lo = np.asarray(lo, float); hi = np.asarray(hi, float)
    cen = 0.5 * (lo + hi); hs = 0.5 * (hi - lo) - rad

    def fn(X, Y, Z):
        qx = np.abs(X - cen[0]) - hs[0]; qy = np.abs(Y - cen[1]) - hs[1]; qz = np.abs(Z - cen[2]) - hs[2]
        out = np.sqrt(np.maximum(qx, 0) ** 2 + np.maximum(qy, 0) ** 2 + np.maximum(qz, 0) ** 2)
        return out + np.minimum(np.maximum(qx, np.maximum(qy, qz)), 0) - rad
    _apply(g, F, lo - 2 * g.h - pad, hi + 2 * g.h + pad, fn, op)


def sd_poly_extrude(g, F, poly, axis, a0, a1, op=np.minimum, rad=0.0):
    """polygon in the two remaining axes (order = increasing axis index), extruded along axis."""
    from matplotlib.path import Path
    o = [i for i in range(3) if i != axis]
    poly = np.asarray(poly, float)
    lo = np.zeros(3); hi = np.zeros(3)
    lo[o[0]] = poly[:, 0].min() - 3; hi[o[0]] = poly[:, 0].max() + 3
    lo[o[1]] = poly[:, 1].min() - 3; hi[o[1]] = poly[:, 1].max() + 3
    lo[axis] = a0 - 2 * g.h; hi[axis] = a1 + 2 * g.h
    sl = g.idx(lo, hi)
    u = g.ax[o[0]][sl[o[0]]]; v = g.ax[o[1]][sl[o[1]]]
    U, Vv = np.meshgrid(u, v, indexing='ij')
    inside = Path(poly).contains_points(np.c_[U.ravel(), Vv.ravel()]).reshape(U.shape)
    sd2 = (ndi.distance_transform_edt(~inside) - ndi.distance_transform_edt(inside)) * g.h
    sd2 = sd2 - rad if rad else sd2   # rad>0 offsets outward
    am = 0.5 * (a0 + a1); hl = 0.5 * (a1 - a0)
    A = g.ax[axis][sl[axis]]
    shape = [1, 1, 1]; shape[axis] = len(A)
    dA = (np.abs(A - am) - hl).reshape(shape)
    # place sd2 into right axes
    S2 = sd2
    if axis == 0: S2 = sd2[None, :, :]
    elif axis == 1: S2 = sd2[:, None, :]
    else: S2 = sd2[:, :, None]
    d = np.maximum(S2, dA)
    out = np.minimum(np.maximum(S2, dA), 0) + np.sqrt(np.maximum(S2, 0) ** 2 + np.maximum(dA, 0) ** 2)
    F[sl] = op(F[sl], out.astype(np.float32))


def sd_sphere(g, F, c, r, op=np.minimum):
    c = np.asarray(c, float)
    _apply(g, F, c - r - 2 * g.h, c + r + 2 * g.h,
           lambda X, Y, Z: np.sqrt((X - c[0]) ** 2 + (Y - c[1]) ** 2 + (Z - c[2]) ** 2) - r, op)


def occ_to_sd(occ, h, smooth=0.7):
    d_out = ndi.distance_transform_edt(~occ); d_in = ndi.distance_transform_edt(occ)
    sd = ((d_out - 0.5) - (d_in - 0.5)) * h
    sd = np.where(occ, -(d_in - 0.5) * h, (d_out - 0.5) * h).astype(np.float32)
    if smooth: sd = ndi.gaussian_filter(sd, smooth)
    return sd


def morph_open(occ, r_mm, h):
    """opening (rounds convex edges / removes < 2r features)"""
    er = ndi.distance_transform_edt(occ) * h > r_mm
    return ndi.distance_transform_edt(~er) * h <= r_mm


def morph_close(occ, r_mm, h):
    """closing (fills concave corners with radius r: root fillets)"""
    dil = ndi.distance_transform_edt(~occ) * h <= r_mm
    return ndi.distance_transform_edt(dil) * h > r_mm


def keep_component(occ, seed_mask):
    lab, n = ndi.label(occ)
    keep = np.unique(lab[seed_mask & occ]); keep = keep[keep > 0]
    return np.isin(lab, keep)


def largest_component(occ):
    lab, n = ndi.label(occ)
    if n <= 1: return occ
    cnt = np.bincount(lab.ravel()); cnt[0] = 0
    return lab == cnt.argmax()


def manifold_simplify(m, tol=0.03):
    import manifold3d as mf
    M = mf.Manifold(mf.Mesh(vert_properties=np.asarray(m.vertices, np.float32), tri_verts=np.asarray(m.faces, np.uint32)))
    if M.status() != mf.Error.NoError: return m
    mm = M.simplify(tol).to_mesh()
    m2 = trimesh.Trimesh(mm.vert_properties[:, :3].copy(), mm.tri_verts.copy(), process=True)
    return m2 if m2.is_watertight and m2.volume > 0 else m


def to_manifold(m):
    import manifold3d as mf
    return mf.Manifold(mf.Mesh(vert_properties=np.asarray(m.vertices, np.float32), tri_verts=np.asarray(m.faces, np.uint32)))


def from_manifold(M):
    mm = M.to_mesh()
    return trimesh.Trimesh(mm.vert_properties[:, :3].copy(), mm.tri_verts.copy(), process=False)


def sd_to_mesh(g, sd, level=0.0, name=None, pad_check=True, simplify=250000, taubin=0):
    sdp = np.pad(sd, 1, constant_values=1.0)
    v, f, n, _ = measure.marching_cubes(sdp, level, spacing=(g.h, g.h, g.h))
    v = v + g.lo - g.h
    m = trimesh.Trimesh(v, f, process=True)
    m.merge_vertices(); m.update_faces(m.nondegenerate_faces()); m.remove_unreferenced_vertices()
    trimesh.repair.fix_normals(m)
    if taubin:
        trimesh.smoothing.filter_taubin(m, lamb=0.5, nu=0.53, iterations=taubin)
    if simplify and len(m.faces) > 20000:
        m = manifold_simplify(m, 0.03)
    if len(m.split(only_watertight=False)) > 1:
        parts = m.split(only_watertight=False)
        m = max(parts, key=lambda p: len(p.faces))
    return m


def finalize(m):
    """snap to float32 and weld exactly as an STL reader will; only if that breaks watertightness, drop sliver faces."""
    v = m.vertices.astype(np.float32).astype(np.float64)
    base = trimesh.Trimesh(v, m.faces, process=False)
    mm = base.copy(); mm.merge_vertices()
    if mm.is_watertight: return mm
    for tol in (1e-9, 1e-7, 1e-6, 1e-5, 1e-4):
        keep = base.area_faces > tol
        mm = trimesh.Trimesh(base.vertices, base.faces[keep], process=False)
        mm.merge_vertices(); mm.update_faces(mm.nondegenerate_faces()); mm.remove_unreferenced_vertices()
        if mm.is_watertight: return mm
    return mm


def _set_offset(name, off):
    f = os.path.join(HERE, "stl_offsets.json")
    d = json.load(open(f)) if os.path.exists(f) else {}
    d[name] = [float(x) for x in off]
    json.dump(d, open(f, "w"), indent=1)


def save_stl(m, name, offset=None):
    """offset: the STL is written with this vector subtracted (assembled position = STL + offset)."""
    path = os.path.join(STL, name + ".stl")
    if offset is not None:
        m = m.copy(); m.apply_translation(-np.asarray(offset)); _set_offset(name, offset)
    else:
        f = os.path.join(HERE, "stl_offsets.json")
        if os.path.exists(f):
            d = json.load(open(f))
            if name in d: d.pop(name); json.dump(d, open(f, "w"), indent=1)
    m2 = finalize(m)
    m2.export(path)
    chk = trimesh.load(path, force="mesh")
    if not chk.is_watertight:
        print("WARNING not watertight after STL round trip:", name)
    return path


# ====================================================== REFERENCE HORN SDF ====
def export_ref_objs():
    """Export the Sketchfab 'trump' collection (mm, world coords) once, read-only, via Blender."""
    out = os.path.join(CACHE, "allobjs.pkl")
    if os.path.exists(out): return out
    script = os.path.join(CACHE, "_exp.py")
    open(script, "w").write(f'''
import bpy, numpy as np, pickle
dg=bpy.context.evaluated_depsgraph_get(); res={{}}
for o in bpy.data.objects:
    if o.type!='MESH': continue
    ev=o.evaluated_get(dg); me=ev.to_mesh(); me.calc_loop_triangles()
    v=np.array([tuple(o.matrix_world@v.co) for v in me.vertices])*1000
    f=np.array([t.vertices[:] for t in me.loop_triangles])
    res[o.name]=(v,f,[c.name for c in o.users_collection]); ev.to_mesh_clear()
pickle.dump(res,open({out!r},'wb'))
''')
    os.system(f'/Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup "{V1}/hyperFRday2.blend" --python "{script}" >/dev/null 2>&1')
    return out


_REPLACED = set(["Cylinder.001", "Cylinder.004", "Cylinder.013", "Cylinder.014", "Cylinder.015", "Cylinder.016", "Cylinder.017",
                 "Cylinder.018", "Cylinder.019", "Cylinder.020", "Cylinder.021", "Cylinder.022", "Cylinder.023", "Cylinder.024",
                 "Cylinder.025", "Cylinder.026", "Cylinder.027", "Cylinder.028", "Cylinder.029", "Cylinder.030", "Cylinder.031",
                 "Cylinder.032", "Cylinder.033", "Cylinder.009", "Cylinder.045", "Cylinder.046", "Cylinder.005", "Cylinder.006",
                 "Torus", "Torus.001", "Cylinder.041", "Cylinder.047"])
_TUBE_INFLATE = {"Cylinder": 1.0, "Cylinder.042": 1.0, "Cylinder.040": 1.0, "Cylinder.039": 1.0, "Cylinder.037": 1.0, "Cylinder.034": 1.0}


def ring_pts(cx, cz, a, b, y, a0=0, a1=360, n=48):
    t = np.radians(np.linspace(a0, a1, n))
    return np.c_[cx + a * np.cos(t), np.full_like(t, y), cz + b * np.sin(t)]


def horn_field(g, include_shell=True):
    """signed distance to the standard-dimension reference horn on grid g (negative inside)."""
    F = g.full(); p = P
    cy = p["casing_y"]
    for cx in p["casing_cx"]:
        sd_cyl(g, F, 2, (cx, cy), p["casing_od"] / 2, p["casing_z0"], p["casing_z1"])
        sd_cyl(g, F, 2, (cx, cy), p["cap_od"] / 2, p["cap_z0"], p["cap_z1"])
        sd_cyl(g, F, 2, (cx, cy), p["stem_od"] / 2, p["cap_z1"], p["stem_z1"])
        sd_cyl(g, F, 2, (cx, cy), p["button_od"] / 2, p["stem_z1"], p["button_z1"])
        sd_cyl(g, F, 2, (cx, cy), p["botcap_od"] / 2, p["botcap_z0"], p["casing_z0"])
    rr = p["slide_leg_od"] / 2
    # 1st slide (+x), 3rd slide (-x): legs + crowns (crown = half circle between the legs)
    zc = 0.5 * (p["slide_leg_z"][0] + p["slide_leg_z"][1]); R = 0.5 * (p["slide_leg_z"][1] - p["slide_leg_z"][0])
    for zl in p["slide_leg_z"]:
        sd_capsule_poly(g, F, [(884, p["slide1_y"], zl), (p["slide1_crown_x"], p["slide1_y"], zl)], rr)
        sd_capsule_poly(g, F, [(p["slide3_crown_x"], p["slide3_y"], zl), (823, p["slide3_y"], zl)], rr)
    t = np.radians(np.linspace(-90, 90, 24))
    sd_capsule_poly(g, F, np.c_[p["slide1_crown_x"] + R * np.cos(t), np.full_like(t, p["slide1_y"]), zc + R * np.sin(t)], rr)
    sd_capsule_poly(g, F, np.c_[p["slide3_crown_x"] - R * np.cos(t), np.full_like(t, p["slide3_y"]), zc + R * np.sin(t)], rr)
    r3 = p["ring3"]; sd_capsule_poly(g, F, ring_pts(r3["cx"], r3["cz"], r3["a"], r3["b"], r3["y"]), r3["tube_r"])
    sa = p["saddle"]; sd_capsule_poly(g, F, ring_pts(sa["cx"], sa["cz"], sa["r"], sa["r"], sa["y"], 90, 270, 24), sa["tube_r"])
    br = p["brace"]; sd_capsule_poly(g, F, [(br["x"], br["y"], br["z0"]), (br["x"], br["y"], br["z1"])], br["r"])
    lp = p["leadpipe"]; sd_capsule_poly(g, F, [(lp["x0"], lp["y"], lp["z"]), (lp["x1"], lp["y"], lp["z"])], lp["r"])
    if include_shell:
        objs = pickle.load(open(export_ref_objs(), "rb"))
        shell = np.zeros(g.n, bool)
        for n, (v, f, c) in objs.items():
            if "trump" not in c or n in _REPLACED or len(f) == 0: continue
            lo = v.min(0); hi = v.max(0)
            if hi[0] < g.lo[0] or lo[0] > g.ax[0][-1] or hi[1] < g.lo[1] or lo[1] > g.ax[1][-1] or hi[2] < g.lo[2] or lo[2] > g.ax[2][-1]:
                continue
            m = trimesh.Trimesh(v, f, process=False)
            pts, _ = trimesh.sample.sample_surface(m, max(2000, int(m.area * 12)))
            ix = np.floor((pts - g.lo) / g.h + 0.5).astype(int)
            ok = np.all((ix >= 0) & (ix < np.array(g.n)), axis=1)
            Ftmp = None
            ix = ix[ok]
            infl = _TUBE_INFLATE.get(n, 0.0)
            shell_obj = np.zeros(g.n, bool); shell_obj[ix[:, 0], ix[:, 1], ix[:, 2]] = True
            if infl > 0:
                # inflate: distance to the shell voxels < 1.0 + infl
                sub = np.argwhere(shell_obj)
                if len(sub):
                    lo_i = np.maximum(sub.min(0) - int(infl / g.h) - 3, 0); hi_i = np.minimum(sub.max(0) + int(infl / g.h) + 4, g.n)
                    sl = tuple(slice(lo_i[i], hi_i[i]) for i in range(3))
                    d = ndi.distance_transform_edt(~shell_obj[sl]) * g.h
                    sh = shell_obj.copy(); sh[sl] = d <= infl
                    shell_obj = sh
            shell |= shell_obj
        # shell voxels -> distance
        d = ndi.distance_transform_edt(~shell) * g.h - 0.5 * g.h
        F = np.minimum(F, d.astype(np.float32))
    return F


def keepout_items():
    """analytic keep-out geometry for reporting and cutting: list of (name, kind, params)"""
    p = P; a = p["open_air"]; ra = p["ring_air"]
    zc = [p["slide_leg_z"][0], p["slide_leg_z"][1]]
    items = [
        ("slide3_travel", "box", ((p["slide3_crown_x"] - p["slide_pull"] - 10, p["slide3_y"] - p["slide_leg_od"] / 2 - a, zc[0] - p["slide_leg_od"] / 2 - a),
                                  (835.0, p["slide3_y"] + p["slide_leg_od"] / 2 + a, zc[1] + p["slide_leg_od"] / 2 + a))),
        ("slide1_travel", "box", ((884.0, p["slide1_y"] - p["slide_leg_od"] / 2 - a, zc[0] - p["slide_leg_od"] / 2 - a),
                                  (p["slide1_crown_x"] + p["slide_pull"] + 10, p["slide1_y"] + p["slide_leg_od"] / 2 + a, zc[1] + p["slide_leg_od"] / 2 + a))),
        ("ring3_finger", "ycyl", (p["ring3"]["cx"], p["ring3"]["cz"], p["ring3"]["a"] + p["ring3"]["tube_r"] + ra, p["ring3"]["b"] + p["ring3"]["tube_r"] + ra, 470.0, p["plate_y0"])),
        ("saddle_thumb", "ycyl", (p["saddle"]["cx"], p["saddle"]["cz"], p["saddle"]["r"] + p["saddle"]["tube_r"] + ra, p["saddle"]["r"] + p["saddle"]["tube_r"] + ra, 470.0, p["plate_y0"])),
        ("hall_rail_zone", "box", ((817.5, 497.0, 108.0), (889.5, 524.5, 121.5))),
        ("button_fingers", "box", ((815.0, 497.0, 121.5), (895.0, 531.0, 175.0))),
        ("waterkey3_assumed", "sphere", ((757.0, 500.0, 20.0), 16.0)),   # [A] 3rd-slide crown water key
        ("waterkey_tuning_assumed", "sphere", ((1045.0, 494.0, 58.0), 16.0)),   # [A] main tuning slide water key
    ]
    return items


def keepout_field(g, upto_y=None):
    F = g.full()
    for n, k, d in keepout_items():
        if k == "box": sd_box(g, F, d[0], d[1])
        elif k == "sphere": sd_sphere(g, F, d[0], d[1])
        elif k == "ycyl":
            cx, cz, ra_, rb_, y0, y1 = d
            lo = (cx - ra_ - 2, y0, cz - rb_ - 2); hi = (cx + ra_ + 2, y1, cz + rb_ + 2)
            _apply(g, F, lo, hi, lambda X, Y, Z, cx=cx, cz=cz, ra_=ra_, rb_=rb_, y0=y0, y1=y1:
                   np.maximum(np.sqrt(((X - cx) / ra_) ** 2 + ((Z - cz) / rb_) ** 2) * min(ra_, rb_) - min(ra_, rb_),
                              np.maximum(y0 - Y, Y - y1)))
    return F


# ---- exact distance used by verify (points) ------------------------------------
def _seg_d(pts, a, b):
    ab = b - a; L2 = float(ab @ ab); t = np.clip(((pts - a) @ ab) / L2, 0, 1)
    return np.linalg.norm(pts - (a + t[:, None] * ab), axis=1)


def poly_d(pts, poly):
    d = np.full(len(pts), 1e9)
    for a, b in zip(poly[:-1], poly[1:]): d = np.minimum(d, _seg_d(pts, np.asarray(a, float), np.asarray(b, float)))
    return d


def horn_items_for_check():
    """(name, kind, data, radius): kind 'cyl' = finite cylinder along z (cx, cy, z0, z1); 'poly' = polyline capsule."""
    p = P; items = []; cy = p["casing_y"]
    for i, cx in enumerate(p["casing_cx"]):
        items.append((f"casing{i+1}", "cyl", (cx, cy, p["casing_z0"], p["casing_z1"]), p["casing_od"] / 2))
        items.append((f"cap{i+1}", "cyl", (cx, cy, p["cap_z0"], p["cap_z1"]), p["cap_od"] / 2))
        items.append((f"stem{i+1}", "cyl", (cx, cy, p["cap_z1"], p["stem_z1"]), p["stem_od"] / 2))
        items.append((f"button{i+1}", "cyl", (cx, cy, p["stem_z1"], p["button_z1"]), p["button_od"] / 2))
        items.append((f"botcap{i+1}", "cyl", (cx, cy, p["botcap_z0"], p["casing_z0"]), p["botcap_od"] / 2))
    rr = p["slide_leg_od"] / 2; zc = 0.5 * sum(p["slide_leg_z"]); R = 0.5 * (p["slide_leg_z"][1] - p["slide_leg_z"][0])
    for j, zl in enumerate(p["slide_leg_z"]):
        items.append((f"slide1_leg{j}", "poly", [(884, p["slide1_y"], zl), (p["slide1_crown_x"], p["slide1_y"], zl)], rr))
        items.append((f"slide3_leg{j}", "poly", [(p["slide3_crown_x"], p["slide3_y"], zl), (823, p["slide3_y"], zl)], rr))
    t = np.radians(np.linspace(-90, 90, 24))
    items.append(("slide1_crown", "poly", list(np.c_[p["slide1_crown_x"] + R * np.cos(t), np.full_like(t, p["slide1_y"]), zc + R * np.sin(t)]), rr))
    items.append(("slide3_crown", "poly", list(np.c_[p["slide3_crown_x"] - R * np.cos(t), np.full_like(t, p["slide3_y"]), zc + R * np.sin(t)]), rr))
    r3 = p["ring3"]; items.append(("ring3", "poly", list(ring_pts(r3["cx"], r3["cz"], r3["a"], r3["b"], r3["y"])), r3["tube_r"]))
    sa = p["saddle"]; items.append(("saddle_hook", "poly", list(ring_pts(sa["cx"], sa["cz"], sa["r"], sa["r"], sa["y"], 90, 270, 24)), sa["tube_r"]))
    br = p["brace"]; items.append(("brace", "poly", [(br["x"], br["y"], br["z0"]), (br["x"], br["y"], br["z1"])], br["r"]))
    lp = p["leadpipe"]; items.append(("leadpipe", "poly", [(lp["x0"], lp["y"], lp["z"]), (lp["x1"], lp["y"], lp["z"])], lp["r"]))
    return items


def cyl_d(pts, cx, cy, z0, z1):
    rho = np.hypot(pts[:, 0] - cx, pts[:, 1] - cy); zm = 0.5 * (z0 + z1); hz = 0.5 * (z1 - z0)
    return rho, np.abs(pts[:, 2] - zm) - hz


def bell_sd_points(pts):
    """distance (approx, negative inside) from points to the bell solid of revolution (rim at bell_rim_x)."""
    yb, zb = P["bell_axis"]; xr = P["bell_rim_x"]
    s = xr - pts[:, 0]; rho = np.hypot(pts[:, 1] - yb, pts[:, 2] - zb)
    sc = np.clip(s, 0, 230)
    r = _BELL(sc); slope = _BELLD(sc)
    d = (rho - r) / np.sqrt(1 + slope ** 2)
    d[(s < 0) | (s > 230)] = 1e3
    rim = np.hypot(np.where(s < 0, -s, np.where(s < 0.0, 0, s)) * 0 + (s), rho - _BELL(0.0)) - 2.0   # rim bead (r=2) as a torus around the rim circle
    return np.minimum(d, rim)


def clearance_report(mesh, n=60000, skip=()):
    pts = mesh.sample(n)
    res = {}
    for name, kind, data, r in horn_items_for_check():
        if name in skip: continue
        if kind == "cyl":
            cx, cy, z0, z1 = data
            rho, dz = cyl_d(pts, cx, cy, z0, z1); d1 = rho - r
            d = np.minimum(np.maximum(d1, dz), 0) + np.hypot(np.maximum(d1, 0), np.maximum(dz, 0))
        else:
            d = poly_d(pts, np.asarray(data, float)) - r
        res[name] = float(d.min())
    res["bell"] = float(bell_sd_points(pts).min())
    ko = {}
    for nme, kind, d in keepout_items():
        if kind == "sphere":
            ko[nme] = float((np.linalg.norm(pts - np.asarray(d[0]), axis=1) - d[1]).min())
    return res, ko, pts


# ============================================================ V1 ARMS INPUT ====
PLATE_OUTLINE = np.array([  # xz outline of the plate; left/top-right notches are v1's (they recede from the ring and hook)  [M]
    [745, 41], [796, 71], [801, 82], [808, 92], [810, 104], [821, 119], [877, 128], [920, 126], [940, 116], [942, 104],
    [942, 44], [930, 38], [894, 34], [894, 14], [888, -4], [877, -13], [821, -13], [809, -4], [804, 9], [796, 21],
    [793, 23], [765, 24], [746, 27], [743, 29], [745, 41]], float)


def v1_arm_occ(g):
    f = os.path.join(CACHE, f"v1_occ_{g.n[0]}_{g.n[1]}_{g.n[2]}_{g.h}.npz")
    if os.path.exists(f): return np.load(f)["occ"]
    import fast_simplification
    m = trimesh.load(os.path.join(V1, "_mount_v3.stl"), force="mesh")
    main = max(m.split(only_watertight=False), key=lambda q: len(q.faces))
    v, fcs = fast_simplification.simplify(main.vertices.astype(np.float32), main.faces.astype(np.int32), target_reduction=0.8)
    mm = trimesh.Trimesh(v, fcs)
    vox = mm.voxelized(g.h).fill()
    pts = vox.points
    ix = np.floor((pts - g.lo) / g.h + 0.5).astype(int)
    ok = np.all((ix >= 0) & (ix < np.array(g.n)), axis=1); ix = ix[ok]
    occ = np.zeros(g.n, bool); occ[ix[:, 0], ix[:, 1], ix[:, 2]] = True
    np.savez_compressed(f, occ=occ)
    return occ


def dovetail_z(g, F, xc, y_front, z0, z1, neck, head, h, clear=0.0, op=np.minimum, grow_back=0.0):
    """dovetail prism along z, narrow at y_front, widening to the rear (y decreasing) by depth h."""
    lo = (xc - head / 2 - 2, y_front - h - 2, z0 - 2); hi = (xc + head / 2 + 2, y_front + 1 + grow_back, z1 + 2)
    wn = neck / 2 + clear; wh = head / 2 + clear; hh = h + clear
    am = 0.5 * (z0 + z1); hl = 0.5 * (z1 - z0)

    def fn(X, Y, Z):
        t = y_front - Y
        w = wn + (wh - wn) * np.clip(t, 0, hh) / hh
        d1 = (np.abs(X - xc) - w) * 0.93
        return np.maximum(np.maximum(d1, -t), np.maximum(t - hh, np.abs(Z - am) - hl))
    _apply(g, F, lo, hi, fn, op)


def dovetail_x(g, F, yc, z_front, x0, x1, neck, head, h, clear=0.0, op=np.minimum, down=True, grow=0.0):
    """dovetail prism along x. Narrow at plane z_front, widening away by depth h (down=True -> toward -z)."""
    lo = (x0 - 2, yc - head / 2 - 2, z_front - h - 2); hi = (x1 + 2, yc + head / 2 + 2, z_front + 1 + grow)
    wn = neck / 2 + clear; wh = head / 2 + clear; hh = h + clear
    am = 0.5 * (x0 + x1); hl = 0.5 * (x1 - x0)

    def fn(X, Y, Z):
        t = (z_front - Z) if down else (Z - z_front)
        w = wn + (wh - wn) * np.clip(t, 0, hh) / hh
        d1 = (np.abs(Y - yc) - w) * 0.93
        return np.maximum(np.maximum(d1, -t), np.maximum(t - hh, np.abs(X - am) - hl))
    _apply(g, F, lo, hi, fn, op)


def build_mount(save=True, max_pads=24):
    p = P; h = G_H_MOUNT
    g = Grid((712, 478, -34), (948, 546, 140), h)
    t0 = time.time()
    occ_v1 = v1_arm_occ(g)
    Y = g.ax[1][None, :, None]
    arms = occ_v1 & (Y <= p["plate_y0"] + 2.0)
    sd_arms = occ_to_sd(arms, h, smooth=0.0) - p["arm_thicken"]
    F = sd_arms.copy()
    # plate
    Fp = g.full()
    sd_poly_extrude(g, Fp, PLATE_OUTLINE, 1, p["plate_y0"], p["plate_y1"])
    F = np.minimum(F, Fp)
    F = np.where((Y > p["plate_y0"]) & (Fp >= 0), np.maximum(F, 0.01), F)    # arms never extend behind the plate outline
    # rest rail (hangs below the plate bottom, along x)  [male]
    Fr = g.full()
    dovetail_x(g, Fr, 0.5 * (p["plate_y0"] + p["plate_y1"]), -12.5, 825.0, 877.0, p["dt_neck"], p["dt_head"], p["dt_h"], 0.0, down=True, grow=2.5)
    F = np.minimum(F, Fr)
    occ = F < 0
    Xg = g.ax[0][:, None, None]; Zg = g.ax[2][None, None, :]; Yg = g.ax[1][None, :, None]
    rail_bot = -12.5 - p["dt_h"]
    occ &= ~((Xg > 828.5) & (Xg < 838.0) & (Yg > 533.0) & (Yg < 539.0) & (Zg < rail_bot + 1.6) & (Zg > rail_bot - 1))   # latch notch in the rest rail
    occ &= ~((Xg < 831.0) & (Zg < rail_bot + 1.5 * (1 - (Xg - 825.0) / 6.0)) & (Zg > rail_bot - 1.0) & (Xg > 824.0) & (Yg > 520) & (Yg < 552) & (Zg < -12.5 - 1))   # lead-in ramp
    print("union", time.time() - t0)
    # root fillets
    occ = morph_close(occ, p["root_fillet"], h)
    Zg2 = g.ax[2][None, None, :]
    occ &= ~((Zg2 < -12.5) & (Fr >= 0))          # nothing below the plate except the rest rail itself
    print("closed", time.time() - t0)
    # clearance to the standard horn + open-air keep-outs (arms only; keep-outs stop at the plate rear face)
    H = horn_field(g)
    K = keepout_field(g)
    occ &= ~(H < p["rigid_clear"] + p["cut_margin"])
    occ &= ~(K < 0.6)
    # pod rail channel (female dovetail along z, open at the bottom) + latch tab (floor, flexes in y)
    C = g.full()
    dovetail_z(g, C, p["pod_rail_x"], p["plate_y1"], -40.0, p["pod_rail_z1"] + 0.6, p["dt_neck"], p["dt_head"], p["dt_h"], p["dt_clear"], grow_back=2.0)
    occ &= ~(C < 0)
    S = g.full()   # latch slits: tab x 845..878, z -8..-1, thickness = floor
    yfl0, yfl1 = p["plate_y0"] - 0.5, p["plate_y1"] - p["dt_h"] - p["dt_clear"] + 0.0
    tz0, tz1 = -8.0, -1.0
    for lo, hi in [((845.0, yfl0, tz0 - 1.1), (880.0, yfl1 + 0.2, tz0 - 0.1)),
                   ((845.0, yfl0, tz1 + 0.1), (880.0, yfl1 + 0.2, tz1 + 1.1)),
                   ((878.0, yfl0, tz0 - 1.1), (879.1, yfl1 + 0.2, tz1 + 1.1))]:
        sd_box(g, S, lo, hi)
    occ &= ~(S < 0)
    # ---- TPU contact pads: patch the arm surface that hugs the horn into ~30 mm cells, recess it, emit one pad per patch
    near = occ & (H < 1.8)
    cell = 30.0
    idx = np.argwhere(near)
    cx_ = np.floor((g.lo[0] + idx[:, 0] * h) / cell).astype(int); cy_ = np.floor((g.lo[1] + idx[:, 1] * h) / cell).astype(int); cz_ = np.floor((g.lo[2] + idx[:, 2] * h) / cell).astype(int)
    key = (cx_ - cx_.min()) * 10000 + (cy_ - cy_.min()) * 100 + (cz_ - cz_.min())
    uk, inv, cnts = np.unique(key, return_inverse=True, return_counts=True)
    lab = np.zeros(g.n, np.int32); lab[idx[:, 0], idx[:, 1], idx[:, 2]] = inv + 1
    dist, nn = ndi.distance_transform_edt(~near, return_indices=True)
    owner = lab[nn[0], nn[1], nn[2]]
    region_all = dist * h <= 1.8
    pads = []
    for k in np.argsort(-cnts)[:max_pads]:
        if cnts[k] < 60: continue
        reg = region_all & (owner == k + 1)
        pad = reg & (H > -p["pad_interf"]) & (H < 1.6) & (Y < p["plate_y0"] + 4.0)
        if pad.sum() < 80: continue
        occ &= ~(reg & (H < 1.7))
        pads.append(pad)
    occ = morph_open(occ, p["edge_round"], h)   # round convex edges
    # keep what is connected to the plate
    seed = np.zeros(g.n, bool)
    sd_seed = g.full(); sd_box(g, sd_seed, (835, 533, 60), (885, 539, 100)); seed = sd_seed < 0
    occ = keep_component(occ, seed)
    print("done", time.time() - t0, occ.sum())
    # stats: v1 arm volume vs v2, volume cut by each keep-out, thin (<5 mm) cores
    stats = {"v1_arm_voxels_mm3": float((occ_v1 & (Y <= p["plate_y0"])).sum() * h ** 3), "v2_mount_mm3": float(occ.sum() * h ** 3)}
    core = ndi.binary_erosion(morph_open(occ, p["arm_min_thick"] / 2, h) ^ occ, iterations=3)
    lab_c, n_c = ndi.label(core)
    thin = []
    for i in range(1, n_c + 1):
        ii = np.argwhere(lab_c == i)
        if len(ii) * h ** 3 > 150: thin.append(dict(mm3=round(len(ii) * h ** 3), lo=(g.lo + ii.min(0) * h).round(0).tolist(), hi=(g.lo + ii.max(0) * h).round(0).tolist()))
    stats["regions_thinner_than_5mm"] = thin
    json.dump(stats, open(os.path.join(HERE, "mount_stats.json"), "w"), indent=1)
    np.savez_compressed(os.path.join(CACHE, "mount_occ.npz"), occ=occ)
    sd = occ_to_sd(occ, h, smooth=0.9)
    m = sd_to_mesh(g, sd, taubin=30)
    if save: save_stl(m, "mount_arms_plate")
    pad_meshes = []
    for f_ in os.listdir(STL):
        if f_.startswith("tpu_pad_"): os.remove(os.path.join(STL, f_))
    for i, pad in enumerate(pads):
        pad = largest_component(pad)
        pm = sd_to_mesh(g, occ_to_sd(pad, h, smooth=0.8), simplify=1, taubin=12)
        if pm.is_watertight and pm.volume > 400:
            pad_meshes.append(pm)
            if save: save_stl(pm, f"tpu_pad_{len(pad_meshes):02d}")
    return m, occ, g, pad_meshes



def rounded_rect_poly(x0, x1, z0, z1, r, n=24):
    from shapely.geometry import box
    b = box(x0 + r, z0 + r, x1 - r, z1 - r).buffer(r, resolution=n)
    return np.array(b.exterior.coords)


def inset_poly(poly, d):
    from shapely.geometry import Polygon
    q = Polygon(poly).buffer(-d)
    return np.array(q.exterior.coords)


# ============================================================ POD (face panel) ===
def build_pod(save=True):
    p = P; h = 0.4
    g = Grid((812, 533, -14), (946, 558, 132), h)
    y_back = p["plate_y1"]; y_pan1 = y_back + p["pod_back"] + p["pod_cav"]; y_face = y_pan1 + p["pod_panel"]
    outline = rounded_rect_poly(p["pod_x0"], p["pod_x1"], p["pod_z0"], p["pod_z1"], p["pod_r"])
    F = g.full(); sd_poly_extrude(g, F, outline, 1, y_back, y_face)
    occ = F < 0
    occ = morph_open(occ, 3.0, h)         # R3 on every outer edge incl. the face rim
    # rail (male dovetail on the back, along z)
    R = g.full()
    dovetail_z(g, R, p["pod_rail_x"], y_back + 0.01, p["pod_rail_z0"], p["pod_rail_z1"], p["dt_neck"], p["dt_head"], p["dt_h"], 0.0)
    # extend rail into the shell by 0.6
    sd_box(g, R, (p["pod_rail_x"] - p["dt_neck"] / 2, y_back - 0.2, p["pod_rail_z0"]), (p["pod_rail_x"] + p["dt_neck"] / 2, y_back + 0.8, p["pod_rail_z1"]))
    occ |= (R < 0)
    # latch notch in the rail rear face (engages the plate latch bump)
    N = g.full(); sd_box(g, N, (870.0, y_back - p["dt_h"] - 0.5, -6.5), (876.0, y_back - p["dt_h"] + 1.6, -2.5))
    occ &= ~(N < 0)
    # cavity (inset outline), leaves back wall + panel + perimeter wall
    cav = inset_poly(outline, p["pod_wall"])
    Cv = g.full(); sd_poly_extrude(g, Cv, cav, 1, y_back + p["pod_back"], y_pan1)
    occ &= ~(Cv < 0)
    # internal features (posts, ribs) added back inside the cavity
    A = g.full()
    cx, cz, pt = p["trellis_cx"], p["trellis_cz"], p["trellis_pitch"]
    for sx in (-1, 1):
        for sz in (-1, 1):
            px = cx + sx * p["trellis_hole_off"]; pz = cz + sz * p["trellis_hole_off"]
            sd_cyl(g, A, 1, (px, pz), 2.6, y_back + p["pod_back"] - 0.3, y_back + p["pod_back"] + 1.4)     # base boss
            sd_cyl(g, A, 1, (px, pz), 1.4, y_back + p["pod_back"], y_back + p["pod_back"] + 5.2)           # snap pin (M3 hole: pin 2.8)
    # joystick seat frame (module 26.5 x 34.5 [G])
    jx, jz = p["joy_xy"]      # KY-023 module 34.5 (x) * 26.5 (z) [G], seat frame 1.6 thick
    for lo, hi in [((jx - 18.9, y_back + 1.5, jz - 14.9), (jx + 18.9, y_pan1 - 2.5, jz - 13.3)),
                   ((jx - 18.9, y_back + 1.5, jz + 13.3), (jx + 18.9, y_pan1 - 2.5, jz + 14.9)),
                   ((jx - 18.9, y_back + 1.5, jz - 14.9), (jx - 17.3, y_pan1 - 2.5, jz + 14.9)),
                   ((jx + 17.3, y_back + 1.5, jz - 14.9), (jx + 18.9, y_pan1 - 2.5, jz + 14.9))]:
        sd_box(g, A, lo, hi)
    # ribs from the panel down behind fader slots (hold the slide pots by a printed clip rib)
    for fz in p["fader_z"]:
        for sx in (-1, 1):
            sd_box(g, A, (p["fader_x"] + sx * 31.5 - 1.0, y_back + 1.5, fz - 6.5), (p["fader_x"] + sx * 31.5 + 1.0, y_pan1 + 0.2, fz + 6.5))
    occ |= (A < 0) & ~(F >= 0 + 99)
    # ---- panel cut-outs (through y >= y_pan1 - 0.1) ------------------------------
    T = g.full()
    hw = (p["trellis_pitch"] * 4 - 1.9) / 2     # one window for the NeoTrellis elastomer sheet (1.0 mm frame)  [G: sheet ~ 60 x 60]
    sd_box(g, T, (cx - hw, y_pan1 - 0.2, cz - hw), (cx + hw, y_face + 1, cz + hw), rad=3.0)
    sd_cyl(g, T, 1, (jx, jz), p["joy_hole"] / 2, y_pan1 - 0.2, y_face + 1)
    for (x, z) in p["pot_xz"]: sd_cyl(g, T, 1, (x, z), p["pot_hole"] / 2, y_pan1 - 0.2, y_face + 1)
    for (x, z) in p["enc_xz"]: sd_cyl(g, T, 1, (x, z), p["enc_hole"] / 2, y_pan1 - 0.2, y_face + 1)
    for fz in p["fader_z"]:
        L, W = p["fader_slot"]; sd_box(g, T, (p["fader_x"] - L / 2, y_pan1 - 0.2, fz - W / 2), (p["fader_x"] + L / 2, y_face + 1, fz + W / 2), rad=W / 2 - 0.01)
    # back-wall windows behind pots (pins) and cable slots
    for (x, z) in p["pot_xz"]: sd_cyl(g, T, 1, (x, z), 7.0, y_back - 1, y_back + p["pod_back"] + 0.2)
    sd_box(g, T, (p["fader_x"] - 6, y_back - 1, p["pod_z0"] - 1), (p["fader_x"] + 6, y_back + 4.5, p["pod_z0"] + 5.0))   # cable slot bottom
    sd_box(g, T, (p["pod_x1"] - 5, y_back + 2.5, 97.0), (p["pod_x1"] + 1, y_pan1 - 0.3, 109.0))                          # cable slot to spiral
    occ &= ~(T < 0)
    occ = largest_component(occ)
    sd = occ_to_sd(occ, h, smooth=0.6)
    m = sd_to_mesh(g, sd, taubin=6)
    if save: save_stl(m, "pod_face_panel")
    return m



# ============================================================== PALM REST ======
def rest_geometry():
    p = P
    xc = 0.5 * (p["rest_x0"] + p["rest_x1"]); yc = 0.5 * (p["rest_y0"] + p["rest_y1"])
    return xc, yc


def build_rest(save=True):
    p = P; h = 0.5
    g = Grid((792, 482, -74), (920, 584, -8), h)
    xc, yc = rest_geometry()
    top = p["rest_top"]; depth = p["rest_depth"]; Rl = p["rest_R_long"]; Rt = p["rest_R_trans"]
    X = g.ax[0][:, None, None]; Y = g.ax[1][None, :, None]; Z = g.ax[2][None, None, :]
    hx = 0.5 * (p["rest_x1"] - p["rest_x0"]); hy = 0.5 * (p["rest_y1"] - p["rest_y0"]); rad = 22.0
    qx = np.abs(X - xc) - (hx - rad); qy = np.abs(Y - yc) - (hy - rad)
    d_xy = np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - rad
    zb = top - depth + (X - xc) ** 2 / (2 * Rl) + (Y - yc) ** 2 / (2 * Rt)
    gx = (X - xc) / Rl; gy = (Y - yc) / Rt
    q1 = d_xy; q2 = Z - top; q3 = (zb - Z) / np.sqrt(1 + gx ** 2 + gy ** 2)
    r = p["rest_round"]
    a_, b_, c_ = q1 + r, q2 + r, q3 + r
    sd = (np.sqrt(np.maximum(a_, 0) ** 2 + np.maximum(b_, 0) ** 2 + np.maximum(c_, 0) ** 2) + np.minimum(np.maximum(a_, np.maximum(b_, c_)), 0) - r).astype(np.float32)
    # --- female dovetail channel for the plate's rest rail (open at +x), latch tab, bump
    C = g.full()
    dovetail_x(g, C, 536.0, -12.5, 823.6, 925.0, p["dt_neck"], p["dt_head"], p["dt_h"], p["dt_clear"], down=True, grow=2.0)
    sd = np.maximum(sd, -C)
    ch_floor = -12.5 - p["dt_h"] - p["dt_clear"]
    S = g.full()
    t0z, t1z = ch_floor - p["latch_t"], ch_floor
    sd_box(g, S, (829.0, 531.3, t0z - 3.6), (871.0, 540.7, t0z))            # void under the tab
    sd_box(g, S, (829.0, 531.3, t0z), (867.0, 532.4, t1z + 0.01))           # slit y-
    sd_box(g, S, (829.0, 539.6, t0z), (867.0, 540.7, t1z + 0.01))           # slit y+
    sd_box(g, S, (828.0, 531.3, t0z), (829.2, 540.7, t1z + 0.01))           # end slit
    sd = np.maximum(sd, -S)
    B = g.full(); sd_box(g, B, (830.0, 534.0, t1z - 0.2), (836.0, 538.0, t1z + p["latch_bump"]))
    sd = np.minimum(sd, B)
    # --- cell bay (open at +x), electronics bay (open at -x), links, FSR pocket
    Cb = g.full()
    bay_y0 = 496.2; bay_y1 = bay_y0 + p["bay_y"]; bay_z1 = -27.0; bay_z0 = bay_z1 - p["bay_z"]
    sd_box(g, Cb, (822.0, bay_y0, bay_z0), (925.0, bay_y1, bay_z1), rad=1.5)
    el_y0, el_y1, el_z1 = 546.5, 572.0, -26.0; el_z0 = el_z1 - 26.0
    sd_box(g, Cb, (786.0, el_y0, el_z0), (904.0, el_y1, el_z1), rad=1.5)
    sd_box(g, Cb, (847.0, el_y0, el_z1 - 1), (859.0, el_y0 + 4.5, top + 1.0))       # cable slot up to the pod
    sd_box(g, Cb, (822.0, bay_y1 - 1, -38.0), (832.0, el_y0 + 1, -30.0))              # contact wires drawer -> electronics
    fx, fy = xc, 559.0
    zb_f = top - depth + (fx - xc) ** 2 / (2 * Rl) + (fy - yc) ** 2 / (2 * Rt)
    sd_cyl(g, Cb, 2, (fx, fy), p["fsr_pocket_d"] / 2, zb_f - 6.0, zb_f + 1.0)          # FSR recess (surface where the palm presses)
    sd_cyl(g, Cb, 2, (fx, fy), 1.7, zb_f, el_z0 + 1)                                   # FSR tail hole to the electronics bay
    for yy in (bay_y0 - 0.8, bay_y1 + 0.0):                                             # drawer detent notches
        sd_box(g, Cb, (892.0, yy, -38.0), (898.0, yy + 0.8, -41.0))
    for zz in (el_z0 + 5, el_z1 - 5):                                                   # end-cap snap notches
        sd_box(g, Cb, (797.5, el_y0 - 0.6, zz - 2), (801.0, el_y0, zz + 2)); sd_box(g, Cb, (797.5, el_y1, zz - 2), (801.0, el_y1 + 0.6, zz + 2))
    sd = np.maximum(sd, -Cb)
    occ = largest_component(sd < 0)
    sd = np.where(occ, sd, np.maximum(sd, 0.05))
    m = sd_to_mesh(g, ndi.gaussian_filter(sd, 0.4), taubin=4)
    if save: save_stl(m, "palm_rest_dome")
    return m


# ========================================================== BATTERY DRAWER =====
def build_drawer(save=True):
    p = P; h = 0.3
    L, W, Hh = p["drw_x"], p["drw_y"], p["drw_z"]
    g = Grid((-2, -2, -2), (L + 6, W + 2, Hh + 2), h)
    F = g.full()
    sd_box(g, F, (0, 0, 0), (L, W, Hh), rad=2.0)
    # finger pull (+x face)
    N = g.full()
    sd_box(g, N, (L - 4.0, W / 2 - 9, Hh - 5.0), (L + 1, W / 2 + 9, Hh + 1), rad=3.0)
    # ribbon slot through the front wall + ribbon anchor window
    sd_box(g, N, (L - 6.0, W / 2 - 5, 3.0), (L + 1, W / 2 + 5, 4.5))
    cell_r = p["cell_d"] / 2 + 0.15; cl = p["cell_l"] + 0.3
    zc = 12.0
    ycs = (W / 2 - 11.6, W / 2 + 11.6)
    x_neg = 4.0                      # negative end wall inner face (rear, -x side? positive at +x)
    x_pos = x_neg + cl
    for yc_ in ycs:
        sd_cyl(g, N, 0, (yc_, zc), cell_r, x_neg, x_pos)
        # snap lips: slot opening above axis, 19.4 wide, with relief slits making 3 lip segments
        sd_box(g, N, (x_neg, yc_ - 9.7, zc), (x_pos, yc_ + 9.7, Hh + 1))
        # contact windows
        sd_cyl(g, N, 0, (yc_, zc), 4.4, x_pos - 0.1, L + 1)           # positive: button recess bore (key) + spring access
        sd_cyl(g, N, 0, (yc_, zc), 7.0, -1.0, x_neg + 0.1)            # negative: spring window
    # lip relief slits
    for xs in (x_neg + 24.0, x_neg + 47.0):
        sd_box(g, N, (xs, 1.2, zc + 3.0), (xs + 1.6, W - 1.2, Hh + 1))
    occ = (F < 0) & ~(N < 0)
    A = g.full()
    # polarity key: neg end wall centre post (blocks a positive button) and raised ring seat; pos wall proud ring is the wall face
    for yc_ in ycs:
        sd_cyl(g, A, 0, (yc_, zc), 4.8, x_neg - 0.2, x_neg + 2.0)
        # ring seat (contact land) - tube r 8.5..9.8 height 2.5
    # side detent bumps
    sd_box(g, A, (L - 22.0, -0.8, 7.0), (L - 16.0, 0.4, 17.0), rad=0.4)
    sd_box(g, A, (L - 22.0, W - 0.4, 7.0), (L - 16.0, W + 0.8, 17.0), rad=0.4)
    occ |= (A < 0) & (F < 6)
    occ = largest_component(occ)
    sd = occ_to_sd(occ, h, smooth=0.5)
    m = sd_to_mesh(g, sd)
    if save: save_stl(m, "battery_drawer")
    return m


def build_elec_cap(save=True):
    p = P; h = 0.3
    y0, y1 = 546.5 + 0.2, 572.0 - 0.2; z1 = -26.0 - 0.2; z0 = -52.0 + 0.2
    g = Grid((786, y0 - 4, z0 - 4), (806, y1 + 4, z1 + 4), h)
    F = g.full()
    sd_box(g, F, (794.0, y0, z0), (803.5, y1, z1), rad=1.0)                         # plug
    sd_box(g, F, (790.5, y0 - 2.5, z0 - 2.5), (794.5, y1 + 2.5, z1 + 2.5), rad=2.0)   # flange (sits on the dome end face)
    occ = F < 0
    N = g.full()
    sd_cyl(g, N, 0, (553.0, -34.0), 3.2, 780, 810)      # 3.5 mm panel jack (BLLNDX, M6 thread [G])
    sd_cyl(g, N, 0, (566.0, -34.0), 3.1, 780, 810)      # Twidec mini toggle (6 mm bushing [G])
    sd_box(g, N, (780.0, 559.5 - 4.7, -45.0 - 1.7), (810.0, 559.5 + 4.7, -45.0 + 1.7), rad=1.6)   # USB-C slot 9.4 x 3.4 (Seed 3)
    sd_box(g, N, (794.0, 546.0, -30.0), (803.6, 549.0, -29.0))       # wire slot
    occ &= ~(N < 0)
    A = g.full()
    for zz in (-52.0 + 5, -26.0 - 5):     # snap bumps matching the bay notches
        sd_box(g, A, (797.5, y0 - 0.5, zz - 1.8), (800.9, y0 + 0.2, zz + 1.8)); sd_box(g, A, (797.5, y1 - 0.2, zz - 1.8), (800.9, y1 + 0.5, zz + 1.8))
    occ |= (A < 0)
    occ = largest_component(occ)
    sd = occ_to_sd(occ, h, smooth=0.5)
    m = sd_to_mesh(g, sd)
    if save: save_stl(m, "elec_bay_cap")
    return m



# ===================================================================== SPIRAL ===
def smoothstep(x):
    x = np.clip(x, 0, 1); return x * x * (3 - 2 * x)


def u_profile(W, iw, g0, base, depth, c=1.0, gd=0.9, groove=True):
    top = g0 + base + depth
    pts = [(-W / 2 + c, g0), (W / 2 - c, g0), (W / 2, g0 + c), (W / 2, top - c), (W / 2 - c, top), (iw / 2 + 0.3, top)]
    if groove:
        pts += [(iw / 2, top - 0.3), (iw / 2, top - 1.0), (iw / 2 + gd, top - 1.0), (iw / 2 + gd, top - 2.0), (iw / 2, top - 2.0)]
    else:
        pts += [(iw / 2, top - 0.3), (iw / 2, top - 1.0), (iw / 2 + 0.01, top - 1.0), (iw / 2 + 0.01, top - 2.0), (iw / 2, top - 2.0)]
    pts += [(iw / 2, g0 + base), (-iw / 2, g0 + base)]
    if groove:
        pts += [(-iw / 2, top - 2.0), (-iw / 2 - gd, top - 2.0), (-iw / 2 - gd, top - 1.0), (-iw / 2, top - 1.0), (-iw / 2, top - 0.3)]
    else:
        pts += [(-iw / 2, top - 2.0), (-iw / 2 - 0.01, top - 2.0), (-iw / 2 - 0.01, top - 1.0), (-iw / 2, top - 1.0), (-iw / 2, top - 0.3)]
    pts += [(-iw / 2 - 0.3, top), (-W / 2 + c, top), (-W / 2, top - c), (-W / 2, g0 + c)]
    return np.array(pts, float)


def solid_profile(W, Hh, g0, c=1.8):
    top = g0 + Hh
    return np.array([(-W / 2 + c, g0), (W / 2 - c, g0), (W / 2, g0 + c), (W / 2, top - c), (W / 2 - c, top), (-W / 2 + c, top), (-W / 2, top - c), (-W / 2, g0 + c)], float)


def sweep_profiles(rings, closed_caps=True):
    """rings: list of (n_pts,3) arrays (same n), consecutive rings connected; returns watertight Trimesh."""
    R = np.asarray(rings); nR, n, _ = R.shape
    V = R.reshape(-1, 3)
    F = []
    for i in range(nR - 1):
        for j in range(n):
            a = i * n + j; b = i * n + (j + 1) % n; c = (i + 1) * n + (j + 1) % n; d = (i + 1) * n + j
            F += [(a, b, c), (a, c, d)]
    # caps: fan via ear clipping on first/last ring using the 2D profile (triangulate in local plane)
    from shapely.geometry import Polygon
    import mapbox_earcut as ec
    def cap(ring, flip):
        c0 = ring.mean(0); u = ring[1] - ring[0]; u /= np.linalg.norm(u)
        nrm = np.cross(ring[1] - ring[0], ring[2] - ring[1]); nrm = nrm / (np.linalg.norm(nrm) + 1e-12)
        w = np.cross(nrm, u)
        uv = np.c_[(ring - c0) @ u, (ring - c0) @ w]
        tri = ec.triangulate_float64(uv, np.array([len(uv)], np.uint32)).reshape(-1, 3)
        return tri
    t0 = cap(R[0], False); t1 = cap(R[-1], True)
    base1 = (nR - 1) * n
    F += [tuple(t[::-1]) for t in t0] + [tuple(base1 + t) for t in t1]
    m = trimesh.Trimesh(V, np.array(F), process=True)
    trimesh.repair.fix_normals(m)
    if m.volume < 0: m.invert()
    return m


def helix_frames(s_arr, theta_arr):
    yb, zb = P["bell_axis"]; xr = P["bell_rim_x"]
    r = _BELL(s_arr).astype(float); drds = _BELLD(s_arr).astype(float)
    th = theta_arr
    ex = np.array([1.0, 0, 0]); er = np.c_[np.zeros_like(th), np.cos(th), np.sin(th)]
    C0 = np.c_[xr - s_arr, yb + r * np.cos(th), zb + r * np.sin(th)]
    T = -ex[None, :] + drds[:, None] * er; T /= np.linalg.norm(T, axis=1)[:, None]
    N = -drds[:, None] * (-ex[None, :]) * 0 + (drds[:, None] * ex[None, :] * -1 * -1 + er)   # placeholder, normalised below
    N = er + drds[:, None] * ex[None, :]
    # make N perpendicular to T in the meridian plane
    N = N - (N * T).sum(1)[:, None] * T; N /= np.linalg.norm(N, axis=1)[:, None]
    return C0, T, N, r


def spiral_theta(s_arr, p=None):
    p = p or P
    return np.radians(p["sp_end_theta_deg"]) + 2 * np.pi * (s_arr - p["sp_s0"]) / p["sp_pitch"]


def gap_k(theta):
    """0 outside the C-gap, 1 inside (smooth edges)."""
    c = np.radians(P["sp_gap_center_deg"]); half = np.radians(P["sp_gap_deg"] / 2); tr = np.radians(14.0)
    d = np.abs((theta - c + np.pi) % (2 * np.pi) - np.pi)
    return 1 - smoothstep((d - (half - tr)) / (2 * tr))


def build_spiral_mesh(s_start, s_end, n_per_turn=180, with_arm=False):
    p = P
    n = int(abs(s_end - s_start) / p["sp_pitch"] * n_per_turn) + 2
    s_arr = np.linspace(s_start, s_end, n)
    th = spiral_theta(s_arr)
    C0, T, N, r = helix_frames(s_arr, th)
    k = gap_k(th)
    rings = []
    for i in range(n):
        base = p["sp_base"] + (1.8 - p["sp_base"]) * k[i]
        depth = p["sp_depth"] + (0.6 - p["sp_depth"]) * k[i]
        prof = u_profile(p["sp_w"], p["sp_inner_w"], p["sp_pad_gap"], base, depth, groove=True)
        rings.append(C0[i] + prof[:, 0:1] * T[i] + prof[:, 1:2] * N[i])
    return sweep_profiles(rings), (s_arr, th, C0, T, N, r)


def build_diffuser(save=True):
    p = P
    s_start, s_end = p["sp_s1"] - 2.0, p["sp_s0"] + 4.0
    n = int(abs(s_end - s_start) / p["sp_pitch"] * 180) + 2
    s_arr = np.linspace(s_start, s_end, n); th = spiral_theta(s_arr); C0, T, N, r = helix_frames(s_arr, th)
    k = gap_k(th); w = p["sp_inner_w"] + 1.6
    rings = []
    for i in range(n):
        base = p["sp_base"] + (1.8 - p["sp_base"]) * k[i]; depth = p["sp_depth"] + (0.6 - p["sp_depth"]) * k[i]
        top = p["sp_pad_gap"] + base + depth
        v0 = (top - 1.95) * (1 - k[i]) + (top + 0.35) * k[i]; v1 = v0 + p["diffuser_t"]
        sw = smoothstep(k[i] / 0.04); ww = w * (1 - sw) + (p["sp_inner_w"] - 0.5) * sw
        prof = np.array([(-ww / 2 + 0.3, v0), (ww / 2 - 0.3, v0), (ww / 2, v0 + 0.3), (ww / 2, v1 - 0.3), (ww / 2 - 0.3, v1), (-ww / 2 + 0.3, v1), (-ww / 2, v1 - 0.3), (-ww / 2, v0 + 0.3)])
        rings.append(C0[i] + prof[:, 0:1] * T[i] + prof[:, 1:2] * N[i])
    m = sweep_profiles(rings)
    if save: save_stl(m, "spiral_diffuser", offset=BELL_OFF)
    return m


def mic_arm_path(s_arr, th, C0, T, N):
    """cubic Bezier from the helix end to the socket base, aimed into the bell."""
    p = P; yb, zb = p["bell_axis"]; xr = p["bell_rim_x"]
    P0 = C0[-1] + N[-1] * (p["sp_pad_gap"] + 11.0)
    thend = th[-1]; er = np.array([0, np.cos(thend), np.sin(thend)])
    S = np.array([xr + 14.0, yb, zb]) + er * (_bell_r(0) + 22.0)
    d0 = T[-1] * -1.0          # continue the helix direction (toward the rim)
    Tt = np.cross([1.0, 0, 0], er)   # circumferential
    dend = np.array([-math.sin(math.radians(12)), 0, 0]) + (-er) * math.cos(math.radians(12))   # points into the bell, tilted 12 deg back
    # start tangent: toward +x (rim) and radially outward
    t0 = np.array([1.0, 0, 0]) * 0.25 + er * 0.97
    A = P0; Bz = P0 + t0 * 34.0; Cc = S - dend * -1 * 0 + er * 0 - np.array([0, 0, 0])
    Cc = S + (-dend) * 22.0      # approach from outside (opposite to the aiming direction)
    ts = np.linspace(0, 1, 90)
    pts = [(1 - t) ** 3 * A + 3 * (1 - t) ** 2 * t * Bz + 3 * (1 - t) * t ** 2 * Cc + t ** 3 * S for t in ts]
    return np.array(pts), S, dend


def build_spiral(save=True):
    p = P
    # helix from the throat end (s1) to the rim end (s0); the arm continues from s0
    m, (s_arr, th, C0, T, N, r) = build_spiral_mesh(p["sp_s1"], p["sp_s0"])
    path, S, dend = mic_arm_path(s_arr, th, C0, T, N)
    # arm sweep: solid rounded profile oriented by (side, up) frames along the path
    rings = []
    tang = np.gradient(path, axis=0); tang /= np.linalg.norm(tang, axis=1)[:, None]
    ref = np.array([1.0, 0, 0]); thend = th[-1]; side0 = np.cross([1.0, 0, 0], [0, np.cos(thend), np.sin(thend)]); side0 /= np.linalg.norm(side0)
    side = side0
    prof = solid_profile(p["mic_arm_w"], p["mic_arm_h"], -p["mic_arm_h"] / 2)
    for i in range(len(path)):
        up = np.cross(tang[i], side); up /= np.linalg.norm(up)
        side = np.cross(up, tang[i]); side /= np.linalg.norm(side)
        rings.append(path[i] + prof[:, 0:1] * side + prof[:, 1:2] * up)
    arm = sweep_profiles(rings)
    ang = dend / np.linalg.norm(dend)
    boss_len = 18.0
    T_al = trimesh.geometry.align_vectors([0, 0, 1], ang)
    boss = trimesh.creation.cylinder(radius=12.43, height=boss_len, sections=71)
    boss.apply_transform(T_al); boss.apply_translation(S + ang * (boss_len / 2 - 4.0))
    pocket = trimesh.creation.cylinder(radius=10.2, height=p["mic_socket_depth"] + 4.0, sections=61)       # round socket, plug turns 360 deg
    pocket.apply_transform(T_al); pocket.apply_translation(S + ang * (boss_len - 4.0 - p["mic_socket_depth"] / 2 + 2.0))
    groove = trimesh.creation.cylinder(radius=10.8, height=2.0, sections=59)                                # click groove for the plug rib
    groove.apply_transform(T_al); groove.apply_translation(S + ang * (boss_len - 4.0 - 8.0))
    pocket = from_manifold(to_manifold(pocket) + to_manifold(groove))
    for q in (m, arm, boss, pocket): q.apply_translation(-BELL_OFF)
    Mh = to_manifold(m); Ma = to_manifold(arm); Mb = to_manifold(boss); Mp = to_manifold(pocket)
    U = Mh + Ma + Mb
    U = U - Mp
    res = from_manifold(U)
    res.apply_translation(BELL_OFF)
    if save:
        save_stl(res, "spiral_ribbon_mic_arm", offset=BELL_OFF)
        json.dump(dict(S=[float(x) for x in S], aim=[float(x) for x in ang], boss_len=boss_len), open(os.path.join(HERE, "spiral_info.json"), "w"))
    return res, dict(S=S, aim=ang)


def build_mic_inserts(save=True):
    """round plugs for the round socket: tilt 0/10/20/30 deg about local y (rotate the plug in the socket to choose the direction)."""
    p = P; out = []
    for a in p["mic_insert_angles"]:
        plug = trimesh.creation.cylinder(radius=10.0, height=p["mic_socket_depth"], sections=60); plug.apply_translation([0, 0, -p["mic_socket_depth"] / 2])
        rib = trimesh.creation.cylinder(radius=10.55, height=1.6, sections=58); rib.apply_translation([0, 0, -8.0])
        boss = trimesh.creation.cylinder(radius=11.0, height=15.0, sections=61)
        boss.apply_transform(trimesh.transformations.rotation_matrix(math.radians(a), [0, 1, 0]))
        boss.apply_translation([math.sin(math.radians(a)) * 7.0, 0, math.cos(math.radians(a)) * 7.0])
        hole = trimesh.creation.cylinder(radius=p["mic_thread_hole"] / 2, height=30.0, sections=48)
        hole.apply_transform(trimesh.transformations.rotation_matrix(math.radians(a), [0, 1, 0]))
        hole.apply_translation([math.sin(math.radians(a)) * 12.0, 0, math.cos(math.radians(a)) * 12.0])
        U = to_manifold(plug) + to_manifold(rib) + to_manifold(boss)
        U = U - to_manifold(hole)
        m = from_manifold(U)
        if save: save_stl(m, f"mic_angle_insert_{a:02d}deg")
        out.append(m)
    return out


# ================================================== HALL RAIL / CAPS / COUPONS ===
def build_hall_rail(save=True):
    p = P; h = 0.3
    g = Grid((818, 498, 106), (892, 528, 124), h)
    F = g.full(); cy = p["casing_y"]
    z0, z1 = p["hall_rail_z"][0], p["hall_rail_z"][0] + 2.0
    sd_box(g, F, (p["casing_cx"][0] - 10.5, cy - 9.0, z0), (p["casing_cx"][2] + 10.5, cy + 13.0, z1), rad=1.2)      # bar
    for cx in p["casing_cx"]:      # C-clip collars around the caps, open to +y
        sd_cyl(g, F, 2, (cx, cy), p["cap_od"] / 2 + 0.4 + 2.0, z0 - 6.5, z0 + 0.5)
    occ = F < 0
    N = g.full()
    for cx in p["casing_cx"]:
        sd_cyl(g, N, 2, (cx, cy), p["cap_od"] / 2 + 0.4, z0 - 8, z0 - 0.0)                 # cap seat (0.4 radial clearance)
        sd_box(g, N, (cx - 8.0, cy + 2.0, z0 - 8), (cx + 8.0, cy + 16.0, z0 - 0.0))         # opening toward +y (snap mouth 16 wide < 21.4)
        sd_cyl(g, N, 2, (cx, cy), 3.4, z0 - 8, z1 + 2)                                       # stem clearance (stem 4.0)
        sd_box(g, N, (cx - 2.8, cy + 8.0 - 2.8, z0 - 0.2), (cx + 2.8, cy + 8.0 + 2.8, z1 + 1))   # 49E TO-92 pocket 5.6 sq
    occ &= ~(N < 0)
    occ = largest_component(occ)
    m = sd_to_mesh(g, occ_to_sd(occ, h, smooth=0.5), simplify=1)
    if save: save_stl(m, "hall_rail_49E")
    return m


def build_valve_caps(save=True):
    p = P; h = 0.25; out = []
    g = Grid((-14, -9, -1), (14, 17, 6), h)
    F = g.full(); sd_box(g, F, (-7.5, -5.0, 0.0), (7.5, 14.0, 4.2), rad=2.0)
    N = g.full()
    sd_cyl(g, N, 2, (0.0, 0.0), 2.15, -2, 8)                            # 4.0 mm stem
    sd_box(g, N, (-1.8, -9.0, -2), (1.8, 0.0, 8))                        # side slot (snap-on)
    sd_cyl(g, N, 2, (0.0, 8.0), p["magnet_d"] / 2, -2, p["magnet_t"] + 0.1)   # 10 x 3 magnet pocket from the underside, 0.8 mm cap
    occ = (F < 0) & ~(N < 0)
    m = sd_to_mesh(g, occ_to_sd(largest_component(occ), h, smooth=0.5), simplify=1)
    if save: save_stl(m, "valve_cap_magnet_carrier")
    return m


def build_coupons(save=True):
    p = P; out = []
    for c in (0.2, 0.4, 0.6):
        idm = p["casing_od"] + 2 * c
        ring = trimesh.creation.annulus(r_min=idm / 2, r_max=idm / 2 + 3.2, height=14.0, sections=96)
        ring = from_manifold(to_manifold(ring))
        if save: save_stl(ring, f"coupon_grip_ring_c{int(c*10):02d}")
        out.append(ring)
    seg, _ = build_spiral_mesh(90.0, 58.0)
    if save: save_stl(seg, "coupon_spiral_segment", offset=BELL_OFF)
    out.append(seg)
    return out



# ===================================================================== VERIFY ===
ASSEMBLED = ["mount_arms_plate", "pod_face_panel", "palm_rest_dome", "elec_bay_cap", "hall_rail_49E", "spiral_ribbon_mic_arm", "spiral_diffuser"]


def load(name):
    m = trimesh.load(os.path.join(STL, name + ".stl"), force="mesh")
    f = os.path.join(HERE, "stl_offsets.json")
    if os.path.exists(f):
        d = json.load(open(f))
        if name in d: m.apply_translation(np.asarray(d[name]))
    return m


def box_sd(pts, lo, hi):
    lo = np.asarray(lo); hi = np.asarray(hi); c = (lo + hi) / 2; hs = (hi - lo) / 2
    q = np.abs(pts - c) - hs
    return np.linalg.norm(np.maximum(q, 0), axis=1) + np.minimum(q.max(1), 0)


def ycyl_sd(pts, cx, cz, ra, rb, y0, y1):
    f = (np.sqrt(((pts[:, 0] - cx) / ra) ** 2 + ((pts[:, 2] - cz) / rb) ** 2) - 1) * min(ra, rb)
    return np.maximum(f, np.maximum(y0 - pts[:, 1], pts[:, 1] - y1))


def verify(write=True):
    p = P; rep = {"parts": {}, "bed": p["bed"]}
    files = sorted(f[:-4] for f in os.listdir(STL) if f.endswith(".stl"))
    ko = keepout_items()
    for name in files:
        m = load(name)
        bodies = len(m.split(only_watertight=False))
        d = dict(watertight=bool(m.is_watertight), bodies=bodies, volume_cm3=round(float(m.volume) / 1000, 2) if m.is_watertight else None,
                 bbox_mm=[round(float(x), 1) for x in (m.bounds[1] - m.bounds[0])],
                 fits_bed=bool(np.all(np.sort(m.bounds[1] - m.bounds[0]) <= p["bed"] - 4)))
        if name in ASSEMBLED or name.startswith("tpu_pad"):
            res, kos, pts = clearance_report(m, skip=())
            # intentional pad interference excluded from "violation" logic
            worst = min(res.items(), key=lambda kv: kv[1])
            d["min_clear_to_horn_mm"] = round(worst[1], 2); d["closest_horn_item"] = worst[0]
            d["horn_clear_items"] = {k: round(v, 2) for k, v in sorted(res.items(), key=lambda kv: kv[1])[:6]}
            kk = {}
            for nme, kind, dd in ko:
                if kind == "box": kk[nme] = round(float(box_sd(pts, dd[0], dd[1]).min()), 2)
                elif kind == "ycyl": kk[nme] = round(float(ycyl_sd(pts, *dd).min()), 2)
                else: kk[nme] = round(float((np.linalg.norm(pts - np.asarray(dd[0]), axis=1) - dd[1]).min()), 2)
            d["keepout_signed_dist_mm"] = kk
        if name == "mount_arms_plate" or name.startswith("tpu_pad"):
            gm = Grid((712, 478, -34), (948, 546, 140), 0.5)
            if "Hshell" not in rep: rep["Hshell"] = horn_field(gm)
            pts_ = load(name).sample(60000)
            c_ = ((pts_ - gm.lo) / gm.h).T
            d["min_clear_to_model_tubes_shell_mm"] = round(float(ndi.map_coordinates(rep["Hshell"], c_, order=1).min()), 2)
        rep["parts"][name] = d
    rep.pop("Hshell", None)
    # pairwise intersections of assembled parts
    inter = {}
    ms = {n: load(n) for n in ASSEMBLED}
    names = list(ms)
    for i in range(len(names)):
        for j in range(i + 1, len(names)):
            a, b = ms[names[i]], ms[names[j]]
            if np.any(a.bounds[1] < b.bounds[0]) or np.any(b.bounds[1] < a.bounds[0]): continue
            try:
                I = from_manifold(to_manifold(a) ^ to_manifold(b))
                v = float(I.volume) if len(I.faces) else 0.0
            except Exception:
                v = -1.0
            if v != 0.0: inter[f"{names[i]} x {names[j]}"] = round(v, 2)
    rep["assembly_intersections_mm3"] = inter
    # TPU pads: interference with horn (intended 0.2)
    # panel thickness vs thread lengths [G values flagged]
    face = p["plate_y1"] + p["pod_back"] + p["pod_cav"] + p["pod_panel"]
    rep["panel_checks"] = {
        "panel_thickness_mm": p["pod_panel"], "face_y": face, "cavity_depth_mm": p["pod_cav"],
        "KY-040 encoder M7 bushing 7.0 mm [G]": dict(through_panel=p["pod_panel"], proud_for_nut_knob=round(7.0 - p["pod_panel"], 1), nut_ok_3mm=bool(7.0 - p["pod_panel"] >= 3.0), hole=p["enc_hole"]),
        "B10K 16 mm pot M7 bushing 6-7.5 mm [G]": dict(proud_min=round(6.0 - p["pod_panel"], 1), nut_ok_2p5mm=bool(6.0 - p["pod_panel"] >= 2.5), hole=p["pot_hole"]),
        "KY-023 stick (no thread; cap clears 20 mm hole) [G]": dict(stick_above_pcb=17.0, pcb_to_panel_back=p["pod_cav"] - 1.5, cap_proud_of_face=round(17.0 - (p["pod_cav"] - 1.5) - p["pod_panel"], 1)),
        "uxcell 45 mm fader [G]": dict(slot=p["fader_slot"], handle_through_slot=True, body_height_under_panel_max=p["pod_cav"] - 0.5),
        "NeoTrellis 4x4 [G]": dict(pcb_back_y=round(p["plate_y1"] + p["pod_back"] + 1.1, 1), keys_top_y=round(p["plate_y1"] + p["pod_back"] + 1.1 + 9.8, 1), keys_proud_of_face=round(p["plate_y1"] + p["pod_back"] + 1.1 + 9.8 - face, 1), window=p["trellis_win"]),
    }
    if write: json.dump(rep, open(os.path.join(HERE, "verify_report.json"), "w"), indent=1)
    return rep


def run(steps):
    allsteps = ["mount", "pod", "rest", "drawer", "capsteps", "spiral", "mic", "rail", "caps", "coupons"]
    if "all" in steps: steps = allsteps + ["verify"]
    Pj = dict(P); Pj["bell_prof_dense"] = [[float(x), float(_BELL(x))] for x in np.linspace(0, 230, 116)]
    json.dump(Pj, open(os.path.join(HERE, "params.json"), "w"), indent=1, default=list)
    for st in steps:
        t = time.time()
        if st == "mount": build_mount()
        elif st == "pod": build_pod()
        elif st == "rest": build_rest()
        elif st == "drawer": build_drawer()
        elif st == "capsteps": build_elec_cap()
        elif st == "spiral": build_spiral(); build_diffuser()
        elif st == "mic": build_mic_inserts()
        elif st == "rail": build_hall_rail()
        elif st == "caps": build_valve_caps()
        elif st == "coupons": build_coupons()
        elif st == "verify": verify()
        print(st, "done", round(time.time() - t, 1), "s")


if __name__ == "__main__":
    run(sys.argv[1:] or ["all"])
