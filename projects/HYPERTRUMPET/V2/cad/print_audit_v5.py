"""v5 print audit: thickness/gap maps, orientation + overhang analysis, trapped voids, bed fit, exports oriented STLs to stl_v5_print/. Run with the venv python."""
import os, sys, json, math, numpy as np, trimesh
from scipy import ndimage as ndi
HERE = os.path.dirname(os.path.abspath(__file__)); OUT = os.path.join(HERE, "stl_v5_print"); os.makedirs(OUT, exist_ok=True)
PARTS = {"shell_pebble": 0.5, "mount_original_arms_plate": 0.5, "battery_door": 0.25, "valve_cap": 0.25}
WALL_FLOOR = 1.6

def vox(m, pitch):
    v = m.voxelized(pitch).fill(); return v.matrix.astype(bool), v.transform[:3, 3]

def clusters(mask, h, org, min_mm3, top=8):
    lab, n = ndi.label(mask); out = []
    if n == 0: return out
    sizes = ndi.sum(mask, lab, range(1, n + 1)) * h ** 3
    for i in np.argsort(-sizes)[:top]:
        if sizes[i] < min_mm3: break
        idx = np.argwhere(lab == i + 1); lo = org + idx.min(0) * h; hi = org + (idx.max(0) + 1) * h
        out.append(dict(volume_mm3=round(float(sizes[i]), 1), bbox_lo=[round(float(v), 1) for v in lo], bbox_hi=[round(float(v), 1) for v in hi]))
    return out

def thickness(solid, h, org, floor):
    r = floor / 2.0 - 0.05
    er = ndi.distance_transform_edt(solid) * h >= r; op = ndi.distance_transform_edt(~er) * h < r
    thin = solid & ~op
    return thin

def gaps(solid, h, gap):
    pad = np.pad(solid, 2); emp = ~pad; r = gap / 2.0 - 0.05
    er = ndi.distance_transform_edt(emp) * h >= r; op = ndi.distance_transform_edt(~er) * h < r
    return (emp & ~op)[2:-2, 2:-2, 2:-2]

def orient(m):
    best = None; rows = []
    for nm, up in (("+z", [0, 0, 1]), ("-z", [0, 0, -1]), ("+y", [0, 1, 0]), ("-y", [0, -1, 0]), ("+x", [1, 0, 0]), ("-x", [-1, 0, 0])):
        T = trimesh.geometry.align_vectors(up, [0, 0, 1]); mm = m.copy(); mm.apply_transform(T)
        n = mm.face_normals; a = mm.area_faces; zc = mm.triangles_center[:, 2]; z0 = mm.bounds[0, 2]
        bed = (n[:, 2] < -0.99) & (zc < z0 + 0.3); oh = (n[:, 2] < -0.766) & ~bed
        ext = mm.extents
        rows.append(dict(up=nm, support_area_mm2=round(float(a[oh].sum()), 0), bed_contact_mm2=round(float(a[bed].sum()), 0), extents=[round(float(v), 1) for v in ext], fits_256=bool((ext <= 256).all())))
        score = a[oh].sum() - 0.5 * a[bed].sum()
        if (ext <= 256).all() and (best is None or score < best[0]): best = (score, nm, T)
    return rows, best

def bridges(mm):
    n = mm.face_normals; zc = mm.triangles_center[:, 2]; z0 = mm.bounds[0, 2]
    f = np.nonzero((n[:, 2] < -0.99) & (zc > z0 + 0.3))[0]
    if len(f) == 0: return 0.0
    sub = mm.submesh([f], append=True); parts = sub.split(only_watertight=False)
    return round(max(float(max(p.extents[:2])) for p in parts), 1)

sel = sys.argv[1:] or list(PARTS)
rep = json.load(open(os.path.join(HERE, 'print_audit_v5.json'))) if sys.argv[1:] else {}
for name, pitch in PARTS.items():
    if name not in sel: continue
    m = trimesh.load(os.path.join(HERE, "stl_v4", name + "_v4.stl"), force="mesh"); r = {}
    r["faces"] = int(len(m.faces)); r["watertight"] = bool(m.is_watertight); r["winding_consistent"] = bool(m.is_winding_consistent); r["bodies"] = int(len(m.split(only_watertight=False)))
    r["volume_cm3"] = round(float(m.volume) / 1000, 2); r["extents_mm"] = [round(float(v), 1) for v in m.extents]
    solid, org = vox(m, pitch)
    thin = thickness(solid, pitch, org, WALL_FLOOR); r["thin_wall_lt_1.6_clusters"] = clusters(thin, pitch, org, 1.0 if pitch >= 0.5 else 0.3)
    thin20 = thickness(solid, pitch, org, 2.0); r["thin_wall_lt_2.0_total_mm3"] = round(float(thin20.sum()) * pitch ** 3, 1)
    gp = gaps(solid, pitch, 0.8); r["gap_lt_0.8_clusters"] = clusters(gp, pitch, org, 2.0 if pitch >= 0.5 else 0.5, top=6)
    emp = np.pad(~solid, 1, constant_values=True); lab, n = ndi.label(emp); outside = lab[0, 0, 0]
    voids = [(i, (lab == i).sum() * pitch ** 3) for i in range(1, n + 1) if i != outside]; r["trapped_voids"] = [round(float(v), 1) for _, v in voids if v > 1.0]
    rows, best = orient(m); r["orientations"] = rows; r["chosen_up_axis"] = best[1]
    mm = m.copy(); mm.apply_transform(best[2]); mm.apply_translation(-mm.bounds[0]); r["bridge_span_max_mm"] = bridges(mm); r["print_extents_mm"] = [round(float(v), 1) for v in mm.extents]
    mm.export(os.path.join(OUT, name + "_print.stl")); r["export"] = "stl_v5_print/%s_print.stl" % name
    rep[name] = r; print(name, r["watertight"], r["chosen_up_axis"], r["print_extents_mm"], len(r["thin_wall_lt_1.6_clusters"]), "thin clusters;", len(r["gap_lt_0.8_clusters"]), "gap clusters", flush=True)
json.dump(rep, open(os.path.join(HERE, "print_audit_v5.json"), "w"), indent=1)
