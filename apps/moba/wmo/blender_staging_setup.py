# Twisted Treeline -> WBS WMO staging setup.
# Run inside Blender 3.4 (Text Editor -> Open -> Run Script) on tt_staging_34.blend.
# Idempotent: safe to re-run. Prints PASS/FAIL at the end. Save the .blend afterwards.

import bpy, re, bmesh

ROOT_NAME  = "TwistedTreeline"
DIR_PATH   = "World\\wmo\\TwistedTreeline\\"
SET_NAME   = "Set_$DefaultGlobal"
DOODAD_OBJ = "TT_TestDoodad_Lamppost"
DOODAD_M2  = "World\\EXPANSION01\\DOODADS\\GHOSTLANDS\\Lampposts\\BE_Lamppost_Ghostlands01.m2"
DOODAD_LOC = (0.0, 0.0, 0.0)

COLLIDE = {
    "TT_Ground", "TT_Walls",
    "TT_FrontWall_E", "TT_FrontWall_W", "TT_FrontWall_EPocket", "TT_FrontWall_WPocket",
    "TT_FWFill_EN", "TT_FWFill_ES", "TT_FWFill_WN", "TT_FWFill_WS",
    "TT_Patch_EN", "TT_Patch_ES", "TT_Patch_WN", "TT_Patch_WS",
    "TT_Blue_NexusPlateau", "TT_Red_NexusPlateau",
} | {"TT_JWall_%02d" % i for i in range(16)}

RENDER_ONLY = {
    "TT_Forest", "TT_Pad_Jungle", "TT_Pad_Lane0", "TT_Pad_Lane1",
    "TT_AltarEast_Pad", "TT_AltarWest_Pad", "TT_Blue_GY_Pad", "TT_Red_GY_Pad", "TT_Pit_Pad",
    "TT_Camp_GolemsE", "TT_Camp_GolemsW", "TT_Camp_WolvesE", "TT_Camp_WolvesW",
    "TT_Camp_WraithsE", "TT_Camp_WraithsW",
}

TEXTURES = {
    "TT_Wall":       "tileset\\expansion01\\ghostlands\\ghostlandsrock01.blp",
    "TT_Ground":     "tileset\\expansion01\\ghostlands\\ghostlandsgrass01.blp",
    "TT_Lane":       "tileset\\expansion01\\ghostlands\\ghostlandspath01.blp",
    "TT_JunglePath": "tileset\\expansion01\\ghostlands\\ghostlandsdirt01.blp",
    "TT_Camp":       "tileset\\expansion01\\ghostlands\\ghostlandscreep01.blp",
    "TT_Stone":      "tileset\\duskwood\\duskwoodcobblestone.blp",
    "TT_Pine":       "world\\azeroth\\duskwood\\passivedoodads\\trees\\dusktallcanopy_new03.blp",
    "TT_PitBoss":    "tileset\\plaguelands\\plaguedearthred01.blp",
}

SHIPPING = COLLIDE | RENDER_ONLY
errors, notes = [], []
def base(name):        # strip Blender's .001 copy suffix
    return re.sub(r"\.\d{3}$", "", name)

scene = bpy.context.scene

# ---------------------------------------------------------------- 0. sanity
present = {o.name for o in bpy.data.objects if o.type == 'MESH' and o.name.startswith("TT_")}
present.discard(DOODAD_OBJ)
if present != SHIPPING:
    errors.append("object set mismatch; missing=%s extra=%s"
                  % (sorted(SHIPPING - present), sorted(present - SHIPPING)))
    raise SystemExit("ABORT: " + errors[-1])

# ---------------------------------------------------------------- 1. scene
scene.wow_scene.type = 'WMO'
scene.wow_scene.version = '2'                      # WotLK

# ---------------------------------------------------------------- 2. root collection
root = bpy.data.collections.get(ROOT_NAME)
if root is None:
    # adopt the collection that currently holds the geometry
    holder = next((c for c in scene.collection.children
                   if any(o.name in SHIPPING for o in c.objects)), None)
    if holder is None:
        raise SystemExit("ABORT: no scene-level collection holds the TT objects")
    holder.name = ROOT_NAME
    root = holder
if root.name not in scene.collection.children:
    errors.append("%s is not a direct child of the Scene Collection" % ROOT_NAME)
root.wow_wmo.enabled  = True
root.wow_wmo.dir_path = DIR_PATH

def child(parent, name):
    c = parent.children.get(name)
    if c is None:
        for k in parent.children:                  # tolerate a .001 suffix
            if base(k.name) == name:
                return k
        c = bpy.data.collections.new(name)
        parent.children.link(c)
    return c

outdoor  = child(root, "Outdoor")
doodads  = child(root, "Doodads")
doodadset = child(doodads, SET_NAME)

# ---------------------------------------------------------------- 3. groups
for name in sorted(SHIPPING):
    ob = bpy.data.objects[name]
    if ob.name not in outdoor.objects:
        outdoor.objects.link(ob)
    for c in list(ob.users_collection):
        if c is not outdoor:
            c.objects.unlink(ob)
    ob.hide_viewport = False
    ob.hide_render   = False
    try:
        ob.hide_set(False)                         # build_references skips hidden objects
    except RuntimeError:
        notes.append("%s not in the view layer; could not unhide" % name)
    ob.wow_wmo_group.export_order = 0

# ---------------------------------------------------------------- 4. collision vertex groups
for name in sorted(SHIPPING):
    ob = bpy.data.objects[name]
    me = ob.data
    if name in COLLIDE:
        vg = ob.vertex_groups.get("Collision") or ob.vertex_groups.new(name="Collision")
        vg.add(list(range(len(me.vertices))), 1.0, 'REPLACE')
        ob.wow_wmo_vertex_info.vertex_group = "Collision"
    else:
        vg = ob.vertex_groups.get("Collision")
        if vg:
            ob.vertex_groups.remove(vg)
        ob.wow_wmo_vertex_info.vertex_group = ""
    ob.wow_wmo_vertex_info.node_size = 0           # dynamic BSP node size

# ---------------------------------------------------------------- 5. materials + textures
used = {}
for name in SHIPPING:
    for m in bpy.data.objects[name].data.materials:
        if m:
            used.setdefault(m.name, m)
for mat in used.values():
    b = base(mat.name)
    path = TEXTURES.get(b)
    if path is None:
        errors.append("material %s has no texture mapping" % mat.name)
        continue
    img = bpy.data.images.get(b) or bpy.data.images.new(b, 1, 1)
    img.wow_wmo_texture.path = path
    mat.wow_wmo_material.diff_texture_1 = img
    mat.wow_wmo_material.shader         = '0'      # Diffuse
    mat.wow_wmo_material.blending_mode  = '0'      # Opaque
    mat.wow_wmo_material.terrain_type   = '0'

# ---------------------------------------------------------------- 6. test doodad
ob = bpy.data.objects.get(DOODAD_OBJ)
if ob is None:
    me = bpy.data.meshes.new(DOODAD_OBJ)
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:                              # 1 x 1 x 3.5, sitting on z = 0
        v.co.z = (v.co.z + 0.5) * 3.5
    bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new(DOODAD_OBJ, me)
for c in list(ob.users_collection):
    c.objects.unlink(ob)
doodadset.objects.link(ob)
ob.wow_wmo_doodad.enabled = True                    # WBS moves non-matching objects out
ob.wow_wmo_doodad.path    = DOODAD_M2
ob.location      = DOODAD_LOC
ob.rotation_mode = 'QUATERNION'
ob.rotation_quaternion = (1.0, 0.0, 0.0, 0.0)
ob.scale = (1.0, 1.0, 1.0)
ob.hide_viewport = ob.hide_render = False

# ---------------------------------------------------------------- 7. active collection
bpy.context.view_layer.update()
lc = bpy.context.view_layer.layer_collection.children.get(root.name)
if lc:
    bpy.context.view_layer.active_layer_collection = lc
else:
    errors.append("could not make %s the active collection" % root.name)

# ---------------------------------------------------------------- 8. report
print("\n" + "=" * 72)
print("root %r enabled=%s dir_path=%r" % (root.name, root.wow_wmo.enabled, root.wow_wmo.dir_path))
print("scene type=%s version=%s" % (scene.wow_scene.type, scene.wow_scene.version))
print("Outdoor holds %d objects (expected 47)" % len(outdoor.objects))
print("%s holds %d objects" % (SET_NAME, len(doodadset.objects)))

n_col = n_ren = 0
for name in sorted(SHIPPING):
    o  = bpy.data.objects[name]
    me = o.data
    vgn = o.wow_wmo_vertex_info.vertex_group
    g   = o.vertex_groups.get(vgn) if vgn else None
    cov = sum(1 for v in me.vertices if g and any(e.group == g.index for e in v.groups))
    want = name in COLLIDE
    ok   = (cov == len(me.vertices)) if want else (cov == 0 and not vgn)
    if not ok:
        errors.append("%s: collide=%s vg=%r coverage %d/%d" % (name, want, vgn, cov, len(me.vertices)))
    n_col += want; n_ren += not want
    if 'UVMap' not in me.uv_layers:
        errors.append("%s: no UV layer named 'UVMap' (export raises)" % name)
    if not me.materials or me.materials[0] is None:
        errors.append("%s: empty material slot" % name)
    if o.name not in outdoor.objects:
        errors.append("%s: not in Outdoor" % name)

print("collide=%d (expect 32)  render-only=%d (expect 15)" % (n_col, n_ren))
print("materials:")
for mat in sorted(used.values(), key=lambda m: m.name):
    t = mat.wow_wmo_material.diff_texture_1
    print("   %-18s -> %s" % (mat.name, t.wow_wmo_texture.path if t else "NONE"))
d = bpy.data.objects[DOODAD_OBJ]
print("doodad %r enabled=%s loc=%s quat=%s scale=%.3f\n   %s"
      % (d.name, d.wow_wmo_doodad.enabled, tuple(round(v, 2) for v in d.location),
         tuple(round(v, 3) for v in d.rotation_quaternion), d.scale[0], d.wow_wmo_doodad.path))

for n in notes:
    print("NOTE : " + n)
print("=" * 72)
if errors:
    print("FAIL (%d)" % len(errors))
    for e in errors:
        print("  - " + e)
else:
    print("PASS - save the .blend, then File > Export > WMO (.wmo)")
print("=" * 72 + "\n")
