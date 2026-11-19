import numpy as np, trimesh, json, time
from geom import *
import logo as LG
def carve_logo(S, cx=787.5, cz=21.7, Wtot=70.0):
    t0=time.time()
    tools,outs,(Wb,Hb,Hbig)=LG.build_tools(Wtot,cx,cz)
    sm=to_m(S)
    # remove the v5.2 inside boss, fill the v5.2 engraving, add the new inside boss
    sm=sm-to_m(box([748.5,563.0,13.0],[803.5,566.12,35.5]))
    sm=sm+to_m(box([750.0,566.0,15.0],[802.0,LG.FACE_Y,33.5]))
    boss=box([cx-Wb/2-3,565.0,cz-Hb/2-3],[cx+Wb/2+3,566.45,cz+Hb/2+3])
    sm=sm+to_m(boss)
    tm=[mf.Manifold.hull_points(np.asarray(t,np.float32)) for t in tools]
    print('bad tools',[i for i,m in enumerate(tm) if str(m.status())!='Error.NoError' or m.volume()<=0])
    cut=mf.Manifold.batch_boolean(tm,mf.OpType.Add)
    sm=sm-cut
    R=from_m(sm)
    info=dict(block_mm=[round(Wb,1),round(Hb,1)],big_cap_height_mm=round(Hbig,2),small_cap_height_mm=round(0.5*Hbig,2),centre_xz=[cx,cz],big_stroke_mm=1.0,small_stroke_mm=0.9,big_depth_mm=1.2,small_depth_mm=0.85,
              wall_under_deepest_mm=round(LG.FACE_Y-1.2-565.0,2),faces=int(len(R.faces)),watertight=bool(R.is_watertight),genus=float(1-R.euler_number/2))
    print(info,time.time()-t0)
    return R,info
if __name__=='__main__':
    S=load_npz('cache/shell_f.npz')
    R,info=carve_logo(S); R.export('cache/shell_l.stl'); save_npz('cache/shell_l.npz',R); json.dump(info,open('cache/logo_info.json','w'))
