"""neuralGrid housing - flared shell body + screw-on base plate.

Form: flat 144 mm top (pads + corner lidar apertures, both facing straight up)
-> R12 fillet rolling 0 deg -> 22.5 deg -> 20 mm flat facet at 22.5 deg where the
SoftPots live -> R15 blend 22.5 -> 45 deg -> straight 45 deg to a 236 mm base rim.
Base plate is separate, screwed up into 4 bosses on an internal flange ring.

Built in phases; set RUN_TO in the exec globals (1..6) to stop early.
Units: millimetres. Author: emgor.
"""
import bpy, bmesh, math

try:
    RUN_TO
except NameError:
    RUN_TO = 6

P = dict(
    half_flat   = 74.0,    # top face half-width -> 148 mm square
                           # (2 mm wider than the holes need, to buy corner radius)
    z_top       = 52.0,    # top face. Set by the Ethernet connector: the flat
                           # band on the back wall is z_top - 24.476, and the
                           # M23 needs ~27 mm of it for hole + seat ring.
    z_rim       = 4.0,     # bottom rim (top of the base plate)
    fillet_r    = 15.0,    # rolls the flat top into the 22.5 deg facet
    facet_ang   = 22.5,    # SoftPot bed angle, from horizontal
    facet_len   = 18.0,    # slope length of the flat SoftPot facet
                           # 16.63 mm of horizontal run for a 12.7 mm ribbon
    lower_r     = 14.0,    # fillet rolling the SoftPot facet over
    lower_ang   = 88.0,    # ...into a near-vertical side. The rim half-width is
                           # DERIVED from these, not targeted: the shell drops
                           # essentially straight down off the facet edge.
    corner_k    = 0.26,    # corner radius as a fraction of half-width.
                           # Now capped by the SoftPots, not the lidars: a flat
                           # ribbon cannot bend around a corner, so the facet's
                           # straight run must stay >= the 113 mm span.
    wall        = 4.0,     # true perpendicular wall thickness, uniform
    base_plate  = 4.0,
    base_edge_r = 2.0,     # round-over on the base plate's ground edge
    # --- Trellis backer plate (separate printed part) ---
    trellis_depth = 4.0,   # top plate underside -> bottom of the NeoTrellis
    backer_size = 122.0,   # >= the 120 mm tiled block, 1 mm proud each side
    backer_r    = 8.0,
    backer_t    = 4.0,
    pillar_top_pos  = 52.0,   # pillar centres where they meet the plate
    pillar_foot_pos = 61.0,   # ...and where they land on the floor: they splay
    pillar_top_r    = 5.0,
    pillar_foot_r   = 7.0,
    # --- back-panel connectors, on the +Y wall, axis normal to it ---
    # A flat makes a hole SMALLER on one side, so an unnecessary flat blocks a
    # round part instead of merely failing to key it. Only the power connector's
    # drawing specifies a real one (14.6 across a 15.6 circle); the Ethernet's
    # 22.00 vs 21.99 is effectively round. All three are round here until the
    # test plate confirms the power key's orientation.
    eth_dia     = 22.0,    # M23x1.5 Ethernet, RIGHT  (drawing 1)
    eth_flat    = 0.0,     # 0 = fully round
    usb_dia     = 22.55,   # USB, LEFT               (drawing 2)
    usb_flat    = 0.0,
    pwr_dia     = 15.6,    # power, MIDDLE           (drawing 3)
    pwr_flat    = 0.0,     # set 14.6 to key it, once the key angle is known
    # Clearance is per connector, not shared: the power hole is CONFIRMED from
    # the test plate (rung 2 = 15.8 = +0.2). The other two stay provisional at
    # +0.4 until their parts arrive.
    # All three CONFIRMED on the test plate.
    # Ethernet only just went into rung 7 (23.2), so it gets two steps more.
    # That also confirms the drawing's "22,00" was never the panel cutout: it is
    # an M23x1.5 thread, so a hole in the 23s is exactly where it belongs.
    eth_clear   = 1.6,     # -> 23.6  (rung 7 fitted tight, +2 steps for a real fit)
    usb_clear   = 0.25,    # -> 22.8  (rung 5)
    pwr_clear   = 0.2,     # -> 15.8  (rung 2)
    conn_depth  = 40.0,    # cutter length along the wall normal
    # --- intake vents: uniform vertical slots on the pad-column centrelines.
    # Vertical, not horizontal: a long horizontal slot forces a 120 mm bridge
    # on the layer above it. Vertical slots only bridge their own 3 mm width.
    vent_w      = 3.0,
    vent_h      = 12.0,
    vent_r      = 1.5,     # stadium ends
    vent_z      = 20.0,    # centre; clears the gusset (z<=10) and the fillet (z>=31.5)
    vent_depth  = 24.0,
    vent_walls  = (1, 2, 3),          # W, S(front), E  - back stays solid
    flange_h    = 6.0,     # gusset ring stiffening the bottom rim
    flange_in   = 96.0,    # its inner half-width
    pad_size    = 34.0,    # local corner pad carrying the insert
    pad_r       = 8.0,
    pad_top     = 16.0,
    flange_bite = 1.5,     # how far the flange reaches into the wall
    clip_inset  = 1.0,     # bosses stop this far short of the outer skin
    boss_pos    = 88.0,    # screw axis, both axes. Held inboard so the insert
                           # is not driven up against the outer skin.
    insert_od   = 6.0,     # M4 heat-set insert, 8.0 mm long
    insert_len  = 8.0,
    boss_pilot  = 0.0,     # solid, as specified. 5.6 gives an M4 insert pilot.
    screw_clear = 4.5,     # M4 shank clearance
    csk_dia     = 8.4,     # M4 countersunk head, 90 deg included
    csk_depth   = 1.95,    # (csk_dia - screw_clear) / 2 -> exactly 90 deg
)
S = 0.001

sc = bpy.context.scene
sc.unit_settings.system = 'METRIC'
sc.unit_settings.scale_length = 0.001
sc.unit_settings.length_unit = 'MILLIMETERS'


# ------------------------------------------------------------------ profile
def profile(fil=10, fac=4, bln=12, tail=6):
    """Meridian from the top edge down to the bottom rim: [(r, z, theta_deg)]."""
    pts = []
    Rf, A = P['fillet_r'], P['facet_ang']
    cx, cz = P['half_flat'], P['z_top'] - Rf
    for k in range(fil + 1):
        th = A * k / fil
        a = math.radians(90.0 - th)
        pts.append((cx + Rf * math.cos(a), cz + Rf * math.sin(a), th))

    r0, z0 = pts[-1][0], pts[-1][1]
    ca, sa = math.cos(math.radians(A)), math.sin(math.radians(A))
    for k in range(1, fac + 1):
        L = P['facet_len'] * k / fac
        pts.append((r0 + L * ca, z0 - L * sa, A))

    # Roll the facet over on a tight fillet and then drop essentially straight
    # down. A single large arc sweeping 22.5 deg -> vertical cannot gain less
    # than ~18 mm of radius over this drop, which is what made the base 236 mm.
    # A small fillet plus a near-vertical run gets the same visual break for a
    # fraction of the footprint.
    r1, z1 = pts[-1][0], pts[-1][1]
    ta, te = math.radians(A), math.radians(P['lower_ang'])
    Rb = P['lower_r']
    ccx, ccz = r1 - Rb * math.sin(ta), z1 - Rb * math.cos(ta)
    for k in range(1, bln + 1):
        t = ta + (te - ta) * k / bln
        pts.append((ccx + Rb * math.sin(t), ccz + Rb * math.cos(t), math.degrees(t)))

    r2, z2 = pts[-1][0], pts[-1][1]
    dz = z2 - P['z_rim']
    for k in range(1, tail + 1):
        d = dz * k / tail
        pts.append((r2 + d / math.tan(te), z2 - d, P['lower_ang']))
    return pts


def offset_in(pts, w):
    """True normal offset of the meridian, inward. Keeps wall thickness uniform
    even where the surface is nearly horizontal (a horizontal offset would give
    zero thickness at the flat top)."""
    out = []
    for r, z, th in pts:
        t = math.radians(th)
        out.append((r - w * math.sin(t), z - w * math.cos(t), th))
    return out


# ------------------------------------------------------------------ geometry
def rr(half, r, seg=16):
    r = max(min(r, half - 1e-6), 1e-6)
    h = half - r
    pts = []
    for cx, cy, a0 in ((h, h, 0.0), (-h, h, math.pi / 2),
                       (-h, -h, math.pi), (h, -h, 3 * math.pi / 2)):
        for k in range(seg + 1):
            a = a0 + (math.pi / 2) * k / seg
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def circle(d, seg=48):
    return [((d / 2) * math.cos(2 * math.pi * k / seg),
             (d / 2) * math.sin(2 * math.pi * k / seg)) for k in range(seg)]


def loft(name, rings, col):
    bm = bmesh.new()
    layers = [[bm.verts.new((x * S, y * S, z * S)) for x, y in prof] for z, prof in rings]
    bm.verts.ensure_lookup_table()
    n = len(rings[0][1])
    for k in range(len(layers) - 1):
        a, b = layers[k], layers[k + 1]
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((a[i], a[j], b[j], b[i]))
    bm.faces.new(layers[0])
    bm.faces.new(layers[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    col.objects.link(ob)
    return ob


def prism(name, prof, z0, z1, loc, col):
    a = [(x + loc[0], y + loc[1]) for x, y in prof]
    return loft(name, [(z0, a), (z1, a)], col)


def sweep(name, meridian, col, cap_top_half=None):
    rings = [(z, rr(r, r * P['corner_k'])) for r, z, th in meridian]
    return loft(name, rings, col)


def boolean(obj, cutter, op='DIFFERENCE'):
    m = obj.modifiers.new("bool", 'BOOLEAN')
    m.operation = op
    m.object = cutter
    m.solver = 'EXACT'
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=m.name)


def solid_span(obj, x_mm, y_mm, z_hi=60.0, z_lo=-10.0):
    """Total solid thickness the screw axis passes through, by raycast.
    Reasoning about which volumes overlap is not proof; this measures it."""
    import mathutils
    hits = []
    z = z_hi
    d = mathutils.Vector((0.0, 0.0, -1.0))
    for _ in range(40):
        ok, loc, nrm, idx = obj.ray_cast(
            mathutils.Vector((x_mm * S, y_mm * S, z * S)), d)
        if not ok:
            break
        zh = loc.z * 1000.0
        if zh > z - 1e-6:
            zh = z - 1e-4
        hits.append(zh)
        z = zh - 1e-3
        if z < z_lo:
            break
    hits = sorted(set(round(h, 4) for h in hits), reverse=True)
    spans = []
    for i in range(0, len(hits) - 1, 2):
        spans.append((round(hits[i + 1], 3), round(hits[i], 3),
                      round(hits[i] - hits[i + 1], 3)))
    return spans


def check(obj, label):
    me = obj.data
    bm = bmesh.new()
    bm.from_mesh(me)
    nonman = [e for e in bm.edges if len(e.link_faces) != 2]
    loose = [v for v in bm.verts if not v.link_edges]
    bm.free()
    vol = None
    try:
        bm2 = bmesh.new()
        bm2.from_mesh(me)
        vol = bm2.calc_volume(signed=True) * 1e9   # mm^3
        bm2.free()
    except Exception:
        pass
    return dict(part=label, verts=len(me.vertices), faces=len(me.polygons),
                non_manifold_edges=len(nonman), loose_verts=len(loose),
                volume_mm3=None if vol is None else round(vol, 1),
                watertight=(len(nonman) == 0 and len(loose) == 0))


# ------------------------------------------------------------------ build
for cn in ("NG_SHELL",):
    c = bpy.data.collections.get(cn)
    if c:
        for o in list(c.objects):
            bpy.data.objects.remove(o, do_unlink=True)
        bpy.data.collections.remove(c)
shellcol = bpy.data.collections.new("NG_SHELL")
sc.collection.children.link(shellcol)

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
    """Panel cutout for a round connector. `flat_across` is measured from the
    flat to the far side of the arc; pass 0 (or >= dia) for a full circle.
    Never add a flat a part does not have - it shrinks the hole and blocks it."""
    if flat_across <= 0.0 or flat_across >= dia - 1e-6:
        return ellipse(dia, dia, seg)
    R = dia / 2.0
    d = max(min(flat_across - R, R - 1e-4), -R + 1e-4)   # chord offset from centre
    a = math.asin(d / R)
    t0, t1 = math.pi - a, 2 * math.pi + a                # the arc BELOW the chord
    return [(R * math.cos(t0 + (t1 - t0) * k / seg),
             R * math.sin(t0 + (t1 - t0) * k / seg)) for k in range(seg + 1)]


def half_at_z(mer, z):
    """Interpolate a meridian's half-width at height z (meridian runs top->bottom)."""
    for i in range(len(mer) - 1):
        z1, z2 = mer[i][1], mer[i + 1][1]
        if z1 >= z >= z2 and z1 != z2:
            t = (z1 - z) / (z1 - z2)
            return mer[i][0] + t * (mer[i + 1][0] - mer[i][0])
    return mer[-1][0]


MER = profile()
REPORT = []

# --- phase 1: outer solid
outer = sweep("SHELL_body", MER, shellcol)
outer_ref = sweep("TMP_outer_ref", MER, shellcol)
REPORT.append(check(outer, "phase1_outer"))

if RUN_TO >= 2:
    inner_mer = offset_in(MER, P['wall'])
    # extend below the rim so the bottom is genuinely open
    r_last, z_last, th_last = inner_mer[-1]
    inner_mer.append((r_last + 6.0 / math.tan(math.radians(th_last)), z_last - 6.0, th_last))
    inner = sweep("TMP_inner", inner_mer, shellcol)
    REPORT.append(check(inner, "phase2_cavity_tool"))
    boolean(outer, inner, 'DIFFERENCE')
    bpy.data.objects.remove(inner, do_unlink=True)
    REPORT.append(check(outer, "phase2_shell"))


# ------------------------------------------------------------------ cutters
# Hole sizes mirror make_cutters.py; placement is owned here because the
# SoftPot slots now live on the 22.5 deg facet, not on a flat plane.
C = dict(
    pad_open=12.6, pad_pitch=15.0, pad_radius=2.0,
    slot_len=13.0, slot_wid=6.0, slot_radius=2.5,
    slot_clear=0.4,          # CONFIRMED on the test plate (rung 2)
    span_total=113.0,
    oval_x=7.0, oval_y=10.0,
    lidar_pos=67.0,          # radial distance; lidars sit at the EDGE MIDPOINTS
    cut_depth=12.0,
)


if RUN_TO >= 3:
    # Never clip a tool against the outer skin: the result shares faces with the
    # shell and the exact solver produces non-manifold garbage. Clip against a
    # surface held `clip_inset` inside the skin instead, so every union is a
    # clean volumetric overlap.
    clip_ref = sweep("TMP_clip_ref", offset_in(MER, P['clip_inset']), shellcol)

    # --- flange: a band that follows the inner wall and bites `flange_bite`
    # into it, so it is fused all the way round rather than merely touching.
    fo = offset_in(MER, P['wall'] - P['flange_bite'])
    zb0, zb1 = P['z_rim'], P['z_rim'] + P['flange_h']
    h0, h1 = half_at_z(fo, zb0), half_at_z(fo, zb1)
    fl = loft("TMP_flange", [(zb0, rr(h0, h0 * P['corner_k'])),
                             (zb1, rr(h1, h1 * P['corner_k']))], shellcol)
    hole = prism("TMP_flange_in", rr(P['flange_in'], P['flange_in'] * P['corner_k']),
                 zb0 - 1.0, zb1 + 1.0, (0, 0), shellcol)
    boolean(fl, hole, 'DIFFERENCE')
    bpy.data.objects.remove(hole, do_unlink=True)
    boolean(outer, fl, 'UNION')
    bpy.data.objects.remove(fl, do_unlink=True)
    REPORT.append(check(outer, "phase3_flange"))

    # Local corner pads. Each is clipped to the shell envelope and unioned, so
    # it fuses into the sloping wall over its full height rather than
    # cantilevering off it. This is the material the insert burns into.
    B = P['boss_pos']
    padprof = rrect(P['pad_size'], P['pad_size'], P['pad_r'])
    for nm, loc in (("NE", (B, B)), ("NW", (-B, B)), ("SW", (-B, -B)), ("SE", (B, -B))):
        b = prism("TMP_pad_%s" % nm, padprof, P['z_rim'], P['pad_top'], loc, shellcol)
        boolean(b, clip_ref, 'INTERSECT')
        boolean(outer, b, 'UNION')
        bpy.data.objects.remove(b, do_unlink=True)
    if P['boss_pilot'] > 0:
        for nm, loc in (("NE", (B, B)), ("NW", (-B, B)), ("SW", (-B, -B)), ("SE", (B, -B))):
            h = prism("TMP_pilot_%s" % nm, ellipse(P['boss_pilot'], P['boss_pilot']),
                      P['z_rim'] - 1, P['z_rim'] + P['insert_len'] + 1.0, loc, shellcol)
            boolean(outer, h, 'DIFFERENCE')
            bpy.data.objects.remove(h, do_unlink=True)
    REPORT.append(check(outer, "phase3_corner_pads"))

if RUN_TO >= 4:
    from mathutils import Matrix, Vector

    # Every cutter that is actually subtracted is kept, in its own hidden
    # collection, so the hole geometry is always recoverable from the file.
    usedcol = bpy.data.collections.get("NG_CUT_USED")
    if usedcol:
        for o in list(usedcol.objects):
            bpy.data.objects.remove(o, do_unlink=True)
        bpy.data.collections.remove(usedcol)
    usedcol = bpy.data.collections.new("NG_CUT_USED")
    sc.collection.children.link(usedcol)

    cutters = []

    def local_prism(name, prof, depth):
        z0, z1 = -depth / 2.0, depth / 2.0
        return loft(name, [(z0, prof), (z1, prof)], usedcol)

    def place(ob, X, Y, Z, origin_mm):
        ob.matrix_world = Matrix((
            (X[0], Y[0], Z[0], origin_mm[0] * S),
            (X[1], Y[1], Z[1], origin_mm[1] * S),
            (X[2], Y[2], Z[2], origin_mm[2] * S),
            (0.0, 0.0, 0.0, 1.0)))

    ZC = P['z_top'] - 0  # cutters on the flat top are centred on the face
    # --- 64 pad squares (vertical, flat top). The grid is already 90 deg symmetric.
    sq = rrect(C['pad_open'], C['pad_open'], C['pad_radius'])
    for gy in range(8):
        for gx in range(8):
            cx = (gx - 3.5) * C['pad_pitch']
            cy = (3.5 - gy) * C['pad_pitch']
            cutters.append(prism("CUTX_pad_x%d_y%d" % (gx, gy), sq,
                                 ZC - C['cut_depth'] / 2, ZC + C['cut_depth'] / 2,
                                 (cx, cy), usedcol))

    # --- 4 lidar ovals (vertical, flat top, alternating 0/90 => C4 symmetry)
    # Lidars at the EDGE MIDPOINTS, long axis running ALONG each edge (parallel
    # to the SoftPot it shares a side with). Still vertical in the flat top, so
    # the sensors look straight up. Placement is C4 symmetric by construction.
    # The 7 mm radial extent also buys clearance both ways: 3.5 mm to the
    # Trellis block and 3.5 mm to the edge of the flat.
    L = C['lidar_pos']
    for nm, loc, (ow, oh) in (("N", (0, L), (C['oval_y'], C['oval_x'])),
                              ("W", (-L, 0), (C['oval_x'], C['oval_y'])),
                              ("S", (0, -L), (C['oval_y'], C['oval_x'])),
                              ("E", (L, 0), (C['oval_x'], C['oval_y']))):
        cutters.append(prism("CUTX_lidar_%s" % nm, ellipse(ow, oh),
                             ZC - C['cut_depth'] / 2, ZC + C['cut_depth'] / 2,
                             loc, usedcol))

    # --- 4 SoftPot tail slots, lying in the 22.5 deg facet.
    # Facet midline, from the meridian: start of facet -> end of facet.
    fs = [m for m in MER if abs(m[2] - P['facet_ang']) < 1e-6]
    r_mid = (fs[0][0] + fs[-1][0]) / 2.0
    z_mid = (fs[0][1] + fs[-1][1]) / 2.0
    A = math.radians(P['facet_ang'])
    # Clearance widens the slot, so the tail hole slides inboard to keep the
    # far edge of the slot exactly on the 113 mm span.
    sl = C['slot_len'] + C['slot_clear']
    sw = C['slot_wid'] + C['slot_clear']
    H = C['span_total'] / 2.0 - sw / 2.0
    slotprof = rrect(sl, sw, C['slot_radius'])
    for k, nm in enumerate(("N", "W", "S", "E")):
        a = math.radians(90.0 * k)
        ca, sa = math.cos(a), math.sin(a)
        u = Vector((-sa, ca, 0.0))        # outward, rotates N -> W -> S -> E
        v = Vector((ca, sa, 0.0))         # along the edge
        n = (math.sin(A) * u + Vector((0, 0, math.cos(A)))).normalized()
        d = (math.cos(A) * u - Vector((0, 0, math.sin(A)))).normalized()
        Yv = n.cross(d)
        o = r_mid * u + Vector((0, 0, z_mid)) + H * v
        ob = local_prism("CUTX_softpot_%s" % nm, slotprof, C['cut_depth'])
        place(ob, d, Yv, n, (o.x, o.y, o.z))
        cutters.append(ob)

    # --- back panel: 3 connectors on the +Y wall, in the flat 88 deg band.
    # That band is genuinely planar along a straight edge of the rounded square,
    # so a hole cut normal to it gets a truly flat seat for the nut.
    LA = math.radians(P['lower_ang'])
    z_hi = min(m[1] for m in MER if abs(m[2] - P['lower_ang']) < 1e-6)
    z_hi = max(m[1] for m in MER if abs(m[2] - P['lower_ang']) < 1e-6)
    CZ = (z_hi + P['z_rim']) / 2.0                      # centre of the flat band
    r_at = half_at_z(MER, CZ)
    straight_half = r_at - r_at * P['corner_k']        # half the flat run
    # Quarters of the SHELL, not of the flat run: at quarters of the flat run
    # the three holes bunch into the middle ~55% with dead space either side.
    QX = r_at / 2.0
    _big = max(P['eth_dia'] + P['eth_clear'], P['usb_dia'] + P['usb_clear'])
    assert QX + _big / 2 < straight_half, "hole runs off the flat"
    assert P['z_rim'] + _big / 2 < CZ < z_hi - _big / 2, "hole runs off the flat band"

    n = Vector((0.0, math.sin(LA), math.cos(LA)))       # outward normal of the wall
    d = Vector((0.0, math.cos(LA), -math.sin(LA)))      # down the wall
    Yv = n.cross(d)
    def cut_prof(dia, flat, cl):
        return dhole(dia + cl, (flat + cl) if flat > 0 else 0.0)
    # Viewed from outside the back: Ethernet right (-X), power middle, USB left.
    CONN = (("eth",   -QX, cut_prof(P['eth_dia'], P['eth_flat'], P['eth_clear'])),
            ("power",  0.0, cut_prof(P['pwr_dia'], P['pwr_flat'], P['pwr_clear'])),
            ("usb",   +QX, cut_prof(P['usb_dia'], P['usb_flat'], P['usb_clear'])))
    for nm, x, prof in CONN:
        ob = local_prism("CUTX_conn_%s" % nm, prof, P['conn_depth'])
        # Basis must be RIGHT-handed or the prism inverts and the boolean
        # silently does nothing. (Yv, d, n) has determinant -1; (Yv, -d, n)
        # is +1 and also puts the flat at the top of the hole.
        u = -d
        ob.matrix_world = Matrix(((Yv[0], u[0], n[0], x * S),
                                  (Yv[1], u[1], n[1], r_at * S),
                                  (Yv[2], u[2], n[2], CZ * S),
                                  (0.0, 0.0, 0.0, 1.0)))
        cutters.append(ob)
    CONN_INFO = dict(band_top=round(z_hi, 4), band_height=round(z_hi - P['z_rim'], 4),
                     centre_z=round(CZ, 4), wall_r=round(r_at, 4),
                     quarter_x=round(QX, 4), flat_half=round(straight_half, 4))

    # --- vents. Same wall basis as the connectors; the N wall (k=0) is skipped
    # because the connector caps eat its whole flat run.
    VZ = P['vent_z']
    r_v = half_at_z(MER, VZ)
    ventprof = rrect(P['vent_w'], P['vent_h'], P['vent_r'])
    VPOS = (-52.5, -37.5, -22.5, -7.5, 7.5, 22.5, 37.5, 52.5)   # pad columns
    for k in P['vent_walls']:
        a = math.radians(90.0 * k)
        ca, sa = math.cos(a), math.sin(a)
        u = Vector((-sa, ca, 0.0))
        n2 = (math.sin(LA) * u + Vector((0, 0, math.cos(LA)))).normalized()
        d2 = (math.cos(LA) * u - Vector((0, 0, math.sin(LA)))).normalized()
        Y2 = n2.cross(d2)
        up = -d2
        for i, pv in enumerate(VPOS):
            o = r_v * u + Vector((0, 0, VZ)) + pv * Y2
            ob = local_prism("CUTX_vent_%d_%d" % (k, i), ventprof, P['vent_depth'])
            ob.matrix_world = Matrix(((Y2[0], up[0], n2[0], o.x * S),
                                      (Y2[1], up[1], n2[1], o.y * S),
                                      (Y2[2], up[2], n2[2], o.z * S),
                                      (0.0, 0.0, 0.0, 1.0)))
            cutters.append(ob)
    VENT_INFO = dict(wall_r=round(r_v, 3), z=VZ, n_walls=len(P['vent_walls']),
                     per_wall=len(VPOS),
                     area_mm2=round(len(P['vent_walls']) * len(VPOS) *
                                    (P['vent_w'] * P['vent_h'] -
                                     (4 - math.pi) * P['vent_r']**2), 1))

    # One boolean for every hole, using throwaway copies so the originals stay.
    dups = []
    for c_ in cutters:
        dcp = c_.copy(); dcp.data = c_.data.copy()
        sc.collection.objects.link(dcp); dups.append(dcp)
    bpy.ops.object.select_all(action='DESELECT')
    for dcp in dups:
        dcp.select_set(True)
    bpy.context.view_layer.objects.active = dups[0]
    bpy.ops.object.join()
    joined = bpy.context.view_layer.objects.active
    joined.name = "TMP_cutters"
    REPORT.append(check(joined, "phase4_cutter_tool"))
    boolean(outer, joined, 'DIFFERENCE')
    bpy.data.objects.remove(joined, do_unlink=True)
    REPORT.append(check(outer, "phase4_shell_punched"))

if RUN_TO >= 5:
    for _t in ('TMP_outer_ref', 'TMP_clip_ref'):
        _o = bpy.data.objects.get(_t)
        if _o:
            bpy.data.objects.remove(_o, do_unlink=True)

    # Base plate with a round-over on its ground edge - the one edge a hand
    # actually touches. The top edge stays square so it mates flush to the rim.
    half_b = MER[-1][0]
    re = P['base_edge_r']
    brings = []
    for k in range(9):
        a = math.radians(90.0 * k / 8)
        h = (half_b - re) + re * math.sin(a)
        brings.append((re - re * math.cos(a), rr(h, h * P['corner_k'])))
    brings.append((P['base_plate'], rr(half_b, half_b * P['corner_k'])))
    base = loft("SHELL_base", brings, shellcol)

    # One lofted stepped solid per screw: countersink + shank bore in a single
    # watertight tool. Two separate cylinders subtracted in sequence leaves a
    # sliver face where their walls meet. The 90 deg cone is also self-supporting,
    # unlike a flat counterbore ceiling, which would have to bridge.
    rs = P['screw_clear'] / 2.0
    rc = P['csk_dia'] / 2.0
    csk = [(-1.0, ellipse(P['csk_dia'], P['csk_dia'])),
           (0.0, ellipse(P['csk_dia'], P['csk_dia'])),
           (P['csk_depth'], ellipse(P['screw_clear'], P['screw_clear'])),
           (P['base_plate'] + 1.0, ellipse(P['screw_clear'], P['screw_clear']))]
    B = P['boss_pos']
    for nm, loc in (("NE", (B, B)), ("NW", (-B, B)), ("SW", (-B, -B)), ("SE", (B, -B))):
        rings = [(z, [(x + loc[0], y + loc[1]) for x, y in prof]) for z, prof in csk]
        t = loft("TMP_csk_%s" % nm, rings, shellcol)
        boolean(base, t, 'DIFFERENCE')
        bpy.data.objects.remove(t, do_unlink=True)
    REPORT.append(check(base, "phase5_base"))

e = bpy.data.objects.get("NG_SHELL_PARAMS")
if e:
    bpy.data.objects.remove(e, do_unlink=True)
e = bpy.data.objects.new("NG_SHELL_PARAMS", None)
e.empty_display_type = 'PLAIN_AXES'
for k, v in list(P.items()) + list(C.items()):
    e[k] = v
shellcol.objects.link(e)

if RUN_TO >= 6:
    # ---------------- Trellis backer: a flat plate the buttons press against,
    # standing on four splayed pillars that land on the inside of the base.
    # Printed as one piece, plate down: the pillars lean outward going up at
    # ~20 deg from vertical, so nothing overhangs.
    bcol = bpy.data.collections.get("NG_BACKER")
    if bcol:
        for o in list(bcol.objects):
            bpy.data.objects.remove(o, do_unlink=True)
        bpy.data.collections.remove(bcol)
    bcol = bpy.data.collections.new("NG_BACKER")
    sc.collection.children.link(bcol)

    # The top plate is just the uniform wall, so its underside is z_top - wall.
    BT = P['z_top'] - P['wall'] - P['trellis_depth']        # 32.0: backer top face
    BB = BT - P['backer_t']                                 # 28.0: backer underside
    FL = P['z_rim']                                         # 4.0 : inside of the base

    plate = prism("TRELLIS_BACKER", rrect(P['backer_size'], P['backer_size'], P['backer_r']),
                  BB, BT, (0, 0), bcol)

    # Pillars run from the floor up into the plate (2 mm of overlap, so the
    # union is volumetric and never a coplanar face-to-face touch).
    PT, PF = P['pillar_top_pos'], P['pillar_foot_pos']
    RT, RF = P['pillar_top_r'], P['pillar_foot_r']
    ZA, ZB = FL, BB + 2.0
    for sx, sy in ((1, 1), (-1, 1), (-1, -1), (1, -1)):
        rings = []
        for k in range(13):
            f = k / 12.0
            z = ZA + (ZB - ZA) * f
            g_ = (z - FL) / (BB - FL)
            c = PF + (PT - PF) * g_
            r = RF + (RT - RF) * g_
            rings.append((z, [(x + sx * c, y + sy * c) for x, y in ellipse(2 * r, 2 * r, 32)]))
        pil = loft("TMP_pillar_%d%d" % (sx, sy), rings, bcol)
        boolean(plate, pil, 'UNION')
        bpy.data.objects.remove(pil, do_unlink=True)

    REPORT.append(check(plate, "phase6_backer"))

# Keep every cutter in the file but out of sight: the flat reference set and
# the set actually subtracted. Recoverable, never visible.
for _cn in ("NG_CUTTERS", "NG_CUT_USED"):
    _cc = bpy.data.collections.get(_cn)
    if _cc:
        _cc.hide_viewport = True
        _cc.hide_render = True
        for _o in _cc.all_objects:
            _o.hide_viewport = True
            _o.hide_render = True
            _o.hide_select = True
    _lc = bpy.context.view_layer.layer_collection.children.get(_cn)
    if _lc:
        _lc.hide_viewport = True
        _lc.exclude = True