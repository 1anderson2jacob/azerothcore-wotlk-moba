"""Build the blockout .blend from a traced boundary and its height field.

    blender --background --python apps/moba/wmo/build_blockout.py -- --params P

Driven by apps/moba/gen_blockout.py, same as blockout_trace.py -- parameters
arrive as JSON because Blender's python has no yaml.

No booleans anywhere. Island loops become solid extruded prisms; the outer loop
becomes one thick ribbon. Every face is a quad or a cap triangle placed
deliberately, so terracing is arithmetic on rings rather than a selection over
boolean output whose topology nothing predicts.

The floor ships as grid chunks rather than one sheet -- see BATCH_TRI_CEILING.
"""
import json
import math
import os
import random
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector
from mathutils.geometry import tessellate_polygon


# A WMO render batch counts MOVI indices in uint16, and WBS emits one batch per
# material per group -- so a group over this many triangles ships a batch whose
# count has WRAPPED, and the client draws only the remainder. Nothing warns: the
# geometry is all in the file and the collision BSP is complete, so the map is
# solid where it is invisible. A 28k-triangle floor drew 23% of itself.
BATCH_TRI_CEILING = 65535 // 3

# Chunks are cut to a quarter of that, so refining the floor tessellation 4x
# still exports without anyone re-deciding the grid.
FLOOR_CHUNK_TARGET = BATCH_TRI_CEILING // 4


# ------------------------------------------------------------------ sampling

class Heights:
    """z at any (x, y), from the grayscale field blockout_trace.py emitted."""

    def __init__(self, path, reg, scale_yd):
        img = bpy.data.images.load(path, check_existing=False)
        img.colorspace_settings.name = "Non-Color"
        w, h = img.size
        buf = np.empty(w * h * 4, dtype=np.float32)
        img.pixels.foreach_get(buf)
        bpy.data.images.remove(img)
        self.a = buf.reshape(h, w, 4)[:, :, 0] * scale_yd     # row 0 is the BOTTOM row
        self.w, self.h = w, h
        self.cx, self.cy, self.s = reg["cx_px"], reg["cy_px"], reg["scale_yd_per_px"]

    def at(self, x, y):
        """Bilinear. Nearest-pixel sampling quantises a slope to the 0.6 yd pixel
        grid, and floor vertices landing on different plateaus then make the
        surface locally non-monotonic -- a ramp you catch on running across."""
        fc = min(max(x / self.s + self.cx, 0.0), self.w - 1.001)
        fr = min(max(y / self.s + self.cy, 0.0), self.h - 1.001)
        c, r = int(fc), int(fr)
        tc, tr = fc - c, fr - r
        g = self.a[r:r + 2, c:c + 2]
        return float((g[0, 0] * (1 - tc) + g[0, 1] * tc) * (1 - tr)
                     + (g[1, 0] * (1 - tc) + g[1, 1] * tc) * tr)

    def at_max(self, x, y):
        """Max over a 3x3 neighbourhood, for wall feet only. A wall foot sampled
        exactly on the platform rim must take the platform's height, not a blend
        with the lane outside it, or the wall sinks into the floor it stands on."""
        c = int(round(x / self.s + self.cx))
        r = int(round(y / self.s + self.cy))
        r0, r1 = max(r - 1, 0), min(r + 2, self.h)
        c0, c1 = max(c - 1, 0), min(c + 2, self.w)
        if r0 >= r1 or c0 >= c1:
            return 0.0
        return float(self.a[r0:r1, c0:c1].max())


# -------------------------------------------------------------------- rings

def signed_area(pts):
    n = len(pts)
    return 0.5 * sum(pts[i][0] * pts[(i + 1) % n][1] - pts[(i + 1) % n][0] * pts[i][1]
                     for i in range(n))


def offset_loop(pts, dist):
    """Move every point `dist` along the outward normal. Positive dist pushes a
    CCW loop outward and a CW loop inward -- in both cases away from the
    playable side, which is the direction wall material lives in."""
    n = len(pts)
    out = []
    for i in range(n):
        ax, ay = pts[i - 1]
        bx, by = pts[i]
        cx, cy = pts[(i + 1) % n]
        nx, ny = 0.0, 0.0
        for (ux, uy) in ((bx - ax, by - ay), (cx - bx, cy - by)):
            length = math.hypot(ux, uy)
            if length > 1e-9:
                nx += uy / length
                ny += -ux / length
        length = math.hypot(nx, ny)
        if length < 1e-9:
            out.append((bx, by))
        else:
            out.append((bx + nx / length * dist, by + ny / length * dist))
    return out


def resample(pts, step):
    """Even spacing around a closed loop."""
    out, carry = [pts[0]], 0.0
    for i in range(1, len(pts) + 1):
        ax, ay = pts[i - 1]
        bx, by = pts[i % len(pts)]
        length = math.hypot(bx - ax, by - ay)
        if length < 1e-9:
            continue
        t = 0.0
        while carry + (length - t) >= step:
            t += step - carry
            out.append((ax + (bx - ax) * t / length, ay + (by - ay) * t / length))
            carry = 0.0
        carry += length - t
    return out


def safe_offset(pts, dist):
    """Offset, then pull back any vertex whose move reversed one of its own
    edges. A uniform offset folds wherever the curve turns tighter than `dist`,
    and a fold does not change the loop's total signed area -- so an area check
    cannot see it. The render shows it as spikes."""
    out = offset_loop(pts, dist)
    scale = [1.0] * len(pts)
    clamped = 0
    for _ in range(8):
        bad = set()
        for i in range(len(pts)):
            j = (i + 1) % len(pts)
            ox, oy = pts[j][0] - pts[i][0], pts[j][1] - pts[i][1]
            nx, ny = out[j][0] - out[i][0], out[j][1] - out[i][1]
            if ox * nx + oy * ny < 0.0:
                bad.add(i)
                bad.add(j)
        if not bad:
            break
        clamped += len(bad)
        for i in bad:
            scale[i] *= 0.5
        full = offset_loop(pts, dist)
        out = [(pts[i][0] + (full[i][0] - pts[i][0]) * scale[i],
                pts[i][1] + (full[i][1] - pts[i][1]) * scale[i])
               for i in range(len(pts))]
    return out, clamped


def hull_ring(pts, margin, step):
    """The outer wall's outer edge: the boundary's convex hull, pushed out.

    Not a parallel offset of the boundary -- that folds at any useful distance.
    Not a bounding rectangle either, which on an oval map leaves enormous flat
    corners nowhere near the play area. Offsetting a CONVEX loop outward cannot
    self-intersect, which is the whole reason to take the hull first."""
    P = sorted({(round(x, 4), round(y, 4)) for x, y in pts})
    if len(P) < 3:
        return list(P)

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

    def half(seq):
        out = []
        for q in seq:
            while len(out) >= 2 and cross(out[-2], out[-1], q) <= 0:
                out.pop()
            out.append(q)
        return out

    hull = half(P)[:-1] + half(P[::-1])[:-1]
    return resample(offset_loop(hull, margin), step)


def loop_length(pts):
    n = len(pts)
    return sum(math.dist(pts[i], pts[(i + 1) % n]) for i in range(n))


def height_profile(pts, cfg, rng):
    """One wall height per point, varying along the loop so segments differ.

    Cosine-interpolated between anchors rather than stepped: a hard change in
    wall height would put a vertical seam through the ribbon's top strip."""
    total = loop_length(pts)
    seg = max(cfg["segment_yd"], 1.0)
    n_anchor = max(2, int(round(total / seg)))
    anchors = [cfg["height_yd"] + rng.uniform(-cfg["variation_yd"], cfg["variation_yd"])
               for _ in range(n_anchor)]
    out, run = [], 0.0
    for i in range(len(pts)):
        u = (run / total) * n_anchor if total > 0 else 0.0
        j = int(u) % n_anchor
        f = u - int(u)
        a, b = anchors[j], anchors[(j + 1) % n_anchor]
        out.append(a + (b - a) * (0.5 - 0.5 * math.cos(math.pi * f)))
        run += math.dist(pts[i], pts[(i + 1) % len(pts)])
    return out


# --------------------------------------------------------------- mesh build

class Shell:
    """Vertex-sharing shell builder. Each ring is added once and strips index
    into it -- appending a fresh copy per strip is what leaves every seam
    non-manifold, and the exporter has no way to tell that from a real hole."""

    def __init__(self):
        self.verts = []
        self.faces = []

    def ring(self, pts):
        base = len(self.verts)
        self.verts.extend(pts)
        return base

    def strip(self, a, b, n):
        for i in range(n):
            j = (i + 1) % n
            self.faces.append([a + i, a + j, b + j, b + i])

    def cap(self, base, pts):
        for t in tessellate_polygon([[Vector((x, y, 0.0)) for x, y, _ in pts]]):
            self.faces.append([base + t[0], base + t[1], base + t[2]])

    def annulus(self, out_base, out_pts, out_z, in_base, in_pts):
        """Fill between an outer ring and an inner one of a different point
        count, by triangulating the outer polygon with the inner as its hole."""
        polys = [[Vector((x, y, 0.0)) for x, y in out_pts],
                 [Vector((x, y, 0.0)) for x, y, _ in in_pts]]
        n_out = len(out_pts)
        for t in tessellate_polygon(polys):
            f = [(out_base + i) if i < n_out else (in_base + i - n_out) for i in t]
            self.faces.append(f)


def ring_at(pts, z_list):
    return [(p[0], p[1], z) for p, z in zip(pts, z_list)]


CLAMPED = [0]


def wall_rings(pts, floor_z, top_z, thickness, cfg):
    """Inner-to-outer rings of one wall cross-section, terraced and bevelled.

    Returns None when the inset would consume the loop -- a small island cannot
    carry a ledge, and forcing one turns it inside out."""
    drop, ledge, bevel = cfg["terrace_drop_yd"], cfg["terrace_ledge_yd"], cfg["bevel_yd"]
    inner = list(pts)
    step1, c1 = safe_offset(inner, ledge)
    step2, c2 = safe_offset(inner, ledge + bevel)
    a0 = signed_area(inner)
    for cand in (step1, step2):
        a = signed_area(cand)
        if a * a0 <= 0 or abs(a) < abs(a0) * 0.15:
            return None
    CLAMPED[0] += c1 + c2
    z_ledge = [max(t - drop, floor_z[i] + 1.0) for i, t in enumerate(top_z)]
    z_top = list(top_z)
    z_bev = [t - bevel for t in z_top]
    return [
        ("inner_bottom", ring_at(inner, [cfg["bottom_yd"]] * len(inner))),
        ("inner_floor", ring_at(inner, floor_z)),
        ("inner_ledge", ring_at(inner, z_ledge)),
        ("ledge_out", ring_at(step1, z_ledge)),
        ("riser_top", ring_at(step1, z_bev)),
        ("bevel_out", ring_at(step2, z_top)),
    ]


def build_wall(name, pts, floor_z, top_z, thickness, cfg):
    rings = wall_rings(pts, floor_z, top_z, thickness, cfg)
    flat = rings is None
    if flat:                                # too small to terrace: capped prism
        rings = [("inner_bottom", ring_at(pts, [cfg["bottom_yd"]] * len(pts))),
                 ("inner_floor", ring_at(pts, floor_z)),
                 ("top", ring_at(pts, top_z))]
    shell = Shell()
    n = len(pts)
    bases = [shell.ring(r[1]) for r in rings]
    for a, b in zip(bases, bases[1:]):
        shell.strip(a, b, n)
    if thickness is None:
        shell.cap(bases[-1], rings[-1][1])
        shell.cap(bases[0], rings[0][1])
    else:
        rim = rings[-1][1]
        plate_z = max(z for _, _, z in rim)
        rect = hull_ring([(x, y) for x, y, _ in rim], thickness,
                         cfg.get("outer_step_yd", 15.0))
        top = shell.ring([(x, y, plate_z) for x, y in rect])
        bot = shell.ring([(x, y, cfg["bottom_yd"]) for x, y in rect])
        shell.annulus(top, rect, plate_z, bases[-1], rim)
        shell.strip(top, bot, len(rect))
        shell.annulus(bot, rect, cfg["bottom_yd"], bases[0], rings[0][1])
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(shell.verts, [], shell.faces)
    mesh.validate()
    obj = bpy.data.objects.new(name, mesh)
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)   # cheaper than reasoning
    bm.to_mesh(mesh)                                    # about winding per cap
    bm.free()
    return obj, flat


def build_floor(loops, heights, coarse_yd, fine_yd):
    """Coarse everywhere, fine only across a slope.

    A uniform target fine enough for a 6 yd ramp puts tens of thousands of
    vertices on flat ground, and a WMO group indexes its vertices with 16 bits.

    Returns the unsplit sheet; chunk_floor cuts it into shippable groups."""
    polys = [[Vector((x, y, 0.0)) for x, y in lp["points"]] for lp in loops]
    tris = tessellate_polygon(polys)
    flat = [v for poly in polys for v in poly]
    mesh = bpy.data.meshes.new("TT_Floor_whole")
    mesh.from_pydata([(v.x, v.y, 0.0) for v in flat], [], [list(t) for t in tris])
    mesh.validate()
    seed_tris = len(mesh.polygons)

    bm = bmesh.new()
    bm.from_mesh(mesh)
    for _ in range(14):
        todo = []
        for e in bm.edges:
            length = e.calc_length()
            if length <= fine_yd:
                continue
            if length > coarse_yd:
                todo.append(e)
                continue
            a, b = e.verts[0].co, e.verts[1].co
            if abs(heights.at(a.x, a.y) - heights.at(b.x, b.y)) > 0.05:
                todo.append(e)
        if not todo:
            break
        bmesh.ops.subdivide_edges(bm, edges=todo, cuts=1, use_grid_fill=True)
    # subdivide_edges splits edges, never the face they belong to, so the seed's
    # long thin triangles accumulate boundary vertices without ever being cut --
    # one reached 105 verts spanning 350 yd. Flat now and non-planar the moment z
    # lands, and then whoever tessellates it may join a 3 yd vertex to a 0 yd one.
    bmesh.ops.triangulate(bm, faces=bm.faces[:],
                          quad_method="BEAUTY", ngon_method="BEAUTY")
    for v in bm.verts:
        v.co.z = heights.at(v.co.x, v.co.y)
    bm.to_mesh(mesh)
    bm.free()
    return mesh, seed_tris


# --------------------------------------------------------------- chunking

def tri_count(mesh):
    """Triangles WBS will batch. Blender tessellates an n-gon into n-2, and this
    matched len(loop_triangles) exactly on the 28297-triangle floor."""
    return sum(len(p.vertices) - 2 for p in mesh.polygons)


def bucket(cells, cols, rows, x0, x1, y0, y1):
    """Grid cell -> indices into `cells`, by polygon centre."""
    out = {}
    for k, (cx, cy, _tris, _pi) in enumerate(cells):
        i = min(int((cx - x0) / (x1 - x0) * cols), cols - 1)
        j = min(int((cy - y0) / (y1 - y0) * rows), rows - 1)
        out.setdefault((i, j), []).append(k)
    return out


def choose_grid(cells, x0, x1, y0, y1, target):
    """Smallest grid whose fullest cell fits `target` triangles.

    Grows whichever axis currently has the longer cells, so chunks stay
    square-ish at any map aspect -- a long thin group culls badly and its
    bounding box overlaps most of the map."""
    cols = rows = 1
    while True:
        grid = bucket(cells, cols, rows, x0, x1, y0, y1)
        worst = max(sum(cells[k][2] for k in members) for members in grid.values())
        if worst <= target:
            return cols, rows
        if cols * rows > 4096:
            raise SystemExit("floor will not chunk under %d triangles per group" % target)
        if (x1 - x0) / cols >= (y1 - y0) / rows:
            cols += 1
        else:
            rows += 1


def chunk_floor(mesh, target):
    """Split the floor sheet into grid chunks, each small enough for one batch.

    Cuts on polygon boundaries, so chunks share vertex positions along a seam
    and neither the render nor the collision has a crack; world-space UVs run
    continuously across one too. Recast rasterises the chunks together, so the
    navmesh does not see a seam either."""
    cells = [(p.center.x, p.center.y, len(p.vertices) - 2, p.index)
             for p in mesh.polygons]
    xs = [v.co.x for v in mesh.vertices]
    ys = [v.co.y for v in mesh.vertices]
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    cols, rows = choose_grid(cells, x0, x1, y0, y1, target)
    grid = bucket(cells, cols, rows, x0, x1, y0, y1)

    out = []
    for key in sorted(grid):
        name = "TT_Floor_%02d" % len(out)
        remap, verts, faces = {}, [], []
        for k in grid[key]:
            face = []
            for vi in mesh.polygons[cells[k][3]].vertices:
                if vi not in remap:
                    remap[vi] = len(verts)
                    verts.append(tuple(mesh.vertices[vi].co))
                face.append(remap[vi])
            faces.append(face)
        chunk = bpy.data.meshes.new(name)
        chunk.from_pydata(verts, [], faces)
        chunk.validate()
        out.append((name, chunk))
    return out, (cols, rows)


# ----------------------------------------------------------------- dressing

def point_in_loop(pts, x, y):
    inside = False
    n = len(pts)
    for i in range(n):
        ax, ay = pts[i]
        bx, by = pts[(i + 1) % n]
        if (ay > y) != (by > y):
            if x < ax + (y - ay) / (by - ay) * (bx - ax):
                inside = not inside
    return inside


def scatter_cones(loops, tops, cfg, rng):
    """Placeholder tree mass on the wall tops. Flat grey and obviously wrong on
    purpose -- it is replaced by M2 doodads in dressing."""
    verts, faces = [], []
    for lp, top in zip(loops, tops):
        pts = lp["points"]
        area = abs(signed_area(pts))
        count = int(area / 1000.0 * cfg["per_1000_yd2"])
        xs = [p[0] for p in pts]
        ys = [p[1] for p in pts]
        placed, guard = 0, 0
        while placed < count and guard < count * 60:
            guard += 1
            x = rng.uniform(min(xs), max(xs))
            y = rng.uniform(min(ys), max(ys))
            if not point_in_loop(pts, x, y):
                continue
            placed += 1
            r = rng.uniform(*cfg["radius_yd"])
            hgt = rng.uniform(*cfg["height_yd"])
            z = top - cfg.get("sink_yd", 1.0)
            base = len(verts)
            sides = 7
            for k in range(sides):
                a = 2 * math.pi * k / sides
                verts.append((x + r * math.cos(a), y + r * math.sin(a), z))
            verts.append((x, y, z + hgt))
            tip = base + sides
            for k in range(sides):
                faces.append([base + k, base + (k + 1) % sides, tip])
    if not verts:
        return None
    mesh = bpy.data.meshes.new("TT_Trees")
    mesh.from_pydata(verts, [], faces)
    mesh.validate()
    return bpy.data.objects.new("TT_Trees", mesh)


# --------------------------------------------------------------- finishing

def cube_uv(obj, scale):
    """World-space cube projection, tiling. WBS wants the layer named UVMap."""
    mesh = obj.data
    bm = bmesh.new()
    bm.from_mesh(mesh)
    uv = bm.loops.layers.uv.new("UVMap")
    for face in bm.faces:
        n = face.normal
        axis = max(range(3), key=lambda i: abs(n[i]))
        for loop in face.loops:
            co = loop.vert.co
            if axis == 0:
                u, v = co.y, co.z
            elif axis == 1:
                u, v = co.x, co.z
            else:
                u, v = co.x, co.y
            loop[uv].uv = (u / scale, v / scale)
    bm.to_mesh(mesh)
    bm.free()


def material(name, colour):
    mat = bpy.data.materials.get(name)
    if mat is None:
        mat = bpy.data.materials.new(name)
        mat.use_nodes = True
        bsdf = mat.node_tree.nodes.get("Principled BSDF")
        if bsdf:
            bsdf.inputs["Base Color"].default_value = (*colour, 1.0)
    return mat


def finish(obj, mat, collide, uv_scale, collection):
    obj.data.materials.append(mat)
    cube_uv(obj, uv_scale)
    # Does NOT reach the staging scene -- custom properties do not survive OBJ
    # (README step 2). The real carrier is the object NAME, matched against
    # blender_staging_setup.py's COLLIDE set. This only drives the gate below.
    obj["wmo_collide"] = bool(collide)
    collection.objects.link(obj)


def nonmanifold(obj):
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    n = sum(1 for e in bm.edges if len(e.link_faces) != 2)
    bm.free()
    return n


# ------------------------------------------------------------------- main

def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    with open(argv[argv.index("--params") + 1]) as fh:
        cfg = json.load(fh)
    bo = cfg["blockout"]
    wall_cfg = dict(bo["wall"])
    wall_cfg["bottom_yd"] = bo["bottom_yd"]

    with open(cfg["geometry"]) as fh:
        geom = json.load(fh)
    heights = Heights(cfg["heights"], geom["registration"],
                      geom["elevation"]["height_scale_yd"])

    bpy.ops.wm.read_factory_settings(use_empty=True)
    coll = bpy.data.collections.new(bo["export_collection"])
    bpy.context.scene.collection.children.link(coll)

    rng = random.Random(bo["seed"])
    outer = [l for l in geom["loops"] if l["kind"] == "outer"]
    holes = [l for l in geom["loops"] if l["kind"] == "hole"]
    if len(outer) != 1:
        raise SystemExit(f"expected 1 outer loop, geometry.json has {len(outer)}")

    mats = {k: material(v, c) for (k, v), c in
            zip(bo["materials"].items(), ((0.32, 0.30, 0.27), (0.38, 0.35, 0.32),
                                          (0.30, 0.33, 0.28)))}

    sheet, seed_tris = build_floor([outer[0]] + holes, heights,
                                   bo["floor_edge_yd"], bo["floor_detail_edge_yd"])
    chunks, grid = chunk_floor(sheet, FLOOR_CHUNK_TARGET)
    bpy.data.meshes.remove(sheet)           # the unsplit sheet is not shipped
    floors = []
    for name, chunk in chunks:
        obj = bpy.data.objects.new(name, chunk)
        finish(obj, mats["floor"], True, bo["uv_scale_yd"], coll)
        floors.append(obj)

    walls, tops, flats = [], [], 0
    jobs = [("TT_OuterWall", outer[0]["points"], wall_cfg["outer_margin_yd"])]
    jobs += [("TT_JWall_%02d" % i, h["points"], None) for i, h in enumerate(holes)]
    for name, pts, thickness in jobs:
        floor_z = [heights.at_max(x, y) for x, y in pts]
        rise = height_profile(pts, wall_cfg, rng)
        top_z = [f + r for f, r in zip(floor_z, rise)]
        obj, flat = build_wall(name, pts, floor_z, top_z, thickness, wall_cfg)
        flats += int(flat)
        finish(obj, mats["wall"], True, bo["uv_scale_yd"], coll)
        walls.append(obj)
        tops.append(sum(top_z) / len(top_z))

    trees = scatter_cones(holes, tops[1:], bo["scatter"], rng)
    if trees is not None:
        finish(trees, mats["tree"], False, bo["uv_scale_yd"], coll)

    for obj in list(coll.objects):
        obj.data.transform(obj.matrix_world)
        obj.matrix_world.identity()

    bpy.ops.wm.save_as_mainfile(filepath=cfg["out_blend"])

    objs = list(coll.objects)
    floor_names = {o.name for o in floors}
    zs = [v.co.z for o in objs for v in o.data.vertices]
    print("BUILD " + json.dumps({
        "objects": len(objs),
        "verts": sum(len(o.data.vertices) for o in objs),
        "faces": sum(len(o.data.polygons) for o in objs),
        "floor_chunks": len(floors),
        "floor_grid": [grid[0], grid[1]],
        "floor_chunk_max_tris": max(tri_count(o.data) for o in floors),
        # Higher than the unsplit sheet's: a seam vertex belongs to both chunks.
        "floor_verts": sum(len(o.data.vertices) for o in floors),
        "floor_faces": sum(len(o.data.polygons) for o in floors),
        "floor_seed_tris": seed_tris,
        "floor_area_yd2": round(sum(p.area for o in floors for p in o.data.polygons)),
        "floor_max_face_verts": max(len(p.vertices)
                                    for o in floors for p in o.data.polygons),
        "floor_max_face_span_yd": round(max(
            max(max(o.data.vertices[i].co[ax] for i in p.vertices)
                - min(o.data.vertices[i].co[ax] for i in p.vertices)
                for ax in (0, 1))
            for o in floors for p in o.data.polygons), 1),
        "over_16bit_index": [o.name for o in objs
                             if len(o.data.vertices) > 65535],
        "over_batch_tris": [o.name for o in objs
                            if tri_count(o.data) > BATCH_TRI_CEILING],
        "walls": len(walls),
        "walls_unterraced": flats,
        "offset_vertices_clamped": CLAMPED[0],
        "trees": 0 if trees is None else len(trees.data.polygons),
        "z_range_yd": [round(min(zs), 2), round(max(zs), 2)],
        # Render-only geometry is allowed to be open; the floor chunks are sheets
        # by construction. Only sealed collision solids are worth gating on.
        "nonmanifold_collide": {o.name: nonmanifold(o) for o in objs
                                if o.get("wmo_collide")
                                and o.name not in floor_names
                                and nonmanifold(o)},
        "no_uv": [o.name for o in objs if "UVMap" not in o.data.uv_layers],
        "empty_material_slots": [o.name for o in objs
                                 if any(s.material is None for s in o.material_slots)],
    }))


if __name__ == "__main__":
    main()
