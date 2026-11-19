import bpy, numpy as np, sys
out=sys.argv[sys.argv.index('--')+1]
for o in bpy.data.objects:
    if o.type!='MESH': continue
    m=o.data; n=len(m.vertices); v=np.empty(n*3); m.vertices.foreach_get('co',v); v=v.reshape(-1,3)
    mw=np.array(o.matrix_world); v=v@mw[:3,:3].T+mw[:3,3]
    m.calc_loop_triangles(); f=np.empty(len(m.loop_triangles)*3,np.int32); m.loop_triangles.foreach_get('vertices',f)
    np.savez(f"{out}/{o.name}.npz",v=v,f=f.reshape(-1,3))
    print(o.name,n,len(m.polygons),len(m.loop_triangles))
