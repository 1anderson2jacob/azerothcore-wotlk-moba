#!/usr/bin/env python3
"""
MOBA blockout generator.

Reads per-map configs (apps/moba/maps/<mode>/map_source.yaml) and drives two
Blender-side tools in sequence:

  trace   wmo/blockout_trace.py   the painted PNG -> geometry.json, heights.png,
                                  boundary_preview.png, into the same bundle
  build   wmo/build_blockout.py   those           -> var/blender/<mode>_blockout.blend

The split exists because Blender's python has numpy but no yaml, while the
system python has yaml but no numpy. This half resolves config and gates the
results; those halves touch pixels and meshes.

Usage (from the repo root):
    python3 apps/moba/gen_blockout.py
    python3 apps/moba/gen_blockout.py --stage trace
    BLENDER=/path/to/blender python3 apps/moba/gen_blockout.py

See apps/moba/README.md for the full workflow and config field reference.
"""

import json
import os
import re
import subprocess
import sys
import tempfile
import yaml
from pathlib import Path

MAPS_DIR = Path(__file__).parent / "maps"
TRACE_TOOL = Path(__file__).parent / "wmo" / "blockout_trace.py"
BUILD_TOOL = Path(__file__).parent / "wmo" / "build_blockout.py"
BLEND_DIR = Path("var/blender")
DEFAULT_BLENDER = "/Applications/Blender.app/Contents/MacOS/Blender"

# mmaps' own limits, from src/tools/mmaps_generator/Config.h. A ramp steeper than
# the first is not walkable at all; a platform lower than the second is climbable
# from every side, which makes its edge decorative rather than a barrier.
WALKABLE_SLOPE_DEG = 60.0
WALKABLE_CLIMB_YD = 1.60

# A WMO group indexes its vertices with 16 bits, so a group cannot carry more.
VERTEX_CEILING = 65535

# ...and its render batch counts MOVI indices in 16 bits, which binds first: the
# floor hit this at 47% of the vertex cap. Over it, the client draws a wrapped
# fraction of the group and nothing anywhere reports a problem.
BATCH_TRI_CEILING = 65535 // 3

# Floor material slots, in material-index order. build_blockout.py assigns the
# same order; the two files cannot share a constant across the yaml gap.
FLOOR_CLASSES = ("lane", "base", "ramp")

# Blender material names travel to the 3.4 staging scene through the OBJ's .mtl,
# so a key that is not plain identifier text may not survive the round trip.
MATERIAL_NAME = re.compile(r"^[A-Za-z0-9_]+$")

# Wall face bands. build_blockout.py maps each to a set of named face spans.
WALL_BANDS = ("lower", "upper")

# Dressing surfaces build_blockout.py knows how to place onto. The top/wall
# split is not cosmetic: a horizontal surface stands a model on its origin and a
# vertical one does not, which is why probe_models reads how to measure a family
# from this and why one family cannot serve both.
DRESSING_TOP_SURFACES = ("outer_plateau", "island_caps", "terrace_ledges",
                         "terrace_ledge_trees", "terrace_ledge_trees_outer")
DRESSING_WALL_SURFACES = ("outer_wall", "island_walls")
# Railed rather than scattered: a fixed pitch along a run, so these validate on
# spacing_yd instead of a density. Every terrace_ledge* surface is in BOTH
# tuples -- railed, and horizontal, which is how probe_models learns to measure
# its families. base_walls and terrace_ledges are catalogues and come out once a
# look is chosen; the tree fills stay.
DRESSING_RAIL_SURFACES = ("base_walls", "terrace_ledges", "terrace_ledge_trees",
                          "terrace_ledge_trees_outer")
DRESSING_SURFACES = tuple(dict.fromkeys(DRESSING_TOP_SURFACES
                                        + DRESSING_WALL_SURFACES
                                        + DRESSING_RAIL_SURFACES))

# The storm binding is a cpython-310 .so, so probing models needs that python
# specifically -- the same constraint apps/moba/wmo/README.md states for the
# standalone tools.
PY310 = os.environ.get("PY310", "python3.10")
MPQ_TOOL = Path(__file__).parent / "wmo" / "mpq_tool.py"

MIRROR_IOU_WARN = 0.90
STAGES = ("trace", "build", "all")

REQUIRED = ("trace", "map_width_yd", "palette", "alpha_threshold",
            "elevation", "smoothing", "blockout")


def fail(msg):
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def note(msg):
    print(f"  {msg}")


# ---------------------------------------------------------------- validation

def parse_hex(value, path, field):
    text = str(value).lstrip("#")
    if len(text) != 6:
        fail(f'{path}: palette "{field}" must be a #RRGGBB hex string, got {value!r}')
    try:
        return [int(text[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    except ValueError:
        fail(f'{path}: palette "{field}" is not valid hex: {value!r}')


def require(block, keys, path, prefix):
    for key in keys:
        if key not in block:
            fail(f'{path}: missing "{prefix}{key}"')


def material_refs(spec):
    """A surfaces value is one material key or a list of them, cycled."""
    return [spec] if isinstance(spec, str) else list(spec)


def validate_materials(bo, path):
    mats = bo["materials"]
    if not isinstance(mats, dict) or not mats:
        fail(f'{path}: "blockout.materials" must be a non-empty mapping')
    for key, spec in mats.items():
        if not MATERIAL_NAME.match(str(key)):
            fail(f'{path}: material name {key!r} must be letters, digits and '
                 "underscores -- it becomes a Blender material name and crosses "
                 "into the staging scene through the OBJ's .mtl")
        if not isinstance(spec, dict) or "texture" not in spec:
            fail(f'{path}: material {key!r} needs a "texture"')
        tex = str(spec["texture"])
        if tex != tex.lower():
            fail(f'{path}: material {key!r} texture must be lowercase -- the '
                 "BLP->PNG import does a .replace('.blp', ...) that misses .BLP")
        if not tex.endswith(".blp"):
            fail(f'{path}: material {key!r} texture must end .blp, got {tex!r}')
        if tex.endswith("_s.blp"):
            fail(f'{path}: material {key!r} points at {tex!r} -- a _s suffix is a '
                 "specular map and is never a diffuse texture")
        if "uv_scale_yd" in spec:
            scale = spec["uv_scale_yd"]
            if not isinstance(scale, (int, float)) or scale <= 0:
                fail(f'{path}: material {key!r} "uv_scale_yd" must be positive')

    surf = bo["surfaces"]
    require(surf, ("floor", "outer_wall", "island_walls"),
            path, "blockout.surfaces.")
    if set(surf["floor"]) != set(FLOOR_CLASSES):
        fail(f'{path}: "blockout.surfaces.floor" must name exactly '
             + ", ".join(FLOOR_CLASSES))

    refs = []
    for cls in FLOOR_CLASSES:
        got = material_refs(surf["floor"][cls])
        if not got:
            fail(f'{path}: "blockout.surfaces.floor.{cls}" must not be empty')
        refs += got
    for which in ("outer_wall", "island_walls"):
        spec = surf[which]
        if isinstance(spec, str):
            refs.append(spec)
            continue
        if set(spec) != set(WALL_BANDS):
            fail(f'{path}: "blockout.surfaces.{which}" must be one material name, '
                 f'or a mapping of exactly {", ".join(WALL_BANDS)}')
        for band in WALL_BANDS:
            got = material_refs(spec[band])
            if not got:
                fail(f'{path}: "blockout.surfaces.{which}.{band}" must not be empty')
            # A list cycles over OBJECTS and the outer wall is always exactly one,
            # so entries past the first would be dropped in silence -- a uniform
            # perimeter that reads as a broken build rather than an unsupported
            # config. Islands cycle on purpose; this surface cannot.
            if which == "outer_wall" and len(got) > 1:
                fail(f'{path}: "blockout.surfaces.outer_wall.{band}" names '
                     f'{len(got)} materials, but the outer wall is one object -- '
                     "only the first would be used. Name a single material.")
            refs += got
    for name in refs:
        if name not in mats:
            fail(f'{path}: "blockout.surfaces" references unknown material {name!r}')


def family_refs(spec):
    """A dressing `families` value: one name, a list cycled over the surface's
    objects, or -- on a wall surface -- either of those per band."""
    if isinstance(spec, dict):
        return [n for band in spec.values() for n in material_refs(band)]
    return material_refs(spec)


def family_kinds(dr):
    """family name -> the set of surface kinds referencing it. probe_models reads
    this to know how to measure the family, so a name in both is a config error
    rather than a choice."""
    kinds = {}
    for which, spec in dr["surfaces"].items():
        kind = "top" if which in DRESSING_TOP_SURFACES else "wall"
        for name in family_refs(spec.get("families", [])):
            kinds.setdefault(name, set()).add(kind)
    return kinds


def validate_dressing(bo, path):
    dr = bo["dressing"]
    if not isinstance(dr, dict):
        fail(f'{path}: "blockout.dressing" must be a mapping')
    require(dr, ("families", "surfaces", "max_slope_deg"), path, "blockout.dressing.")

    fams = dr["families"]
    if not isinstance(fams, dict) or not fams:
        fail(f'{path}: "blockout.dressing.families" must be a non-empty mapping')
    for name, spec in fams.items():
        if not isinstance(spec, dict) or "models" not in spec or "height_yd" not in spec:
            fail(f'{path}: dressing family {name!r} needs "models" and "height_yd"')
        if not isinstance(spec["height_yd"], (int, float)) or spec["height_yd"] <= 0:
            fail(f'{path}: dressing family {name!r} "height_yd" must be positive')
        if not spec["models"]:
            fail(f'{path}: dressing family {name!r} has no models')
        for m in spec["models"]:
            if not str(m).lower().endswith(".m2"):
                fail(f'{path}: dressing family {name!r} model {m!r} must end .m2 '
                     "-- MODN takes the model, not an .mdx alias")

    surf = dr["surfaces"]
    if set(surf) - set(DRESSING_SURFACES):
        fail(f'{path}: "blockout.dressing.surfaces" may only name '
             + ", ".join(DRESSING_SURFACES))
    for which, spec in surf.items():
        rail = which in DRESSING_RAIL_SURFACES
        require(spec, ("families", "spacing_yd") if rail
                else ("families", "per_1000_yd2", "min_spacing_yd"),
                path, f"blockout.dressing.surfaces.{which}.")
        scope = spec.get("scope", "all")
        if scope not in ("all", "islands", "outer"):
            fail(f'{path}: dressing surface {which!r} "scope" must be one of '
                 "all, islands, outer")
        if isinstance(spec["families"], dict):
            if which not in DRESSING_WALL_SURFACES:
                fail(f'{path}: dressing surface {which!r} is horizontal and has no '
                     'bands, so its "families" cannot be a per-band mapping')
            if set(spec["families"]) != set(WALL_BANDS):
                fail(f'{path}: dressing surface {which!r} "families" must name '
                     f'exactly {", ".join(WALL_BANDS)}')
        for name in family_refs(spec["families"]):
            if name not in fams:
                fail(f'{path}: dressing surface {which!r} references unknown '
                     f"family {name!r}")
        if rail:
            if spec["spacing_yd"] <= 0:
                fail(f'{path}: dressing surface {which!r} wants a positive '
                     '"spacing_yd"')
            seen = [n for n in family_refs(spec["families"])]
            if len(seen) != len(set(seen)):
                fail(f'{path}: dressing surface {which!r} names a family twice -- '
                     "a catalogue places each model once, so a repeat silently "
                     "costs a slot and shows you nothing new")
        elif spec["min_spacing_yd"] <= 0 or spec["per_1000_yd2"] <= 0:
            fail(f'{path}: dressing surface {which!r} wants a positive '
                 '"per_1000_yd2" and "min_spacing_yd"')
        if spec.get("push_yd", 0.0) < 0:
            fail(f'{path}: dressing surface {which!r} "push_yd" moves a prop OUT '
                 "along the face normal and cannot be negative")

    for name, used in family_kinds(dr).items():
        if len(used) > 1:
            fail(f'{path}: dressing family {name!r} is used on both a wall and a '
                 "horizontal surface -- a cap buries everything below the model's "
                 "origin and a wall face buries nothing, so one height_yd cannot "
                 "mean both. Copy the family under a second name")


def validate_placements(dr, path):
    """Explicit single-model placements, resolved against the geometry rather
    than typed: an anchor names the intent and the wall supplies everything
    else."""
    places = dr.get("placements", [])
    if not isinstance(places, list):
        fail(f'{path}: "blockout.dressing.placements" must be a list')
    for i, spec in enumerate(places):
        where = f"blockout.dressing.placements[{i}]"
        if not isinstance(spec, dict):
            fail(f"{path}: {where} must be a mapping")
        require(spec, ("model", "height_yd", "anchor"), path, where + ".")
        if not str(spec["model"]).lower().endswith(".m2"):
            fail(f'{path}: {where} model {spec["model"]!r} must end .m2')
        if not isinstance(spec["height_yd"], (int, float)) or spec["height_yd"] <= 0:
            fail(f'{path}: {where} "height_yd" must be positive')
        a = spec["anchor"]
        if not (isinstance(a, list) and len(a) == 2
                and all(isinstance(v, (int, float)) for v in a)):
            fail(f'{path}: {where} "anchor" must be [x, y] in map yards -- the '
                 "nearest riser face to it is what carries the model")
        if spec.get("push_yd", 0.0) < 0:
            fail(f'{path}: {where} "push_yd" moves the model OUT along the face '
                 "normal and cannot be negative")


def probe_models(bo, path):
    """Every model's own bounding box, so each family's per-model scale is
    derived from the height the config asks for rather than typed.

    Shelled out because the storm binding is compiled against Blender 3.4's
    interpreter and this process is the system python -- the same split that
    makes the Blender-side tools take JSON."""
    dr = bo["dressing"]
    kinds = family_kinds(dr)
    wanted = []
    for _fam, spec in sorted(dr["families"].items()):
        for m in spec["models"]:
            key = str(m).lower()
            if key not in wanted:
                wanted.append(key)
    for spec in dr.get("placements", []):
        key = str(spec["model"]).lower()
        if key not in wanted:
            wanted.append(key)

    proc = subprocess.run([PY310, str(MPQ_TOOL), "probe", "--json"] + wanted,
                          capture_output=True, text=True)
    if proc.returncode != 0:
        tail = "\n".join((proc.stdout + proc.stderr).splitlines()[-12:])
        fail(f"{path}: mpq_tool.py probe failed -- is {PY310} the interpreter "
             f"pywowlib's storm binding was built for?\n{tail}")
    try:
        probed = json.loads(proc.stdout.strip().splitlines()[-1])
    except (ValueError, IndexError):
        fail(f"{path}: mpq_tool.py probe --json produced no JSON\n"
             + proc.stdout[-400:])

    models, families, legend = {}, {}, []
    missing, flat, no_collision, solid = [], [], [], []
    for fam, spec in sorted(dr["families"].items()):
        want_h = float(spec["height_yd"])
        wall = "wall" in kinds.get(fam, ())
        keys, scales = [], []
        for i, m in enumerate(spec["models"]):
            src = str(m).lower()
            rec = probed.get(src)
            if rec is None:
                missing.append(src)
                continue
            (rlo, rhi), (clo, chi) = rec["box_a"], rec["box_b"]
            # A cap stands the model on its origin, so whatever the author left
            # below z=0 is buried and is not height -- counting a tree's root
            # flare shrinks all 38 of them by up to 12%. A wall face buries
            # nothing, and a model authored to HANG puts every vertex below the
            # origin: azjol_hangingfern_01 tops out at 0.27, which read as a
            # height scales it 96x.
            z_lo, z_hi = (rlo[2], rhi[2]) if wall else (0.0, rhi[2])
            if z_hi - z_lo <= 0.0:
                flat.append(src)
                continue
            if rec["bound_tris"] and wall:
                solid.append(src)
            elif not rec["bound_tris"] and not wall:
                no_collision.append(src)
            key = f"{fam}_{i:02d}"
            # Footprint off the collision box, which zeroes out exactly when
            # bound_tris does -- and that is every model worth hanging on a wall,
            # so box_a is the fallback. box_a's xy is not tight (ghostlandstree04
            # claims 175 yd across), which is why it only ever sizes the viewport
            # proxy and never rejects a placement.
            box = (clo, chi) if (chi[0] - clo[0]) or (chi[1] - clo[1]) else (rlo, rhi)
            models[key] = {"path": src,
                           "scale": round(want_h / (z_hi - z_lo), 5),
                           # model units, so the export scale draws the proxy at
                           # the size the doodad ships at. z starts where the
                           # placement rule treats the model as starting: the
                           # origin on a cap, the geometry itself on a wall.
                           "box": [round(box[1][0] - box[0][0], 3),
                                   round(box[1][1] - box[0][1], 3),
                                   round(z_lo, 3),
                                   round(z_hi, 3)]}
            keys.append(key)
            scales.append(models[key]["scale"])
        families[fam] = keys
        if keys:
            legend.append({"family": fam, "height_yd": want_h, "models": len(keys),
                           "scale_lo": min(scales), "scale_hi": max(scales)})

    if missing:
        fail(f"{path}: dressing model not in the client MPQs: " + ", ".join(missing))
    if flat:
        fail(f"{path}: dressing model has a zero-height bounding box, so no "
             "scale can be derived from it: " + ", ".join(flat))
    for src in sorted(set(no_collision)):
        note(f"warn  {src} probes 0 bounding triangles -- renders in the client,"
             " dropped from the vmaps")
    for src in sorted(set(solid)):
        note(f"warn  {src} carries collision and is dressing a wall face --"
             " players run against those constantly, so it ships as an invisible"
             " bump mid-lane")
    places = dr.get("placements", [])
    anchors = []
    for i, spec in enumerate(places):
        src = str(spec["model"]).lower()
        rec = probed.get(src)
        if rec is None:
            fail(f"{path}: placement {i} model is not in the client MPQs: {src}")
        (rlo, rhi), (clo, chi) = rec["box_a"], rec["box_b"]
        z_lo, z_hi = rlo[2], rhi[2]        # a placement always lands on a riser
        if z_hi - z_lo <= 0.0:
            fail(f"{path}: placement {i} model has a zero-height bounding box, so "
                 f"no scale can be derived from it: {src}")
        if rec["bound_tris"]:
            note(f"warn  {src} carries collision and is placed on a wall face --"
                 " players run against those constantly, so it ships as an"
                 " invisible bump")
        box = (clo, chi) if (chi[0] - clo[0]) or (chi[1] - clo[1]) else (rlo, rhi)
        key = "place_%02d" % i
        models[key] = {"path": src,
                       "scale": round(float(spec["height_yd"]) / (z_hi - z_lo), 5),
                       "box": [round(box[1][0] - box[0][0], 3),
                               round(box[1][1] - box[0][1], 3),
                               round(z_lo, 3), round(z_hi, 3)]}
        anchors.append((key, list(spec["anchor"]), float(spec.get("push_yd", 0.3))))
    return {"models": models, "families": families, "legend": legend,
            "anchors": anchors}


def validate(cfg, path):
    require(cfg, REQUIRED, path, "")

    if not isinstance(cfg["map_width_yd"], (int, float)) or cfg["map_width_yd"] <= 0:
        fail(f'{path}: "map_width_yd" must be a positive number')

    if set(cfg["palette"]) != {"floor", "base", "ramp"}:
        fail(f'{path}: "palette" must name exactly floor, base and ramp')

    base_yd = cfg["elevation"].get("base_yd")
    if not isinstance(base_yd, (int, float)) or base_yd <= WALKABLE_CLIMB_YD:
        fail(f'{path}: "elevation.base_yd" must exceed walkableClimb '
             f"{WALKABLE_CLIMB_YD} yd, or the platform edge is climbable anywhere")

    sigma = cfg["smoothing"].get("sigma_yd")
    if not isinstance(sigma, (int, float)) or sigma <= 0:
        fail(f'{path}: "smoothing.sigma_yd" must be a positive number')

    bo = cfg["blockout"]
    require(bo, ("floor_edge_yd", "floor_detail_edge_yd", "bottom_yd", "wall",
                 "dressing", "materials", "surfaces", "uv_scale_yd",
                 "export_collection", "seed"),
            path, "blockout.")
    if bo["floor_detail_edge_yd"] >= bo["floor_edge_yd"]:
        fail(f'{path}: "blockout.floor_detail_edge_yd" must be finer than '
             '"blockout.floor_edge_yd" -- it is the target used across a slope')
    if bo["bottom_yd"] >= 0:
        fail(f'{path}: "blockout.bottom_yd" is the underside of the walls and '
             "must sit below the floor")

    wall = bo["wall"]
    require(wall, ("height_yd", "variation_yd", "segment_yd", "terrace_drop_yd",
                   "terrace_ledge_yd", "bevel_yd", "outer_margin_yd"),
            path, "blockout.wall.")
    shortest = wall["height_yd"] - wall["variation_yd"]
    if shortest <= WALKABLE_CLIMB_YD:
        fail(f'{path}: shortest wall is {shortest} yd, at or under walkableClimb '
             f"{WALKABLE_CLIMB_YD} -- creatures would path straight over it")
    if wall["terrace_drop_yd"] >= shortest:
        fail(f'{path}: "blockout.wall.terrace_drop_yd" ({wall["terrace_drop_yd"]}) '
             f"must be less than the shortest wall ({shortest} yd), or the ledge "
             "drops below the floor it stands on")

    validate_materials(bo, path)
    validate_dressing(bo, path)
    validate_placements(bo["dressing"], path)


# ------------------------------------------------------------------- running

def run_tool(tool, params, blender, marker, path):
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(params, fh)
        params_path = fh.name
    try:
        proc = subprocess.run(
            [blender, "--background", "--python", str(tool),
             "--", "--params", params_path],
            capture_output=True, text=True)
    finally:
        os.unlink(params_path)

    report = None
    for line in proc.stdout.splitlines():
        if line.startswith(marker + " "):
            report = json.loads(line[len(marker) + 1:])
    if report is None:
        tail = "\n".join((proc.stdout + proc.stderr).splitlines()[-25:])
        fail(f"{path}: {tool.name} produced no report\n{tail}")
    return report


def run_trace(cfg, path, out_dir, blender):
    trace = Path(cfg["trace"]).resolve()
    if not trace.is_file():
        fail(f'{path}: trace not found: {cfg["trace"]} (paths are repo-relative)')
    params = {
        "repo": str(Path.cwd()),
        "trace": str(trace),
        "out_dir": str(out_dir),
        "map_width_yd": float(cfg["map_width_yd"]),
        "palette": {k: parse_hex(v, path, k) for k, v in cfg["palette"].items()},
        "alpha_threshold": float(cfg["alpha_threshold"]),
        "base_yd": float(cfg["elevation"]["base_yd"]),
        "ramp_blur_yd": float(cfg["elevation"].get("ramp_blur_yd", 0.0)),
        "sigma_yd": float(cfg["smoothing"]["sigma_yd"]),
        "resample_yd": float(cfg["smoothing"]["resample_yd"]),
        "min_blob_yd2": float(cfg["smoothing"]["min_blob_yd2"]),
        "min_loop_yd2": float(cfg["smoothing"]["min_loop_yd2"]),
    }
    return run_tool(TRACE_TOOL, params, blender, "BLOCKOUT", path)


def run_build(cfg, path, out_dir, blender):
    geometry = out_dir / "geometry.json"
    heights = out_dir / "heights.png"
    for needed in (geometry, heights):
        if not needed.is_file():
            fail(f"{path}: {needed} missing -- run the trace stage first")
    BLEND_DIR.mkdir(parents=True, exist_ok=True)
    blend = (BLEND_DIR / f"{out_dir.name}_blockout.blend").resolve()
    # Texture paths and doodad placements cannot reach the 3.4 staging scene any
    # other way: custom properties do not survive the OBJ, and 3.4's python has
    # no yaml.
    materials = (BLEND_DIR / f"{out_dir.name}_materials.json").resolve()
    doodads = (BLEND_DIR / f"{out_dir.name}_doodads.json").resolve()
    blockout = dict(cfg["blockout"])
    blockout["dressing"] = dict(blockout["dressing"])
    blockout["dressing"]["resolved"] = probe_models(cfg["blockout"], path)
    params = {
        "geometry": str(geometry.resolve()),
        "heights": str(heights.resolve()),
        "out_blend": str(blend),
        "out_materials": str(materials),
        "out_doodads": str(doodads),
        "blockout": blockout,
    }
    report = run_tool(BUILD_TOOL, params, blender, "BUILD", path)
    report["_blend"] = str(blend)
    report["_materials"] = str(materials)
    report["_doodads"] = str(doodads)
    report["_dressing"] = blockout["dressing"]["resolved"]["legend"]
    return report


# --------------------------------------------------------------------- gates

def gate_trace(report):
    ok = True
    if report["floor_regions"] != 1:
        ok = False
        note(f'FAIL  {report["floor_regions"]} disconnected walkable regions, expected 1'
             " -- a paint gap seals part of the map off; check the ramps bridge"
             " floor and base rather than merely abutting them")
    if report["max_slope_deg"] >= WALKABLE_SLOPE_DEG:
        ok = False
        note(f'FAIL  steepest slope {report["max_slope_deg"]}deg reaches mmaps\''
             f" walkableSlopeAngle {WALKABLE_SLOPE_DEG}deg -- lengthen the ramps"
             " or lower elevation.base_yd")
    if report["mirror_iou"] < MIRROR_IOU_WARN:
        note(f'warn  mirror IoU {report["mirror_iou"]} -- the two halves disagree'
             " enough that the derived x origin may be off")
    return ok


def gate_build(report):
    ok = True
    if report["over_16bit_index"]:
        ok = False
        note("FAIL  over " + str(VERTEX_CEILING) + " vertices: "
             + ", ".join(report["over_16bit_index"])
             + " -- a WMO group indexes vertices with 16 bits;"
               " raise blockout.floor_edge_yd")
    if report["over_batch_tris"]:
        ok = False
        note("FAIL  over " + str(BATCH_TRI_CEILING) + " triangles in one batch: "
             + ", ".join(report["over_batch_tris"])
             + " -- WBS emits one batch per material per group and a batch cannot"
               " index more; the client would draw only part of the group")
    if report["no_uv"]:
        ok = False
        note("FAIL  no UVMap layer on " + ", ".join(report["no_uv"])
             + " -- WBS needs the layer named exactly UVMap")
    if report["empty_material_slots"]:
        ok = False
        note("FAIL  empty material slot on "
             + ", ".join(report["empty_material_slots"]))
    if report["nonmanifold_collide"]:
        note("warn  non-manifold collision solids: "
             + ", ".join(f"{k} ({v} edges)"
                         for k, v in report["nonmanifold_collide"].items()))
    if report["walls_unterraced"]:
        note(f'warn  {report["walls_unterraced"]} wall(s) too small to terrace,'
             " built as plain prisms")
    for d in report["doodad_legend"]:
        where = f'{d["object"]} {d["band"]}'.strip()
        if not d["area_yd2"]:
            ok = False
            note(f"FAIL  {where} offers no surface to dress -- its normals are"
                 " inverted, or dressing.max_slope_deg sits under the ledge's"
                 " own slope")
        elif d["want"] and d["placed"] < d["want"] * 0.6:
            note(f'warn  {where} took {d["placed"]} of {d["want"]} doodads'
                 " -- min_spacing_yd is saturating the surface before the"
                 " density is met")
    if report["wall_span_desync"]:
        ok = False
        note("FAIL  mesh.validate() dropped faces on "
             + ", ".join(report["wall_span_desync"])
             + " -- band spans index the face list as built, so every face after"
               " the drop now carries the wrong material")
    if report["floor_max_face_verts"] > 3:
        ok = False
        note(f'FAIL  floor carries a {report["floor_max_face_verts"]}-vertex face'
             " -- it is non-planar once z lands and tessellates arbitrarily")
    return ok


# ---------------------------------------------------------------------- main

def main():
    argv = sys.argv[1:]
    stage = "all"
    if "--stage" in argv:
        stage = argv[argv.index("--stage") + 1]
        if stage not in STAGES:
            fail(f"--stage must be one of {', '.join(STAGES)}")

    blender = os.environ.get("BLENDER", DEFAULT_BLENDER)
    if not Path(blender).is_file():
        fail(f"blender not found at {blender} -- set BLENDER to override")
    for tool in (TRACE_TOOL, BUILD_TOOL):
        if not tool.is_file():
            fail(f"missing {tool}")

    configs = sorted(MAPS_DIR.glob("*/map_source.yaml"))
    if not configs:
        fail(f"no map sources found under {MAPS_DIR}/*/map_source.yaml")

    failed = False
    for path in configs:
        cfg = yaml.safe_load(path.read_text())
        validate(cfg, path)
        out_dir = path.parent
        print(out_dir.name)

        if stage in ("trace", "all"):
            report = run_trace(cfg, path, out_dir, blender)
            note(f'trace        {cfg["map_width_yd"]} yd across'
                 f' {report["total_points"]} boundary points')
            note(f'  mirror IoU {report["mirror_iou"]},'
                 f' {report["ambiguous_px"]} ambiguous px,'
                 f' {report["blobs_dropped"]} specks dropped')
            note(f'  smoothing  max deviation'
                 f' {report["max_smoothing_deviation_yd"]} yd,'
                 f' area {report["area_change_pct"]:+.3f}%')
            note(f'  slope      {report["max_slope_deg"]}deg peak'
                 f" (limit {WALKABLE_SLOPE_DEG})")
            note("  wrote      geometry.json, heights.png, boundary_preview.png")
            if not gate_trace(report):
                failed = True
                continue

        if stage in ("build", "all"):
            report = run_build(cfg, path, out_dir, blender)
            note(f'build        {report["objects"]} objects,'
                 f' {report["verts"]} verts, {report["faces"]} faces,'
                 f' {report["materials"]} materials')
            note(f'  floor      {report["floor_chunks"]} chunks'
                 f' {report["floor_grid"][0]}x{report["floor_grid"][1]},'
                 f' {report["floor_verts"]} verts, {report["floor_faces"]} faces,'
                 f' {report["floor_area_yd2"]} yd2')
            note('  classes    ' + ", ".join(
                f'{k} {report["floor_class_faces"].get(k, 0)}'
                for k in FLOOR_CLASSES))
            for f in report["floor_legend"]:
                note("    %-14s %s" % (f["object"], "  ".join(
                    "%s=%s" % (k, f["materials"][k]) for k in FLOOR_CLASSES)))
            note(f'  batches    largest {report["max_batch_tris"]} tris'
                 f' (ceiling {BATCH_TRI_CEILING})')
            note(f'  walls      {report["walls"]},'
                 f' {report["offset_vertices_clamped"]} offset vertices clamped')
            for w in report["wall_legend"]:
                note("    %-14s (%7.1f, %7.1f)  lower=%-17s upper=%s"
                     % (w["object"], w["centre_yd"][0], w["centre_yd"][1],
                        w["lower"], w["upper"]))
            note(f'  dressing   {report["doodads"]} doodads,'
                 f' {len(report["_dressing"])} families,'
                 f' {report["doodad_models"]} models used')
            for f in report["_dressing"]:
                note("    %-20s %4.1f yd  %d models  scale %.3f .. %.3f"
                     % (f["family"], f["height_yd"], f["models"],
                        f["scale_lo"], f["scale_hi"]))
            for d in report["doodad_legend"]:
                note("    %-14s %-6s %-20s %7.0f yd2  want %4d  placed %4d"
                     % (d["object"], d["band"], d["family"], d["area_yd2"],
                        d["want"], d["placed"]))
            note(f'  faces      max span {report["floor_max_face_span_yd"]} yd,'
                 f' {report["floor_max_face_verts"]} verts')
            note(f'  z range    {report["z_range_yd"][0]} .. '
                 f'{report["z_range_yd"][1]} yd')
            note(f'  wrote      {report["_blend"]}')
            note(f'             {report["_materials"]}  (key -> texture path)')
            note(f'             {report["_doodads"]}  (doodad placements)')
            if not gate_build(report):
                failed = True

    if failed:
        sys.exit(1)


if __name__ == "__main__":
    main()
