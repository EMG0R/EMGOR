import numpy as np, trimesh, time
from geom import *
t=time.time()
z=np.load('cur/shell_pebble_v4.npz'); S0=trimesh.Trimesh(z['v'],z['f'],process=False)
msd=clean(trimesh.load('cache/shellp_big_dec.stl',force='mesh'))
top=to_m(S0)^to_m(box([700,450,-9.0],[960,620,160]))
res=top+to_m(msd)
R=from_m(res)
print('spliced',len(R.faces),R.is_watertight,round(R.volume),'euler',R.euler_number,'genus',1-R.euler_number/2,len(R.split(only_watertight=False)),time.time()-t)
R.export('cache/shell_spliced.stl'); save_npz('cache/shell_spliced.npz',R)
