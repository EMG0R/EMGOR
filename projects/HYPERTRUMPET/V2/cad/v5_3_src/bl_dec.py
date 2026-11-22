import bpy, sys
a=sys.argv[sys.argv.index('--')+1:]; src,dst,target=a[0],a[1],int(a[2])
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.wm.stl_import(filepath=src)
o=bpy.context.selected_objects[0] if bpy.context.selected_objects else bpy.data.objects[0]
bpy.context.view_layer.objects.active=o
n=len(o.data.polygons); print('in',n)
m=o.modifiers.new('d','DECIMATE'); m.decimate_type='COLLAPSE'; m.ratio=min(1.0,target/n); m.use_collapse_triangulate=True
bpy.ops.object.modifier_apply(modifier=m.name)
print('out',len(o.data.polygons))
bpy.ops.wm.stl_export(filepath=dst,export_selected_objects=True,ascii_format=False)
