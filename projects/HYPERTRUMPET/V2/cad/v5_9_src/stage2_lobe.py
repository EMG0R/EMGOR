"""v5.9 stage 2: regenerate the -x lobe end, its floor, the sweep into the housing and the junction ramp from ONE analytic field, mesh once (marching cubes 0.25 mm),
stitch to the untouched shell along seams that lie in flat faces.
Field (per x slice):
  lobe mass  = pod sweep profile (measured pure sweep: outer roll + face plate) along the -x outline x_o(z), face roll as measured, back plane y=541.6, rounded bottom
  housing    = analytic body of fieldA (x0 771, top -16.4, R10/R14 edges)
  outer      = smin(lobe, housing, k) , k 18 at the lobe -> 12 at the seam (the pod/housing junction k of v5.4), fill clipped at the open-back plane
  cavity     = measured sweep cavity (open back, floor -5.93) blended over x 772.3..782 into the housing cavity (floor -16.4, measured concave sweep to the face plane)
  USB-C      = stadium 7.5 x 13 at (y 555, z 6.25) through the end wall, edge smax 0.8
Seams: curved seam at plan distance 14 mm from the outline (flat face plate, coplanar), horizontal seam z=100 (profile identical to the old pod),
x=782 and z=-6.2 / -3.2 planes (inside solid or flat cavity wall).  In an overlap strip of 0.8 mm outside the cut the new skin is inset 0.15 mm (hidden under the old skin)."""
import numpy as np, trimesh, pickle, time, sys
from scipy.ndimage import map_coordinates
from scipy.spatial import cKDTree
from skimage import measure
from geom import *
FACE=568.40; REAR=541.6; FACE_IN=565.68; ZFL=-5.93; RIM_R=0.95
import os
TILT=float(os.environ.get('TILT','0.35')); KSW=float(os.environ.get('KSW','16')); KB=float(os.environ.get('KB','12')); ZB0=float(os.environ.get('ZB0','-9.5')); XT=float(os.environ.get('XT','748'))
SEAM_D=14.0; ZT=100.0; XA=772.3; XB=782.0; ZB1=2.0; ZBOT=-36.0
H=float(os.environ.get('HRES','0.25'))
LO=np.array([722.0,528.0,-38.5]); HI=np.array([783.2,591.0,101.0])
def smin(a,b,k):
    h=np.maximum(k-np.abs(a-b),0)/k; return np.minimum(a,b)-h*h*k/4
def smax(a,b,k): return -smin(-a,-b,k)
def sstep(t): t=np.clip(t,0,1); return t*t*t*(t*(t*6-15)+10)
def load_tables():
    a=np.load('out/tables_profile.npz'); b=np.load('out/tables_house.npz')
    return dict(tO=a['tO'],tC=a['tC'],d0=float(a['d0']),py0=float(a['y0']),ph=float(a['h']),tB=b['tB'],by0=float(b['y0']),bz0=float(b['z0']),bh=float(b['h']))
def outline_polyline(xo):
    zz=np.arange(-60.0,140.0,0.05); return np.c_[xo(zz),zz]
def din_grid(xo,X,Z):
    P=outline_polyline(xo); tr=cKDTree(P); XX,ZZ=np.meshgrid(X,Z,indexing='ij'); d,_=tr.query(np.c_[XX.ravel(),ZZ.ravel()])
    sgn=np.where(XX.ravel()>xo(ZZ.ravel()),1.0,-1.0); return (d*sgn).reshape(XX.shape)
def look(tab,d0,y0,h,d,y):
    """bilinear lookup of a (d,y) SDF table; outside the table: clamp + add overshoot distance (keeps the sign and slope 1)"""
    nd,ny=tab.shape; dc=np.clip(d,d0,d0+(nd-1)*h); yc=np.clip(y,y0,y0+(ny-1)*h)
    c=np.stack(np.broadcast_arrays((dc-d0)/h,(yc-y0)/h)); v=map_coordinates(tab,c,order=1,mode='nearest')
    return v+np.hypot(d-dc,y-yc)
def body_slice(x,Y,Z):
    """fieldA housing body (x- side, top edges); top plane -16.4"""
    x0=771.0; y0=496.4; kv=1.2*11.0
    yf=584.0+0.29*(Z+22.0); nrm=np.sqrt(1+0.29**2)
    a_xm=(x0-x)+0*Y+0*Z; a_ym=(y0-Y)+0*Z; a_yp=(Y-yf)/nrm
    side=np.maximum(smax(a_xm,a_ym,kv),smax(a_xm,a_yp,kv))
    tq=np.clip((Y-548.0)/24.0,0,1); tq=tq*tq*tq*(tq*(tq*6-15)+10); kt=1.2*10.0+(1.2*14.0-1.2*10.0)*tq
    return smax(side,(Z+16.4)+0*Y,kt)
def stadium(Y,Z):
    cy=555.0; z0,z1=3.5,9.0; r=3.75; t=np.clip((Z-z0)/(z1-z0),0,1); return np.hypot(Y-cy,Z-(z0+t*(z1-z0)))-r
def face_ripple(S1,xo):
    """old face plate deviates up to +0.065 mm from the 568.40 plane near the seam (z 20..66); sample it so the new face rises to meet it"""
    zs=np.arange(-6.0,101.0,3.0); out=[]
    for z in zs:
        o=np.array([[xo(z)+SEAM_D+dx,600.0,z] for dx in (0.5,2.0,3.5)]); loc,ir,_=S1.ray.intersects_location(o,np.tile([0,-1.0,0],(3,1)),multiple_hits=False)
        y=np.full(3,np.nan); y[ir]=loc[:,1]; out.append(np.nanmean(y)-FACE if ir.any() else 0.0)
    out=np.array(out); out=np.where((zs<6.0)|(np.abs(out)>0.1),0.0,out)
    from scipy.ndimage import gaussian_filter1d
    return zs,gaussian_filter1d(out,1.0,mode='nearest')
def build(S,xo,T,h=H,ripple=None):
    t0=time.time(); X=LO[0]+h*np.arange(int((HI[0]-LO[0])/h)+1); Y=LO[1]+h*np.arange(int((HI[1]-LO[1])/h)+1); Z=LO[2]+h*np.arange(int((HI[2]-LO[2])/h)+1)
    nx,ny,nz=len(X),len(Y),len(Z); print('grid',nx,ny,nz,nx*ny*nz/1e6,'M')
    DIN=din_grid(xo,X,Z); print('din',round(time.time()-t0,1))
    Yc=Y[:,None]; Zr=Z[None,:]
    F=np.empty((nx,ny,nz),np.float32)
    # house cavity table along (y,z)
    tB=T['tB']
    for i,x in enumerate(X):
        w=float(sstep((x-XA)/(XB-XA)))
        d=DIN[i][None,:]*np.ones((ny,1))
        O2=look(T['tO'],T['d0'],T['py0'],T['ph'],d,Yc*np.ones((1,nz)))
        zb0=ZB0-TILT*np.clip(x-XT,0,XA-XT); zb=zb0+(-30.0-zb0)*w; kb=KB
        Flobe=smax(O2,zb-Zr,kb)
        if ripple is not None: Flobe=Flobe-np.interp(Zr,ripple[0],ripple[1])*sstep((d-7.0)/7.0)
        Fb=body_slice(x,Yc,Zr)
        k=12.0+(KSW-12.0)*(1-w)
        Fu=smin(Flobe,Fb,k)
        # cavity
        C2=look(T['tC'],T['d0'],T['py0'],T['ph'],d,Yc*np.ones((1,nz)))
        ew=0.45*sstep((92.0-Zr)/20.0)*(1-sstep((d-6.0)/6.0))          # wall thickens from the old 2.3 at the seam to 2.75 (>= 2.5 skin) below z 72
        A=smax(C2+ew,ZFL-Zr,1.2*1.0)
        if w>0:
            B=look(tB,T['by0'],T['bz0'],T['bh'],Yc*np.ones((1,nz)),Zr*np.ones((ny,1)))
            Cv=(1-w)*A+w*B
        else: Cv=A
        if x>=751.0: Cv=np.where(Zr>=1.5,np.maximum(Cv,Yc-564.9),Cv)      # logo window: face plate inner plane 564.9 for x>=751, z>=1.5 (old slab)
        # distance outside the cut region Q (Chebyshev metric) and the inset ramp
        dR1=np.maximum.reduce([d-SEAM_D+0*Zr,Zr-ZT+0*Yc,ZB1-Zr+0*Yc,538.0-Yc+0*Zr,Yc-575.0+0*Zr])
        dB1=np.maximum.reduce([np.full((ny,nz),x-XB),Zr-ZB1+0*Yc,ZBOT-Zr+0*Yc,528.0-Yc+0*Zr,Yc-590.0+0*Zr])
        dQ=np.minimum(dR1,dB1)
        c=0.15*sstep(dQ/0.5)
        Ffill=smax(np.maximum(Fu+c,-(Cv-c)),REAR-Yc,1.2*RIM_R)           # fill + lobe: carve the cavity, clip at the open-back plane with a r~1 edge round
        Fbod=np.maximum(Fb+c,-(Cv-c))
        Fi=np.minimum(Ffill,Fbod)
        # USB-C through the end wall
        G=np.maximum(stadium(Yc,Zr)+0*Zr,np.full((ny,nz),x-760.0))
        Fi=smax(Fi,-G,0.8)
        F[i]=np.maximum(Fi,dQ-0.8)
    print('field',round(time.time()-t0,1))
    return F,(X,Y,Z)
def mesh(F,axes,h=H):
    X,Y,Z=axes; Fp=np.pad(F,1,constant_values=1.0); v,f,_,_=measure.marching_cubes(Fp,0.0,spacing=(h,h,h)); v=v-h+np.array([X[0],Y[0],Z[0]])
    return trimesh.Trimesh(v,f,process=False)
if __name__=='__main__':
    import tables as TB
    S=TB.load_shell(); z=pickle.load(open('out/xo.pkl','rb')); xo,_=TB.outline(S)
    S1=load_npz('out/s1.npz'); rp=face_ripple(S1,xo); print('ripple',np.round(rp[1][::3],3).tolist()); T=load_tables(); F,ax=build(S,xo,T,ripple=rp); np.save('out/F_lobe.npy',F)
    m=mesh(F,ax); print('mc',len(m.faces),m.is_watertight,round(m.volume)); save_npz('out/lobe_mc.npz',m)
