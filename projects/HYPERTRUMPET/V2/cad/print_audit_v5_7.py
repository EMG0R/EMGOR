"""v5.3 print audit (derived from print_audit_v5.py). Reads cad/stl_v5_7/*.stl, voxelises with the in-house scanline voxeliser (trimesh's fails on the door),
writes oriented STLs to cad/stl_v5_print/ (old ones were archived to cad/_attic/stl_v5_print_v5_6/ first) and the '_v5_3' block of cad/print_audit_v5.json.
Run with the venv python:  python print_audit_v5_3.py"""
import os, sys, json, shutil, numpy as np, trimesh
from scipy import ndimage as ndi
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, os.path.join(HERE, "v5_3_src"))
from vox import voxelize
SRC = os.path.join(HERE, "stl_v5_7"); OUT = os.path.join(HERE, "stl_v5_print"); ATT = os.path.join(HERE, "_attic", "stl_v5_print_v5_6")
PARTS = {"shell_pebble": ("shell_pebble_v5_7.stl", 0.5), "mount_original_arms_plate": ("mount_original_arms_plate_v4.stl", 0.5), "battery_door": ("battery_door_v5_7.stl", 0.25), "valve_cap": ("valve_cap_v4.stl", 0.25)}
WALL_FLOOR = 1.6
def vx(m, h):
    lo = m.bounds[0] - 2 * h; hi = m.bounds[1] + 2 * h
    return voxelize(np.asarray(m.vertices), np.asarray(m.faces), lo, hi, h), lo
def clusters(mask, h, org, min_mm3, top=8):
    lab, n = ndi.label(mask); out = []
    if n == 0: return out
    sizes = ndi.sum(mask, lab, range(1, n + 1)) * h ** 3
    for i in np.argsort(-sizes)[:top]:
        if sizes[i] < min_mm3: break
        idx = np.argwhere(lab == i + 1); lo = org + idx.min(0) * h; hi = org + (idx.max(0) + 1) * h
        out.append(dict(volume_mm3=round(float(sizes[i]), 1), bbox_lo=[round(float(v), 1) for v in lo], bbox_hi=[round(float(v), 1) for v in hi]))
    return out
def thickness(solid, h, floor):
    r = floor / 2.0 - 0.05
    er = ndi.distance_transform_edt(solid) * h >= r; op = ndi.distance_transform_edt(~er) * h < r
    return solid & ~op
def orient(m):
    best = None; rows = []
    for nm, up in (("+z", [0, 0, 1]), ("-z", [0, 0, -1]), ("+y", [0, 1, 0]), ("-y", [0, -1, 0]), ("+x", [1, 0, 0]), ("-x", [-1, 0, 0])):
        T = trimesh.geometry.align_vectors(up, [0, 0, 1]); mm = m.copy(); mm.apply_transform(T)
        n = mm.face_normals; a = mm.area_faces; zc = mm.triangles_center[:, 2]; z0 = mm.bounds[0, 2]
        bed = (n[:, 2] < -0.99) & (zc < z0 + 0.3); oh = (n[:, 2] < -0.766) & ~bed; ext = mm.extents
        rows.append(dict(up=nm, support_area_mm2=round(float(a[oh].sum()), 0), bed_contact_mm2=round(float(a[bed].sum()), 0), extents=[round(float(v), 1) for v in ext], fits_256=bool((ext <= 256).all())))
        score = a[oh].sum() - 0.5 * a[bed].sum()
        if (ext <= 256).all() and (best is None or score < best[0]): best = (score, nm, T)
    return rows, best
if __name__ == "__main__":
    os.makedirs(ATT, exist_ok=True)
    for f in os.listdir(OUT):
        if f.endswith(".stl") and not os.path.exists(os.path.join(ATT, f)): shutil.copy2(os.path.join(OUT, f), os.path.join(ATT, f))
    rep = {}
    for name, (fn, h) in PARTS.items():
        m = trimesh.load(os.path.join(SRC, fn), force="mesh"); r = {}
        r["source"] = "stl_v5_7/" + fn; r["faces"] = int(len(m.faces)); r["watertight"] = bool(m.is_watertight); r["bodies"] = int(len(m.split(only_watertight=False)))
        r["genus"] = float(1 - m.euler_number / 2) if r["bodies"] == 1 else None; r["volume_cm3"] = round(float(m.volume) / 1000, 2); r["extents_mm"] = [round(float(v), 1) for v in m.extents]
        S, org = vx(m, h); thin = thickness(S, h, WALL_FLOOR); r["thin_wall_lt_1.6_clusters"] = clusters(thin, h, org, 1.0 if h >= 0.5 else 0.3)
        r["thin_wall_lt_2.0_total_mm3"] = round(float(thickness(S, h, 2.0).sum()) * h ** 3, 1)
        emp = np.pad(~S, 1, constant_values=True); lab, n = ndi.label(emp); outside = lab[0, 0, 0]
        r["trapped_voids_mm3"] = [round(float((lab == i).sum() * h ** 3), 1) for i in range(1, n + 1) if i != outside and (lab == i).sum() * h ** 3 > 1.0]
        rows, best = orient(m); r["orientations"] = rows; r["chosen_up_axis"] = best[1]
        mm = m.copy(); mm.apply_transform(best[2]); mm.apply_translation(-mm.bounds[0]); r["print_extents_mm"] = [round(float(v), 1) for v in mm.extents]
        mm.export(os.path.join(OUT, name + "_print.stl")); r["export"] = "stl_v5_print/%s_print.stl" % name
        rep[name] = r; print(name, r["faces"], r["watertight"], r["genus"], r["chosen_up_axis"], r["print_extents_mm"], len(r["thin_wall_lt_1.6_clusters"]), "thin clusters", flush=True)
    jp = os.path.join(HERE, "print_audit_v5.json"); J = json.load(open(jp)); J["_v5_7"] = rep; json.dump(J, open(jp, "w"), indent=1)
