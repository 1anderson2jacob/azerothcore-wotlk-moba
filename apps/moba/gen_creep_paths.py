#!/usr/bin/env python3
"""
MOBA lane waypoint-path generator.

Reads a human-owned lane config (walked centerline points + formation slot
offsets), densifies each lane to a max node spacing, generates one offset
path per formation slot per direction, and emits idempotent
DELETE+INSERT SQL for `waypoint_data`.

Path IDs are auto-assigned from the config's id_range on first use and then
persisted to a machine-owned lockfile next to the config
(<config-stem>.lock.json). Later runs reuse the locked IDs, so re-walking or
re-tuning a lane never changes which waypoint_data id a creature's
mod_moba_creep_data.WaypointPathId points at. Do not hand-edit the lockfile.

Usage (from the repo root):
    python3 apps/moba/gen_creep_paths.py [path/to/lane_config.json]
    python3 apps/moba/gen_creep_paths.py --extract scrollback.txt

--extract parses raw .gps console scrollback and prints a JSON points array
ready to paste into the config's "points" field.

See apps/moba/README.md for the full workflow and config field reference.
"""

import json
import math
import re
import sys
from pathlib import Path

DEFAULT_CONFIG = Path(__file__).parent / "lane_config.json"
COORD_FMT = "{:.4f}"
DEDUP_EPSILON = 0.5  # yards; consecutive walked points closer than this are merged

GPS_LINE_RE = re.compile(r"X:\s*(-?[\d.]+)\s+Y:\s*(-?[\d.]+)\s+Z:\s*(-?[\d.]+)")


def fail(msg):
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def note(msg):
    print(f"  {msg}")


# ---------------------------------------------------------------- extraction

def extract_points(text):
    """Pull [x, y, z] triples from raw .gps console scrollback."""
    return [[float(x), float(y), float(z)] for x, y, z in GPS_LINE_RE.findall(text)]


# ---------------------------------------------------------------- validation

def validate_config(cfg):
    id_range = cfg.get("id_range")
    if (not isinstance(id_range, list) or len(id_range) != 2
            or not all(isinstance(v, int) for v in id_range) or id_range[0] > id_range[1]):
        fail('config "id_range" must be [low, high] with low <= high')

    spacing = cfg.get("max_spacing")
    if not isinstance(spacing, (int, float)) or spacing <= 0:
        fail('config "max_spacing" must be a positive number')

    if not isinstance(cfg.get("output"), str) or not cfg["output"]:
        fail('config "output" must be a file path string')

    scan_dirs = cfg.get("scan_sql_dirs", [])
    if not isinstance(scan_dirs, list) or not all(isinstance(d, str) for d in scan_dirs):
        fail('config "scan_sql_dirs" must be a list of directory paths')

    validate_slots(cfg.get("slots"), "config")

    lanes = cfg.get("lanes")
    if not isinstance(lanes, list) or not lanes:
        fail('config "lanes" must be a non-empty list')
    names = set()
    for lane in lanes:
        name = lane.get("name")
        if not isinstance(name, str) or not name:
            fail('every lane needs a non-empty "name"')
        if name in names:
            fail(f'duplicate lane name "{name}"')
        names.add(name)
        if "slots" in lane:
            validate_slots(lane["slots"], f'lane "{name}"')
        points = lane.get("points")
        if not isinstance(points, list):
            fail(f'lane "{name}": "points" must be a list of [x, y, z]')
        for p in points:
            if (not isinstance(p, list) or len(p) != 3
                    or not all(isinstance(v, (int, float)) for v in p)):
                fail(f'lane "{name}": bad point {p!r} — each point must be [x, y, z]')


def validate_slots(slots, where):
    if not isinstance(slots, list) or not slots:
        fail(f'{where}: "slots" must be a non-empty list')
    names = set()
    for slot in slots:
        name = slot.get("name")
        if not isinstance(name, str) or not name:
            fail(f'{where}: every slot needs a non-empty "name"')
        if name in names:
            fail(f'{where}: duplicate slot name "{name}"')
        names.add(name)
        for key in ("lateral_offset", "longitudinal_offset"):
            if not isinstance(slot.get(key), (int, float)):
                fail(f'{where}: slot "{name}" needs numeric "{key}"')


def dedup_points(lane_name, points):
    out = [points[0]]
    dropped = 0
    for p in points[1:]:
        if math.dist(p, out[-1]) < DEDUP_EPSILON:
            dropped += 1
        else:
            out.append(p)
    if dropped:
        note(f'lane "{lane_name}": dropped {dropped} near-duplicate walked point(s)')
    if len(out) < 2:
        fail(f'lane "{lane_name}": needs at least 2 distinct points')
    return out


# ---------------------------------------------------------------- id locking

def collect_used_ids(scan_dirs):
    """Every waypoint_data id referenced anywhere in the scanned SQL files."""
    used = set()
    delete_re = re.compile(
        r"DELETE\s+FROM\s+`?waypoint_data`?\s+WHERE\s+`?id`?\s+IN\s*\(([^)]*)\)", re.I)
    insert_re = re.compile(r"INSERT\s+INTO\s+`?waypoint_data`?.*?;", re.I | re.S)
    row_re = re.compile(r"\(\s*(\d+)\s*,")
    for d in scan_dirs:
        path = Path(d)
        if not path.is_dir():
            note(f'scan dir "{d}" not found — skipping')
            continue
        for sql_file in sorted(path.glob("*.sql")):
            text = sql_file.read_text()
            for m in delete_re.finditer(text):
                used.update(int(t) for t in re.findall(r"\d+", m.group(1)))
            for block in insert_re.finditer(text):
                used.update(int(t) for t in row_re.findall(block.group(0)))
    return used


def get_path_ids(lock, lane_name, slot_name, used, id_range, assigned_log):
    """Reuse locked IDs for lane/slot, or allocate fresh ones and lock them."""
    lane_lock = lock.setdefault("path_ids", {}).setdefault(lane_name, {})
    slot_lock = lane_lock.get(slot_name)
    if slot_lock is not None:
        return slot_lock["forward"], slot_lock["reverse"], False

    ids = []
    candidate = id_range[0]
    while len(ids) < 2:
        if candidate > id_range[1]:
            fail(f"id_range {id_range} exhausted — no free waypoint IDs left")
        if candidate not in used:
            ids.append(candidate)
            used.add(candidate)
        candidate += 1
    lane_lock[slot_name] = {"forward": ids[0], "reverse": ids[1]}
    assigned_log.append((lane_name, slot_name, ids[0], ids[1]))
    return ids[0], ids[1], True


# ------------------------------------------------------------------ geometry

def densify(points, max_spacing):
    out = [list(points[0])]
    for a, b in zip(points, points[1:]):
        segments = max(1, math.ceil(math.dist(a, b) / max_spacing))
        for i in range(1, segments + 1):
            f = i / segments
            out.append([a[j] + (b[j] - a[j]) * f for j in range(3)])
    return out


def tangents(points):
    """Unit 2D direction of travel at each node (central difference)."""
    ts = []
    for i in range(len(points)):
        a = points[max(0, i - 1)]
        b = points[min(len(points) - 1, i + 1)]
        dx, dy = b[0] - a[0], b[1] - a[1]
        norm = math.hypot(dx, dy)
        if norm < 1e-6:
            ts.append(ts[-1] if ts else (1.0, 0.0))
        else:
            ts.append((dx / norm, dy / norm))
    return ts


def offset_path(centerline, lateral, longitudinal):
    """
    Shift each node by the slot's offsets relative to local direction of
    travel. Lateral: + = walker's own left (WoW coords: left of (dx,dy) is
    (-dy,dx)), so identical values mirror physically between the two
    directions. Longitudinal: + = ahead. Z is copied from the centerline.
    """
    out = []
    for (x, y, z), (tx, ty) in zip(centerline, tangents(centerline)):
        out.append([x + longitudinal * tx - lateral * ty,
                    y + longitudinal * ty + lateral * tx,
                    z])
    return out


# ----------------------------------------------------------------- sql emit

def sql_rows(path_id, points):
    rows = []
    for n, (x, y, z) in enumerate(points, start=1):
        coords = ",".join(COORD_FMT.format(v) for v in (x, y, z))
        rows.append(f"({path_id},{n},{coords},NULL,0,0,0,1,0,100,0)")
    return rows


def emit_sql(config_path, generated):
    try:
        config_path = config_path.resolve().relative_to(Path.cwd())
    except ValueError:
        pass  # config outside the repo -- keep the path as given

    lines = [
        "-- ============================================================",
        "-- GENERATED FILE — do not hand-edit.",
        f"-- Produced by apps/moba/gen_creep_paths.py from {config_path}.",
        "-- Re-running the generator recreates this file with the same path",
        "-- IDs (persisted in the lockfile next to the config).",
        "-- ============================================================",
        "",
        "DELETE FROM `waypoint_data` WHERE `id` IN ("
        + ", ".join(str(pid) for pid, _, _ in generated) + ");",
        "",
        "INSERT INTO `waypoint_data`",
        "(`id`, `point`, `position_x`, `position_y`, `position_z`, `orientation`,"
        " `velocity`, `delay`, `smoothTransition`, `move_type`, `action`,"
        " `action_chance`, `wpguid`)",
        "VALUES",
    ]
    chunks = []
    for pid, label, points in generated:
        chunks.append(f"-- {label} (id {pid}, {len(points)} points)\n"
                      + ",\n".join(sql_rows(pid, points)))
    lines.append(",\n".join(chunks) + ";")
    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------- main

def main():
    args = sys.argv[1:]
    if args and args[0] == "--extract":
        if len(args) != 2:
            fail("usage: gen_creep_paths.py --extract <scrollback.txt>")
        points = extract_points(Path(args[1]).read_text())
        if not points:
            fail("no 'X: .. Y: .. Z: ..' lines found in input")
        print(json.dumps(points))
        return

    config_path = Path(args[0]) if args else DEFAULT_CONFIG
    if not config_path.is_file():
        fail(f"config not found: {config_path}")
    cfg = json.loads(config_path.read_text())
    validate_config(cfg)

    lock_path = config_path.with_suffix(".lock.json")
    lock = json.loads(lock_path.read_text()) if lock_path.is_file() else {}

    used = collect_used_ids(cfg.get("scan_sql_dirs", []))
    # Locked IDs are owned (reused per lane/slot), but still off-limits for
    # fresh allocation.
    for lane_lock in lock.get("path_ids", {}).values():
        for slot_lock in lane_lock.values():
            used.update(slot_lock.values())

    lane_names = {lane["name"] for lane in cfg["lanes"]}
    for locked_lane in lock.get("path_ids", {}):
        if locked_lane not in lane_names:
            note(f'lockfile has lane "{locked_lane}" not present in config — '
                 f"keeping its IDs reserved")

    assigned_log = []
    generated = []  # (path_id, label, points)
    for lane in cfg["lanes"]:
        name = lane["name"]
        slots = lane.get("slots", cfg["slots"])
        centerline = densify(dedup_points(name, lane["points"]), cfg["max_spacing"])
        reversed_centerline = list(reversed(centerline))
        for slot in slots:
            fwd_id, rev_id, fresh = get_path_ids(
                lock, name, slot["name"], used, cfg["id_range"], assigned_log)
            lat, lon = slot["lateral_offset"], slot["longitudinal_offset"]
            generated.append(
                (fwd_id, f"{name} / {slot['name']} / forward",
                 offset_path(centerline, lat, lon)))
            generated.append(
                (rev_id, f"{name} / {slot['name']} / reverse",
                 offset_path(reversed_centerline, lat, lon)))

    output_path = Path(cfg["output"])
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(emit_sql(config_path, generated))

    lock["_comment"] = ("Machine-generated by gen_creep_paths.py — do not edit. "
                        "Maps lane/slot names to their permanently assigned "
                        "waypoint_data IDs.")
    lock_path.write_text(json.dumps(lock, indent=2, sort_keys=True) + "\n")

    print(f"\nWrote {output_path} ({len(generated)} paths, "
          f"{sum(len(p) for _, _, p in generated)} waypoint rows).")
    if assigned_log:
        print("Newly assigned path IDs (now locked):")
        for lane_name, slot_name, fwd, rev in assigned_log:
            print(f"  {lane_name} / {slot_name}: forward={fwd} reverse={rev}")
        print("If these are for a NEW slot, point the matching "
              "mod_moba_creep_data.WaypointPathId rows at them.")
    else:
        print("All path IDs reused from lockfile — no creature config changes needed.")
    print("Apply the SQL to acore_world, then fully restart worldserver.")


if __name__ == "__main__":
    main()
