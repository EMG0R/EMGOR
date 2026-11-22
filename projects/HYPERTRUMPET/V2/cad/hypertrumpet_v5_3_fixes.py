#!/usr/bin/env python3
"""HYPERTRUMPET V2 pass v5.3 driver (reproducible). Requires a venv with numpy scipy scikit-image trimesh manifold3d shapely networkx mapbox-earcut matplotlib
(the build used a throwaway venv) and Blender 5.1 at /Applications/Blender.app. Never touches a running Blender GUI / MCP (headless only).

What it does (sources in cad/v5_3_src/, one script per stage, all operate on cad/_v5_3_work/):
  export   v5.2 shell/door/mount/caps from cad/hypertrumpet_v5.blend (after a snapshot copy) -> cur/*.npz
  V        voxelise shell+door at 0.25 mm over the housing box
  H0       close the 0.2 mm door/shell gaps (closing r 0.9)                       -> U
  H        seal wire stadium + switch hole, find the bay cavity C
  H2       big-radius (R 8->12 mm) variable opening of the housing envelope        -> top-edge rounding (wall >= 2.3 mm guard around the cavity)
  H3       apply rounding with cavity guard
  H4       palm dip (7.5 mm, 62 x 41 mm superellipse, C2 smootherstep, along the hand-rest face normal)
  H5       parting plane z = -44 (level all round, hand rest included), door floor +1.0 mm, marching cubes, Taubin, Blender decimate
  S        splice the new housing onto the untouched pod (z > -9)
  D        door snap: 1.5 mm lip 6 mm tall, 0.5 mm bead, 0.2 mm gap, groove + bead pocket in the shell, 3 nail notches, ring omitted around the switch
  L        logo: clear v5.2 engraving + boss, new 70 x 20.6 mm V-carve (logo.py), boss 565.0
  F        FSR402 pad on the bell (+x) side, top Trellis row height
  G        Seed lobe corner clearance (outward bump + 1.0 mm clearance cut) and 16 mm toggle spot-face
  final    finalize (merge, float32-safe), STLs to cad/stl_v5_3/, blend update (atomic), snapshot, print audit, fit audit
Usage: <venv>/bin/python hypertrumpet_v5_3_fixes.py [stage ...]     (no args = all)
"""
import os, sys, subprocess, shutil, json
HERE = os.path.dirname(os.path.abspath(__file__)); SRC = os.path.join(HERE, "v5_3_src"); WORK = os.path.join(HERE, "_v5_3_work")
BLENDER = "/Applications/Blender.app/Contents/MacOS/Blender"; BLEND = os.path.join(HERE, "hypertrumpet_v5.blend")
STAGES = ["export", "V", "H0", "H", "H2", "H3", "H4", "H5", "dec", "S", "D", "L", "F", "G", "final"]
def run(cmd, **kw): print(">>", " ".join(cmd)); subprocess.run(cmd, check=True, cwd=WORK, **kw)
def py(script): run([sys.executable, os.path.join(SRC, script)], env=dict(os.environ, PYTHONPATH=SRC))
def main(sel):
    os.makedirs(os.path.join(WORK, "cache"), exist_ok=True); os.makedirs(os.path.join(WORK, "cur"), exist_ok=True)
    for f in ("bl_dec.py", "bl_render.py"): shutil.copy2(os.path.join(SRC, f), WORK)
    snap = os.path.join(HERE, "snapshots", "hypertrumpet_v5_2_pre53.blend")
    for st in sel:
        if st == "export":
            if not os.path.exists(snap): shutil.copy2(BLEND, snap)
            run([BLENDER, "-b", BLEND, "--python", os.path.join(SRC, "exp.py"), "--", "cur"], stdout=subprocess.DEVNULL)
        elif st == "dec": run(["bash", os.path.join(SRC, "dec_stage.sh")], env=dict(os.environ, PYTHONPATH=SRC, PY=sys.executable, B=BLENDER))
        elif st == "final": final()
        else: py({"V": "stageV.py", "H0": "stageH0.py", "H": "stageH.py", "H2": "stageH2.py", "H3": "stageH3.py", "H4": "stageH4.py", "H5": "stageH5.py", "S": "stageS.py", "D": "stageD.py", "L": "stageL.py", "F": "stageF.py", "G": "stageG.py"}[st])
def final():
    sys.path.insert(0, SRC); import numpy as np, trimesh
    from geom import finalize, stl_ok, save_npz, load_npz
    out = os.path.join(HERE, "stl_v5_3"); os.makedirs(out, exist_ok=True)
    for src, name in (("cache/shell_g.npz", "shell_pebble_v5_3"), ("cache/door_f.npz", "battery_door_v5_3")):
        m = finalize(load_npz(os.path.join(WORK, src))); ok, q = stl_ok(m, os.path.join(out, name + ".stl")); assert ok, name; save_npz(os.path.join(WORK, name + ".npz"), q)
    for f in ("mount_original_arms_plate_v4.stl", "valve_cap_v4.stl"): shutil.copy2(os.path.join(HERE, "stl_v4", f), os.path.join(out, f))
    tmp = BLEND + ".v53tmp"; snap = os.path.join(HERE, "snapshots", "hypertrumpet_v5_3.blend")
    run([BLENDER, "-b", BLEND, "--python", os.path.join(SRC, "bl_update_blend.py"), "--", tmp,
         "shell_pebble_v4=%s" % os.path.join(WORK, "shell_pebble_v5_3.npz"), "battery_door_v4=%s" % os.path.join(WORK, "battery_door_v5_3.npz")], stdout=subprocess.DEVNULL)
    shutil.copy2(tmp, snap); os.replace(tmp, BLEND)                 # atomic replace of the live blend
    run([sys.executable, os.path.join(HERE, "print_audit_v5_3.py")])
    run([sys.executable, os.path.join(HERE, "_fit_audit_work_v5_3", "fit_audit.py"), "--blend", BLEND])
if __name__ == "__main__": main(sys.argv[1:] or STAGES)
