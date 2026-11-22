"""v5.9 mount: remove the dovetail tab (x 867-890, z 8-101, top y 547) and rebuild the plate back surface from a smooth heightfield.
Run from the work dir (needs geom.py from cad/v5_8_src): python mount_dovetail.py [in.npz] [out.npz]"""
import sys, os
sys.path.insert(0, os.getcwd()); sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'v5_8_src'))
import numpy as np, trimesh, manifold3d as mf
from scipy import ndimage as ndi
from scipy.interpolate import RBFInterpolator
from geom import to_m, from_m, heal_export, save_npz
src = sys.argv[1] if len(sys.argv) > 1 else 'cur/mount_original_arms_plate_v4.npz'
dst = sys.argv[2] if len(sys.argv) > 2 else 'out/mount_v59.npz'
z = np.load(src); M = trimesh.Trimesh(z['v'], z['f'], process=False)
h = 0.25; X0, X1, Z0, Z1 = 857.0, 896.0, 0.0, 109.0
xs = np.arange(X0, X1 + h/2, h); zs = np.arange(Z0, Z1 + h/2, h)
GX, GZ = np.meshgrid(xs, zs, indexing='ij')
o = np.c_[GX.ravel(), np.full(GX.size, 560.0), GZ.ravel()]
loc, ir, _ = M.ray.intersects_location(o, np.tile([0, -1.0, 0], (len(o), 1)), multiple_hits=False)
Y = np.full(GX.size, np.nan); Y[ir] = loc[:, 1]; Y = Y.reshape(GX.shape)
RODS = [(738.0, 72.0), (892.5, 111.0), (859.0, -8.5), (894.5, 45.0)]
nr = np.min([np.hypot(GX - rx, GZ - rz) for rx, rz in RODS], axis=0)       # distance to nearest rod axis
tab = (np.nan_to_num(Y, nan=0) > 540.3) & (GX < 891.5) & (GZ > 5) & (GZ < 103.5)
tab = ndi.binary_opening(tab, iterations=3)
cut = ndi.binary_dilation(tab, structure=ndi.generate_binary_structure(2, 2), iterations=int(round(3.0 / h))) & (nr > 5.6)   # footprint + 3 mm (fillets), clear of the rod roots
# no rod / plate-edge material in the cut region
bad = cut & (np.nan_to_num(Y, nan=0) > 541.5) & ~tab
if bad.any(): print('non-tab high pts in cut', int(bad.sum()), GX[bad].min(), GX[bad].max(), GZ[bad].min(), GZ[bad].max())
print('tab bbox', GX[tab].min(), GX[tab].max(), GZ[tab].min(), GZ[tab].max(), 'cut bbox', GX[cut].min(), GX[cut].max(), GZ[cut].min(), GZ[cut].max())
known = ~cut & np.isfinite(Y) & (Y < 541.0) & (Y > 536.0) & (nr > 7.0) & (GX < 892.0)
# only ring points close to the cut for the fit
ring = known & ndi.binary_dilation(cut, iterations=int(round(8 / h)))
pts = np.c_[GX[ring], GZ[ring]]; val = Y[ring]
sel = np.random.default_rng(0).choice(len(pts), min(4000, len(pts)), replace=False)
rbf = RBFInterpolator(pts[sel], val[sel], kernel='thin_plate_spline', smoothing=1e-3)
Yf = Y.copy(); un = cut | ~np.isfinite(Y)
# unknowns that are outside the part (right of the plate edge) are not part of the cut region
Yf[cut] = rbf(np.c_[GX[cut], GZ[cut]])
print('fit residual on ring (mm): rms', float(np.sqrt(np.mean((rbf(pts) - val) ** 2))), 'max', float(np.abs(rbf(pts) - val).max()))
# smooth interior of the cut a little (keeps exact values outside)
Ys = ndi.gaussian_filter(np.where(np.isfinite(Yf), Yf, 539.5), 1.5)
Yf[cut] = Ys[cut]
# region of the hole: cut mask rectangle bbox with the cut footprint (dilated polygon of the mask)
import skimage.measure as sm
lab = ndi.label(cut)[0]; assert lab.max() == 1, lab.max()
YB = 538.0   # cutter floor, inside the plate (back surface ~539.5, plate underside ~534)
# cutter: prism from YB up, footprint = cut mask contour (vertical walls)
def mask_poly(mk):
    from shapely.geometry import Polygon
    c = sm.find_contours(np.pad(mk, 1).astype(float), 0.5)[0] - 1
    P = Polygon(np.c_[X0 + c[:, 0] * h, Z0 + c[:, 1] * h]).buffer(0).simplify(0.02)
    return P
P = mask_poly(cut)
def prism(P, y0, y1):
    ex = trimesh.creation.extrude_polygon(P, y1 - y0)            # extrudes along +z of polygon plane
    V = ex.vertices; Pm = np.c_[V[:, 0], y0 + V[:, 2], V[:, 1]]
    m = trimesh.Trimesh(Pm, ex.faces[:, ::-1], process=False)
    if m.volume < 0: m.invert()
    return m
cutter = prism(P, YB, 552.0)
# filler: heightfield solid over cut footprint grown by 0.5 mm (laterally buried), top = fit inside, 0.05 below old surface in the buried ring
Pg = P.buffer(0.5)
Ygrid = np.where(np.isfinite(Yf), Yf, 539.5)
def top_at(x, zq):
    return ndi.map_coordinates(Ygrid, [(x - X0) / h, (zq - Z0) / h], order=3, mode='nearest')
Bx = np.array(Pg.bounds); gh = 0.25
from shapely import contains_xy
fx = np.arange(Bx[0] - 1.0, Bx[2] + 1.0 + gh, gh); fz = np.arange(Bx[1] - 1.0, Bx[3] + 1.0 + gh, gh)
FX, FZ = np.meshgrid(fx, fz, indexing='ij')
inP = contains_xy(P, FX, FZ)
import shapely
dP = np.where(inP, 1.0, -1.0) * shapely.distance(shapely.points(FX, FZ), P.boundary)       # + inside P (hole), exact
low = 0.04 * np.clip(-dP / 0.3, 0, 1)                                          # buried rim: 0.06 below the old surface outside/at the hole wall
TOP = top_at(FX, FZ) - low
nx, nz = TOP.shape; idx = np.arange(nx * nz).reshape(nx, nz)
Vt = np.c_[FX.ravel(), TOP.ravel(), FZ.ravel()]; Vb = np.c_[FX.ravel(), np.full(nx * nz, 537.4), FZ.ravel()]
V = np.r_[Vt, Vb]; Ft = []
a_, b_, c_, d_ = idx[:-1, :-1].ravel(), idx[1:, :-1].ravel(), idx[1:, 1:].ravel(), idx[:-1, 1:].ravel()
Ft = np.r_[np.c_[a_, d_, c_], np.c_[a_, c_, b_]]            # top normal +y when x then z ... fixed below
box_ = trimesh.Trimesh(V, Ft, process=False)
# boundary skirt + bottom via convex-free construction: use hull-free approach -> build side walls from boundary edges
E = box_.edges_sorted; u, cnt = np.unique(E, axis=0, return_counts=True); be = u[cnt == 1]
off = nx * nz; side = []
for a2, b2 in box_.edges[np.isin(box_.edges_sorted.view([('', E.dtype)] * 2).ravel(), be.view([('', E.dtype)] * 2).ravel())]:
    side.append([a2, b2, b2 + off]); side.append([a2, b2 + off, a2 + off])
bot = np.r_[np.c_[a_, c_, d_], np.c_[a_, b_, c_]] + off
Fall = np.r_[Ft, np.array(side), bot]
slab = trimesh.Trimesh(V, Fall, process=False)
slab.merge_vertices(); slab.fix_normals()
if slab.volume < 0: slab.invert()
print('slab watertight', slab.is_watertight, 'vol', slab.volume)
import fast_simplification as fs
vs, fs_ = fs.simplify(np.asarray(slab.vertices), np.asarray(slab.faces), target_reduction=0.995, agg=3)
slab = trimesh.Trimesh(vs, fs_, process=False)
print('slab simplified', len(slab.faces), slab.is_watertight)
# top-surface deviation of the simplified slab from the fit
q = slab.ray.intersects_location(np.c_[FX[::3,::3].ravel(), np.full(FX[::3,::3].size, 560.0), FZ[::3,::3].ravel()], np.tile([0,-1.0,0],(FX[::3,::3].size,1)), multiple_hits=False)
print('top simplification max dev (mm)', float(np.abs(q[0][:,1] - (TOP[::3,::3].ravel()[q[1]])).max()))
fill = from_m(to_m(slab) ^ to_m(prism(Pg, 537.0, 545.0)))
print('filler faces', len(fill.faces), fill.is_watertight)
R0 = from_m(to_m(M) - to_m(cutter))
parts = R0.split(only_watertight=False)
print('after cut bodies', [(len(b.faces), round(b.volume,1), b.bounds.round(1).tolist()) for b in parts if len(b.faces) < 5000])
R = to_m(max(parts, key=lambda b: len(b.faces)))
R = R + to_m(fill)
R = from_m(R)
bodies = R.split(only_watertight=False)
print('bodies', len(bodies))
R = max(bodies, key=lambda b: len(b.faces))
print('faces', len(R.faces), 'wt', R.is_watertight, 'genus', 1 - R.euler_number / 2)
R = heal_export(R, dst.replace('.npz', '.stl'))
print('after STL roundtrip: faces', len(R.faces), 'wt', R.is_watertight, 'genus', 1 - R.euler_number / 2)
save_npz(dst, R)
