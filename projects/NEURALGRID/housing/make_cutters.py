"""neuralGrid housing - regenerate the boolean cutter set.
Run inside Blender (Scripting tab -> Run Script). Rebuilds NG_CUTTERS from scratch.
Edit P below and re-run. Units: millimetres. Author: emgor.
"""
import bpy, bmesh, math

P = dict(
    pad_open     = 12.6,   # square hole per silicone button (12.0 button + 0.3/side)
    pad_pitch    = 15.0,   # NeoTrellis button pitch
    pad_radius   = 0.8,    # corner fillet on square holes
    slot_len     = 13.0,   # softpot tail slot, long dimension (radial)
    slot_wid     = 6.0,    # softpot tail slot, short dimension (along ribbon axis)
    slot_radius  = 1.0,
    span_total   = 113.0,  # far edge of slot -> far edge of softpot body, centred on the edge
    softpot_ring = 70.0,   # centre -> ribbon centreline
    oval_x       = 7.0,    # lidar aperture
    oval_y       = 10.0,
    lidar_ring   = 67.0,   # centre -> lidar aperture centre, both axes (corners)
    cut_depth    = 12.0,   # cutter Z extent (-6..+6); clears a 4 mm plate with 4 mm spare each side
)

# Slot is rotated 90 deg from its edge: the 13 mm runs radially (perpendicular to
# the edge), so only slot_wid is consumed along the ribbon axis.
SLOT_AXIAL   = P['slot_wid']
HOLE_OFFSET  = P['span_total'] / 2.0 - SLOT_AXIAL / 2.0   # 53.5 mm

sc = bpy.context.scene
sc.unit_settings.system = 'METRIC'
sc.unit_settings.scale_length = 0.001
sc.unit_settings.length_unit = 'MILLIMETERS'

for cn in ("NG_CUTTERS", "CUT_pads", "CUT_softpot_slots", "CUT_lidar_ovals"):
    c = bpy.data.collections.get(cn)
    if c:
        for o in list(c.objects):
            bpy.data.objects.remove(o, do_unlink=True)
        bpy.data.collections.remove(c)

root = bpy.data.collections.new("NG_CUTTERS")
sc.collection.children.link(root)
cols = {}
for cn in ("CUT_pads", "CUT_softpot_slots", "CUT_lidar_ovals"):
    c = bpy.data.collections.new(cn)
    root.children.link(c)
    cols[cn] = c

S = 0.001  # mm -> blender units


def _rr_profile(w, h, r, seg=8):
    r = min(r, w / 2 - 1e-6, h / 2 - 1e-6)
    hx, hy = w / 2 - r, h / 2 - r
    pts = []
    for cx, cy, a0 in ((hx, hy, 0.0), (-hx, hy, math.pi / 2),
                       (-hx, -hy, math.pi), (hx, -hy, 3 * math.pi / 2)):
        for k in range(seg + 1):
            a = a0 + (math.pi / 2) * k / seg
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    out = []
    for p in pts:
        if not out or (abs(p[0] - out[-1][0]) > 1e-9 or abs(p[1] - out[-1][1]) > 1e-9):
            out.append(p)
    if abs(out[0][0] - out[-1][0]) < 1e-9 and abs(out[0][1] - out[-1][1]) < 1e-9:
        out.pop()
    return out


def _ellipse_profile(w, h, seg=48):
    return [((w / 2) * math.cos(2 * math.pi * k / seg),
             (h / 2) * math.sin(2 * math.pi * k / seg)) for k in range(seg)]


def make_prism(name, profile, depth, loc, rot_z, col):
    bm = bmesh.new()
    z0, z1 = -depth / 2 * S, depth / 2 * S
    bot = [bm.verts.new((x * S, y * S, z0)) for x, y in profile]
    top = [bm.verts.new((x * S, y * S, z1)) for x, y in profile]
    bm.verts.ensure_lookup_table()
    n = len(profile)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((bot[i], bot[j], top[j], top[i]))
    bm.faces.new(list(reversed(bot)))
    bm.faces.new(top)
    bm.normal_update()
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    ob.location = (loc[0] * S, loc[1] * S, 0.0)
    ob.rotation_euler = (0.0, 0.0, math.radians(rot_z))
    col.objects.link(ob)
    return ob


# --- 64 pad squares. y = 0 is the BACK row: index map i -> x = i % 8, y = i // 8.
sq = _rr_profile(P['pad_open'], P['pad_open'], P['pad_radius'])
for gy in range(8):
    for gx in range(8):
        make_prism("CUT_pad_x%d_y%d" % (gx, gy), sq, P['cut_depth'],
                   ((gx - 3.5) * P['pad_pitch'], (3.5 - gy) * P['pad_pitch']),
                   0.0, cols['CUT_pads'])

# --- 4 softpot tail slots. C4 rotational symmetry: rotating the whole set 90 deg
# maps each slot onto the next. Parallel pairs therefore sit on OPPOSITE ends
# (N at +H, S at -H; W at +H, E at -H). Base profile runs 13 mm along X.
slot = _rr_profile(P['slot_len'], P['slot_wid'], P['slot_radius'])
Rs, H = P['softpot_ring'], HOLE_OFFSET
for nm, loc, rz in (("N", ( H,  Rs), 90.0),
                    ("W", (-Rs,  H),  0.0),
                    ("S", (-H, -Rs), 90.0),
                    ("E", ( Rs, -H),  0.0)):
    make_prism("CUT_softpot_%s" % nm, slot, P['cut_depth'], loc, rz,
               cols['CUT_softpot_slots'])

# --- 4 lidar ovals at the corners, alternating 0/90 around the ring. That
# alternation IS the C4 symmetry: a 90 deg rotation maps each oval onto the next.
ov = _ellipse_profile(P['oval_x'], P['oval_y'])
Rl = P['lidar_ring']
for nm, loc, rz in (("NE", ( Rl,  Rl),  0.0),
                    ("NW", (-Rl,  Rl), 90.0),
                    ("SW", (-Rl, -Rl),  0.0),
                    ("SE", ( Rl, -Rl), 90.0)):
    make_prism("CUT_lidar_%s" % nm, ov, P['cut_depth'], loc, rz,
               cols['CUT_lidar_ovals'])

e = bpy.data.objects.get("NG_CUT_PARAMS")
if e:
    bpy.data.objects.remove(e, do_unlink=True)
e = bpy.data.objects.new("NG_CUT_PARAMS", None)
e.empty_display_type = 'PLAIN_AXES'
e.empty_display_size = 0.005
for k, v in P.items():
    e[k] = v
e['_hole_offset_derived'] = HOLE_OFFSET
root.objects.link(e)

print("cutters: %d pads, %d slots, %d ovals | hole offset %.2f mm"
      % (len(cols['CUT_pads'].objects), len(cols['CUT_softpot_slots'].objects),
         len(cols['CUT_lidar_ovals'].objects), HOLE_OFFSET))
