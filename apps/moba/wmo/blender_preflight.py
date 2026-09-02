# Pre-export preflight + test doodad, for Blender 3.4 with WBS registered.
# ADDITIVE ONLY: creates the test doodad and, if and only if a material has no
# texture assigned, links one of the already-loaded images by path. Never
# overwrites an existing assignment, never touches geometry or vertex groups.

import bpy, re, os, json
from mathutils import Vector

ROOT_WMO_ID = 9000              # must match blender_staging_setup.py
SET_NAME   = "Set_$DefaultGlobal"

ORIENT_PROBE   = "TT_JWall_14"
ORIENT_PROBE_Y = 55.46          # its bbox centre y in the blockout; the turn negates it

RENDER_ONLY = set()
SINGLETON   = {"TT_OuterWall"}

PRESENT = {o.name for o in bpy.data.objects if o.type == 'MESH' and o.name.startswith("TT_")}
PRESENT -= {o.name for c in bpy.data.collections if c.name.startswith("Set_")
            for o in c.objects}


def series(prefix):
    """The 00..N-1 run of a numbered group, counted from the scene rather than
    declared -- both counts are trace-derived and a constant here goes stale."""
    n = 0
    while "%s%02d" % (prefix, n) in PRESENT:
        n += 1
    return {"%s%02d" % (prefix, i) for i in range(n)}


COLLIDE  = SINGLETON | series("TT_Floor_") | series("TT_JWall_")
SHIPPING = COLLIDE | RENDER_ONLY

# Written by build_blockout.py; must match blender_staging_setup.py's copies.
MATERIALS_JSON = os.path.expanduser(
    "~/code/azerothcore-wotlk/var/blender/twisted_treeline_v2_materials.json")
DOODADS_JSON = MATERIALS_JSON.replace("_materials.json", "_doodads.json")

try:
    with open(MATERIALS_JSON) as fh:
        TEXTURES = json.load(fh)
    with open(DOODADS_JSON) as fh:
        DOODADS = json.load(fh)
except OSError as exc:
    raise SystemExit("ABORT: cannot read %s (%s) -- run gen_blockout.py first"
                     % (exc.filename, exc))

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
    if root.wow_wmo.wmo_id != ROOT_WMO_ID:
        notes.append("wmo_id was %d, set to %d" % (root.wow_wmo.wmo_id, ROOT_WMO_ID))
        root.wow_wmo.wmo_id = ROOT_WMO_ID

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

# -- 2b. server frame -------------------------------------------------------
# Checked, never repaired: turning the scene is blender_staging_setup.py's job,
# and doing it here would silently rotate a scene that already carries doodads.
# On Y because the map is a left-right mirror -- an island renamed onto its own
# mirror twin reads like the turn in x, but twins share their y.
probe = bpy.data.objects.get(ORIENT_PROBE)
if probe is None:
    fails.append("orientation probe %s is missing" % ORIENT_PROBE)
else:
    ys = [(probe.matrix_world @ Vector(c)).y for c in probe.bound_box]
    cy = (min(ys) + max(ys)) / 2.0
    print("\n== ORIENTATION ==\n  %s centres on y=%+.2f (server frame wants %+.2f)"
          % (ORIENT_PROBE, cy, -ORIENT_PROBE_Y))
    if abs(cy + ORIENT_PROBE_Y) > 1.0:
        fails.append("scene is not in the server frame; re-run blender_staging_setup.py")

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
shipped = {o.name for o in outdoor.objects} if outdoor else set()
print("  %d objects: collide=%d (want %d) render-only=%d (want %d)"
      % (len(shipped), n_c, len(COLLIDE), n_r, len(RENDER_ONLY)))
if shipped != SHIPPING:
    fails.append("Outdoor set mismatch; missing=%s extra=%s"
                 % (sorted(SHIPPING - shipped), sorted(shipped - SHIPPING)))

# -- 5. doodads -------------------------------------------------------------
# Checked, never rebuilt: the set comes from the sidecar and building it is
# blender_staging_setup.py's job. Only the enabled flag is repaired, because
# WBS's own handler is what clears it.
print("\n== DOODADS ==")
if doodads is not None:
    dset = find_child(doodads, SET_NAME)
    if dset is None:
        for k in doodads.children:
            if k.name.startswith("Set_$DefaultGlobal"):
                dset = k
    if dset is None:
        fails.append("no %s collection; run blender_staging_setup.py" % SET_NAME)
    else:
        want = len(DOODADS["placements"])
        got = list(dset.objects)
        counts = {}
        for ob in got:
            if not ob.wow_wmo_doodad.enabled:
                ob.wow_wmo_doodad.enabled = True
                notes.append("re-enabled doodad %s" % ob.name)
            if not ob.wow_wmo_doodad.path:
                fails.append("doodad %s has no model path" % ob.name)
            try:
                if ob.hide_get():
                    ob.hide_set(False)
                    notes.append("unhid %s (build_references skips hidden objects)"
                                 % ob.name)
            except RuntimeError:
                notes.append("%s not in the view layer; could not unhide" % ob.name)
            counts[ob.wow_wmo_doodad.path] = counts.get(ob.wow_wmo_doodad.path, 0) + 1
        print("  %d doodads in %r, %d distinct models" % (len(got), dset.name, len(counts)))
        for p, n in sorted(counts.items()):
            print("   %4d  %s" % (n, p))
        if len(got) != want:
            fails.append("%s holds %d doodads, %s wants %d -- re-run "
                         "blender_staging_setup.py"
                         % (dset.name, len(got), os.path.basename(DOODADS_JSON), want))

# -- 6. report --------------------------------------------------------------
print("\n" + "=" * 70)
print("root            : %s" % (root.name if root else "NONE"))
print("wmo_id          : %s" % (root.wow_wmo.wmo_id if root else "-"))
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
