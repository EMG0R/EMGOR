import numpy as np, time
from scipy import ndimage as ndi
lo=np.array([758.,488.,-62.]); h=0.25
def ix(ax,v): return (v-lo[ax])/h-0.5
def cen(ax,i): return lo[ax]+h*(np.asarray(i)+0.5)
PD=dict(cx=837.0,a=31.0,b=20.5,D=7.5,pw=2.6)
def palm_carve(EnvN,PD=PD):
    n0=np.array([0,0.928,-0.371]); n0/=np.linalg.norm(n0); es=np.array([0,n0[2]*-1,n0[1]]);  # es = (0,0.371,0.928)
    shp=EnvN.shape
    # sub-box
    x0,x1=ix(0,PD['cx']-PD['a']-4),ix(0,PD['cx']+PD['a']+4)
    X,Y,Z=np.meshgrid(cen(0,np.arange(int(x0),int(x1))),cen(1,np.arange(int(ix(1,552)),int(ix(1,592)))),cen(2,np.arange(int(ix(2,-61)),int(ix(2,-9)))),indexing='ij')
    sub=(slice(int(x0),int(x1)),slice(int(ix(1,552)),int(ix(1,592))),slice(int(ix(2,-61)),int(ix(2,-9))))
    # face centre: z=-36 outer y
    col=EnvN[int(ix(0,PD['cx'])),:,int(ix(2,-36))]; yc=cen(1,np.nonzero(col)[0].max())
    pc=np.array([PD['cx'],yc,-36.0]); print('palm centre',pc)
    # heightfield Hs(x,s) via column march along n0
    xs=np.arange(PD['cx']-PD['a']-3,PD['cx']+PD['a']+3,0.5); ss=np.arange(-PD['b']-3,PD['b']+3,0.5)
    ts=np.arange(-12,14,0.1)
    Hs=np.zeros((len(xs),len(ss)))
    for i,xx in enumerate(xs):
        P=pc[None,:]+ss[:,None,None]*es[None,None,:]+ts[None,:,None]*n0[None,None,:]    # (s,t,3)
        P=P.reshape(-1,3); P[:,0]=xx
        co=np.stack([(P[:,0]-lo[0])/h-0.5,(P[:,1]-lo[1])/h-0.5,(P[:,2]-lo[2])/h-0.5])
        v=ndi.map_coordinates(EnvN.astype(np.float32),co,order=1,mode='nearest').reshape(len(ss),len(ts))
        for j in range(len(ss)):
            k=np.nonzero(v[j]>0.5)[0]; Hs[i,j]=ts[k.max()] if len(k) else -99
    # voxel carve
    P=np.stack([X,Y,Z],-1)-pc; u=P[...,0]; s=P@es; t=P@n0
    q=((np.abs(u)/PD['a'])**PD['pw']+(np.abs(s)/PD['b'])**PD['pw'])**(1/PD['pw'])
    g=np.clip((1-q)/0.7,0,1); w=g**3*(g*(g*6-15)+10)          # smootherstep, C2
    d=PD['D']*w
    from scipy.interpolate import RegularGridInterpolator
    Hi=RegularGridInterpolator((xs,ss),Hs,bounds_error=False,fill_value=-99)
    Hv=Hi(np.stack([X.ravel(),s.ravel()],-1)).reshape(X.shape)
    cut=(t>Hv-d)&(d>1e-3)
    out=EnvN.copy(); out[sub]&=~cut
    return out,dict(n0=n0.tolist(),es=es.tolist(),pc=pc.tolist(),cut_vol=float(cut.sum()*h**3))
if __name__=='__main__':
    EnvN=np.load('cache/EnvN.npy'); out,info=palm_carve(EnvN); print(info)
    np.save('cache/EnvP.npy',out)
