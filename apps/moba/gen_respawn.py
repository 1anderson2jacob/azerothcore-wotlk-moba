#!/usr/bin/env python3
"""
MOBA respawn generator.

Reads per-map respawn configs (apps/moba/maps/<mode>/respawn_config.json) and
generates data/sql/custom/mod_moba_respawn.sql: the map-keyed mod_moba_respawn
timing table, plus the per-map spawn wiring (game_graveyard coordinates and the
battleground_template start-location/orientation that back them).

Timing is the only thing that lives in the table; the spawn LOCATION is written
into game_graveyard / battleground_template, which the server already reads via
GetTeamStartPosition / GetClosestGraveyard (see MobaRespawnData.{h,cpp}).

Usage (from the repo root):
    python3 apps/moba/gen_respawn.py
"""

import json
import sys
from pathlib import Path

MAPS_DIR = Path(__file__).parent / "maps"
OUTPUT = Path("data/sql/custom/mod_moba_respawn.sql")

REQUIRED_TIMING = ["base_ms", "per_min_ms", "cap_ms"]
REQUIRED_SPAWN_TEAM = ["graveyard_id", "x", "y", "z", "o"]


def fail(msg):
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def validate(cfg, path):
    if not isinstance(cfg.get("map"), int):
        fail(f'{path}: "map" must be an integer map id')
    if not isinstance(cfg.get("battleground_template_id"), int):
        fail(f'{path}: "battleground_template_id" must be an integer')
    timing = cfg.get("timing")
    if not isinstance(timing, dict) or any(k not in timing for k in REQUIRED_TIMING):
        fail(f'{path}: "timing" must contain {REQUIRED_TIMING}')
    spawn = cfg.get("spawn")
    if not isinstance(spawn, dict) or "alliance" not in spawn or "horde" not in spawn:
        fail(f'{path}: "spawn" must have "alliance" and "horde"')
    for team in ("alliance", "horde"):
        for k in REQUIRED_SPAWN_TEAM:
            if k not in spawn[team]:
                fail(f'{path}: spawn.{team} missing "{k}"')
    recall = cfg.get("recall")
    if recall is not None:
        if not isinstance(recall, dict) or not isinstance(recall.get("cast_time_ms"), int):
            fail(f'{path}: "recall.cast_time_ms" must be an integer (ms)')
        emp = recall.get("empowered_cast_time_ms")
        if emp is not None and not isinstance(emp, int):
            fail(f'{path}: "recall.empowered_cast_time_ms" must be an integer (ms)')


def load_configs():
    configs = []
    for path in sorted(MAPS_DIR.glob("*/respawn_config.json")):
        cfg = json.loads(path.read_text())
        validate(cfg, path)
        configs.append((path, cfg))
    if not configs:
        fail(f"no respawn configs found under {MAPS_DIR}/*/respawn_config.json")
    return configs


def emit(configs):
    lines = [
        "-- ============================================================",
        "-- GENERATED FILE -- do not hand-edit.",
        "-- Produced by apps/moba/gen_respawn.py from apps/moba/maps/*/respawn_config.json.",
        "-- Timing lives in mod_moba_respawn; spawn LOCATION is written into",
        "-- game_graveyard / battleground_template (read at runtime via",
        "-- GetTeamStartPosition / GetClosestGraveyard).",
        "-- ============================================================",
        "",
        "USE acore_world;",
        "",
        "DROP TABLE IF EXISTS `mod_moba_respawn`;",
        "CREATE TABLE `mod_moba_respawn` (",
        "    `Map`      INT UNSIGNED NOT NULL PRIMARY KEY,        -- BG map id",
        "    `BaseMs`   INT UNSIGNED NOT NULL DEFAULT 10000,      -- base respawn wait",
        "    `PerMinMs` INT UNSIGNED NOT NULL DEFAULT 1500,       -- added per elapsed match-minute",
        "    `CapMs`    INT UNSIGNED NOT NULL DEFAULT 60000,       -- maximum respawn wait",
        "    `RecallCastMs`          INT UNSIGNED NOT NULL DEFAULT 0,  -- recall cast time (ms); 0 = spell default",
        "    `RecallEmpoweredCastMs` INT UNSIGNED NOT NULL DEFAULT 0   -- empowered recall cast time (ms); 0 = fall back to normal",
        ");",
        "",
        "INSERT INTO `mod_moba_respawn` (`Map`, `BaseMs`, `PerMinMs`, `CapMs`, `RecallCastMs`, `RecallEmpoweredCastMs`)",
        "VALUES",
    ]
    rows = []
    for _, cfg in configs:
        t = cfg["timing"]
        recall = cfg.get("recall") or {}
        recall_ms = recall.get("cast_time_ms", 0)
        recall_emp_ms = recall.get("empowered_cast_time_ms", 0)
        rows.append(f"({cfg['map']}, {t['base_ms']}, {t['per_min_ms']}, {t['cap_ms']}, {recall_ms}, {recall_emp_ms})")
    lines.append(",\n".join(rows) + ";")

    for path, cfg in configs:
        mode = path.parent.name
        a, h = cfg["spawn"]["alliance"], cfg["spawn"]["horde"]
        lines += [
            "",
            f"-- Spawn wiring for map {cfg['map']} ({mode})",
            (f"UPDATE battleground_template SET "
             f"AllianceStartLoc = {a['graveyard_id']}, AllianceStartO = {a['o']}, "
             f"HordeStartLoc = {h['graveyard_id']}, HordeStartO = {h['o']} "
             f"WHERE ID = {cfg['battleground_template_id']};"),
            f"UPDATE game_graveyard SET x = {a['x']}, y = {a['y']}, z = {a['z']} WHERE ID = {a['graveyard_id']};",
            f"UPDATE game_graveyard SET x = {h['x']}, y = {h['y']}, z = {h['z']} WHERE ID = {h['graveyard_id']};",
        ]
    return "\n".join(lines) + "\n"


def main():
    configs = load_configs()
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(emit(configs))
    maps = ", ".join(str(c["map"]) for _, c in configs)
    print(f"Wrote {OUTPUT} ({len(configs)} map(s): {maps}).")
    print("Apply the SQL to acore_world, then fully restart worldserver.")


if __name__ == "__main__":
    main()
