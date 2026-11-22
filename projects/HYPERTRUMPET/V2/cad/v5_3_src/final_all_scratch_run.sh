set -e
PY=../venv/bin/python; CAD=/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/HYPERTRUMPET_V2/cad; B=/Applications/Blender.app/Contents/MacOS/Blender
bash run_chain.sh
$PY - <<'P' 2>&1 | grep -v "Warn\|center_mass"
import numpy as np, trimesh, shutil
from geom import *
CAD='/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/HYPERTRUMPET_V2/cad'
for src,name in (('cache/shell_g.npz','shell_pebble_v5_3'),('cache/door_f.npz','battery_door_v5_3')):
    m=finalize(load_npz(src)); ok,q=stl_ok(m,f'{CAD}/stl_v5_3/{name}.stl'); print(name,ok,len(q.faces),1-q.euler_number/2); assert ok; save_npz(f'final/{name}.npz',q)
qS=trimesh.load(f'{CAD}/stl_v5_3/shell_pebble_v5_3.stl'); qD=trimesh.load(f'{CAD}/stl_v5_3/battery_door_v5_3.stl'); M=trimesh.load(f'{CAD}/stl_v5_3/mount_original_arms_plate_v4.stl')
sm,dm,mm_=to_m(qS),to_m(qD),to_m(M); iv=lambda a,b: round((a^b).volume(),3)
print('shell x door',iv(sm,dm),'shell x mount',iv(sm,mm_),'door x mount',iv(dm,mm_))
bb=to_m(box([805.0,506.7,-48.9],[890.0,556.7,-18.9])); print('battery box x shell',iv(sm,bb),'x door',iv(dm,bb))
for i,n in enumerate(('valve_cap_v4','valve_cap_v4.001','valve_cap_v4.002')):
    d=np.load(f'cur/{n}.npz'); c=to_m(trimesh.Trimesh(d['v'],d['f'],process=False)); print('cap',i,'x shell',iv(sm,c),'x mount',iv(mm_,c),'x door',iv(dm,c))
loc,ri,_=qD.ray.intersects_location(np.array([[x,y,-40.0] for x in range(790,890,10) for y in (510,520,530,540,550,556)]),np.tile([0,0,-1.0],(60,1)),multiple_hits=True)
import collections; dd=collections.defaultdict(list)
for l,r in zip(loc,ri): dd[r].append(l[2])
th=[sorted(z,reverse=True)[0]-sorted(z,reverse=True)[1] for z in dd.values() if len(z)>=2]; print('door floor vertical thickness min',round(min(th),2),'max',round(max(th),2))
print('shell bounds',qS.bounds.round(1).tolist(),'door bounds',qD.bounds.round(1).tolist())
P
$B -b $CAD/hypertrumpet_v5.blend --python bl_update_blend.py -- $CAD/hypertrumpet_v5.blend.v53tmp shell_pebble_v4=final/shell_pebble_v5_3.npz battery_door_v4=final/battery_door_v5_3.npz 2>&1 | grep -E "replaced|saved"
cp $CAD/hypertrumpet_v5.blend.v53tmp $CAD/snapshots/hypertrumpet_v5_3.blend && mv -f $CAD/hypertrumpet_v5.blend.v53tmp $CAD/hypertrumpet_v5.blend && echo blend-updated
rm -f $CAD/renders_v5_3/*.png
($B -b --python bl_render.py -- render_logo.json > rl.log 2>&1; $B -b --python bl_render.py -- render_main.json > rm.log 2>&1; echo RENDERS_DONE >> rm.log) &
(cd $CAD; $PY print_audit_v5_3.py > /tmp/pa53.log 2>&1; echo PA_DONE >> /tmp/pa53.log) &
(cd $CAD/v5_3_src; $PY genus_check.py ../stl_v5_3/shell_pebble_v5_3.stl > /tmp/gen53.log 2>&1; echo GEN_DONE >> /tmp/gen53.log) &
(cd $CAD; rm -f _fit_audit_work_v5_3/meshes.npz _fit_audit_work_v5_3/occ.npz; $PY _fit_audit_work_v5_3/fit_audit.py --blend $CAD/hypertrumpet_v5.blend > /tmp/fit53.log 2>&1; echo FIT_DONE >> /tmp/fit53.log) &
wait
echo FINAL_ALL_DONE
