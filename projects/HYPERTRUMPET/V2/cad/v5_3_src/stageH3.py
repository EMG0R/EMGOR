import numpy as np, time
from scipy import ndimage as ndi
import matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
lo=np.array([758.,488.,-62.]); h=0.25
def ix(ax,v): return int(round((v-lo[ax])/h-0.5))
Env=np.load('cache/Env.npy'); C=np.load('cache/C.npy'); rem2=np.load('cache/rem2.npy')
rem=ndi.gaussian_filter(ndi.zoom(rem2.astype(np.float32),2,order=1),2.0)>0.35
guard=ndi.distance_transform_edt(~C,sampling=h)<=2.3     # wall >= 2.0 from the battery cavity
rem&=~guard
EnvN=Env&~rem
np.save('cache/EnvN.npy',EnvN); print('removed',rem.sum()*h**3)
fig,ax=plt.subplots(2,3,figsize=(21,12))
for a,(axs,v) in zip(ax.ravel(),[(0,837),(0,775),(0,800),(1,505),(1,530),(2,-22)]):
    sl=[slice(None)]*3; sl[axs]=ix(axs,v)
    a.imshow((Env[tuple(sl)].astype(int)+EnvN[tuple(sl)].astype(int)).T,origin='lower',cmap='viridis'); a.set_title(f'{axs}={v}')
plt.savefig('H3.png',dpi=50)
