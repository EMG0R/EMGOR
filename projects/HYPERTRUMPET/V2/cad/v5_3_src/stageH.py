import numpy as np, time
from scipy import ndimage as ndi
lo=np.array([758.,488.,-62.]); h=0.25
def ix(ax,v): return int(round((v-lo[ax])/h-0.5))
def cen(ax,i): return lo[ax]+h*(np.asarray(i)+0.5)
U=np.load('cache/U.npy'); shape=U.shape
X,Y,Z=np.meshgrid(cen(0,np.arange(shape[0])),cen(1,np.arange(shape[1])),cen(2,np.arange(shape[2])),indexing='ij')
t=time.time()
plug=np.zeros(shape,bool)
plug[ix(0,826):ix(0,868),ix(1,538):ix(1,562),ix(2,-19.6):ix(2,-15.8)]=True           # wire stadium riser
sw=((Y-531.7)**2+(Z+34.0)**2<=3.8**2)&(X>768)&(X<775.5)                                # switch hole
plug|=sw
Up=U|plug
lab,n=ndi.label(~Up)
C=(lab==lab[ix(0,837),ix(1,530),ix(2,-30)])
b=C[0].any() or C[-1].any() or C[:,0].any() or C[:,-1].any() or C[:,:,0].any() or C[:,:,-1].any()
print('cavity vol mm3',C.sum()*h**3,'touches border',bool(b))
if b:
    # find where it touches
    for nm,a in (('x0',C[0]),('x1',C[-1]),('y0',C[:,0]),('y1',C[:,-1]),('z0',C[:,:,0]),('z1',C[:,:,-1])):
        if a.any(): 
            idx=np.argwhere(a); print(nm,idx.min(0),idx.max(0),a.sum())
np.save('cache/C.npy',C); np.save('cache/plug.npy',plug)
