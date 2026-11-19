import sys,numpy as np, trimesh
from geom import *
import fast_simplification as fs
N=load_npz('out/lobe_mc.npz'); tgt=int(sys.argv[1]) if len(sys.argv)>1 else 80000
v,f=fs.simplify(np.asarray(N.vertices),np.asarray(N.faces),target_count=tgt,agg=7)
M=trimesh.Trimesh(v,f,process=False); save_npz('out/lobe_dec.npz',M); print('dec',len(M.faces),M.is_watertight)
