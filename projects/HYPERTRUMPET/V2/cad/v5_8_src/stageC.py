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
def carve_logo(sm,cx=787.0,cz=19.85,Wtot=60.0):
    import fontlogo
    res,info=fontlogo.layout('out/text.json',Wtot,cx,cz,0.70,3.84,0.5); T=fontlogo.tools(res)
    sm=sm-to_m(box([751.0,563.0,1.5],[826.5,569.5,34.5]))
    sm=sm+to_m(box([751.0,564.9,1.5],[826.5,FACE-0.5,34.5]))+to_m(dense_box([751.0,FACE-0.5,1.5],[826.5,FACE,34.5],0.4))
    tm=[to_m(t) for t in T]; bad=[i for i,m in enumerate(tm) if str(m.status())!='Error.NoError']; print('font tools',len(tm),'bad',bad)
    cut=mf.Manifold.batch_boolean([m for i,m in enumerate(tm) if i not in bad],mf.OpType.Add); sm=sm-cut
    info.update(font='Sheeping Cats (Chequered Ink), flat floor',depth_mm=fontlogo.DEPTH,wall_under_floor_mm=round(FACE-fontlogo.DEPTH-564.9,2))
    return sm,info
def smooth_lobe(S,iters=70):
    """masked Taubin on the -x lobe end (removes the rim banding); USB hole edges are excluded"""
    import trimesh.graph as tg
    V=S.vertices.copy(); mask=(V[:,0]<778)&(V[:,2]>-5.2)&(V[:,2]<50)&(V[:,1]>537)
    usb=(V[:,1]>548.5)&(V[:,1]<562.5)&(V[:,2]>-0.5)&(V[:,2]<12)&(V[:,0]<756); mask&=~usb
    n=len(V); E=S.edges_unique; A=[[] for _ in range(n)]
    for a,b in E: A[a].append(b); A[b].append(a)
    idx=np.nonzero(mask)[0]
    for it in range(iters):
        for lam in (0.5,-0.53):
            Vn=V.copy()
            for i in idx:
                nb=A[i]
                if nb: Vn[i]=V[i]+lam*(V[nb].mean(0)-V[i])
            V=Vn
    return trimesh.Trimesh(V,S.faces,process=False)
def rim_cut(S):
    from shapely.geometry import Polygon
    zs=np.arange(-5.1,117.6,3.0); xc=[]
    for zz in zs:
        loc,_,_=S.ray.intersects_location([[700.0,550.0,zz]],[[1,0,0]],multiple_hits=True); h=np.sort(np.array(loc).reshape(-1,3)[:,0])
        xin=h[1] if len(h)>=2 else 748.0; xc.append(min(xin+3.0,775.0))
    xc=np.convolve(np.pad(xc,2,mode='edge'),np.ones(5)/5,mode='valid')
    pts=[(x,z) for x,z in zip(xc,zs)]+[(855.0,zs[-1]),(855.0,zs[0])]
    p=Polygon(pts).buffer(0); m=trimesh.creation.extrude_polygon(p,545.6-539.07); V=m.vertices.copy()
    P=np.c_[V[:,0],539.07+V[:,2],V[:,1]]; mm=trimesh.Trimesh(P,m.faces[:,::-1],process=False)
    if mm.volume<0: mm.invert()
    print('rim edge x(z):',[round(v,1) for v in xc[::6]])
    return mm
def run():
    t0=time.time()
    z=np.load('cur/shell_pebble_v4.npz'); S0=trimesh.Trimesh(z['v'],z['f'],process=False)
    Sup=load_stl('out/sup_dec.stl'); print('S_up decimated',len(Sup.faces),Sup.is_watertight)
    Sup=planarize(Sup)
    Sup=smooth_lobe(Sup)
    Sup=from_m(to_m(Sup)-to_m(rim_cut(Sup)))        # v5.8: open pod back (no back wall); rim follows the sloped side wall: inner wall + 3 mm
    low=to_m(load_stl('out/shell_low_dec.stl'))
    sm=to_m(Sup)+low
    print('before logo',len(from_m(sm).faces)); sm,info=carve_logo(sm); print('after logo',len(from_m(sm).faces))
    pad=to_m(load_stl('out/lobe_dec.stl')); sm=sm+pad
    import rods
    for b in rods.bosses(): sm=sm+b
    for k in rods.sockets(): sm=sm-k
    R=from_m(sm); print('assembled',len(R.faces),R.is_watertight,1-R.euler_number/2,time.time()-t0)
    save_npz('out/shell_c.npz',R); json.dump(info,open('out/logo_info.json','w'))
if __name__=='__main__': run()
