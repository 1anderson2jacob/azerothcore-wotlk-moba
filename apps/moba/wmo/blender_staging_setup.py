# Twisted Treeline -> WBS WMO staging setup.
# Run inside Blender 3.4 (Text Editor -> Open -> Run Script) on tt_staging_34.blend.
# Idempotent: safe to re-run. Prints PASS/FAIL at the end. Save the .blend afterwards.

import bpy, re, bmesh, math
from mathutils import Matrix, Vector

ROOT_NAME   = "TwistedTreeline"
DIR_PATH    = "World\\wmo\\TwistedTreeline\\"
ROOT_WMO_ID = 9000              # MOHD.id; <= 32767 or the WMOAreaTable key aliases
SET_NAME    = "Set_$DefaultGlobal"
DOODAD_OBJ  = "TT_TestDoodad_Lamppost"
DOODAD_M2   = "World\\EXPANSION01\\DOODADS\\GHOSTLANDS\\Lampposts\\BE_Lamppost_Ghostlands01.m2"
DOODAD_SERVER_LOC = (0.0, 70.0, 0.0)    # open floor, 13 yd clear of any island

ORIENT_PROBE   = "TT_JWall_14"
ORIENT_PROBE_Y = 55.46          # its bbox centre y in the blockout; the turn negates it

RENDER_ONLY = {"TT_Trees"}
SINGLETON   = {"TT_OuterWall"}

TEXTURES = {
    "TT_Floor": "tileset\\expansion01\\ghostlands\\ghostlandsgrass01.blp",
    "TT_Wall":  "tileset\\expansion01\\ghostlands\\ghostlandsrock01.blp",
    "TT_Tree":  "tileset\\expansion01\\ghostlands\\ghostlandsrock01.blp",
}

errors, notes = [], []
def base(name):        # strip Blender's .001 copy suffix
    return re.sub(r"\.\d{3}$", "", name)

scene = bpy.context.scene

# ---------------------------------------------------------------- 0. sanity
present = {o.name for o in bpy.data.objects if o.type == 'MESH' and o.name.startswith("TT_")}
present.discard(DOODAD_OBJ)


def series(prefix):
    """The 00..N-1 run of a numbered group, counted from the scene rather than
    declared. Both counts are trace-derived -- islands follow the paint, floor
    chunks follow gen_blockout's batch-ceiling grid -- so a constant here is a
    copy of a number this file cannot see, and it goes stale silently."""
    n = 0
    while "%s%02d" % (prefix, n) in present:
        n += 1
    return {"%s%02d" % (prefix, i) for i in range(n)}


FLOORS   = series("TT_Floor_")
ISLANDS  = series("TT_JWall_")
COLLIDE  = SINGLETON | FLOORS | ISLANDS
SHIPPING = COLLIDE | RENDER_ONLY

if not FLOORS or not ISLANDS:
    raise SystemExit("ABORT: no numbered floor chunks or island walls; is the import empty?")
if present != SHIPPING:
    errors.append("object set mismatch; missing=%s extra=%s"
                  % (sorted(SHIPPING - present), sorted(present - SHIPPING)))
    raise SystemExit("ABORT: " + errors[-1])
if ORIENT_PROBE not in present:
    raise SystemExit("ABORT: orientation probe %s is not in the scene" % ORIENT_PROBE)

# ---------------------------------------------------------------- 0b. server frame
# The engine reads a global WMO back as server = (-model_x, -model_y, model_z), and
# WBS writes Blender coordinates into MOVT verbatim -- so a 180 deg turn here makes
# server coordinates equal the blockout's own. Chain in apps/moba/wmo/README.md.
#
# Probe on Y, not X. The map is a left-right mirror and islands are numbered by
# descending area, so a repaint can rename the probe onto its own mirror twin --
# and an x-mirror of the naming reads exactly like the turn, which would skip the
# rotation and pass. Twins share their y; only the turn flips it. Any other
# renumbering lands on a different y and trips the check below.
def probe_y(name):
    o  = bpy.data.objects[name]
    ys = [(o.matrix_world @ Vector(c)).y for c in o.bound_box]
    return (min(ys) + max(ys)) / 2.0

if probe_y(ORIENT_PROBE) * ORIENT_PROBE_Y > 0.0:        # still in the blockout frame
    R = Matrix.Rotation(math.pi, 4, 'Z')
    for name in sorted(SHIPPING):
        ob = bpy.data.objects[name]
        ob.matrix_world = R @ ob.matrix_world
    bpy.context.view_layer.update()
    notes.append("turned the scene 180 deg about Z into the server frame")
if abs(probe_y(ORIENT_PROBE) + ORIENT_PROBE_Y) > 1.0:
    errors.append("%s centres on y=%+.2f, server frame wants %+.2f"
                  % (ORIENT_PROBE, probe_y(ORIENT_PROBE), -ORIENT_PROBE_Y))

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
root.wow_wmo.wmo_id   = ROOT_WMO_ID

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
# Section 0b turned SHIPPING, not this object, so its location is the model frame
# -- the server frame negated in x and y.
ob.location      = (-DOODAD_SERVER_LOC[0], -DOODAD_SERVER_LOC[1], DOODAD_SERVER_LOC[2])
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
print("root %r enabled=%s wmo_id=%d dir_path=%r"
      % (root.name, root.wow_wmo.enabled, root.wow_wmo.wmo_id, root.wow_wmo.dir_path))
print("orientation: %s centres on y=%+.2f (server frame wants %+.2f)"
      % (ORIENT_PROBE, probe_y(ORIENT_PROBE), -ORIENT_PROBE_Y))
print("scene type=%s version=%s" % (scene.wow_scene.type, scene.wow_scene.version))
print("Outdoor holds %d objects (expected %d)" % (len(outdoor.objects), len(SHIPPING)))
print("derived: %d floor chunks, %d island walls (counted from the scene)"
      % (len(FLOORS), len(ISLANDS)))
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

print("collide=%d (expect %d)  render-only=%d (expect %d)"
      % (n_col, len(COLLIDE), n_ren, len(RENDER_ONLY)))
print("materials:")
for mat in sorted(used.values(), key=lambda m: m.name):
    t = mat.wow_wmo_material.diff_texture_1
    print("   %-18s -> %s" % (mat.name, t.wow_wmo_texture.path if t else "NONE"))
d = bpy.data.objects[DOODAD_OBJ]
print("doodad %r enabled=%s model loc=%s (server %s) quat=%s scale=%.3f\n   %s"
      % (d.name, d.wow_wmo_doodad.enabled, tuple(round(v, 2) for v in d.location),
         DOODAD_SERVER_LOC, tuple(round(v, 3) for v in d.rotation_quaternion),
         d.scale[0], d.wow_wmo_doodad.path))

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
