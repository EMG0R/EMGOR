"""NEPTR 4.2 fit coupon generator (run in the live Blender session after NEPTR_4.2_apply_features.py).

    exec(compile(open('NEPTR_4.2_apply_features.py').read(),'a','exec'), globals())
    exec(compile(open('NEPTR_4.2_fit_coupon.py').read(),'c','exec'), globals())
    obj = build_coupon()          # returns the joined object (not linked to a visible collection)

Flat plate 160 x 110 x 2.4 (= the dome skin), printed OUTER face down (bed), so the 0.5 mm engraved labels and the
LiDAR seat pad are on the top. Hole cutters are the library cutters themselves (same mesh as the dome), moved to a slot;
wall-port cutters are turned so their axis is the plate normal. Plus two seam pieces cut from the REAL parts around the
(+89.75, -89.76) fastener corner: cap corner (z 0-14: boss, bore, recess) and dome corner (z 12.4-26: rim, corner block,
skirt) - stack dome piece on cap piece to test the seam and the M3 stack. Seam pieces print as modelled (rim on the bed).
Final frame: plate z' = 62 - z, y' = -y (proper 180 deg rotation about x).
"""
import bpy, bmesh, math, json
import numpy as np
from mathutils import Vector

PLATE = (160.0, 110.0, 2.4)
# slots in the FINAL frame (x, y). y' = -y of the model, but slots are defined directly here.
SLOTS = dict(
    arcade=(-58, 32), joystick=(-22, 32), hdmi=(14, 32), eth=(47, 32),
    usbc=(-62, 0), lidar=(-33, 0), fsr=(-5, 0), trs=(12, 0), jack=(28, 0), button=(43, 0), encoder=(57, 0),
    usbA=(-55, -32), dots=(-20, -32),
)
LABELS = dict(arcade='30.7', joystick='27', hdmi='21.5', eth='24.2 D', usbc='16.3', lidar='10.5+SEAT', fsr='6x8', trs='TRS 10',
              jack='8.3', button='7.4', encoder='7.5', usbA='USB-A 7x15', dots='LED 0.9/1.3')
LABEL_DY = dict(arcade=-19, joystick=-17, hdmi=-15, eth=-16, usbc=-12, lidar=-11, fsr=-8, trs=-8, jack=-8, button=-8, encoder=-8, usbA=-19, dots=-12)


def _world_verts(o):
    M = o.matrix_basis
    return [M @ v.co for v in o.data.vertices]


def _mesh_from(o, fn, name):
    """copy of library object o with every vertex mapped by fn(world_vec)->vec; faces preserved (flip if det<0 is avoided by proper maps)."""
    me = bpy.data.meshes.new(name)
    vs = [fn(p) for p in _world_verts(o)]
    me.from_pydata([tuple(v) for v in vs], [], [tuple(p.vertices) for p in o.data.polygons])
    me.update()
    return me


def _top_cutter(name, slot, final_z0=-1.0):
    o = bpy.data.objects[name]
    cx, cy = o.location.x, o.location.y
    return _mesh_from(o, lambda p: Vector((slot[0] + (p.x - cx), slot[1] - (p.y - cy), 62.0 - p.z)), name + '_c')


def _wall_cutter(name, slot, zmid=1.2):
    """axis-y library cutter -> axis-z: x'=dx, y'=dz, z'=-dy (proper rotation); hole centre at z'=zmid"""
    o = bpy.data.objects[name]
    vs = _world_verts(o); c = Vector([sum(v[i] for v in vs) / len(vs) for i in range(3)])
    c.z = 31.0  # hole centre height on the wall (bbox centre for the D-hole is offset)
    return _mesh_from(o, lambda p: Vector((slot[0] + (p.x - c.x), slot[1] + (p.z - c.z), zmid - (p.y - c.y))), name + '_w')


def _circle_prism(slot, d, z0=-1.0, z1=3.5, n=48, phase=0.9):
    R = d / 2 / math.cos(math.pi / n)
    bm = bmesh.new()
    lo = [bm.verts.new((slot[0] + R * math.cos(math.radians(phase) + 2 * math.pi * i / n), slot[1] + R * math.sin(math.radians(phase) + 2 * math.pi * i / n), z0)) for i in range(n)]
    hi = [bm.verts.new((v.co.x, v.co.y, z1)) for v in lo]
    bm.faces.new(lo[::-1]); bm.faces.new(hi)
    for i in range(n):
        j = (i + 1) % n; bm.faces.new((lo[i], lo[j], hi[j], hi[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:]); bmesh.ops.triangulate(bm, faces=bm.faces[:])
    me = bpy.data.meshes.new('_jack'); bm.to_mesh(me); bm.free()
    return me


def _dots_cutters(slot):
    """one LED cell of the real dot table (~5 dots, rows -1/0/+1): same d, spacing, phase; axes made vertical (plate is flat)"""
    spec = json.load(open(bpy.path.abspath('//NEPTR_4.2_led_dots.json')))
    pc = spec['pitch']    # v2 table (3 rows, thin band): one full LED cell = all dots whose arclength c lies in [pitch, 2 pitch)
    dots = [d for d in spec['dots'] if pc <= d['c'] < 2 * pc]
    # lay them out on the plate by (c, s): c along x, s along y (profile offset), as on the wall
    c0 = min(d['c'] for d in dots)
    bm = bmesh.new()
    for d in dots:
        cx = slot[0] + (d['c'] - c0) * 1.0 - 3.0; cy = slot[1] + d['s'] * 1.0
        R = d['d'] / 2 / math.cos(math.pi / 16); ph = d['phase']
        lo = [bm.verts.new((cx + R * math.cos(ph + 2 * math.pi * i / 16), cy + R * math.sin(ph + 2 * math.pi * i / 16), -1.0)) for i in range(16)]
        hi = [bm.verts.new((v.co.x, v.co.y, 3.5)) for v in lo]
        bm.faces.new(lo[::-1]); bm.faces.new(hi)
        for i in range(16):
            j = (i + 1) % 16; bm.faces.new((lo[i], lo[j], hi[j], hi[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:]); bmesh.ops.triangulate(bm, faces=bm.faces[:])
    me = bpy.data.meshes.new('_dots'); bm.to_mesh(me); bm.free()
    return me


def _box(name, x0, x1, y0, y1, z0, z1):
    bm = bmesh.new()
    vs = [bm.verts.new((x, y, z)) for z in (z0, z1) for y in (y0, y1) for x in (x0, x1)]
    for f in ((0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)):
        bm.faces.new([vs[i] for i in f])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:]); bmesh.ops.triangulate(bm, faces=bm.faces[:])
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    return me


def _join(meshes, name):
    bm = bmesh.new()
    for m in meshes: bm.from_mesh(m)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(o)
    return o


def _bool(target, other, op):
    m = target.modifiers.new('b', 'BOOLEAN'); m.operation = op; m.operand_type = 'OBJECT'; m.object = other
    m.solver = 'EXACT'; m.use_self = False; m.use_hole_tolerant = False
    with bpy.context.temp_override(object=target, active_object=target, selected_objects=[target]):
        bpy.ops.object.modifier_apply(modifier=m.name)


def _kill(o):
    me = o.data; bpy.data.objects.remove(o, do_unlink=True)
    if me.users == 0: bpy.data.meshes.remove(me)


def _text(label, x, y, size=2.6):
    cu = bpy.data.curves.new('_t', 'FONT'); cu.body = label; cu.size = size; cu.extrude = 0.5; cu.align_x = 'CENTER'; cu.align_y = 'CENTER'
    o = bpy.data.objects.new('_t', cu); bpy.context.scene.collection.objects.link(o)
    o.location = (x, y, 2.4)    # extrude is +-0.5 about z=2.4: engraves 0.5 into the top face
    dg = bpy.context.evaluated_depsgraph_get(); ev = o.evaluated_get(dg)
    me = bpy.data.meshes.new_from_object(ev); me.transform(o.matrix_world)
    bpy.data.objects.remove(o, do_unlink=True); bpy.data.curves.remove(cu)
    bm = bmesh.new(); bm.from_mesh(me)                      # glyph mesh comes out with split verts: weld + triangulate
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=1e-5)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:]); bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bm.to_mesh(me); bm.free()
    return me


def build_coupon(name='NEPTR_4.2_fit_coupon', seam=True, labels=True):
    W, H, T = PLATE
    plate = bpy.data.objects.new('_plate', _box('_plate', -W / 2, W / 2, -H / 2 - 0.0, H / 2, 0.0, T)); bpy.context.scene.collection.objects.link(plate)
    S = SLOTS; cut = []
    cut.append(_top_cutter('dome_cut_arcade_hole', S['arcade']))
    cut.append(_top_cutter('dome_cut_joystick_hole', S['joystick']))
    cut.append(_wall_cutter('dome_cut_hdmi', S['hdmi']))
    cut.append(_wall_cutter('dome_cut_ethernet', S['eth']))
    cut.append(_wall_cutter('dome_cut_usbc_bore', S['usbc']))
    cut.append(_top_cutter('dome_cut_lidar_L', S['lidar']))
    cut.append(_top_cutter('dome_cut_fsr_R', S['fsr']))
    cut.append(_wall_cutter('dome_cut_trs', S['trs']))
    cut.append(_circle_prism(S['jack'], 8.3))
    cut.append(_top_cutter('dome_cut_btn_L1', S['button']))
    cut.append(_top_cutter('dome_cut_enc_C', S['encoder']))
    cut.append(_wall_cutter('dome_cut_usbA_L1', S['usbA']))
    cut.append(_dots_cutters(S['dots']))
    if labels:
        for k, lab in LABELS.items():
            cut.append(_text(lab, S[k][0], S[k][1] + LABEL_DY[k], 2.4 if len(lab) > 7 else 2.8))
    # seat pad (add): real L bracket, hole at the slot
    br = bpy.data.objects['dome_add_lidar_seat_L']
    sx, sy = S['lidar']; cx0, cy0 = -25.7, 67.2
    brm = _mesh_from(br, lambda p: Vector((sx + (p.x - cx0), sy - (p.y - cy0), 62.0 - p.z)), '_br')
    # cutters in two batches: holes+text first, then union the seat pad (pad has its own aperture; the D10.5 window cutter is separate)
    cutter = _join(cut, '_cutter')
    _bool(plate, cutter, 'DIFFERENCE')
    bro = bpy.data.objects.new('_brobj', brm); bpy.context.scene.collection.objects.link(bro)
    _bool(plate, bro, 'UNION')
    for o in (cutter, bro):
        _kill(o)
    for m in cut:
        try:
            if m.users == 0: bpy.data.meshes.remove(m)
        except ReferenceError:
            pass
    parts = [plate]
    if seam:
        for key, (z0, z1, dx) in {'cap': (0.0, 14.0, -45.0), 'dome': (12.4, 26.0, 45.0)}.items():
            src = bpy.data.objects['neptr_bottom_cap' if key == 'cap' else 'neptr_top_dome_4.2']
            cp = bpy.data.objects.new('_seam_' + key, src.data.copy()); bpy.context.scene.collection.objects.link(cp)
            bx = bpy.data.objects.new('_bx', _box('_bx', 71.0, 107.0, -107.0, -73.0, z0 - 1.0 if key == 'cap' else z0 - 0.4, z1)); bpy.context.scene.collection.objects.link(bx)
            if key == 'dome':
                # seam plane: keep z >= 12.4 exactly (box bottom on the rim plane -> coplanar); lower the box 0.0 and rely on rim
                pass
            _bool(cp, bx, 'INTERSECT'); _kill(bx)
            # move to the sheet: centre the 36x34 footprint at (dx, -85), z unchanged (rim-down as modelled), cap bottom at z 0, dome rim at z 12.4 -> shift down 12.4
            M = Vector((dx - 89.0, -85.0 - (-90.0), -z0 if key == 'dome' else 0.0))
            for v in cp.data.vertices: v.co += M
            parts.append(cp)
    out = _join([p.data for p in parts], name)
    for p in parts: _kill(p)
    out.hide_viewport = False
    return out
