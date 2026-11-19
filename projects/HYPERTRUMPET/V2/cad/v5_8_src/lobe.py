"""v5.5 FSR lobe: the pod's face outline grows a circular ear. Plan-view smooth union (fillets r>=8) of a circle and the pod edge; the edge roll-off and the 3D shape are functions of the plan inside-distance, so the face stays exactly coplanar (y 568.4) and rolls off like the pod edge."""
import numpy as np, trimesh
from skimage import measure
from geom import *
FACE=568.4
def smin(a,b,k):
    h=np.maximum(k-np.abs(a-b),0)/k; return np.minimum(a,b)-h*h*k/4
YE=np.array([546.0,556.0,559.5,562.5,565.5,567.0,568.4]); XE=np.array([902.0,902.0,901.2,899.8,897.8,896.4,893.7])
def build_lobe(cx=908.5,cz=111.0,R=17.5,k=12.0,h=0.2):
    lo=np.array([884.0,543.0,90.0]); hi=np.array([930.0,571.0,134.0])
    X=lo[0]+h*np.arange(int((hi[0]-lo[0])/h)+1); Y=lo[1]+h*np.arange(int((hi[1]-lo[1])/h)+1); Z=lo[2]+h*np.arange(int((hi[2]-lo[2])/h)+1)
    x=X[:,None,None]; y=Y[None,:,None]; z=Z[None,None,:]
    xz_c=np.hypot(x-np.clip(x,870.0,cx),z-cz)-R                                   # circle sdf (plan)
    ZE=np.array([60,80,95,105,112.8,120,125,130.0]); XE_=np.array([890.9,894.3,894.4,894.2,893.3,890.9,887.2,884.0])
    box_=(x-np.interp(z,ZE,XE_))*np.ones((1,1,1))                  # pod face-edge outline incl. its rolled top corner (plan)
    box_=np.maximum(box_,z-131.0)
    d2=smin(xz_c,box_,k)[:,0,:]                                  # (nx,nz) negative inside the filleted outline
    din=np.maximum(-d2,0.0)[:,None,:]*np.ones((1,len(Y),1))
    # roll-off like the pod face edge (width/height shrink toward the ear so a 20 mm flat remains for the FSR head)
    t=np.clip((x-893.0)/8.0,0,1); t=t*t*(3-2*t)
    w=8.0; hh=12.0
    u=np.clip(1-din/w,0,1); drop=np.where(din<w,hh*(1-np.clip(din/w,0,1))**2.2,0.0)
    tt=np.clip((x-896.5)/3.5,0,1); tt=tt*tt*(3-2*tt); tz=np.clip((z-114.0)/8.0,0,1); tz=tz*tz*(3-2*tz); tt=tt+(1-tt)*tz
    ytop=FACE-drop*tt                                            # flat over the pod's own roll (it ends up inside the lobe), ear roll-off starts outboard
    thick=1.0+1.4*din; ybot=np.maximum(ytop-thick,546.0)
    outside=(-d2>0)[:,None,:]*np.ones((1,len(Y),1))==0
    F=np.maximum(np.maximum(y-ytop,ybot-y),np.where(outside,1.0,-1.0)*0.0+np.where(outside,1.0,-1.0))
    F=np.where(outside,1.0,np.maximum(y-ytop,ybot-y))
    xe=np.interp(Y,YE,XE)[None,:,None]
    xedge=np.interp(z,ZE,XE_)-1.0
    xs_=np.interp(z,[90,100,112.8,120,125,130,134],[903.0,903.0,902.1,900.5,897.9,894.0,891.0])
    xe_eff=xe-(902.0-xs_)
    F=np.maximum(F,np.where(y<559.5,(xe_eff-0.8)-x,xedge-x))                                   # keep only the part outside the pod envelope (1 mm overlap into the wall)
    Fp=np.pad(F.astype(np.float32),2,constant_values=1.0); v,f,_,_=measure.marching_cubes(Fp,0.0,spacing=(h,h,h)); v=v-2*h+lo
    return trimesh.Trimesh(v,f,process=False)
if __name__=='__main__':
    m=build_lobe(); print(len(m.faces),m.is_watertight,m.volume,m.bounds.round(1).tolist()); save_npz('out/lobe_mc.npz',m); m.export('out/lobe_mc.stl')
