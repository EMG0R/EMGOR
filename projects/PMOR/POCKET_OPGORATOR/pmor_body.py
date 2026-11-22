"""PMOR v2 body-shell generator. Reload + build() to iterate the form fast."""
import bpy, bmesh, math

def _segs(E):
    for lim, n in ((0.55,128), (0.30,64), (0.15,32), (0.07,16)):
        if E > lim: return n
    return 8

def build(name="V2_BODY", LEN=160.0, DEPTH=34.0, W_MIN=97.0, W_MAX=115.0,
          N_XZ=3.0, N_Y=3.5, HIP_K=3.5, NY=161, hide_others=True, N_BOT=None):
    """N_XZ: cross-section roundness (lower = rounder).
       N_Y:  end closure fullness (higher = blunter dome).
       HIP_K: hip lobe shape (HIGHER = broader, flatter-topped hips)."""
    def env(v): return (1.0 - min(abs(v),1.0)**N_Y)**(1.0/N_Y)
    def wd(v):
        u, e = (v+1.0)/2.0, env(v)
        hip = 1.0 - abs(math.cos(2*math.pi*u))**HIP_K
        return (W_MIN + (W_MAX-W_MIN)*hip)*e/2.0, (DEPTH/2.0)*e
    nb = N_XZ if N_BOT is None else N_BOT
    def sec(a, b, RN):
        out = []
        for k in range(RN):
            th = 2*math.pi*k/RN
            c, s = math.cos(th), math.sin(th)
            p = 2.0/(N_XZ if s >= 0 else nb)      # flatter top, rounder bottom
            out.append((a*math.copysign(abs(c)**p, c), b*math.copysign(abs(s)**p, s)))
        return out

    vs = [-math.cos(math.pi*i/(NY-1)) for i in range(NY)]
    bm = bmesh.new()
    ps = bm.verts.new((0.0, -LEN/2.0, 0.0)); pn = bm.verts.new((0.0, LEN/2.0, 0.0))
    rings = []
    for v in vs[1:-1]:
        a, b = wd(v)
        rings.append([bm.verts.new((x, v*LEN/2.0, z)) for (x, z) in sec(a, b, _segs(env(v)))])
    bm.verts.ensure_lookup_table()

    def stitch(A, B):
        na, nb = len(A), len(B)
        if na == nb:
            for j in range(na):
                k = (j+1) % na; bm.faces.new((A[j], A[k], B[k], B[j]))
        elif na == 2*nb:
            for j in range(nb):
                k = (j+1) % nb
                f0, f1, f2 = A[2*j], A[2*j+1], A[(2*j+2) % na]
                bm.faces.new((B[j], f0, f1)); bm.faces.new((B[j], f1, f2, B[k]))
        elif nb == 2*na:
            for j in range(na):
                k = (j+1) % na
                f0, f1, f2 = B[2*j], B[2*j+1], B[(2*j+2) % nb]
                bm.faces.new((A[j], f1, f0)); bm.faces.new((A[j], A[k], f2, f1))
        else:
            raise ValueError("bad stitch %d->%d" % (na, nb))

    for i in range(len(rings)-1): stitch(rings[i], rings[i+1])
    for j in range(len(rings[0])):
        bm.faces.new((ps, rings[0][(j+1) % len(rings[0])], rings[0][j]))
    for j in range(len(rings[-1])):
        bm.faces.new((pn, rings[-1][j], rings[-1][(j+1) % len(rings[-1])]))
    bm.normal_update(); bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])

    n = 1
    while bpy.data.objects.get("%s_v%02d" % (name, n)): n += 1
    oname = "%s_v%02d" % (name, n)
    me = bpy.data.meshes.new(oname)
    bm.to_mesh(me); bm.free()
    for p in me.polygons: p.use_smooth = True
    ob = bpy.data.objects.new(oname, me)
    bpy.context.scene.collection.objects.link(ob)
    if bpy.data.materials.get("petg"): me.materials.append(bpy.data.materials["petg"])
    if hide_others:
        for o in bpy.data.objects:
            if o.type == 'MESH' and o is not ob:
                o.hide_viewport = True; o.hide_set(True); o.hide_render = True
    ob.hide_render = False

    bm2 = bmesh.new(); bm2.from_mesh(me)
    chk = {"non_manifold": len([e for e in bm2.edges if not e.is_manifold]),
           "ngons": len([f for f in bm2.faces if len(f.verts) > 4]),
           "euler": len(bm2.verts)-len(bm2.edges)+len(bm2.faces),
           "max_valence": max(len(v.link_edges) for v in bm2.verts)}
    bm2.free()

    def drop(xq, v=0.0):
        a, b = wd(v)
        if xq >= a: return None
        return round(b - b*(1.0-(xq/a)**N_XZ)**(1.0/N_XZ), 2)
    return {"object": oname, "dims_mm": [round(d,2) for d in ob.dimensions],
            "verts": len(me.vertices), "waist_W": round(wd(0.0)[0]*2,1),
            "hip_W": round(max(wd(x/1000.0)[0] for x in range(-999,1000))*2,1),
            "W_y68": round(wd(0.85)[0]*2,1), "D_y68": round(wd(0.85)[1]*2,1),
            "top_drop_x28": drop(28.0), "top_drop_x40": drop(40.0), **chk}

BASE = "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/Pocket-OpGorator/"

def shots(obname, tag):
    import mathutils
    sc = bpy.context.scene
    cam = bpy.data.objects.get("InspectCam_obj") or bpy.data.objects.get("Camera")
    sc.camera = cam; cam.data.type = 'ORTHO'
    sc.render.engine = 'BLENDER_EEVEE'
    sc.render.resolution_x, sc.render.resolution_y = 1500, 1050
    sc.world.use_nodes = True
    sc.world.node_tree.nodes["Background"].inputs[0].default_value = (0.05,0.05,0.06,1)
    out = []
    for loc, rot, osc, nm in (((180,-160,140), None, 215, "iso"),
                              ((0,0,300), (0,0,0), 185, "top"),
                              ((0,-300,0), (math.radians(90),0,0), 130, "north"),
                              ((300,0,0), (math.radians(90),0,math.radians(90)), 185, "side")):
        cam.location = loc; cam.data.ortho_scale = osc
        if rot is None:
            d = mathutils.Vector((0,0,0)) - mathutils.Vector(loc)
            cam.rotation_euler = d.to_track_quat('-Z','Y').to_euler()
        else:
            cam.rotation_euler = rot
        f = "%s%s_%s.png" % (BASE, tag, nm)
        sc.render.filepath = f
        bpy.ops.render.render(write_still=True)
        out.append(f)
    return out


# ---------------------------------------------------------------- shell + seam
def show(o, v=True):
    o.hide_viewport = not v; o.hide_render = not v
    try: o.hide_set(not v)
    except Exception: pass

def _cube(name, s, loc):
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=s, verts=bm.verts)
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new(name, me); ob.location = loc
    bpy.context.scene.collection.objects.link(ob); show(ob); return ob

def boolean(tgt, cut, op, label="bool", allow_noop=False):
    """Boolean that FAILS LOUDLY. Hidden objects are not evaluated by the
    depsgraph, so both operands are forced visible first — a silent no-op
    here is the single easiest way to corrupt this pipeline."""
    show(tgt); show(cut); bpy.context.view_layer.update()
    def _vol(o):
        bb = bmesh.new(); bb.from_mesh(o.data)
        v = bb.calc_volume(signed=True); bb.free(); return v
    before, vbefore = len(tgt.data.vertices), _vol(tgt)
    m = tgt.modifiers.new("b", 'BOOLEAN')
    m.operation = op; m.object = cut; m.solver = 'MANIFOLD'
    dg = bpy.context.evaluated_depsgraph_get()
    new = bpy.data.meshes.new_from_object(tgt.evaluated_get(dg))
    tgt.modifiers.clear(); old = tgt.data; tgt.data = new
    bpy.data.meshes.remove(old)
    for p in tgt.data.polygons: p.use_smooth = True
    after, vafter = len(tgt.data.vertices), _vol(tgt)
    if before == after and abs(vafter - vbefore) < 1.0 and not allow_noop:
        raise RuntimeError("BOOLEAN NO-OP: %s (verts %d, vol %.1f unchanged)"
                           % (label, before, vbefore/1000.0))
    return {"op": label, "before": before, "after": after,
            "vol_cm3": round(vafter/1000.0, 2),
            "d_vol_cm3": round((vafter-vbefore)/1000.0, 2)}

def stats(ob):
    bm = bmesh.new(); bm.from_mesh(ob.data)
    r = {"verts": len(bm.verts),
         "non_manifold": len([e for e in bm.edges if not e.is_manifold]),
         "boundary": len([e for e in bm.edges if e.is_boundary]),
         "ngons": len([f for f in bm.faces if len(f.verts) > 4]),
         "euler": len(bm.verts)-len(bm.edges)+len(bm.faces),
         "max_val": max(len(v.link_edges) for v in bm.verts),
         "vol_cm3": round(bm.calc_volume(signed=True)/1000.0, 2),
         "z": [round(min(v.co.z for v in bm.verts),2),
               round(max(v.co.z for v in bm.verts),2)]}
    bm.free(); return r

def make_shell(body_name, wall=2.5, name="V2_SHELL_v01"):
    src = bpy.data.objects[body_name]; show(src)
    o = bpy.data.objects.get(name)
    if o: bpy.data.objects.remove(o, do_unlink=True)
    ob = src.copy(); ob.data = src.data.copy(); ob.name = name; ob.data.name = name
    bpy.context.scene.collection.objects.link(ob); show(ob)
    m = ob.modifiers.new("Solidify", 'SOLIDIFY')
    m.thickness = wall; m.offset = -1.0
    m.use_even_offset = True; m.use_quality_normals = True
    bpy.context.view_layer.update()
    dg = bpy.context.evaluated_depsgraph_get()
    new = bpy.data.meshes.new_from_object(ob.evaluated_get(dg))
    ob.modifiers.clear(); old = ob.data; ob.data = new; bpy.data.meshes.remove(old)
    for p in ob.data.polygons: p.use_smooth = True
    # NEVER remove_doubles / dissolve_degenerate / edge_face_add here.
    return ob

def split_seam(shell_name, body_name, seam=-12.0, wall=2.5, lip_t=1.6,
               clear=0.2, embed=0.15, z_lo=None, z_hi=None):
    from mathutils.bvhtree import BVHTree
    z_lo = seam - 5.0 if z_lo is None else z_lo
    z_hi = seam + 3.0 if z_hi is None else z_hi   # stay clear of the ports
    for n in ("V2_TUB", "V2_PLATE", "_hi", "_lo", "LIP_RING"):
        o = bpy.data.objects.get(n)
        if o: bpy.data.objects.remove(o, do_unlink=True)
    SH = bpy.data.objects[shell_name]; show(SH)
    log = []
    hi = _cube("_hi", (400,400,300), (0,0,seam+150))
    lo = _cube("_lo", (400,400,300), (0,0,seam-150))
    TUB = SH.copy(); TUB.data = SH.data.copy(); TUB.name = "V2_TUB"
    bpy.context.scene.collection.objects.link(TUB); show(TUB)
    log.append(boolean(TUB, hi, 'INTERSECT', "tub"))
    PLA = SH.copy(); PLA.data = SH.data.copy(); PLA.name = "V2_PLATE"
    bpy.context.scene.collection.objects.link(PLA); show(PLA)
    log.append(boolean(PLA, lo, 'INTERSECT', "plate"))

    BODY = bpy.data.objects[body_name]
    bb = bmesh.new(); bb.from_mesh(BODY.data); BV = BVHTree.FromBMesh(bb); bb.free()

    def _make_lip(name, clr, thick, zhi_over=0.0, zlo_over=0.0):
        bs = bmesh.new(); bs.from_mesh(SH.data)
        # CLEAN band: bisect to straight rims. Selecting faces by centre-z gives
        # a ragged, face-granular edge -- that was the jagged lip.
        ZH, ZL = z_hi+zhi_over, z_lo-zlo_over
        for co, co_out, co_in in ((ZH, True, False), (ZL, False, True)):
            bmesh.ops.bisect_plane(bs, geom=bs.verts[:]+bs.edges[:]+bs.faces[:],
                                   plane_co=(0,0,co), plane_no=(0,0,1),
                                   clear_outer=co_out, clear_inner=co_in)
        bs.verts.ensure_lookup_table(); bs.faces.ensure_lookup_table(); bs.normal_update()
        def ramp(z):
            if z <= seam-1.5: return -embed
            if z >= seam:     return  clr
            t = (z-(seam-1.5))/1.5
            return -embed + (clr+embed)*(t*t*(3-2*t))
        keep=[]
        for f in bs.faces:
            c=f.calc_center_median()
            loc,nor,idx,d = BV.find_nearest(c, 30.0)
            if d is not None and d > wall*0.6: keep.append(f)
        NRM={}
        for f in keep:
            for v in f.verts:
                if v.index not in NRM:
                    loc,nor,idx,d = BV.find_nearest(v.co, 30.0)
                    # inner surface faces the cavity -> opposite the body normal
                    NRM[v.index] = (-nor).normalized().copy() if nor is not None \
                                   else v.normal.normalized().copy()
        out=bmesh.new(); vo,vi={},{}
        need={v.index:v for f in keep for v in f.verts}
        for i,v in need.items():
            n=NRM[i]; d=ramp(v.co.z)
            vo[i]=out.verts.new(v.co + n*d)
            vi[i]=out.verts.new(v.co + n*(d+thick))
        out.verts.ensure_lookup_table()
        par=set()
        for f in keep:
            idx=[v.index for v in f.verts]
            out.faces.new([vo[i] for i in idx])
            out.faces.new([vi[i] for i in reversed(idx)])
            for k in range(len(idx)):
                e=frozenset((idx[k], idx[(k+1)%len(idx)]))
                par.discard(e) if e in par else par.add(e)
        for e in par:
            a,b=tuple(e); out.faces.new((vo[a],vo[b],vi[b],vi[a]))
        out.normal_update(); bmesh.ops.recalc_face_normals(out,faces=out.faces[:])
        me=bpy.data.meshes.new(name); out.to_mesh(me); out.free(); bs.free()
        ob=bpy.data.objects.new(name,me)
        bpy.context.scene.collection.objects.link(ob); show(ob)
        # flatten the rims AFTER offsetting (offsetting tilts a flat cut edge)
        for zlo,zhi in ((ZH,200.0),(-200.0,ZL)):
            k=_cube("_LTRIM",(400,400,abs(zhi-zlo)),(0,0,(zlo+zhi)/2.0))
            boolean(ob,k,'DIFFERENCE',"trim "+name,allow_noop=True)
            bpy.data.objects.remove(k,do_unlink=True)
        return ob

    LIP = _make_lip("LIP_RING", clear, lip_t)
    lip_st = stats(LIP)
    # keep-out: the same lip grown, so anything the tub adds near the wall
    # (anchors, chamber walls) gets cleared out of the lip's path
    KO = _make_lip("LIP_KEEPOUT", -0.2, lip_t + 0.7, zhi_over=0.5, zlo_over=0.5)
    log.append(boolean(PLA, LIP, 'UNION', "plate += tongue"))
    for o in (hi, lo, LIP): bpy.data.objects.remove(o, do_unlink=True)
    return {"log": log, "tongue": lip_st, "TUB": stats(TUB), "PLATE": stats(PLA)}

def fit_check(seam=-12.0):
    """Clearance across the lap + true interpenetration above the seam plane."""
    from mathutils.bvhtree import BVHTree
    TUB = bpy.data.objects["V2_TUB"]; PLA = bpy.data.objects["V2_PLATE"]
    BODY = [o for o in bpy.data.objects if o.name.startswith("V2_BODY")][-1]
    bb = bmesh.new(); bb.from_mesh(BODY.data); BV = BVHTree.FromBMesh(bb); bb.free()
    bt = bmesh.new(); bt.from_mesh(TUB.data); bt.faces.ensure_lookup_table()
    bp = bmesh.new(); bp.from_mesh(PLA.data); bp.faces.ensure_lookup_table()
    TT = BVHTree.FromBMesh(bt); TP = BVHTree.FromBMesh(bp)
    g = []
    for f in bp.faces:
        c = f.calc_center_median()
        if c.z < seam + 0.1: continue
        loc, nor, idx, d = BV.find_nearest(c, 12.0)
        if d is None or not (2.4 <= d <= 3.0): continue
        l2, n2, i2, d2 = TT.find_nearest(c, 5.0)
        if l2 is not None: g.append(d2)
    above = 0
    for ip, it in TP.overlap(TT):
        if bp.faces[ip].calc_center_median().z > seam + 0.2: above += 1
    g.sort(); bt.free(); bp.free()
    return {"mating_gap": ({"min": round(g[0],3), "med": round(g[len(g)//2],3),
                            "max": round(g[-1],3), "n": len(g)} if g else None),
            "interpenetration_above_seam": above}


# --------------------------------------------------------------- shading
def finalize_shading(ob, angle_deg=30.0):
    """Smooth faces + SHARP edges by dihedral angle.
    Blanket use_smooth=True across boolean seams is what makes cut edges
    look melted / bulging. Sharp edges fix it without touching geometry."""
    me = ob.data
    for p in me.polygons: p.use_smooth = True
    bm = bmesh.new(); bm.from_mesh(me)
    thr = math.radians(angle_deg)
    for e in bm.edges:
        if len(e.link_faces) == 2:
            try: e.smooth = (e.calc_face_angle() < thr)
            except Exception: e.smooth = True
        else:
            e.smooth = False
    bm.to_mesh(me); bm.free(); me.update()
    return {"sharp_edges": sum(1 for e in me.edges if not e.use_edge_sharp) and None}


# --------------------------------------------------------------- full tub
def _box(n,x0,x1,y0,y1,z0,z1):
    bm=bmesh.new(); bmesh.ops.create_cube(bm,size=1.0)
    bmesh.ops.scale(bm,vec=(x1-x0,y1-y0,z1-z0),verts=bm.verts)
    me=bpy.data.meshes.new(n); bm.to_mesh(me); bm.free()
    ob=bpy.data.objects.new(n,me); ob.location=((x0+x1)/2,(y0+y1)/2,(z0+z1)/2)
    bpy.context.scene.collection.objects.link(ob); show(ob); return ob

def _cyl(n,d,z0,z1,x,y,seg=128):
    bm=bmesh.new(); bmesh.ops.create_cone(bm,cap_ends=True,cap_tris=False,segments=seg,
                                          radius1=d/2,radius2=d/2,depth=z1-z0)
    me=bpy.data.meshes.new(n); bm.to_mesh(me); bm.free()
    ob=bpy.data.objects.new(n,me); ob.location=(x,y,(z0+z1)/2)
    bpy.context.scene.collection.objects.link(ob); show(ob); return ob

def _stadium(n,L,H,z0,z1,cx,cy,seg=64):
    r=H/2.0; run=(L-H)/2.0; pts=[]
    for i in range(seg+1):
        a=-math.pi/2+math.pi*i/seg; pts.append((cx+run+r*math.cos(a), cy+r*math.sin(a)))
    for i in range(seg+1):
        a=math.pi/2+math.pi*i/seg; pts.append((cx-run+r*math.cos(a), cy+r*math.sin(a)))
    bm=bmesh.new()
    lo=[bm.verts.new((px,py,z0)) for px,py in pts]
    hi=[bm.verts.new((px,py,z1)) for px,py in pts]
    bm.faces.new(lo); bm.faces.new(list(reversed(hi)))
    for k in range(len(pts)):
        m=(k+1)%len(pts); bm.faces.new((lo[k],lo[m],hi[m],hi[k]))
    bm.normal_update(); bmesh.ops.recalc_face_normals(bm,faces=bm.faces[:])
    me=bpy.data.meshes.new(n); bm.to_mesh(me); bm.free()
    ob=bpy.data.objects.new(n,me); bpy.context.scene.collection.objects.link(ob); show(ob); return ob

def _cone_taper(n,d_bot,d_top,z0,z1,x,y,seg=64):
    bm=bmesh.new(); bmesh.ops.create_cone(bm,cap_ends=True,cap_tris=False,segments=seg,
                                          radius1=d_bot/2.0,radius2=d_top/2.0,depth=z1-z0)
    me=bpy.data.meshes.new(n); bm.to_mesh(me); bm.free()
    ob=bpy.data.objects.new(n,me); ob.location=(x,y,(z0+z1)/2.0)
    bpy.context.scene.collection.objects.link(ob); show(ob); return ob

def _stadium_y(n,L,H,y0,y1,cx,cz,seg=64):
    """Obround prism along Y: L across X, H across Z, semicircular ends."""
    r=H/2.0; run=(L-H)/2.0; pts=[]
    for i in range(seg+1):
        a=-math.pi/2+math.pi*i/seg; pts.append((cx+run+r*math.cos(a), cz+r*math.sin(a)))
    for i in range(seg+1):
        a=math.pi/2+math.pi*i/seg; pts.append((cx-run+r*math.cos(a), cz+r*math.sin(a)))
    bm=bmesh.new()
    lo=[bm.verts.new((px,y0,pz)) for px,pz in pts]
    hi=[bm.verts.new((px,y1,pz)) for px,pz in pts]
    bm.faces.new(lo); bm.faces.new(list(reversed(hi)))
    for k in range(len(pts)):
        m=(k+1)%len(pts); bm.faces.new((lo[k],lo[m],hi[m],hi[k]))
    bm.normal_update(); bmesh.ops.recalc_face_normals(bm,faces=bm.faces[:])
    me=bpy.data.meshes.new(n); bm.to_mesh(me); bm.free()
    ob=bpy.data.objects.new(n,me); bpy.context.scene.collection.objects.link(ob); show(ob); return ob

def _stadium_y(n,L,H,y0,y1,cx,cz,seg=64):
    r=H/2.0; run=(L-H)/2.0; pts=[]
    for i in range(seg+1):
        a=-math.pi/2+math.pi*i/seg; pts.append((cx+run+r*math.cos(a), cz+r*math.sin(a)))
    for i in range(seg+1):
        a=math.pi/2+math.pi*i/seg; pts.append((cx-run+r*math.cos(a), cz+r*math.sin(a)))
    bm=bmesh.new()
    lo=[bm.verts.new((px,y0,pz)) for px,pz in pts]
    hi=[bm.verts.new((px,y1,pz)) for px,pz in pts]
    bm.faces.new(lo); bm.faces.new(list(reversed(hi)))
    for k in range(len(pts)):
        m=(k+1)%len(pts); bm.faces.new((lo[k],lo[m],hi[m],hi[k]))
    bm.normal_update(); bmesh.ops.recalc_face_normals(bm,faces=bm.faces[:])
    me=bpy.data.meshes.new(n); bm.to_mesh(me); bm.free()
    ob=bpy.data.objects.new(n,me); bpy.context.scene.collection.objects.link(ob); show(ob); return ob

def build_tub_full(seam=-12.0, enc_x=-19.0, joy_x=19.0, row_y=6.0,
                   joy_d=16.0, enc_d=7.5, mic_d=10.0, mic_x=-22.0, mic_y=72.0,
                   sw_w=13.0, sw_h=7.0,
                   jack_x=16.0, jack_w=7.0, jack_h=13.0, jack_leave=1.6,
                   thin_wall=2.0, sw_y=72.0,
                   chg_xy=(0.0,-72.0), lift_xy=(20.0,72.0), aux_w=13.0, aux_h=7.0,
                   aux_switches=False,   # NO charge / lift switches in this build
                   screw_d=3.4, csink_d=6.6, screw_holes=True,
                   usb_w=9.5, usb_h=3.4, usb_leave=1.5,
                   trellis_y=-37.0, oled_y=40.0,
                   panel_zo=22.0, panel_zi=19.5, shade_angle=30.0):
    """Rebuild V2_TUB and V2_PLATE with every internal feature, from the shell."""
    from mathutils import Vector as _V
    from mathutils.bvhtree import BVHTree as _BVH
    BODY = bpy.data.objects["V2_BODY_v01"]
    for n in ("_SH15","_CAV15","_SH25","_CAV25"):
        o=bpy.data.objects.get(n)
        if o: bpy.data.objects.remove(o,do_unlink=True)
    split_seam("V2_SHELL_v01","V2_BODY_v01",seam=seam)
    TUB=bpy.data.objects["V2_TUB"]; PLA=bpy.data.objects["V2_PLATE"]

    def mkcav(w,tag):
        sh=make_shell(BODY.name,wall=w,name="_SH%s"%tag)
        cv=BODY.copy(); cv.data=BODY.data.copy(); cv.name="_CAV%s"%tag
        bpy.context.scene.collection.objects.link(cv); show(cv)
        boolean(cv,sh,'DIFFERENCE',"cav%s"%tag); return sh,cv
    SH15,CAV15=mkcav(1.5,"15"); SH25,CAV25=mkcav(2.5,"25")
    SHJ,CAVJ=mkcav(jack_leave,"J"); SHU,CAVU=mkcav(usb_leave,"U")

    ANCH=[(-26.0,-72.0),(26.0,-72.0),(-26.0,72.0),(26.0,72.0)]
    for i,(ax,ay) in enumerate(ANCH):
        p=_cyl("_PAD%d"%i,10.5,-24.0,-12.2,ax,ay)
        boolean(PLA,p,'UNION',"pad%d"%i); bpy.data.objects.remove(p,do_unlink=True)
    boolean(PLA,BODY,'INTERSECT',"trim pads")
    if screw_holes:
        pbm=bmesh.new(); pbm.from_mesh(PLA.data); PVT=_BVH.FromBMesh(pbm); pbm.free()
        for i,(ax,ay) in enumerate(ANCH):
            hh=PVT.ray_cast(_V((ax,ay,-40.0)),_V((0,0,1.0)),100.0)
            oz=-40.0+hh[3]
            c=_cyl("_SCL%d"%i,screw_d,oz-3.0,seam+0.5,ax,ay)
            boolean(PLA,c,'DIFFERENCE',"screw%d"%i); bpy.data.objects.remove(c,do_unlink=True)
            rise=(csink_d-screw_d)/2.0; drop=0.8
            k=_cone_taper("_SCK%d"%i,csink_d+2*drop,screw_d,oz-drop,oz+rise,ax,ay)
            boolean(PLA,k,'DIFFERENCE',"csink%d"%i); bpy.data.objects.remove(k,do_unlink=True)
    for i,(ax,ay) in enumerate(ANCH):
        x0,x1=(abs(ax)-4.0,60.0) if ax>0 else (-60.0,-(abs(ax)-4.0))
        b=_box("_ANB%d"%i,x0,x1,ay-4.0,ay+4.0,seam,-3.0)
        boolean(b,CAV15,'INTERSECT',"anch%d"%i,allow_noop=True)
        boolean(TUB,b,'UNION',"tub+=anch%d"%i); bpy.data.objects.remove(b,do_unlink=True)
        # The anchor runs out to the wall, which is exactly where the plate's
        # lip lives. Notch the lip here or the halves cannot close.
        nx0,nx1=(abs(ax)-4.6,60.0) if ax>0 else (-60.0,-(abs(ax)-4.6))
        nb=_box("_ANN%d"%i,nx0,nx1,ay-4.6,ay+4.6,seam,-4.0)
        boolean(PLA,nb,'DIFFERENCE',"notch lip %d"%i); bpy.data.objects.remove(nb,do_unlink=True)

    for sgn in (1.0,-1.0):
        s="R" if sgn>0 else "L"
        xa=lambda a,b:((a,b) if sgn>0 else (-b,-a))
        bi=_box("_BI"+s,*xa(31.5,38.0),10.0,68.0,seam,9.0)
        bo=_box("_BO"+s,*xa(38.0,44.0),10.0,68.0,seam,13.5)
        boolean(bi,bo,'UNION',"blk%s"%s); bpy.data.objects.remove(bo,do_unlink=True)
        boolean(bi,CAV15,'INTERSECT',"blk%s^cav"%s,allow_noop=True)
        aa=_box("_AA"+s,*xa(33.5,38.0),12.0,66.0,-9.5,6.5)
        ab=_box("_AB"+s,*xa(38.0,40.0),12.0,66.0,-9.5,11.0)
        boolean(aa,ab,'UNION',"air%s"%s); bpy.data.objects.remove(ab,do_unlink=True)
        boolean(aa,CAV25,'INTERSECT',"air%s^cav"%s,allow_noop=True)
        boolean(bi,aa,'DIFFERENCE',"walls%s"%s)
        boolean(TUB,bi,'UNION',"tub+=cham%s"%s)
        bpy.data.objects.remove(bi,do_unlink=True); bpy.data.objects.remove(aa,do_unlink=True)
        d=_box("_DRV"+s,*xa(39.0,45.0),18.0,58.0,-9.0,11.0)
        boolean(TUB,d,'DIFFERENCE',"driver%s"%s); bpy.data.objects.remove(d,do_unlink=True)
        g=bmesh.new(); x0,x1=xa(46.0,60.0); j=0; z=-9.0+2.8
        while z<=11.0-2.8:
            y=18.0+2.8+(3.5 if j%2 else 0.0)
            while y<=58.0-2.8:
                pts=[(y-2.8,z),(y,z-2.8),(y+2.8,z),(y,z+2.8)]
                vl=[g.verts.new((x0,py,pz)) for py,pz in pts]
                vh=[g.verts.new((x1,py,pz)) for py,pz in pts]
                g.faces.new(vl); g.faces.new(list(reversed(vh)))
                for k in range(4):
                    m=(k+1)%4; g.faces.new((vl[k],vl[m],vh[m],vh[k]))
                y+=7.0
            z+=3.5; j+=1
        g.normal_update(); bmesh.ops.recalc_face_normals(g,faces=g.faces[:])
        me=bpy.data.meshes.new("_G"+s); g.to_mesh(me); g.free()
        GR=bpy.data.objects.new("_G"+s,me); bpy.context.scene.collection.objects.link(GR); show(GR)
        boolean(TUB,GR,'DIFFERENCE',"grille%s"%s); bpy.data.objects.remove(GR,do_unlink=True)
        # ACCESS: the chamber must open toward the main cavity or the driver
        # can never be installed. Inner wall becomes a rebated opening + lid.
        ac=_box("_AC"+s,*xa(30.0,34.5),14.0,64.0,-9.0,8.5)
        boolean(TUB,ac,'DIFFERENCE',"access%s"%s); bpy.data.objects.remove(ac,do_unlink=True)
        rb=_box("_RB"+s,*xa(30.0,32.0),12.5,65.5,-10.5,9.5)
        boolean(TUB,rb,'DIFFERENCE',"rebate%s"%s); bpy.data.objects.remove(rb,do_unlink=True)
        lid=_box("CHAMBER_LID_"+s,*xa(30.2,32.0),12.7,65.3,-10.3,9.3)
        plug=_box("_LP"+s,*xa(31.4,34.3),14.2,63.8,-8.8,8.3)
        boolean(lid,plug,'UNION',"lid%s plug"%s); bpy.data.objects.remove(plug,do_unlink=True)
        finalize_shading(lid,shade_angle)

    def _skin_min(cx,cy,w,l,n=11):
        bb=bmesh.new(); bb.from_mesh(BODY.data); tv=_BVH.FromBMesh(bb); bb.free()
        vals=[]
        for a in range(n):
            for b in range(n):
                x=cx-w/2+w*a/(n-1); y=cy-l/2+l*b/(n-1)
                h=tv.ray_cast(_V((x,y,60.0)),_V((0,0,-1.0)),200.0)
                if h[0] is not None: vals.append(60.0-h[3])
        return min(vals)

    SHTH = make_shell(BODY.name, wall=thin_wall, name="_SHTH")
    CAVTH = BODY.copy(); CAVTH.data = BODY.data.copy(); CAVTH.name="_CAVTH"
    bpy.context.scene.collection.objects.link(CAVTH); show(CAVTH)
    boolean(CAVTH, SHTH, 'DIFFERENCE', "cav@thin")

    def _skin_min(cx,cy,w,l,n=11):
        bb2=bmesh.new(); bb2.from_mesh(BODY.data); tv=_BVH.FromBMesh(bb2); bb2.free()
        vals=[]
        for a2 in range(n):
            for b2 in range(n):
                x=cx-w/2+w*a2/(n-1); y=cy-l/2+l*b2/(n-1)
                h=tv.ray_cast(_V((x,y,60.0)),_V((0,0,-1.0)),200.0)
                if h[0] is not None: vals.append(60.0-h[3])
        return min(vals)

    def panel(tag,cx,cy,w,l):
        """1) thin the wall from the inside, 2) level the inner surface to a
        FLAT plane so parts mount square. Outer skin is never touched."""
        c=_box("_TH"+tag,cx-w/2,cx+w/2,cy-l/2,cy+l/2,8.0,60.0)
        boolean(c,CAVTH,'INTERSECT',"thin%s^cav"%tag,allow_noop=True)
        boolean(TUB,c,'DIFFERENCE',"thin wall %s"%tag); bpy.data.objects.remove(c,do_unlink=True)
        zi=_skin_min(cx,cy,w,l)-thin_wall            # flat inner plane
        f=_box("_FL"+tag,cx-w/2,cx+w/2,cy-l/2,cy+l/2,zi,60.0)
        boolean(f,BODY,'INTERSECT',"flat%s^body"%tag,allow_noop=True)
        boolean(TUB,f,'UNION',"level inner %s"%tag,allow_noop=True)
        bpy.data.objects.remove(f,do_unlink=True)
        return {"tag":tag,"wall_min":thin_wall,"flat_inner_z":round(zi,2)}

    pinfo=[panel("TR",0.0,trellis_y,58.0,58.0),
           panel("OL",0.0,oled_y,62.0,50.0),   # must stay inboard of the
                                                # chambers inner wall at x=31.5
           panel("RW",0.0,row_y,62.0,30.0)]   # must span the joystick body (x 7..31)

    cuts=[]
    for gx in (-22.5,-7.5,7.5,22.5):
        for gy in (trellis_y-22.5,trellis_y-7.5,trellis_y+7.5,trellis_y+22.5):
            cuts.append(_box("_t%.0f_%.0f"%(gx,gy),gx-6.25,gx+6.25,gy-6.25,gy+6.25,5.0,60.0))
    cuts.append(_box("_oled",-31.5,31.5,oled_y-20.5,oled_y+20.5,5.0,60.0))
    cuts.append(_cyl("_joy",joy_d,5.0,60.0,joy_x,row_y))
    cuts.append(_stadium("_lidar",10.0,7.0,5.0,60.0,0.0,row_y))
    cuts.append(_cyl("_enc",enc_d,5.0,60.0,enc_x,row_y))
    cuts.append(_cyl("_mic",mic_d,5.0,60.0,mic_x,mic_y))
    cuts.append(_box("_sw",-sw_w/2,sw_w/2,sw_y-sw_h/2,sw_y+sw_h/2,5.0,60.0))
    if aux_switches:
        for nm,(ax,ay) in (("chg",chg_xy),("lift",lift_xy)):
            cuts.append(_box("_"+nm,ax-aux_w/2,ax+aux_w/2,ay-aux_h/2,ay+aux_h/2,5.0,60.0))
    # ---- end-face ports: thinned from INSIDE, outside stays flush ----
    # Cutters MUST be bounded to the end wall. A full-length box bores through
    # whatever shares its x/z further in (it ate the north anchors once).
    def _inner_y(x,z,sgn):
        sb=bmesh.new(); sb.from_mesh(bpy.data.objects["V2_SHELL_v01"].data)
        tv=_BVH.FromBMesh(sb); sb.free()
        h=tv.ray_cast(_V((x,0.0,z)),_V((0.0,sgn,0.0)),300.0)
        return (sgn*h[3]) if h[0] is not None else None
    port_info={}
    for tag,jx in (("A",-jack_x),("B",jack_x)):
        y0=_inner_y(jx,0.0,1.0)-6.0                 # start just inside the end wall
        t=_box("_JT"+tag, jx-jack_w/2-3.0, jx+jack_w/2+3.0, y0, 200.0,
               -jack_h/2-3.0, jack_h/2+3.0)
        boolean(t,CAVJ,'INTERSECT',"jack thin %s ^cav"%tag,allow_noop=True)
        boolean(TUB,t,'DIFFERENCE',"jack thin "+tag); bpy.data.objects.remove(t,do_unlink=True)
        h=_box("_JK"+tag, jx-jack_w/2, jx+jack_w/2, y0, 200.0, -jack_h/2, jack_h/2)
        boolean(TUB,h,'DIFFERENCE',"jack "+tag); bpy.data.objects.remove(h,do_unlink=True)
        port_info["jack_"+tag]={"x":jx,"size":[jack_w,jack_h],"wall":jack_leave,
                                "cut_from_y":round(y0,2)}
    uy=_inner_y(0.0,0.0,-1.0)+6.0
    ut=_box("_UT", -usb_w/2-3.5, usb_w/2+3.5, -200.0, uy, -usb_h/2-3.0, usb_h/2+3.0)
    boolean(ut,CAVU,'INTERSECT',"usb thin ^cav",allow_noop=True)
    boolean(TUB,ut,'DIFFERENCE',"usb thin"); bpy.data.objects.remove(ut,do_unlink=True)
    uh=_stadium_y("_UH", usb_w, usb_h, -200.0, uy, 0.0, 0.0)
    boolean(TUB,uh,'DIFFERENCE',"usb"); bpy.data.objects.remove(uh,do_unlink=True)
    port_info["usbc"]={"shape":"stadium","size":[usb_w,usb_h],"wall":usb_leave,"cut_from_y":round(uy,2)}

    for c in cuts:
        boolean(TUB,c,'DIFFERENCE',"cut "+c.name); bpy.data.objects.remove(c,do_unlink=True)

    KO = bpy.data.objects.get("LIP_KEEPOUT")
    if KO is not None:
        boolean(TUB, KO, 'DIFFERENCE', "relieve tub for lip", allow_noop=True)
        bpy.data.objects.remove(KO, do_unlink=True)
    for o in (SH15,CAV15,SH25,CAV25,SHJ,CAVJ,SHU,CAVU,SHTH,CAVTH):
        bpy.data.objects.remove(o,do_unlink=True)
    finalize_shading(TUB,shade_angle); finalize_shading(PLA,shade_angle)
    for o in bpy.data.objects:
        if o.type=='MESH' and not o.name.startswith("_"): show(o, True)
    return {"TUB":stats(TUB),"PLATE":stats(PLA),"panels":pinfo,"ports":port_info,
            "row":{"encoder":[enc_x,row_y],"lidar":[0.0,row_y],"joystick":[joy_x,row_y]},
            "joystick_hole_d":joy_d}
