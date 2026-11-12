"""oFOOTS v2 - two-part diamond shell + bottom plates.  Author: emgor.

Form: raked deck -> single 45 deg crown band (0.35 of LOCAL height) -> vertical
girdle straight to the floor.  Widest at the floor, nothing fancy underneath.
Plan is an 8-gon with one 45 deg cut per corner, plus a rear tab carrying the
raised screen plateau.  A jagged crack splits the whole object into two
printable halves; the bottom plate splits on the same line.

Units: millimetres.  Rebuild with:
    exec(compile(open(PATH).read(), PATH, 'exec'), globals())

ADDITIVE ONLY - this file writes nothing into the archive.
"""
import bpy, bmesh, math, json
from mathutils import Vector
from mathutils.bvhtree import BVHTree

P = dict(
    pitch_x   = 60.0,   # encoder pitch across; 6 columns -> 300 mm outer span
    row_y     = 90.0,   # front row to back row
    knob_r    = 10.0,   # ASSUMPTION - 20 mm knob, not yet calipered
    pix_off   = 18.0,   # NeoPixel sits this far up-slope of its encoder
    enc_hole  = 7.4,    # M7x0.75 bushing, panel hole per the parts addendum
    pix_hole  = 4.0,    # emitting circle of a WS2812B 5050, not the package
    edge_s    = 20.0,   # deck edge beyond the outer knob, sides
    edge_f    = 16.0,   # ...and front
    corner    = 30.0,   # ONE 45 deg cut per corner. 40 clashed with the outer
                        # encoder bodies: the chamfer ran 11.3 mm from their
                        # centres and the EC11 half-diagonal is 9.05 + 4 wall.
    rake      = 9.0,
    z_front   = 26.0,   # deck height at the front edge
    crown     = 0.35,   # 45 deg band, as a fraction of LOCAL deck height.
                        # Must be local: a fraction of the MAX height drove the
                        # front flank through the floor and folded it back.
    tab_x     = 52.0,   # rear tab half-width -> 104 mm, 29% of deck width
    tab_y0    = 74.0,
    tab_y1    = 126.0,
    tab_c     = 16.0,
    pod_h     = 8.0,    # plateau rise; its sides are 45 deg to match the crown
    wall      = 4.0,
    plate_t   = 3.0,
    grv_w     = 3.0,    # light channel milled along the crack
    grv_d     = 1.5,
    boss_r    = 4.5,    # M3 heat-set boss. SOLID - the insert burns its seat.
    boss_h    = 12.0,   # capped so it stays inside the vertical girdle, which
                        # is only 16.9 mm tall at the front.
    screw_r   = 1.7,    # M3 clearance in the plate
    csk_r     = 3.2,
    csk_t     = 1.5,
    gap       = 0.1,    # half the crack width; 0.2 mm total
)

COLX = [(-2.5 + i) * P['pitch_x'] for i in range(6)]
ROWY = [-P['row_y'] / 2, P['row_y'] / 2]
PIXY = {y: y + P['pix_off'] for y in ROWY}
HX = abs(COLX[0]) + P['knob_r'] + P['edge_s']
YF = ROWY[0] - P['knob_r'] - P['edge_f']
YR = PIXY[ROWY[1]] + P['pix_hole'] / 2 + P['edge_s']
TAN = math.tan(math.radians(P['rake']))
YLO, YHI = -320.0, 320.0

# jagged crack: 7 knots, odd-symmetric about y=0 over the main deck, then it
# runs straight out through the tab.  Straight segments only - sampling this
# into 121 points seeded sliver triangles in the boolean.
KNOTS = [(YF - 40, -26), (YF, -26), (-38, 10), (0, 0),
         (38, -10), (75, 26), (P['tab_y1'] + 40, 26)]

def dz(y):        return P['z_front'] + (y - YF) * TAN
def crown_at(y):  return P['crown'] * (dz(y) - P['plate_t'])
def crack_x(u):
    if u <= KNOTS[0][0]: return KNOTS[0][1]
    for (a, b), (c, d) in zip(KNOTS, KNOTS[1:]):
        if a <= u <= c: return b + (d - b) * (u - a) / (c - a)
    return KNOTS[-1][1]
def seam_top(y):  return dz(y) + (P['pod_h'] if y > P['tab_y0'] + P['pod_h'] else 0.0)

TX, TY0, TY1, TC, C = P['tab_x'], P['tab_y0'], P['tab_y1'], P['tab_c'], P['corner']
PLAN = [( HX - C, YF), ( HX, YF + C), ( HX, YR - C), ( HX - C, YR),
        ( TX, YR), ( TX, TY1 - TC), ( TX - TC, TY1), (-(TX - TC), TY1),
        (-TX, TY1 - TC), (-TX, YR),
        (-HX + C, YR), (-HX, YR - C), (-HX, YF + C), (-HX + C, YF)]
# convex decomposition of the same outline - used for the plate, whose caps
# would otherwise be a concave n-gon.  Ear-clip put a cap triangle 0.72 mm
# past the reflex corner where the tab meets the body.
MAIN = [( HX - C, YF), ( HX, YF + C), ( HX, YR - C), ( HX - C, YR),
        (-HX + C, YR), (-HX, YR - C), (-HX, YF + C), (-HX + C, YF)]
TAB  = [( TX, YR - 12.0), ( TX, TY1 - TC), ( TX - TC, TY1),
        (-(TX - TC), TY1), (-TX, TY1 - TC), (-TX, YR - 12.0)]

# ----------------------------------------------------------------- helpers
SC = bpy.context.scene

def offs(pts, d, clamp=0.45):
    """Offset a closed polygon along mitred vertex normals. d may be callable."""
    n = len(pts); out = []
    for i in range(n):
        px, py = pts[i - 1]; cx, cy = pts[i]; nx, ny = pts[(i + 1) % n]
        ns = []
        for ex, ey in ((cx - px, cy - py), (nx - cx, ny - cy)):
            L = math.hypot(ex, ey) or 1.0
            ns.append((ey / L, -ex / L))
        ax, ay = ns[0][0] + ns[1][0], ns[0][1] + ns[1][1]
        L = math.hypot(ax, ay) or 1.0
        ax, ay = ax / L, ay / L
        ch = max(ax * ns[0][0] + ay * ns[0][1], clamp)
        v = d(cx, cy) if callable(d) else d
        out.append((cx + ax * v / ch, cy + ay * v / ch))
    return out

def _finish(bm, me, caps):
    bmesh.ops.triangulate(bm, faces=caps, quad_method='FIXED', ngon_method='EAR_CLIP')
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.normal_update(); bm.to_mesh(me); bm.free()

def loft(name, loops, coll):
    me = bpy.data.meshes.new(name); ob = bpy.data.objects.new(name, me)
    coll.objects.link(ob)
    bm = bmesh.new()
    V = [[bm.verts.new(p) for p in L] for L in loops]
    n = len(V[0])
    for a, b in zip(V, V[1:]):
        for i in range(n):
            try: bm.faces.new((a[i], a[(i + 1) % n], b[(i + 1) % n], b[i]))
            except ValueError: pass
    _finish(bm, me, [bm.faces.new(V[0][::-1]), bm.faces.new(V[-1])])
    return ob

def slab(name, poly, z0, z1, coll):
    """Prism over a CONVEX polygon - caps triangulate exactly."""
    me = bpy.data.meshes.new(name); ob = bpy.data.objects.new(name, me)
    coll.objects.link(ob)
    bm = bmesh.new()
    A = [bm.verts.new((x, y, z1)) for x, y in poly]
    B = [bm.verts.new((x, y, z0)) for x, y in poly]
    n = len(poly)
    for i in range(n): bm.faces.new((A[i], A[(i + 1) % n], B[(i + 1) % n], B[i]))
    _finish(bm, me, [bm.faces.new(A[::-1]), bm.faces.new(B)])
    return ob

def boolean(target, tool, op):
    m = target.modifiers.new('b', 'BOOLEAN')
    m.operation = op; m.object = tool; m.solver = 'EXACT'
    bpy.context.view_layer.objects.active = target
    bpy.ops.object.modifier_apply(modifier='b')
    bpy.data.objects.remove(tool, do_unlink=True)

def mat(name, rgb, rough=0.5, metal=0.15):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*rgb, 1)
    b.inputs['Roughness'].default_value = rough
    b.inputs['Metallic'].default_value = metal
    return m

# ----------------------------------------------------------------- scene
for o in list(bpy.data.objects): bpy.data.objects.remove(o, do_unlink=True)
COL = bpy.data.collections.get('oFOOTS_v2') or bpy.data.collections.new('oFOOTS_v2')
if COL.name not in [c.name for c in SC.collection.children]:
    SC.collection.children.link(COL)
SC.unit_settings.system = 'METRIC'
SC.unit_settings.scale_length = 0.001
SC.unit_settings.length_unit = 'MILLIMETERS'
TMP = SC.collection

# ---- 1. outer solid: deck / crown base / floor.
# Built as MAIN + TAB and unioned.  Lofting the concave PLAN outline directly
# makes a concave deck cap, and ear-clip throws a triangle ~0.7 mm past the
# reflex corner where the tab meets the body -> self-intersections.
# A convex cap cannot triangulate wrong.
P1 = offs(PLAN, lambda x, y: crown_at(y))       # still needed for bosses + plate

def body(name, poly, coll):
    q = offs(poly, lambda x, y: crown_at(y))
    return loft(name, [
        [(x, y, dz(y)) for x, y in poly],
        [(x, y, P['plate_t'] + (dz(poly[i][1]) - P['plate_t']) * (1 - P['crown']))
         for i, (x, y) in enumerate(q)],
        [(x, y, P['plate_t']) for x, y in q]], coll)

shell = body('oFOOTS_SHELL', MAIN, COL)
boolean(shell, body('TABBODY', TAB, TMP), 'UNION')

# ---- 2. screen plateau, 45 deg sides, rear face flush into the crown
PH = P['pod_h']
BASE = [( TX, TY0), ( TX, TY1 - TC), ( TX - TC, TY1),
        (-(TX - TC), TY1), (-TX, TY1 - TC), (-TX, TY0)]
TOP  = [( TX - PH, TY0 + PH), ( TX - PH, TY1 - TC), ( TX - TC, TY1),
        (-(TX - TC), TY1), (-TX + PH, TY1 - TC), (-TX + PH, TY0 + PH)]
boolean(shell, loft('POD', [[(x, y, dz(y) + PH) for x, y in TOP],
                            [(x, y, dz(y) - 14.0) for x, y in BASE]], TMP), 'UNION')

# ---- 3. hollow to a 4 mm wall, open bottom
bm = bmesh.new(); bm.from_mesh(shell.data)
bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.normal.z < -0.9
                           and abs(f.calc_center_median().z - P['plate_t']) < 0.5],
                 context='FACES')
bm.to_mesh(shell.data); bm.free()
m = shell.modifiers.new('s', 'SOLIDIFY'); m.thickness = P['wall']; m.offset = 1.0
bpy.context.view_layer.objects.active = shell
bpy.ops.object.modifier_apply(modifier='s')

# ---- 4. NO milled light channel.
# A swept groove needs the height of the real surface under every sample.
# seam_top() is a STEP - it jumps pod_h at the plateau riser instead of
# following the 45 deg face - so the cutter leapt vertically there and punched
# a rectangular slot clean through the riser, and did the same where the path
# ran off the deck onto the crown.  The 0.2 mm split IS the crack; a light
# channel belongs on the split faces as a chamfer, where no surface query is
# needed and it cannot miss.

# ---- 5. encoder + pixel apertures, cut normal to the deck
cuts = []
def cyl(d, x, y, coll=TMP):
    bpy.ops.mesh.primitive_cylinder_add(radius=d / 2, depth=90, vertices=24,
                                        location=(x, y, dz(y)))
    o = bpy.context.object; o.rotation_euler = (math.radians(-P['rake']), 0, 0)
    cuts.append(o)
for x in COLX:
    for y in ROWY: cyl(P['enc_hole'], x, y)
    for y in ROWY: cyl(P['pix_hole'], x, PIXY[y])
bpy.ops.object.select_all(action='DESELECT')
for o in cuts: o.select_set(True)
bpy.context.view_layer.objects.active = cuts[0]; bpy.ops.object.join()
boolean(shell, bpy.context.object, 'DIFFERENCE')

# ---- 6. four SOLID corner bosses, fused 2 mm into the inner wall
S = offs(P1, -(P['wall'] + P['boss_r'] - 2.0))
mid = lambda i, j: ((S[i][0] + S[j][0]) / 2, (S[i][1] + S[j][1]) / 2)
BOSS = [mid(0, 1), mid(2, 3), mid(10, 11), mid(12, 13)]
bs = []
for x, y in BOSS:
    bpy.ops.mesh.primitive_cylinder_add(radius=P['boss_r'], depth=P['boss_h'],
        vertices=20, location=(x, y, P['plate_t'] + P['boss_h'] / 2))
    bs.append(bpy.context.object)
bpy.ops.object.select_all(action='DESELECT')
for o in bs: o.select_set(True)
bpy.context.view_layer.objects.active = bs[0]; bpy.ops.object.join()
boolean(shell, bpy.context.object, 'UNION')

# ---- 7. split on the crack
def prism(dx, name):
    ys = [YLO] + [k[0] for k in KNOTS] + [YHI]
    xs = [KNOTS[0][1]] + [k[1] for k in KNOTS] + [KNOTS[-1][1]]
    poly = [(-900.0, YLO)] + [(x + dx, y) for x, y in zip(xs, ys)] + [(-900.0, YHI)]
    return slab(name, poly, -60.0, 320.0, TMP)

SL = shell.copy(); SL.data = shell.data.copy(); SL.name = 'SHELL_L'; COL.objects.link(SL)
SR = shell; SR.name = 'SHELL_R'
boolean(SL, prism(-P['gap'], 'pa'), 'INTERSECT')
boolean(SR, prism(+P['gap'], 'pb'), 'DIFFERENCE')

# ---- 8. bottom plate: two convex prisms unioned, never a concave cap
plate = slab('PLATE', offs(MAIN, lambda x, y: crown_at(y)), 0.0, P['plate_t'], TMP)
boolean(plate, slab('PTAB', offs(TAB, lambda x, y: crown_at(y)),
                    0.0, P['plate_t'], TMP), 'UNION')
SEG = 24
def stepped(x, y):
    """csk cone + shank as ONE solid. Overlapping cutters joined into one mesh
    are self-intersecting and the exact solver mangles them."""
    rings = [(-1.0, P['csk_r']), (0.0, P['csk_r']),
             (P['csk_t'], P['screw_r']), (P['plate_t'] + 2.0, P['screw_r'])]
    me = bpy.data.meshes.new('h'); ob = bpy.data.objects.new('h', me)
    TMP.objects.link(ob)
    bm = bmesh.new(); L = []
    for z, r in rings:
        L.append([bm.verts.new((x + r * math.cos(2 * math.pi * i / SEG),
                                y + r * math.sin(2 * math.pi * i / SEG), z))
                  for i in range(SEG)])
    for a, c in zip(L, L[1:]):
        for i in range(SEG): bm.faces.new((a[i], a[(i + 1) % SEG], c[(i + 1) % SEG], c[i]))
    _finish(bm, me, [bm.faces.new(L[0][::-1]), bm.faces.new(L[-1])])
    return ob
hs = [stepped(x, y) for x, y in BOSS]
bpy.ops.object.select_all(action='DESELECT')
for o in hs: o.select_set(True)
bpy.context.view_layer.objects.active = hs[0]; bpy.ops.object.join()
boolean(plate, bpy.context.object, 'DIFFERENCE')

PL = plate.copy(); PL.data = plate.data.copy(); PL.name = 'PLATE_L'; COL.objects.link(PL)
PR = plate; PR.name = 'PLATE_R'
TMP.objects.unlink(PR); COL.objects.link(PR)
boolean(PL, prism(-P['gap'], 'pc'), 'INTERSECT')
boolean(PR, prism(+P['gap'], 'pd'), 'DIFFERENCE')

# ---- 8b. cleanup: booleans leave hairline doubles and degenerate slivers
def tidy(ob):
    bm = bmesh.new(); bm.from_mesh(ob.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-4)
    bmesh.ops.dissolve_degenerate(bm, dist=1e-4, edges=bm.edges)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.normal_update(); bm.to_mesh(ob.data); bm.free()

# ---- 9. materials
for ob, m in ((SL, mat('shellL', (0.075, 0.075, 0.085), 0.48)),
              (SR, mat('shellR', (0.105, 0.105, 0.115), 0.48)),
              (PL, mat('plate',  (0.16, 0.16, 0.18), 0.55)),
              (PR, mat('plate',  (0.16, 0.16, 0.18), 0.55))):
    tidy(ob)
    ob.data.materials.clear(); ob.data.materials.append(m)
    for f in ob.data.polygons: f.use_smooth = False

# ---- 10. audit.  Blender's statistics will not tell you about
#          self-intersection, so test it explicitly after every boolean.
def audit(ob):
    bm = bmesh.new(); bm.from_mesh(ob.data)
    bmesh.ops.triangulate(bm, faces=bm.faces[:], quad_method='FIXED',
                          ngon_method='EAR_CLIP')
    bm.faces.ensure_lookup_table()
    t = BVHTree.FromBMesh(bm, epsilon=0.0); sx = 0
    for a, b in t.overlap(t):
        if a >= b: continue
        if {v.index for v in bm.faces[a].verts} & {v.index for v in bm.faces[b].verts}:
            continue
        sx += 1
    v = bm.calc_volume(signed=True)
    d = dict(tris=len(bm.faces),
             non_manifold=len([e for e in bm.edges if not e.is_manifold]),
             loose=len([x for x in bm.verts if not x.link_edges]),
             degenerate=len([f for f in bm.faces if f.calc_area() < 1e-7]),
             self_intersections=sx, inverted=v < 0,
             volume_cm3=round(v / 1000.0, 1),
             size=[round(q, 1) for q in ob.dimensions])
    bm.free(); return d

bpy.ops.object.select_all(action='DESELECT')
REPORT = {o.name: audit(o) for o in COL.objects if o.type == 'MESH'}
REPORT['_geometry'] = dict(
    deck=[2 * HX, YR - YF], corner_cut=C,
    z_front=round(dz(YF), 1), z_deck_rear=round(dz(YR), 1),
    screen_crest=round(dz(TY1) + PH, 1),
    plateau=[2 * (TX - PH), TY1 - (TY0 + PH)],
    crown_front=round(crown_at(YF), 1), crown_rear=round(crown_at(TY1), 1),
    boss_xy=[[round(x, 1), round(y, 1)] for x, y in BOSS],
    crack_gap=2 * P['gap'])
result = REPORT
