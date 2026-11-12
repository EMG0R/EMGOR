"""NEPTR 4.2 Pi sled, parametric height (2026-10-15 polish).  exec(open(PATH).read()) in the live Blender session.

ONE value to change after caliper measurement:  STACK['riser_tall']  (HAT top -> screen PCB back).  Then
    exec(...); z = fit_z(); o = build_sled('neptr_pi_sled_fit', z); report(o)
(or pass the Pi-bottom z directly:  build_sled('neptr_pi_sled_fit', 7.0)).

Design = the existing z6/z10 sled (handoff 11b): frame bars z 2.4-4.8, D9 corner pads + D4.4 pin holes, 6 wide diagonal
bars to the 4 posts, left rail gap y 14-32 (Pi USB-C), 0.1 underside pockets. Only the part ABOVE the frame (z > 4.8)
depends on the height: D6 shoulder 4.8 -> Zs, D2.4 pin Zs -> Zs+2.6, 0.4x45 chamfer to Zs+3.0. z10 and z6 are identical
below 4.8 (checked), so the fit sled is z10 with every vertex above 4.8 shifted by (Zs - 10): the shoulder wall is
stretched, the pin / chamfer / tip move rigidly. Zs <= 4.85 removes the shoulder wall (Pi sits on the D6.6 boss tops).
Zs here is the Pi PCB BOTTOM z in the assembled model (cap floor top 2.4)."""
import bpy, bmesh, json, math
import numpy as np

# ------------------------------------------------------------------ stack (all mm, z up, model coordinates)
SCREEN_BOSS_TOP = 55.7        # dome screen bosses end (handoff 11 Part A)  -> screen PCB FRONT (LCD side) seats here
SCREEN_PCB_T = 1.6            # components_4.2 proxy 'screen.pcb' (brief board; thickness ASSUMED 1.6)
LCD_T = 3.9                   # brief: LCD 3.9 above PCB (55.7 + 3.9 = 59.6 = ceiling underside, checks)
CEIL = 59.6
STACK = dict(
    pi_pcb=1.6,               # RPi 5 PCB thickness (standard; components_4.2 'pi5.pcb')
    stack_above_pi_top=35.0,  # brief 'HAT 65x56.5x35', READ AS total above the Pi PCB top incl. the GPIO riser (ASSUMED reading)
    gpio_riser=14.0,          # ASSUMED ('fan/GPIO riser'); only a split of the 35, not an extra term
    riser_tall=13.0,          # 'tallest riser' HAT top -> screen PCB back: no source; ASSUMED = brief 'screen header 13' (components_4.2 'screen.header')
    preload=0.3,              # requested compression so the stack is snug
)


def screen_pcb_back_z():
    return SCREEN_BOSS_TOP - SCREEN_PCB_T          # 54.1


def stack_height(S=None):
    S = S or STACK
    return S['pi_pcb'] + S['stack_above_pi_top'] + S['riser_tall']


def fit_z(S=None):
    """Pi PCB bottom z so that  Zs + stack = screen PCB back + preload  (stack 'taller' than the free gap by preload)."""
    S = S or STACK
    return screen_pcb_back_z() + S['preload'] - stack_height(S)


def stack_table(S=None):
    S = S or STACK
    zs = fit_z(S)
    rows = [('screen PCB back (bosses 55.7 - PCB 1.6)', screen_pcb_back_z(), 'handoff 11 + proxy screen.pcb; PCB t ASSUMED 1.6'),
            ('LCD face = ceiling underside (55.7 + 3.9)', SCREEN_BOSS_TOP + LCD_T, 'brief LCD 3.9; = 59.6 check'),
            ('Pi PCB', S['pi_pcb'], 'RPi 5 standard'),
            ('HAT stack above Pi PCB top (incl. 14 riser)', S['stack_above_pi_top'], 'brief 65x56.5x35, reading ASSUMED; riser 14 ASSUMED'),
            ('tallest riser HAT top -> screen PCB back', S['riser_tall'], 'ASSUMED = brief screen header 13'),
            ('stack sum (Pi bottom -> screen back)', stack_height(S), ''),
            ('preload (compression)', S['preload'], 'requested'),
            ('Pi PCB bottom z (post top)', zs, 'computed'),
            ('sled shoulder height above frame top 4.8', zs - 4.8, '')]
    return rows


# ------------------------------------------------------------------ build

def _annulus(bm, px, py):
    """flat annulus at z=4.8 between the frame-top hole ring (r 3.0, 120 verts) and the pin base ring (r 1.2, 48 verts):
    merge-march triangulation by polar angle (no collinear / zero-area triangles; a generic bridge made ~50 slivers per post)."""
    bnd = {v for e in bm.edges if e.is_boundary for v in e.verts}
    def ring(r):
        vs = [v for v in bnd if abs(math.hypot(v.co.x - px, v.co.y - py) - r) < 0.01]
        return sorted(vs, key=lambda v: math.atan2(v.co.y - py, v.co.x - px))
    o, i_ = ring(3.0), ring(1.2)
    ang = lambda v: math.atan2(v.co.y - py, v.co.x - px)
    # rotate outer so that ang(o[0]) <= ang(i_[0]) < ang(o[1])
    k = max([n for n in range(len(o)) if ang(o[n]) <= ang(i_[0]) + 1e-12] or [len(o) - 1])
    o = o[k:] + o[:k]
    def u(v, base):  # unwrapped angle >= base
        a = ang(v)
        while a < base - 1e-12: a += 2 * math.pi
        return a
    a0 = ang(o[0]); oa = [u(v, a0) for v in o]; ia = [u(v, a0) for v in i_]
    ordr = sorted(range(len(i_)), key=lambda n: ia[n]); i_ = [i_[n] for n in ordr]; ia = [ia[n] for n in ordr]
    no, ni = len(o), len(i_); a = b = 0; faces = []
    while a < no or b < ni:
        na = oa[a + 1] if a + 1 < no else a0 + 2 * math.pi
        nb = ia[b + 1] if b + 1 < ni else a0 + 2 * math.pi
        if a >= no: adv = 'i'
        elif b >= ni: adv = 'o'
        else: adv = 'o' if na <= nb else 'i'
        oa_v, ia_v = o[a % no], i_[b % ni]
        if adv == 'o':
            faces.append((oa_v, o[(a + 1) % no], ia_v)); a += 1
        else:
            faces.append((oa_v, i_[(b + 1) % ni], ia_v)); b += 1
    for f in faces:
        try: bm.faces.new(f)
        except ValueError: pass
FRAME_TOP = 4.8
POSTS = [(-25.15, 30.7), (23.85, 30.7), (-25.15, -27.3), (23.85, -27.3)]   # Pi M2.5 holes (58 x 49)


# left-rail relief for the two micro-HDMI plugs (Pi west face, y 8.2 / -5.3, overmold ~9 x 8, ASSUMED): with the Pi at Zs < ~5.3 the
# plug bottoms (Zs + 1.6 + 1.7 - 4) dip below the rail top 4.8. Extends the existing USB-C gap (y 14..32) down to y -10.8.
HDMI_RELIEF = dict(x0=-37.0, x1=-27.0, y0=-10.8, y1=15.0, z0=2.0, z1=5.5)


def build_sled(name, zs, base='neptr_pi_sled_z10', coll='PI_SLED', loc=(0.0, 0.0, 0.0), relief=None):
    """relief: None = automatic (on when the HDMI plug bottom Zs+1.6+1.7-4 < 4.8 + 0.3), True / False to force."""
    src = bpy.data.objects[base]
    bm = bmesh.new(); bm.from_mesh(src.data)
    top_base = max(v.co.z for v in bm.verts) - 3.0           # base Zs (10 for z10)
    dz = zs - top_base
    no_shoulder = zs - FRAME_TOP < 0.05
    if 0.05 <= zs - FRAME_TOP < 0.2: raise ValueError('shoulder between 0.05 and 0.2 mm: degenerate, pick Zs <= 4.85 or >= 5.0')
    def near_post(v, lo, hi):
        return any(lo <= math.hypot(v.co.x - px, v.co.y - py) <= hi for px, py in POSTS)
    if no_shoulder:      # delete shoulder wall (R3, z 4.8..top_base) + shoulder top annulus (R1.2..3 at top_base) BEFORE shifting
        bm.faces.ensure_lookup_table(); dele = []
        for f in bm.faces:
            vs = f.verts
            if abs(f.normal.z) < 0.1 and all(near_post(v, 2.99, 3.01) and FRAME_TOP - 1e-3 <= v.co.z <= top_base + 1e-3 for v in vs): dele.append(f)
            elif f.normal.z > 0.9 and all(abs(v.co.z - top_base) < 1e-3 and near_post(v, 1.19, 3.01) for v in vs): dele.append(f)
        bmesh.ops.delete(bm, geom=dele, context='FACES')
    for v in bm.verts:
        if v.co.z > FRAME_TOP + 0.01: v.co.z += dz
    if no_shoulder:      # refill the flat annulus (hole ring R3 in the frame top, 120 verts) <-> pin ring R1.2 (48) at z 4.8
        bm.edges.ensure_lookup_table()
        for px, py in POSTS:
            _annulus(bm, px, py)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
        for v in bm.verts: v.co.z = FRAME_TOP if abs(v.co.z - FRAME_TOP) < 1e-4 else v.co.z
    if relief is None: relief = (zs + 1.6 + 1.7 - 4.0) < FRAME_TOP + 0.3
    old = bpy.data.objects.get(name)
    if old:
        m = old.data; bpy.data.objects.remove(old, do_unlink=True); bpy.data.meshes.remove(m)
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new(name, me)   # location set AFTER the relief boolean (world-space solve)
    bpy.data.collections[coll].objects.link(o)
    if relief:
        R = HDMI_RELIEF; cb = bmesh.new()
        bmesh.ops.create_cube(cb, size=1.0)
        bmesh.ops.scale(cb, vec=(R['x1'] - R['x0'], R['y1'] - R['y0'], R['z1'] - R['z0']), verts=cb.verts)
        bmesh.ops.translate(cb, vec=((R['x0'] + R['x1']) / 2, (R['y0'] + R['y1']) / 2, (R['z0'] + R['z1']) / 2), verts=cb.verts)
        cm = bpy.data.meshes.new('_relief'); cb.to_mesh(cm); cb.free()
        co = bpy.data.objects.new('_relief', cm); bpy.context.scene.collection.objects.link(co)
        m = o.modifiers.new('relief', 'BOOLEAN'); m.operation = 'DIFFERENCE'; m.operand_type = 'OBJECT'; m.object = co
        m.solver = 'EXACT'; m.use_self = False; m.use_hole_tolerant = False
        with bpy.context.temp_override(object=o, active_object=o, selected_objects=[o]):
            bpy.ops.object.modifier_apply(modifier=m.name)
        bpy.data.objects.remove(co, do_unlink=True); bpy.data.meshes.remove(cm)
    o.location = loc
    o['hdmi_relief'] = bool(relief)
    o['pi_bottom_z'] = float(zs); o['stack'] = json.dumps(STACK); o['stack_height'] = stack_height()
    return o


def report(o):
    """manifold / self-intersection / print-flat checks on the sled mesh (local coordinates = installed pose)."""
    ns = {}
    exec(open('/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS/neptr/shell/NEPTR_4.2_apply_features.py').read(), ns)
    V, T = ns['_arrays'](o)
    topo = ns['topology'](V, T); si, raw = ns['selfint'](V, T)
    area = 0.5 * np.linalg.norm(np.cross(V[T[:, 1]] - V[T[:, 0]], V[T[:, 2]] - V[T[:, 0]]), axis=1)
    nrm = np.cross(V[T[:, 1]] - V[T[:, 0]], V[T[:, 2]] - V[T[:, 0]]); nn = np.linalg.norm(nrm, axis=1); nn[nn == 0] = 1; nrm = nrm / nn[:, None]
    zmin = float(V[:, 2].min())
    # overhang (print flat on z=zmin): downward faces not on the bed plane, angle from -z
    down = (nrm[:, 2] < -0.7071) & (V[T].mean(1)[:, 2] > zmin + 0.001)
    oh = float(area[down].sum())
    vol = float(np.sum(np.einsum('ij,ij->i', V[T[:, 0]], np.cross(V[T[:, 1]], V[T[:, 2]]))) / 6)
    return dict(topo=topo, strict=si, raw=raw, zmin=zmin, zmax=float(V[:, 2].max()), overhang_mm2=oh, volume=vol,
                bbox=[V.min(0).round(3).tolist(), V.max(0).round(3).tolist()])
