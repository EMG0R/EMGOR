"""NEPTR 4.2 component proxies + interference audit (run inside the live Blender session).

    exec(open('NEPTR_4.2_components.py').read())
    P = build_pieces()            # list[Piece] from the data tables (installed positions)
    make_blender_proxies(P)       # hidden collection COMPONENTS_4.2, objects comp_<part>
    audit_pairs(P)                # convex-vs-convex overlaps  (min thickness of the overlap region)
    audit_shells(P, solids)       # proxy surface samples inside dome / cap / sled / ring solids
    audit_lowering(P, dome, cap)  # dome-attached vs cap-attached vertical insertion
Every dimension carries a `src` string: doc/brief/measurement, or 'ASSUMED'.
"""
import bpy, bmesh, json, math
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree


# ------------------------------------------------------------------ convex pieces
class Piece:
    def __init__(self, comp, name, kind, attach, src, **kw):
        self.comp, self.name, self.kind, self.attach, self.src = comp, name, kind, attach, src
        self.kw = kw
        if kind == 'box':
            self.c = np.array(kw['c'], float); self.size = np.array(kw['size'], float); self.rz = math.radians(kw.get('rz', 0.0))
            cr, sr = math.cos(self.rz), math.sin(self.rz)
            self.ry = math.radians(kw.get('ry', 0.0))  # rotation about world/local y, applied first (tilted boards)
            cy_, sy_ = math.cos(self.ry), math.sin(self.ry)
            Ry = np.array([[cy_, 0, sy_], [0, 1.0, 0], [-sy_, 0, cy_]])
            self.R = np.array([[cr, -sr, 0], [sr, cr, 0], [0, 0, 1.0]]) @ Ry  # local->world
            h = self.size / 2
            corners = np.array([[sx * h[0], sy * h[1], sz * h[2]] for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)])
            wc = corners @ self.R.T + self.c
            self.lo, self.hi = wc.min(0), wc.max(0)
        else:
            self.p0 = np.array(kw['p0'], float); self.p1 = np.array(kw['p1'], float); self.r = kw['d'] / 2
            ax = self.p1 - self.p0; self.L = float(np.linalg.norm(ax)); self.u = ax / self.L
            ext = self.r * np.sqrt(np.clip(1 - self.u ** 2, 0, 1))
            self.lo = np.minimum(self.p0, self.p1) - ext; self.hi = np.maximum(self.p0, self.p1) + ext
        self.full = f'{comp}.{name}'

    @property
    def assumed(self): return 'ASSUMED' in self.src

    def margin(self, pts):
        """>0 inside (distance to nearest boundary face/cap, approx), <0 outside"""
        if self.kind == 'box':
            q = (pts - self.c) @ self.R
            return (self.size / 2 - np.abs(q)).min(axis=1)
        d = pts - self.p0; t = d @ self.u
        rad = np.linalg.norm(d - np.outer(t, self.u), axis=1)
        return np.minimum(self.r - rad, np.minimum(t, self.L - t))

    def contains(self, pts): return self.margin(pts) >= 0

    def volume_grid(self, h, lo=None, hi=None):
        lo = self.lo if lo is None else np.maximum(self.lo, lo); hi = self.hi if hi is None else np.minimum(self.hi, hi)
        if (hi < lo).any(): return np.zeros((0, 3))
        ax = [np.arange(lo[i] + h / 2, hi[i], h) if hi[i] - lo[i] > h else np.array([(lo[i] + hi[i]) / 2]) for i in range(3)]
        G = np.stack(np.meshgrid(*ax, indexing='ij'), -1).reshape(-1, 3)
        return G[self.contains(G)]

    def surface_points(self, h):
        """points on the surface at ~h spacing (for inside-solid tests)"""
        pts = []
        if self.kind == 'box':
            s = self.size
            for ax in range(3):
                o = [i for i in range(3) if i != ax]
                nu = max(2, int(math.ceil(s[o[0]] / h)) + 1); nv = max(2, int(math.ceil(s[o[1]] / h)) + 1)
                U, V = np.meshgrid(np.linspace(-s[o[0]] / 2, s[o[0]] / 2, nu), np.linspace(-s[o[1]] / 2, s[o[1]] / 2, nv), indexing='ij')
                for sg in (-1, 1):
                    q = np.zeros((U.size, 3)); q[:, o[0]] = U.ravel(); q[:, o[1]] = V.ravel(); q[:, ax] = sg * s[ax] / 2
                    pts.append(q @ self.R.T + self.c)
        else:
            u = self.u; ref = np.array([0, 0, 1.0]) if abs(u[2]) < 0.9 else np.array([1.0, 0, 0])
            a = np.cross(u, ref); a /= np.linalg.norm(a); b = np.cross(u, a)
            nt = max(2, int(math.ceil(self.L / h)) + 1); nth = max(8, int(math.ceil(2 * math.pi * self.r / h)))
            T, TH = np.meshgrid(np.linspace(0, self.L, nt), np.linspace(0, 2 * math.pi, nth, endpoint=False), indexing='ij')
            pts.append(self.p0 + np.outer(T.ravel(), u) + self.r * (np.outer(np.cos(TH.ravel()), a) + np.outer(np.sin(TH.ravel()), b)))
            nr = max(1, int(math.ceil(self.r / h)))
            for sg, base in ((0, self.p0), (1, self.p1)):
                for k in range(nr + 1):
                    rr = self.r * k / nr; nn = max(1, int(math.ceil(2 * math.pi * rr / h)))
                    th = np.linspace(0, 2 * math.pi, nn, endpoint=False)
                    pts.append(base + rr * (np.outer(np.cos(th), a) + np.outer(np.sin(th), b)))
        return np.vstack(pts)

    def mesh_geom(self, seg=24):
        if self.kind == 'box':
            h = self.size / 2
            V = np.array([[sx * h[0], sy * h[1], sz * h[2]] for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)]) @ self.R.T + self.c
            F = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
            return V, F
        u = self.u; ref = np.array([0, 0, 1.0]) if abs(u[2]) < 0.9 else np.array([1.0, 0, 0])
        a = np.cross(u, ref); a /= np.linalg.norm(a); b = np.cross(u, a)
        th = np.linspace(0, 2 * math.pi, seg, endpoint=False)
        ring = self.r * (np.outer(np.cos(th), a) + np.outer(np.sin(th), b))
        V = np.vstack([self.p0 + ring, self.p1 + ring])
        F = [tuple(range(seg))[::-1], tuple(range(seg, 2 * seg))] + [(i, (i + 1) % seg, seg + (i + 1) % seg, seg + i) for i in range(seg)]
        return V, F


def box(comp, name, attach, src, c, size, rz=0.0, ry=0.0): return Piece(comp, name, 'box', attach, src, c=c, size=size, rz=rz, ry=ry)
def cyl(comp, name, attach, src, p0, p1, d): return Piece(comp, name, 'cyl', attach, src, p0=p0, p1=p1, d=d)


def overlap(A, B, h=0.4):
    """convex-convex: returns (thickness_mm, volume_mm3, centre) or None. thickness = smallest bbox extent of the common region."""
    lo = np.maximum(A.lo, B.lo); hi = np.minimum(A.hi, B.hi)
    if (hi < lo - 1e-9).any(): return None
    span = hi - lo; hh = max(h, float(np.prod(np.maximum(span, 1e-3)) ** (1 / 3)) / 40)
    pad = hh * 0.5
    G = A.volume_grid(hh, lo - pad, hi + pad)
    if len(G) == 0: return None
    G = G[B.contains(G)]
    if len(G) == 0: return None
    ext = G.max(0) - G.min(0) + hh
    return float(ext.min()), float(len(G) * hh ** 3), G.mean(0)

# ------------------------------------------------------------------ component data (installed positions, mm)
# x east, y north (performer side = south = y-), z up. Cap floor top 2.4, seam 12.4, ceiling 59.6, top 62.
YJ, YU = 100.07, -99.5   # flat seat planes added in v19 (north jack nuts / south USB flanges)
YN, YS = 101.06, -101.06           # inner wall planes at the port heights (ray-measured, flat over x -21..21 / 75)
CEIL = 59.6
A = 'ASSUMED'

# per-part overrides so a fix can move things without touching the tables
DEF = dict(
    sled_z=6.0,          # PI_SLED variant (6 or 10)
    hat_gap=14.0,        # GPIO riser height (Pi PCB top -> HAT bottom)
    teensy_c=(66.0, 2.0),
    teensy_rz=0.0,
    imu_c=(-80.0, -20.0),
)


def build_pieces(**over):
    p = dict(DEF); p.update(over)
    P = []
    add = P.append
    Zs = p['sled_z']; T = Zs + 1.6
    # ---------------- Pi 5 (cap, on sled). hole pattern 58x49 from sled posts (-25.15/23.85, +30.7/-27.2) => board x -28.65..27.35, y -50.8..34.2, long axis y
    pi_src = 'brief 85x56 + sled post pattern (handoff 11b); jack positions = RPi5 mech drawing (from memory)'
    add(box('pi5', 'pcb', 'cap', 'brief 85x56x1.6; position from sled posts', (-0.65, -8.3, Zs + 0.8), (56, 85, 1.6)))
    add(box('pi5', 'eth', 'cap', pi_src + '; RJ45 16x21.2x13.5', (-18.4, -41.2, T + 6.75), (16, 21.2, 13.5)))
    add(box('pi5', 'usb_L', 'cap', pi_src + '; stacked USB-A 13.1x17x16', (0.35, -42.8, T + 8), (13.1, 17, 16)))
    add(box('pi5', 'usb_R', 'cap', pi_src + '; stacked USB-A 13.1x17x16', (18.35, -42.8, T + 8), (13.1, 17, 16)))
    add(box('pi5', 'usbc', 'cap', pi_src, (-25.9, 23.0, T + 1.65), (7.3, 9.0, 3.3)))
    add(box('pi5', 'hdmi0', 'cap', pi_src, (-25.9, 8.2, T + 1.7), (6.6, 7.5, 3.4)))
    add(box('pi5', 'hdmi1', 'cap', pi_src, (-25.9, -5.3, T + 1.7), (6.6, 7.5, 3.4)))
    add(box('pi5', 'cooler', 'cap', A + ': Pi 5 active cooler 30x30x11 at the SoC (brief says "fan")', (-0.65, -0.8, T + 5.5), (30, 30, 11)))
    # plugs + boots (insertion volume, straight run before the bend)
    add(box('pi_plugs', 'usbc_plug', 'cap', A + ': USB-C overmold 12x7, 20 straight (brief: ~25 bend clearance is the cable route)', (-39.55, 23.0, T + 1.65), (20, 12, 7)))
    add(box('pi_plugs', 'hdmi0_plug', 'cap', A + ': micro-HDMI overmold 9x8, 20 straight', (-39.2, 8.2, T + 1.7), (20, 9, 8)))
    add(box('pi_plugs', 'hdmi1_plug', 'cap', A + ': micro-HDMI overmold 9x8, 20 straight', (-39.2, -5.3, T + 1.7), (20, 9, 8)))
    for nm, x in (('L', 0.35), ('R', 18.35)):
        add(box('pi_plugs', f'usb{nm}_lo_plug', 'cap', A + ': USB-A overmold 13x7.2, 25 straight', (x, -63.8, T + 3.9), (13, 25, 7.2)))
        add(box('pi_plugs', f'usb{nm}_hi_plug', 'cap', A + ': USB-A overmold 13x7.2, 25 straight', (x, -63.8, T + 12.1), (13, 25, 7.2)))
    add(box('pi_plugs', 'eth_plug', 'cap', A + ': RJ45 plug+boot 15x13, 30 straight', (-18.4, -66.8, T + 7.0), (15, 30, 13)))
    # riser + HAT
    add(box('fan_riser', 'riser', 'cap', A + ': 2x20 GPIO stacking riser 5.5 x 50.8 x hat_gap (raises HAT over the cooler); GPIO row from RPi drawing', (23.8, 1.7, T + p['hat_gap'] / 2), (5.5, 50.8, p['hat_gap'])))
    add(box('raspiaudio_hat', 'hat', 'cap', 'brief 65x56.5x35 (35 = total above Pi PCB top incl. riser: ASSUMED reading); same hole pattern as Pi', (-0.65, 1.75, T + p['hat_gap'] + (35 - p['hat_gap']) / 2), (56.5, 65, 35 - p['hat_gap'])))
    # ---------------- Teensy 4.1 on cap floor, 3 mm standoffs
    tx, ty = p['teensy_c']; rz = p['teensy_rz']
    cz = 2.4 + 3.0 + 0.8
    add(box('teensy41', 'pcb', 'cap', 'plan.md 61x18; ' + A + ' location (floor, east side, free zone)', (tx, ty, cz), (18, 61, 1.6), rz))
    # micro-USB end at south (-y) end; local offset rotated by rz
    c, s = math.cos(math.radians(rz)), math.sin(math.radians(rz))
    def rot(dx, dy): return (tx + dx * c - dy * s, ty + dx * s + dy * c)
    ux, uy = rot(0, -30.5 - 3.0); add(box('teensy41', 'usb_jack', 'cap', 'PJRC T4.1 micro-B 8x6x3 (typical)', (ux, uy, cz + 1.2), (8, 6, 3), rz))
    ux, uy = rot(0, -30.5 - 6 - 12.5); add(box('teensy41', 'usb_plug', 'cap', A + ': micro-B overmold 12x7, 25 straight', (ux, uy, cz + 1.2), (12, 25, 7), rz))
    ix, iy = p['imu_c']
    add(box('imu_mpu9250', 'pcb', 'cap', 'plan.md 15x25; ' + A + ' height 4 on 3 mm standoffs, location east floor', (ix, iy, 2.4 + 3 + 2.0), (15, 25, 4.0)))
    # ---------------- joystick KY-023 on cap supports (tops z 45.7, pins to 50)
    add(box('joystick_ky023', 'pcb', 'cap', 'plan.md 34x26; supports (-76,32)(-76,52)(-50,52) top z 45.7 (handoff 12)', (-63, 42, 45.7 + 0.8), (34, 26, 1.6)))
    add(box('joystick_ky023', 'body', 'cap', A + ': gimbal body 22x22 up to ceiling (plan.md total 32 above PCB incl. stick)', (-63, 42, 47.3 + (CEIL - 47.3) / 2), (22, 22, CEIL - 47.3)))
    # ---------------- screen (dome, hangs on 4 bosses z 55.7)
    add(box('screen', 'pcb', 'dome', 'brief board 98.58x76.54; underside of bosses z 55.7 (handoff 11 Part A)', (0, -2.1, 54.9), (76.54, 98.58, 1.6)))
    add(box('screen', 'lcd', 'dome', 'brief LCD 3.9 above PCB (55.7 -> 59.6); outline ASSUMED 57.2x94.6 (x edge 28.6 from handoff)', (0, -2.1, 57.65), (57.2, 94.6, 3.9)))
    add(box('screen', 'header', 'dome', 'brief header 13 below PCB; ' + A + ' 2x13 33x5.1 at the south edge', (0, -48.5, 54.1 - 6.5), (33, 5.1, 13)))
    # ---------------- buttons (8), encoders (5), arcade, FSR (2), LiDAR (2)
    for nm, (x, y) in {'L1': (-44, 0), 'L2': (-55, 11), 'L3': (-55, -11), 'L4': (-66, 0), 'R1': (44, 0), 'R2': (55, 11), 'R3': (55, -11), 'R4': (66, 0)}.items():
        add(cyl(f'btn_{nm}', 'body', 'dome', 'plan.md PBS-110 7 mm; ' + A + ' 16 behind panel', (x, y, CEIL - 16), (x, y, CEIL), 7.0))
        add(cyl(f'btn_{nm}', 'nut', 'dome', A + ': nut D10 x 2', (x, y, CEIL - 2), (x, y, CEIL), 10.0))
    for nm, (x, y) in {'L2': (-85.4, -65.4), 'L1': (-42.7, -65.4), 'C': (0.0, -65.4), 'R1': (42.7, -65.4), 'R2': (85.4, -65.4),
                       'L2N': (-85.4, 75.0), 'R2N': (85.4, 75.0)}.items():   # N = ENC6/ENC7 (handoff 17)
        add(box(f'enc_{nm}', 'body', 'dome', 'brief EC11-style 12x12; handoff 14b body z 52.6..59.6', (x, y, 52.6 + 3.5), (12, 12, 7)))
        add(box(f'enc_{nm}', 'pins', 'dome', A + ': solder pins 10x10x4 under body', (x, y, 48.6 + 2), (10, 10, 4)))
    add(cyl('arcade', 'clip_ring', 'dome', 'brief ~38 mm ring', (62.5, 42, CEIL - 3), (62.5, 42, CEIL), 38.0))
    add(cyl('arcade', 'body', 'dome', 'brief 30 mm body; ' + A + ' 19.6 deep', (62.5, 42, 40.0), (62.5, 42, CEIL - 3), 30.0))
    add(box('arcade', 'microswitch', 'dome', A + ': microswitch 28x14x11 under body (handoff: clear to z 20)', (62.5, 42, 29 + 5.5), (28, 14, 11)))
    for nm, x in (('L', -42.1), ('R', 42.1)):
        add(cyl(f'fsr_{nm}', 'head', 'dome', A + ': FSR402 head D18.3 x 0.5 on ceiling (plan.md says 43.7 square = FSR406 body; slot 6x8 at (+-42.1,-37.6))', (x, -37.6, CEIL - 0.5), (x, -37.6, CEIL), 18.3))
    # LiDAR: pose comes from the bracket feature spec (single source of truth: LIDAR dict in NEPTR_4.2_apply_features.py)
    for nm, x in (('L', -25.7), ('R', 25.7)):
        fo = bpy.data.objects.get(f'dome_add_lidar_bracket_{nm}')
        if fo is not None:
            sp = json.loads(fo['spec']); a = np.array(sp['axis']); C = np.array(sp['pcb_top_centre']); ryd = sp['ry_deg']
            pc_c = C - a * 0.8                       # PCB centre = top face - half thickness
            ap0 = C + a * 0.0; ap1 = C + a * sp['sensor_h']
            src_pcb = 'plan.md TOF400C VL53L1X 20x12 (pcb thickness 1.6 ASSUMED), seated on bracket posts; pose from dome_add_lidar_bracket spec'
            add(box(f'lidar_{nm}', 'pcb', 'dome', src_pcb, tuple(pc_c), (20, 12, 1.6), 0.0, ryd))
            add(cyl(f'lidar_{nm}', 'sensor', 'dome', A + ': aperture body D8 x ' + str(sp['sensor_h']) + ' above the PCB top, up into the D10.5 hole', tuple(ap0), tuple(ap1), 8.0))
        else:
            add(box(f'lidar_{nm}', 'pcb', 'dome', 'plan.md TOF400C VL53L1X 20x12; z/orientation ' + A, (x, 67.2, 57.7), (20, 12, 1.6)))
            add(cyl(f'lidar_{nm}', 'sensor', 'dome', A + ': aperture body D8 up into the D10.5 hole', (x, 67.2, 58.5), (x, 67.2, 60.4), 8.0))
    # ---------------- piezo discs on the E/W inner walls (plan.md 20 mm; location ASSUMED)
    for nm, sg in (('L', -1), ('R', 1)):
        add(cyl(f'piezo_{nm}', 'disc', 'dome', 'plan.md D20; ' + A + ' location (y 0, z 31) and 1 mm epoxy+disc', (sg * 100.6, 0, 31), (sg * 99.6, 0, 31), 20.0))
    # ---------------- north wall: 8 jacks, USB-C IP67, TRS
    for xi, x in enumerate((-21, -7, 7, 21)):
        for zi, z in enumerate((24, 38)):
            nm = f'jack_{xi}{zi}'
            add(cyl(nm, 'nut', 'dome', 'brief knurled nut ~12; ' + A + ' 2.5 thick on the inside', (x, YJ - 2.5, z), (x, YJ, z), 12.0))
            add(cyl(nm, 'body', 'dome', 'brief M8 (hole D8.3); ' + A + ' body D9 x 14 deep', (x, YJ - 14, z), (x, YJ - 2.5, z), 9.0))
    add(cyl('usbc_ip67', 'body', 'dome', 'plan.md IP67 USB-C D16.3 hole; ' + A + ' body D16 x 23 behind wall (through collar bore)', (-75, YN - 23, 31), (-75, YN, 31), 16.0))
    add(cyl('trs_expression', 'nut', 'dome', A + ': M6 nut D9.5 x 2', (75, 98.96, 31), (75, 100.96, 31), 9.5))
    add(box('trs_expression', 'body', 'dome', 'plan.md PJ-307; ' + A + ' body 8x14x8', (75, 91.96, 31), (8, 14, 8)))
    # ---------------- south wall
    for nm, x in (('A', -50.2), ('B', -36.2), ('C', 51.4)):
        add(box(f'usbA_{nm}', 'flange', 'dome', A + ': ears plate 12 x 3 x 30 (screws z 18.5/43.5)', (x, YU + 1.5, 31), (12, 3, 30)))
        add(box(f'usbA_{nm}', 'housing', 'dome', 'opening 7x15 (handoff 12); ' + A + ' housing 7x15.2 x 17 deep', (x, YU + 3 + 8.5, 31), (7, 17, 15.2)))
    add(cyl('hdmi_coupler', 'nut', 'dome', 'handoff 13: nut D30, seat y -100.2', (-72.2, -100.2, 31), (-72.2, -97.2, 31), 30.0))
    add(cyl('hdmi_coupler', 'body', 'dome', A + ': body D24 x 25 behind the nut', (-72.2, -97.2, 31), (-72.2, -72.2, 31), 24.0))
    add(cyl('eth_m23_coupler', 'nut', 'dome', 'handoff 13: nut D29, seat y -100.4 (M23)', (72.9, -100.4, 31), (72.9, -96.4, 31), 29.0))
    add(cyl('eth_m23_coupler', 'body', 'dome', A + ': body D24 x 30 behind the nut', (72.9, -96.4, 31), (72.9, -66.4, 31), 24.0))
    return P


def cable_defs(P, p=None):
    """(name, dia, start, end, own piece full-names). Starts/ends sit 2-3 mm beyond the rear face of the panel part / plug."""
    pc = {x.full: x for x in P}
    def rear_y(full, sgn):  # centre of the +/-y end face of a y-aligned box/cyl
        x = pc[full]
        if x.kind == 'box':
            c = x.c.copy(); c[1] += sgn * x.size[1] / 2; return c
        c = (x.p0 + x.p1) / 2; c[1] = x.hi[1] if sgn > 0 else x.lo[1]; return c
    def rear_x(full, sgn):
        x = pc[full]; c = x.c.copy(); c[0] += sgn * x.size[0] / 2; return c
    out = []
    def add(name, d, s, e, own): out.append(dict(name=name, d=d, start=np.array(s, float), end=np.array(e, float), own=own))
    s = rear_y('usbc_ip67.body', -1); e = rear_x('pi_plugs.usbc_plug', -1)
    add('usbc_pd', 8, s + [0, -2, 0], e + [-2, 0, 0], ['usbc_ip67.body', 'pi_plugs.usbc_plug'])
    s = (pc['hdmi_coupler.body'].p0 + pc['hdmi_coupler.body'].p1) / 2; s = np.array([s[0], pc['hdmi_coupler.body'].hi[1] + 2, s[2]])
    e = rear_x('pi_plugs.hdmi1_plug', -1)
    add('hdmi', 8, s, e + [-2, 0, 0], ['hdmi_coupler.body', 'hdmi_coupler.nut', 'pi_plugs.hdmi1_plug'])
    b = pc['eth_m23_coupler.body']; s = np.array([b.p0[0], b.hi[1] + 2, b.p0[2]])
    e = rear_y('pi_plugs.eth_plug', -1)
    add('ethernet', 8, s, e + [0, -2, 0], ['eth_m23_coupler.body', 'eth_m23_coupler.nut', 'pi_plugs.eth_plug'])
    for nm, plug in (('A', 'pi_plugs.usbL_lo_plug'), ('B', 'pi_plugs.usbL_hi_plug'), ('C', 'pi_plugs.usbR_lo_plug')):
        h = pc[f'usbA_{nm}.housing']; s = h.c.copy(); s[1] = h.c[1] + h.size[1] / 2 + 2
        e = rear_y(plug, -1)
        add(f'usbA_{nm}', 8, s, e + [0, -2, 0], [f'usbA_{nm}.housing', f'usbA_{nm}.flange', plug])
    tp = pc['teensy41.usb_plug']; s = tp.c.copy(); s[1] -= tp.size[1] / 2 + 2
    e = rear_y('pi_plugs.usbR_hi_plug', -1)
    add('teensy_usb', 8, s, e + [0, -2, 0], ['teensy41.usb_plug', 'pi_plugs.usbR_hi_plug'])
    for xi, x in enumerate((-21, -7, 7, 21)):
        for zi, z in enumerate((24, 38)):
            b = pc[f'jack_{xi}{zi}.body']
            add(f'jack_{xi}{zi}', 8, [x, b.p0[1] - 2, z], [x, 34.2 + 5, z], [f'jack_{xi}{zi}.body', f'jack_{xi}{zi}.nut', 'raspiaudio_hat.hat'])
    tb = pc['trs_expression.body']; t = pc['teensy41.pcb']
    add('trs', 8, [75, tb.c[1] - tb.size[1] / 2 - 2, 31], [t.c[0], t.c[1] + 30.5 + 5, t.c[2] + 3.4], ['trs_expression.body', 'trs_expression.nut', 'teensy41.pcb'])
    return out

# ------------------------------------------------------------------ audit machinery
def mesh_arrays(o, M=None):
    me = o.data; me.calc_loop_triangles()
    V = np.empty(len(me.vertices) * 3); me.vertices.foreach_get('co', V); V = V.reshape(-1, 3)
    T = np.empty(len(me.loop_triangles) * 3, np.int64); me.loop_triangles.foreach_get('vertices', T)
    if M is not None: V = (np.array(M)[:3, :3] @ V.T).T + np.array(M)[:3, 3]
    return V, T.reshape(-1, 3)


class Solid:
    def __init__(self, name, V, T):
        self.name = name; self.V = V; self.T = T
        self.bvh = BVHTree.FromPolygons(V.tolist(), T.tolist())
        self.lo, self.hi = V.min(0), V.max(0)

    def inside(self, pts, dirs=((0.2672, 0.5345, 0.8018), (-0.6, 0.3, -0.7416))):
        out = np.ones(len(pts), bool)
        for d in dirs:
            d = Vector(d).normalized(); res = np.zeros(len(pts), bool)
            for i, p in enumerate(pts):
                n = 0; q = Vector(p.tolist())
                for _ in range(40):
                    h = self.bvh.ray_cast(q, d)
                    if h[0] is None: break
                    n += 1; q = h[0] + d * 1e-4
                res[i] = n % 2 == 1
            out &= res
        return out

    def depth(self, pts):
        return np.array([self.bvh.find_nearest(Vector(p.tolist()))[3] for p in pts])


def make_solids(dome='neptr_top_dome_4.2', cap='neptr_bottom_cap', sleds=('neptr_pi_sled_z6',)):
    S = {}
    for key, nm in (('dome', dome), ('cap', cap)):
        o = bpy.data.objects[nm]; V, T = mesh_arrays(o); S[key] = Solid(key, V, T)
    for nm in sleds:
        o = bpy.data.objects[nm]
        M = o.get('installed_matrix')
        M = np.array(M).reshape(4, 4) if M is not None else np.array(o.matrix_world)
        V, T = mesh_arrays(o, M); S[nm] = Solid(nm, V, T)
    return S


# ------------------------------------------------------------------ pair audit
def severity(th, assumed):
    s = 'HIGH' if th >= 1.0 else ('MED' if th >= 0.2 else 'LOW')
    return s


def audit_pairs(P, h=0.4, min_th=0.05, skip=lambda a, b: False):
    rows = {}
    n = len(P)
    for i in range(n):
        A = P[i]
        for j in range(i + 1, n):
            B = P[j]
            if A.comp == B.comp or skip(A, B): continue
            if (np.minimum(A.hi, B.hi) < np.maximum(A.lo, B.lo)).any(): continue
            r = overlap(A, B, h)
            if r is None or r[0] < min_th: continue
            key = (A.comp, B.comp)
            row = rows.get(key)
            if row is None or r[0] > row['mm']:
                rows[key] = dict(a=A.comp, b=B.comp, pa=A.name, pb=B.name, mm=round(r[0], 2), vol=round(r[1], 1), at=[round(float(x), 1) for x in r[2]],
                                 assumed=A.assumed or B.assumed)
    out = sorted(rows.values(), key=lambda r: -r['mm'])
    for r in out: r['sev'] = severity(r['mm'], r['assumed'])
    return out


# ------------------------------------------------------------------ pieces vs mesh solids
def audit_shells(P, solids, h=0.7, ignore=lambda piece, solid, pt: False):
    rows = []
    for key, S in solids.items():
        for pc in P:
            if (np.minimum(pc.hi, S.hi) < np.maximum(pc.lo, S.lo)).any(): continue
            pts = pc.surface_points(h)
            m = ((pts >= S.lo) & (pts <= S.hi)).all(1); pts = pts[m]
            if len(pts) == 0: continue
            ins = S.inside(pts)
            if not ins.any(): continue
            q = pts[ins]
            keep = np.array([not ignore(pc, S, x) for x in q])
            q = q[keep]
            if len(q) == 0: continue
            d = S.depth(q); k = int(d.argmax())
            rows.append(dict(a=pc.comp, pa=pc.name, b=key, mm=round(float(d.max()), 2), n=int(len(q)), at=[round(float(x), 1) for x in q[k]],
                             assumed=pc.assumed))
    rows.sort(key=lambda r: -r['mm'])
    for r in rows: r['sev'] = severity(r['mm'], r['assumed'])
    return rows


# ------------------------------------------------------------------ dome lowering (vertical insertion)
def audit_lowering(P, dome_solid, cap_solid, cell=1.0, h=1.0):
    """dome-attached pieces + dome shell move down onto the cap. A cap-attached piece (or cap feature above the seam)
    blocks if it lies above the lowest point of any dome-attached thing in the same xy column."""
    xs = np.arange(-126, 126.01, cell); ys = np.arange(-106, 106.01, cell)
    zmin = np.full((len(ys), len(xs)), np.inf); who = np.full((len(ys), len(xs)), -1, int)
    names = []
    # dome shell: lowest solid point per column by upward rays
    for iy, y in enumerate(ys):
        for ix, x in enumerate(xs):
            hh = dome_solid.bvh.ray_cast(Vector((x, y, -5)), Vector((0, 0, 1)))
            if hh[0] is not None and hh[0].z < zmin[iy, ix]:
                zmin[iy, ix] = hh[0].z; who[iy, ix] = 0
    names.append('dome shell')
    for pc in P:
        if pc.attach != 'dome': continue
        g = pc.volume_grid(h)
        if len(g) == 0: continue
        names.append(pc.full); k = len(names) - 1
        ix = np.clip(np.round((g[:, 0] - xs[0]) / cell).astype(int), 0, len(xs) - 1); iy = np.clip(np.round((g[:, 1] - ys[0]) / cell).astype(int), 0, len(ys) - 1)
        for a, b, z in zip(ix, iy, g[:, 2]):
            if z < zmin[b, a]: zmin[b, a] = z; who[b, a] = k
    rows = {}
    def check(pts, label):
        ix = np.clip(np.round((pts[:, 0] - xs[0]) / cell).astype(int), 0, len(xs) - 1); iy = np.clip(np.round((pts[:, 1] - ys[0]) / cell).astype(int), 0, len(ys) - 1)
        zm = zmin[iy, ix]; bad = pts[:, 2] > zm + 1e-6
        for p, a, b, w, zz in zip(pts[bad], ix[bad], iy[bad], who[iy[bad], ix[bad]], zm[bad]):
            key = (label, names[w])
            r = rows.get(key)
            depth = float(p[2] - zz)
            if r is None or depth > r['mm']:
                rows[key] = dict(cap_item=label, dome_item=names[w], mm=round(depth, 2), at=[round(float(v), 1) for v in p])
    for pc in P:
        if pc.attach == 'cap': check(pc.volume_grid(h), pc.full)
    # cap solid features above the seam plane (supports, posts), sampled on their surface
    V = cap_solid.V; sel = V[V[:, 2] > 12.5]
    if len(sel): check(sel, 'cap solid >z12.5')
    out = sorted(rows.values(), key=lambda r: -r['mm'])
    return out


# ------------------------------------------------------------------ NeoPixel ring proxy (mesh solid) from the LED spec
def ring_mesh_arrays(bvh_dome, zc, width=10.0, thick=3.0, lift=0.45, N=900):
    """strip proxy: 10 wide (along the shoulder profile) x 3 thick, centreline = inner-skin contour at zc, lifted `lift` off the wall
    (a flat 10 mm strip on the concave shoulder sags ~0.43 into the wall at its edges, so it is held off by that amount)."""
    P = []; Nn = []
    for k in range(N):
        a = math.pi / 2 + 2 * math.pi * k / N
        d = Vector((-math.cos(a), -math.sin(a), 0)); p = Vector((300 * math.cos(a), 300 * math.sin(a), zc))
        h1 = bvh_dome.ray_cast(p, d); h2 = bvh_dome.ray_cast(h1[0] + d * 1e-3, d)
        P.append(np.array(h2[0][:])); Nn.append(-np.array(h2[1][:]))
    P = np.array(P); Nn = np.array(Nn)
    V = []
    for k in range(N):
        tc = P[(k + 1) % N] - P[k - 1]; tc /= np.linalg.norm(tc)
        n = Nn[k] / np.linalg.norm(Nn[k]); q = np.cross(n, tc)
        if q[2] < 0: q = -q
        q /= np.linalg.norm(q); inn = -n
        for s, t in ((-width / 2, lift), (width / 2, lift), (width / 2, lift + thick), (-width / 2, lift + thick)):
            V.append(P[k] + s * q + t * inn)
    V = np.array(V); F = []
    for k in range(N):
        k2 = (k + 1) % N
        for e in range(4):
            e2 = (e + 1) % 4
            a, b, c, d = 4 * k + e, 4 * k + e2, 4 * k2 + e2, 4 * k2 + e
            F.append((a, b, c)); F.append((a, c, d))
    return V, np.array(F), P


def ring_solid(bvh_dome, zc, **kw):
    V, F, P = ring_mesh_arrays(bvh_dome, zc, **kw)
    S = Solid('ring', V, F)
    # orientation: make sure solid is outward oriented by testing a centre point (cross-section centroid)
    c = V[0:4].mean(0)
    n_hits = 0; d = Vector((0.31, 0.62, 0.72)).normalized(); q = Vector(c.tolist())
    for _ in range(40):
        h = S.bvh.ray_cast(q, d)
        if h[0] is None: break
        n_hits += 1; q = h[0] + d * 1e-4
    S.centre_inside = (n_hits % 2 == 1)
    return S, P


# ------------------------------------------------------------------ Blender proxy objects
def make_blender_proxies(P, ring=None, coll_name='COMPONENTS_4.2'):
    """one object per component `comp_<comp>` (pieces joined), props dims/source/attach; collection hidden."""
    if coll_name in bpy.data.collections:
        c = bpy.data.collections[coll_name]
        for o in list(c.objects):
            me = o.data; bpy.data.objects.remove(o, do_unlink=True)
            if me and me.users == 0 and isinstance(me, bpy.types.Mesh) and not me.name.startswith('neptr'): bpy.data.meshes.remove(me)
    else:
        c = bpy.data.collections.new(coll_name); bpy.context.scene.collection.children.link(c)
    by = {}
    for pc in P: by.setdefault(pc.comp, []).append(pc)
    mat_cache = {}
    for comp, pcs in by.items():
        bm = bmesh.new()
        for pc in pcs:
            V, F = pc.mesh_geom()
            vs = [bm.verts.new(v.tolist()) for v in V]
            for f in F:
                try: bm.faces.new([vs[i] for i in f])
                except ValueError: pass
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
        me = bpy.data.meshes.new('comp_' + comp); bm.to_mesh(me); bm.free()
        o = bpy.data.objects.new('comp_' + comp, me); c.objects.link(o)
        dims = {}
        for pc in pcs:
            dims[pc.name] = dict(box=dict(c=[round(x, 3) for x in pc.c], size=[round(x, 3) for x in pc.size], rz=round(math.degrees(pc.rz), 2), ry=round(math.degrees(pc.ry), 2))) if pc.kind == 'box' \
                else dict(cyl=dict(p0=[round(x, 3) for x in pc.p0], p1=[round(x, 3) for x in pc.p1], d=round(pc.r * 2, 3)))
        o['dims'] = json.dumps(dims)
        o['source'] = json.dumps({pc.name: pc.src for pc in pcs})
        o['attach'] = pcs[0].attach
        o['assumed'] = any(pc.assumed for pc in pcs)
        o.display_type = 'WIRE'
    if ring is not None:
        V, F, spec = ring
        bm = bmesh.new(); vs = [bm.verts.new(v.tolist()) for v in V]
        for f in F: bm.faces.new([vs[i] for i in f])
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
        me = bpy.data.meshes.new('comp_neopixel_ring'); bm.to_mesh(me); bm.free()
        o = bpy.data.objects.new('comp_neopixel_ring', me); c.objects.link(o)
        o['dims'] = json.dumps(spec['dims']); o['source'] = json.dumps(spec['source']); o['attach'] = 'dome'; o['assumed'] = True
        o.display_type = 'WIRE'
    # installed copies of the sleds (shared mesh data, installed matrix)
    from mathutils import Matrix
    for nm in ('neptr_pi_sled_z6', 'neptr_pi_sled_z10'):
        s = bpy.data.objects[nm]; M = s.get('installed_matrix')
        o = bpy.data.objects.new('comp_' + nm.replace('neptr_', '') + '_installed', s.data); c.objects.link(o)
        if M is not None: o.matrix_world = Matrix([M[i * 4:(i + 1) * 4] for i in range(4)])
        o['dims'] = json.dumps(dict(bbox_mm='70 x 89 x ' + ('6.6' if '6' in nm[-3:] else '10.6'))); o['source'] = json.dumps('handoff 11b (installed transform)'); o['attach'] = 'cap'; o['assumed'] = False
        o.display_type = 'WIRE'
    # hide: collection + layer
    c.hide_render = True; c.hide_viewport = True
    def walk(l):
        yield l
        for ch in l.children: yield from walk(ch)
    for l in walk(bpy.context.view_layer.layer_collection):
        if l.name == coll_name: l.hide_viewport = True
    return c


# ------------------------------------------------------------------ cable routing (A* on a 2 mm voxel grid, 8 mm channel)
import heapq
GX0, GY0, GZ0, GD = -126.0, -106.0, 0.0, 2.0
GN = (int(252 / GD) + 1, int(212 / GD) + 1, int(66 / GD) + 1)


def g_idx(p): return (int(round((p[0] - GX0) / GD)), int(round((p[1] - GY0) / GD)), int(round((p[2] - GZ0) / GD)))
def g_pos(i): return (GX0 + i[0] * GD, GY0 + i[1] * GD, GZ0 + i[2] * GD)


def dilate(a, r_cells):
    out = a.copy(); R = int(math.ceil(r_cells))
    for dx in range(-R, R + 1):
        for dy in range(-R, R + 1):
            for dz in range(-R, R + 1):
                if (dx, dy, dz) == (0, 0, 0) or dx * dx + dy * dy + dz * dz > r_cells * r_cells + 1e-9: continue
                sx = slice(max(0, dx), GN[0] + min(0, dx)); tx = slice(max(0, -dx), GN[0] + min(0, -dx))
                sy = slice(max(0, dy), GN[1] + min(0, dy)); ty = slice(max(0, -dy), GN[1] + min(0, -dy))
                sz = slice(max(0, dz), GN[2] + min(0, dz)); tz = slice(max(0, -dz), GN[2] + min(0, -dz))
                out[tx, ty, tz] |= a[sx, sy, sz]
    return out


def shell_occ(solid):
    occ = np.zeros(GN, bool)
    for iy in range(GN[1]):
        for iz in range(GN[2]):
            y = GY0 + iy * GD; z = GZ0 + iz * GD + 1e-3 * 0.37
            q = Vector((-200, y + 0.0137, z + 0.0071)); xs = []
            for _ in range(60):
                h = solid.bvh.ray_cast(q, Vector((1, 0, 0)))
                if h[0] is None: break
                xs.append(h[0].x); q = h[0] + Vector((1e-4, 0, 0))
            for a, b in zip(xs[0::2], xs[1::2]):
                i0 = max(0, int(math.ceil((a - GX0) / GD - 0.5))); i1 = min(GN[0] - 1, int(math.floor((b - GX0) / GD + 0.5)))
                if i1 >= i0: occ[i0:i1 + 1, iy, iz] = True
    return occ


def piece_occ(pc):
    occ = np.zeros(GN, bool)
    g = pc.volume_grid(1.0)
    if len(g) == 0: return occ
    ix = np.clip(np.round((g[:, 0] - GX0) / GD).astype(int), 0, GN[0] - 1); iy = np.clip(np.round((g[:, 1] - GY0) / GD).astype(int), 0, GN[1] - 1); iz = np.clip(np.round((g[:, 2] - GZ0) / GD).astype(int), 0, GN[2] - 1)
    occ[ix, iy, iz] = True
    return occ


NB = [(dx, dy, dz) for dx in (-1, 0, 1) for dy in (-1, 0, 1) for dz in (-1, 0, 1) if (dx, dy, dz) != (0, 0, 0)]


def astar(blocked, s, e):
    si, ei = g_idx(s), g_idx(e)
    gs = np.full(GN, np.inf, np.float32); par = {}
    gs[si] = 0; h = [(0.0, si)]; ex = np.array(ei); closed = set()
    while h:
        f, cur = heapq.heappop(h)
        if cur == ei: break
        if cur in closed: continue
        closed.add(cur); gc = gs[cur]
        for d in NB:
            n = (cur[0] + d[0], cur[1] + d[1], cur[2] + d[2])
            if n[0] < 0 or n[1] < 0 or n[2] < 0 or n[0] >= GN[0] or n[1] >= GN[1] or n[2] >= GN[2]: continue
            if blocked[n]: continue
            c = gc + GD * math.sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2])
            if c < gs[n]:
                gs[n] = c; par[n] = cur
                heapq.heappush(h, (c + float(np.linalg.norm(np.array(n) - ex)) * GD, n))
    if ei not in par and si != ei: return None
    path = [ei]
    while path[-1] != si: path.append(par[path[-1]])
    return [np.array(g_pos(i)) for i in path[::-1]]


def simplify(path, blocked, step=0.5):
    if path is None: return None
    def clear(a, b):
        L = np.linalg.norm(b - a); n = max(2, int(L / step))
        for t in np.linspace(0, 1, n):
            i = g_idx(a + (b - a) * t)
            if blocked[i]: return False
        return True
    out = [path[0]]; i = 0
    while i < len(path) - 1:
        j = len(path) - 1
        while j > i + 1 and not clear(path[i], path[j]): j -= 1
        out.append(path[j]); i = j
    return out


def bend_radii(wp):
    out = []
    for a, b, c in zip(wp[:-2], wp[1:-1], wp[2:]):
        u = b - a; v = c - b; lu, lv = np.linalg.norm(u), np.linalg.norm(v)
        th = math.acos(np.clip(np.dot(u, v) / (lu * lv), -1, 1))
        if th < 1e-3: out.append(1e9); continue
        out.append(min(lu, lv) / 2 / math.tan(th / 2))
    return out


# ------------------------------------------------------------------ full audit driver
POSTS = [(-25.15, 30.7), (23.85, 30.7), (-25.15, -27.2), (23.85, -27.2)]       # sled pins through Pi M2.5 holes
JOY = [(-76, 32), (-76, 52), (-50, 52), (-62, 42)]                                # cap supports / peg through KY-023 holes
PEGS = [(-33, -47.1), (33, -47.1), (-33, 42.9), (33, 42.9)]                       # screen pegs through the screen PCB holes


def _near(pt, lst, r): return any(math.hypot(pt[0] - a, pt[1] - b) < r for a, b in lst)


def ignore_pass_through(pc, S, pt):
    if pc.comp == 'pi5' and pc.name == 'pcb' and S.name.startswith('neptr_pi_sled') and _near(pt, POSTS, 3.2): return True
    if pc.comp == 'joystick_ky023' and S.name == 'cap' and _near(pt, JOY, 4.0) and pt[2] > 45.0: return True
    if pc.comp == 'screen' and pc.name == 'pcb' and S.name == 'dome' and _near(pt, PEGS, 4.0): return True
    return False


def full_audit(P, label='', do_routes=True):
    out = {}
    solids = make_solids()
    v17 = bpy.data.objects['neptr_top_dome_4.2_backup_v17']  # dots-free dome for contour/ray work (live dome has 565 tiny holes)
    V17, T17 = mesh_arrays(v17); b17 = BVHTree.FromPolygons(V17.tolist(), T17.tolist())
    out['pairs'] = audit_pairs(P)
    out['shells'] = audit_shells(P, solids, ignore=ignore_pass_through)
    spec = json.loads(bpy.data.objects['dome_cut_led_dots']['spec']); zc = spec['zc_inner']
    R, Pc = ring_solid(b17, zc)
    rr = audit_shells(P, {'ring': R}, h=0.8)
    pts = np.vstack([R.V, R.V[R.T].mean(1)]); rd = []
    for key in ('dome', 'cap'):
        S = solids[key]; ins = S.inside(pts)
        if ins.any():
            d = S.depth(pts[ins]); rd.append(dict(a='neopixel_ring', pa='strip', b=key, mm=round(float(d.max()), 2), n=int(ins.sum()), at=[round(float(x), 1) for x in pts[ins][d.argmax()]], assumed=True))
    for r in rd + rr: r['sev'] = severity(r['mm'], r.get('assumed', False))
    out['ring'] = rd + rr
    out['lowering'] = audit_lowering(P, solids['dome'], solids['cap'])
    if do_routes:
        class _S: pass
        sd = _S(); sd.bvh = b17
        occ = shell_occ(sd) | shell_occ(solids['cap']) | shell_occ(solids['neptr_pi_sled_z6'])
        shell_d = dilate(occ, 2)
        pocc = {pc.full: piece_occ(pc) for pc in P}
        cab = cable_defs(P); cd = {c['name']: c for c in cab}
        order = ['ethernet', 'hdmi', 'usbc_pd', 'usbA_A', 'usbA_B', 'usbA_C', 'teensy_usb', 'trs'] + [c['name'] for c in cab if c['name'].startswith('jack')]
        routed = np.zeros(GN, bool); res = {}
        for nm in order:
            c = cd[nm]; others = np.zeros(GN, bool)
            for k, v in pocc.items():
                if k not in c['own']: others |= v
            blocked = shell_d | dilate(others, 2) | routed
            for pnt in (c['start'], c['end']):
                i = g_idx(pnt); r = 4
                sl = tuple(slice(max(0, i[a] - r), i[a] + r + 1) for a in range(3)); blocked[sl] = shell_d[sl]
            path = astar(blocked, c['start'], c['end'])
            if path is None: res[nm] = dict(ok=False); continue
            wp = simplify(path, blocked); br = bend_radii(wp)
            res[nm] = dict(ok=True, len=round(sum(float(np.linalg.norm(b - a)) for a, b in zip(wp[:-1], wp[1:])), 1), minR=round(min(br), 1) if br else None, wp=[w.round(1).tolist() for w in wp])
            pa = np.zeros(GN, bool)
            for a, b in zip(path[:-1], path[1:]): pa[g_idx(a)] = True
            pa[g_idx(path[-1])] = True; routed |= dilate(pa, 4)
        out['routes'] = res
    return out


def summarize(out):
    S = {}
    S['pairs'] = [(r['a'] + '.' + r['pa'], r['b'] + '.' + r['pb'], r['mm'], r['vol'], r['at'], 'A' if r['assumed'] else 'M', r['sev']) for r in out['pairs']]
    S['shells'] = [(r['a'] + '.' + r['pa'], r['b'], r['mm'], r['n'], r['at'], 'A' if r['assumed'] else 'M', r['sev']) for r in out['shells']]
    S['ring'] = [(r['a'] + '.' + r['pa'], r['b'], r['mm'], r['n'], r['at'], r['sev']) for r in out['ring']]
    S['lowering'] = out['lowering'][:20]
    if 'routes' in out:
        S['routes'] = {k: ({kk: vv for kk, vv in v.items() if kk != 'wp'}) for k, v in out['routes'].items()}
    return S


def near_pairs(P, thr=2.0, h=0.8, skip=lambda a, b: False):
    """pieces of different comps closer than thr (surface-to-surface, sampled)"""
    rows = {}
    n = len(P)
    for i in range(n):
        A = P[i]
        for j in range(i + 1, n):
            B = P[j]
            if A.comp == B.comp or skip(A, B): continue
            gap = np.maximum(0, np.maximum(A.lo - B.hi, B.lo - A.hi))
            if np.linalg.norm(gap) >= thr: continue
            pa = A.surface_points(h); pb = B.surface_points(h)
            pa = pa[((pa >= B.lo - thr) & (pa <= B.hi + thr)).all(1)]; pb = pb[((pb >= A.lo - thr) & (pb <= A.hi + thr)).all(1)]
            if len(pa) == 0 or len(pb) == 0: continue
            d = np.inf
            for k in range(0, len(pa), 2000):
                dd = np.linalg.norm(pa[k:k + 2000, None, :] - pb[None, :, :], axis=2).min()
                d = min(d, dd)
            if d < thr:
                key = (A.comp, B.comp)
                if key not in rows or d < rows[key]['gap']: rows[key] = dict(a=A.full, b=B.full, gap=round(float(d), 2))
    return sorted(rows.values(), key=lambda r: r['gap'])
