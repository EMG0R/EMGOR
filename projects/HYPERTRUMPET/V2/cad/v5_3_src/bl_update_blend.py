"""Replace the mesh data of named objects in a .blend with meshes from .npz (v,f) files, save to a temp file, then atomically rename.
usage: blender -b SRC.blend --python bl_update_blend.py -- DST.blend name=path.npz [name=path.npz ...]"""
import bpy, sys, os, numpy as np
a=sys.argv[sys.argv.index('--')+1:]; dst=a[0]; reps=dict(x.split('=',1) for x in a[1:])
for name,path in reps.items():
    ob=bpy.data.objects[name]; old=ob.data; z=np.load(path); v=z['v'].astype(np.float32); f=z['f'].astype(np.int32)
    me=bpy.data.meshes.new(old.name+'_v53')
    me.vertices.add(len(v)); me.vertices.foreach_set('co',v.ravel())
    me.loops.add(len(f)*3); me.loops.foreach_set('vertex_index',f.ravel())
    me.polygons.add(len(f)); me.polygons.foreach_set('loop_start',np.arange(0,len(f)*3,3,dtype=np.int32)); me.polygons.foreach_set('loop_total',np.full(len(f),3,np.int32))
    me.polygons.foreach_set('use_smooth',np.ones(len(f),bool)); me.update(calc_edges=True); me.validate()
    for m in old.materials: me.materials.append(m)
    ob.data=me; bpy.data.meshes.remove(old)
    me.name=name; print('replaced',name,len(me.vertices),len(me.polygons))
tmp=dst+'.tmp'
bpy.ops.wm.save_as_mainfile(filepath=tmp,compress=True,copy=True)
os.replace(tmp,dst); print('saved',dst)
