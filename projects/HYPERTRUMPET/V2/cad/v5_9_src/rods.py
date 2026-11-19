"""v5.8 rods on the mount and sockets/bosses in the pod back rim (like the v1 pegs)."""
import numpy as np, trimesh
from geom import *
ROD_R=2.5; SOCK_R=2.7; BOSS_R=5.5; REF_Y=541.6
RODS=[('R1',738.0,72.0),('R2',892.5,111.0),('R3',859.0,-8.5),('R4',894.5,45.0)]
def cylY(x,z,r,y0,y1,n=64):
    a=np.linspace(0,2*np.pi,n,endpoint=False); pts=[]
    for y in (y0,y1): pts+=[[x+r*np.cos(t),y,z+r*np.sin(t)] for t in a]
    return mf.Manifold.hull_points(np.array(pts,np.float32))
def bosses():
    out=[]
    for nm,x,z in RODS:
        b=cylY(x,z,BOSS_R,REF_Y,553.0)
        if nm in ('R3','R4'):
            b=b+to_m(box([x-BOSS_R,REF_Y,-17.0],[x+BOSS_R,553.0,z]))        # column down to the housing top
        if nm=='R1': b=b+to_m(box([729.7,REF_Y,z-BOSS_R],[x,553.0,z+BOSS_R]))     # bar back to the -x side wall
        if nm=='R2': b=b+to_m(box([x-BOSS_R,REF_Y,100.0],[x+BOSS_R,553.0,z]))     # merges with the dovetail rib below
        out.append(b)
    return out
def sockets():
    out=[]
    for nm,x,z in RODS:
        s=cylY(x,z,SOCK_R,REF_Y-1.0,REF_Y+8.2)
        # 0.5 mm lead-in chamfer
        a=np.linspace(0,2*np.pi,48,endpoint=False)
        pts=[[x+(SOCK_R+0.5+0.5)*np.cos(t),REF_Y-0.5,z+(SOCK_R+0.5+0.5)*np.sin(t)] for t in a]+[[x+SOCK_R*np.cos(t),REF_Y+0.5,z+SOCK_R*np.sin(t)] for t in a]
        out.append(s+mf.Manifold.hull_points(np.array(pts,np.float32)))
    return out
def rod_mesh(x,z,yf,L_tip=549.2):
    """lathe profile around the y axis: embedded base, r3 root fillet, rod dia 5, chamfered tip"""
    fr=min(3.0,REF_Y-0.25-yf); t=np.linspace(0,np.pi/2,14); rb=ROD_R+fr
    prof=[(0,yf-2.0),(rb,yf-2.0),(rb,yf)]+[(rb-fr*np.sin(a),yf+fr-fr*np.cos(a)) for a in t[1:]]+[(ROD_R,L_tip-0.5),(ROD_R-0.5,L_tip),(0,L_tip)]
    m=trimesh.creation.revolve(np.array(prof)[:,[0,1]],sections=40)      # revolves profile (x=radius,y=height) around z
    V=m.vertices.copy(); P=np.c_[V[:,0],V[:,2],V[:,1]]; P[:,0]+=x; P[:,2]+=z      # height axis -> y
    mm=trimesh.Trimesh(P,m.faces,process=False)
    if mm.volume<0: mm.invert()
    return mm
def build_rods(mount):
    out=[]; info=[]
    for nm,x,z in RODS:
        loc,_,_=mount.ray.intersects_location([[x,600.0,z]],[[0,-1.0,0]],multiple_hits=False); yf=float(loc[0,1]); info.append((nm,x,z,round(yf,2)))
        out.append(to_m(rod_mesh(x,z,yf)))
    mm=to_m(mount)
    for r in out: mm=mm+r
    return from_m(mm),info
