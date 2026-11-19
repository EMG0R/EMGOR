"""v5.5 logo: Sheeping Cats (Chequered Ink) outlines from Blender text -> flat-floor engrave 1.2 mm."""
import numpy as np, json, trimesh
from shapely.geometry import Polygon
from shapely.ops import unary_union
from shapely import affinity
from geom import *
FACE=568.4; DEPTH=1.2
def line_polys(loops):
    P=[Polygon(l) for l in loops if len(l)>=3]; P=[p.buffer(0) for p in P]
    # even-odd: sort by area desc, holes = contained polygons (odd depth)
    P.sort(key=lambda p:-p.area); out=None
    res=[]
    for i,p in enumerate(P):
        depth=sum(1 for j,q in enumerate(P) if j!=i and q.area>p.area and q.contains(p.representative_point()))
        res.append((p,depth))
    shape=None
    for p,d in res:
        if d%2==0: shape=p if shape is None else shape.union(p)
    for p,d in res:
        if d%2==1: shape=shape.difference(p)
    return shape
def layout(txt_json,Wtot=70.0,cx=787.5,cz=21.7,gapk=0.45,smallk=0.5):
    T=json.load(open(txt_json)); sh={k:line_polys(v) for k,v in T.items()}
    b=sh['HYPERTRUMPET'].bounds; sB=Wtot/(b[2]-b[0]); capB=(b[3]-b[1])*sB
    capS=smallk*capB; gap=gapk*capB; Hb=2*capS+capB+2*gap
    out=[]; zt=Hb/2
    for key,cap in (('THE',capS),('HYPERTRUMPET',capB),('EMGOR',capS)):
        g=sh[key]; bb=g.bounds; s=cap/(bb[3]-bb[1])
        g=affinity.scale(g,s,s,origin=(0,0)); bb=g.bounds
        g=affinity.translate(g,-(bb[0]+bb[2])/2,(zt-cap)-bb[1])      # centre x, top of line at zt
        out.append((key,g)); zt-=cap+gap
    # mirror in x for reading from +y, place at (cx,cz)
    res=[]
    for k,g in out:
        g=affinity.scale(g,-1,1,origin=(0,0)); g=affinity.translate(g,cx,cz); res.append((k,g))
    return res,dict(block_mm=[round(Wtot,1),round(Hb,1)],big_cap_height_mm=round(capB,2),small_cap_height_mm=round(capS,2),centre_xz=[cx,cz])
def tools(res):
    T=[]
    for k,g in res:
        for p in (g.geoms if hasattr(g,'geoms') else [g]):
            if p.is_empty or p.area<0.05: continue
            m=trimesh.creation.extrude_polygon(p,DEPTH+0.6)            # z in [0,1.8]
            v=m.vertices.copy(); V=np.c_[v[:,0],(FACE-DEPTH)+v[:,2],v[:,1]]   # x, y=depth axis, z=poly y
            mm=trimesh.Trimesh(V,m.faces[:,::-1],process=False)
            if mm.volume<0: mm.invert()
            T.append(mm)
    return T
if __name__=='__main__':
    res,info=layout('out/text.json'); print(info)
    import matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
    fig,ax=plt.subplots(figsize=(16,6))
    for k,g in res:
        for p in (g.geoms if hasattr(g,'geoms') else [g]):
            x,y=p.exterior.xy; ax.fill([-(v-787.5) for v in x],[v-21.7 for v in y],color='k')
            for h in p.interiors: x,y=h.xy; ax.fill([-(v-787.5) for v in x],[v-21.7 for v in y],color='w')
    ax.set_aspect('equal'); plt.savefig('fontlogo2d.png',dpi=60)
