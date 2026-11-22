"""v5.4 stage C: assemble the shell. S0 (v5.2) above z=-6 + analytic housing; planarised control face; logo window + dense flat slab + V-carve; FSR pad; Seed corner."""
import numpy as np, trimesh, json, time
from geom import *
import logo as LG
FACE=LG.FACE_Y
def load_stl(p): m=trimesh.load(p,force='mesh'); return m
def planarize(S,tol=0.12):
    V=S.vertices.copy(); n=S.vertex_normals
    sel=(np.abs(V[:,1]-FACE)<tol)&(n[:,1]>0.9); print('planarised face vertices',int(sel.sum()),'max shift',float(np.abs(V[sel,1]-FACE).max()))
    V[sel,1]=FACE; return trimesh.Trimesh(V,S.faces,process=False)
def dense_box(lo,hi,edge=0.45):
    b=box(lo,hi); v,f=trimesh.remesh.subdivide_to_size(b.vertices,b.faces,max_edge=edge); m=trimesh.Trimesh(v,f,process=False); m.merge_vertices(); return m
def carve_logo(sm,cx=787.5,cz=21.7,Wtot=60.5):
    import fontlogo
    res,info=fontlogo.layout('out/text.json',Wtot,cx,cz); T=fontlogo.tools(res)
    sm=sm-to_m(box([748.0,563.0,7.0],[827.0,569.5,36.0]))
    sm=sm+to_m(box([748.0,564.9,7.0],[827.0,FACE-0.5,36.0]))+to_m(dense_box([748.0,FACE-0.5,7.0],[827.0,FACE,36.0],0.4))
    tm=[to_m(t) for t in T]; bad=[i for i,m in enumerate(tm) if str(m.status())!='Error.NoError']; print('font tools',len(tm),'bad',bad)
    cut=mf.Manifold.batch_boolean([m for i,m in enumerate(tm) if i not in bad],mf.OpType.Add); sm=sm-cut
    info.update(font='Sheeping Cats (Chequered Ink), flat floor',depth_mm=fontlogo.DEPTH,wall_under_floor_mm=round(FACE-fontlogo.DEPTH-564.9,2))
    return sm,info
def run():
    t0=time.time()
    z=np.load('cur/shell_pebble_v4.npz'); S0=trimesh.Trimesh(z['v'],z['f'],process=False)
    Sup=load_stl('out/sup_dec.stl'); print('S_up decimated',len(Sup.faces),Sup.is_watertight)
    Sup=planarize(Sup)
    low=to_m(load_stl('out/shell_low_dec.stl'))
    sm=to_m(Sup)+low
    print('before logo',len(from_m(sm).faces)); sm,info=carve_logo(sm); print('after logo',len(from_m(sm).faces))
    pad=to_m(load_stl('out/lobe_dec.stl')); sm=sm+pad
    R=from_m(sm); print('assembled',len(R.faces),R.is_watertight,1-R.euler_number/2,time.time()-t0)
    save_npz('out/shell_c.npz',R); json.dump(info,open('out/logo_info.json','w'))
if __name__=='__main__': run()
