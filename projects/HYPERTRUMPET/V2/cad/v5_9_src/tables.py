"""v5.9 tables: measured pod sweep profile (d,y), -x outline x_o(z), housing cavity profile (y,z) -> exact 2D signed-distance tables (npz).
Run from the work dir with cur/shell_pebble_v4.npz (v5.8 shell) present.  The pod is a pure sweep: the section at z=60/75/90/100 is identical in (d = x - x_o(z), y)."""
import numpy as np, trimesh, json
from shapely.geometry import Polygon, box, Point
from shapely.ops import unary_union
import shapely
from scipy.interpolate import UnivariateSpline
FACE=568.40; REAR=541.6; FACE_IN=565.68; RIM_R=0.95

def load_shell(p='cur/shell_pebble_v4.npz'):
    z=np.load(p); return trimesh.Trimesh(z['v'],z['f'],process=False)

def outline(S):
    """x_o(z): outer end-wall x at y 543..550, z -6..110 (first hit going +x), smoothed; linear extrapolation outside"""
    zs=np.arange(-6.0,111.0,1.0); xs=[]
    for y in (543.0,546.0,550.0):
        o=np.c_[np.full(len(zs),700.0),np.full(len(zs),y),zs]; loc,ir,_=S.ray.intersects_location(o,np.tile([1.0,0,0],(len(o),1)),multiple_hits=False)
        r=np.full(len(zs),np.nan); r[ir]=loc[:,0]; xs.append(r)
    xs=np.nanmedian(np.array(xs),0)
    sp=UnivariateSpline(zs,xs,k=4,s=len(zs)*0.0003)
    res=xs-sp(zs); print('x_o fit resid max',np.abs(res).max().round(3),'at z',zs[np.abs(res).argmax()])
    s0=sp.derivative()(-6.0); x0=float(sp(-6.0))
    def xo(z):
        z=np.asarray(z,float); out=sp(np.clip(z,-6.0,110.0))
        lo=z<-6.0; out=np.where(lo,x0+s0*(z+6.0),out); return out
    return xo,sp

def sweep_profile(S,zz=75.0,xo=None,dmax=33.0):
    """measured shell band polygon at z=zz in (d,y); returns O2 (outer solid, filled), C2 (cavity, open back) extended far in d"""
    s=S.section(plane_origin=[0,0,zz],plane_normal=[0,0,1]); best=None
    for e in s.discrete:
        if e[:,0].min()<735 and len(e)>50: best=e
    P=Polygon(best[:,:2]); xf=float(xo(zz)) if xo is not None else best[:,0].min()
    P=P.intersection(box(xf-5,REAR-0.5,xf+dmax,FACE+1))
    from shapely import affinity
    P=affinity.translate(P,-xf,0)
    P=P.buffer(0)
    P=P.buffer(-RIM_R,join_style=1,resolution=24).buffer(RIM_R,join_style=1,resolution=24)   # open-back rim: convex corners rounded r=%.2f
    dm=P.bounds[2]; print('profile d range',P.bounds)
    B=box(0.0,REAR,dm,FACE)
    R=B.difference(P)
    parts=list(R.geoms) if R.geom_type=='MultiPolygon' else [R]
    outside=[p for p in parts if p.contains(Point(0.3,FACE-0.4))]
    assert outside, 'outside component not found'
    O2=B.difference(outside[0]).buffer(0)
    cav=[p for p in parts if not p.contains(Point(0.3,FACE-0.4)) and p.area>20]
    C2=unary_union(cav)
    BIG=120.0
    O2=unary_union([O2,box(dm-1,REAR,BIG,FACE)])                       # face plate continues far in +d (flat)
    C2=unary_union([C2,box(5.3,REAR-12,BIG,544.0),box(dm-1,REAR-12,BIG,FACE_IN)])   # open back, flat inner face
    return O2.buffer(0),C2.buffer(0),P

def sdf_table(poly,d0,d1,y0,y1,h=0.04):
    """signed distance (negative inside) on a grid, exact to the polygon boundary"""
    ds=np.arange(d0,d1+h,h); ys=np.arange(y0,y1+h,h); D,Y=np.meshgrid(ds,ys,indexing='ij')
    pts=shapely.points(D.ravel(),Y.ravel()); bd=poly.boundary
    dist=shapely.distance(bd,pts); ins=shapely.contains(poly,pts)
    return (np.where(ins,-dist,dist).reshape(D.shape)).astype(np.float32),ds,ys

def housing_cavity():
    """air region above the housing top inside the pod face plate: floor z=-16.4, measured concave sweep to the face inner plane (y,z)"""
    cur=np.array([[554.2,-16.4],[556.1,-16.34],[557.0,-16.2],[557.9,-16.0],[558.8,-15.73],[559.43,-15.5],[560.3,-15.1],[560.91,-14.75],[561.88,-14.06],[562.41,-13.59],[562.93,-13.03],
                  [563.48,-12.33],[563.99,-11.5],[564.43,-10.59],[564.77,-9.69],[565.04,-8.73],[565.27,-8.0],[565.43,-7.4],[565.56,-6.5],[565.6,-5.59],[565.62,-4.68],[565.66,-3.5]])
    pts=[(500.0,-16.4)]+[tuple(p) for p in cur]+[(FACE_IN,40.0),(500.0,40.0)]
    return Polygon(pts).buffer(0)

if __name__=='__main__':
    S=load_shell(); xo,sp=outline(S)
    zz=np.arange(-9.5,111,10.0); print('x_o',dict(zip(zz.round(1).tolist(),np.round(xo(zz),2).tolist())))
    O2,C2,P=sweep_profile(S,92.0,xo)
    print('O2 area',round(O2.area,1),'C2 area',round(C2.area,1),'P area',round(P.area,1))
    tO,ds,ys=sdf_table(O2,-4.0,60.0,REAR-6,FACE+4); tC,_,_=sdf_table(C2,-4.0,60.0,REAR-6,FACE+4)
    np.savez('out/tables_profile.npz',tO=tO,tC=tC,d0=ds[0],y0=ys[0],h=0.04)
    H=housing_cavity(); tB,bs,zs_=sdf_table(H,500.0,570.0,-20.0,40.0,0.05); np.savez('out/tables_house.npz',tB=tB,y0=bs[0],z0=zs_[0],h=0.05)
    print('tables done',tO.shape,tC.shape,tB.shape)
    import pickle; pickle.dump(dict(zs=np.arange(-6.0,111.0,1.0),xo=xo(np.arange(-6.0,111.0,1.0))),open('out/xo.pkl','wb'))
