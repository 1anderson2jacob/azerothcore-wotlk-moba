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
import bisect
import colorsys
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
from mathutils.bvhtree import BVHTree


# A WMO render batch counts MOVI indices in uint16, and WBS emits one batch per
# material per group -- so a group over this many triangles ships a batch whose
# count has WRAPPED, and the client draws only the remainder. Nothing warns: the
# geometry is all in the file and the collision BSP is complete, so the map is
# solid where it is invisible. A 28k-triangle floor drew 23% of itself.
BATCH_TRI_CEILING = 65535 // 3

# Chunks are cut to a quarter of that, so refining the floor tessellation 4x
# still exports without anyone re-deciding the grid.
FLOOR_CHUNK_TARGET = BATCH_TRI_CEILING // 4

# Floor material slots, in material-index order. gen_blockout.py holds the same
# tuple -- the two cannot share a constant across the yaml gap.
FLOOR_CLASSES = ("lane", "base", "ramp")

# Half-band around each plateau that still counts as flat. Wider than the height
# field's 0.6 yd pixel and far narrower than the 3 yd platform, so a face has to
# be genuinely on a slope to read as ramp.
FLOOR_CLASS_TOL_YD = 0.25

# Blender material name for a config key. The name is the only thing that
# reaches the staging scene through the OBJ, so it is the sidecar JSON's key.
MAT_PREFIX = "TT_"


# Wall face bands, and the face spans each covers. The ledge sits with `upper` so
# the whole upper terrace -- shelf, riser, chamfer, top -- reads as one unit; move
# "ledge" to the other tuple to group it with the lower tier instead. `skirt` is
# buried below the floor and never seen, so it rides along with `lower`.
WALL_BANDS = {"lower": ("skirt", "lower"),
              "upper": ("ledge", "upper", "bevel", "cap")}

# The vertical risers, and the only faces a wall SCATTER may land on.
# Deliberately NOT WALL_BANDS: that groups the buried skirt with `lower` and the
# horizontal cap with `upper`, which is right for a texture -- one island reads
# as one rock -- and would put half the props underground and stack the rest on
# the surface island_caps already scatters. The ledge rail names its own span
# and does not consult this.
WALL_RISERS = ("lower", "upper")


# How far out from a riser to sample the floor it faces. Past the wall's own
# footprint and well inside the narrowest base ring.
BASE_PROBE_YD = 5.0


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
        self.spans = []                     # (name, first, last) into self.faces

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

    def annulus(self, out_base, out_pts, in_base, in_pts):
        """Fill between an outer ring and an inner one of a different point
        count, by triangulating the outer polygon with the inner as its hole.

        Ring vertices carry their own z and the tessellation is 2D, so the fill
        follows whatever heights the two rings were built with."""
        polys = [[Vector((x, y, 0.0)) for x, y in out_pts],
                 [Vector((x, y, 0.0)) for x, y, _ in in_pts]]
        n_out = len(out_pts)
        for t in tessellate_polygon(polys):
            f = [(out_base + i) if i < n_out else (in_base + i - n_out) for i in t]
            self.faces.append(f)

    def mark(self, name, start):
        """Name the faces the last call appended. Caps and annuli tessellate to a
        count nobody predicts, so the range is measured, never computed."""
        if len(self.faces) > start:
            self.spans.append((name, start, len(self.faces)))


def loop_arc(ring):
    """Cumulative distance along a riser's xy loop, plus its total.

    Both rings of a riser share one xy loop -- wall_rings builds them from the
    same list -- so the arc belongs to the strip, not to either ring."""
    pts = [(x, y) for x, y, _ in ring]
    n = len(pts)
    arc, run = [], 0.0
    for i in range(n):
        arc.append(run)
        run += math.dist(pts[i], pts[(i + 1) % n])
    return pts, arc, run


def nearest_z(x, y, ring):
    """z of the closest point on a ring. The outer plate is sampled far more
    coarsely than the rim it follows -- a convex hull at 15 yd against a 4.5 yd
    boundary loop -- so this is a resample, not a lookup."""
    return min(ring, key=lambda p: (p[0] - x) ** 2 + (p[1] - y) ** 2)[2]


def ring_at(pts, z_list):
    return [(p[0], p[1], z) for p, z in zip(pts, z_list)]


CLAMPED = [0]


def wall_rings(pts, floor_z, top_z, thickness, cfg):
    """Inner-to-outer rings of one wall cross-section, terraced and bevelled.

    Returns None when the inset would consume the loop -- forcing a ledge there
    turns it inside out. Driven by shape, not size: safe_offset folds wherever
    the curve turns tighter than the 3.25 yd inset, so a long thin island fails
    at 336 yd2 while a compact one passes at 202."""
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


# Which strip each consecutive ring pair produces, in wall_rings' order. Change
# one and this changes with it -- the names are how a face learns what it is.
WALL_STRIPS_TERRACED = ("skirt", "lower", "ledge", "upper", "bevel")
WALL_STRIPS_FLAT = ("skirt", "lower")


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
    riser_loops = {}
    for k, nm in enumerate(WALL_STRIPS_FLAT if flat else WALL_STRIPS_TERRACED):
        start = len(shell.faces)
        shell.strip(bases[k], bases[k + 1], n)
        shell.mark(nm, start)
        if nm in WALL_RISERS:
            riser_loops[nm] = loop_arc(rings[k][1])
    if thickness is None:
        start = len(shell.faces)
        shell.cap(bases[-1], rings[-1][1])
        shell.mark("cap", start)
        start = len(shell.faces)
        shell.cap(bases[0], rings[0][1])
        shell.mark("skirt", start)
    else:
        rim = rings[-1][1]
        rect = hull_ring([(x, y) for x, y, _ in rim], thickness,
                         cfg.get("outer_step_yd", 15.0))
        # The plate FOLLOWS the crest. Pinned flat at the crest's global maximum
        # it turns the whole plateau into a ramp climbing to the tallest segment,
        # which is then visible over the lip from every stretch of wall shorter
        # than that one -- nearly all of them, since heights vary 12-20.
        top = shell.ring([(x, y, nearest_z(x, y, rim)) for x, y in rect])
        bot = shell.ring([(x, y, cfg["bottom_yd"]) for x, y in rect])
        start = len(shell.faces)
        shell.annulus(top, rect, bases[-1], rim)
        shell.mark("cap", start)
        start = len(shell.faces)
        shell.strip(top, bot, len(rect))
        shell.annulus(bot, rect, bases[0], rings[0][1])
        shell.mark("skirt", start)
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(shell.verts, [], shell.faces)
    built = len(shell.faces)
    mesh.validate()
    obj = bpy.data.objects.new(name, mesh)
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)   # cheaper than reasoning
    bm.to_mesh(mesh)                                    # about winding per cap
    bm.free()
    # Spans index the face list AS BUILT. A face dropped by validate() slides
    # every later face into the wrong band and nothing downstream could tell, so
    # the shortfall is returned and gated rather than assumed to be zero.
    return obj, flat, shell.spans, built - len(mesh.polygons), riser_loops


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


def max_batch_tris(mesh):
    """The count that actually hits the uint16 MOVI ceiling: WBS emits one batch
    per material per group, so a multi-material group's object total overstates
    it. Equal to tri_count on a single-material group."""
    per = {}
    for p in mesh.polygons:
        per[p.material_index] = per.get(p.material_index, 0) + len(p.vertices) - 2
    return max(per.values()) if per else 0


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

def bvh_of(mesh):
    """Fan-triangulated BVH over a built object.

    Fanned here rather than handing polygons to FromPolygons because a strip
    quad spanning two rings of differing z is not planar, and a ray must not be
    able to fall through whichever way someone else chose to split it."""
    verts = [tuple(v.co) for v in mesh.vertices]
    tris = []
    for p in mesh.polygons:
        vi = list(p.vertices)
        for k in range(1, len(vi) - 1):
            tris.append((vi[0], vi[k], vi[k + 1]))
    return BVHTree.FromPolygons(verts, tris, all_triangles=True)


def flat_area(mesh, min_nz):
    """Near-horizontal area projected onto the map plane, which is what a
    per-1000-yd2 density means when the surface is a stepped mesa: two terraces
    stacked over the same ground read as that ground's worth of trees."""
    return sum(p.area * p.normal.z for p in mesh.polygons if p.normal.z >= min_nz)


def scatter_on(obj, count, spec, keys, models, rng, min_nz):
    """Sample points on whatever the object's upward-facing surface turns out
    to be, by ray-casting straight down onto it.

    Cap, terrace ledge and the outer plateau are all reached the same way, and
    the slope test drops the risers and the chamfer without any of them being
    named -- so a retessellation, or an island that failed to terrace and came
    out a plain prism, cannot leave a doodad hanging off a wall face.

    A candidate is kept only when its whole FOOTPRINT lands on the surface, not
    just its origin. `inner_ledge` shares the loop's own xy, so the terrace
    shelf's outer edge IS the island boundary -- an origin-only test put 27% of
    island trees over an 8 yd drop, several of them within a yard of the edge."""
    me = obj.data
    if count <= 0:
        return []
    spacing = float(spec["min_spacing_yd"])
    jitter = float(spec.get("scale_jitter", 0.0))
    margin = float(spec.get("edge_margin_yd", 0.0))
    bvh = bvh_of(me)
    xs = [v.co.x for v in me.vertices]
    ys = [v.co.y for v in me.vertices]
    top = max(v.co.z for v in me.vertices) + 10.0
    down = Vector((0.0, 0.0, -1.0))
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    # A surface inside the allowed slope cannot rise more than this across the
    # footprint, so anything that does is a terrace step, not one surface.
    grade = math.tan(math.acos(min(max(min_nz, -1.0), 1.0)))
    cell = max(spacing, 0.001)
    grid, out = {}, []

    def ground(x, y):
        loc, nor, _i, _d = bvh.ray_cast(Vector((x, y, top)), down)
        return None if loc is None or nor.z < min_nz else loc.z

    for _ in range(count * 60):
        if len(out) >= count:
            break
        x = rng.uniform(x0, x1)
        y = rng.uniform(y0, y1)
        key = keys[rng.randrange(len(keys))]
        m = models[key]
        # Jittered here, not by the caller: the radius the footprint is tested
        # at has to be the radius that ships.
        scale = m["scale"] * (1.0 + rng.uniform(-jitter, jitter))
        r = max(m["box"][0], m["box"][1]) / 2.0 * scale + margin
        z = ground(x, y)
        if z is None:
            continue
        tol = r * grade + 0.25
        if any(pz is None or abs(pz - z) > tol
               for pz in (ground(x + r * math.cos(a), y + r * math.sin(a))
                          for a in (k * math.pi / 4.0 for k in range(8)))):
            continue
        gx, gy = int(x // cell), int(y // cell)
        clash = any((px - x) ** 2 + (py - y) ** 2 < spacing * spacing
                    for i in range(gx - 1, gx + 2)
                    for j in range(gy - 1, gy + 2)
                    for px, py in grid.get((i, j), ()))
        if clash:
            continue
        grid.setdefault((gx, gy), []).append((x, y))
        out.append((x, y, z, key, scale))
    return out


def band_faces(mesh, spans, band):
    """Polygon indices of one riser, from build_wall's record of which ring pair
    made each face. An unterraced prism has no `upper` span at all, so it yields
    nothing and the band reports as absent rather than as an empty scatter."""
    out = []
    for name, start, end in spans:
        if name == band:
            out += range(start, min(end, len(mesh.polygons)))
    return out


def face_columns(mesh, poly):
    """A riser face as its two vertical edges: ((xy, z_bottom, z_top), same).

    Both rings of a strip are built on the SAME xy loop, so a riser quad is
    exactly vertical and its corners fall into two columns. Pairing by xy rather
    than sorting the four corners by z matters where the ledge over one point
    sits below the floor under the next -- clear today by 4 yd, and by nothing
    that enforces it."""
    cols = {}
    for i in poly.vertices:
        co = mesh.vertices[i].co
        cols.setdefault((round(co.x, 3), round(co.y, 3)), []).append(co.z)
    if len(cols) != 2 or any(len(z) != 2 for z in cols.values()):
        return None
    return tuple((Vector(xy), min(z), max(z)) for xy, z in cols.items())


def ledge_columns(mesh, poly):
    """A terrace-shelf quad as its two CROSS-sections: ((lip_xy, foot_xy, z), same).

    The shelf is horizontal, so its four corners fall into four xy columns and
    face_columns reads nothing there. They pair by VERTEX INDEX instead:
    Shell.ring appends `inner_ledge` before `ledge_out` and both are the same
    loop, so the two lowest indices are the drop lip and the two highest the
    riser foot -- which is what pairs them across the wrap face too, where loop
    order is not index order. Self-checking: wall_rings gives both rings one z
    list, so a mispaired corner reads a z that does not match its partner's."""
    vi = sorted(poly.vertices)
    if len(vi) != 4 or vi[2] - vi[0] != vi[3] - vi[1]:
        return None
    co = [mesh.vertices[i].co for i in vi]
    if abs(co[0].z - co[2].z) > 1e-3 or abs(co[1].z - co[3].z) > 1e-3:
        return None
    return tuple((Vector((co[k].x, co[k].y)),
                  Vector((co[k + 2].x, co[k + 2].y)), co[k].z) for k in (0, 1))


def scatter_on_faces(obj, idx, count, spec, keys, models, rng):
    """Sample points across one riser, area-weighted over its faces.

    A downward ray cannot land on a vertical face, so this samples the faces
    directly -- and needs no slope test to do it: a riser is exactly vertical by
    construction, which leaves the span filter as the whole selector.

    The model is FITTED to the band by its own scaled extent rather than dropped
    at the sample point, because the three ways a doodad can be authored --
    standing on its origin, hanging below it, floating above it -- put its
    geometry in three different places relative to that point."""
    me = obj.data
    if count <= 0:
        return []
    spacing = float(spec["min_spacing_yd"])
    jitter = float(spec.get("scale_jitter", 0.0))
    margin = float(spec.get("edge_margin_yd", 0.0))
    push = float(spec.get("push_yd", 0.0))

    faces, weights, total = [], [], 0.0
    for k in idx:
        cols = face_columns(me, me.polygons[k])
        if cols is None:
            continue
        total += me.polygons[k].area
        faces.append((cols, me.polygons[k].normal.copy()))
        weights.append(total)
    if not faces:
        return []

    cell = max(spacing, 0.001)
    grid, out = {}, []
    for _ in range(count * 60):
        if len(out) >= count:
            break
        j = min(bisect.bisect_left(weights, rng.uniform(0.0, total)),
                len(faces) - 1)
        (a, b), n = faces[j]
        u = rng.random()
        xy = a[0].lerp(b[0], u)
        z_bot = a[1] + (b[1] - a[1]) * u + margin
        z_top = a[2] + (b[2] - a[2]) * u - margin
        key = keys[rng.randrange(len(keys))]
        m = models[key]
        # Jittered here, not by the caller: the extent fitted into the band has
        # to be the extent that ships.
        scale = m["scale"] * (1.0 + rng.uniform(-jitter, jitter))
        lo, hi = (v * scale for v in m["box"][2:4])
        room = (z_top - z_bot) - (hi - lo)
        base = (z_bot + rng.uniform(0.0, room) if room >= 0.0
                else (z_bot + z_top - (hi - lo)) / 2.0)   # taller than the band
        # Spaced on the geometry's own centre, never the origin: origins sit
        # anywhere from 14 yd above a model to 4 yd below it, so spacing those
        # spaces nothing anyone can see.
        cx, cy, cz = xy.x, xy.y, base + (hi - lo) / 2.0
        gx, gy, gz = int(cx // cell), int(cy // cell), int(cz // cell)
        if any((px - cx) ** 2 + (py - cy) ** 2 + (pz - cz) ** 2 < spacing * spacing
               for p in range(gx - 1, gx + 2)
               for q in range(gy - 1, gy + 2)
               for r in range(gz - 1, gz + 2)
               for px, py, pz in grid.get((p, q, r), ())):
            continue
        grid.setdefault((gx, gy, gz), []).append((cx, cy, cz))
        out.append((cx + n.x * push, cy + n.y * push, base - lo,
                    math.degrees(math.atan2(n.y, n.x)), key, scale))
    return out

def base_facing(mesh, idx, heights, base_yd):
    """Split a riser's faces into those fronting base-height floor and the rest.

    A base is a ring of raised platform, so the walls bounding it -- the outer
    ring's back arc, and the whole of the island the ring encircles -- are
    exactly the faces whose ground stands at base height. Nothing is named or
    indexed, so a repaint that moves a base moves this with it."""
    at, rest = [], []
    for k in idx:
        p = mesh.polygons[k]
        c, n = p.center, p.normal
        z = heights.at(c.x + n.x * BASE_PROBE_YD, c.y + n.y * BASE_PROBE_YD)
        (at if z > base_yd * 0.5 else rest).append(k)
    return at, rest


def contiguous(idx):
    """Maximal runs of consecutive face indices.

    band_faces returns them in the order shell.strip appended, which is loop
    order -- so a gap in the numbering IS a gap in the wall, and the outer
    ring's two base arcs fall out as two runs without anything here knowing a
    base exists."""
    runs, cur = [], []
    for k in idx:
        if cur and k != cur[-1] + 1:
            runs.append(cur)
            cur = []
        cur.append(k)
    if cur:
        runs.append(cur)
    return runs


def run_segments(mesh, run, columns=face_columns):
    """A run as an ordered walk: (near column, far column, outward normal) per
    face, each face's columns ordered to continue the one before it.

    `columns` reads one face. Both readers put the xy that ADVANCES along the
    run in slot 0, which is the only slot this touches.

    face_columns reads poly.vertices, and recalc_face_normals may have flipped
    the winding of every face on the object -- consistently, but in whichever
    direction it chose. Walked unordered, a rail zig-zags on the spot instead of
    advancing along the wall."""
    cols = [(k, columns(mesh, mesh.polygons[k])) for k in run]
    cols = [(k, c) for k, c in cols if c is not None]
    if len(cols) < 2:
        return []
    a0, b0 = cols[0][1]                   # seed: face 0's shared column goes last
    nxt = cols[1][1]
    if (min((a0[0] - q[0]).length for q in nxt)
            < min((b0[0] - q[0]).length for q in nxt)):
        cols[0] = (cols[0][0], (b0, a0))
    segs, prev = [], None
    for k, (a, b) in cols:
        if prev is not None and (a[0] - prev).length > (b[0] - prev).length:
            a, b = b, a
        segs.append((a, b, mesh.polygons[k].normal.copy()))
        prev = b[0]
    return segs


def rail_on(obj, runs, keys, models, spec, start):
    """One model every spacing_yd along each run, in list order.

    A catalogue, not a scatter. The question a gallery answers is *which of
    these do I want*, which needs one readable example at a known place; a
    density answers *how does a mass of them read*, which is a different
    question and why the scatter surfaces exist separately.

    Returns the placements and how far through `keys` it reached, so one list
    spans every run on the map without a model ever appearing twice."""
    me = obj.data
    spacing = float(spec["spacing_yd"])
    margin = float(spec.get("edge_margin_yd", 0.0))
    push = float(spec.get("push_yd", 0.0))
    out, i = [], start
    for run in runs:
        travelled, next_at = 0.0, spacing / 2.0
        for a, b, n in run_segments(me, run):
            width = (b[0] - a[0]).length
            while width > 0.0 and next_at <= travelled + width and i < len(keys):
                u = (next_at - travelled) / width
                xy = a[0].lerp(b[0], u)
                z_bot = a[1] + (b[1] - a[1]) * u + margin
                z_top = a[2] + (b[2] - a[2]) * u - margin
                m = models[keys[i]]
                lo, hi = m["box"][2] * m["scale"], m["box"][3] * m["scale"]
                out.append((xy.x + n.x * push, xy.y + n.y * push,
                            (z_bot + z_top - (hi - lo)) / 2.0 - lo,
                            math.degrees(math.atan2(n.y, n.x)), keys[i],
                            m["scale"]))
                i += 1
                next_at += spacing
            travelled += width
    return out, i


def rail_on_ledge(obj, runs, keys, models, spec, start, seed):
    """rail_on's horizontal sibling: one model every spacing_yd along a terrace
    shelf, standing on it.

    Separate from rail_on for the reason scatter_on_faces is separate from
    scatter_on -- a horizontal surface stands a model on its ORIGIN, so a
    lamppost's sunk base plate is buried and is not height, the same way a
    tree's root flare is not. Yaw points from the riser foot out over the lip,
    so an arm reaches across the drop rather than into the wall.

    Placed on the centreline and never rejected for width. A footprint test
    reads the model at its widest point, which on a tree is the canopy, not the
    roots; over a 12-20 yd drop that overhang is the thing worth having, and a
    catalogue entry silently dropped shows you nothing. The count wider than its
    own shelf is returned instead."""
    me = obj.data
    if not keys:
        return [], start, 0
    spacing = float(spec["spacing_yd"])
    sink = float(spec.get("sink_yd", 0.0))
    mul = float(spec.get("scale", 1.0))
    # A catalogue STOPS when its list runs out -- that exhaustion is what makes
    # one model per slot answer "which of these do I want". A fill runs past the
    # end, in list order or drawn per slot.
    shuffle = bool(spec.get("shuffle", False))
    cycle = bool(spec.get("cycle", False)) or shuffle
    out, wide, i = [], 0, start
    for run in runs:
        travelled, next_at = 0.0, spacing / 2.0
        for a, b, _n in run_segments(me, run, ledge_columns):
            mid_a, mid_b = (a[0] + a[1]) / 2.0, (b[0] + b[1]) / 2.0
            width = (mid_b - mid_a).length
            while (width > 0.0 and next_at <= travelled + width
                   and (cycle or i < len(keys))):
                u = (next_at - travelled) / width
                xy = mid_a.lerp(mid_b, u)
                across = a[0].lerp(b[0], u) - a[1].lerp(b[1], u)
                # Seeded PER SLOT, not from a running generator: the draw must
                # not depend on how runs split across objects, and must never
                # come from dress()'s shared rng, which would re-roll every
                # scatter downstream of it.
                j = (random.Random(seed * 1000003 + i).randrange(len(keys))
                     if shuffle else i % len(keys))
                key = keys[j]
                m = models[key]
                scale = m["scale"] * mul
                if max(m["box"][0], m["box"][1]) * scale > across.length:
                    wide += 1
                out.append((xy.x, xy.y, a[2] + (b[2] - a[2]) * u - sink,
                            math.degrees(math.atan2(across.y, across.x)),
                            key, scale))
                i += 1
                next_at += spacing
            travelled += width
    return out, i, wide


def placement(models, key, x, y, z, yaw, scale):
    return {"model": key,
            "pos": [round(x, 3), round(y, 3), round(z, 3)],
            "yaw_deg": round(yaw, 1),
            "scale": round(scale, 5)}


def place_explicit(key, anchor, push, models, walls):
    """One model at one place: the nearest riser face to the anchor carries it.

    Scatter is the right tool for mass and the wrong one for intent -- it cannot
    put a specific model at a specific spot, and marking a base is exactly that.
    Only the anchor is hand-authored; yaw, the height within the band and the
    push out of the wall are all read off whichever face it lands on, so a
    retessellation moves the banner with the wall instead of stranding it."""
    want = Vector(anchor)
    best = None
    for obj, spans in walls:
        me = obj.data
        for band in WALL_RISERS:
            for k in band_faces(me, spans, band):
                cols = face_columns(me, me.polygons[k])
                if cols is None:
                    continue
                a, b = cols
                seg = b[0] - a[0]
                sl = seg.length
                u = 0.0 if sl == 0 else max(0.0, min(1.0,
                                                     (want - a[0]).dot(seg) / (sl * sl)))
                xy = a[0].lerp(b[0], u)
                d = (want - xy).length
                if best is None or d < best[0]:
                    best = (d, xy, u, a, b, me.polygons[k].normal.copy(),
                            obj.name, band, me.polygons[k].area)
    if best is None:
        return None
    d, xy, u, a, b, n, name, band, area = best
    z_bot = a[1] + (b[1] - a[1]) * u
    z_top = a[2] + (b[2] - a[2]) * u
    m = models[key]
    lo, hi = m["box"][2] * m["scale"], m["box"][3] * m["scale"]
    return ((xy.x + n.x * push, xy.y + n.y * push,
             (z_bot + z_top - (hi - lo)) / 2.0 - lo,
             math.degrees(math.atan2(n.y, n.x)), m["scale"]),
            {"object": name, "band": band, "off_yd": d, "area": round(area)})


def dress(dressing, outer, islands, heights, base_yd, rng, seed):
    """Doodad placements in the SERVER frame. `outer` and `islands` are
    (object, spans) pairs -- the spans are what tells a riser from the buried
    skirt, which nothing about the geometry recovers.

    geometry.json's frame is the world frame on map 900, so these are directly
    what `.gps` reads back; blender_staging_setup.py applies the model-frame turn
    when it instantiates them.

    Horizontal surfaces run first and in their original order, so adding a wall
    surface leaves every tree exactly where the previous build put it.
    `base_walls` runs last and CLAIMS its faces -- the scatter surfaces are handed
    the complement, so a gallery wall never also carries dressing.
    `terrace_ledges` draws no rng at all, so it can be added or dropped without
    moving anything else."""
    models = dressing["resolved"]["models"]
    fams = dressing["resolved"]["families"]
    min_nz = math.cos(math.radians(float(dressing["max_slope_deg"])))
    surfaces = dressing["surfaces"]
    objs = {"outer_plateau": [outer], "island_caps": islands,
            "outer_wall": [outer], "island_walls": islands}
    gallery = "base_walls" in surfaces

    out, legend = [], []
    for which in ("outer_plateau", "island_caps", "outer_wall", "island_walls"):
        if which not in surfaces:
            continue
        spec = surfaces[which]
        density = float(spec["per_1000_yd2"])
        sink = float(spec.get("sink_yd", 0.0))
        bands = (spec["families"] if isinstance(spec["families"], dict)
                 else {b: spec["families"] for b in WALL_RISERS})
        for i, (obj, spans) in enumerate(objs[which]):
            if which not in ("outer_wall", "island_walls"):
                fam = pick(spec["families"], i)
                area = flat_area(obj.data, min_nz)
                want = int(area / 1000.0 * density)
                pts = scatter_on(obj, want, spec, fams[fam], models, rng, min_nz)
                for x, y, z, key, scale in pts:
                    out.append(placement(models, key, x, y, z - sink,
                                         rng.uniform(0.0, 360.0), scale))
                legend.append({"surface": which, "object": obj.name, "band": "",
                               "family": fam, "area_yd2": round(area),
                               "want": want, "placed": len(pts)})
                continue
            for band in WALL_RISERS:
                idx = band_faces(obj.data, spans, band)
                if gallery:
                    _claimed, idx = base_facing(obj.data, idx, heights, base_yd)
                if not idx:               # an unterraced prism has no `upper`
                    continue
                fam = pick(bands[band], i)
                area = sum(obj.data.polygons[k].area for k in idx)
                want = int(area / 1000.0 * density)
                pts = scatter_on_faces(obj, idx, want, spec, fams[fam],
                                       models, rng)
                for x, y, z, yaw, key, scale in pts:
                    out.append(placement(models, key, x, y, z, yaw, scale))
                legend.append({"surface": which, "object": obj.name, "band": band,
                               "family": fam, "area_yd2": round(area),
                               "want": want, "placed": len(pts)})

    if gallery:
        spec = surfaces["base_walls"]
        flat = spec["families"]
        flat = [flat] if isinstance(flat, str) else list(flat)
        keys = [k for fam in flat for k in fams[fam]]
        i, claimed_area = 0, 0.0
        for obj, spans in [outer] + islands:
            for band in WALL_RISERS:
                at, _rest = base_facing(obj.data,
                                        band_faces(obj.data, spans, band),
                                        heights, base_yd)
                if not at:
                    continue
                area = sum(obj.data.polygons[k].area for k in at)
                claimed_area += area
                pts, i = rail_on(obj, contiguous(at), keys, models, spec, i)
                for x, y, z, yaw, key, scale in pts:
                    out.append(placement(models, key, x, y, z, yaw, scale))
                legend.append({"surface": "base_walls", "object": obj.name,
                               "band": band, "family": "rail",
                               "area_yd2": round(area),
                               "want": len(pts), "placed": len(pts)})
        legend.append({"surface": "base_walls", "object": "(catalogue)",
                       "band": "", "family": "%d slots" % i,
                       "area_yd2": round(claimed_area),
                       "want": len(keys), "placed": i})

    # Rails sharing the terrace shelves, discovered by prefix -- gen_blockout.py's
    # whitelist decides which names are legal, so a new one needs no edit here.
    # `scope` is what lets the island shelves and the perimeter's carry different
    # mixes: one surface class, read at completely different distances.
    for which in sorted(s for s in surfaces if s.startswith("terrace_ledge")):
        spec = surfaces[which]
        flat = spec["families"]
        flat = [flat] if isinstance(flat, str) else list(flat)
        keys = [k for fam in flat for k in fams[fam]]
        i, shelf_area, wide = 0, 0.0, 0
        # Islands before the outer ring, which is the one ordering decision here:
        # a catalogue runs out, and the islands are what a walk actually visits.
        scope = spec.get("scope", "all")
        for obj, spans in ({"islands": islands, "outer": [outer]}
                           .get(scope, islands + [outer])):
            runs = contiguous(band_faces(obj.data, spans, "ledge"))
            if not runs:                  # an unterraced prism has no shelf
                continue
            area = sum(obj.data.polygons[k].area for r in runs for k in r)
            shelf_area += area
            pts, i, over = rail_on_ledge(obj, runs, keys, models, spec, i, seed)
            wide += over
            for x, y, z, yaw, key, scale in pts:
                out.append(placement(models, key, x, y, z, yaw, scale))
            legend.append({"surface": which, "object": obj.name,
                           "band": "ledge", "family": "rail",
                           "area_yd2": round(area),
                           "want": len(pts), "placed": len(pts)})
        cycled = bool(spec.get("cycle") or spec.get("shuffle"))
        legend.append({"surface": which,
                       "object": "(fill)" if cycled else "(catalogue)",
                       "band": "", "family": "%d slots, %d wider than the shelf"
                       % (i, wide),
                       "area_yd2": round(shelf_area),
                       "want": i if cycled else len(keys), "placed": i})

    for key, anchor, push in dressing["resolved"].get("anchors", []):
        got = place_explicit(key, anchor, push, models, [outer] + islands)
        if got is None:
            continue
        (x, y, z, yaw, scale), info = got
        out.append(placement(models, key, x, y, z, yaw, scale))
        legend.append({"surface": "placement", "object": info["object"],
                       "band": info["band"], "family": "%s +%.1f yd"
                       % (key, info["off_yd"]),
                       "area_yd2": info["area"], "want": 1, "placed": 1})
    return out, legend


# --------------------------------------------------------------- finishing

def riser_uvs(spans, loops, scale_of):
    """face index -> (xy_i, u_i, xy_j, u_j), u already in tile units.

    Shell.strip appends one quad per loop segment in order, so a span's k-th face
    is segment k. The tile count is ROUNDED to a whole number: a riser is a
    closed ring, and an arc length that is not a whole number of tiles leaves a
    visible seam at the wrap. Rounding absorbs it into a sub-percent change in
    the effective scale instead."""
    out = {}
    for nm, start, end in spans:
        if nm not in loops:
            continue
        pts, arc, total = loops[nm]
        n = len(pts)
        if total <= 0 or end - start != n:
            continue
        tiles = max(1, round(total / scale_of[nm]))
        for i in range(n):
            j = (i + 1) % n
            out[start + i] = (pts[i], arc[i] / total * tiles,
                              pts[j], (arc[j] if j else total) / total * tiles)
    return out


def vertex_jitter(seed, vi, amount):
    """Deterministic offset in tile units, keyed on the VERTEX index so every
    face touching it moves together. Per-face jitter would hard-cut the texture
    at every shared edge, which reads worse than the lattice it breaks."""
    if not amount:
        return 0.0, 0.0
    r = random.Random(seed * 1000003 + vi)
    return r.uniform(-amount, amount), r.uniform(-amount, amount)


def assign_uv(obj, slots, seed, risers=None):
    """World-space cube projection, tiling at each material's own rate. A
    masonry texture depicts a known real-world span, so one global rate cannot
    serve both it and a ground texture. WBS wants the layer named UVMap.

    `risers` replaces u with arc length along the loop on wall riser faces. Cube
    projection reads u off whichever world axis the face normal points down, so
    on a curving ring the texture direction flips 90 degrees wherever that axis
    changes and stretches by 1/cos between the flips."""
    mesh = obj.data
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bm.verts.index_update()
    uv = bm.loops.layers.uv.new("UVMap")
    for fi, face in enumerate(bm.faces):
        idx = face.material_index
        _, scale, jit = slots[idx] if idx < len(slots) else slots[0]
        arc = risers.get(fi) if risers else None
        if arc is None:
            nrm = face.normal
            axis = max(range(3), key=lambda i: abs(nrm[i]))
        for loop in face.loops:
            co = loop.vert.co
            if arc is not None:
                p_i, u_i, p_j, u_j = arc
                near_i = ((co.x - p_i[0]) ** 2 + (co.y - p_i[1]) ** 2
                          <= (co.x - p_j[0]) ** 2 + (co.y - p_j[1]) ** 2)
                u, v = (u_i if near_i else u_j), co.z / scale
            elif axis == 0:
                u, v = co.y / scale, co.z / scale
            elif axis == 1:
                u, v = co.x / scale, co.z / scale
            else:
                u, v = co.x / scale, co.y / scale
            du, dv = vertex_jitter(seed, loop.vert.index, jit)
            loop[uv].uv = (u + du, v + dv)
    bm.to_mesh(mesh)
    bm.free()


def swatch(i, n):
    """Viewport colour only -- WMO ships the texture path, never this. Spread in
    hue so the material assignment is checkable by eye in the 5.1 blend, which is
    the only preview there is: the staging scene never loads a BLP."""
    return colorsys.hsv_to_rgb(i / max(n, 1), 0.45, 0.55)


def material(name, colour):
    mat = bpy.data.materials.get(name)
    if mat is None:
        mat = bpy.data.materials.new(name)
        mat.use_nodes = True
        bsdf = mat.node_tree.nodes.get("Principled BSDF")
        if bsdf:
            bsdf.inputs["Base Color"].default_value = (*colour, 1.0)
    return mat


def floor_class_of(mesh, poly, base_yd):
    """The one place the height bands are decided -- assignment and the report
    both read it, so they cannot drift apart."""
    z = sum(mesh.vertices[i].co.z for i in poly.vertices) / len(poly.vertices)
    if z <= FLOOR_CLASS_TOL_YD:
        return "lane"
    if z >= base_yd - FLOOR_CLASS_TOL_YD:
        return "base"
    return "ramp"


def assign_floor_slots(mesh, base_yd, slot_of):
    """Material slot per face, by the height it sits at.

    Read off the chunk's own geometry rather than carried through chunk_floor by
    polygon index -- mesh.validate() may drop a degenerate face, and a shifted
    index would mis-texture every face after it with nothing to show for it."""
    for p in mesh.polygons:
        p.material_index = slot_of[floor_class_of(mesh, p, base_yd)]


def assign_wall_bands(mesh, spans, slot_of):
    """Material slot per face, from build_wall's record of which ring pair made
    it. Unlike the floor there is no height rule that recovers this -- the ledge
    z varies per point along the loop, so face order is the only record."""
    for name, start, end in spans:
        slot = slot_of.get(name, 0)
        for i in range(start, min(end, len(mesh.polygons))):
            mesh.polygons[i].material_index = slot


def pick(spec, i):
    """A surfaces value is one material key, or a list cycled over an index."""
    return spec if isinstance(spec, str) else spec[i % len(spec)]


def wall_spec(spec):
    """A bare name means that material on every band."""
    return {b: spec for b in WALL_BANDS} if isinstance(spec, str) else spec


def dedupe_slots(chosen, registry):
    """chosen: class -> material key. Returns the slot list and class -> slot
    index, with two classes naming the same material sharing one slot -- WBS
    emits a batch per slot, so a duplicate slot is a duplicate batch for no gain."""
    slots, seen, index = [], {}, {}
    for cls, key in chosen.items():
        if key not in seen:
            seen[key] = len(slots)
            slots.append(registry[key])
        index[cls] = seen[key]
    return slots, index


def finish(obj, slots, collide, collection, classify=None, risers=None, seed=0):
    """slots: [(material, uv_scale, uv_jitter)] in material-index order. `classify`
    runs after the slots exist and before the UVs, which need the indices."""
    for mat, _, _ in slots:
        obj.data.materials.append(mat)
    if classify is not None:
        classify(obj.data)
    assign_uv(obj, slots, seed, risers)
    # Does NOT reach the staging scene -- custom properties do not survive OBJ
    # (README step 2). The real carrier is the object NAME, matched against
    # blender_staging_setup.py's COLLIDE set. This only drives the gate below.
    obj["wmo_collide"] = bool(collide)
    collection.objects.link(obj)


def centroid(pts):
    """geometry.json's frame is the world frame on map 900 -- no offset, rotation
    or axis swap -- so this is directly what `.gps` reads back in game."""
    return [round(sum(p[0] for p in pts) / len(pts), 1),
            round(sum(p[1] for p in pts) / len(pts), 1)]


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
    base_yd = geom["elevation"]["base_yd"]

    bpy.ops.wm.read_factory_settings(use_empty=True)
    coll = bpy.data.collections.new(bo["export_collection"])
    bpy.context.scene.collection.children.link(coll)

    rng = random.Random(bo["seed"])
    outer = [l for l in geom["loops"] if l["kind"] == "outer"]
    holes = [l for l in geom["loops"] if l["kind"] == "hole"]
    if len(outer) != 1:
        raise SystemExit(f"expected 1 outer loop, geometry.json has {len(outer)}")

    specs = bo["materials"]
    order = sorted(specs)
    default_uv = float(bo["uv_scale_yd"])
    default_jitter = float(bo.get("uv_jitter", 0.0))
    registry = {k: (material(MAT_PREFIX + k, swatch(i, len(order))),
                    float(specs[k].get("uv_scale_yd", default_uv)),
                    float(specs[k].get("uv_jitter", default_jitter)))
                for i, k in enumerate(order)}
    with open(cfg["out_materials"], "w") as fh:
        json.dump({MAT_PREFIX + k: specs[k]["texture"] for k in order},
                  fh, indent=2, sort_keys=True)

    surf = bo["surfaces"]

    sheet, seed_tris = build_floor([outer[0]] + holes, heights,
                                   bo["floor_edge_yd"], bo["floor_detail_edge_yd"])
    chunks, grid = chunk_floor(sheet, FLOOR_CHUNK_TARGET)
    bpy.data.meshes.remove(sheet)           # the unsplit sheet is not shipped
    floors, floor_legend = [], []
    for i, (name, chunk) in enumerate(chunks):
        chosen = {c: pick(surf["floor"][c], i) for c in FLOOR_CLASSES}
        slots, slot_of = dedupe_slots(chosen, registry)
        obj = bpy.data.objects.new(name, chunk)
        finish(obj, slots, True, coll, seed=bo["seed"],
               classify=lambda me, m=slot_of: assign_floor_slots(me, base_yd, m))
        floors.append(obj)
        floor_legend.append({"object": name, "materials": chosen})

    outer_spec = wall_spec(surf["outer_wall"])
    island_spec = wall_spec(surf["island_walls"])
    walls, flats, legend, desync = [], 0, [], []
    jobs = [("TT_OuterWall", outer[0]["points"], wall_cfg["outer_margin_yd"],
             outer_spec, 0)]
    jobs += [("TT_JWall_%02d" % i, h["points"], None, island_spec, i)
             for i, h in enumerate(holes)]
    for name, pts, thickness, spec, idx in jobs:
        floor_z = [heights.at_max(x, y) for x, y in pts]
        rise = height_profile(pts, wall_cfg, rng)
        top_z = [f + r for f, r in zip(floor_z, rise)]
        obj, flat, spans, lost, riser_loops = build_wall(
            name, pts, floor_z, top_z, thickness, wall_cfg)
        flats += int(flat)
        if lost:
            desync.append(name)
        chosen = {b: pick(spec[b], idx) for b in WALL_BANDS}
        slots, band_slot = dedupe_slots(chosen, registry)
        span_slot = {nm: band_slot[b]
                     for b, names in WALL_BANDS.items() for nm in names}
        finish(obj, slots, True, coll, seed=bo["seed"],
               risers=riser_uvs(spans, riser_loops,
                                {b: registry[chosen[b]][1] for b in WALL_BANDS}),
               classify=lambda me, s=spans, m=span_slot: assign_wall_bands(me, s, m))
        walls.append((obj, spans))
        legend.append({"object": name, "centre_yd": centroid(pts),
                       "lower": chosen["lower"], "upper": chosen["upper"]})

    doodads, doodad_legend = dress(bo["dressing"], walls[0], walls[1:],
                                   heights, base_yd, rng, bo["seed"])
    with open(cfg["out_doodads"], "w") as fh:
        json.dump({"frame": "server",
                   "models": {k: {"path": v["path"], "box": v["box"]}
                              for k, v in bo["dressing"]["resolved"]["models"].items()},
                   "placements": doodads}, fh, indent=1, sort_keys=True)

    for obj in list(coll.objects):
        obj.data.transform(obj.matrix_world)
        obj.matrix_world.identity()

    bpy.ops.wm.save_as_mainfile(filepath=cfg["out_blend"])

    objs = list(coll.objects)
    floor_names = {o.name for o in floors}
    zs = [v.co.z for o in objs for v in o.data.vertices]
    class_faces = {c: 0 for c in FLOOR_CLASSES}
    for o in floors:
        for p in o.data.polygons:
            class_faces[floor_class_of(o.data, p, base_yd)] += 1
    print("BUILD " + json.dumps({
        "objects": len(objs),
        "verts": sum(len(o.data.vertices) for o in objs),
        "faces": sum(len(o.data.polygons) for o in objs),
        "materials": len(order),
        "floor_chunks": len(floors),
        "floor_grid": [grid[0], grid[1]],
        "floor_chunk_max_tris": max(tri_count(o.data) for o in floors),
        "floor_class_faces": class_faces,
        "floor_legend": floor_legend,
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
        "max_batch_tris": max(max_batch_tris(o.data) for o in objs),
        "over_batch_tris": [o.name for o in objs
                            if max_batch_tris(o.data) > BATCH_TRI_CEILING],
        "walls": len(walls),
        "walls_unterraced": flats,
        "wall_legend": legend,
        "wall_span_desync": desync,
        "offset_vertices_clamped": CLAMPED[0],
        "doodads": len(doodads),
        "doodad_legend": doodad_legend,
        "doodad_models": len({d["model"] for d in doodads}),
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
