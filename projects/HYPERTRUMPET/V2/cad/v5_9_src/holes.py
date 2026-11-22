import numpy as np, trimesh, sys, json
from scipy import ndimage as ndi
from geom import *
R=load_npz(sys.argv[1]); h=0.4
xs=np.arange(722,940,h); zs=np.arange(-14,146,h); X,Z=np.meshgrid(xs,zs,indexing='ij')
o=np.c_[X.ravel(),np.full(X.size,640.0),Z.ravel()]; loc,ir,_=R.ray.intersects_location(o,np.tile([0,-1.0,0],(len(o),1)),multiple_hits=False)
Y=np.full(X.size,np.nan); Y[ir]=loc[:,1]; Y=Y.reshape(X.shape)
face=Y>566.0                       # a hit on the face plate plane (or rolled edge) from the front
empty=~(Y>0)|(Y<560.0)             # no hit or only deep geometry behind the plate
lab,n=ndi.label(empty); border=set(np.unique(np.r_[lab[0],lab[-1],lab[:,0],lab[:,-1]]))
out=[]
for k in range(1,n+1):
    if k in border: continue
    m=lab==k; a=m.sum()*h*h
    if a<3: continue
    ii,jj=np.nonzero(m); out.append((round(float(xs[ii].mean()),1),round(float(zs[jj].mean()),1),round(float(a),1)))
print('face-plate through holes:',len(out)); 
for o_ in sorted(out): print(o_)
