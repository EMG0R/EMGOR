"""HYPERTRUMPET V4 shared parameters + 2D silhouette (imported by hypertrumpet_v4_build.py). Frame: x along horn, y lateral (pod +y), z valve axis up, mm."""
import numpy as np
from scipy import ndimage as ndi

# ---------------- locked control table (hole SIZES unchanged) ----------------
TRELLIS_X = (830.6, 845.6, 860.7, 875.7); TRELLIS_Z = (67.6, 82.7, 97.7, 112.8); TRELLIS_WH = (11.0, 11.1)
FADER_WH = (5.6, 41.0); FADER_OUTER_SPAN = 51.0          # EMGOR: outer edge of left slot to outer edge of right slot = 51 mm, middle centred; slots oversized for slop
FADER_PITCH = (FADER_OUTER_SPAN - FADER_WH[0]) / 2.0                    # 10.95
FADER_X_MID = 852.7
KNOB_LEN = 10.0; LEVER_W = 3.0                                          # [A] 10 mm knob, 3 mm lever, lever at top of travel
LIDAR_WH = (11.0, 8.0); LIDAR_FAR_EDGE_FROM_KNOB = 25.0
TRELLIS_BOTTOM_EDGE = TRELLIS_Z[0] - TRELLIS_WH[1] / 2                  # 62.05
LIDAR_GAP_TO_TRELLIS = FADER_OUTER_SPAN / 2 - FADER_WH[0] / 2 - FADER_PITCH + (FADER_PITCH - FADER_WH[0])  # placeholder (overwritten below)
LIDAR_GAP_TO_TRELLIS = 10.85                                            # lidar + faders pulled down a further 4 mm (EMGOR)
LIDAR_TOP = TRELLIS_BOTTOM_EDGE - LIDAR_GAP_TO_TRELLIS                  # 55.20
KNOB_LOWER_TOP = LIDAR_TOP - LIDAR_FAR_EDGE_FROM_KNOB                   # 30.20 (knob lower face, middle fader at top of travel)
FADER_Z = KNOB_LOWER_TOP - (FADER_WH[1] / 2 - LEVER_W / 2 - KNOB_LEN / 2)   # slot centre z = 17.5
FADER_POS = [(FADER_X_MID + i * FADER_PITCH, FADER_Z) for i in (-1, 0, 1)]
LIDAR_POS = (FADER_X_MID, LIDAR_TOP - LIDAR_WH[1] / 2)
GROUP_SHIFT = 4.0
ROUND_HOLES = [(749.7 + GROUP_SHIFT, 41.8), (749.7 + GROUP_SHIFT, 101.5), (806.7 + GROUP_SHIFT, 51.3), (806.7 + GROUP_SHIFT, 95.0)]   # knobs spread 5 mm further apart in z
ROUND_WH = (8.7, 8.3); ROUND_WH = (8.7, 8.3)
JOY_POS = (776.7 + GROUP_SHIFT, 72.3); JOY_WH = (29.4, 28.0)
CONTROLS = ([("trellis_%d%d" % (i, j), (TRELLIS_X[i], TRELLIS_Z[j]), TRELLIS_WH, "rbox") for i in range(4) for j in range(4)]
            + [("fader_%d" % i, FADER_POS[i], FADER_WH, "rbox") for i in range(3)] + [("lidar", LIDAR_POS, LIDAR_WH, "stadium")]
            + [("round_%d" % i, ROUND_HOLES[i], ROUND_WH, "ellipse") for i in range(4)] + [("joystick", JOY_POS, JOY_WH, "ellipse")])

# ---------------- depth / plate ----------------
PLATE_FRONT = 541.0            # mount plate front face (y)
YB = PLATE_FRONT + 0.6         # shell back rim
WALL = 2.2
FACE_Y = 568.4                 # THE control face: flat, constant y. clear depth behind skin = FACE_Y-WALL-PLATE_FRONT = 25.2 (Seed >=20, joystick >=18.9, battery-free)
ROLL_W, ROLL_H = 11.0, 16.0    # edge roll: horizontal / vertical extent of the quarter-ellipse
MARGIN = ROLL_W + 3.2          # control hole edge -> silhouette edge (flat zone + 3.2 web)

# ---------------- Seed / switch / battery ----------------
SEED_C = (788.0, 6.0); SEED_L, SEED_W = 51.0, 18.0   # Seed 3 lies along x: USB-C end at the -x (mouthpiece) wall     # Daisy Seed 3 lying along x, USB-C end at -x (mouthpiece side); PCB centre
SWITCH_Z = -5.0; SWITCH_Y = 552.0                        # toggle on the -x wall, above the battery bay, actuator facing the mouthpiece (-x)
HOLDER = (77.0, 24.0, 21.0)    # [A] typical 21700 single PCB-mount holder L x W x H (B0GFSMXJF7 dims not in repo docs)
HCLR = 0.4
BAY_CX = 853.5
RIMW = 1.2
# v5.0 battery housing: clear interior >= 50 x 85 x 30 (battery box at the +x end), 24 mm switch zone at the -x end, 6 mm walls, 6 mm outer radii
BAY_X = (781.0, 892.9); BAY_Y = (505.2, 558.2); BAY_ZTOP = -18.7; BAY_H = 30.4; BAY_ZBOT = BAY_ZTOP - BAY_H
BATT_BOX = ((805.0, 890.0), (506.7, 556.7), (BAY_ZBOT + 0.2, BAY_ZBOT + 30.2))       # 85 x 50 x 30 test volume
DOOR_T = 2.4; KEEL_W = 8.0; KEEL_R = 8.0
KEEL_X = (BAY_X[0] - KEEL_W, BAY_X[1] + KEEL_W); KEEL_Y = (BAY_Y[0] - KEEL_W, BAY_Y[1] + KEEL_W); KEEL_Z = (BAY_ZBOT - DOOR_T, BAY_ZTOP + WALL)
SW_Y = 0.5 * (KEEL_Y[0] + KEEL_Y[1]); SW_Z = 0.5 * (KEEL_Z[0] + KEEL_Z[1])           # power switch hole centre on the -x face


def smin(a, b, k):
    h = np.maximum(k - np.abs(a - b), 0) / k
    return np.minimum(a, b) - h * h * k / 4


def d_rbox(X, Z, c, half, r):
    qx = np.abs(X - c[0]) - (half[0] - r); qz = np.abs(Z - c[1]) - (half[1] - r)
    return np.hypot(np.maximum(qx, 0), np.maximum(qz, 0)) + np.minimum(np.maximum(qx, qz), 0) - r


def d_capsule(X, Z, a, b, R):
    ax, az = a; bx, bz = b; dx, dz = bx - ax, bz - az; L2 = dx * dx + dz * dz
    t = np.clip(((X - ax) * dx + (Z - az) * dz) / L2, 0, 1)
    return np.hypot(X - ax - t * dx, Z - az - t * dz) - R


def d_circle(X, Z, c, R): return np.hypot(X - c[0], Z - c[1]) - R


def backbone_raw(X, Z, k=18.0):
    """v3.1 'backbone' (Trellis block + arm to joystick/pots + fader bank), plus the Seed lobe, as one smooth union"""
    f = d_rbox(X, Z, (853.2, 90.2), (38.5, 38.5), 18.0)
    f = smin(f, d_rbox(X, Z, (849.9, FADER_Z), (36.8, 30.0), 18.0), k)        # +x side pulled in 3 mm (tuning-slide access), Trellis block untouched
    f = smin(f, d_rbox(X, Z, (780.7, 29.0), (16.0, 30.0), 12.0), k)
    f = smin(f, d_rbox(X, Z, (774.0, 22.0), (24.0, 24.0), 8.0), 24.0)                          # flat vertical -x side wall at the Seed's USB height (no slanted face)                      # Seed / switch lobe
    f = smin(f, d_circle(X, Z, JOY_POS, 26.0), k)
    f = smin(f, d_capsule(X, Z, ROUND_HOLES[0], ROUND_HOLES[1], 13.5), k)
    f = smin(f, d_capsule(X, Z, ROUND_HOLES[2], ROUND_HOLES[3], 13.5), k)
    f = smin(f, d_capsule(X, Z, (LIDAR_POS[0], 38.0), (LIDAR_POS[0], LIDAR_POS[1] + 3), 11.0), k)
    return f


def hole_outline_pts(m=0.0):
    P = []
    for name, (cx, cz), (w, h), kind in CONTROLS:
        for a in np.linspace(0, 2 * np.pi, 48, endpoint=False):
            if kind == "ellipse": P.append((cx + (w / 2 + m) * np.cos(a), cz + (h / 2 + m) * np.sin(a)))
            else:
                ex = np.sign(np.cos(a)) * (w / 2) + m * np.cos(a); ez = np.sign(np.sin(a)) * (h / 2) + m * np.sin(a); P.append((cx + ex, cz + ez))
    return np.array(P)


_SIL = {}


def silhouette(res=0.25):
    """smoothed backbone silhouette as a bilinear-interpolable 2D signed distance, grown so every hole edge is >= MARGIN inside"""
    if "f" in _SIL: return _SIL["f"]
    xs = np.arange(700, 930, res); zs = np.arange(-60, 170, res)
    X, Z = np.meshgrid(xs, zs, indexing="ij")
    f = backbone_raw(X, Z)
    f = ndi.gaussian_filter(f, 8.0 / res)                      # soften the concave notches and corners (the 'pebble' rounding of the backbone)
    P = hole_outline_pts(0.0)
    ix = (P[:, 0] - xs[0]) / res; iz = (P[:, 1] - zs[0]) / res
    dd = ndi.map_coordinates(f, [ix, iz], order=1)
    grow = MARGIN + dd.max()                                   # dd is negative inside; need dd <= -MARGIN
    f = f - grow
    _SIL["f"] = (xs, zs, f, grow)
    return _SIL["f"]


def sil_sd(X, Z):
    xs, zs, f, grow = silhouette()
    res = xs[1] - xs[0]
    ix = (np.asarray(X, float) - xs[0]) / res; iz = (np.asarray(Z, float) - zs[0]) / res
    ix, iz = np.broadcast_arrays(ix, iz)
    return ndi.map_coordinates(f, [ix, iz], order=1, mode="nearest").astype(np.float32)


if __name__ == "__main__":
    import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
    xs, zs, f, grow = silhouette(); print("grow", grow)
    fig, ax = plt.subplots(figsize=(11, 10)); ax.contour(xs, zs, f.T, levels=[-ROLL_W, 0], colors=["gray", "k"])
    for name, (cx, cz), (w, h), kind in CONTROLS:
        ax.add_patch(plt.Rectangle((cx - w / 2, cz - h / 2), w, h, fill=False, ec="purple", lw=0.8))
    ax.add_patch(plt.Rectangle((BAY_X[0], BAY_Y[0] * 0 + -60), 1, 1, fill=False))
    ax.set_aspect("equal"); ax.set_xlim(720, 910); ax.set_ylim(-50, 145); ax.invert_xaxis(); fig.savefig("/private/tmp/claude-501/-Users-emgor/f9645495-f198-4180-aee5-9e133ca60c53/scratchpad/ht4/sil.png", dpi=70)
    print("lidar", LIDAR_POS, "fader z", FADER_Z, "knob", KNOB_LOWER_TOP, "lid top", LIDAR_TOP)

PORT_Z = 6.0; PORT_Y = 555.2          # side-face ports: USB-C through the -x (mouthpiece) wall, 3.5 mm jack through the +x (bell) wall, same z and y, opposite each other
PALM_C = (835.0, 567.0, -27.0); PALM_HALF_X = 66.0; PALM_TILT = -20.0


# v5.2 clam door: parting line on the side band, wrap height 16 mm above the flat bottom
DOOR_WRAP = 16.0; DOOR_ZP = KEEL_Z[0] + DOOR_WRAP; DOOR_LIP = 1.5; DOOR_GAP = 0.2; BEAD_H = 0.7; BEAD_ZB = DOOR_ZP - 3.5; DOOR_PLATE = 1.6
DENT = dict(cx=837.0, cz=-36.0, ax=29.0, az=17.0, depth=4.0)
