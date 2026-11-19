import numpy as np, trimesh, manifold3d as mf
def to_m(m):
    mm=mf.Mesh(vert_properties=np.asarray(m.vertices,np.float32),tri_verts=np.asarray(m.faces,np.uint32))
    return mf.Manifold(mm)
def from_m(M):
    mm=M.to_mesh(); return trimesh.Trimesh(np.asarray(mm.vert_properties)[:,:3].astype(np.float64),np.asarray(mm.tri_verts),process=False)
def box(lo,hi):
    b=trimesh.creation.box(extents=np.array(hi)-np.array(lo)); b.apply_translation((np.array(hi)+np.array(lo))/2); return b
def clean(m):
    m=m.copy(); m.merge_vertices(); m.update_faces(m.nondegenerate_faces()); m.remove_unreferenced_vertices(); return m
def vox2mesh(X,lo,h,sigma=0.6,pad=2):
    from skimage import measure
    from scipy import ndimage as ndi
    F=np.pad(X.astype(np.float32),pad)
    F=ndi.gaussian_filter(F,sigma)
    v,f,_,_=measure.marching_cubes(F,0.5)
    v=(v-pad+0.5)*h+lo
    m=trimesh.Trimesh(v,f[:,::-1] if False else f,process=False)
    m.fix_normals()
    return m
def decimate(m,n):
    import fast_simplification as fs
    v,f=fs.simplify(np.asarray(m.vertices),np.asarray(m.faces),target_count=n,agg=5)
    return trimesh.Trimesh(v,f,process=False)
def keep_big(X,min_vox=20000,touch_top=True):
    from scipy import ndimage as ndi
    lab,k=ndi.label(X); s=np.bincount(lab.ravel()); keep=s>=min_vox; keep[0]=False
    if touch_top:
        tops=np.unique(lab[:,:,-1]); keep[tops]=keep[tops]|(s[tops]>=100); keep[0]=False
    return keep[lab]
def main_parts(m,min_vol=300.0):
    parts=[p for p in m.split(only_watertight=False) if abs(p.volume)>=min_vol]
    return parts
def dec_safe(m,target,agg=5):
    import fast_simplification as fs
    v,f=fs.simplify(np.asarray(m.vertices),np.asarray(m.faces),target_count=int(target),agg=agg)
    r=clean(trimesh.Trimesh(v,f,process=False)); return r
def save_npz(path,m): np.savez(path,v=np.asarray(m.vertices),f=np.asarray(m.faces))
def load_npz(path):
    z=np.load(path); return trimesh.Trimesh(z['v'],z['f'],process=False)
def finalize(m,digits=4):
    """merge (tolerance 1e-4 mm), drop degenerate faces, round-trip through manifold so the exported STL (float32) stays watertight"""
    m=m.copy(); m.merge_vertices(digits_vertex=digits); m.update_faces(m.nondegenerate_faces(height=1e-6)); m.remove_unreferenced_vertices()
    r=from_m(to_m(m)); r.export('/tmp/_fin_test.stl') if False else None
    return r
def stl_ok(m,path):
    m.export(path); q=trimesh.load(path,force='mesh'); return q.is_watertight and len(q.split(only_watertight=False))==1, q
def heal_export(m,path,tries=4):
    for dg in (4,3,5,2):
        f=finalize(m,dg); f.export(path); q=trimesh.load(path,force='mesh')
        if q.is_watertight and len(q.split(only_watertight=False))==1: return q
        m=q
    return q
