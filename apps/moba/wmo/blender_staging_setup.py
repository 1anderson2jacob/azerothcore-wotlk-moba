# Twisted Treeline -> WBS WMO staging setup.
# Run inside Blender 3.4 (Text Editor -> Open -> Run Script) on tt_staging_34.blend.
# Idempotent: safe to re-run. Prints PASS/FAIL at the end. Save the .blend afterwards.

import bpy, re, bmesh, math, os, json, sys
from mathutils import Matrix, Quaternion, Vector

ROOT_NAME   = "TwistedTreeline"
DIR_PATH    = "World\\wmo\\TwistedTreeline\\"
ROOT_WMO_ID = 9000              # MOHD.id; <= 32767 or the WMOAreaTable key aliases
SET_NAME    = "Set_$DefaultGlobal"
DOODAD_PREFIX = "Doodad_"       # NOT TT_: that prefix is the shipping-object scan

ORIENT_PROBE   = "TT_JWall_14"
ORIENT_PROBE_Y = 55.46          # its bbox centre y in the blockout; the turn negates it

RENDER_ONLY = set()
SINGLETON   = {"TT_OuterWall"}

# Written by build_blockout.py beside the blockout .blend. Loaded rather than
# declared for the same reason series() counts from the scene: the material set
# follows map_source.yaml, so a copy here goes stale silently.
MATERIALS_JSON = os.path.expanduser(
    "~/code/azerothcore-wotlk/var/blender/twisted_treeline_v2_materials.json")
DOODADS_JSON = MATERIALS_JSON.replace("_materials.json", "_doodads.json")

# Real client textures in the 3.4 viewport. PREVIEW ONLY -- the exporter reads
# wow_wmo_material.diff_texture_1 for its path string (wmo_scene.py:539) and
# never the pixels or the node tree, so every failure here degrades to the 1x1
# placeholder and is reported instead of stopping the setup.
TEXCACHE     = os.path.join(os.path.dirname(MATERIALS_JSON), "texcache")
MPQ_TOOLS    = os.path.abspath(os.path.join(os.path.dirname(MATERIALS_JSON),
                                            "../../apps/moba/wmo"))
WBS_ROOT     = os.path.expanduser("~/tools/blender-wow-studio/io_scene_wmo")
WBS_BLP      = WBS_ROOT + "/pywowlib/blp/BLP2PNG"
WBS_3P       = WBS_ROOT + "/third_party"   # pywowlib's m2 parser imports bidict
M2CACHE      = os.path.join(os.path.dirname(MATERIALS_JSON), "m2cache")
PREVIEW_NODE = "TT_Preview"

try:
    with open(MATERIALS_JSON) as fh:
        TEXTURES = json.load(fh)
    with open(DOODADS_JSON) as fh:
        DOODADS = json.load(fh)
except OSError as exc:
    raise SystemExit("ABORT: cannot read %s (%s) -- run gen_blockout.py first"
                     % (exc.filename, exc))

errors, notes = [], []
def base(name):        # strip Blender's .001 copy suffix
    return re.sub(r"\.\d{3}$", "", name)

scene = bpy.context.scene

# ---------------------------------------------------------------- 0. sanity
present = {o.name for o in bpy.data.objects if o.type == 'MESH' and o.name.startswith("TT_")}
# Anything already sitting in a doodad set is dressing, not a shipping group,
# whatever it happens to be called.
present -= {o.name for c in bpy.data.collections if c.name.startswith("Set_")
            for o in c.objects}


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
_MPQ = []                       # [(storm, handles)] -- opened on the first cache miss


def cache_name(path):
    """A texture path as a flat filename.

    BlpConvert writes basePath + separator + the name it is handed and creates
    no directories on this code path, so the DUNGEONS\\TEXTURES\\ tree has to
    collapse into the name. Keyed on the TEXTURE, never the material: swapping a
    candidate is the whole preview loop, and a material-keyed cache hands back
    the candidate before it."""
    return re.sub(r"[\\/]", "_", path)


def blp_png(path):
    """Decode one client BLP into TEXCACHE and return the PNG's path."""
    png = os.path.join(TEXCACHE, os.path.splitext(cache_name(path))[0] + ".png")
    if os.path.isfile(png):
        return png
    for p in (MPQ_TOOLS, WBS_BLP):
        if p not in sys.path:
            sys.path.insert(0, p)
    import mpq_tool
    from BLP2PNG import BlpConverter
    if not _MPQ:
        _MPQ.append(mpq_tool.open_archives())
    storm, handles = _MPQ[0]
    data = mpq_tool.read(storm, handles, path)
    if not data:
        raise IOError("no archive holds it")
    os.makedirs(TEXCACHE, exist_ok=True)
    BlpConverter().convert([(data, cache_name(path).encode())], TEXCACHE.encode())
    if not os.path.isfile(png):
        raise IOError("BLP2PNG wrote nothing")
    return png


def show_texture(mat, img):
    """Wire the image in so the viewport actually draws it.

    Our own node, not WBS's: update_diff_texture_1 populates a node named
    DiffuseTexture1 that nothing outside WBS's material panel creates, so
    assigning the pointer alone renders nothing. mat.diffuse_color is left
    alone, so the per-material hue Solid shading uses to catch a wall wearing
    the wrong candidate still works."""
    mat.use_nodes = True
    nt = mat.node_tree
    tex = nt.nodes.get(PREVIEW_NODE)
    if tex is None:
        tex = nt.nodes.new("ShaderNodeTexImage")
        tex.name = tex.label = PREVIEW_NODE
        tex.location = (-360, 300)
    tex.image = img
    bsdf = next((n for n in nt.nodes if n.type == "BSDF_PRINCIPLED"), None)
    if bsdf is not None and not bsdf.inputs["Base Color"].is_linked:
        nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])


used = {}
for name in SHIPPING:
    for m in bpy.data.objects[name].data.materials:
        if m:
            used.setdefault(m.name, m)
unpreviewed = []
for mat in used.values():
    b = base(mat.name)
    path = TEXTURES.get(b)
    if path is None:
        errors.append("material %s has no texture mapping in %s"
                      % (mat.name, os.path.basename(MATERIALS_JSON)))
        continue
    if path != path.lower():
        errors.append("material %s texture %r is not lowercase" % (mat.name, path))
        continue
    img = bpy.data.images.get(b)
    try:
        png = blp_png(path)
    # SystemExit too: mpq_tool.open_archives calls sys.exit when storm will not
    # import, and that is a BaseException -- a bare `except Exception` lets a
    # missing binding kill the whole setup over a preview.
    except (Exception, SystemExit) as exc:
        unpreviewed.append("%s -- %s" % (b, exc))
        png = None
    # Reload whenever what is loaded is not THIS texture's png. Repointing a
    # material at another candidate is the whole preview loop, and keeping the
    # image just because one is present leaves the previous art on the wall
    # while the exported path says otherwise.
    if png is not None and (img is None or os.path.realpath(
            bpy.path.abspath(img.filepath)) != os.path.realpath(png)):
        if img is not None:
            bpy.data.images.remove(img)
        img = bpy.data.images.load(png, check_existing=False)
        img.name = b
    if img is None:
        img = bpy.data.images.new(b, 1, 1)
    if tuple(img.size) != (1, 1):
        show_texture(mat, img)
    img.wow_wmo_texture.path = path
    mat.wow_wmo_material.diff_texture_1 = img
    mat.wow_wmo_material.shader         = '0'      # Diffuse
    mat.wow_wmo_material.blending_mode  = '0'      # Opaque
    mat.wow_wmo_material.terrain_type   = '0'

# ---------------------------------------------------------------- 6. doodads
# Rebuilt from scratch every run, including anything a previous revision left in
# the set -- the retired TT_TestDoodad_Lamppost among it.
stale = {o.name: o for o in list(doodadset.objects)}
stale.update({o.name: o for o in bpy.data.objects
              if o.name.startswith(DOODAD_PREFIX)})
for ob in stale.values():
    bpy.data.objects.remove(ob, do_unlink=True)
for me in [m for m in bpy.data.meshes
           if m.name.startswith(DOODAD_PREFIX) and not m.users]:
    bpy.data.meshes.remove(me)
# Materials are per-mesh and never shared (see m2_mesh), so a dropped mesh
# always orphans its own -- purge or every run leaks one set per model.
for mat in [m for m in bpy.data.materials
            if m.name.startswith(DOODAD_PREFIX) and not m.users]:
    bpy.data.materials.remove(mat)
unmodelled = []


def proxy_mesh(key, box):
    """One box mesh per model, at the M2's own footprint and z range in model
    units -- so the object's export scale draws it in the viewport at the size it
    will ship at, and spacing is judgeable before a pack. Spans z rather than
    rising from it because a hanging model's geometry is entirely BELOW its
    origin, and a box drawn upward from there shows the wrong half of the wall.

    Carries NO material on purpose: WoWWMODoodad.on_each_update copies the
    material of any doodad whose material has other users, so one shared
    material would fork into one copy per tree on every depsgraph tick."""
    name = DOODAD_PREFIX + key
    me = bpy.data.meshes.get(name)
    if me is not None:
        return me
    dx, dy = (max(float(v), 0.1) for v in box[:2])
    z0, z1 = float(box[2]), float(box[3])
    z1 = max(z1, z0 + 0.1)
    me = bpy.data.meshes.new(name)
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co.x *= dx
        v.co.y *= dy
        v.co.z = z0 + (v.co.z + 0.5) * (z1 - z0)
    bm.to_mesh(me)
    bm.free()
    return me


def image_for(path):
    """One image datablock per texture, shared across materials freely -- the
    doodad handler forks MATERIALS, never images."""
    png = blp_png(path)
    name = os.path.basename(png)
    img = bpy.data.images.get(name)
    if img is not None and os.path.realpath(
            bpy.path.abspath(img.filepath)) == os.path.realpath(png):
        return img
    if img is not None:
        bpy.data.images.remove(img)
    img = bpy.data.images.load(png, check_existing=False)
    img.name = name
    return img


def m2_mesh(name, path):
    """The model's real geometry, one material per submesh texture.

    Walked as submesh index range -> triangle_indices -> vertex_indices ->
    vertices. Verified against an independent reader: the box this reconstructs
    for duskwoodspookytree01 matches mpq_tool's own MD20 header read on all six
    bounds.

    Every material here is NEW and belongs to this mesh alone.
    WoWWMODoodad.on_each_update copies each material slot of any doodad whose
    active material has users > 1, on every depsgraph update -- and a material
    is owned by the MESH, so N objects sharing one mesh hold users at 1 while
    two meshes sharing one material fork forever. Never key these by texture."""
    for p in (MPQ_TOOLS, WBS_ROOT, WBS_3P):
        if p not in sys.path:
            sys.path.insert(0, p)
    import mpq_tool
    from pywowlib.m2_file import M2File
    if not _MPQ:
        _MPQ.append(mpq_tool.open_archives())
    storm, handles = _MPQ[0]
    os.makedirs(M2CACHE, exist_ok=True)
    stem = os.path.join(M2CACHE, os.path.splitext(cache_name(path))[0])
    # pywowlib reads from disk and derives skin paths as <stem>NN.skin, so every
    # file has to land beside the .m2 under one basename.
    def grab(src, dst):
        if os.path.isfile(dst):
            return
        data = mpq_tool.read(storm, handles, src)
        if not data:
            raise IOError("no archive holds %s" % os.path.basename(src))
        with open(dst, "wb") as fh:
            fh.write(data)

    grab(path, stem + ".m2")
    m2 = M2File(2, stem + ".m2")            # 2 -> WOTLK via from_expansion_number
    # read_additional_files insists on ALL num_skin_profiles being present, LOD
    # profiles included -- the two Lunar New Year lanterns declare 2 and boxed
    # themselves when only 00 was extracted. The count is knowable only after the
    # root is parsed, which is why this runs after the M2File call.
    for i in range(m2.root.num_skin_profiles):
        grab("%s%02d.skin" % (path[:-3], i), "%s%02d.skin" % (stem, i))
    m2.read_additional_files(m2.find_model_dependencies().skins, [])
    skin = m2.skins[0]

    lut = list(m2.root.texture_lookup_table)
    tex_of = {tu.skin_section_index: lut[tu.texture_combo_index]
              for tu in skin.texture_units if tu.texture_combo_index < len(lut)}
    tris, face_slot, slots = [], [], {}
    for i, sm in enumerate(skin.submeshes):
        tex_id = tex_of.get(i, -1)
        slots.setdefault(tex_id, len(slots))
        for k in range(sm.index_start, sm.index_start + sm.index_count, 3):
            tris.append(tuple(skin.vertex_indices[skin.triangle_indices[k + o]]
                              for o in range(3)))
            face_slot.append(slots[tex_id])
    if not tris:
        raise IOError("no triangles")

    me = bpy.data.meshes.new(name)
    me.from_pydata([tuple(v.pos) for v in m2.root.vertices], [], tris)
    for tex_id, slot in sorted(slots.items(), key=lambda kv: kv[1]):
        mat = bpy.data.materials.new("%s_%02d" % (name, slot))
        me.materials.append(mat)
        if tex_id < 0:
            continue
        src = m2.root.textures[tex_id].filename.value.lower().replace("/", "\\")
        try:
            show_texture(mat, image_for(src))
        except (Exception, SystemExit):
            continue                    # untextured slot; the geometry still shows
        nt = mat.node_tree
        bsdf = next((n for n in nt.nodes if n.type == "BSDF_PRINCIPLED"), None)
        if bsdf is not None:
            nt.links.new(nt.nodes[PREVIEW_NODE].outputs["Alpha"],
                         bsdf.inputs["Alpha"])
            # Canopy and web cards are alpha-tested; drawn opaque they read as
            # billboards, which is worse than the box they replace.
            mat.blend_method = mat.shadow_method = 'CLIP'
    # Material indices go on BEFORE validate, which makes a desync impossible
    # rather than merely detected. These models carry the same triangle twice to
    # fake double-siding -- 16 such faces on the horde banner, 57 on
    # zuldrak_largetree_01 -- and validate drops the copy; assigned afterwards, a
    # survivor would slide onto the next submesh's texture.
    for poly, slot in zip(me.polygons, face_slot):
        poly.material_index = slot
    me.validate()
    uv = me.uv_layers.new(name="UVMap")
    for loop in me.loops:
        # WoW's V runs down from the top-left, Blender's up from the bottom-left.
        u, v = m2.root.vertices[loop.vertex_index].tex_coords
        uv.data[loop.index].uv = (u, 1.0 - v)
    return me


def model_mesh(key, spec):
    """Real geometry when the model can be read, the box proxy when it cannot."""
    name = DOODAD_PREFIX + key
    me = bpy.data.meshes.get(name)
    if me is not None:
        return me
    try:
        return m2_mesh(name, spec["path"])
    except (Exception, SystemExit) as exc:
        unmodelled.append("%s -- %s" % (key, exc))
        # m2_mesh may have died AFTER creating the mesh, and proxy_mesh returns
        # any mesh already under this name -- so the half-built one has to go.
        partial = bpy.data.meshes.get(name)
        if partial is not None:
            bpy.data.meshes.remove(partial)
        return proxy_mesh(key, spec["box"])

for i, d in enumerate(DOODADS["placements"]):
    spec = DOODADS["models"][d["model"]]
    ob = bpy.data.objects.new("%s%04d" % (DOODAD_PREFIX, i),
                              model_mesh(d["model"], spec))
    doodadset.objects.link(ob)
    ob.wow_wmo_doodad.enabled = True             # WBS moves non-matching objects out
    ob.wow_wmo_doodad.path    = spec["path"]
    # Section 0b turned SHIPPING and not these, so a placement is the server
    # frame turned 180 deg about Z: x and y negate AND the yaw gains 180. The
    # old single test doodad hid the second half by being rotationally symmetric.
    x, y, z = d["pos"]
    ob.location = (-x, -y, z)
    ob.rotation_mode = 'QUATERNION'
    # Yaw puts the model's local +X along the wall's face normal, which is the
    # plane a hanging card is authored in, so the card lands flat on the wall.
    ob.rotation_quaternion = Quaternion((0.0, 0.0, 1.0),
                                        math.radians(d["yaw_deg"] + 180.0))
    ob.scale = (d["scale"],) * 3
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
    print("   %-18s %-9s -> %s"
          % (mat.name, "%dx%d" % tuple(t.size) if t else "-",
             t.wow_wmo_texture.path if t else "NONE"))
if unpreviewed:
    print("not previewed (%d) -- 1x1 placeholder, export unaffected:" % len(unpreviewed))
    for u in sorted(unpreviewed):
        print("   " + u)
counts = {}
for ob in doodadset.objects:
    counts[ob.wow_wmo_doodad.path] = counts.get(ob.wow_wmo_doodad.path, 0) + 1
print("doodads: %d placements, %d distinct models" % (len(doodadset.objects), len(counts)))
for p, n in sorted(counts.items()):
    print("   %4d  %s" % (n, p))

if unmodelled:
    print("box proxies (%d) -- geometry unreadable, placement unaffected:"
          % len(unmodelled))
    for u in sorted(unmodelled):
        print("   " + u)

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
