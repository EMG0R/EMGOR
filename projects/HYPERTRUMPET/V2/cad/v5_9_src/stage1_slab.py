"""v5.9 stage 1: remove the dovetail slab (rib, channel, panel, stop block) from the shell back.
Cutter = XZ polygon (left x 853, right = wall inner surface x_w(z) continued straight down in y) extruded y 541.0..553.9,
bottom = the housing top measured at y 553.9 (continued in y), top z 116.64 (underside of the roof foot). The +x rim foot (y<544, last 5.4 mm of the wall)
and the rod bosses R2/R3/R4 (with columns) are protected. Input cur/shell_pebble_v4.npz -> out/s1.npz"""
import numpy as np, trimesh, sys
from geom import *
import rods
def run(S):
    zs=np.arange(-6,118,2.0)
    # wall inner surface x_w(z) at y=553.2: first hit going +x
    o=np.c_[np.full(len(zs),870.0),np.full(len(zs),553.2),zs]; loc,ir,_=S.ray.intersects_location(o,np.tile([1.0,0,0],(len(o),1)),multiple_hits=False)
    xw=np.full(len(zs),np.nan); xw[ir]=loc[:,0]
    ok=~np.isnan(xw); xw=np.interp(zs,zs[ok],xw[ok])
    from scipy.ndimage import gaussian_filter1d
    xw=gaussian_filter1d(xw,1.5,mode='nearest')
    print('x_w(z)',dict(zip(zs[::6].round(0).tolist(),xw[::6].round(2).tolist())))
    ZB,ZT=-17.6,116.64
    # outline polygon in (x,z): left 853, right x_w(z) (minus 0.0), bottom/top flat (bottom is replaced by the floor profile below)
    pts=[(853.0,ZB),(xw[0],ZB)]+[(x,z) for z,x in zip(zs,xw)]+[(xw[-1],ZT),(853.0,ZT)]
    from shapely.geometry import Polygon
    poly=Polygon(pts).buffer(0)
    pm=trimesh.creation.extrude_polygon(poly,553.9-541.0)           # extruded along +z of the polygon frame
    V=pm.vertices; P=np.c_[V[:,0],541.0+V[:,2],V[:,1]]               # (x, y, z)
    cut=trimesh.Trimesh(P,pm.faces[:,::-1],process=False)
    if cut.volume<0: cut.invert()
    cutm=to_m(cut)
    # floor: keep everything below the measured housing top (per x), constant in y
    xs=np.arange(852.0,900.0,1.0)
    o=np.c_[xs,np.full(len(xs),553.9),np.full(len(xs),0.0)]; loc,ir,_=S.ray.intersects_location(o,np.tile([0,0,-1.0],(len(o),1)),multiple_hits=False)
    zf=np.full(len(xs),np.nan); zf[ir]=loc[:,2]; ok=~np.isnan(zf); zf=np.interp(xs,xs[ok],zf[ok]); zf=np.minimum(zf,-9.0)
    print('floor z(x)',dict(zip(xs[::6].round(0).tolist(),zf[::6].round(2).tolist())))
    # keep-below volume: region z < floor(x) over the footprint -> subtract from cutter
    below=[]
    for i in range(len(xs)-1):
        z0,z1=zf[i],zf[i+1]
        below.append(np.array([[xs[i],540.0,ZB-1],[xs[i+1],540.0,ZB-1],[xs[i+1],554.5,ZB-1],[xs[i],554.5,ZB-1],[xs[i],540.0,z0],[xs[i+1],540.0,z1],[xs[i+1],554.5,z1],[xs[i],554.5,z0]],np.float32))
    bm=mf.Manifold.batch_boolean([mf.Manifold.hull_points(b) for b in below],mf.OpType.Add)
    cutm=cutm-bm
    # protect: +x rim foot (x > x_w-5.4, y < 544) and rod bosses R2,R3,R4 (R1 belongs to the lobe)
    fx=[(xw[i]-5.4,z) for i,z in enumerate(zs)]
    foot=Polygon([(xw[0]-5.4,ZB),(xw[0]+3,ZB)]+[(x+3,z) for z,x in zip(zs,xw)]+[(xw[-1]+3,ZT)]+[(x-5.4,z) for z,x in reversed(list(zip(zs,xw)))]).buffer(0)
    fm=trimesh.creation.extrude_polygon(foot,544.0-540.5); V=fm.vertices; FP=np.c_[V[:,0],540.5+V[:,2],V[:,1]]; ft=trimesh.Trimesh(FP,fm.faces[:,::-1],process=False)
    if ft.volume<0: ft.invert()
    cutm=cutm-to_m(ft)
    bs=rods.bosses(); 
    for i in (1,2,3): cutm=cutm-bs[i]
    R=from_m(to_m(S)-cutm)
    return R
if __name__=='__main__':
    z=np.load('cur/shell_pebble_v4.npz'); S=trimesh.Trimesh(z['v'],z['f'],process=False)
    R=run(S); print('s1',len(R.faces),R.is_watertight,1-R.euler_number/2,len(R.split(only_watertight=False)))
    save_npz('out/s1.npz',R); R.export('out/s1.stl')
