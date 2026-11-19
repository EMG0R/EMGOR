import numpy as np, trimesh, sys
from geom import *
def thickness(path,box=((722,541.0,-12),(772,569,101)),n=60000,seed=1):
    M=load_npz(path); V=M.vertices; N=M.vertex_normals
    lo,hi=np.array(box[0]),np.array(box[1]); sel=np.all((V>lo)&(V<hi),1)&(V[:,1]>REARMIN)
    idx=np.nonzero(sel)[0]; rng=np.random.default_rng(seed); idx=rng.choice(idx,min(n,len(idx)),replace=False)
    o=V[idx]-N[idx]*0.01; d=-N[idx]
    loc,ir,tri=M.ray.intersects_location(o,d,multiple_hits=False)
    th=np.full(len(idx),np.nan); th[ir]=np.linalg.norm(loc-o[ir],axis=1)
    return V[idx],th
REARMIN=541.7
if __name__=='__main__':
    P,th=thickness(sys.argv[1])
    ok=~np.isnan(th); print('rays',len(th),'hit',ok.sum())
    print('thickness percentiles (mm) 0.1,1,5,25,50',np.nanpercentile(th,[0.1,1,5,25,50]).round(2))
    # only report thin ones (<2.4) clusters
    thin=ok&(th<2.3); print('thin <2.3:',thin.sum())
    if thin.sum():
        Q=P[thin]; print('thin region bbox',Q.min(0).round(1),Q.max(0).round(1))
        import collections
        # coarse histogram by (x,z) cell
        cells=collections.Counter(map(tuple,(np.round(Q[:,[0,2]]/4)*4).astype(int))); print(cells.most_common(10))
