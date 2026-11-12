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
try: SKIP
except NameError: SKIP=set()
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
    win       = 30.0,   # screen window, square. VIEWING area of the panel -
                        # no drawing for the GME128128-01 is published, so this
                        # is the one number to caliper and retype.
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

def _orient(bm):
    """Force outward orientation. recalc_face_normals alone can settle inward,
    and an inverted tool makes the exact solver produce nonsense."""
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    if bm.calc_volume(signed=True) < 0:
        bmesh.ops.reverse_faces(bm, faces=bm.faces)

def _finish(bm, me, caps, ngon='EAR_CLIP'):
    bmesh.ops.triangulate(bm, faces=caps, quad_method='FIXED', ngon_method=ngon)
    _orient(bm)
    bm.normal_update(); bm.to_mesh(me); bm.free()

def loft(name, loops, coll, ngon='EAR_CLIP'):
    me = bpy.data.meshes.new(name); ob = bpy.data.objects.new(name, me)
    coll.objects.link(ob)
    bm = bmesh.new()
    V = [[bm.verts.new(p) for p in L] for L in loops]
    n = len(V[0])
    for a, b in zip(V, V[1:]):
        for i in range(n):
            try: bm.faces.new((a[i], a[(i + 1) % n], b[(i + 1) % n], b[i]))
            except ValueError: pass
    _finish(bm, me, [bm.faces.new(V[0][::-1]), bm.faces.new(V[-1])], ngon)
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

def body(name, poly, coll, ngon='EAR_CLIP'):
    q = offs(poly, lambda x, y: crown_at(y))
    return loft(name, [
        [(x, y, dz(y)) for x, y in poly],
        [(x, y, P['plate_t'] + (dz(poly[i][1]) - P['plate_t']) * (1 - P['crown']))
         for i, (x, y) in enumerate(q)],
        [(x, y, P['plate_t']) for x, y in q]], coll, ngon)

# One concave loft with BEAUTY caps.  EAR_CLIP on this outline, and the
# MAIN+TAB union, both left self-intersections in the right half.
shell = body('oFOOTS_SHELL', PLAN, COL, ngon='BEAUTY')

# ---- 2. screen plateau, 45 deg sides, rear face flush into the crown
PH = P['pod_h']
BASE = [( TX, TY0), ( TX, TY1 - TC), ( TX - TC, TY1),
        (-(TX - TC), TY1), (-TX, TY1 - TC), (-TX, TY0)]
TOP  = [( TX - PH, TY0 + PH), ( TX - PH, TY1 - TC), ( TX - TC, TY1),
        (-(TX - TC), TY1), (-TX + PH, TY1 - TC), (-TX + PH, TY0 + PH)]
boolean(shell, loft('POD', [[(x, y, dz(y) + PH) for x, y in TOP],
                            [(x, y, dz(y) - 14.0) for x, y in BASE]], TMP), 'UNION')


# ---- 2b. diagonal deck facets: the plateau's side runs out to meet the edge.
# NOT 45 deg - it cannot be.  The plateau is only pod_h tall, so a 45 deg facet
# off it dies 8 mm away.  Sweeping facet_len to the deck edge gives ~9 deg.
# Additive, not subtractive: the deck between plateau and edge is FLAT and
# BELOW the plateau top, so the facet is a fin laid on it, not a cut into it.
def wedge(name, tri):
    """Solid between a sloped triangle and a buried base."""
    me = bpy.data.meshes.new(name); ob = bpy.data.objects.new(name, me)
    TMP.objects.link(ob)
    bm = bmesh.new()
    A = [bm.verts.new(p) for p in tri]
    B = [bm.verts.new((p[0], p[1], dz(p[1]) - 14.0)) for p in tri]
    n = len(tri)
    for i in range(n): bm.faces.new((A[i], A[(i+1) % n], B[(i+1) % n], B[i]))
    bm.faces.new(A[::-1]); bm.faces.new(B)
    bmesh.ops.triangulate(bm, faces=bm.faces[:], quad_method='FIXED', ngon_method='EAR_CLIP')
    _orient(bm)
    bm.normal_update(); bm.to_mesh(me); bm.free()
    return ob

def clip_solid(name):
    """The shell's plan footprint as a tall prism, built from the two convex
    pieces.  Every fin gets intersected with this, so it is impossible for one
    to hang out past the outline as a spike again."""
    a = slab(name + 'A', offs(MAIN, lambda x, y: crown_at(y)), -60.0, 300.0, TMP)
    b = slab(name + 'B', offs(TAB,  lambda x, y: crown_at(y)), -60.0, 300.0, TMP)
    boolean(a, b, 'UNION')
    return a

# Right-angle facet: the right angle sits on the plateau's top side edge, one
# leg runs up that edge, the other reaches the deck's rear corner where the
# 45 deg corner cut begins.  It slopes from plateau top down to deck level and
# STOPS THERE - it does not carry on down the crown, because that plane would
# drop below the deck by x=90 and eat the back row of pixel apertures.
for s_ in (() if 'facets' in SKIP else (1.0, -1.0)):
    fx = s_ * (TX - P['pod_h'])                 # plateau TOP edge, not the tab base
    A  = (fx, TY0 + P['pod_h'], dz(TY0 + P['pod_h']) + P['pod_h'])
    B  = (fx, TY1 - TC,         dz(TY1 - TC)  + P['pod_h'])
    # Pull the outer vertex 3 mm inboard and sink it 0.4 mm under the deck.
    # Landing it exactly on the plan corner AND exactly on the deck plane gave
    # the solver a knife edge tangent to the surface -> non-manifold edges.
    Cc = (s_ * (HX - C - 3.0), YR - 3.0, dz(YR) - 0.4)
    fin = wedge('facet', [A, Cc, B] if s_ > 0 else [A, B, Cc])
    boolean(fin, clip_solid('clip'), 'INTERSECT')
    boolean(shell, fin, 'UNION')

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

# ---- 7b. LAP JOINT along the whole seam.
# The old ear/pad blocks were the wrong idea: below the deck the shell is
# hollow, so blocks near the crack had nothing to attach to and floated, and
# one of them landed outside the tab entirely and ate the boolean.
# The only place the two halves are actually adjacent is the deck itself, so
# the lock lives IN the 4 mm deck: the cut steps sideways at mid-thickness.
# Above the step L stops at the crack; below it L runs LAP past the crack.
# R is the exact complement, so R's deck lands on L's lip along the entire
# seam.  The visible top line is still the crack, unchanged.
LAP = 2.0
JOINFO = [dict(kind='lap along seam', lap_mm=LAP,
               step_at='deck mid-thickness', lip_depth=P['wall'] / 2)]

def lap_prism(dx, name):
    ys = sorted(set([YLO] + [k[0] for k in KNOTS] +
                    [TY0 + P['pod_h'] - 0.05, TY0 + P['pod_h'] + 0.05, TY1, YHI]))
    ys = [y for y in ys if YLO <= y <= YHI]
    me = bpy.data.meshes.new(name); ob = bpy.data.objects.new(name, me)
    TMP.objects.link(ob)
    bm = bmesh.new(); V = []
    for y in ys:
        cx = crack_x(y) + dx
        T = dz(y) + (P['pod_h'] if y > TY0 + P['pod_h'] else 0.0)
        zm = T - P['wall'] / 2.0        # step plane, mid-deck
        sec = [(-900.0, -60.0), (cx + LAP, -60.0), (cx + LAP, zm),
               (cx, zm), (cx, 300.0), (-900.0, 300.0)]
        V.append([bm.verts.new((px, y, pz)) for px, pz in sec])
    for a, b in zip(V, V[1:]):
        for i in range(6): bm.faces.new((a[i], a[(i + 1) % 6], b[(i + 1) % 6], b[i]))
    bm.faces.new(V[0][::-1]); bm.faces.new(V[-1])
    bmesh.ops.triangulate(bm, faces=bm.faces[:], quad_method='FIXED', ngon_method='EAR_CLIP')
    _orient(bm)
    bm.normal_update(); bm.to_mesh(me); bm.free()
    return ob

# ---- 7. split on the crack
def prism(dx, name):
    ys = [YLO] + [k[0] for k in KNOTS] + [YHI]
    xs = [KNOTS[0][1]] + [k[1] for k in KNOTS] + [KNOTS[-1][1]]
    poly = [(-900.0, YLO)] + [(x + dx, y) for x, y in zip(xs, ys)] + [(-900.0, YHI)]
    return slab(name, poly, -60.0, 320.0, TMP)

SL = shell.copy(); SL.data = shell.data.copy(); SL.name = 'SHELL_L'; COL.objects.link(SL)
SR = shell; SR.name = 'SHELL_R'
boolean(SL, lap_prism(-P['gap'], 'pa'), 'INTERSECT')
boolean(SR, lap_prism(+P['gap'], 'pb'), 'DIFFERENCE')

# ---- 7b. interlock across the crack.
# L carries an EAR hanging off its deck and reaching over into R's airspace.
# R carries a PAD sitting 0.3 mm under that ear.  One M3 goes up through the
# pad into the ear, which is solid so the insert burns its own seat.
# The jagged crack already blocks sliding along Y; the screw shank in shear
# blocks the halves pulling apart in X; the screw in tension holds Z.
# All four are reached through the open bottom, before the plate goes on.
# (y, under_plateau, half_length, ear_x0, ear_x1) - offsets are relative to the
# crack at that y.  Placed to clear the encoder bodies (12.4 x 13.2 hanging
# 18 mm below the deck), the 10 x 10 pixel carriers, and the screen window.
# ---- 7c. apertures FIRST, then soften - the reverse order left a
# non-manifold mesh for the boolean and it ate both halves.
WIN = P['win']
cuts = []
def cyl(d, x, y):
    bpy.ops.mesh.primitive_cylinder_add(radius=d / 2, depth=120, vertices=24,
                                        location=(x, y, dz(y)))
    o = bpy.context.object; o.rotation_euler = (math.radians(-P['rake']), 0, 0)
    cuts.append(o)
for x in COLX:
    for y in ROWY: cyl(P['enc_hole'], x, y)
    for y in ROWY: cyl(P['pix_hole'], x, PIXY[y])
WY = (TY0 + P['pod_h'] + TY1) / 2
bpy.ops.mesh.primitive_cube_add(size=1, location=(0, WY, dz(WY) + P['pod_h']))
w = bpy.context.object; w.scale = (WIN, WIN, 120.0)
w.rotation_euler = (math.radians(-P['rake']), 0, 0); cuts.append(w)
bpy.ops.object.select_all(action='DESELECT')
for o in cuts: o.select_set(True)
bpy.context.view_layer.objects.active = cuts[0]
bpy.ops.object.join()
cutter = bpy.context.object
c2 = cutter.copy(); c2.data = cutter.data.copy(); TMP.objects.link(c2)
if 'holes' in SKIP:
    bpy.data.objects.remove(cutter, do_unlink=True); bpy.data.objects.remove(c2, do_unlink=True)
else:
    boolean(SL, cutter, 'DIFFERENCE')
    boolean(SR, c2, 'DIFFERENCE')

# ---- 7d. soften every long edge.  Hole rims are excluded by LENGTH: a 24-gon
# on a 7.4 mm bore has ~0.97 mm edges and the bore walls are wall-thick, both
# far under min_len, so no aperture is touched.  Bevelling before the holes are
# cut would be the other way to guarantee it; this way also protects the window.
def soften(ob, off=0.4, segs=2, min_len=6.0, ang=math.radians(25)):
    bm = bmesh.new(); bm.from_mesh(ob.data)
    wx, wy0, wy1 = P['win'] / 2 + 1.0, WY - P['win'] / 2 - 1.0, WY + P['win'] / 2 + 1.0
    def on_window(e):
        return all(abs(v.co.x) <= wx and wy0 <= v.co.y <= wy1 for v in e.verts)
    es = [e for e in bm.edges
          if e.is_manifold and e.calc_length() > min_len
          and e.calc_face_angle(0.0) > ang and not on_window(e)]
    if es:
        bmesh.ops.bevel(bm, geom=es, offset=off, segments=segs, profile=0.5,
                        affect='EDGES', clamp_overlap=True)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.normal_update(); bm.to_mesh(ob.data); bm.free()

if 'soften' not in SKIP: soften(SL); soften(SR)

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
if 'soften' not in SKIP:
    for ob in (PL, PR): soften(ob, off=0.5)
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
