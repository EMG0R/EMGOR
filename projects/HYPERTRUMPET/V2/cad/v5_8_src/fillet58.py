"""v5.8: big concave fillet (closing r=10) under the -x lobe, blending the lobe bottom into the housing/wall below; only material added below z=-8.9."""
import numpy as np, trimesh
from scipy import ndimage as ndi
from skimage import measure
from geom import *
from vox import voxelize
def lobe_fillet(S,r=10.0,h=0.3):
    lo=np.array([728.0,536.0,-24.0]); hi=np.array([800.0,572.0,-3.0])
    V=voxelize(np.asarray(S.vertices),np.asarray(S.faces),lo,hi,h)
    dil=ndi.distance_transform_edt(~V,sampling=h)<=r; cl=ndi.distance_transform_edt(dil,sampling=h)>r
    add=cl&~V
    X,Y,Z=np.meshgrid(lo[0]+h*(np.arange(V.shape[0])+.5),lo[1]+h*(np.arange(V.shape[1])+.5),lo[2]+h*(np.arange(V.shape[2])+.5),indexing='ij')
    add&=(Z<-8.9)&(X<778.0)&(Y>541.0)&(Y<568.0)
    # keep the added skin inside the exterior (not inside the pod cavity): only where the voxel is below the plate and outside the ring footprint
    lab,n=ndi.label(add); 
    if n==0: return S,0.0
    sz=np.bincount(lab.ravel()); sz[0]=0; add=lab==np.argmax(sz)
    ext=add|(ndi.binary_dilation(add,iterations=4)&V)                      # hidden overlap into the shell
    F=ndi.gaussian_filter(np.pad(ext.astype(np.float32),2),1.2)
    v,f,_,_=measure.marching_cubes(F,0.5); v=(v-2+0.5)*h+lo
    m=trimesh.Trimesh(v,f,process=False); m.fix_normals()
    import trimesh.smoothing as sm; sm.filter_taubin(m,lamb=0.5,nu=-0.53,iterations=15)
    R=from_m(to_m(S)+to_m(m)); return R,float(add.sum()*h**3)
if __name__=='__main__':
    S=load_npz('out/shell_final.npz'); R,vol=lobe_fillet(S); print('added mm3',round(vol),len(R.faces),R.is_watertight,1-R.euler_number/2)
    save_npz('out/shell_fil.npz',R)
