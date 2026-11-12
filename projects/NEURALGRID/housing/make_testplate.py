"""neuralGrid housing - fit test plate.

One fast print that answers every "will the real part go in" question before
any 300 cm3 shell is committed. Every hole appears at four clearances; you fit
the actual part, see which one is right, and that number goes into the shell.

Plate is 4.0 mm - the same as the shell wall - so hole depth is representative.
Units: millimetres. Author: emgor.
"""
import bpy, bmesh, math

S = 0.001
T = dict(
    plate_x=194.0, plate_y=92.0, plate_t=4.0, plate_r=6.0,
    # ONE ladder of round holes covers both big connectors: the Ethernet's
    # nominal cutout is 22.0 and the USB's is 22.55, so 22.0 -> 23.2 in 0.2
    # steps brackets them both. Fewer holes, and no chance of confusing two
    # near-identical rows.
    big_lo=22.0, big_step=0.2, big_n=7, big_y=28.0, big_pitch=27.0,
    pwr_dia=15.6, pwr_clears=(0.0, 0.2, 0.4, 0.6),
    pwr_y=0.0, pwr_pitch=22.0,
    pwr_key_dia=16.0, pwr_key_flat=15.0, pwr_key_x=57.0,
    pad_open=12.6, pad_pitch=15.0, pad_r=2.0,  pad_y=-27.0,
    slot_l=13.0,   slot_w=6.0,     slot_r=2.5, slot_x=(-57.0, -80.0),
    boss_dia=14.0, boss_h=12.0,    boss_y=-27.0, boss_x=80.0,
    insert_pilot=5.6, marker_dia=2.2, marker_pitch=3.6,
)

sc = bpy.context.scene
sc.unit_settings.system = 'METRIC'
sc.unit_settings.scale_length = 0.001
sc.unit_settings.length_unit = 'MILLIMETERS'

col = bpy.data.collections.get("NG_TESTPLATE")
if col:
    for o in list(col.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    bpy.data.collections.remove(col)
col = bpy.data.collections.new("NG_TESTPLATE")
sc.collection.children.link(col)


def rrect(w, h, r, seg=8):
    r = min(r, w / 2 - 1e-6, h / 2 - 1e-6)
    hx, hy = w / 2 - r, h / 2 - r
    pts = []
    for cx, cy, a0 in ((hx, hy, 0.0), (-hx, hy, math.pi / 2),
                       (-hx, -hy, math.pi), (hx, -hy, 3 * math.pi / 2)):
        for k in range(seg + 1):
            a = a0 + (math.pi / 2) * k / seg
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def ellipse(w, h, seg=48):
    return [((w / 2) * math.cos(2 * math.pi * k / seg),
             (h / 2) * math.sin(2 * math.pi * k / seg)) for k in range(seg)]


def dhole(dia, flat_across, seg=64):
    R = dia / 2.0
    d = max(min(flat_across - R, R - 1e-4), -R + 1e-4)
    a = math.asin(d / R)
    t0, t1 = math.pi - a, 2 * math.pi + a
    return [(R * math.cos(t0 + (t1 - t0) * k / seg),
             R * math.sin(t0 + (t1 - t0) * k / seg)) for k in range(seg + 1)]


def loft(name, rings, c):
    bm = bmesh.new()
    layers = [[bm.verts.new((x * S, y * S, z * S)) for x, y in prof] for z, prof in rings]
    bm.verts.ensure_lookup_table()
    n = len(rings[0][1])
    for k in range(len(layers) - 1):
        A, B = layers[k], layers[k + 1]
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((A[i], A[j], B[j], B[i]))
    bm.faces.new(layers[0]); bm.faces.new(layers[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new(name, me); c.objects.link(ob)
    return ob


def prism(name, prof, z0, z1, loc, c):
    a = [(x + loc[0], y + loc[1]) for x, y in prof]
    return loft(name, [(z0, a), (z1, a)], c)


def boolean(obj, cutter, op='DIFFERENCE'):
    m = obj.modifiers.new("b", 'BOOLEAN'); m.operation = op
    m.object = cutter; m.solver = 'EXACT'
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=m.name)


def check(obj, label):
    bm = bmesh.new(); bm.from_mesh(obj.data)
    nm = [e for e in bm.edges if len(e.link_faces) != 2]
    lv = [v for v in bm.verts if not v.link_edges]
    vol = bm.calc_volume(signed=True) * 1e9
    bm.free()
    return dict(part=label, verts=len(obj.data.vertices), non_manifold_edges=len(nm),
                loose_verts=len(lv), volume_mm3=round(vol, 1),
                watertight=(not nm and not lv))


Z0, Z1 = 0.0, T['plate_t']
plate = prism("FIT_TEST_PLATE", rrect(T['plate_x'], T['plate_y'], T['plate_r']),
              Z0, Z1, (0, 0), col)

cuts = []
def add(name, prof, loc, z0=Z0 - 1, z1=Z1 + 1):
    cuts.append(prism(name, prof, z0, z1, loc, col))

def markers(n, x, y):
    """n dots identify the variant: 1 dot = smallest clearance."""
    span = (n - 1) * T['marker_pitch']
    for i in range(n):
        add("MK", ellipse(T['marker_dia'], T['marker_dia'], 20),
            (x - span / 2 + i * T['marker_pitch'], y))

# --- row A: the big-connector ladder, 22.0 -> 23.2 in 0.2 steps, all ROUND.
# A flat shrinks a hole on one side, so it blocks a round part rather than
# merely failing to key it. Only add one where the part actually has a key.
for i in range(T['big_n']):
    dia = T['big_lo'] + i * T['big_step']
    x = (i - (T['big_n'] - 1) / 2.0) * T['big_pitch']
    add("BIG", ellipse(dia, dia, 96), (x, T['big_y']))
    markers(i + 1, x, T['big_y'] - 15.0)

# --- row B: power hole at 4 clearances (round), one keyed copy, two slots
for i, c in enumerate(T['pwr_clears']):
    x = (i - 1.5) * T['pwr_pitch']
    add("PWR", ellipse(T['pwr_dia'] + c, T['pwr_dia'] + c, 64), (x, T['pwr_y']))
    markers(i + 1, x, T['pwr_y'] - 12.0)
# the keyed variant: checks whether the connector's flat wants to be at the top
add("PWRKEY", dhole(T['pwr_key_dia'], T['pwr_key_flat']), (T['pwr_key_x'], T['pwr_y']))
for i, (x, c) in enumerate(zip(T['slot_x'], (0.0, 0.4))):
    add("SLOT", rrect(T['slot_l'] + c, T['slot_w'] + c, T['slot_r']), (x, T['pwr_y']))
    markers(i + 1, x, T['pwr_y'] - 8.0)

# --- row C: a full 8-hole row at true pitch. Tests accumulated pitch error,
# which a 2x2 patch cannot, and the button opening at the same time.
for gx in range(8):
    add("PAD", rrect(T['pad_open'], T['pad_open'], T['pad_r']),
        ((gx - 3.5) * T['pad_pitch'], T['pad_y']))

# --- two M4 insert bosses, one solid, one with a pilot
for sx, pilot in ((-1, 0.0), (1, T['insert_pilot'])):
    b = prism("BOSS", ellipse(T['boss_dia'], T['boss_dia']), Z1 - 0.5, Z1 + T['boss_h'],
              (sx * T['boss_x'], T['boss_y']), col)
    boolean(plate, b, 'UNION')
    bpy.data.objects.remove(b, do_unlink=True)
    if pilot > 0:
        add("PILOT", ellipse(pilot, pilot), (sx * T['boss_x'], T['boss_y']),
            z0=Z1 + T['boss_h'] - 10.0, z1=Z1 + T['boss_h'] + 1)

bpy.ops.object.select_all(action='DESELECT')
for c_ in cuts:
    c_.select_set(True)
bpy.context.view_layer.objects.active = cuts[0]
bpy.ops.object.join()
j = bpy.context.view_layer.objects.active
j.name = "TMP_testcuts"
boolean(plate, j, 'DIFFERENCE')
bpy.data.objects.remove(j, do_unlink=True)

TEST_REPORT = [check(plate, "testplate")]
print(TEST_REPORT)
