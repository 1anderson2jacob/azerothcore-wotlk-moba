#!/usr/bin/env python3
"""
MOBA base generator.

Reads per-map base configs (apps/moba/maps/<mode>/base_config.json) and
generates data/sql/custom/mod_moba_base.sql: the map-keyed mod_moba_base table
(respawn timing, recall cast times, fountain healing), plus the per-map spawn
wiring (game_graveyard coordinates, and the battleground_template
start-location / orientation / radius that back them).

"Base" here means the team's base -- everything anchored to it. Tunables live in
the table; the base LOCATION and RADIUS are written into game_graveyard /
battleground_template, which the server already reads via GetTeamStartPosition /
GetClosestGraveyard / GetStartMaxDist (see MobaBaseData.{h,cpp}).

spawn.radius does double duty: it is the core's prep-phase leash
(_CheckSafePositions teleports you back if you leave it before doors open) AND
the fountain heal zone. One value, so the two cannot drift apart.

Usage (from the repo root):
    python3 apps/moba/gen_base.py
"""

import json
import sys
from pathlib import Path

MAPS_DIR = Path(__file__).parent / "maps"
OUTPUT = Path("data/sql/custom/mod_moba_base.sql")

REQUIRED_RESPAWN = ["base_ms", "per_min_ms", "cap_ms"]
REQUIRED_SPAWN_TEAM = ["graveyard_id", "x", "y", "z", "o"]


def fail(msg):
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def validate(cfg, path):
    if not isinstance(cfg.get("map"), int):
        fail(f'{path}: "map" must be an integer map id')
    if not isinstance(cfg.get("battleground_template_id"), int):
        fail(f'{path}: "battleground_template_id" must be an integer')
    respawn = cfg.get("respawn")
    if not isinstance(respawn, dict) or any(k not in respawn for k in REQUIRED_RESPAWN):
        fail(f'{path}: "respawn" must contain {REQUIRED_RESPAWN}')
    spawn = cfg.get("spawn")
    if not isinstance(spawn, dict) or "alliance" not in spawn or "horde" not in spawn:
        fail(f'{path}: "spawn" must have "alliance" and "horde"')
    # Positive, because 0 would silently disable the core's prep-phase leash too.
    if not isinstance(spawn.get("radius"), (int, float)) or spawn["radius"] <= 0:
        fail(f'{path}: "spawn.radius" must be a positive number (yards)')
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
    fountain = cfg.get("fountain")
    if fountain is not None:
        if not isinstance(fountain, dict):
            fail(f'{path}: "fountain" must be an object')
        for k in ("tick_ms", "hp_pct", "mana_pct"):
            if fountain.get(k) is not None and not isinstance(fountain[k], int):
                fail(f'{path}: "fountain.{k}" must be an integer')


def load_configs():
    configs = []
    for path in sorted(MAPS_DIR.glob("*/base_config.json")):
        cfg = json.loads(path.read_text())
        validate(cfg, path)
        configs.append((path, cfg))
    if not configs:
        fail(f"no base configs found under {MAPS_DIR}/*/base_config.json")
    return configs


def emit(configs):
    lines = [
        "-- ============================================================",
        "-- GENERATED FILE -- do not hand-edit.",
        "-- Produced by apps/moba/gen_base.py from apps/moba/maps/*/base_config.json.",
        "-- Tunables live in mod_moba_base; the base LOCATION and RADIUS are written",
        "-- into game_graveyard / battleground_template (read at runtime via",
        "-- GetTeamStartPosition / GetClosestGraveyard / GetStartMaxDist).",
        "-- ============================================================",
        "",
        "USE acore_world;",
        "",
        "DROP TABLE IF EXISTS `mod_moba_base`;",
        "CREATE TABLE `mod_moba_base` (",
        "    `Map`             INT UNSIGNED NOT NULL PRIMARY KEY,      -- BG map id",
        "    `RespawnBaseMs`   INT UNSIGNED NOT NULL DEFAULT 10000,    -- base respawn wait",
        "    `RespawnPerMinMs` INT UNSIGNED NOT NULL DEFAULT 1500,     -- added per elapsed match-minute",
        "    `RespawnCapMs`    INT UNSIGNED NOT NULL DEFAULT 60000,    -- maximum respawn wait",
        "    `RecallCastMs`          INT UNSIGNED NOT NULL DEFAULT 0,  -- recall cast time (ms); 0 = spell default",
        "    `RecallEmpoweredCastMs` INT UNSIGNED NOT NULL DEFAULT 0,  -- empowered recall cast time (ms); 0 = fall back to normal",
        "    `FountainTickMs`  INT UNSIGNED NOT NULL DEFAULT 0,        -- fountain heal cadence (ms); 0 = fountain healing off",
        "    `FountainHpPct`   INT UNSIGNED NOT NULL DEFAULT 0,        -- % of max health restored per tick",
        "    `FountainManaPct` INT UNSIGNED NOT NULL DEFAULT 0         -- % of max mana restored per tick (mana users only)",
        ");",
        "",
        "INSERT INTO `mod_moba_base` (`Map`, `RespawnBaseMs`, `RespawnPerMinMs`, `RespawnCapMs`, `RecallCastMs`, `RecallEmpoweredCastMs`, `FountainTickMs`, `FountainHpPct`, `FountainManaPct`)",
        "VALUES",
    ]
    rows = []
    for _, cfg in configs:
        t = cfg["respawn"]
        recall = cfg.get("recall") or {}
        recall_ms = recall.get("cast_time_ms", 0)
        recall_emp_ms = recall.get("empowered_cast_time_ms", 0)
        f = cfg.get("fountain") or {}
        rows.append(
            f"({cfg['map']}, {t['base_ms']}, {t['per_min_ms']}, {t['cap_ms']}, {recall_ms}, {recall_emp_ms}, "
            f"{f.get('tick_ms', 0)}, {f.get('hp_pct', 0)}, {f.get('mana_pct', 0)})")
    lines.append(",\n".join(rows) + ";")

    for path, cfg in configs:
        mode = path.parent.name
        spawn = cfg["spawn"]
        a, h = spawn["alliance"], spawn["horde"]
        lines += [
            "",
            f"-- Spawn wiring for map {cfg['map']} ({mode})",
            "-- StartMaxDist is the base bubble: the core's prep-phase leash AND the fountain heal zone.",
            (f"UPDATE battleground_template SET "
             f"AllianceStartLoc = {a['graveyard_id']}, AllianceStartO = {a['o']}, "
             f"HordeStartLoc = {h['graveyard_id']}, HordeStartO = {h['o']}, "
             f"StartMaxDist = {spawn['radius']} "
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
