"""v5.4 stage A: analytic housing field (rounded body + cosine saddle + smooth pod junction), marching cubes, NO post-blur.
Frame mm: x along horn, y depth (+y control face), z up. Everything below z=-5 of the shell is rebuilt from this one field."""
import numpy as np, trimesh, json, time
from scipy import ndimage as ndi
from scipy.interpolate import CubicSpline
from skimage import measure
from shapely.geometry import Polygon
from geom import *
LO=np.array([735.0,488.0,-62.0]); HI=np.array([915.0,592.0,-3.7]); H=0.3
P_SPLIT=-44.0
def axes():
    n=np.ceil((HI-LO)/H).astype(int)+1
    return [LO[i]+H*np.arange(n[i]) for i in range(3)]
def smin(a,b,k):
    if np.isscalar(k) and k==0: return np.minimum(a,b)
    h=np.maximum(k-np.abs(a-b),0)/k; return np.minimum(a,b)-h*h*k/4
def smax(a,b,k): return -smin(-a,-b,k)
# ---- measured floor curve of the bay (door inner surface) -> analytic cubic spline, extended linearly
FY=np.array([505.2,508,515,522,529,536,543,550,557,558.2]); FZ=np.array([-50.35,-50.41,-50.54,-50.70,-50.99,-51.49,-52.30,-53.55,-55.08,-55.30])
ZF=CubicSpline(FY,FZ,bc_type='natural')
def zf(y):
    y=np.asarray(y); out=ZF(np.clip(y,505.2,558.2)); sl=(ZF(558.2)-ZF(557.2))/1.0
    return np.where(y>558.2,out+(y-558.2)*sl,out)
PAR=dict(x0=771.0,x1=903.0,y0=496.4,tilt=0.29,yface22=584.0,skin=2.0,zbot_min=-58.4,ztop=-16.5,
         Rv=11.0,Rb=9.0,Rt_back=10.0,Rt_face=14.0,k_pod=12.0,saddle_A=5.0,saddle_L=103.0,saddle_xc=837.0,saddle_z=(-54.0,-20.0),
         bay=dict(x=(781.0,892.9),y=(505.2,558.2),ztop=-18.7),sw=dict(y=531.7,z=-34.0,hole_r=3.15,pocket_r=8.0,wall_x=(771.0,773.5)))
def build_field(par=PAR, pod_sections=None):
    pod_secs=pod_sections
    t0=time.time(); X,Y,Z=axes(); nx,ny,nz=len(X),len(Y),len(Z)
    x=X[:,None,None]; y=Y[None,:,None]; z=Z[None,None,:]
    p=par
    # saddle: broad cosine along the length (>= 70 %), 5 mm deep, no tight radius
    u=np.clip((x-p['saddle_xc'])/(p['saddle_L']/2),-1,1); cx=0.5*(1+np.cos(np.pi*u))
    zc=0.5*(sum(p['saddle_z'])); zh=0.5*(p['saddle_z'][1]-p['saddle_z'][0]); v=np.clip((z-zc)/zh,-1,1); cz=0.5*(1+np.cos(np.pi*v))
    yf=p['yface22']+p['tilt']*(z+22.0)-p['saddle_A']*cx*cz
    nrm=np.sqrt(1+p['tilt']**2)
    a_xm=(p['x0']-x)*np.ones((1,ny,nz),np.float32); a_xp=(x-p['x1'])*np.ones((1,ny,nz),np.float32)
    a_ym=(p['y0']-y)*np.ones((nx,1,nz),np.float32); a_yp=(y-yf)/nrm
    kv=1.2*p['Rv']
    s1=smax(a_xm,a_ym,kv); s2=smax(a_xp,a_ym,kv); s3=smax(a_xm,a_yp,kv); s4=smax(a_xp,a_yp,kv)
    side=np.maximum(np.maximum(s1,s2),np.maximum(s3,s4)); del s1,s2,s3,s4
    zbot=smax(zf(Y),p['zbot_min']+0*Y,4.0)-p['skin']        # follows the floor, clamped for the hand rest
    a_zb=(zbot[None,:,None]-z)*np.ones((nx,1,1),np.float32)*0.995
    body=smax(side,a_zb,1.2*p['Rb'])
    # top: flat at ztop with 10 mm edge (back/sides) and 14 mm round into the hand-rest face
    a_zt=(z-p['ztop'])*np.ones((nx,ny,1),np.float32)
    tq=np.clip((y-548.0)/24.0,0,1); tq=tq*tq*tq*(tq*(tq*6-15)+10); kt=1.2*p['Rt_back']+(1.2*p['Rt_face']-1.2*p['Rt_back'])*tq     # smooth radius change, no crease
    body=smax(body,a_zt,kt)
    del side,a_xm,a_xp,a_ym,a_yp
    # pod ring proxy (thin walls), morphing between measured sections, standing on the housing top
    pod=pod_field(pod_sections,X,Y,Z,p)
    F=smin(body,pod,p['k_pod']); del body,pod
    # v5.7: close the underside of the -x pod lobe (the big window): 3 mm plate under the lobe cavity, x < 772.3, joined to the ring walls
    gx=(X-pod_secs['RX'][0])/0.1; gy=(Y-pod_secs['RY'][0])/0.1; GX,GY=np.meshgrid(gx,gy,indexing='ij')
    Dfill=ndi.map_coordinates(pod_secs['fill'],[GX,GY],order=1,mode='nearest')[:,:,None]
    plate=np.maximum(np.maximum(Dfill,-9.0-z),np.maximum(z+6.0,x-772.3))
    F=smin(F,plate,3.0)
    # v5.5: cap the dovetail channel's lower end / close the back mouth between the pod ring ends (skin 10 mm, rounded), top = channel floor/stop at z=-5
    cx0,cx1,cy0,cy1,cz0,cz1=866.5,891.8,540.4,550.5,-17.5,-5.0
    q=np.stack(np.broadcast_arrays(np.abs(x-0.5*(cx0+cx1))-0.5*(cx1-cx0)+1.0,np.abs(y-0.5*(cy0+cy1))-0.5*(cy1-cy0)+1.0,np.abs(z-0.5*(cz0+cz1))-0.5*(cz1-cz0)+1.0))
    capbox=np.sqrt(np.maximum(q,0).__pow__(2).sum(0))+np.minimum(q.max(0),0)-1.0
    F=np.minimum(F,capbox)
    # cavities
    bay=p['bay']
    cav=smax(smax(np.maximum(bay['x'][0]-x,x-bay['x'][1])*np.ones((1,ny,nz),np.float32),np.maximum(bay['y'][0]-y,y-bay['y'][1])*np.ones((nx,1,nz),np.float32),1.8),
             smax(z-bay['ztop'],(zf(Y)[None,:,None]-z),1.8)*np.ones((nx,1,1),np.float32),1.8)
    F=np.minimum(F,cav-2.3)                                   # wall >= 2.3 around the cavity
    sw=p['sw']; rho=np.hypot(y-sw['y'],z-sw['z'])
    hole=np.maximum(rho-sw['hole_r'],np.maximum(x-774.0,760.0-x)); pocket=np.maximum(rho-sw['pocket_r'],np.maximum(x-783.0,sw['wall_x'][1]-x))
    sd=np.hypot(np.maximum(np.abs(x-837.0)-8.0,0),y-552.0)-7.0      # v5.7 wire hole 30 x 14 mm, centred on the housing in x, toward +y
    stad=np.maximum(sd,np.abs(z+17.6)-2.6)
    # v5.8 big wire hole on the housing top below the valves/mount, from the bay up into the open pod back
    qx=np.abs(x-817.0)-14.0; qy=np.abs(y-536.0)-6.0
    big2=np.hypot(np.maximum(qx,0),np.maximum(qy,0))+np.minimum(np.maximum(qx,qy),0)-6.0
    bigh=np.maximum(big2,np.abs(z+17.6)-2.9)
    air=np.minimum(np.minimum(cav,hole),np.minimum(pocket,stad))
    # v5.8: open the pod back: no back wall between the side rims
    def abox(x0,x1,y0,y1,z0,z1): return np.maximum(np.maximum(np.maximum(x0-x,x-x1),np.maximum(y0-y,y-y1)),np.maximum(z0-z,z-z1))
    backair=np.minimum(abox(752.0,772.3,539.07,545.8,-5.93,-3.0),abox(772.3,865.9,539.07,545.8,-16.2,-3.0))
    F=np.maximum(F,-backair)
    Fpre=F.astype(np.float32)
    F=np.maximum(F,-air)
    F=smax(F,-bigh,1.8)                 # 1.5 mm rounded edge on the big wire hole
    print('field built',F.shape,round(time.time()-t0,1),'s')
    return F.astype(np.float32),(X,Y,Z),Fpre
def pod_field(secs,X,Y,Z,p):
    """secs: dict z-> 2D signed-distance raster (negative inside walls) on grid RX,RY. linear morph in z between nodes, constant below the lowest."""
    RX,RY,R2=secs['RX'],secs['RY'],secs['maps']; zs=secs['zs']; nx,ny,nz=len(X),len(Y),len(Z)
    gx=(X-RX[0])/(RX[1]-RX[0]); gy=(Y-RY[0])/(RY[1]-RY[0]); GX,GY=np.meshgrid(gx,gy,indexing='ij')
    maps=[ndi.map_coordinates(m,[GX,GY],order=1,mode='nearest') for m in R2]
    out=np.empty((nx,ny,nz),np.float32)
    inbox=((X[:,None]>772.0)&(X[:,None]<902.0)&(Y[None,:]>500.0)&(Y[None,:]<572.0))
    for k,zz in enumerate(Z):
        if zz<=zs[0]: d=maps[0]
        elif zz>=zs[-1]: d=maps[-1]
        else:
            j=np.searchsorted(zs,zz)-1; w=(zz-zs[j])/(zs[j+1]-zs[j]); d=(1-w)*maps[j]+w*maps[j+1]
        zlow=np.where(inbox,-17.5,-9.0)
        out[:,:,k]=np.maximum(d,zlow-zz)
    return out
def pod_sections(S0,zs=(-8.5,-7.0,-5.0,-3.7)):
    """thin-wall ring sections of the pod at z=zs -> 2D SDF rasters (0.1 mm)"""
    from skimage.draw import polygon2mask
    RX=np.arange(730.0,920.0,0.1); RY=np.arange(530.0,580.0,0.1); maps=[]
    for z in zs:
        s=S0.section(plane_origin=[0,0,z],plane_normal=[0,0,1]); m=np.zeros((len(RX),len(RY)),bool)
        for q in s.discrete:
            if len(q)<4: continue
            pts=np.c_[(q[:,0]-RX[0])/0.1,(q[:,1]-RY[0])/0.1]; m^=polygon2mask(m.shape,pts)
        d_out=ndi.distance_transform_edt(~m)*0.1; d_in=ndi.distance_transform_edt(m)*0.1
        maps.append((d_out-d_in+ (0.05 if True else 0)*0).astype(np.float32))
    # filled outline of the z=-8.5 section (ring gap closed) for the lobe underside plate
    s=S0.section(plane_origin=[0,0,-8.5],plane_normal=[0,0,1]); m=np.zeros((len(RX),len(RY)),bool)
    for q in s.discrete:
        if len(q)<4: continue
        pts=np.c_[(q[:,0]-RX[0])/0.1,(q[:,1]-RY[0])/0.1]; m^=polygon2mask(m.shape,pts)
    m[int((866-RX[0])/0.1):int((892-RX[0])/0.1),int((539.5-RY[0])/0.1):int((546-RY[0])/0.1)]=True
    mf_=ndi.binary_fill_holes(m); fill=(ndi.distance_transform_edt(~mf_)-ndi.distance_transform_edt(mf_))*0.1
    return dict(RX=RX,RY=RY,maps=maps,zs=np.array(zs),fill=fill.astype(np.float32))
if __name__=='__main__':
    z=np.load('cur/shell_pebble_v4.npz'); S0=trimesh.Trimesh(z['v'],z['f'],process=False)
    secs=pod_sections(S0); F,(X,Y,Z),Fpre=build_field(PAR,secs)
    np.save('cache/F.npy',F)
    v,f,_,_=measure.marching_cubes(F,0.0,spacing=(H,H,H)); v=v+LO
    m=trimesh.Trimesh(v,f,process=False); print('mc faces',len(m.faces),'watertight',m.is_watertight)
    m.export('out/housing_mc.stl')
