import numpy as np, time
from scipy import ndimage as ndi
lo=np.array([758.,488.,-62.]); h=0.25
def ix(ax,v): return int(round((v-lo[ax])/h-0.5))
def cen(ax,i): return lo[ax]+h*(np.asarray(i)+0.5)
U=np.load('cache/U.npy'); plug=np.load('cache/plug.npy'); Up=U|plug; shape=U.shape
box=np.zeros(shape,bool); box[ix(0,758)+2:ix(0,915)-2,ix(1,488)+2:ix(1,592)-2,ix(2,-61):ix(2,-17.0)]=True
lab,n=ndi.label((~Up)&box); C=lab==lab[ix(0,837),ix(1,530),ix(2,-30)]
Env=Up|C
np.save('cache/C.npy',C); np.save('cache/Env.npy',Env)
t=time.time()
def blk(a): 
    s=a.shape; return a.reshape(s[0]//2,2,s[1]//2,2,s[2]//2,2).mean((1,3,5))
h2=0.5
Env2=blk(Env.astype(np.float32))>0.5
z2=lo[2]+h2*(np.arange(Env2.shape[2])+0.5)
R=np.interp(z2,[-62,-48,-38,100],[8,8,12,12]).astype(np.float32)[None,None,:]
E2=ndi.distance_transform_edt(Env2,sampling=h2).astype(np.float32)
Rq=np.round(np.broadcast_to(R,E2.shape)*2)/2
Open=np.zeros_like(Env2)
for r in np.unique(Rq):
    c_=(E2>=r)&(Rq==r)
    if not c_.any(): continue
    Open|=ndi.distance_transform_edt(~c_,sampling=h2)<=r
    print(r,time.time()-t,flush=True)
rem2=Env2&~Open
zz=np.broadcast_to(z2[None,None,:],rem2.shape)
rem2&=(zz<-13.0)
np.save('cache/rem2.npy',rem2); np.save('cache/Open2.npy',Open)
print('removed vol',rem2.sum()*h2**3)
