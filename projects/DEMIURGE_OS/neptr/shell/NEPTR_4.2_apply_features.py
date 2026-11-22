"""NEPTR 4.2 feature rebuild helper (run inside the live Blender session).

Rebuilds a part from its SMOOTH_base_v2 by applying library features
(FEATURES_4.2_CUT / FEATURES_4.2_ADD) in order, with optional group filters.

    exec(open(PATH).read())              # defines the functions below
    o = rebuild('dome', 'tmp_dome')                          # everything
    o = rebuild('dome', 'tmp_dome', exclude_groups=['buttons','encoders'])
    o = rebuild('dome', 'tmp_dome', include_groups=['screen','ports'])
    rep = verify(o, ref=bpy.data.objects['neptr_top_dome_4.2'])

Revert one control type: rebuild(..., exclude_groups=['fsr']) and swap the mesh in.
Feature props: part, op(cut|add), order, batch (int, ascending), group, spec (json).
Per batch: all features of one op are joined into ONE helper, ONE EXACT boolean
(use_self=False, use_hole_tolerant=False), helper deleted. Library objects are
read from matrix_basis (hidden collections -> matrix_world is stale).
"""
import bpy, bmesh, json, math
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

BASES = {'dome': 'dome_SMOOTH_base_v2', 'cap': 'cap_SMOOTH_base_v2'}
LIVE = {'dome': 'neptr_top_dome_4.2', 'cap': 'neptr_bottom_cap'}
LIB = ('FEATURES_4.2_CUT', 'FEATURES_4.2_ADD')
SEG = 48


# ---------------------------------------------------------------- cutter builders
def prism_mesh(name, poly_xy, z0, z1, centre):
    """Watertight triangulated prism; poly in WORLD xy, mesh local = world - centre."""
    cx, cy, cz = centre
    bm = bmesh.new()
    lo = [bm.verts.new((x - cx, y - cy, z0 - cz)) for x, y in poly_xy]
    hi = [bm.verts.new((x - cx, y - cy, z1 - cz)) for x, y in poly_xy]
    n = len(poly_xy)
    bm.faces.new(lo[::-1]); bm.faces.new(hi)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((lo[i], lo[j], hi[j], hi[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me); bm.free()
    return me


PHASE = math.radians(2.9)  # avoids cutter vertices on the base grid axes (slivers at 0/90/180/270 deg)


def circle_poly(cx, cy, d, n=SEG, phase=PHASE):
    R = d / 2 / math.cos(math.pi / n)  # flats are >= nominal diameter
    return [(cx + R * math.cos(phase + 2 * math.pi * i / n), cy + R * math.sin(phase + 2 * math.pi * i / n)) for i in range(n)]


def rect_poly(cx, cy, w, h, ang_deg):
    a = math.radians(ang_deg); c, s = math.cos(a), math.sin(a)
    pts = [(-w / 2, -h / 2), (w / 2, -h / 2), (w / 2, h / 2), (-w / 2, h / 2)]
    return [(cx + x * c - y * s, cy + x * s + y * c) for x, y in pts]


def make_feature(name, part, op, order, batch, group, poly, z0, z1, spec, coll, split_z=None):
    cx = sum(p[0] for p in poly) / len(poly); cy = sum(p[1] for p in poly) / len(poly)
    centre = (cx, cy, (z0 + z1) / 2)
    old = bpy.data.objects.get(name)
    if old: bpy.data.objects.remove(old, do_unlink=True)
    o = bpy.data.objects.new(name, prism_mesh(name, poly, z0, z1, centre))
    o.location = centre
    bpy.data.collections[coll].objects.link(o)
    o.hide_viewport = True; o.hide_render = True
    for k, v in dict(part=part, op=op, order=order, batch=batch, group=group,
                     spec=json.dumps(spec), centre=json.dumps([round(c, 4) for c in centre]),
                     applied=False, split_z=split_z).items():
        if v is not None: o[k] = v
    return o


# ---------------------------------------------------------------- library access
def features(part, include_groups=None, exclude_groups=None, max_batch=None):
    out = []
    for cn in LIB:
        for o in bpy.data.collections[cn].objects:
            if o.get('part') != part or int(o.get('order', 0)) == 0: continue
            g = o.get('group', '')
            if include_groups is not None and g not in include_groups: continue
            if exclude_groups is not None and g in exclude_groups: continue
            if max_batch is not None and int(o.get('batch', 0)) > max_batch: continue
            out.append(o)
    out.sort(key=lambda o: (int(o.get('batch', 0)), 0 if o['op'] == 'add' else 1, int(o['order'])))
    return out


def _helper(objs, name):
    bm = bmesh.new()
    for o in objs:
        t = o.data.copy()
        tmp = bmesh.new(); tmp.from_mesh(t)
        bmesh.ops.transform(tmp, matrix=o.matrix_basis, verts=tmp.verts)
        me2 = bpy.data.meshes.new('_h'); tmp.to_mesh(me2); tmp.free()
        bm.from_mesh(me2)
        bpy.data.meshes.remove(me2); bpy.data.meshes.remove(t)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    h = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(h)
    return h


def _split_boolean(target, objs, op, z):
    """EXACT boolean on the part ABOVE plane z only (closed with a bridged annulus), then weld back.
    Everything below z stays byte-identical: whole-mesh EXACT re-solves float32 rounding
    interpenetrations left by earlier cuts (new strict self-intersections far from the cut)."""
    bm = bmesh.new(); bm.from_mesh(target.data)
    bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], plane_co=(0, 0, z),
                           plane_no=(0, 0, 1), dist=1e-5)
    for v in bm.verts:
        if abs(v.co.z - z) < 2e-4: v.co.z = z
    bm.faces.index_update(); bm.faces.ensure_lookup_table()
    up = [f for f in bm.faces if sum(v.co.z for v in f.verts) / len(f.verts) > z]
    lo = [f for f in bm.faces if sum(v.co.z for v in f.verts) / len(f.verts) <= z]
    bu = bm.copy(); bu.faces.ensure_lookup_table()
    lo_u = [bu.faces[f.index] for f in lo]
    bmesh.ops.delete(bu, geom=lo_u, context='FACES')
    bnd = [e for e in bu.edges if e.is_boundary]
    br = bmesh.ops.bridge_loops(bu, edges=bnd)['faces']
    bmesh.ops.triangulate(bu, faces=br)
    bmesh.ops.recalc_face_normals(bu, faces=bu.faces[:])
    meU = bpy.data.meshes.new('_U'); bu.to_mesh(meU); bu.free()
    U = bpy.data.objects.new('_U', meU); bpy.context.scene.collection.objects.link(U)
    boolean(U, objs, op, split=False)
    bm2 = bmesh.new(); bm2.from_mesh(U.data)
    fill = [f for f in bm2.faces if all(abs(v.co.z - z) < 2e-4 for v in f.verts)]
    bmesh.ops.delete(bm2, geom=fill, context='FACES')
    me2 = bpy.data.meshes.new('_U2'); bm2.to_mesh(me2); bm2.free()
    bpy.data.objects.remove(U, do_unlink=True); bpy.data.meshes.remove(meU)
    bm.faces.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[bm.faces[f.index] for f in up], context='FACES')
    bm.from_mesh(me2); bpy.data.meshes.remove(me2)
    seam = [v for v in bm.verts if abs(v.co.z - z) < 2e-4]
    bmesh.ops.remove_doubles(bm, verts=seam, dist=1e-6)
    bm.to_mesh(target.data); bm.free()
    target.data.update()


def _fix_slivers(bm, box=None, thr=1e-9, passes=6):
    """Remove (near-)zero-area triangles left by EXACT n-gon triangulation (3 collinear verts on a wall edge): the sliver
    A-B-C (B between A and C) and its neighbour A-C-D across the long edge become A-B-D + B-C-D. Manifold, no vertex moved.
    box = (x0, x1, y0, y1) limits the search to slivers inside the slab. Returns the number fixed."""
    fixed = 0
    def in_box(f):
        if box is None: return True
        c = f.calc_center_median()
        return box[0] - 0.5 < c.x < box[1] + 0.5 and box[2] - 0.5 < c.y < box[3] + 0.5
    # (a) two distinct vertices at the same position (EXACT leaves these on cutter-wall edges): weld those verts only
    bm.faces.ensure_lookup_table()
    dv = {v for f in bm.faces if len(f.verts) == 3 and f.calc_area() < thr and in_box(f)
          and min(e.calc_length() for e in f.edges) < 5e-4 for v in f.verts}
    if dv:
        n0 = len(bm.verts)
        bmesh.ops.remove_doubles(bm, verts=list(dv), dist=5e-4)
        fixed += n0 - len(bm.verts)
    # (b) collinear slivers: A-B-C + neighbour A-C-D -> A-B-D + B-C-D
    for _ in range(passes):
        bm.faces.ensure_lookup_table()
        bad = []
        for f in bm.faces:
            if len(f.verts) != 3 or f.calc_area() >= thr: continue
            if box is not None:
                c = f.calc_center_median()
                if not (box[0] - 0.5 < c.x < box[1] + 0.5 and box[2] - 0.5 < c.y < box[3] + 0.5): continue
            bad.append(f)
        if not bad: break
        done = False
        for f in bad:
            if not f.is_valid: continue
            es = sorted(f.edges, key=lambda e: -e.calc_length())
            le = es[0]
            nb = [g for g in le.link_faces if g is not f]
            if len(nb) != 1 or len(nb[0].verts) != 3: continue
            g = nb[0]
            A, C = le.verts
            B = [v for v in f.verts if v not in (A, C)][0]
            D = [v for v in g.verts if v not in (A, C)][0]
            # winding of g: A->C or C->A
            gv = list(g.verts); i = gv.index(A)
            fwd = gv[(i + 1) % 3] is C
            bmesh.ops.delete(bm, geom=[f, g], context='FACES_ONLY')
            try:
                if fwd:  # g was A->C->D, so edge A->C direction
                    bm.faces.new((A, B, D)); bm.faces.new((B, C, D))
                else:    # g was C->A->D
                    bm.faces.new((B, A, D)); bm.faces.new((C, B, D))
            except ValueError:
                pass
            fixed += 1; done = True
        if not done: break
    return fixed


def _box_boolean(target, objs, op, box):
    """EXACT boolean on a LOCAL slab only: faces inside box = (x0, x1, y0, y1, zlo) (centroid test after bisecting the
    mesh on the 4 side planes). The slab is closed by bridging its top/bottom outline loops, solved alone, then welded
    back. Everything outside the box stays byte-identical (a whole-mesh or whole-upper-part EXACT re-solves float32
    rounding noise elsewhere, e.g. the LED-dot shoulder: strict self-intersections far from the feature)."""
    x0, x1, y0, y1, zlo = box
    bm = bmesh.new(); bm.from_mesh(target.data)
    mg = 3.0  # only faces whose bbox overlaps the box (+3) are cut: a whole-mesh bisect slices the entire dome along the plane (slivers, +9k tris)
    def near_faces():
        out = []
        for f in bm.faces:
            xs = [v.co.x for v in f.verts]; ys = [v.co.y for v in f.verts]; zs = [v.co.z for v in f.verts]
            if max(xs) >= x0 - mg and min(xs) <= x1 + mg and max(ys) >= y0 - mg and min(ys) <= y1 + mg and max(zs) > zlo: out.append(f)
        return out
    for co, no in (((x0, 0, 0), (1, 0, 0)), ((x1, 0, 0), (1, 0, 0)), ((0, y0, 0), (0, 1, 0)), ((0, y1, 0), (0, 1, 0))):
        nf = near_faces()
        ge = list({e for f in nf for e in f.edges}); gv = list({v for f in nf for v in f.verts})
        bmesh.ops.bisect_plane(bm, geom=gv + ge + nf, plane_co=co, plane_no=no, dist=1e-5)
    ng = [f for f in bm.faces if len(f.verts) > 3]   # bisect leaves quads/n-gons (cut pieces, and neighbours that got a collinear vertex)
    if ng: bmesh.ops.triangulate(bm, faces=ng, quad_method='BEAUTY', ngon_method='BEAUTY')
    def snap(v):
        c = v.co
        for ax, val in ((0, x0), (0, x1), (1, y0), (1, y1)):
            if abs(c[ax] - val) < 2e-4: c[ax] = val
    for v in bm.verts: snap(v)
    bm.verts.index_update()
    def inside(f):
        c = f.calc_center_median()
        return x0 < c.x < x1 and y0 < c.y < y1 and c.z > zlo
    bm.faces.index_update(); bm.faces.ensure_lookup_table()
    ins = [f.index for f in bm.faces if inside(f)]
    insset = set(ins)
    bu = bm.copy(); bu.faces.ensure_lookup_table()
    bmesh.ops.delete(bu, geom=[bu.faces[f.index] for f in bm.faces if f.index not in insset], context='FACES')
    bnd = [e for e in bu.edges if e.is_boundary]
    br = bmesh.ops.bridge_loops(bu, edges=bnd)['faces']
    bmesh.ops.triangulate(bu, faces=br)
    bmesh.ops.recalc_face_normals(bu, faces=bu.faces[:])
    meU = bpy.data.meshes.new('_U'); bu.to_mesh(meU); bu.free()
    U = bpy.data.objects.new('_U', meU); bpy.context.scene.collection.objects.link(U)
    boolean(U, objs, op, split=False)
    bm2 = bmesh.new(); bm2.from_mesh(U.data)
    def onplane(v): return any(abs(v.co[ax] - val) < 2e-4 for ax, val in ((0, x0), (0, x1), (1, y0), (1, y1)))
    fill = [f for f in bm2.faces if all(onplane(v) for v in f.verts) and abs(f.normal.z) < 0.5]
    bmesh.ops.delete(bm2, geom=fill, context='FACES')
    me2 = bpy.data.meshes.new('_U2'); bm2.to_mesh(me2); bm2.free()
    bpy.data.objects.remove(U, do_unlink=True); bpy.data.meshes.remove(meU)
    bm.faces.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[bm.faces[i] for i in ins], context='FACES')
    bm.from_mesh(me2); bpy.data.meshes.remove(me2)
    seam = [v for v in bm.verts if any(abs(v.co[ax] - val) < 2e-4 for ax, val in ((0, x0), (0, x1), (1, y0), (1, y1)))]
    bmesh.ops.remove_doubles(bm, verts=seam, dist=1e-6)
    ng2 = [f for f in bm.faces if len(f.verts) > 3 and x0 - 0.5 < f.calc_center_median().x < x1 + 0.5 and y0 - 0.5 < f.calc_center_median().y < y1 + 0.5]
    if ng2: bmesh.ops.triangulate(bm, faces=ng2, quad_method='BEAUTY', ngon_method='BEAUTY')   # EXACT output n-gons with collinear verts tessellate to zero-area tris
    _fix_slivers(bm, (x0, x1, y0, y1))
    bm.to_mesh(target.data); bm.free()
    target.data.update()


def _puck_boolean(target, objs, op, puck):
    """EXACT boolean on a flat PATCH of one planar face region (e.g. the seam face z=12.4 around a blind pilot hole).
    puck = (cx, cy, half, z_plane, z_top). Faces lying in z=z_plane whose centroid is inside the (cx+-half, cy+-half) box
    are bisected out, closed into a free-standing puck (walls extruded to z_top + cap), solved alone, then the
    new patch is welded back. Everything else stays byte-identical. Use when the cutter never touches anything but the
    planar face (a slab that includes skin/blocks bridged wrongly: 400+ non-manifold edges per corner)."""
    cx, cy, hw, zp, zt = puck
    x0, x1, y0, y1 = cx - hw, cx + hw, cy - hw, cy + hw
    bm = bmesh.new(); bm.from_mesh(target.data)
    def planar(f): return all(abs(v.co.z - zp) < 2e-4 for v in f.verts)
    def near():
        out = []
        for f in bm.faces:
            if not planar(f): continue
            xs = [v.co.x for v in f.verts]; ys = [v.co.y for v in f.verts]
            if max(xs) >= x0 - 1 and min(xs) <= x1 + 1 and max(ys) >= y0 - 1 and min(ys) <= y1 + 1: out.append(f)
        return out
    for co, no in (((x0, 0, 0), (1, 0, 0)), ((x1, 0, 0), (1, 0, 0)), ((0, y0, 0), (0, 1, 0)), ((0, y1, 0), (0, 1, 0))):
        nf = near()
        ge = list({e for f in nf for e in f.edges}); gv = list({v for f in nf for v in f.verts})
        bmesh.ops.bisect_plane(bm, geom=gv + ge + nf, plane_co=co, plane_no=no, dist=1e-5)
    ng = [f for f in bm.faces if len(f.verts) > 3]
    if ng: bmesh.ops.triangulate(bm, faces=ng, quad_method='BEAUTY', ngon_method='BEAUTY')
    def onbox(v): return any(abs(v.co[ax] - val) < 2e-4 for ax, val in ((0, x0), (0, x1), (1, y0), (1, y1)))
    for v in bm.verts:
        for ax, val in ((0, x0), (0, x1), (1, y0), (1, y1)):
            if abs(v.co[ax] - val) < 2e-4: v.co[ax] = val
    bm.faces.index_update(); bm.faces.ensure_lookup_table()
    def inside(f):
        c = f.calc_center_median()
        return planar(f) and x0 < c.x < x1 and y0 < c.y < y1
    ins = [f.index for f in bm.faces if inside(f)]; insset = set(ins)
    bu = bm.copy(); bu.faces.ensure_lookup_table()
    bmesh.ops.delete(bu, geom=[bu.faces[f.index] for f in bm.faces if f.index not in insset], context='FACES')
    bnd = [e for e in bu.edges if e.is_boundary]
    ex = bmesh.ops.extrude_edge_only(bu, edges=bnd)['geom']
    nv = [g for g in ex if isinstance(g, bmesh.types.BMVert)]
    for v in nv: v.co.z = zt
    top_edges = [e for e in bu.edges if all(abs(v.co.z - zt) < 1e-6 for v in e.verts) and e.is_boundary]
    cap = bmesh.ops.holes_fill(bu, edges=top_edges, sides=len(top_edges))['faces']
    bmesh.ops.triangulate(bu, faces=cap)
    bmesh.ops.recalc_face_normals(bu, faces=bu.faces[:])
    meU = bpy.data.meshes.new('_U'); bu.to_mesh(meU); bu.free()
    U = bpy.data.objects.new('_U', meU); bpy.context.scene.collection.objects.link(U)
    boolean(U, objs, op, split=False)
    bm2 = bmesh.new(); bm2.from_mesh(U.data)
    def junk(f):
        vs = f.verts
        if all(abs(v.co.z - zt) < 2e-4 for v in vs): return True
        return any(all(abs(v.co[ax] - val) < 2e-4 for v in vs) for ax, val in ((0, x0), (0, x1), (1, y0), (1, y1)))
    bmesh.ops.delete(bm2, geom=[f for f in bm2.faces if junk(f)], context='FACES')
    me2 = bpy.data.meshes.new('_U2'); bm2.to_mesh(me2); bm2.free()
    bpy.data.objects.remove(U, do_unlink=True); bpy.data.meshes.remove(meU)
    bm.faces.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[bm.faces[i] for i in ins], context='FACES')
    bm.from_mesh(me2); bpy.data.meshes.remove(me2)
    seam = [v for v in bm.verts if abs(v.co.z - zp) < 2e-4 and onbox(v)]
    bmesh.ops.remove_doubles(bm, verts=seam, dist=1e-6)
    bm.to_mesh(target.data); bm.free()
    target.data.update()


def boolean(target, objs, op, split=True):
    """One EXACT boolean of the joined library objects into target (in place).
    If the first object has a split_z prop the boolean runs on the part above that plane only."""
    sp_ = objs[0].get('split_puck')
    if split and sp_ is not None:
        return _puck_boolean(target, objs, op, json.loads(sp_))
    sb = objs[0].get('split_box')
    if split and sb is not None:
        return _box_boolean(target, objs, op, json.loads(sb))
    sz = objs[0].get('split_z')
    if split and sz is not None:
        return _split_boolean(target, objs, op, float(sz))
    h = _helper(objs, '_helper_tmp')
    m = target.modifiers.new('bool', 'BOOLEAN')
    m.operation = 'UNION' if op == 'add' else 'DIFFERENCE'
    m.operand_type = 'OBJECT'; m.object = h
    m.solver = 'EXACT'; m.use_self = False; m.use_hole_tolerant = False
    vl = bpy.context.view_layer
    with bpy.context.temp_override(object=target, active_object=target, selected_objects=[target]):
        bpy.ops.object.modifier_apply(modifier=m.name)
    me = h.data; bpy.data.objects.remove(h, do_unlink=True); bpy.data.meshes.remove(me)


def ensure_tris(obj):
    if any(len(p.vertices) != 3 for p in obj.data.polygons):
        bm = bmesh.new(); bm.from_mesh(obj.data)
        bmesh.ops.triangulate(bm, faces=bm.faces[:], quad_method='BEAUTY', ngon_method='BEAUTY')
        bm.to_mesh(obj.data); bm.free()


def apply_features(obj, feats):
    """Apply already-sorted features to obj: consecutive same (batch, op) = one boolean."""
    i = 0
    while i < len(feats):
        k = (int(feats[i].get('batch', 0)), feats[i]['op']); grp = []
        while i < len(feats) and (int(feats[i].get('batch', 0)), feats[i]['op']) == k:
            grp.append(feats[i]); i += 1
        boolean(obj, grp, k[1])


def rebuild(part, name, include_groups=None, exclude_groups=None, max_batch=None, base=None):
    src = bpy.data.objects[base or BASES[part]]
    obj = bpy.data.objects.new(name, src.data.copy())
    bpy.context.scene.collection.objects.link(obj)
    ensure_tris(obj)
    apply_features(obj, features(part, include_groups, exclude_groups, max_batch))
    return obj


# ---------------------------------------------------------------- verification
def _arrays(obj):
    me = obj.data; me.calc_loop_triangles()
    V = np.empty(len(me.vertices) * 3); me.vertices.foreach_get('co', V); V = V.reshape(-1, 3)
    T = np.empty(len(me.loop_triangles) * 3, np.int64); me.loop_triangles.foreach_get('vertices', T)
    return V, T.reshape(-1, 3)


def _pierce(P, Q, tri, eps=1e-5):
    """segment P->Q strictly through triangle tri (n,3,3); float64 Moller-Trumbore."""
    a, b, c = tri[:, 0], tri[:, 1], tri[:, 2]
    d = Q - P; e1 = b - a; e2 = c - a
    h = np.cross(d, e2); det = np.einsum('ij,ij->i', e1, h)
    ok = np.abs(det) > 1e-14
    inv = np.where(ok, 1.0 / np.where(ok, det, 1), 0)
    s = P - a; u = np.einsum('ij,ij->i', s, h) * inv
    q = np.cross(s, e1); v = np.einsum('ij,ij->i', d, q) * inv
    t = np.einsum('ij,ij->i', e2, q) * inv
    return ok & (u > eps) & (v > eps) & (u + v < 1 - eps) & (t > eps) & (t < 1 - eps)


def selfint(V, T, eps=1e-5):
    bvh = BVHTree.FromPolygons(V.tolist(), T.tolist())
    pr = np.array(bvh.overlap(bvh), dtype=np.int64).reshape(-1, 2)
    pr = pr[pr[:, 0] < pr[:, 1]]
    A, B = T[pr[:, 0]], T[pr[:, 1]]
    share = (A[:, :, None] == B[:, None, :]).any(axis=(1, 2))
    pr = pr[~share]; raw = len(pr)
    A, B = T[pr[:, 0]], T[pr[:, 1]]
    hit = np.zeros(raw, bool)
    for X, Y in ((A, B), (B, A)):
        tri = V[Y]
        for e in range(3):
            hit |= _pierce(V[X[:, e]], V[X[:, (e + 1) % 3]], tri, eps)
    return int(hit.sum()), raw


def topology(V, T):
    nT = len(T)
    ed = np.sort(np.concatenate([T[:, [0, 1]], T[:, [1, 2]], T[:, [2, 0]]]), axis=1)
    _, cnt = np.unique(ed[:, 0] * len(V) + ed[:, 1], return_counts=True)
    used = np.zeros(len(V), bool); used[T.ravel()] = True
    area = 0.5 * np.linalg.norm(np.cross(V[T[:, 1]] - V[T[:, 0]], V[T[:, 2]] - V[T[:, 0]]), axis=1)
    # components via union-find on vertices
    par = np.arange(len(V))
    def find(x):
        while par[x] != x:
            par[x] = par[par[x]]; x = par[x]
        return x
    for a, b in np.unique(ed, axis=0):
        ra, rb = find(a), find(b)
        if ra != rb: par[ra] = rb
    comps = len({find(v) for v in np.where(used)[0]})
    return dict(verts=len(V), tris=nT, comps=comps, nonmanifold=int((cnt != 2).sum()),
                loose=int((~used).sum()), zero_area=int((area < 1e-9).sum()))


def _bvh(V, T): return BVHTree.FromPolygons(V.tolist(), T.tolist())


def parity(bvh, step=6.0):
    odd = 0; D = Vector((0, 0, -1))
    def cnt(p, d):
        n = 0; p = Vector(p)
        for _ in range(60):
            h = bvh.ray_cast(p, d)
            if h[0] is None: return n
            n += 1; p = h[0] + d * 1e-4
        return n
    xs = np.arange(-126.1, 126.1, step); ys = np.arange(-106.1, 106.1, step); zs = np.arange(0.2, 65, step)
    for x in xs:
        for y in ys:
            odd += cnt((x + 0.013, y + 0.007, 100), D) % 2
    z_odd = odd; odd = 0
    for x in xs:
        for z in zs:
            odd += cnt((x + 0.013, -150, z + 0.007), Vector((0, 1, 0))) % 2
    return z_odd, odd


def topz(bvh, xs, ys):
    out = np.full((len(ys), len(xs)), np.nan); D = Vector((0, 0, -1))
    for i, y in enumerate(ys):
        for j, x in enumerate(xs):
            h = bvh.ray_cast(Vector((x, y, 100)), D)
            if h[0] is not None: out[i, j] = h[0].z
    return out


def hull_inside(poly, pt, m=0.0):
    # convex polygon (ccw or cw) point test
    s = []
    for i in range(len(poly)):
        a, b = poly[i], poly[(i + 1) % len(poly)]
        s.append((b[0] - a[0]) * (pt[1] - a[1]) - (b[1] - a[1]) * (pt[0] - a[0]))
    return all(v >= -m for v in s) or all(v <= m for v in s)


def footprints_of(feat_objs):
    """xy outlines (world) of the cutter top caps, convex."""
    out = []
    for o in feat_objs:
        M = o.matrix_basis
        pts = [(M @ v.co) for v in o.data.vertices]
        zt = max(p.z for p in pts)
        top = [(p.x, p.y) for p in pts if p.z > zt - 1e-6]
        # order by angle around centroid
        cx = sum(p[0] for p in top) / len(top); cy = sum(p[1] for p in top) / len(top)
        top.sort(key=lambda p: math.atan2(p[1] - cy, p[0] - cx))
        out.append(top)
    return out


def verify(obj, ref=None, new_feats=None, step_skin=1.0, parity_step=6.0):
    V, T = _arrays(obj)
    rep = topology(V, T)
    rep['selfint_strict'], rep['selfint_raw_touch'] = selfint(V, T)
    bvh = _bvh(V, T)
    rep['parity_odd_z_y'] = parity(bvh, parity_step)
    if new_feats:
        fps = footprints_of(new_feats)
        # holes open: coprime sample count + phase, rays from above must miss (cavity open below)
        bad = 0; tot = 0; D = Vector((0, 0, -1))
        for fp in fps:
            cx = sum(p[0] for p in fp) / len(fp); cy = sum(p[1] for p in fp) / len(fp)
            rmax = max(math.hypot(p[0] - cx, p[1] - cy) for p in fp)
            for k in range(37):
                a = 2 * math.pi * (k + 0.31) / 37
                for f in (0.3, 0.6, 0.9):
                    pt = (cx + f * rmax * math.cos(a), cy + f * rmax * math.sin(a))
                    if not hull_inside(fp, pt, -0.05): continue
                    tot += 1
                    if bvh.ray_cast(Vector((pt[0], pt[1], 100)), D)[0] is not None: bad += 1
        rep['holes_open'] = f'{tot - bad}/{tot}'
        if ref is not None:
            Vr, Tr = _arrays(ref); br = _bvh(Vr, Tr)
            xs = np.arange(-125, 125.01, step_skin); ys = np.arange(-105, 105.01, step_skin)
            za, zb = topz(bvh, xs, ys), topz(br, xs, ys)
            X, Y = np.meshgrid(xs, ys); mask = np.zeros(X.shape, bool)
            for fp in fps:
                xm = (X > min(p[0] for p in fp) - 0.8) & (X < max(p[0] for p in fp) + 0.8) & \
                     (Y > min(p[1] for p in fp) - 0.8) & (Y < max(p[1] for p in fp) + 0.8)
                mask |= xm
            d = np.abs(np.nan_to_num(za, nan=-1) - np.nan_to_num(zb, nan=-1))
            rep['skin_diff_outside_footprints_max'] = float(d[~mask].max())
            rep['skin_diff_cells_gt_0.01'] = int((d[~mask] > 0.01).sum())
    return rep


def mesh_diff(a, b, n=40000, seed=1):
    """symmetric nearest-surface distance between two objects (sample verts vs other's BVH)."""
    Va, Ta = _arrays(a); Vb, Tb = _arrays(b)
    ba, bb = _bvh(Va, Ta), _bvh(Vb, Tb)
    rng = np.random.default_rng(seed); res = []
    for V, other in ((Va, bb), (Vb, ba)):
        idx = rng.choice(len(V), min(n, len(V)), replace=False)
        d = [other.find_nearest(Vector(V[i]))[3] for i in idx]
        res.append((float(np.max(d)), float(np.mean(d))))
    return res


# ---------------------------------------------------------------- control features (2026-10-15)
# dome top is flat (z 62 / ceiling 59.6) over every control -> vertical axes, 2.4 mm panel.
Z0, Z1 = 57.0, 64.0
SPLIT_Z = 50.0  # control cuts only touch the top region; screen pegs end at 51.7
CONTROLS = {
    'buttons':  dict(batch=7, order0=80, d=7.4,
                     pts={'L1': (-44, 0), 'L2': (-55, 11), 'L3': (-55, -11), 'L4': (-66, 0),
                          'R1': (44, 0), 'R2': (55, 11), 'R3': (55, -11), 'R4': (66, 0)},
                     hw='panel pushbutton, D7.4 clearance, 2.4 mm flat panel'),
    'encoders': dict(batch=8, order0=90, d=7.5,
                     pts={'L2': (-85.4, -65.4), 'L1': (-42.7, -65.4), 'C': (0, -65.4),
                          'R1': (42.7, -65.4), 'R2': (85.4, -65.4)},  # y -60.4 -> -65.4 (2026-10-15 coordinator)
                     hw='panel rotary encoder, D7.5 bushing hole, 2.4 mm panel (<= usable thread), no anti-rotation tab in old cutters'),
    'joystick': dict(batch=10, order0=110, d=27.0, pts={'': (-62.5, 42.0)},
                     hw='KY-023 joystick (plan.md: cap OD ~25, panel hole D27) over cap supports 20x26 posts; centre (-62.5,42) = old holes, between peg axis -62 and post-box centre -63'),
    'arcade':   dict(batch=12, order0=130, d=30.7, pts={'': (62.5, 42.0)},
                     hw='30 mm snap-in arcade button (EG STARTS, BTN9); old Phase-4 dome hole D30.7 at (+62.5,42) (plan.md says D28.5 -> edit d here if the button is tight); ring/clip OD ~38, body ~30 deep: clear to z 20'),
    'lidar':    dict(batch=11, order0=120, d=10.5, pts={'L': (-25.7, 67.2), 'R': (25.7, 67.2)},
                     hw='LiDAR sensor window D10.5, 2.4 mm panel'),
}
FSR = dict(batch=9, order0=100, w=6.0, h=8.0, ang=10.0, pts={'L': (-42.1, -37.6, +10.0), 'R': (42.1, -37.6, -10.0)})
# FSR: old rotated cutter = 6 x 8 rectangle, long axis 10 deg off +Y (R: toward -x going +y; L mirrored)
# rect_poly angle is CCW rotation of the box (w along x, h along y): R -> +10 deg, L -> -10 deg wrt the 'h along y' box.


PHASES = {'buttons': 0.9, 'encoders': 0.9}  # group -> cutter polygon phase in degrees (tuned per group to avoid collinear needle faces)


def create_control_features(only=None):
    made = []
    for grp, c in CONTROLS.items():
        if only and grp not in only: continue
        for i, (k, (x, y)) in enumerate(c['pts'].items()):
            nm = f"dome_cut_{grp}_{k}".rstrip('_') if grp not in ('buttons', 'encoders') else \
                 f"dome_cut_{'btn' if grp == 'buttons' else 'enc'}_{k}"
            if grp == 'joystick': nm = 'dome_cut_joystick_hole'
            if grp == 'arcade': nm = 'dome_cut_arcade_hole'
            if grp == 'lidar': nm = f'dome_cut_lidar_{k}'
            made.append(make_feature(nm, 'dome', 'cut', c['order0'] + i, c['batch'], grp,
                        circle_poly(x, y, c['d'], phase=math.radians(PHASES.get(grp, math.degrees(PHASE)))), Z0, Z1,
                        dict(d=c['d'], x=x, y=y, z0=Z0, z1=Z1, phase_deg=PHASES.get(grp, round(math.degrees(PHASE), 3)), depth='through top skin 59.6..62', axis='vertical (top flat, tilt 0)', hardware=c['hw']),
                        'FEATURES_4.2_CUT', split_z=SPLIT_Z))
    for i, (k, (x, y, a)) in enumerate(FSR['pts'].items() if (not only or 'fsr' in only) else []):
        # old corners R: (39.84,-42.06)(45.749,-41.018)(44.36,-33.14)(38.451,-34.182) = 6 x 8 box rotated; sign: R long axis tilts to -x
        ang = -a  # a=+10 for L means long axis toward +x going +y => box rotated by -10 about z
        made.append(make_feature(f'dome_cut_fsr_{k}', 'dome', 'cut', FSR['order0'] + i, FSR['batch'], 'fsr',
                    rect_poly(x, y, FSR['w'], FSR['h'], ang), Z0, Z1,
                    dict(w=6.0, h=8.0, rot_deg=ang, x=x, y=y, z0=Z0, z1=Z1, axis='vertical', hardware='FSR active-area slot 6x8, rotated 10 deg (old CUT_FSR_rotated_v3)'),
                    'FEATURES_4.2_CUT', split_z=SPLIT_Z))
    return made


# ---------------------------------------------------------------- batch try / commit
def try_batch(part, group, name='WORK'):
    live = bpy.data.objects[LIVE[part]]
    w = bpy.data.objects.new(name, live.data.copy()); bpy.context.scene.collection.objects.link(w)
    fs = features(part, include_groups=[group])
    apply_features(w, fs)
    rep = verify(w, ref=live, new_feats=[f for f in fs if f['op'] == 'cut'])
    return w, rep


def commit(part, work, note):
    import re
    live = bpy.data.objects[LIVE[part]]
    pre = LIVE[part] + '_backup_v'
    n = max([int(re.findall(r'_v(\d+)$', o.name)[0]) for o in bpy.data.objects if o.name.startswith(pre)] + [0]) + 1
    live.data = work.data
    bpy.data.objects.remove(work, do_unlink=True)
    bk = bpy.data.objects.new(f'{pre}{n:02d}', live.data.copy())
    bpy.data.collections['BACKUPS_4.2'].objects.link(bk); bk['note'] = note
    return bk.name  # caller: mark features applied, bpy.data.orphans_purge() when done


# ---------------------------------------------------------------- LiDAR brackets (2026-10-15, group 'lidar_mount')
# TOF400C VL53L1X module (plan.md): PCB 20 x 12, 2x D2 (M2) holes 16 apart on the long axis, long axis along X.
# Bracket = two M2 self-tap posts per sensor (hollow: blind D1.7 pilot from the tip, ends 0.4 under the ceiling), grown
# from the ceiling (dome prints top skin on the bed -> posts grow up from the bed, axis within ANGLE of vertical = no
# supports). The PCB (component side up) is screwed from below onto the post tips; the sensor axis passes through the
# middle of the D10.5 window skin (z = hole_mid_z). ONE add batch. A bore that ends AT the ceiling plane (or a separate
# pilot cut batch) left non-manifold edges on the bore/ceiling circle after any later boolean: tried, rejected.
# ONE value to change the aim: LIDAR['angle_deg'] (+ = tilted OUTWARD, away from the centre line; 0 = straight up),
# then  create_lidar_features(); w = rebuild-or-try_batch('dome', 'lidar_mount') ...
LIDAR = dict(
    angle_deg=15.0,       # aim, degrees from vertical in the XZ plane, + = outward (L toward -x, R toward +x); 15 = old STAGE13 tilt, coordinator choice 2026-10-15
    centres={'L': (-25.7, 67.2), 'R': (25.7, 67.2)},
    hole_mid_z=60.8,      # axis crosses the middle of the window skin (ceiling 59.6 .. top 62)
    pcb_depth=4.6,        # PCB top face (component side = seat plane) -> hole_mid plane, along the axis (seat z 56.36 at the centre)
    pcb_xy=(20.0, 12.0), pcb_t=1.6,
    sensor_h=1.9,         # aperture body above PCB top (ASSUMED)
    # 2026-10-18: flat seat replaces the M2 posts. Pad grown from the ceiling, underside = plane normal to the sensor axis.
    pocket=(20.4, 12.4, 0.8),   # locating pocket u x v x depth (board 20 x 12, 0.2 play per side); floor = seat plane t=0, lip tips t=-depth
    pad_wall=1.2,               # lip thickness on the 3 closed sides
    open_end='inboard',         # connector side, no lip (ASSUMED: wire side toward the centre line); pad ends flush with the board there
    aperture=(12.6, 8.6),       # stadium clearance for the sensor housing (u x v), constant section along the axis
    clear_r=5.65, clear_z=(59.0, 62.0),  # vertical cylinder keeping the pad out of the D10.5 window (r 5.25) above z 59
    pad_top_z=60.6,             # pad embedded 1.0 into the 2.4 skin, outer skin untouched
    phase=7.3, ap_seg=24,
    batch=14, order=150,
    split_box=(-42.0, 42.0, 57.43, 77.07, 50.0),   # local-slab EXACT (see _box_boolean)
)


def lidar_pose(side, L=None):
    L = L or LIDAR
    cx, cy = L['centres'][side]; th = math.radians(L['angle_deg']); sx = 1.0 if side == 'R' else -1.0
    a = np.array([sx * math.sin(th), 0.0, math.cos(th)])          # sensor axis (up)
    u = np.array([math.cos(th), 0.0, -sx * math.sin(th)])         # PCB long axis
    H = np.array([cx, cy, L['hole_mid_z']])
    C = H - a * L['pcb_depth']                                     # PCB top-face centre
    return dict(a=a, u=u, C=C, H=H, ry_deg=sx * L['angle_deg'])


def _ext_prism(name, pts, E):
    """Closed prism: base polygon pts (3D, planar, simple) extruded by vector E."""
    bm = bmesh.new()
    lo = [bm.verts.new(tuple(float(c) for c in p)) for p in pts]
    hi = [bm.verts.new(tuple(float(c) for c in (np.array(p) + E))) for p in pts]
    n = len(pts)
    bm.faces.new(lo[::-1]); bm.faces.new(hi)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((lo[i], lo[j], hi[j], hi[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bmesh.ops.triangulate(bm, faces=bm.faces[:], quad_method='BEAUTY', ngon_method='BEAUTY')
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(o)
    return o


def _solve(a, b, op):
    m = a.modifiers.new('bool', 'BOOLEAN'); m.operation = op; m.operand_type = 'OBJECT'; m.object = b
    m.solver = 'EXACT'; m.use_self = False; m.use_hole_tolerant = False
    with bpy.context.temp_override(object=a, active_object=a, selected_objects=[a]):
        bpy.ops.object.modifier_apply(modifier=m.name)
    me = b.data; bpy.data.objects.remove(b, do_unlink=True); bpy.data.meshes.remove(me)


def lidar_seat_geometry(side, L=None):
    """Seat pad for one sensor as a closed solid in world coordinates (scratch object, caller owns it).
    t = distance along the sensor axis from the seat plane (PCB component face); u = board long axis; v = y."""
    L = L or LIDAR
    P = lidar_pose(side, L); a, u, C, H = P['a'], P['u'], P['C'], P['H']
    V = np.array([0.0, 1.0, 0.0]); sx = 1.0 if side == 'R' else -1.0
    s_in = -sx                                   # sign along u of the inboard direction
    pw, pl, pd = L['pocket'][1], L['pocket'][0], L['pocket'][2]
    wall = L['pad_wall']
    closed = -s_in if L['open_end'] == 'inboard' else s_in
    openu = -closed
    pt = lambda uu, vv, tt: C + uu * u + vv * V + tt * a
    def quad(u0, u1, v0, v1, t): return [pt(u0, v0, t), pt(u1, v0, t), pt(u1, v1, t), pt(u0, v1, t)]
    # pad: footprint closed side = pocket half + wall, open side flush with the board end, +-(pocket half + wall) wide
    uc = closed * (pl / 2 + wall); uo = openu * (pl / 2)
    pad = _ext_prism('_pad', quad(uc, uo, -(pw / 2 + wall), pw / 2 + wall, -pd), a * 10.0)
    box = _ext_prism('_zbox', [np.array(q) for q in ((-60, 40, 40.0), (60, 40, 40.0), (60, 90, 40.0), (-60, 90, 40.0))], np.array([0, 0, L['pad_top_z'] - 40.0]))
    _solve(pad, box, 'INTERSECT')
    pocket = _ext_prism('_pocket', quad(closed * pl / 2, openu * (pl / 2 + 3.0), -pw / 2, pw / 2, -pd - 0.4), a * (pd + 0.4))
    _solve(pad, pocket, 'DIFFERENCE')
    # stadium aperture along the axis (u long)
    al, aw = L['aperture']; r = aw / 2; cx_ = al / 2 - r; n = L['ap_seg']; ph = math.radians(L['phase'])
    poly = []
    for k in range(n + 1):
        t_ = ph + math.pi * (-0.5 + k / n); poly.append((cx_ + r * math.cos(t_), r * math.sin(t_)))
    for k in range(n + 1):
        t_ = ph + math.pi * (0.5 + k / n); poly.append((-cx_ + r * math.cos(t_), r * math.sin(t_)))
    ap = _ext_prism('_ap', [pt(x_, y_, -pd - 0.4) for x_, y_ in poly], a * 14.0)
    _solve(pad, ap, 'DIFFERENCE')
    cyl = _ext_prism('_clear', [np.array([H[0] + L['clear_r'] / math.cos(math.pi / 48) * math.cos(ph + 2 * math.pi * k / 48), H[1] + L['clear_r'] / math.cos(math.pi / 48) * math.sin(ph + 2 * math.pi * k / 48), L['clear_z'][0]]) for k in range(48)],
                     np.array([0, 0, L['clear_z'][1] - L['clear_z'][0]]))
    _solve(pad, cyl, 'DIFFERENCE')
    return pad, dict(a=a, u=u, C=C, H=H, closed=closed, open=openu)


def create_lidar_features(L=None):
    """(Re)creates dome_add_lidar_seat_L/R (flat seat pad + locating pocket + clearance aperture) and removes the superseded posts."""
    L = L or LIDAR
    for nm in ('dome_add_lidar_bracket_L', 'dome_add_lidar_bracket_R', 'dome_cut_lidar_pilot_L', 'dome_cut_lidar_pilot_R'):
        old = bpy.data.objects.get(nm)
        if old:
            m = old.data; bpy.data.objects.remove(old, do_unlink=True); bpy.data.meshes.remove(m)
    made = []
    for side in ('L', 'R'):
        pad, I = lidar_seat_geometry(side, L)
        nm = f'dome_add_lidar_seat_{side}'
        old = bpy.data.objects.get(nm)
        if old:
            m = old.data; bpy.data.objects.remove(old, do_unlink=True); bpy.data.meshes.remove(m)
        bm = bmesh.new(); bm.from_mesh(pad.data)
        bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3], quad_method='BEAUTY', ngon_method='BEAUTY')
        co = np.array([v.co[:] for v in bm.verts]); centre = tuple(float(x) for x in 0.5 * (co.min(0) + co.max(0)))
        bmesh.ops.translate(bm, vec=(-centre[0], -centre[1], -centre[2]), verts=bm.verts[:])
        me = bpy.data.meshes.new(nm); bm.to_mesh(me); bm.free()
        pm = pad.data; bpy.data.objects.remove(pad, do_unlink=True); bpy.data.meshes.remove(pm)
        o = bpy.data.objects.new(nm, me); o.location = centre
        bpy.data.collections['FEATURES_4.2_ADD'].objects.link(o)
        o.hide_viewport = True; o.hide_render = True
        P = lidar_pose(side, L)
        spec = dict(kind='lidar flat seat', side=side, angle_deg=L['angle_deg'], axis=[round(float(x), 6) for x in I['a']],
                    pcb_long_axis=[round(float(x), 6) for x in I['u']], pcb_top_centre=[round(float(x), 4) for x in I['C']],
                    hole_mid=[round(float(x), 4) for x in I['H']], ry_deg=P['ry_deg'], sensor_h=L['sensor_h'],
                    pocket=list(L['pocket']), pad_wall=L['pad_wall'], aperture=list(L['aperture']), clear_r=L['clear_r'],
                    pad_top_z=L['pad_top_z'], open_end=L['open_end'], open_dir_u=float(I['open']), phase_deg=L['phase'],
                    hardware='TOF400C VL53L1X 20x12 board, component side against the pocket floor (seat plane), glue/tape, no screws; sensor housing through the stadium aperture; connector side open')
        for k, v in dict(part='dome', op='add', order=L['order'] + (0 if side == 'L' else 1), batch=L['batch'], group='lidar_mount',
                         spec=json.dumps(spec), centre=json.dumps([round(c, 4) for c in centre]), applied=False,
                         split_box=json.dumps(list(L['split_box']))).items():
            o[k] = v
        made.append(o)
    return made


# ---------------------------------------------------------------- north encoders ENC6/ENC7 (2026-10-15, restored from the old Phase-4 dome)
# firmware (plan.md pin map) has ENC1-7; the 4.2 dome only had the 5 at y -65.4. The old FINAL dome had D7.5 holes at (+-85.4, +75).
# top skin is flat there (z 62.000 to y 78, 61.99 at the hole's north edge); LED dot holes >= 12.7 from the centre.
ENC_N = dict(d=7.5, pts={'L2N': (-85.4, 75.0), 'R2N': (85.4, 75.0)}, batches={'L2N': 15, 'R2N': 16}, orders={'L2N': 170, 'R2N': 171},
             box_half=(8.0, 7.0), phase=0.9)


def create_north_encoders(E=None):
    E = E or ENC_N
    made = []
    for k, (x, y) in E['pts'].items():
        hx, hy = E['box_half']
        o = make_feature(f'dome_cut_enc_{k}', 'dome', 'cut', E['orders'][k], E['batches'][k], 'encoders_n',
                         circle_poly(x, y, E['d'], phase=math.radians(E['phase'])), Z0, Z1,
                         dict(d=E['d'], x=x, y=y, z0=Z0, z1=Z1, axis='vertical', hardware='KY-040 encoder bushing hole D7.5, ENC6/ENC7 (plan.md), restored from old Phase-4 dome (+-85.4,+75)'),
                         'FEATURES_4.2_CUT', split_z=None)
        o['split_box'] = json.dumps([x - hx, x + hx, y - hy, y + hy, 50.0])
        made.append(o)
    return made
