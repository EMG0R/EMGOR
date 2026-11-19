import numpy as np, trimesh, json, time
from geom import *
FACE=568.4
PAD=dict(cx=908.5,cz=112.8,R=14.5,t=4.0,rec_d=19.0,rec_depth=0.6,tail_w=7.5,tail_depth=0.6)
def cyl_pts(cx,cz,R,y0,y1,n=96):
    a=np.linspace(0,2*np.pi,n,endpoint=False); pts=[]
    for y in (y0,y1): pts+= [[cx+R*np.cos(t),y,cz+R*np.sin(t)] for t in a]
    return np.array(pts,np.float32)
def hullm(pts): return mf.Manifold.hull_points(np.asarray(pts,np.float32))
def cyl_y(cx,cz,R,y0,y1,n=96): return hullm(cyl_pts(cx,cz,R,y0,y1,n))
def add_pad(S,P=PAD):
    cx,cz,R,t=P['cx'],P['cz'],P['R'],P['t']
    sm=to_m(S)
    # disc: 4 mm thick root, 1 mm chamfer on the visible rim
    disc=hullm(np.vstack([cyl_pts(cx,cz,R,FACE-t,FACE-1.0),cyl_pts(cx,cz,R-1.0,FACE-1.0,FACE)]))
    # tapered root: hull of the disc back and a block that reaches into the pod side wall (x 890-903), blended
    blk=np.array([[x,y,z] for x in (889.0,903.0) for y in (552.0,FACE-t) for z in (cz-15.0,cz+15.0)],np.float32)
    back=cyl_pts(cx,cz,R,FACE-t,FACE-t,n=48)
    root=hullm(np.vstack([blk,cyl_pts(cx,cz,R*0.85,FACE-t,FACE-t,48)]))
    # two gusset ribs 3 mm thick (triangles in x-y) at z = cz +- 9.5, from the pod wall out to the disc back
    ribs=[]
    for dz in (-9.5,9.5):
        tri=np.array([[x,y,z] for (x,y) in ((901.0,552.0),(901.0,FACE-t+0.01),(cx+6.0,FACE-t+0.01)) for z in (cz+dz-1.5,cz+dz+1.5)],np.float32)
        ribs.append(hullm(tri))
    body=disc+root
    for r in ribs: body=body+r
    sm=sm+body
    # FSR402 locating recess and tail groove (0.6 mm deep), no wire hole
    rec=cyl_y(cx,cz,P['rec_d']/2,FACE-P['rec_depth'],FACE+0.5,n=96)
    tail=to_m(box([893.0,FACE-P['tail_depth'],cz-P['tail_w']/2],[cx,FACE+0.5,cz+P['tail_w']/2]))
    sm=sm-rec-tail
    R_=from_m(sm)
    return R_,body
if __name__=='__main__':
    S=load_npz('cache/shell_l.npz')
    R,body=add_pad(S)
    print('faces',len(R.faces),'watertight',R.is_watertight,'genus',1-R.euler_number/2,'bodies',len(R.split(only_watertight=False)))
    R.export('cache/shell_p.stl'); save_npz('cache/shell_p.npz',R)
    # clearance to mount
    z=np.load('cur/mount_original_arms_plate_v4.npz'); Mt=trimesh.Trimesh(z['v'],z['f'],process=False)
    B=from_m(body); 
    pts=B.sample(60000)
    d=trimesh.proximity.closest_point(Mt,pts)[1]; print('pad body to mount min dist mm',round(d.min(),2))
    print('pad bounds',B.bounds.round(1).tolist())
