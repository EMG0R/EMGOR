set -e
PY=../venv/bin/python; B=/Applications/Blender.app/Contents/MacOS/Blender
$PY stageB.py
$PY - <<'P'
import trimesh
from geom import *
for n in ('door','shell_low'):
    load_npz(f'out/{n}_mc.npz').export(f'out/{n}_mc.stl')
P
$B -b --python bl_dec.py -- out/door_mc.stl out/door_dec.stl 110000 2>&1 | grep -E "^out"
$B -b --python bl_dec.py -- out/shell_low_mc.stl out/shell_low_dec.stl 130000 2>&1 | grep -E "^out"
$B -b --python bl_dec.py -- out/sup.stl out/sup_dec.stl 110000 2>&1 | grep -E "^out"
$PY lobe.py | tail -1
$PY - <<'P'
import trimesh.smoothing as sm
from geom import *
m=load_npz('out/lobe_mc.npz'); sm.filter_taubin(m,lamb=0.5,nu=-0.53,iterations=25); m.export('out/lobe_mc.stl')
P
$B -b --python bl_dec.py -- out/lobe_mc.stl out/lobe_dec.stl 45000 2>&1 | grep -E "^out"
$PY finish.py 2>&1 | grep -v "Warn\|center_mass" | tail -2
$PY - <<'P' 2>&1 | grep -v "Warn\|center_mass"
import trimesh
from geom import *
D=trimesh.load('out/door_dec.stl',force='mesh'); d=heal_export(D,'out/door_final.stl'); print('door',len(d.faces),d.is_watertight,1-d.euler_number/2,d.bounds.round(1).tolist()); save_npz('out/door_final.npz',d)
P
