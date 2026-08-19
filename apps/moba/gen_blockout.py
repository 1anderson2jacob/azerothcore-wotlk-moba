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
                 "scatter", "materials", "uv_scale_yd", "export_collection", "seed"),
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

    if set(bo["materials"]) != {"floor", "wall", "tree"}:
        fail(f'{path}: "blockout.materials" must name exactly floor, wall and tree')


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
    params = {
        "geometry": str(geometry.resolve()),
        "heights": str(heights.resolve()),
        "out_blend": str(blend),
        "blockout": cfg["blockout"],
    }
    report = run_tool(BUILD_TOOL, params, blender, "BUILD", path)
    report["_blend"] = str(blend)
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
        note("FAIL  over " + str(BATCH_TRI_CEILING) + " triangles: "
             + ", ".join(report["over_batch_tris"])
             + " -- one WMO render batch cannot index more; the client would"
               " draw only part of the group")
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
                 f' {report["verts"]} verts, {report["faces"]} faces')
            note(f'  floor      {report["floor_chunks"]} chunks'
                 f' {report["floor_grid"][0]}x{report["floor_grid"][1]},'
                 f' {report["floor_verts"]} verts, {report["floor_faces"]} faces,'
                 f' {report["floor_area_yd2"]} yd2')
            note(f'  batches    largest chunk'
                 f' {report["floor_chunk_max_tris"]} tris'
                 f' (ceiling {BATCH_TRI_CEILING})')
            note(f'  walls      {report["walls"]},'
                 f' {report["offset_vertices_clamped"]} offset vertices clamped')
            note(f'  faces      max span {report["floor_max_face_span_yd"]} yd,'
                 f' {report["floor_max_face_verts"]} verts')
            note(f'  z range    {report["z_range_yd"][0]} .. '
                 f'{report["z_range_yd"][1]} yd')
            note(f'  wrote      {report["_blend"]}')
            if not gate_build(report):
                failed = True

    if failed:
        sys.exit(1)


if __name__ == "__main__":
    main()
