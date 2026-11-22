import numpy as np, trimesh, time, json
from shapely.geometry import Polygon, box as sbox, MultiPolygon
from shapely.ops import unary_union
from geom import *
P=-44.0; GAP=0.2
def poly_extrude(poly,z0,z1):
    out=[]
    geoms=poly.geoms if hasattr(poly,'geoms') else [poly]
    for g in geoms:
        if g.is_empty or g.area<0.5: continue
        m=trimesh.creation.extrude_polygon(g,z1-z0); m.apply_translation([0,0,z0]); out.append(m)
    return out
def mf_union(ms):
    r=None
    for m in ms:
        x=to_m(clean(m)); r=x if r is None else r+x
    return r
def build():
    door=clean(trimesh.load('cache/door_big_dec.stl',force='mesh'))
    shell=load_npz('cache/shell_spliced.npz')
    sec=door.section(plane_origin=[0,0,P-1.0],plane_normal=[0,0,1])
    polys=[Polygon(p[:,:2]) for p in sec.discrete if len(p)>=4]
    outer=max(polys,key=lambda p:p.area); door_out=outer
    for zz in (P+0.5,P+2.0,P+4.0,P+6.5):                      # most-inset outline over the lip height, so the groove never breaks the outer skin
        sh=shell.section(plane_origin=[0,0,zz],plane_normal=[0,0,1])
        pp=[Polygon(q[:,:2]) for q in sh.discrete if len(q)>=4]
        outer=outer.intersection(max(pp,key=lambda p:p.area))
    if hasattr(outer,'geoms'): outer=max(outer.geoms,key=lambda p:p.area)
    outer=Polygon(outer.exterior).simplify(0.03)
    print('outline area',outer.area,outer.bounds)
    a=outer.buffer(-2.0,join_style=1); b=outer.buffer(-3.5,join_style=1)
    ring=a.difference(b)
    ga=outer.buffer(-1.8,join_style=1); gb=outer.buffer(-3.7,join_style=1)       # outer skin above the groove 1.8 mm
    groove=ga.difference(gb)
    # bead on lip INNER face, pocket in shell inner groove wall
    bead=b.buffer(0.5,join_style=1).difference(b)            # 0.5 mm proud inward... (inner face of lip is at inset 2.9 => bead is toward cavity)
    pocket=gb.buffer(0.6,join_style=1).difference(gb)
    # avoid the switch zone (-x wall, around y 531.7) and keep clear of the toggle
    keepout=sbox(760,517,800,547)
    ring=ring.difference(keepout); groove=groove.difference(keepout); bead=bead.difference(keepout); pocket=pocket.difference(keepout)
    LIPH=6.0
    door_t=to_m(door)^to_m(box([700,450,-80],[960,620,P-GAP]))
    lip=mf_union(poly_extrude(ring,P-1.0,P-GAP+LIPH))
    bd=mf_union(poly_extrude(bead.intersection(ring.buffer(0.6)),P-GAP+LIPH*0.45,P-GAP+LIPH*0.85))
    # bead: a 45 deg wedge would be nicer; use a box bead capped with chamfer via hull is overkill -> straight bead 0.5 x 2.4
    # nail notches (door side), 3 x 8 mm wide, 1.2 deep, straddling the line, -y side
    strip=door_out.buffer(0.6).difference(outer.buffer(-1.2,join_style=1))
    notches=[strip.intersection(sbox(x-4,480,x+4,520)) for x in (805.0,837.0,869.0)]
    nt=mf_union([m for n in notches for m in poly_extrude(n,P-3.0,P+0.01)])
    g=lambda M:1-from_m(M).euler_number/2
    print('genus door_t',g(door_t),'lip',g(lip),'bd',g(bd),'door_t+lip',g(door_t+lip),'+bd',g(door_t+lip+bd),'nt',g(nt))
    door_f=door_t+lip+bd
    door_f=door_f-nt
    sm=to_m(shell); gm=mf_union(poly_extrude(groove,P-0.01,P+LIPH+0.4)); pm=mf_union(poly_extrude(pocket.intersection(groove.buffer(0.7)),P+LIPH*0.45-0.1,P+LIPH*0.85+0.2))
    print('shell vol',sm.volume(),'groove',gm.volume(),'pocket',pm.volume(),sm.status())
    shell_f=sm-gm-pm
    D=from_m(door_f); S=from_m(shell_f)
    pass
    inter=(to_m(D)^to_m(S)).volume()
    print('door',len(D.faces),D.is_watertight,round(D.volume),1-D.euler_number/2,'| shell',len(S.faces),S.is_watertight,round(S.volume),1-S.euler_number/2,'| door x shell mm3',inter)
    return D,S
if __name__=='__main__':
    D,S=build(); D.export('cache/door_f.stl'); S.export('cache/shell_f.stl'); save_npz('cache/door_f.npz',D); save_npz('cache/shell_f.npz',S)
