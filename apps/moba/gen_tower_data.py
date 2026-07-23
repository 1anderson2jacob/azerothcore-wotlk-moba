#!/usr/bin/env python3
"""
MOBA tower generator.

Reads per-map tower configs (apps/moba/maps/<mode>/tower_config.yaml) and
generates data/sql/custom/db_world/mod_moba_towers.sql: the map-keyed mod_moba_tower_data
table (spawn position, tier/guard dependency, per-tower AI config).

The tower CREATURES (creature_template / model / health) are shared across maps
and hand-written in data/sql/custom/db_world/mod_moba_tower_defs.sql -- this generator
owns only the per-map rows. Tower entries are assigned by hand in the config
(900000/900001), so there's no lockfile.

Usage (from the repo root):
    python3 apps/moba/gen_tower_data.py
"""

import yaml
import sys
from pathlib import Path

MAPS_DIR = Path(__file__).parent / "maps"
OUTPUT = Path("data/sql/custom/db_world/mod_moba_towers.sql")

REQUIRED_TOWER = ["entry", "team", "tier", "guarded_by_entry", "x", "y", "z", "o",
                  "attack_range", "attack_interval_ms", "attack_spell_id"]


def fail(msg):
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def validate(cfg, path):
    if not isinstance(cfg.get("map"), int):
        fail(f'{path}: "map" must be an integer map id')
    towers = cfg.get("towers")
    if not isinstance(towers, list) or not towers:
        fail(f'{path}: "towers" must be a non-empty list')
    for t in towers:
        for k in REQUIRED_TOWER:
            if k not in t:
                fail(f'{path}: tower {t.get("entry", "?")} missing "{k}"')
        if t["team"] not in (0, 1):
            fail(f'{path}: tower {t["entry"]} "team" must be 0 or 1')


def load_configs():
    configs = []
    for path in sorted(MAPS_DIR.glob("*/tower_config.yaml")):
        cfg = yaml.safe_load(path.read_text())
        validate(cfg, path)
        configs.append((path, cfg))
    if not configs:
        fail(f"no tower configs found under {MAPS_DIR}/*/tower_config.yaml")
    return configs


def emit(configs):
    lines = [
        "-- ============================================================",
        "-- GENERATED FILE -- do not hand-edit.",
        "-- Produced by apps/moba/gen_tower_data.py from apps/moba/maps/*/tower_config.yaml.",
        "-- Tower CREATURES (creature_template/model/health) are shared and",
        "-- hand-written in mod_moba_tower_defs.sql; this is only the per-map rows.",
        "-- GuardedByEntry = 0 means always vulnerable; otherwise the tower spawns",
        "-- inert until the referenced tower entry is destroyed (single FK).",
        "-- ============================================================",
        "",
        "USE acore_world;",
        "",
        "DROP TABLE IF EXISTS `mod_moba_tower_data`;",
        "CREATE TABLE `mod_moba_tower_data` (",
        "    `CreatureEntry`    INT UNSIGNED NOT NULL PRIMARY KEY,",
        "    `Map`              INT UNSIGNED NOT NULL,               -- BG map id",
        "    `Team`             TINYINT UNSIGNED NOT NULL,           -- 0 = Alliance, 1 = Horde",
        "    `Tier`             TINYINT UNSIGNED NOT NULL DEFAULT 0,",
        "    `GuardedByEntry`   INT UNSIGNED NOT NULL DEFAULT 0,      -- 0 = none / always vulnerable",
        "    `PosX`             FLOAT NOT NULL,",
        "    `PosY`             FLOAT NOT NULL,",
        "    `PosZ`             FLOAT NOT NULL,",
        "    `Orientation`      FLOAT NOT NULL,",
        "    `AttackRange`      FLOAT NOT NULL DEFAULT 40,",
        "    `AttackIntervalMs` INT UNSIGNED NOT NULL DEFAULT 1500,",
        "    `AttackSpellId`    INT UNSIGNED NOT NULL DEFAULT 9053",
        ");",
        "",
        "INSERT INTO `mod_moba_tower_data`",
        "(`CreatureEntry`, `Map`, `Team`, `Tier`, `GuardedByEntry`, `PosX`, `PosY`, `PosZ`, `Orientation`, `AttackRange`, `AttackIntervalMs`, `AttackSpellId`)",
        "VALUES",
    ]
    rows = []
    for _, cfg in configs:
        for t in cfg["towers"]:
            rows.append(
                f"({t['entry']}, {cfg['map']}, {t['team']}, {t['tier']}, {t['guarded_by_entry']}, "
                f"{t['x']}, {t['y']}, {t['z']}, {t['o']}, "
                f"{t['attack_range']}, {t['attack_interval_ms']}, {t['attack_spell_id']})")
    lines.append(",\n".join(rows) + ";")
    return "\n".join(lines) + "\n"


def main():
    configs = load_configs()
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(emit(configs))
    total = sum(len(c["towers"]) for _, c in configs)
    print(f"Wrote {OUTPUT} ({total} towers across {len(configs)} map(s)).")
    print("Restart worldserver — mod_moba_tower_defs.sql and mod_moba_towers.sql auto-apply on boot.")


if __name__ == "__main__":
    main()
