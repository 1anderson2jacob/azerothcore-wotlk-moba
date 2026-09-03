#!/usr/bin/env python3
"""
MOBA lane waypoint-path generator.

Reads per-map lane configs (apps/moba/maps/<mode>/lane_config.yaml: walked
centerline points + formation slot offsets), densifies each lane to a max
node spacing, generates one offset path per formation slot per direction, and
emits one combined idempotent DELETE+INSERT SQL file for `waypoint_data`
(data/sql/custom/db_world/mod_moba_creep_paths.sql) across all maps.

Path IDs are auto-assigned from the generator's block in apps/moba/id_blocks.json
on first use and persisted to a machine-owned lockfile in the same bundle
(lane_config.lock.json). Later runs reuse the locked IDs, so re-walking or
re-tuning a lane never changes which waypoint_data id a creature's
mod_moba_creep_data.WaypointPathId points at. Do not hand-edit the lockfile.

Usage (from the repo root):
    python3 apps/moba/gen_creep_paths.py
    python3 apps/moba/gen_creep_paths.py --extract scrollback.txt

--extract parses raw .gps console scrollback and prints a JSON points array
ready to paste into a config's "points" field.

See apps/moba/README.md for the full workflow and config field reference.
"""

import json
import bisect
import yaml
import math
import re
import sys
from pathlib import Path

import id_alloc

MAPS_DIR = Path(__file__).parent / "maps"
OUTPUT = Path("data/sql/custom/db_world/mod_moba_creep_paths.sql")
COORD_FMT = "{:.4f}"
DEDUP_EPSILON = 0.5  # yards; consecutive walked points closer than this are merged
CENTERLINE_SLOT = "_centerline"  # reserved lockfile slot name for the reference path

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

def validate_config(cfg, path):
    spacing = cfg.get("max_spacing")
    if not isinstance(spacing, (int, float)) or spacing <= 0:
        fail(f'{path}: "max_spacing" must be a positive number')

    validate_slots(cfg.get("slots"), path)

    lanes = cfg.get("lanes")
    if not isinstance(lanes, list) or not lanes:
        fail(f'{path}: "lanes" must be a non-empty list')
    names = set()
    for lane in lanes:
        name = lane.get("name")
        if not isinstance(name, str) or not name:
            fail(f'{path}: every lane needs a non-empty "name"')
        if name in names:
            fail(f'{path}: duplicate lane name "{name}"')
        names.add(name)
        if "slots" in lane:
            validate_slots(lane["slots"], f'{path} lane "{name}"')
        points = lane.get("points")
        if not isinstance(points, list):
            fail(f'{path} lane "{name}": "points" must be a list of [x, y, z]')
        for p in points:
            if (not isinstance(p, list) or len(p) != 3
                    or not all(isinstance(v, (int, float)) for v in p)):
                fail(f'{path} lane "{name}": bad point {p!r} — each point must be [x, y, z]')


def validate_slots(slots, where):
    if not isinstance(slots, list) or not slots:
        fail(f'{where}: "slots" must be a non-empty list')
    names = set()
    for slot in slots:
        name = slot.get("name")
        if not isinstance(name, str) or not name:
            fail(f'{where}: every slot needs a non-empty "name"')
        if name == CENTERLINE_SLOT:
            fail(f'{where}: slot name "{CENTERLINE_SLOT}" is reserved for the '
                 f"generated speed-compensation reference path")
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

def get_path_ids(lock, lane_name, slot_name, alloc, assigned_log):
    """Reuse locked IDs for lane/slot, or allocate a fresh pair and lock them.

    Same short-circuit as gen_creep_roster.get_entry: a locked pair is returned
    with no block check. Today's pairs are not adjacent (melee_right is
    900110/900120), and nothing requires them to be.
    """
    lane_lock = lock.setdefault("path_ids", {}).setdefault(lane_name, {})
    slot_lock = lane_lock.get(slot_name)
    if slot_lock is not None:
        return slot_lock["forward"], slot_lock["reverse"], False

    fwd, rev = alloc.take(2)
    lane_lock[slot_name] = {"forward": fwd, "reverse": rev}
    assigned_log.append((lane_name, slot_name, fwd, rev))
    return fwd, rev, True


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


def arc_lengths(points):
    """Cumulative 3D distance from the first node, one entry per node."""
    s = [0.0]
    for a, b in zip(points, points[1:]):
        s.append(s[-1] + math.dist(a, b))
    return s


def sample_at_arc(centerline, tans, s, target):
    """Position and unit tangent at an arc length, extrapolating past either end."""
    if target <= 0.0:
        x, y, z = centerline[0]
        tx, ty = tans[0]
        return [x + target * tx, y + target * ty, z], (tx, ty)

    if target >= s[-1]:
        over = target - s[-1]
        x, y, z = centerline[-1]
        tx, ty = tans[-1]
        return [x + over * tx, y + over * ty, z], (tx, ty)

    j = bisect.bisect_right(s, target) - 1
    span = s[j + 1] - s[j]
    f = (target - s[j]) / span if span > 1e-9 else 0.0
    pos = [centerline[j][d] + (centerline[j + 1][d] - centerline[j][d]) * f
           for d in range(3)]
    tx = tans[j][0] + (tans[j + 1][0] - tans[j][0]) * f
    ty = tans[j][1] + (tans[j + 1][1] - tans[j][1]) * f
    norm = math.hypot(tx, ty)
    return pos, ((tx / norm, ty / norm) if norm > 1e-6 else tans[j])


def offset_path(centerline, lateral, longitudinal):
    """
    One node per centerline node, placed at the slot's formation position.

    Longitudinal walks the centerline ARC, not the local tangent: offsetting
    along the tangent throws nodes off the outside of every curve, so a
    staggered slot's path grows longer than the lane it is meant to follow.
    Lateral: + = walker's own left (WoW coords: left of (dx,dy) is (-dy,dx)),
    so identical values mirror physically between the two directions. Z comes
    from the shifted arc position, so a staggered slot picks up the ramp
    height where it actually stands.
    """
    tans = tangents(centerline)
    s = arc_lengths(centerline)
    out = []
    for i in range(len(centerline)):
        (x, y, z), (tx, ty) = sample_at_arc(centerline, tans, s, s[i] + longitudinal)
        out.append([x - lateral * ty, y + lateral * tx, z])
    return out


# ----------------------------------------------------------------- sql emit

def sql_rows(path_id, points):
    rows = []
    for n, (x, y, z) in enumerate(points, start=1):
        coords = ",".join(COORD_FMT.format(v) for v in (x, y, z))
        rows.append(f"({path_id},{n},{coords},NULL,0,0,0,1,0,100,0)")
    return rows


def emit_sql(generated, blocks):
    lines = [
        "-- ============================================================",
        "-- GENERATED FILE — do not hand-edit.",
        "-- Produced by apps/moba/gen_creep_paths.py from apps/moba/maps/*/lane_config.yaml.",
        "-- Re-running recreates this file with the same path IDs (persisted in",
        "-- each map bundle's lane_config.lock.json).",
        "-- ============================================================",
        "",
        "-- The DELETE clears this generator's whole ID block, not just the paths",
        "-- about to be inserted, so a lane or slot removed from config loses its",
        "-- waypoint rows too. Blocks are declared in apps/moba/id_blocks.json.",
        f"DELETE FROM `waypoint_data` WHERE {id_alloc.sql_window(blocks, '`id`')};",
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

    configs = sorted(MAPS_DIR.glob("*/lane_config.yaml"))
    if not configs:
        fail(f"no lane configs found under {MAPS_DIR}/*/lane_config.yaml")

    alloc = id_alloc.Allocator(id_alloc.Registry(), id_alloc.WAYPOINT_DATA, "creep_paths")
    locks = {}
    for cp in configs:
        lp = cp.with_suffix(".lock.json")
        locks[cp] = json.loads(lp.read_text()) if lp.is_file() else {}

    assigned_log = []
    generated = []  # (path_id, label, points)
    for cp in configs:
        cfg = yaml.safe_load(cp.read_text())
        validate_config(cfg, cp)
        lock = locks[cp]

        lane_names = {lane["name"] for lane in cfg["lanes"]}
        for locked_lane in lock.get("path_ids", {}):
            if locked_lane not in lane_names:
                note(f'{cp}: lockfile has lane "{locked_lane}" not in config — '
                     f"keeping its IDs reserved")

        for lane in cfg["lanes"]:
            name = lane["name"]
            slots = lane.get("slots", cfg["slots"])
            centerline = densify(dedup_points(name, lane["points"]), cfg["max_spacing"])
            reversed_centerline = list(reversed(centerline))
            # Speed-compensation reference: a creep divides its own leg length by
            # the leg at the same index here, so this must stay node-for-node
            # aligned with every slot path built off this centerline.
            ref_fwd, ref_rev, _ = get_path_ids(
                lock, name, CENTERLINE_SLOT, alloc, assigned_log)
            generated.append((ref_fwd, f"{name} / centreline / forward", centerline))
            generated.append((ref_rev, f"{name} / centreline / reverse", reversed_centerline))

            for slot in slots:
                fwd_id, rev_id, fresh = get_path_ids(
                    lock, name, slot["name"], alloc, assigned_log)
                lat, lon = slot["lateral_offset"], slot["longitudinal_offset"]
                generated.append(
                    (fwd_id, f"{name} / {slot['name']} / forward",
                     offset_path(centerline, lat, lon)))
                generated.append(
                    (rev_id, f"{name} / {slot['name']} / reverse",
                     offset_path(reversed_centerline, lat, lon)))

        lock["_comment"] = ("Machine-generated by gen_creep_paths.py — do not edit. "
                            "Maps lane/slot names to their permanently assigned "
                            "waypoint_data IDs.")
        cp.with_suffix(".lock.json").write_text(json.dumps(lock, indent=2, sort_keys=True) + "\n")

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(emit_sql(generated, alloc.blocks))

    print(f"\nWrote {OUTPUT} ({len(generated)} paths across {len(configs)} map(s), "
          f"{sum(len(p) for _, _, p in generated)} waypoint rows).")
    if assigned_log:
        print("Newly assigned path IDs (now locked):")
        for lane_name, slot_name, fwd, rev in assigned_log:
            print(f"  {lane_name} / {slot_name}: forward={fwd} reverse={rev}")
        print("If these are for a NEW slot, point the matching "
              "mod_moba_creep_data.WaypointPathId rows at them.")
    else:
        print("All path IDs reused from lockfiles — no creature config changes needed.")
    print("Apply the SQL to acore_world, then fully restart worldserver.")


if __name__ == "__main__":
    main()
