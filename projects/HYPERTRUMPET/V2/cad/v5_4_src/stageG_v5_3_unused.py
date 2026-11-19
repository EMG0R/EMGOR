import numpy as np, trimesh, json, time
from scipy import ndimage as ndi
from geom import *
from vox import voxelize
def seed_boxes(S):
    loc,ri,_=S.ray.intersects_location([[700.,549.,6.]],[[1.,0,0]],multiple_hits=True)
    xin=np.sort(loc[:,0])[1]; mouth=xin+0.4; sx0=mouth+1.55; PT=553.6
    return xin,[('pcb',sx0,sx0+51.1,PT-1.6,PT,-3,15),('hdrA',sx0+.3,sx0+50.8,PT-1.6-8.5,PT-1.6,-2.9,-0.36),('hdrB',sx0+.3,sx0+50.8,PT-1.6-8.5,PT-1.6,12.36,14.9)]
def seed_fix(S,clr=1.0,grow=3.4):
    xin,parts=seed_boxes(S)
    lo=np.array([722.,538.,-16.]); hi=np.array([780.,566.,14.]); h=0.5
    V=voxelize(S.vertices,S.faces,lo,hi,h)
    X,Y,Z=np.meshgrid(lo[0]+h*(np.arange(V.shape[0])+.5),lo[1]+h*(np.arange(V.shape[1])+.5),lo[2]+h*(np.arange(V.shape[2])+.5),indexing='ij')
    left=np.cumsum(V,axis=0)==0; below=np.cumsum(V,axis=2)==0; EA=(left|below)&~V
    d=np.hypot(X-742.0,Z-(-2.0)); g=np.clip(1-d/11.0,0,1); w=g**3*(g*(g*6-15)+10); gz=np.clip((-0.5-Z)/3.0,0,1); w=w*(gz**3*(gz*(gz*6-15)+10))     # stays below the USB-C mouth (z>1.5)
    G=np.zeros(V.shape,bool)
    for r in np.arange(0.5,grow+0.01,0.5):
        reg=(grow*w>=r)&EA
        if reg.any(): G|=(ndi.distance_transform_edt(~V,sampling=h)<=r)&reg
    print('bump vol mm3',G.sum()*h**3)
    out=S
    sm=to_m(S)
    if G.any():
        Gs=ndi.gaussian_filter(np.pad(G.astype(np.float32),2),0.9)
        from skimage import measure
        v,f,_,_=measure.marching_cubes(Gs,0.3); v=(v-2+0.5)*h+lo
        bm=trimesh.Trimesh(v,f,process=False); bm.fix_normals()
        # embed the bump slightly into the shell so the union is clean
        sm=sm+to_m(bm)
    for nm,x0,x1,y0,y1,z0,z1 in parts:
        # 1.0 mm clearance, only around the lobe corner (x < 752, z < 6.5) and never toward the open back rim (no -y expansion)
        cx1=min(x1+clr,752.0); cz1=min(z1+clr,6.5)
        if cx1>x0-clr and cz1>z0-clr: sm=sm-to_m(box([x0-clr,y0,z0-clr],[cx1,y1+clr,cz1]))
    R=from_m(sm)
    return R,dict(seed_inner_wall_x=float(xin),bump_mm3=float(G.sum()*h**3))
def toggle_spotface(S,dia=16.0,y0=531.7,z0=-34.0,x0=773.5,x1=783.0):
    c=trimesh.creation.cylinder(radius=dia/2,height=x1-x0,sections=96); c.apply_transform(trimesh.transformations.rotation_matrix(np.pi/2,[0,1,0]))
    c.apply_translation([(x0+x1)/2,y0,z0])
    return from_m(to_m(S)-to_m(c))
if __name__=='__main__':
    S=load_npz('cache/shell_p.npz'); t=time.time()
    R,info=seed_fix(S); print(info,len(R.faces),R.is_watertight,1-R.euler_number/2,time.time()-t)
    R=toggle_spotface(R); print('toggle',len(R.faces),R.is_watertight,1-R.euler_number/2)
    save_npz('cache/shell_g.npz',R); R.export('cache/shell_g.stl')
