import numpy as np, trimesh
from skimage import measure
from geom import *
FACE=568.4
def smin(a,b,k):
    h=np.maximum(k-np.abs(a-b),0)/k; return np.minimum(a,b)-h*h*k/4
def build_pad(cx=908.5,cz=112.8,R=14.5,t=4.0,rr=1.5,h=0.2):
    lo=np.array([880.0,545.0,94.0]); hi=np.array([930.0,571.0,132.0])
    X=lo[0]+h*np.arange(int((hi[0]-lo[0])/h)+1); Y=lo[1]+h*np.arange(int((hi[1]-lo[1])/h)+1); Z=lo[2]+h*np.arange(int((hi[2]-lo[2])/h)+1)
    x=X[:,None,None]; y=Y[None,:,None]; z=Z[None,None,:]
    r=np.hypot(x-cx,z-cz)
    # disc with a true circular rim (analytic), rim round-over rr on the face side, flat face y = FACE
    u=r-(R-rr); v=y-(FACE-rr)
    d=np.hypot(np.maximum(u,0),np.maximum(v,0))+np.minimum(np.maximum(u,v),0)-rr
    disc=np.maximum(d,(FACE-t)-y)
    # conical root flare under the disc, blended into the pod side wall
    y0=FACE-t; Rc=R-(y0-y)*(6.5/13.4)
    cone=np.maximum(r-Rc,np.maximum(y-y0,(551.0)-y))*0.93
    wall=np.maximum(np.maximum(np.maximum(887.0-x,x-902.0),np.maximum(552.0-y,y-(FACE-1.8))),np.abs(z-cz)-10.0)
    root=smin(cone,wall,6.0)
    # two gusset ribs (3 mm thick triangles in x-y) blended in
    ribs=None
    for dz in (-9.5,9.5):
        tri=np.maximum(np.maximum(898.0-x,y-y0),(552.0+(x-898.0)*0.67)-y)*0.8; tri=np.maximum(tri,np.abs(z-(cz+dz))-1.5)
        ribs=tri if ribs is None else np.minimum(ribs,tri)
    F=np.minimum(disc,np.minimum(root,smin(ribs,root,3.0)))
    Fp=np.pad(F,2,constant_values=1.0); v,f,_,_=measure.marching_cubes(Fp,0.0,spacing=(h,h,h)); v=v-2*h+lo
    m=trimesh.Trimesh(v,f,process=False); return m
if __name__=='__main__':
    m=build_pad(); print(len(m.faces),m.is_watertight,m.volume); save_npz('out/pad_mc.npz',m); m.export('out/pad_mc.stl')
