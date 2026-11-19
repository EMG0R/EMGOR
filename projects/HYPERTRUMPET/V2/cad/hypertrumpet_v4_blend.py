#!/usr/bin/env python3
"""Assemble hypertrumpet_v4.blend from cad/stl_v4 and render.  Blender -b --factory-startup --python hypertrumpet_v4_blend.py -- out.blend render_dir [shots,comma]
Collections: SHELL_POD, MOUNT_ARMS, BATTERY_DOOR, WRAP_CROWN_A, WRAP_CROWN_B, VALVE_CAPS, REF_HORN."""
import bpy, sys, os, json, math
import numpy as np
from mathutils import Vector, Matrix
HERE = os.path.dirname(os.path.abspath(__file__))
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = argv[0] if argv else os.path.join(HERE, "hypertrumpet_v4.blend"); RDIR = argv[1] if len(argv) > 1 else os.path.join(HERE, "renders_v4"); os.makedirs(RDIR, exist_ok=True)
ONLY = argv[2].split(",") if len(argv) > 2 else None
RES = int(os.environ.get("RES", "1800"))
P = json.load(open(os.path.join(HERE, "params_v4.json"))); STL = os.path.join(HERE, "stl_v4")
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene; sc.unit_settings.system = 'METRIC'; sc.unit_settings.scale_length = 0.001; sc.unit_settings.length_unit = 'MILLIMETERS'
COL = {}
for n in ["SHELL_POD", "MOUNT_ARMS", "BATTERY_DOOR", "VALVE_CAPS", "REF_HORN"]:
    c = bpy.data.collections.new(n); sc.collection.children.link(c); COL[n] = c
def mat(name, rgb, rough=0.45, metal=0.0, emit=None, es=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True; b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*rgb, 1); b.inputs["Roughness"].default_value = rough; b.inputs["Metallic"].default_value = metal
    if emit: b.inputs["Emission Color"].default_value = (*emit, 1); b.inputs["Emission Strength"].default_value = es
    return m
M = dict(brass=mat("brass", (0.8, 0.55, 0.16), 0.3, 1.0), shell=mat("shell", (0.36, 0.13, 0.62), 0.32), wrap=mat("wrap", (0.42, 0.16, 0.72), 0.3), mount=mat("mount", (0.17, 0.08, 0.32), 0.4),
         mic=mat("mic", (0.06, 0.06, 0.07), 0.35, 0.6), rail=mat("rail", (0.95, 0.85, 0.2), 0.4), cell=mat("cells", (0.1, 0.5, 0.9), 0.4), cap=mat("cap", (1.0, 0.25, 0.7), 0.35),
         door=mat("door", (0.62, 0.3, 0.95), 0.35), tpu=mat("tpu", (0.9, 0.9, 0.2), 0.6), strip=mat("strip", (1, 0.9, 1), 0.2, 0, (0.9, 0.5, 1.0), 6.0), sw=mat("switch", (0.85, 0.85, 0.9), 0.3, 1.0), seed=mat("seed", (0.1, 0.6, 0.3), 0.4))
def link(o, coll, m=None):
    for c in list(o.users_collection): c.objects.unlink(o)
    coll.objects.link(o)
    if m is not None: o.data.materials.clear(); o.data.materials.append(m)
    return o
def import_stl(name, coll, m, extra=None):
    before = set(bpy.data.objects); bpy.ops.wm.stl_import(filepath=os.path.join(STL, name + ".stl"))
    o = [x for x in bpy.data.objects if x not in before][0]; o.name = name
    if extra is not None: o.matrix_world = extra @ o.matrix_world
    link(o, coll, m); bpy.context.view_layer.objects.active = o; o.select_set(True)
    try: bpy.ops.object.shade_smooth_by_angle(angle=math.radians(50))
    except Exception: bpy.ops.object.shade_smooth()
    o.select_set(False); return o
import_stl("mount_original_arms_plate_v4", COL["MOUNT_ARMS"], M["mount"]); import_stl("shell_pebble_v4", COL["SHELL_POD"], M["shell"])
import_stl("battery_door_v4", COL["BATTERY_DOOR"], M["door"])
for cx in P["casing_cx"]: import_stl("valve_cap_v4", COL["VALVE_CAPS"], M["cap"], Matrix.Translation((cx, P["casing_y"], P["cap_z0"])))
# reference horn (standard dims)
H = COL["REF_HORN"]
def cyl(name, p0, p1, r, m=M["brass"], seg=96):
    p0 = Vector(p0); p1 = Vector(p1); d = p1 - p0
    bpy.ops.mesh.primitive_cylinder_add(vertices=seg, radius=r, depth=d.length, location=(p0 + p1) / 2)
    o = bpy.context.object; o.rotation_mode = 'QUATERNION'; o.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(d); o.name = name; return link(o, H, m)
def tube(name, pts, r, m=M["brass"]):
    cu = bpy.data.curves.new(name, 'CURVE'); cu.dimensions = '3D'; cu.bevel_depth = r; cu.bevel_resolution = 6; cu.resolution_u = 4
    sp = cu.splines.new('POLY'); sp.points.add(len(pts) - 1)
    for i, q in enumerate(pts): sp.points[i].co = (q[0], q[1], q[2], 1)
    o = bpy.data.objects.new(name, cu); cu.use_fill_caps = True; H.objects.link(o); o.data.materials.append(m); return o
cy = P["casing_y"]
for i, cx in enumerate(P["casing_cx"]):
    cyl(f"casing{i+1}", (cx, cy, P["casing_z0"]), (cx, cy, P["casing_z1"]), P["casing_od"] / 2); cyl(f"cap{i+1}", (cx, cy, P["cap_z0"]), (cx, cy, P["cap_z1"]), P["cap_od"] / 2)
    cyl(f"stem{i+1}", (cx, cy, P["cap_z1"]), (cx, cy, 133.0), P["stem_od"] / 2, seg=24); cyl(f"button{i+1}", (cx, cy, 133.0), (cx, cy, 133.0 + P["btn_h"]), 9.0, seg=96); cyl(f"botcap{i+1}", (cx, cy, P["botcap_z0"]), (cx, cy, P["casing_z0"]), P["botcap_od"] / 2)
rr = P["slide_leg_od"] / 2; zc = 0.5 * sum(P["slide_leg_z"]); R = 0.5 * (P["slide_leg_z"][1] - P["slide_leg_z"][0])
for j, zl in enumerate(P["slide_leg_z"]):
    cyl(f"slide1_leg{j}", (884, P["slide1_y"], zl), (P["slide1_crown_x"], P["slide1_y"], zl), rr); cyl(f"slide3_leg{j}", (P["slide3_crown_x"], P["slide3_y"], zl), (823, P["slide3_y"], zl), rr)
t = np.radians(np.linspace(-90, 90, 24))
tube("slide1_crown", [(P["slide1_crown_x"] + R * math.cos(a), P["slide1_y"], zc + R * math.sin(a)) for a in t], rr); tube("slide3_crown", [(P["slide3_crown_x"] - R * math.cos(a), P["slide3_y"], zc + R * math.sin(a)) for a in t], rr)
r3 = P["ring3"]; tube("ring3_finger_ring", [(r3["cx"] + r3["a"] * math.cos(a), r3["y"], r3["cz"] + r3["b"] * math.sin(a)) for a in np.radians(np.linspace(0, 360, 49))], r3["tube_r"])
sa = P["saddle"]; tube("saddle_thumb_hook", [(sa["cx"] + sa["r"] * math.cos(a), sa["y"], sa["cz"] + sa["r"] * math.sin(a)) for a in np.radians(np.linspace(90, 270, 25))], sa["tube_r"])
br = P["brace"]; cyl("brace", (br["x"], br["y"], br["z0"]), (br["x"], br["y"], br["z1"]), br["r"], seg=16)
lp = P["leadpipe"]; cyl("leadpipe", (lp["x0"], lp["y"], lp["z"]), (lp["x1"], lp["y"], lp["z"]), lp["r"]); cyl("mouthpiece_receiver", (lp["x0"] - 25, lp["y"], lp["z"]), (lp["x0"] + 25, lp["y"], lp["z"]), 11.5)
yb, zb = P["bell_axis"]; xr = P["bell_rim_x"]; ss = np.linspace(0, 230, 80); nseg = 72; verts = []; faces = []
dn = np.array(P["bell_prof_dense"]); rf = lambda s: float(np.interp(s, dn[:, 0], dn[:, 1]))
for s in ss:
    for k in range(nseg): a = 2 * math.pi * k / nseg; verts.append((xr - s, yb + rf(s) * math.cos(a), zb + rf(s) * math.sin(a)))
for i in range(len(ss) - 1):
    for k in range(nseg): a = i * nseg + k; b = i * nseg + (k + 1) % nseg; c = (i + 1) * nseg + (k + 1) % nseg; d_ = (i + 1) * nseg + k; faces.append((a, b, c, d_))
me = bpy.data.meshes.new("bell"); me.from_pydata(verts, [], faces); me.update(); bo = bpy.data.objects.new("bell_standard", me); H.objects.link(bo); bo.data.materials.append(M["brass"])
sm = bo.modifiers.new("solid", 'SOLIDIFY'); sm.thickness = -0.7
for pl in me.polygons: pl.use_smooth = True
w = bpy.data.worlds.new("w"); sc.world = w; w.use_nodes = True
w.node_tree.nodes["Background"].inputs[0].default_value = (0.10, 0.09, 0.14, 1); w.node_tree.nodes["Background"].inputs[1].default_value = 0.9
for nm, loc, e in [("key", (700, 1100, 700), 3.5e6), ("fill", (1300, 150, 600), 2.0e6), ("back", (900, 600, -700), 1.5e6)]:
    l = bpy.data.lights.new(nm, 'AREA'); l.energy = e; l.size = 400; lo = bpy.data.objects.new(nm, l); sc.collection.objects.link(lo); lo.location = loc
    lo.rotation_euler = (Vector((880, 520, 50)) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
try: sc.render.engine = 'BLENDER_EEVEE'
except Exception: sc.render.engine = 'BLENDER_WORKBENCH'
sc.render.resolution_x = RES; sc.render.resolution_y = int(RES * 0.56)
cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam")); sc.collection.objects.link(cam); sc.camera = cam; cam.data.clip_end = 8000
def hide(names, v):
    for n in names:
        for o in COL.get(n, bpy.data.collections.new('_x')).objects: o.hide_render = v
def shot(name, loc, tgt, lens=50, hide_cols=(), ortho=None, labels=None):
    if ONLY and name not in ONLY: return
    for n in list(COL): hide([n], n in hide_cols)
    cam.data.lens = lens; cam.location = loc
    if ortho: cam.data.type = 'ORTHO'; cam.data.ortho_scale = ortho
    else: cam.data.type = 'PERSP'
    cam.rotation_euler = (Vector(tgt) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    sc.render.filepath = os.path.join(RDIR, name); bpy.ops.render.render(write_still=True)
    if labels:
        from bpy_extras.object_utils import world_to_camera_view
        bpy.context.view_layer.update(); out = []
        for txt, p3, off in labels:
            co = world_to_camera_view(sc, cam, Vector(p3)); out.append([txt, co.x * sc.render.resolution_x, (1 - co.y) * sc.render.resolution_y, off])
        json.dump(out, open(os.path.join(RDIR, name + ".labels.json"), "w"))
SHOTS = json.load(open(os.path.join(HERE, "v4_shots.json"))) if os.path.exists(os.path.join(HERE, "v4_shots.json")) else []
if os.environ.get("NO_RENDER") != "1":
    for s in SHOTS: shot(s["name"], s["loc"], s["tgt"], s.get("lens", 50), tuple(s.get("hide", ())), s.get("ortho"), s.get("labels"))
for _o in list(H.objects): bpy.data.objects.remove(_o, do_unlink=True)      # v4.8: reference trumpet removed from the blend
for _c in list(bpy.data.curves): bpy.data.curves.remove(_c)
bpy.data.collections.remove(H)
import os as _os
_tmp = OUT + ".tmp.blend"
bpy.ops.wm.save_as_mainfile(filepath=_tmp, copy=True); _os.replace(_tmp, OUT); print("saved", OUT)
