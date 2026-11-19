import numpy as np, trimesh, collections, sys
from geom import *
R=load_npz(sys.argv[1]); V=R.vertices
n=R.vertex_normals
sel=(V[:,1]>541.0)&(V[:,1]<545.5)&(V[:,2]>-16.5)
idx=np.nonzero(sel)[0]; print('rim-band vertices',len(idx),flush=True)
rng=np.random.default_rng(0); idx=rng.choice(idx,min(6000,len(idx)),replace=False)
o=V[idx]-n[idx]*0.005; d=-n[idx]
ok=np.isfinite(d).all(1)&np.isfinite(o).all(1); idx,o,d=idx[ok],o[ok],d[ok]
loc,ir,_=R.ray.intersects_location(o,d,multiple_hits=False)
th=np.full(len(idx),np.inf); th[ir]=np.linalg.norm(loc-o[ir],axis=1)
print('thickness<0.5:',(th<0.5).sum(),' <1.0:',(th<1.0).sum(),' <2.0:',(th<2.0).sum(),' of',len(idx),flush=True)
bad=idx[th<0.5]
if len(bad):
    c=collections.Counter(map(tuple,(np.round(V[bad][:,[0,2]]/10)*10).astype(int))); print(c.most_common(10))
