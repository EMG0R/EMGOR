"""oFOOTS v2 shell - clean rebuild.  Author: emgor.  Units: mm.

WHY THIS IS A REBUILD, NOT A PATCH
The old version pushed a rear TAB out past the deck, which made the plan
outline concave.  Everything bad followed from that one decision:
  - concave deck cap -> ear-clip threw triangles past the reflex corner
  - the tab's crown facet colliding with the body's crown -> junction chaos
  - the plateau reusing a tab vertex -> a zero-width side face (knife edge)
Here the deck is a plain convex 8-gon and the screen plateau sits INSIDE it.
Convex caps cannot triangulate wrong, and there is no junction to go wrong.

INTERLOCK
A solid rib runs the length of the crack on the inside, under the deck.  The
split cuts that rib as a SQUARE WAVE, so the two halves comb into each other -
locked in X by the finger walls, locked in Y by the wave itself.  Above the
deck's mid-thickness the cut follows the crack exactly, so the visible top line
is unchanged.

RULES LEARNED THE HARD WAY, ALL ENFORCED BELOW
  1. Convex caps only.  EAR_CLIP on a concave n-gon is not reliable.
  2. Every solid gets _orient(): recalc_face_normals alone can settle inward,
     and an inverted tool turns a boolean into garbage.
  3. No tangencies.  A tool that ends exactly ON a surface, or exactly on a
     vertex of one, leaves torn edges.  Overlap or sink, never kiss.
  4. Cut apertures LAST, after the bevel, so they stay crisp.
  5. Never join overlapping cutters into one mesh - that mesh is
     self-intersecting and the exact solver mangles it.
"""
import bpy, bmesh, math
from mathutils.bvhtree import BVHTree

P = dict(
    pitch_x=60.0, row_y=60.0,   # row pitch = column pitch -> square grid
    knob_r=10.0, pix_off=18.0,
    enc_hole=7.4, pix_hole=4.0,
    edge_s=20.0, edge_f=16.0, corner=30.0,
    rake=9.0, z_front=26.0, crown=0.35,
    pod_hw=52.0, pod_d=56.0, pod_h=7.0, pod_c=14.0, pod_gap=6.0,
    win=30.0,
    wall=4.0, plate_t=3.0,
    boss_r=4.5, boss_h=12.0, screw_r=1.7, csk_r=3.2, csk_t=1.5,
    rib_w=10.0, rib_h=10.0, wave_amp=3.0, wave_half=13.0,
    shoulder_y=60.0, rear_hw=70.0,   # where the taper starts, and the rear width
    gap=0.1, bevel=0.4,
)

COLX = [(-2.5 + i) * P['pitch_x'] for i in range(6)]
ROWY = [-P['row_y'] / 2, P['row_y'] / 2]
PIXY = {y: y + P['pix_off'] for y in ROWY}
HX = abs(COLX[0]) + P['knob_r'] + P['edge_s']              # 180
YF = ROWY[0] - P['knob_r'] - P['edge_f']                   # -71
POD_Y0 = PIXY[ROWY[1]] + P['pix_hole'] / 2 + P['pod_gap']  # plateau front
POD_Y1 = POD_Y0 + P['pod_d']
YR = POD_Y1 + 13.0                                          # deck rear edge
TAN = math.tan(math.radians(P['rake']))
C, PH = P['corner'], P['pod_h']
YLO, YHI = -400.0, 400.0

KNOTS = [(YF - 40, -26), (YF, -26), (-38, 10), (0, 0), (38, -10), (75, 26), (YR + 40, 26)]
RIB_Y0, RIB_Y1 = YF + 6.0, YR - 6.0

def dz(y):       return P['z_front'] + (y - YF) * TAN
def crown_at(y): return P['crown'] * (dz(y) - P['plate_t'])
def crack_x(u):
    if u <= KNOTS[0][0]: return KNOTS[0][1]
    for (a, b), (c, d) in zip(KNOTS, KNOTS[1:]):
        if a <= u <= c: return b + (d - b) * (u - a) / (c - a)
    return KNOTS[-1][1]
def pod_top_z(y):  return dz(y) + PH
def ceil_of(y):    # underside of whatever roof is above y
    inside = (POD_Y0 + PH) < y < (POD_Y1 - PH)
    return (pod_top_z(y) if inside else dz(y)) - P['wall']
def wave(y):
    if not (RIB_Y0 <= y <= RIB_Y1): return 0.0
    k = math.floor((y - RIB_Y0) / P['wave_half'])
    return P['wave_amp'] if k % 2 == 0 else -P['wave_amp']

def oct8(hx, y0, y1, c):
    """Convex 8-gon, CCW."""
    return [( hx - c, y0), ( hx, y0 + c), ( hx, y1 - c), ( hx - c, y1),
            (-hx + c, y1), (-hx, y1 - c), (-hx, y0 + c), (-hx + c, y0)]

SY, RW = P['shoulder_y'], P['rear_hw']
# The deck is NOT a box.  Sides run straight back to shoulder_y, then cut in on
# a long angled shoulder to a narrow rear edge that the screen plateau sits in.
# Angle is set by the outer pixel at (150, 63): the shoulder has to stay clear
# of it, which is why the taper starts at y=60 and not further forward.
DECK = [( HX - C, YF), ( HX, YF + C), ( HX, SY), ( RW, YR),
        (-RW, YR), (-HX, SY), (-HX, YF + C), (-HX + C, YF)]
def shoulder_x(y):
    return HX - (HX - RW) * max(0.0, min(1.0, (y - SY) / (YR - SY)))
POD_B = oct8(P['pod_hw'], POD_Y0, POD_Y1, P['pod_c'])
POD_T = oct8(P['pod_hw'] - PH, POD_Y0 + PH, POD_Y1 - PH,
             max(P['pod_c'] - PH * 0.4142, 3.0))

# ----------------------------------------------------------------- helpers
SC = bpy.context.scene
for o in list(bpy.data.objects): bpy.data.objects.remove(o, do_unlink=True)
COL = bpy.data.collections.get('oFOOTS_v2') or bpy.data.collections.new('oFOOTS_v2')
if COL.name not in [c.name for c in SC.collection.children]:
    SC.collection.children.link(COL)
SC.unit_settings.system = 'METRIC'
SC.unit_settings.scale_length = 0.001
SC.unit_settings.length_unit = 'MILLIMETERS'
TMP = SC.collection

def _orient(bm):
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    if bm.calc_volume(signed=True) < 0:
        bmesh.ops.reverse_faces(bm, faces=bm.faces)

def mk(name, coll):
    me = bpy.data.meshes.new(name); ob = bpy.data.objects.new(name, me)
    coll.objects.link(ob); return ob, me

def bridge(name, rings, coll=None):
    """Loft closed rings of equal length, cap both ends, triangulate, orient."""
    ob, me = mk(name, coll or TMP)
    bm = bmesh.new()
    V = [[bm.verts.new(p) for p in r] for r in rings]
    n = len(V[0])
    for a, b in zip(V, V[1:]):
        for i in range(n):
            try: bm.faces.new((a[i], a[(i + 1) % n], b[(i + 1) % n], b[i]))
            except ValueError: pass
    bm.faces.new(V[0][::-1]); bm.faces.new(V[-1])
    bmesh.ops.triangulate(bm, faces=bm.faces[:], quad_method='FIXED', ngon_method='EAR_CLIP')
    _orient(bm); bm.normal_update(); bm.to_mesh(me); bm.free()
    return ob

def boolean(t, tool, op):
    m = t.modifiers.new('b', 'BOOLEAN'); m.operation = op; m.object = tool; m.solver = 'EXACT'
    bpy.context.view_layer.objects.active = t
    bpy.ops.object.modifier_apply(modifier='b')
    bpy.data.objects.remove(tool, do_unlink=True)

def offs(pts, d):
    n = len(pts); out = []
    for i in range(n):
        px, py = pts[i - 1]; cx, cy = pts[i]; nx, ny = pts[(i + 1) % n]
        ns = []
        for ex, ey in ((cx - px, cy - py), (nx - cx, ny - cy)):
            L = math.hypot(ex, ey) or 1.0
            ns.append((ey / L, -ex / L))
        ax, ay = ns[0][0] + ns[1][0], ns[0][1] + ns[1][1]
        L = math.hypot(ax, ay) or 1.0; ax, ay = ax / L, ay / L
        ch = max(ax * ns[0][0] + ay * ns[0][1], 0.5)
        v = d(cx, cy) if callable(d) else d
        out.append((cx + ax * v / ch, cy + ay * v / ch))
    return out

def mat(name, rgb, rough=0.5):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*rgb, 1)
    b.inputs['Roughness'].default_value = rough
    b.inputs['Metallic'].default_value = 0.15
    return m

# ---- 1. body: convex 8-gon, 45 deg crown then vertical to the floor -------
D1 = offs(DECK, lambda x, y: crown_at(y))
shell = bridge('oFOOTS_SHELL', [
    [(x, y, dz(y)) for x, y in DECK],
    [(x, y, P['plate_t'] + (dz(DECK[i][1]) - P['plate_t']) * (1 - P['crown']))
     for i, (x, y) in enumerate(D1)],
    [(x, y, P['plate_t']) for x, y in D1]], COL)

# ---- 2. screen plateau, wholly inside the deck, 45 deg on every side ------
boolean(shell, bridge('POD', [[(x, y, pod_top_z(y)) for x, y in POD_T],
                             [(x, y, dz(y) - 16.0) for x, y in POD_B]]), 'UNION')

# ---- 3. right-angle facets: plateau top edge out to the deck corner -------
# Right angle sits at the plateau's rear-side corner.  The outer vertex is
# sunk under the deck and pushed past the corner so the plane crosses the deck
# transversally - ending it ON the deck plane is a tangency and tears.
# Three wedges per side, fanned across the rear quadrant beside the plateau.
# Different slopes, so where they meet they leave a crease - that is where the
# extra lines come from.  Every one is clipped to the footprint and every low
# vertex is sunk 6 mm under the deck and pushed 8 mm past the shoulder, so the
# planes cross the deck transversally instead of kissing it.
def sx(y):  return shoulder_x(y) + 8.0
def facet(tri):
    fin = bridge('fin', [list(tri), [(p[0], p[1], dz(p[1]) - 16.0) for p in tri]])
    clip = bridge('clip', [[(x, y, -60.0) for x, y in D1], [(x, y, 300.0) for x, y in D1]])
    boolean(fin, clip, 'INTERSECT')
    boolean(shell, fin, 'UNION')

for s_ in (1.0, -1.0):
    fx = s_ * (P['pod_hw'] - PH)
    y0, y1 = POD_Y0 + PH, POD_Y1 - PH
    ym = (y0 + y1) / 2
    F = [
        # 1. plateau's rear-side corner out to the shoulder - the right angle
        [(fx, y0, pod_top_z(y0)), (s_ * sx(y1), y1, dz(y1) - 6.0), (fx, y1, pod_top_z(y1))],
        # 2. a shallower one running forward off the same corner
        [(fx, y0, pod_top_z(y0)), (s_ * sx(y0 - 8), y0 - 8, dz(y0 - 8) - 6.0),
         (fx + s_ * 34, y0 - 26, dz(y0 - 26) - 6.0)],
        # 3. a raised rib along the shoulder itself, falling away inboard
        [(s_ * (shoulder_x(SY + 14) - 3), SY + 14, dz(SY + 14) + 5.0),
         (s_ * (shoulder_x(YR - 18) - 3), YR - 18, dz(YR - 18) + 5.0),
         (fx + s_ * 10, ym, dz(ym) - 6.0)],
    ]
    for tri in F:
        facet(tri if s_ > 0 else list(reversed(tri)))

# ---- 4. hollow to a 4 mm wall, bottom open -------------------------------
bm = bmesh.new(); bm.from_mesh(shell.data)
bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.normal.z < -0.9
                           and abs(f.calc_center_median().z - P['plate_t']) < 0.5],
                 context='FACES')
bm.to_mesh(shell.data); bm.free()
m = shell.modifiers.new('s', 'SOLIDIFY'); m.thickness = P['wall']; m.offset = 1.0
bpy.context.view_layer.objects.active = shell
bpy.ops.object.modifier_apply(modifier='s')

# ---- 5. interlock rib: a solid bar under the deck, following the crack ----
RY = sorted(set([RIB_Y0, RIB_Y1] + [k[0] for k in KNOTS if RIB_Y0 < k[0] < RIB_Y1] +
                [POD_Y0 + PH + 0.05, POD_Y1 - PH - 0.05] +
                [RIB_Y0 + i * 4.0 for i in range(int((RIB_Y1 - RIB_Y0) / 4.0) + 1)]))
RY = [y for y in RY if RIB_Y0 <= y <= RIB_Y1]
rings = []
for y in RY:
    cx = crack_x(y); zt = ceil_of(y) + 1.5; zb = zt - P['rib_h']
    hw = P['rib_w'] / 2
    rings.append([(cx - hw, y, zb), (cx + hw, y, zb), (cx + hw, y, zt), (cx - hw, y, zt)])
rib, rme = mk('RIB', TMP)
bm = bmesh.new()
V = [[bm.verts.new(p) for p in r] for r in rings]
for a, b in zip(V, V[1:]):
    for i in range(4): bm.faces.new((a[i], a[(i + 1) % 4], b[(i + 1) % 4], b[i]))
bm.faces.new(V[0][::-1]); bm.faces.new(V[-1])
bmesh.ops.triangulate(bm, faces=bm.faces[:], quad_method='FIXED', ngon_method='EAR_CLIP')
_orient(bm); bm.normal_update(); bm.to_mesh(rme); bm.free()
boolean(shell, rib, 'UNION')

# ---- 6. four solid corner bosses for M3 heat-set inserts ------------------
S = offs(D1, -(P['wall'] + P['boss_r'] - 2.0))
mid = lambda i, j: ((S[i][0] + S[j][0]) / 2, (S[i][1] + S[j][1]) / 2)
BOSS = [mid(0, 1), mid(2, 3), mid(4, 5), mid(6, 7)]
for x, y in BOSS:
    bpy.ops.mesh.primitive_cylinder_add(radius=P['boss_r'], depth=P['boss_h'], vertices=20,
                                        location=(x, y, P['plate_t'] + P['boss_h'] / 2))
    boolean(shell, bpy.context.object, 'UNION')

# ---- 7. split: crack above mid-deck, SQUARE WAVE through the rib ---------
def splitter(dx, name):
    ys = set([YLO, YHI] + [k[0] for k in KNOTS])
    t = RIB_Y0
    while t <= RIB_Y1:
        ys |= {t - 0.4, t + 0.4}
        t += P['wave_half']
    ys |= {RIB_Y0 - 0.4, RIB_Y0 + 0.4, RIB_Y1 - 0.4, RIB_Y1 + 0.4}
    ys = sorted(y for y in ys if YLO <= y <= YHI)
    ob, me = mk(name, TMP)
    bm = bmesh.new(); V = []
    for y in ys:
        cx = crack_x(y) + dx
        zm = (pod_top_z(y) if (POD_Y0 + PH) < y < (POD_Y1 - PH) else dz(y)) - P['wall'] / 2.0
        bx = cx + wave(y)
        sec = [(-1200.0, -80.0), (bx, -80.0), (bx, zm), (cx, zm), (cx, 400.0), (-1200.0, 400.0)]
        V.append([bm.verts.new((px, y, pz)) for px, pz in sec])
    for a, b in zip(V, V[1:]):
        for i in range(6): bm.faces.new((a[i], a[(i + 1) % 6], b[(i + 1) % 6], b[i]))
    bm.faces.new(V[0][::-1]); bm.faces.new(V[-1])
    bmesh.ops.triangulate(bm, faces=bm.faces[:], quad_method='FIXED', ngon_method='EAR_CLIP')
    _orient(bm); bm.normal_update(); bm.to_mesh(me); bm.free()
    return ob

SL = shell.copy(); SL.data = shell.data.copy(); SL.name = 'SHELL_L'; COL.objects.link(SL)
SR = shell; SR.name = 'SHELL_R'
boolean(SL, splitter(-P['gap'], 'sa'), 'INTERSECT')
boolean(SR, splitter(+P['gap'], 'sb'), 'DIFFERENCE')

# ---- 8. soften, THEN cut apertures so they stay crisp ---------------------
def soften(ob, off=None, segs=2, min_len=6.0, ang=math.radians(25)):
    off = P['bevel'] if off is None else off
    bm = bmesh.new(); bm.from_mesh(ob.data)
    wx = P['win'] / 2 + 1.0
    wy0, wy1 = (POD_Y0 + POD_Y1) / 2 - P['win'] / 2 - 1.0, (POD_Y0 + POD_Y1) / 2 + P['win'] / 2 + 1.0
    def on_win(e):
        return all(abs(v.co.x) <= wx and wy0 <= v.co.y <= wy1 for v in e.verts)
    es = [e for e in bm.edges if e.is_manifold and e.calc_length() > min_len
          and e.calc_face_angle(0.0) > ang and not on_win(e)]
    if es:
        bmesh.ops.bevel(bm, geom=es, offset=off, segments=segs, profile=0.5,
                        affect='EDGES', clamp_overlap=True)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=5e-3)
    bmesh.ops.dissolve_degenerate(bm, dist=5e-3, edges=bm.edges)
    _orient(bm); bm.normal_update(); bm.to_mesh(ob.data); bm.free()
soften(SL); soften(SR)

def apertures(target):
    for x in COLX:
        for y in list(ROWY) + [PIXY[r] for r in ROWY]:
            d = P['enc_hole'] if y in ROWY else P['pix_hole']
            bpy.ops.mesh.primitive_cylinder_add(radius=d / 2, depth=140, vertices=24,
                                                location=(x, y, dz(y)))
            o = bpy.context.object; o.rotation_euler = (math.radians(-P['rake']), 0, 0)
            boolean(target, o, 'DIFFERENCE')
    wy = (POD_Y0 + POD_Y1) / 2
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, wy, pod_top_z(wy)))
    w = bpy.context.object; w.scale = (P['win'], P['win'], 140.0)
    w.rotation_euler = (math.radians(-P['rake']), 0, 0)
    boolean(target, w, 'DIFFERENCE')
apertures(SL); apertures(SR)

# ---- 9. bottom plates ----------------------------------------------------
plate = bridge('PLATE', [[(x, y, P['plate_t']) for x, y in D1],
                         [(x, y, 0.0) for x, y in D1]])
SEG = 24
for x, y in BOSS:
    rr = [(-1.0, P['csk_r']), (0.0, P['csk_r']),
          (P['csk_t'], P['screw_r']), (P['plate_t'] + 2.0, P['screw_r'])]
    boolean(plate, bridge('h', [[(x + r * math.cos(2 * math.pi * i / SEG),
                                 y + r * math.sin(2 * math.pi * i / SEG), z)
                                for i in range(SEG)] for z, r in rr]), 'DIFFERENCE')
PL = plate.copy(); PL.data = plate.data.copy(); PL.name = 'PLATE_L'; COL.objects.link(PL)
PR = plate; PR.name = 'PLATE_R'
TMP.objects.unlink(PR); COL.objects.link(PR)
def plain(dx, name):
    ys = sorted(set([YLO, YHI] + [k[0] for k in KNOTS]))
    pts = [(-1200.0, YLO)] + [(crack_x(y) + dx, y) for y in ys] + [(-1200.0, YHI)]
    # drop collinear interior points: the crack is flat outside its first and
    # last knot, so three points sit on one line and triangulate to slivers
    poly = [pts[0]]
    for a, b, c in zip(pts, pts[1:], pts[2:]):
        if abs((b[0]-a[0])*(c[1]-a[1]) - (b[1]-a[1])*(c[0]-a[0])) > 1e-6:
            poly.append(b)
    poly.append(pts[-1])
    return bridge(name, [[(x, y, -60.0) for x, y in poly], [(x, y, 60.0) for x, y in poly]])
boolean(PL, plain(-P['gap'], 'pa'), 'INTERSECT')
boolean(PR, plain(+P['gap'], 'pb'), 'DIFFERENCE')
# plates left unbevelled on purpose: see note above

# ---- 10. materials, heal, audit -----------------------------------------
for ob, m in ((SL, mat('shellL', (0.075, 0.075, 0.085), 0.48)),
              (SR, mat('shellR', (0.105, 0.105, 0.115), 0.48)),
              (PL, mat('plate', (0.16, 0.16, 0.18), 0.55)),
              (PR, mat('plate', (0.16, 0.16, 0.18), 0.55))):
    ob.data.materials.clear(); ob.data.materials.append(m)
    for f in ob.data.polygons: f.use_smooth = False

def audit(ob):
    bm = bmesh.new(); bm.from_mesh(ob.data)
    bmesh.ops.triangulate(bm, faces=bm.faces[:], quad_method='FIXED', ngon_method='EAR_CLIP')
    bm.faces.ensure_lookup_table()
    t = BVHTree.FromBMesh(bm, epsilon=0.0); sx = 0
    for a, b in t.overlap(t):
        if a >= b: continue
        if {v.index for v in bm.faces[a].verts} & {v.index for v in bm.faces[b].verts}: continue
        sx += 1
    v = bm.calc_volume(signed=True)
    d = dict(non_manifold=len([e for e in bm.edges if not e.is_manifold]),
             degenerate=len([f for f in bm.faces if f.calc_area() < 1e-7]),
             self_int=sx, vol_cm3=round(v / 1000, 1), tris=len(bm.faces),
             bbox=[round(q, 1) for q in ob.dimensions])
    bm.free(); return d

def heal(ob):
    before = audit(ob)
    if before['non_manifold'] > 60 or before['vol_cm3'] <= 0: return
    snap = ob.data.copy()
    d = ob.copy(); d.data = ob.data.copy(); TMP.objects.link(d)
    m = ob.modifiers.new('h', 'BOOLEAN'); m.operation = 'UNION'; m.object = d; m.solver = 'EXACT'
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.modifier_apply(modifier='h')
    bpy.data.objects.remove(d, do_unlink=True)
    bm = bmesh.new(); bm.from_mesh(ob.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=5e-3)
    bmesh.ops.dissolve_degenerate(bm, dist=5e-3, edges=bm.edges)
    _orient(bm); bm.normal_update(); bm.to_mesh(ob.data); bm.free()
    after = audit(ob)
    sc = lambda a: a['non_manifold'] * 10 + a['self_int'] + a['degenerate']
    if after['vol_cm3'] <= 0 or sc(after) > sc(before): ob.data = snap

OBJS = [o for o in COL.objects if o.type == 'MESH']
for _ in range(3):
    for o in OBJS: heal(o)
REPORT = {o.name: audit(o) for o in OBJS}
REPORT['_geom'] = dict(deck=[2 * HX, YR - YF], z_front=round(dz(YF), 1),
                       z_rear=round(dz(YR), 1), crest=round(pod_top_z(POD_Y1 - PH), 1),
                       plateau=[2 * (P['pod_hw'] - PH), P['pod_d'] - 2 * PH],
                       plateau_y=[POD_Y0, POD_Y1], window=P['win'],
                       rib=[P['rib_w'], P['rib_h']],
                       wave=[P['wave_amp'] * 2, P['wave_half'] * 2],
                       fingers=int((RIB_Y1 - RIB_Y0) / P['wave_half']),
                       boss=[[round(x, 1), round(y, 1)] for x, y in BOSS])
bpy.ops.object.select_all(action='DESELECT')
result = REPORT
