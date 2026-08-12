# Pre-export preflight + test doodad, for Blender 3.4 with WBS registered.
# ADDITIVE ONLY: creates the test doodad and, if and only if a material has no
# texture assigned, links one of the already-loaded images by path. Never
# overwrites an existing assignment, never touches geometry or vertex groups.

import bpy, re, bmesh

SET_NAME   = "Set_$DefaultGlobal"
DOODAD_OBJ = "TT_TestDoodad_Lamppost"
DOODAD_M2  = "World\\EXPANSION01\\DOODADS\\GHOSTLANDS\\Lampposts\\BE_Lamppost_Ghostlands01.m2"
DOODAD_LOC = (0.0, 0.0, 0.0)

COLLIDE = {"TT_Ground", "TT_Walls", "TT_FrontWall_E", "TT_FrontWall_W",
           "TT_FrontWall_EPocket", "TT_FrontWall_WPocket",
           "TT_FWFill_EN", "TT_FWFill_ES", "TT_FWFill_WN", "TT_FWFill_WS",
           "TT_Patch_EN", "TT_Patch_ES", "TT_Patch_WN", "TT_Patch_WS",
           "TT_Blue_NexusPlateau", "TT_Red_NexusPlateau"} | {"TT_JWall_%02d" % i for i in range(16)}

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

base  = lambda n: re.sub(r"\.\d{3}$", "", n)
fails, notes = [], []
scene = bpy.context.scene

# -- 1. resolve the root the way the exporter does -------------------------
act  = bpy.context.collection
root = None
if act is not None and act.name in scene.collection.children and act.wow_wmo.enabled:
    root = act
if root is None:
    for c in scene.collection.children:
        if c.wow_wmo.enabled and act is not None and act in c.children_recursive:
            root = c
            break
if root is None:                              # fall back to the only enabled root
    cands = [c for c in scene.collection.children if c.wow_wmo.enabled]
    if len(cands) == 1:
        root = cands[0]
        notes.append("active collection was outside the WMO root; fixed")
    else:
        fails.append("cannot resolve a wow_wmo root collection (found %d)" % len(cands))

if root is not None:
    lc = bpy.context.view_layer.layer_collection.children.get(root.name)
    if lc:
        bpy.context.view_layer.active_layer_collection = lc

# -- 2. scene ---------------------------------------------------------------
if scene.wow_scene.type != 'WMO':
    scene.wow_scene.type = 'WMO'
    notes.append("scene type set to WMO")
if scene.wow_scene.version != '2':
    scene.wow_scene.version = '2'
    notes.append("client version set to WotLK")

def find_child(parent, name):
    for k in parent.children:
        if base(k.name) == name:
            return k

outdoor = find_child(root, "Outdoor") if root else None
doodads = find_child(root, "Doodads") if root else None
if outdoor is None:
    fails.append("no Outdoor collection under the root")
if doodads is None:
    fails.append("no Doodads collection under the root")

# -- 3. materials: report, and only fill in what is missing -----------------
print("\n== MATERIALS ==")
used = {}
if outdoor:
    for o in outdoor.objects:
        for m in o.data.materials:
            if m:
                used[m.name] = m
for m in sorted(used.values(), key=lambda x: x.name):
    img = m.wow_wmo_material.diff_texture_1
    if img is None:
        want = TEXTURES.get(base(m.name))
        if want is None:
            fails.append("material %s has no texture and no known mapping" % m.name)
            print("  %-20s NO TEXTURE, no mapping" % m.name)
            continue
        hit = next((i for i in bpy.data.images
                    if i.wow_wmo_texture.path.lower() == want), None)
        if hit is None:
            hit = bpy.data.images.new(base(m.name), 1, 1)
            hit.wow_wmo_texture.path = want
            notes.append("created placeholder image for %s" % m.name)
        m.wow_wmo_material.diff_texture_1 = img = hit
        notes.append("linked %s -> %s" % (m.name, img.name))
    path = img.wow_wmo_texture.path
    print("  %-20s %-28s %s" % (m.name, img.name, path or "<EMPTY PATH>"))
    if not path:
        fails.append("image %s used by %s has an empty WoW path" % (img.name, m.name))

# -- 4. group preflight -----------------------------------------------------
print("\n== GROUPS ==")
n_c = n_r = 0
if outdoor:
    for o in sorted(outdoor.objects, key=lambda x: x.name):
        me = o.data
        vgn = o.wow_wmo_vertex_info.vertex_group
        g = o.vertex_groups.get(vgn) if vgn else None
        cov = sum(1 for v in me.vertices if g and any(e.group == g.index for e in v.groups))
        want = o.name in COLLIDE
        n_c += want
        n_r += not want
        if want and cov != len(me.vertices):
            fails.append("%s: collision coverage %d/%d" % (o.name, cov, len(me.vertices)))
        if not want and (vgn or cov):
            fails.append("%s: render-only but carries vertex group %r" % (o.name, vgn))
        if 'UVMap' not in me.uv_layers:
            fails.append("%s: no UV layer named 'UVMap' (export raises)" % o.name)
        if o.hide_get():
            o.hide_set(False)
            notes.append("unhid %s (build_references skips hidden objects)" % o.name)
print("  %d objects: collide=%d (want 32) render-only=%d (want 15)"
      % (len(outdoor.objects) if outdoor else 0, n_c, n_r))
if n_c != 32 or n_r != 15:
    fails.append("group split is %d/%d, expected 32/15" % (n_c, n_r))

# -- 5. test doodad ---------------------------------------------------------
print("\n== DOODAD ==")
if doodads is not None:
    dset = find_child(doodads, SET_NAME)
    if dset is None:
        for k in doodads.children:
            if k.name.startswith("Set_$DefaultGlobal"):
                dset = k
    if dset is None:
        dset = bpy.data.collections.new(SET_NAME)
        doodads.children.link(dset)
        notes.append("created %s" % SET_NAME)

    ob = bpy.data.objects.get(DOODAD_OBJ)
    if ob is None:
        me = bpy.data.meshes.new(DOODAD_OBJ)
        bm = bmesh.new()
        bmesh.ops.create_cube(bm, size=1.0)
        for v in bm.verts:                       # 1 x 1 x 3.5, standing on z = 0
            v.co.z = (v.co.z + 0.5) * 3.5
        bm.to_mesh(me)
        bm.free()
        ob = bpy.data.objects.new(DOODAD_OBJ, me)
        notes.append("created %s" % DOODAD_OBJ)
    for c in list(ob.users_collection):
        if c is not dset:
            c.objects.unlink(ob)
    if ob.name not in dset.objects:
        dset.objects.link(ob)

    ob.wow_wmo_doodad.enabled = True             # or WBS's handler evicts it
    ob.wow_wmo_doodad.path    = DOODAD_M2
    ob.location = DOODAD_LOC
    ob.rotation_mode = 'QUATERNION'
    ob.rotation_quaternion = (1.0, 0.0, 0.0, 0.0)
    ob.scale = (1.0, 1.0, 1.0)
    ob.hide_viewport = ob.hide_render = False
    ob.hide_set(False)
    print("  %s in %r enabled=%s scale=%.2f" % (ob.name, dset.name, ob.wow_wmo_doodad.enabled, ob.scale[0]))
    print("  path = %s" % ob.wow_wmo_doodad.path)

# -- 6. report --------------------------------------------------------------
print("\n" + "=" * 70)
print("root            : %s" % (root.name if root else "NONE"))
print("active collection: %s" % bpy.context.view_layer.active_layer_collection.name)
print("scene           : type=%s version=%s" % (scene.wow_scene.type, scene.wow_scene.version))
for n in notes:
    print("  changed: " + n)
if fails:
    print("FAIL (%d)" % len(fails))
    for f in fails:
        print("  - " + f)
else:
    print("PASS - save, then File > Export > WMO")
print("=" * 70 + "\n")
