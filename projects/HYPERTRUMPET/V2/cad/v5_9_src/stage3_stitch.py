"""v5.9 stage 3: cut the old lobe/junction region out of the shell (stage-1 result) and stitch the freshly meshed field into it.
Q = R1 (plan distance < 14 mm from the -x outline, z 2..100, y 538..575) + B1 (x<782, z -36..2, y 528..590)."""
import numpy as np, trimesh, pickle, sys
from skimage import measure
from shapely.geometry import Polygon
from geom import *
import rods
from stage2_lobe import *
import tables as TB
def qsolid(xo):
    X=np.arange(690.0,800.0,0.1); Z=np.arange(-8.0,102.0,0.1); D=din_grid(xo,X,Z)
    cs=measure.find_contours(D,SEAM_D); c=max(cs,key=len); pts=np.c_[690.0+0.1*c[:,0],-8.0+0.1*c[:,1]]
    pts=pts[(pts[:,1]>=ZB1-1e-6)&(pts[:,1]<=ZT+1e-6)]; o=np.argsort(pts[:,1]); pts=pts[o]
    z0,z1=ZB1,ZT; xs0=np.interp(z0,pts[:,1],pts[:,0]); xs1=np.interp(z1,pts[:,1],pts[:,0])
    poly=[(680.0,z0),(xs0,z0)]+[(x,z) for x,z in pts[::3] if z0<z<z1]+[(xs1,z1),(680.0,z1)]
    P=Polygon(poly).buffer(0); pm=trimesh.creation.extrude_polygon(P,575.0-538.0); V=pm.vertices
    R1=trimesh.Trimesh(np.c_[V[:,0],538.0+V[:,2],V[:,1]],pm.faces[:,::-1],process=False)
    if R1.volume<0: R1.invert()
    B1=box([680.0,528.0,ZBOT],[XB,590.0,ZB1])
    return to_m(R1)+to_m(B1),P
def hole_patch(R):
    """torn through-hole in the face plate above the junction (x 764..782, z -3.3..1.3): fill with a face-plate prism (y 565.68..568.40), hull of the through cells + 1 mm"""
    from shapely.geometry import MultiPoint
    xs=np.arange(762.0,792.0,0.25); zs=np.arange(-4.5,2.5,0.25); X,Z=np.meshgrid(xs,zs,indexing='ij')
    o=np.c_[X.ravel(),np.full(X.size,600.0),Z.ravel()]; loc,ir,_=R.ray.intersects_location(o,np.tile([0,-1.0,0],(len(o),1)),multiple_hits=False)
    Y=np.full(X.size,np.nan); Y[ir]=loc[:,1]; bad=np.isnan(Y)|(Y<567.5)
    pts=np.c_[X.ravel()[bad],Z.ravel()[bad]]; print('torn hole cells',len(pts),'x',pts[:,0].min(),pts[:,0].max(),'z',pts[:,1].min(),pts[:,1].max())
    hull=MultiPoint(pts).convex_hull.buffer(1.0)
    pm=trimesh.creation.extrude_polygon(hull,568.40-565.68); V=pm.vertices
    pt=trimesh.Trimesh(np.c_[V[:,0],565.68+V[:,2],V[:,1]],pm.faces[:,::-1],process=False)
    if pt.volume<0: pt.invert()
    return to_m(pt)
def run(S1,N,xo):
    Q,P=qsolid(xo)
    old=to_m(S1); cut=old-Q
    C=from_m(cut); print('old cut',len(S1.faces),'->',len(C.faces),'watertight',C.is_watertight)
    r=to_m(N)
    b=rods.bosses()[0]; k=rods.sockets()[0]
    R=cut+r+b-k
    R0=from_m(R); R=to_m(R0)+hole_patch(R0)
    return from_m(R)
if __name__=='__main__':
    S1=load_npz('out/s1.npz'); S=TB.load_shell(); xo,_=TB.outline(S)
    N=load_npz('out/lobe_mc.npz') if len(sys.argv)<2 else load_npz(sys.argv[1])
    R=run(S1,N,xo); print('s2',len(R.faces),R.is_watertight,1-R.euler_number/2,len(R.split(only_watertight=False)))
    save_npz('out/s2.npz',R); R.export('out/s2.stl')
