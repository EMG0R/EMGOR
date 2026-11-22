"""v5.4 stage B: split the analytic field at the level plane z=-44 into shell-lower and door, add snap features as field ops, mesh each with marching cubes (no blur)."""
import numpy as np, trimesh, json, time
from scipy import ndimage as ndi
from skimage import measure
from fieldA import *
P=P_SPLIT; GAP=0.2
def dist_in_map(Fpre,X,Y,Z,zs):
    """distance (mm) from the outline, inside the solid (cavity filled), min over several z slices (the snap ring must stay inside every slice)"""
    masks=[]
    for zz in zs:
        k=int(round((zz-LO[2])/H)); sl=Fpre[:,:,k]
        # upsample to 0.1 mm by bilinear interpolation of the field, then EDT
        f=3; gx=np.linspace(0,len(X)-1,(len(X)-1)*f+1); gy=np.linspace(0,len(Y)-1,(len(Y)-1)*f+1); GX,GY=np.meshgrid(gx,gy,indexing='ij')
        up=ndi.map_coordinates(sl,[GX,GY],order=1); masks.append(up<0)
    m=np.logical_and.reduce(masks); d=ndi.distance_transform_edt(m)*(H/3)
    gx=np.arange(len(X))*3; gy=np.arange(len(Y))*3
    return d[np.ix_(gx,gy)]
def close_mc(F,lo,pad=2):
    Fp=np.pad(F,pad,constant_values=1.0); v,f,_,_=measure.marching_cubes(Fp,0.0,spacing=(H,H,H)); v=v-pad*H+lo
    m=trimesh.Trimesh(v,f,process=False); return m
def run():
    z=np.load('cur/shell_pebble_v4.npz'); S0=trimesh.Trimesh(z['v'],z['f'],process=False)
    secs=pod_sections(S0); F,(X,Y,Z),Fpre=build_field(PAR,secs); t0=time.time()
    nx,ny,nz=F.shape; x=X[:,None,None]; y=Y[None,:,None]; zz=Z[None,None,:]
    dm=dist_in_map(Fpre,X,Y,Z,(P-0.6,P+2.0,P+4.0,P+6.5))[:,:,None]            # (nx,ny,1)
    keep=((x<800.0)&(y>517.0)&(y<547.0))                                        # ring omitted over the switch zone
    ring=lambda a,b,z0,z1: np.maximum(np.maximum(a-dm,dm-b),np.maximum(z0-zz,zz-z1))
    LIP=6.0
    lip=ring(2.0,3.5,P-1.0,P-GAP+LIP); lip=np.where(keep,1.0,lip)
    bead=ring(3.5,4.0,P-GAP+LIP*0.45,P-GAP+LIP*0.85); bead=np.where(keep,1.0,bead)
    groove=ring(1.8,3.7,P-0.5,P+LIP+0.4); groove=np.where(keep,1.0,groove)
    pocket=ring(3.6,4.3,P-GAP+LIP*0.45-0.1,P-GAP+LIP*0.85+0.2); pocket=np.where(keep,1.0,pocket)
    # door
    Fd=np.maximum(F,zz-(P-GAP+0.011)); Fd=np.minimum(np.minimum(Fd,lip),bead)
    notch=None
    for x0 in (805.0,837.0,869.0):                       # 8 x 1.2 mm nail notches on the flat -y wall, straddling the parting line
        n=np.maximum(np.maximum(np.abs(x-x0)-4.0,y-(PAR['y0']+1.2)),np.maximum((P-3.0)-zz,zz-(P+1.0)))
        notch=n if notch is None else np.minimum(notch,n)
    Fd=np.maximum(Fd,-notch)
    # shell lower
    Fs=np.maximum(F,(P+0.011)-zz); Fs=np.maximum(np.maximum(Fs,-groove),-pocket)
    kd=int(round((P+7.0-LO[2])/H)); ks=int(round((P-0.5-LO[2])/H))
    door=close_mc(Fd[:,:,:kd],LO); shell=close_mc(Fs[:,:,ks:],LO+np.array([0,0,ks*H]))
    print('mc door',len(door.faces),'shell low',len(shell.faces),round(time.time()-t0,1))
    for n,m in (('door',door),('shell_low',shell)): print(n,'watertight',m.is_watertight,'volume',round(m.volume),'bodies',len(m.split(only_watertight=False)),'genus',1-m.euler_number/2)
    save_npz('out/door_mc.npz',door); save_npz('out/shell_low_mc.npz',shell)
if __name__=='__main__': run()
