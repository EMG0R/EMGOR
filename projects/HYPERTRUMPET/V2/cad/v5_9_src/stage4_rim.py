"""v5.9 stage 4: floor-spur cleanup (x 782..853), near-lossless decimation of the stitched shell, and r=1 rounding of the open-back rim edge on the old (non-regenerated) rim.
Rim = faces on the plane y=541.6 (normal -y). Their plan outline (x,z) is traced; every CONVEX boundary edge (probe above the rim plane lands in air) gets a swept corner-remover (square minus quarter disc, r 1.0)
subtracted along it. Concave boundaries (rim face running into other solid) and rod-socket mouths are left alone. The regenerated -x lobe rim (z<100) already carries its r 0.95 round from the field."""
import numpy as np, trimesh, sys, time
from shapely.geometry import Polygon, MultiPolygon
from shapely.ops import unary_union
from shapely import affinity
from shapely.geometry.polygon import orient
import fast_simplification as fs
from geom import *
import rods
REAR=541.6; R=1.0
def spur_cut(M):
    cut=to_m(box([781.9,532.0,-16.4],[853.0,553.7,-3.0]))+to_m(box([852.9,532.0,-16.4],[886.0,541.6,-3.0]))+to_m(box([885.9,538.0,-12.0],[893.0,541.6,-3.0]))   # spurs + the cap-block remnant behind the rim plane
    return M-cut
def decimate(S,target,agg=3):
    v,f=fs.simplify(np.asarray(S.vertices),np.asarray(S.faces),target_count=int(target),agg=agg); return trimesh.Trimesh(v,f,process=False)
def rim_polygon(S):
    tri=S.triangles; n=S.face_normals; cen=tri.mean(1)
    rimf=(n[:,1]<-0.98)&(np.abs(tri[:,:,1]-REAR).max(1)<0.03)
    inN=(cen[:,0]<783.5)&(cen[:,2]<100.0)&((cen[:,2]<3.0)|(cen[:,0]<760.0))      # faces of the regenerated lobe (already rounded by the field)
    sel=rimf&~inN
    T=tri[sel]; polys=[Polygon(t[:,[0,2]]) for t in T if abs(Polygon(t[:,[0,2]]).area)>1e-9]
    P=unary_union([p.buffer(0.0005) for p in polys]).buffer(-0.0005); return P,int(sel.sum())
def crescent(res=7):
    a=np.linspace(-np.pi/2,-np.pi,res+1)                              # arc centre (1,1) radius 1, from (1,0) to (0,1)
    arc=[(1+np.cos(t),1+np.sin(t)) for t in a]
    return Polygon([(-0.3,-0.3),(1.0,-0.3)]+arc+[(-0.3,1.0)])
def bite_prism(a,b,n_in,prof):
    L=np.linalg.norm(b-a); t=(b-a)/L
    pm=trimesh.creation.extrude_polygon(prof,L)                         # profile in XY (u,v), extruded along +Z
    V=pm.vertices; u,v,w=V[:,0],V[:,1],V[:,2]
    P=np.zeros((len(V),3)); P[:,0]=a[0]+n_in[0]*u+t[0]*w; P[:,2]=a[1]+n_in[1]*u+t[1]*w; P[:,1]=REAR+v
    m=trimesh.Trimesh(P,pm.faces,process=False)
    if m.volume<0: m.invert()
    return m
def round_rim(S,skip_centres):
    P,nf=rim_polygon(S); print('rim faces',nf,'polygon parts',len(P.geoms) if hasattr(P,'geoms') else 1,'area',round(P.area,1))
    prof=crescent(); bites=[]; skipped=0; probes=[]; segs=[]
    polys=list(P.geoms) if hasattr(P,'geoms') else [P]
    for pg in polys:
        pg=orient(pg.simplify(0.004),1.0)
        for ring in [pg.exterior]+list(pg.interiors):
            c=np.array(ring.coords)
            for i in range(len(c)-1):
                a,b=c[i],c[i+1]; L=np.linalg.norm(b-a)
                if L<0.05: continue
                t=(b-a)/L; n_out=np.array([t[1],-t[0]]); m=(a+b)/2
                if any(np.hypot(*(m-s))<4.3 for s in skip_centres): skipped+=1; continue
                segs.append((a,b,n_out)); probes.append([m[0]+0.5*n_out[0],REAR+0.35,m[1]+0.5*n_out[1]])
    solid=S.contains(np.array(probes)); print('boundary edges',len(segs),'concave/skipped',int(solid.sum()),'socket skipped',skipped)
    for (a,b,n_out),sd in zip(segs,solid):
        if sd: continue
        bites.append(to_m(bite_prism(a,b,-n_out,prof)))
    print('bites',len(bites))
    cutter=mf.Manifold.batch_boolean(bites,mf.OpType.Add)
    return from_m(to_m(S)-cutter)
if __name__=='__main__':
    t0=time.time(); S=load_npz(sys.argv[1] if len(sys.argv)>1 else 'out/s2.npz'); print('in',len(S.faces))
    S=from_m(spur_cut(to_m(S))); print('spur cut',len(S.faces),S.is_watertight)
    S=decimate(S,385000); print('dec',len(S.faces),S.is_watertight,round(time.time()-t0,1))
    skip=[(x,z) for _,x,z in rods.RODS]
    R=round_rim(S,skip); print('rim',len(R.faces),R.is_watertight,1-R.euler_number/2,round(time.time()-t0,1))
    parts=R.split(only_watertight=False); R=max(parts,key=lambda p:len(p.faces)); print('kept main body of',len(parts),'(rest are zero-volume boolean remnants)',len(R.faces),R.is_watertight,'genus',1-R.euler_number/2)
    save_npz('out/s4.npz',R); R.export('out/s4.stl')
