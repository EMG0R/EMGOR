"""Genus bookkeeping for the shell: plug every intended opening and show the genus falls to the expected value.
Genus of a closed body with N separate openings into one connected air region = N - 1. Intended N (v5.3 shell):
 25 control-face openings (16 Trellis + 3 fader + lidar + 4 round + joystick) + 3 Hall holes + USB-C + 3.5 mm jack + switch + 1 battery bay (door side) + 1 dovetail channel + 1 OPEN BACK (the mount plate closes it in assembly) = 34 -> genus 33.
The v5.2 expectation of 32 left out the open back. Run: venv python genus_check.py shell.stl"""
import sys, json, numpy as np, trimesh
from scipy import ndimage as ndi
from skimage.measure import euler_number, label
from vox import voxelize
m = trimesh.load(sys.argv[1], force="mesh"); h = 0.5
lo = m.bounds[0] - 3; hi = m.bounds[1] + 3
X = voxelize(np.asarray(m.vertices), np.asarray(m.faces), lo, hi, h)
def gen(A): return int(label(A, connectivity=1).max() - euler_number(A, connectivity=1))
def ix(ax, v): return int(round((v - lo[ax]) / h - 0.5))
def fill(A, ax, a, b):
    A = A.copy()
    for q in range(ix(ax, a), ix(ax, b) + 1):
        sl = [slice(None)] * 3; sl[ax] = q; A[tuple(sl)] = ndi.binary_fill_holes(A[tuple(sl)])
    return A
res = dict(mesh_genus=float(1 - m.euler_number / 2), voxel_genus_0p5mm=gen(X))
Y = fill(X, 1, 566.4, 567.3); res["after_plugging_25_face_openings"] = gen(Y)
Y = fill(Y, 1, 541.9, 542.6); res["after_plugging_open_back"] = gen(Y)
Y = fill(Y, 1, 524, 525); res["after_plugging_3_hall_holes"] = gen(Y)
res["remaining_expected_openings"] = "USB-C, jack, switch, battery bay, dovetail channel = 5 -> genus 4"
print(json.dumps(res, indent=1)); json.dump(res, open("genus_v5_3.json", "w"), indent=1)
