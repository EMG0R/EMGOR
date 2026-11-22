"""v5.3 logo: blocky angular caps as stroke polylines. Each straight stroke -> convex hexagon (flat/joint ends squared, free ends chisel-pointed) -> V-roof polytope (exact straight-skeleton roof of a convex polygon), union of all roofs = clean V-carve, no joint spikes."""
import numpy as np
from scipy.spatial import HalfspaceIntersection, ConvexHull
from shapely.geometry import LineString, Polygon
import trimesh
FACE_Y=568.4
W_=1.0     # placeholder
def chamf(w,c=0.2): return c*w
def glyphs(W):
    c=0.2*W; b=0.24*W   # chamfer sizes
    G={
    'T':[[(0,1),(W,1)],[(W/2,1),(W/2,0)]],
    'H':[[(0,0),(0,1)],[(W,0),(W,1)],[(0,.5),(W,.5)]],
    'E':[[(W,1),(0,1),(0,0),(W,0)],[(0,.5),(.82*W,.5)]],
    'Y':[[(0,1),(W/2,.52),(W,1)],[(W/2,.52),(W/2,0)]],
    'P':[[(0,0),(0,1),(W-b,1),(W,1-b),(W,.44+b),(W-b,.44),(0,.44)]],
    'R':[[(0,0),(0,1),(W-b,1),(W,1-b),(W,.44+b),(W-b,.44),(0,.44)],[(.38*W,.44),(W,0)]],
    'U':[[(0,1),(0,b),(b,0),(W-b,0),(W,b),(W,1)]],
    'M':[[(0,0),(0,1),(W/2,.45),(W,1),(W,0)]],
    'G':[[(W,1-b),(W-b,1),(b,1),(0,1-b),(0,b),(b,0),(W-b,0),(W,b),(W,.47),(.5*W,.47)]],
    'O':[[(b,1),(W-b,1),(W,1-b),(W,b),(W-b,0),(b,0),(0,b),(0,1-b),(b,1)]],
    }
    return G
def stroke_polys(polys,hw,Hh):
    """returns list of (Polygon, open_flags) ; polygon vertices ccw, flags[i] True if edge i (v_i -> v_i+1) is an open seam (vertical, not sloped)"""
    segs=[]   # (a,b,prev_dir or None,next_dir or None)
    for pl in polys:
        P=[np.array(p,float) for p in pl]
        for k in range(len(P)-1):
            a,b=P[k],P[k+1]
            din=(a-P[k-1]) if k>0 else None; dout=(P[k+2]-b) if k+2<len(P) else None
            segs.append([a,b,None if din is None else din/np.linalg.norm(din),None if dout is None else dout/np.linalg.norm(dout)])
    def touches(p,i):
        for j,(c,e,_,_) in enumerate(segs):
            if j==i: continue
            if LineString([c,e]).distance(LineString([p,p+1e-7]))<1e-3: return True
        return False
    out=[]
    for i,(a,b,din,dout) in enumerate(segs):
        d=(b-a)/np.linalg.norm(b-a); n=np.array([-d[1],d[0]])
        def corner(v,dn,sign,start):
            # returns left/right corner at vertex v; dn = neighbour direction at this vertex (or None)
            if dn is None: return v+sign*n*hw
            nn=np.array([-dn[1],dn[0]]); m=(n+nn); k=1+n@nn
            if k<0.2: return v+sign*n*hw
            return v+sign*m*hw/k
        pts=[]; flags=[]
        # build ccw: right side a->b, then end at b, then left side b->a, then end at a   (left = +n)
        ra=corner(a,din,-1,True); rb=corner(b,dout,-1,False); lb=corner(b,dout,+1,False); la=corner(a,din,+1,True)
        poly=[ra,rb]; fl=[False]
        # end at b
        if dout is None and not touches(b,i): poly+= [b+d*hw*1.2,lb]; fl+=[False,False]      # chisel tip
        else: poly+=[lb]; fl+=[True]
        poly+=[la]; fl+=[False]
        if din is None and not touches(a,i): poly+=[a-d*hw*1.2]; fl+=[False,False]
        else: fl+=[True]
        # fl list: edges ra->rb, (rb->tip, tip->lb) or (rb->lb), lb->la, (la->tip, tip->ra) or (la->ra)
        # rebuild flags properly
        edges=[]
        E_=[('side',ra,rb)]
        if dout is None and not touches(b,i): E_+= [('tip',rb,b+d*hw*1.2),('tip',b+d*hw*1.2,lb)]
        else: E_+= [('open',rb,lb)]
        E_+= [('side',lb,la)]
        if din is None and not touches(a,i): E_+= [('tip',la,a-d*hw*1.2),('tip',a-d*hw*1.2,ra)]
        else: E_+= [('open',la,ra)]
        V=np.array([e[1] for e in E_]); F=[e[0]=='open' for e in E_]
        p=Polygon(V)
        if not p.is_valid or p.area<1e-6: continue
        out.append((V,F))
    return out
def roof(V,F,slope,dmax,above=0.6):
    c=np.array(V)
    area=0.5*np.sum(c[:,0]*np.roll(c[:,1],-1)-np.roll(c[:,0],-1)*c[:,1])
    assert area>0, 'polygon must be ccw'
    hs=[]
    for i in range(len(c)):
        a=c[i]; b=c[(i+1)%len(c)]; d=b-a; d/=np.linalg.norm(d); n=np.array([-d[1],d[0]])
        if F[i]: hs.append([-n[0],-n[1],0.0,n@a])
        else: hs.append([-slope*n[0],-slope*n[1],1.0,slope*n@a])
    hs.append([0,0,1,-dmax]); hs.append([0,0,-1,-above])
    hs=np.array(hs)
    cen=c.mean(0); 
    ip=np.array([cen[0],cen[1],min(dmax,0.25*dmax)])
    hi=HalfspaceIntersection(hs,ip)
    return hi.intersections
def layout(W_total,pitch_big=0.84,Wg_big=0.60,small=0.5,gap=0.45,Wg_small=0.98,pitch_small=1.32):
    """returns list of (char,x0,z0,H,Wg,small?) block centred at (0,0) and block w,h given big cap height solved from W_total"""
    Hbig=W_total/(11*pitch_big+Wg_big)
    Hs=small*Hbig; g=gap*Hbig; Hb=2*Hs+Hbig+2*g
    z=Hb/2; out=[]
    for txt,H,Wg,pt,sm in (("THE",Hs,Wg_small,pitch_small,True),("HYPERTRUMPET",Hbig,Wg_big,pitch_big,False),("EMGOR",Hs,Wg_small,pitch_small,True)):
        w=(len(txt)-1)*pt*H+Wg*H; x=-w/2; z-=H
        for ch in txt: out.append((ch,x,z,H,Wg*H,sm)); x+=pt*H
        z-=g
    return out,W_total,Hb,Hbig
def build_tools(W_total,cx,cz,big_hw=0.5,big_d=1.2,small_hw=0.45,small_d=0.85):
    items,Wb,Hb,Hbig=layout(W_total)
    tools=[]; outlines=[]
    for ch,x0,z0,H,Wg,sm in items:
        hw,d=(small_hw,small_d) if sm else (big_hw,big_d)
        G=glyphs(Wg/H)         # glyph units: x in [0,Wg/H], z in [0,1]
        # scale to mm (hw in mm); glyph polylines mm = u*H
        polys=[[(p[0]*H,p[1]*H) for p in pl] for pl in G[ch]]
        for Vv,Ff in stroke_polys(polys,hw,H):
            co=np.c_[cx-(x0+Vv[:,0]),cz+z0+Vv[:,1]]         # mirrored in x so it reads correctly from +y
            n_=len(co); Ff=[bool(Ff[(n_-2-k)%n_]) for k in range(n_)]; co=co[::-1]     # mirror flips winding: reverse vertices, remap edge flags
            outlines.append(Polygon(co))
            Vr=roof(co,Ff,d/hw,d)
            P=np.c_[Vr[:,0],FACE_Y-Vr[:,2],Vr[:,1]]
            tools.append(np.unique(np.round(P,5),axis=0))
    return tools,outlines,(Wb,Hb,Hbig)
