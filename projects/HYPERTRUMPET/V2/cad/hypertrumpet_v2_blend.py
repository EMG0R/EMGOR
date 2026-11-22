#!/usr/bin/env python3
"""Assemble hypertrumpet_v2.blend from cad/stl_v2/*.stl + params.json, then render the review views.
Run:  /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup --python hypertrumpet_v2_blend.py -- <out.blend> <render_dir>
Collections: POD_plate, ARMS_mount, PALM_REST, BATTERY_DRAWER, SPIRAL, MIC_ARM, HALL_RAIL, VALVE_CAPS, REF_HORN, COUPONS
REF_HORN is the standard-dimension reference horn built from params.json (not the Sketchfab mesh)."""
import bpy, sys, os, json, math
import numpy as np
from mathutils import Vector, Matrix

HERE = os.path.dirname(os.path.abspath(__file__))
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = argv[0] if argv else os.path.join(HERE, "hypertrumpet_v2.blend")
RDIR = argv[1] if len(argv) > 1 else os.path.join(HERE, "renders_v2")
os.makedirs(RDIR, exist_ok=True)
P = json.load(open(os.path.join(HERE, "params.json")))
OFF = json.load(open(os.path.join(HERE, "stl_offsets.json"))) if os.path.exists(os.path.join(HERE, "stl_offsets.json")) else {}
INFO = json.load(open(os.path.join(HERE, "spiral_info.json")))
STL = os.path.join(HERE, "stl_v2")

bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
sc.unit_settings.system = 'METRIC'; sc.unit_settings.scale_length = 0.001; sc.unit_settings.length_unit = 'MILLIMETERS'
COL = {}
for n in ["POD_plate", "ARMS_mount", "PALM_REST", "BATTERY_DRAWER", "SPIRAL", "MIC_ARM", "HALL_RAIL", "VALVE_CAPS", "REF_HORN", "COUPONS"]:
    c = bpy.data.collections.new(n); sc.collection.children.link(c); COL[n] = c


def mat(name, rgb, rough=0.45, metal=0.0, emit=None, es=0.0, alpha=1.0):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*rgb, 1); b.inputs["Roughness"].default_value = rough; b.inputs["Metallic"].default_value = metal
    if emit:
        b.inputs["Emission Color"].default_value = (*emit, 1); b.inputs["Emission Strength"].default_value = es
    if alpha < 1:
        b.inputs["Alpha"].default_value = alpha
    return m


M = dict(
    brass=mat("brass", (0.80, 0.55, 0.16), 0.30, 1.0), petg=mat("petg_violet", (0.30, 0.11, 0.55), 0.38), petg2=mat("petg_dark", (0.16, 0.07, 0.30), 0.40),
    rest=mat("rest_dome", (0.22, 0.10, 0.42), 0.30), tpu=mat("tpu_pad", (1.0, 0.45, 0.08), 0.6), diffuser=mat("diffuser", (0.95, 0.85, 1.0), 0.2, 0.0, (0.9, 0.5, 1.0), 2.2),
    led=mat("led_strip", (1, 0.4, 1), 0.5, 0.0, (1.0, 0.35, 1.0), 6.0), mic=mat("mic", (0.06, 0.06, 0.07), 0.35, 0.6), rail=mat("rail", (0.95, 0.85, 0.2), 0.4),
    cell=mat("cells", (0.1, 0.5, 0.9), 0.4), cap=mat("magnet_cap", (0.9, 0.9, 0.95), 0.3), cp=mat("coupon", (0.5, 0.5, 0.55), 0.5),
)


def link(o, coll, m=None):
    for c in list(o.users_collection): c.objects.unlink(o)
    coll.objects.link(o)
    if m is not None:
        o.data.materials.clear(); o.data.materials.append(m)
    return o


def import_stl(name, coll, m, extra=None):
    path = os.path.join(STL, name + ".stl")
    before = set(bpy.data.objects)
    bpy.ops.wm.stl_import(filepath=path)
    o = [x for x in bpy.data.objects if x not in before][0]
    o.name = name
    off = OFF.get(name)
    if off: o.location = Vector(off)
    if extra is not None: o.matrix_world = extra @ o.matrix_world
    link(o, coll, m)
    bpy.context.view_layer.objects.active = o
    o.select_set(True)
    try: bpy.ops.object.shade_smooth_by_angle(angle=math.radians(35))
    except Exception: pass
    o.select_set(False)
    return o


# ---------------------------------------------------------------- parts ---------
import_stl("mount_arms_plate", COL["ARMS_mount"], M["petg2"])
for f in sorted(os.listdir(STL)):
    if f.startswith("tpu_pad_"): import_stl(f[:-4], COL["ARMS_mount"], M["tpu"])
import_stl("pod_face_panel", COL["POD_plate"], M["petg"])
import_stl("palm_rest_dome", COL["PALM_REST"], M["rest"])
import_stl("elec_bay_cap", COL["PALM_REST"], M["petg2"])
d = import_stl("battery_drawer", COL["BATTERY_DRAWER"], M["petg"], Matrix.Translation((832.0, 496.8, -51.8)))
# two dummy cells in the drawer so the render shows the keyed 2-cell bay
for yc in (496.8 + 46.4 / 2 - 11.6, 496.8 + 46.4 / 2 + 11.6):
    bpy.ops.mesh.primitive_cylinder_add(radius=10.9, depth=70.6, vertices=48, location=(832.0 + 4.0 + 35.5, yc, -51.8 + 12.0), rotation=(0, math.pi / 2, 0))
    link(bpy.context.object, COL["BATTERY_DRAWER"], M["cell"]).name = "cell_21700_ref"
import_stl("spiral_ribbon_mic_arm", COL["SPIRAL"], M["petg"])
import_stl("spiral_diffuser", COL["SPIRAL"], M["diffuser"])
import_stl("hall_rail_49E", COL["HALL_RAIL"], M["rail"])
for cx in P["casing_cx"]:
    import_stl("valve_cap_magnet_carrier", COL["VALVE_CAPS"], M["cap"], Matrix.Translation((cx, P["casing_y"], P["stem_z1"] - 4.2)))
S = Vector(INFO["S"]); aim = Vector(INFO["aim"]).normalized()
zx = (-Vector((1, 0, 0)) + aim * aim.x).normalized()        # local +x of the insert points toward -x (into the bell) so the tilt tips the mic rearward
yy = aim.cross(zx).normalized()
rotq = Matrix(((zx.x, yy.x, aim.x, 0), (zx.y, yy.y, aim.y, 0), (zx.z, yy.z, aim.z, 0), (0, 0, 0, 1)))
import_stl("mic_angle_insert_20deg", COL["MIC_ARM"], M["petg2"], Matrix.Translation(S + aim * (INFO["boss_len"] - 4.0 + 2.0)) @ rotq)
# generic clip-on dynamic mic proxy (body 22 mm x 90 mm) on the insert axis tilted 20 deg  [placeholder, mic unknown]
mic_axis = (rotq @ Vector((math.sin(math.radians(20)), 0, math.cos(math.radians(20))))).normalized()
bpy.ops.mesh.primitive_cylinder_add(radius=9.0, depth=70.0, vertices=40, location=S + aim * (INFO["boss_len"] - 2.0) + mic_axis * (14.0 + 35.0))
mo = bpy.context.object; mo.rotation_mode = 'QUATERNION'; mo.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(mic_axis)
link(mo, COL["MIC_ARM"], M["mic"]).name = "mic_proxy_generic_dynamic"
x0 = 1300
for f in sorted(os.listdir(STL)):
    if f.startswith("coupon_grip"):
        import_stl(f[:-4], COL["COUPONS"], M["cp"], Matrix.Translation((x0, 400, 0))); x0 += 34
import_stl("coupon_spiral_segment", COL["COUPONS"], M["petg"], Matrix.Translation((1300, 450, 0)) @ Matrix.Translation((-1090, -525, -110 + 55)))

# ---------------------------------------------------------- reference horn ------
H = COL["REF_HORN"]


def cyl(name, p0, p1, r, m=M["brass"], seg=48):
    p0 = Vector(p0); p1 = Vector(p1); d = p1 - p0
    bpy.ops.mesh.primitive_cylinder_add(vertices=seg, radius=r, depth=d.length, location=(p0 + p1) / 2)
    o = bpy.context.object; o.rotation_mode = 'QUATERNION'; o.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(d)
    o.name = name; return link(o, H, m)


def tube(name, pts, r, m=M["brass"]):
    cu = bpy.data.curves.new(name, 'CURVE'); cu.dimensions = '3D'; cu.bevel_depth = r; cu.bevel_resolution = 6; cu.resolution_u = 4
    sp = cu.splines.new('POLY'); sp.points.add(len(pts) - 1)
    for i, q in enumerate(pts): sp.points[i].co = (q[0], q[1], q[2], 1)
    o = bpy.data.objects.new(name, cu); cu.use_fill_caps = True
    H.objects.link(o); o.data.materials.append(m); return o


cy = P["casing_y"]
for i, cx in enumerate(P["casing_cx"]):
    cyl(f"casing{i+1}", (cx, cy, P["casing_z0"]), (cx, cy, P["casing_z1"]), P["casing_od"] / 2)
    cyl(f"cap{i+1}", (cx, cy, P["cap_z0"]), (cx, cy, P["cap_z1"]), P["cap_od"] / 2)
    cyl(f"stem{i+1}", (cx, cy, P["cap_z1"]), (cx, cy, P["stem_z1"]), P["stem_od"] / 2, seg=24)
    cyl(f"button{i+1}", (cx, cy, P["stem_z1"]), (cx, cy, P["button_z1"]), P["button_od"] / 2)
    cyl(f"botcap{i+1}", (cx, cy, P["botcap_z0"]), (cx, cy, P["casing_z0"]), P["botcap_od"] / 2)
rr = P["slide_leg_od"] / 2; zc = 0.5 * sum(P["slide_leg_z"]); R = 0.5 * (P["slide_leg_z"][1] - P["slide_leg_z"][0])
for j, zl in enumerate(P["slide_leg_z"]):
    cyl(f"slide1_leg{j}", (884, P["slide1_y"], zl), (P["slide1_crown_x"], P["slide1_y"], zl), rr)
    cyl(f"slide3_leg{j}", (P["slide3_crown_x"], P["slide3_y"], zl), (823, P["slide3_y"], zl), rr)
t = np.radians(np.linspace(-90, 90, 24))
tube("slide1_crown", [(P["slide1_crown_x"] + R * math.cos(a), P["slide1_y"], zc + R * math.sin(a)) for a in t], rr)
tube("slide3_crown", [(P["slide3_crown_x"] - R * math.cos(a), P["slide3_y"], zc + R * math.sin(a)) for a in t], rr)
r3 = P["ring3"]; tube("ring3_finger_ring", [(r3["cx"] + r3["a"] * math.cos(a), r3["y"], r3["cz"] + r3["b"] * math.sin(a)) for a in np.radians(np.linspace(0, 360, 49))], r3["tube_r"])
sa = P["saddle"]; tube("saddle_thumb_hook", [(sa["cx"] + sa["r"] * math.cos(a), sa["y"], sa["cz"] + sa["r"] * math.sin(a)) for a in np.radians(np.linspace(90, 270, 25))], sa["tube_r"])
br = P["brace"]; cyl("brace", (br["x"], br["y"], br["z0"]), (br["x"], br["y"], br["z1"]), br["r"], seg=16)
lp = P["leadpipe"]; cyl("leadpipe", (lp["x0"], lp["y"], lp["z"]), (lp["x1"], lp["y"], lp["z"]), lp["r"])
cyl("mouthpiece_receiver", (lp["x0"] - 25, lp["y"], lp["z"]), (lp["x0"] + 25, lp["y"], lp["z"]), 11.5)
# bell: surface of revolution through the profile, with wall
yb, zb = P["bell_axis"]; xr = P["bell_rim_x"]
ss = np.linspace(0, 230, 80); nseg = 72; verts = []; faces = []
_dn = np.array(P["bell_prof_dense"]); rf = lambda s: float(np.interp(s, _dn[:, 0], _dn[:, 1]))
for s in ss:
    for k in range(nseg):
        a = 2 * math.pi * k / nseg; verts.append((xr - s, yb + rf(s) * math.cos(a), zb + rf(s) * math.sin(a)))
for i in range(len(ss) - 1):
    for k in range(nseg):
        a = i * nseg + k; b = i * nseg + (k + 1) % nseg; c = (i + 1) * nseg + (k + 1) % nseg; d_ = (i + 1) * nseg + k; faces.append((a, b, c, d_))
me = bpy.data.meshes.new("bell"); me.from_pydata(verts, [], faces); me.update()
bo = bpy.data.objects.new("bell_standard", me); H.objects.link(bo); bo.data.materials.append(M["brass"])
sm = bo.modifiers.new("solid", 'SOLIDIFY'); sm.thickness = -0.7
for p in me.polygons: p.use_smooth = True
# tuning-slide stub (assumed) + front loop so the bell tube reads as connected
tube("bell_tube_to_loop", [(xr - 230, yb, zb), (xr - 262, yb, zb), (xr - 290, yb - 8, zb + 4)], 3.6)

# ------------------------------------------------------------- scene / render ---
w = bpy.data.worlds.new("w"); sc.world = w; w.use_nodes = True
w.node_tree.nodes["Background"].inputs[0].default_value = (0.10, 0.09, 0.14, 1); w.node_tree.nodes["Background"].inputs[1].default_value = 0.9
for nm, loc, e in [("key", (700, 1100, 700), 3.5e6), ("fill", (1300, 150, 600), 2.0e6), ("back", (900, 600, -700), 1.5e6)]:
    l = bpy.data.lights.new(nm, 'AREA'); l.energy = e; l.size = 400
    lo = bpy.data.objects.new(nm, l); sc.collection.objects.link(lo); lo.location = loc
    d = Vector((880, 520, 50)) - Vector(loc); lo.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
try:
    sc.render.engine = 'BLENDER_EEVEE'
except Exception:
    sc.render.engine = 'BLENDER_WORKBENCH'
sc.render.resolution_x = 1800; sc.render.resolution_y = 1000
cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam")); sc.collection.objects.link(cam); sc.camera = cam
cam.data.clip_end = 8000; cam.data.lens = 50


def shot(name, loc, tgt, lens=50):
    cam.data.lens = lens; cam.location = loc
    cam.rotation_euler = (Vector(tgt) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    sc.render.filepath = os.path.join(RDIR, name); bpy.ops.render.render(write_still=True)


SHOTS = [
    ("v2_iso_pod_side.png", (640, 780, 330), (900, 540, 50), 42),
    ("v2_iso_other_side.png", (1200, 180, 330), (900, 515, 40), 38),
    ("v2_top.png", (900, 520, 1700), (900, 520, 40), 70),
    ("v2_left_hand_view.png", (610, 690, 190), (885, 548, 45), 34),
    ("v2_bell_mic_closeup.png", (1330, 700, 300), (1120, 525, 125), 55),
    ("v2_underside_palm_rest.png", (790, 440, -280), (860, 535, -30), 45),
]
if os.environ.get("NO_RENDER") != "1":
    for s in SHOTS: shot(*s)
bpy.ops.wm.save_as_mainfile(filepath=OUT)
print("saved", OUT)
