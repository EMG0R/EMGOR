"""v5.10 stage 5: the pot hole at (753.7, 41.7) had a chipped, ragged edge (area 50 vs 56.4 mm2 of its twins). Fill a clean plate disc (r 6.5) and re-cut the hole as a revolved cutter that
matches the good holes: r 4.25 bore, R 0.75 lip on both faces (measured on the hole at (753.7, 101.5)).  in out/s4.npz -> out/s5.npz"""
import numpy as np, trimesh, sys
from shapely.geometry import Point
from geom import *
FACE=568.40; FIN=565.68; RB=4.25; RL=0.75
def cutter(cx,cz):
    ph=np.linspace(0,np.pi/2,10)
    inner=[(5.0-RL*np.sin(p),FIN+RL-RL*np.cos(p)) for p in ph]          # (5.0, 565.68) -> (4.25, 566.43)
    outer=[(5.0-RL*np.cos(p),FACE-RL+RL*np.sin(p)) for p in ph]         # (4.25, 567.65) -> (5.0, 568.4)
    prof=[(0.0,FIN-0.6),(5.0,FIN-0.6)]+inner+outer+[(5.0,FACE+0.4),(0.0,FACE+0.4)]
    m=trimesh.creation.revolve(np.array(prof),sections=96); V=m.vertices
    P=np.c_[V[:,0]+cx,V[:,2],V[:,1]+0]; P[:,2]=V[:,1]*0+V[:,2]*0   # placeholder, rebuilt below
    P=np.c_[V[:,0],V[:,2],V[:,1]]                                      # (x, y=height axis, z)
    P[:,0]+=cx; P[:,2]+=cz
    mm=trimesh.Trimesh(P,m.faces,process=False)
    if mm.volume<0: mm.invert()
    return mm
def disc(cx,cz,r=6.5):
    pm=trimesh.creation.extrude_polygon(Point(cx,cz).buffer(r,resolution=64),FACE-FIN); V=pm.vertices
    pt=trimesh.Trimesh(np.c_[V[:,0],FIN+V[:,2],V[:,1]],pm.faces[:,::-1],process=False)
    if pt.volume<0: pt.invert()
    return pt
if __name__=='__main__':
    S=load_npz('out/s4.npz'); cx,cz=753.7,41.7
    R=from_m(to_m(S)+to_m(disc(cx,cz))-to_m(cutter(cx,cz)))
    parts=R.split(only_watertight=False); R=max(parts,key=lambda p:len(p.faces)); print('s5',len(R.faces),R.is_watertight,'genus',1-R.euler_number/2)
    save_npz('out/s5.npz',R)
