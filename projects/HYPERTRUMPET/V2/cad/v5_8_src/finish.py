import numpy as np, trimesh, json
from geom import *
import stageC, stageG
stageC.run()
S=heal_export(load_npz('out/shell_c.npz'),'out/shell_fin.stl'); save_npz('out/shell_fin.npz',S)
q=heal_export(S,'out/shell_final.stl'); print('shell',len(q.faces),q.is_watertight,1-q.euler_number/2,q.bounds.round(1).tolist()); save_npz('out/shell_final.npz',q)
