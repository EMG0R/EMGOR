import numpy as np
def voxelize(v,f,lo,hi,h):
    """parity fill along z. returns bool array [nx,ny,nz], cell centres lo+h*(i+.5)"""
    n=np.ceil((np.array(hi)-lo)/h).astype(int)
    off=np.array([0.0000731,0.0000377])  # avoid exact hits
    tri=v[f]  # (T,3,3)
    xy=(tri[:,:,:2]-lo[:2]-off)/h-0.5   # in cell-centre index units
    imin=np.floor(xy.min(1)).astype(int)+1; imax=np.ceil(xy.max(1)).astype(int)-1+1
    imin=np.maximum(imin,0); imax=np.minimum(imax,n[:2]-1)
    w=imax-imin+1; ok=(w>0).all(1); idx=np.nonzero(ok)[0]
    cnt=(w[idx,0]*w[idx,1])
    rep=np.repeat(idx,cnt)
    k=np.arange(cnt.sum())-np.repeat(np.cumsum(cnt)-cnt,cnt)
    ww=w[rep,0]; ix=imin[rep,0]+k%ww; iy=imin[rep,1]+k//ww
    px=lo[0]+off[0]+(ix+0.5)*h; py=lo[1]+off[1]+(iy+0.5)*h
    A=tri[rep,0]; B=tri[rep,1]; C=tri[rep,2]
    d=(B[:,1]-C[:,1])*(A[:,0]-C[:,0])+(C[:,0]-B[:,0])*(A[:,1]-C[:,1])
    good=np.abs(d)>1e-12
    l1=((B[:,1]-C[:,1])*(px-C[:,0])+(C[:,0]-B[:,0])*(py-C[:,1]))/np.where(good,d,1)
    l2=((C[:,1]-A[:,1])*(px-C[:,0])+(A[:,0]-C[:,0])*(py-C[:,1]))/np.where(good,d,1)
    l3=1-l1-l2
    hit=good&(l1>=0)&(l2>=0)&(l3>=0)
    z=l1*A[:,2]+l2*B[:,2]+l3*C[:,2]
    ix=ix[hit]; iy=iy[hit]; z=z[hit]
    iz=np.floor((z-lo[2])/h).astype(int)   # crossing between cell iz-1... centres at (i+.5): crossing at z lies in cell iz -> toggles cells > centre
    # cell c is inside if number of crossings with z < centre(c) is odd.  centre(c)=lo+(c+.5)h; crossing z<centre -> c >= floor((z-lo)/h - .5)+1
    c0=np.floor((z-lo[2])/h-0.5).astype(int)+1
    c0=np.clip(c0,0,n[2])
    grid=np.zeros((n[0],n[1],n[2]+1),np.int8)
    np.add.at(grid,(ix,iy,c0),1)
    grid=(np.cumsum(grid,axis=2)&1).astype(bool)[:,:,:n[2]]
    return grid
