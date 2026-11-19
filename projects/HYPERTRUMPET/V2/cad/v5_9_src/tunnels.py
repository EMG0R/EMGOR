import numpy as np, trimesh, sys, json
from scipy import ndimage as ndi
from shapely.geometry import MultiPoint, box as sbox
from geom import *
R=load_npz(sys.argv[1]); h=0.4
def genus(m): return 1-m.euler_number/2
print('start genus',genus(R),'bodies',len(R.split(only_watertight=False)))
xs=np.arange(722,940,h); zs=np.arange(-14,146,h); X,Z=np.meshgrid(xs,zs,indexing='ij')
o=np.c_[X.ravel(),np.full(X.size,640.0),Z.ravel()]; loc,ir,_=R.ray.intersects_location(o,np.tile([0,-1.0,0],(len(o),1)),multiple_hits=False)
Y=np.full(X.size,np.nan); Y[ir]=loc[:,1]; Y=Y.reshape(X.shape)
empty=~(Y>0)|(Y<560.0); lab,n=ndi.label(empty); border=set(np.unique(np.r_[lab[0],lab[-1],lab[:,0],lab[:,-1]]))
holes=[]
for k in range(1,n+1):
    if k in border: continue
    m=lab==k
    if m.sum()*h*h<3: continue
    ii,jj=np.nonzero(m); holes.append(np.c_[xs[ii],zs[jj]])
caps=[]
for pts in holes:
    hull=MultiPoint(pts).convex_hull.buffer(0.8)
    pm=trimesh.creation.extrude_polygon(hull,6.0); V=pm.vertices
    pt=trimesh.Trimesh(np.c_[V[:,0],563.5+V[:,2],V[:,1]],pm.faces[:,::-1],process=False)
    if pt.volume<0: pt.invert()
    caps.append(to_m(pt))
M=to_m(R)
def step(name,capm):
    global M
    M=M+capm; m=from_m(M); print('%-28s genus %s  bodies %d'%(name,genus(m),len(m.split(only_watertight=False))),flush=True); return m
step('face holes (%d)'%len(caps),mf.Manifold.batch_boolean(caps,mf.OpType.Add))
step('USB-C',to_m(box([727.0,550.5,-1.0],[748.0,559.5,13.5])))
step('switch hole',to_m(box([764.0,527.0,-38.0],[776.0,537.0,-30.0])))
step('wire holes (roof)',to_m(box([795.0,522.0,-18.9],[854.0,560.5,-16.3])))
step('bay bottom (door side)',to_m(box([765.0,494.0,-45.0],[905.0,585.0,-43.5])))
m=step('back, plate at rim plane',to_m(box([727.0,540.9,-5.0],[905.0,542.3,121.5])))
# residual: where are remaining handles? raster rays along +y from behind
def sg(m): return round(genus(m)+len(m.split(only_watertight=False))-1)
print('sum-of-genus after caps so far',sg(m))
m=step('jack (+x end)',to_m(box([893.0,551.0,2.0],[906.0,560.0,10.0])))
for x0 in (827.0,849.0,871.4):
    m=step('hall window x%.0f'%x0,to_m(box([x0-0.5,540.0,120.5],[x0+9.2,548.5,127.5])))
print('final sum-of-genus',sg(m))
