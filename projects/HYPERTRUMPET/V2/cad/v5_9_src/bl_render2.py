"""Headless Blender renderer: python bl_render.py -- spec.json   (spec: {objects:[{stl,color,name}], shots:[{name,loc,target,ortho|lens,res}], out:dir})"""
import bpy, sys, json, math, os
from mathutils import Vector
spec=json.load(open(sys.argv[sys.argv.index('--')+1]))
bpy.ops.wm.read_factory_settings(use_empty=True)
sc=bpy.context.scene
sc.render.engine='BLENDER_EEVEE'
w=bpy.data.worlds.new('w'); w.use_nodes=True; w.node_tree.nodes['Background'].inputs[0].default_value=(0.30,0.31,0.38,1); w.node_tree.nodes['Background'].inputs[1].default_value=1.0; sc.world=w
for o in spec['objects']:
    bpy.ops.wm.stl_import(filepath=o['stl']); ob=bpy.context.selected_objects[0]; ob.name=o.get('name','o')
    m=bpy.data.materials.new('m'); m.use_nodes=True; b=m.node_tree.nodes['Principled BSDF']; b.inputs['Base Color'].default_value=(*o['color'],1); b.inputs['Roughness'].default_value=o.get('rough',0.45)
    ob.data.materials.append(m)
    bpy.context.view_layer.objects.active=ob
    if o.get('smooth',True):
        try: bpy.ops.object.shade_smooth_by_angle(angle=math.radians(o.get('angle',35)))
        except Exception as e: print('smooth_by_angle failed',e); [setattr(p,'use_smooth',True) for p in ob.data.polygons]
    else:
        for p in ob.data.polygons: p.use_smooth=False
cam=bpy.data.cameras.new('c'); co=bpy.data.objects.new('c',cam); sc.collection.objects.link(co); sc.camera=co
def light(name,energy,rot,kind='SUN'):
    l=bpy.data.lights.new(name,kind); l.energy=energy; lo=bpy.data.objects.new(name,l); sc.collection.objects.link(lo); lo.rotation_euler=tuple(math.radians(r) for r in rot)
KEY=light('key',3.0,(45,0,35)) or None
light('fill',1.3,(60,0,-140)); light('rim',1.0,(-30,0,200))
L=[o for o in bpy.data.objects if o.name.startswith('key')][0]
os.makedirs(spec['out'],exist_ok=True)
for s in spec['shots']:
    sc.render.resolution_x,sc.render.resolution_y=s.get('res',[1400,1000])
    if 'ortho' in s: cam.type='ORTHO'; cam.ortho_scale=s['ortho']
    else: cam.type='PERSP'; cam.lens=s.get('lens',50)
    cam.clip_start=1; cam.clip_end=5000
    co.location=Vector(s['loc']); d=Vector(s['target'])-co.location
    co.rotation_euler=d.to_track_quat('-Z','Y').to_euler()
    # key light relative to camera
    L.rotation_euler=tuple(math.radians(r) for r in s.get('keyrot',(45,0,35))); L.data.energy=s.get('keyE',3.0)
    sc.render.filepath=os.path.join(spec['out'],s['name']+'.png'); bpy.ops.render.render(write_still=True); print('rendered',s['name'])
