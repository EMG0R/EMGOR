PY=../venv/bin/python; B=/Applications/Blender.app/Contents/MacOS/Blender
$PY - <<'P'
import numpy as np, trimesh
from geom import *
for n in ('shellp','door'):
    m=clean(trimesh.load(f'cache/{n}_mc.stl',force='mesh')); parts=main_parts(m)
    big=max(parts,key=lambda p:len(p.faces)); import trimesh.smoothing as sm; sm.filter_taubin(big,lamb=0.5,nu=-0.53,iterations=30); big.export(f'cache/{n}_big.stl'); print(n,[len(p.faces) for p in parts])
P
$B -b --python bl_dec.py -- cache/shellp_big.stl cache/shellp_big_dec.stl 70000 2>&1 | grep -E "^out"
$B -b --python bl_dec.py -- cache/door_big.stl cache/door_big_dec.stl 85000 2>&1 | grep -E "^out"
