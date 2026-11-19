import numpy as np, time, trimesh
from scipy import ndimage as ndi
from geom import *
lo=np.array([758.,488.,-62.]); h=0.25
def ix(ax,v): return int(round((v-lo[ax])/h-0.5))
U=np.load('cache/U.npy'); plug=np.load('cache/plug.npy'); C=np.load('cache/C.npy'); EnvP=np.load('cache/EnvP.npy')
def opn(X,r):
    e=ndi.distance_transform_edt(X,sampling=h)>r
    return ndi.distance_transform_edt(~e,sampling=h)<=r
P=-44.0; SHIFT=1.0
EnvP=ndi.gaussian_filter(EnvP.astype(np.float32),3.0)>0.5          # smooth the whole housing envelope (removes the voxel/opening ripple, 0.75 mm sigma)
# thicken the door floor: union with itself shifted down
k=int(round(SHIFT/h)); low=EnvP.copy(); low[:,:,ix(2,P):]=False
sh=np.zeros_like(low); sh[:,:,:-k]=low[:,:,k:]
Env2=EnvP|sh
Un=Env2&~C&~(plug&~U)
zi=ix(2,P)
Un=opn(Un,0.35); door=Un.copy(); door[:,:,zi:]=False
shellp=Un.copy(); shellp[:,:,:zi]=False
door=keep_big(door,1e9,False) if False else keep_big(door,100000,False); shellp=keep_big(shellp,100000,True); np.save('cache/door_vox.npy',door); np.save('cache/shellp_vox.npy',shellp)
t=time.time()
md=vox2mesh(door,lo,h,sigma=2.2); print('door mc',len(md.faces),time.time()-t)
ms=vox2mesh(shellp,lo,h,sigma=2.2); print('shell part mc',len(ms.faces))
md.export('cache/door_mc.stl'); ms.export('cache/shellp_mc.stl')
print(md.is_watertight, ms.is_watertight, md.volume, ms.volume)
