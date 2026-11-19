"""Blender headless: build THE / HYPERTRUMPET / EMGOR as filled 2D outlines from the font -> JSON polygons (mm) via curve->mesh flattening.
usage: blender -b --python bl_text.py -- font.otf out.json"""
import bpy, sys, json, bmesh
a=sys.argv[sys.argv.index('--')+1:]; font=a[0]; out=a[1]
bpy.ops.wm.read_factory_settings(use_empty=True)
fnt=bpy.data.fonts.load(font)
res={}
for name,txt in (('THE','THE'),('HYPERTRUMPET','HYPERTRUMPET'),('EMGOR','EMGOR')):
    cu=bpy.data.curves.new(name,'FONT'); cu.body=txt; cu.font=fnt; cu.size=10.0; cu.resolution_u=24; cu.fill_mode='BOTH'
    cu.align_x='LEFT'; cu.space_character=1.0
    ob=bpy.data.objects.new(name,cu); bpy.context.collection.objects.link(ob)
    dg=bpy.context.evaluated_depsgraph_get(); ev=ob.evaluated_get(dg); me=ev.to_mesh()
    # boundary edges -> loops
    bm=bmesh.new(); bm.from_mesh(me)
    edges=[e for e in bm.edges if e.is_boundary]
    adj={}
    for e in edges:
        v0,v1=e.verts; adj.setdefault(v0.index,[]).append(v1.index); adj.setdefault(v1.index,[]).append(v0.index)
    seen=set(); loops=[]
    for e in edges:
        s=e.verts[0].index
        if s in seen: continue
        loop=[s]; seen.add(s); prev=None; cur=s
        while True:
            nxt=[n for n in adj[cur] if n!=prev and n not in seen]
            if not nxt: break
            prev=cur; cur=nxt[0]; seen.add(cur); loop.append(cur)
        loops.append([[bm.verts.ensure_lookup_table() or bm.verts[i].co.x, bm.verts[i].co.y] for i in loop])
    res[name]=loops
    print(name,len(loops),'loops',len(me.vertices),'verts')
json.dump(res,open(out,'w'))
