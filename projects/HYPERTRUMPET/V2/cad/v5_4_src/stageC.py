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
def carve_logo(sm,cx=787.5,cz=21.7,Wtot=70.0):
    tools,outs,(Wb,Hb,Hbig)=LG.build_tools(Wtot,cx,cz)
    win=([748.0,563.0,7.0],[827.0,569.5,36.0])
    sm=sm-to_m(box(*win))
    sm=sm+to_m(box([748.0,564.9,7.0],[827.0,FACE-0.5,36.0]))+to_m(dense_box([748.0,FACE-0.5,7.0],[827.0,FACE,36.0],0.4))
    tm=[mf.Manifold.hull_points(np.asarray(t,np.float32)) for t in tools]
    cut=mf.Manifold.batch_boolean(tm,mf.OpType.Add); sm=sm-cut
    info=dict(block_mm=[round(Wb,1),round(Hb,1)],big_cap_height_mm=round(Hbig,2),small_cap_height_mm=round(0.5*Hbig,2),centre_xz=[cx,cz],big_stroke_mm=1.0,small_stroke_mm=0.9,big_depth_mm=1.2,small_depth_mm=0.85,wall_under_deepest_mm=round(FACE-1.2-564.9,2))
    return sm,info
def run():
    t0=time.time()
    z=np.load('cur/shell_pebble_v4.npz'); S0=trimesh.Trimesh(z['v'],z['f'],process=False)
    Sup=load_stl('out/sup_dec.stl'); print('S_up decimated',len(Sup.faces),Sup.is_watertight)
    Sup=planarize(Sup)
    low=to_m(load_stl('out/shell_low_dec.stl'))
    sm=to_m(Sup)+low
    print('before logo',len(from_m(sm).faces)); sm,info=carve_logo(sm); print('after logo',len(from_m(sm).faces))
    pad=to_m(load_stl('out/pad_dec.stl')); sm=sm+pad
    R=from_m(sm); print('assembled',len(R.faces),R.is_watertight,1-R.euler_number/2,time.time()-t0)
    save_npz('out/shell_c.npz',R); json.dump(info,open('out/logo_info.json','w'))
if __name__=='__main__': run()
