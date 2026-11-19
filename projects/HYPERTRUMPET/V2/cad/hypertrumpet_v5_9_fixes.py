#!/usr/bin/env python3
"""HYPERTRUMPET V2 pass v5.9 driver (reproducible). Needs a venv with numpy scipy scikit-image trimesh manifold3d shapely fast-simplification matplotlib, and Blender 5.1 at /Applications/Blender.app (headless only).
Work dir: any scratch dir; each stage reads/writes cur/ and out/ there. Sources in cad/v5_9_src/ (copy them into the work dir; geom.py/rods.py are the v5.8 helpers).
  export  : v5.8 objects from cad/hypertrumpet_v5.blend -> cur/*.npz   (blender -b --python v5_3_src/exp.py -- cur)
  tables  : tables.py        measured pod sweep profile (z=92, bar-free) + x_o(z) outline + housing cavity profile -> exact 2D SDF tables
  stage1  : stage1_slab.py   remove the dovetail slab (rib, channel, panel, stop block) from the shell back; floor restored at the measured housing top
  stage2  : stage2_lobe.py   ONE analytic field (HRES=0.125): -x lobe + sweep + junction ramp -> marching cubes. env: ZB0=-14 TILT=0.35 KSW=12 KB=10 XT=748
  stage3  : stage3_stitch.py cut the old region out (plan distance 14 mm seam in the flat face, z=2 / z=100 planes, x=782) and union the new mesh; R1 rod boss/socket re-added; torn face hole closed
  stage4  : stage4_rim.py    floor-spur + cap-remnant cut, near-lossless decimation to <=400k, r=1 rim rounding (swept corner removers) on the old rim, keep main body
  mount   : mount_dovetail.py (dovetail tab removed, plate back restored)
  final   : heal_export STLs -> cad/stl_v5_9, bl_update_blend.py (atomic), snapshot, print_audit_v5_9.py (archives stl_v5_print to _attic/stl_v5_print_v5_8, writes oriented STLs)
Usage (from the work dir):  python hypertrumpet_v5_9_fixes.py [stage ...]"""
import os, sys, subprocess, shutil
HERE = os.path.dirname(os.path.abspath(__file__)); SRC = os.path.join(HERE, "v5_9_src"); BLENDER = "/Applications/Blender.app/Contents/MacOS/Blender"; BLEND = os.path.join(HERE, "hypertrumpet_v5.blend")
PY = sys.executable
def run(cmd, **kw): print(">>", " ".join(cmd)); subprocess.run(cmd, check=True, **kw)
def stage(name, env=None):
    e = dict(os.environ, PYTHONPATH=SRC + ":" + os.path.join(HERE, "v5_3_src")); e.update(env or {})
    run([PY, os.path.join(SRC, name)], env=e)
def main(sel):
    os.makedirs("cur", exist_ok=True); os.makedirs("out", exist_ok=True)
    for s in sel:
        if s == "export": run([BLENDER, "-b", BLEND, "--python", os.path.join(HERE, "v5_3_src", "exp.py"), "--", "cur"], stdout=subprocess.DEVNULL)
        elif s == "tables": stage("tables.py")
        elif s == "stage1": stage("stage1_slab.py")
        elif s == "stage2": stage("stage2_lobe.py", dict(HRES="0.125", ZB0="-14", TILT="0.35", KSW="12", KB="10", XT="748"))
        elif s == "stage3": stage("stage3_stitch.py")          # argv: out/lobe_mc.npz (full-resolution mesh; decimated once, globally, in stage4)
        elif s == "stage4": stage("stage4_rim.py")
        elif s == "mount": stage("mount_dovetail.py")
        elif s == "final": final()
def final():
    sys.path.insert(0, SRC); import numpy as np
    from geom import load_npz, save_npz, heal_export
    S = load_npz("out/s4.npz"); q = heal_export(S, os.path.join(HERE, "stl_v5_9", "shell_pebble_v5_9.stl")); save_npz("out/shell_final.npz", q)
    tmp = BLEND + ".v59tmp"
    run([BLENDER, "-b", BLEND, "--python", os.path.join(SRC, "bl_update_blend.py"), "--", tmp, "shell_pebble_v4=out/shell_final.npz", "mount_original_arms_plate_v4=out/mount_v59.npz"], stdout=subprocess.DEVNULL)
    shutil.copy2(tmp, os.path.join(HERE, "snapshots", "hypertrumpet_v5_9.blend")); os.replace(tmp, BLEND)
    run([PY, os.path.join(HERE, "print_audit_v5_9.py")])
if __name__ == "__main__": main(sys.argv[1:] or ["export", "tables", "stage1", "stage2", "stage3", "stage4", "mount", "final"])
