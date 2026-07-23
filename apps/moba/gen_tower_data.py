#!/usr/bin/env python3
"""
MOBA structure generator (towers, inhibitors, bases).

Reads per-map structure configs (apps/moba/maps/<mode>/tower_config.yaml) and
generates data/sql/custom/db_world/mod_moba_towers.sql in full:
  * creature_template       -- the structure creatures (name/faction/health/armor)
  * creature_template_model -- their display id + scale
  * mod_moba_tower_data     -- per-map placement (position, kind, guard chain,
                               respawn, AI attack config)

Fixed creature invariants (level 80, unit flags, npc_moba_tower script, etc.)
are enforced in code below; everything a designer tunes -- name, display,
scale, health -- is a config field, mirroring gen_creep_roster.py. Structure
entries are assigned by hand in the config (900000+) and must be globally
unique across maps (mod_moba_tower_data keys on CreatureEntry).

Usage (from the repo root):
    python3 apps/moba/gen_tower_data.py
"""

import yaml
import sys
from pathlib import Path

MAPS_DIR = Path(__file__).parent / "maps"
OUTPUT = Path("data/sql/custom/db_world/mod_moba_towers.sql")

REQUIRED_TOWER = ["entry", "team", "tier", "guarded_by_entry",
                  "name", "display_id", "display_scale", "health_modifier",
                  "x", "y", "z", "o",
                  "attack_range", "attack_interval_ms", "attack_spell_id"]

# Structure kind -> mod_moba_tower_data.Kind. Matches MobaStructureKind in
# MobaTowerData.h. "kind" is optional in config (default "tower").
KIND_IDS = {"tower": 0, "inhibitor": 1, "core": 2}

DEFAULT_SUBNAME = "MOBA Objective"
DEFAULT_ARMOR_MODIFIER = 5


def fail(msg):
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def sql_str(s):
    return "'" + str(s).replace("'", "''") + "'"


def validate(cfg, path):
    if not isinstance(cfg.get("map"), int):
        fail(f'{path}: "map" must be an integer map id')
    towers = cfg.get("towers")
    if not isinstance(towers, list) or not towers:
        fail(f'{path}: "towers" must be a non-empty list')
    for t in towers:
        for k in REQUIRED_TOWER:
            if k not in t:
                fail(f'{path}: structure {t.get("entry", "?")} missing "{k}"')
        if t["team"] not in (0, 1):
            fail(f'{path}: structure {t["entry"]} "team" must be 0 or 1')
        if t.get("kind", "tower") not in KIND_IDS:
            fail(f'{path}: structure {t["entry"]} "kind" must be one of {sorted(KIND_IDS)}')


def load_configs():
    configs = []
    seen_entries = {}
    for path in sorted(MAPS_DIR.glob("*/tower_config.yaml")):
        cfg = yaml.safe_load(path.read_text())
        validate(cfg, path)
        for t in cfg["towers"]:
            if t["entry"] in seen_entries:
                fail(f'{path}: structure entry {t["entry"]} already defined in '
                     f'{seen_entries[t["entry"]]} -- entries must be globally unique')
            seen_entries[t["entry"]] = path
        configs.append((path, cfg))
    if not configs:
        fail(f"no structure configs found under {MAPS_DIR}/*/tower_config.yaml")
    return configs


def emit(configs):
    structures = [t for _, cfg in configs for t in cfg["towers"]]
    entries_csv = ", ".join(str(t["entry"]) for t in structures)

    lines = [
        "-- ============================================================",
        "-- GENERATED FILE -- do not hand-edit.",
        "-- Produced by apps/moba/gen_tower_data.py from apps/moba/maps/*/tower_config.yaml.",
        "-- Owns the structure creatures (creature_template + creature_template_model)",
        "-- AND their per-map placement (mod_moba_tower_data) -- there is no separate",
        "-- hand-written defs file. Fixed creature invariants (level 80, unit flags,",
        "-- npc_moba_tower script) are enforced in the generator; name/faction/health/",
        "-- armor/display/scale are config fields.",
        "-- Kind: 0 = tower (attacks), 1 = inhibitor (passive; grants super minions",
        "-- and respawns after RespawnMs on death), 2 = core/base (passive; its",
        "-- destruction wins the match). RespawnMs applies to inhibitors; 0 = never.",
        "-- ============================================================",
        "",
        "USE acore_world;",
        "",
        f"DELETE FROM `creature_template` WHERE `entry` IN ({entries_csv});",
        "INSERT INTO `creature_template`",
        "(`entry`, `name`, `subname`, `minlevel`, `maxlevel`, `faction`, `npcflag`,",
        " `speed_walk`, `speed_run`, `rank`, `unit_class`, `unit_flags`, `unit_flags2`,",
        " `type`, `type_flags`, `MovementType`, `HealthModifier`, `ArmorModifier`,",
        " `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`)",
        "VALUES",
    ]
    ct_rows = []
    for t in structures:
        faction = 84 if t["team"] == 0 else 83
        subname = t.get("subname", DEFAULT_SUBNAME)
        armor = t.get("armor_modifier", DEFAULT_ARMOR_MODIFIER)
        ct_rows.append(
            f"({t['entry']}, {sql_str(t['name'])}, {sql_str(subname)}, 80, 80, {faction}, 0, "
            f"1.0, 1.14286, 1, 1, 32768, 2048, "
            f"9, 0, 0, {t['health_modifier']}, {armor}, "
            f"0, 0, 0, 'npc_moba_tower', 0)")
    lines.append(",\n".join(ct_rows) + ";")

    lines += [
        "",
        f"DELETE FROM `creature_template_model` WHERE `CreatureID` IN ({entries_csv});",
        "INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)",
        "VALUES",
    ]
    ctm_rows = [f"({t['entry']}, 0, {t['display_id']}, {t['display_scale']}, 1, 0)"
                for t in structures]
    lines.append(",\n".join(ctm_rows) + ";")

    lines += [
        "",
        "DROP TABLE IF EXISTS `mod_moba_tower_data`;",
        "CREATE TABLE `mod_moba_tower_data` (",
        "    `CreatureEntry`    INT UNSIGNED NOT NULL PRIMARY KEY,",
        "    `Map`              INT UNSIGNED NOT NULL,               -- BG map id",
        "    `Team`             TINYINT UNSIGNED NOT NULL,           -- 0 = Alliance, 1 = Horde",
        "    `Tier`             TINYINT UNSIGNED NOT NULL DEFAULT 0,",
        "    `GuardedByEntry`   INT UNSIGNED NOT NULL DEFAULT 0,      -- 0 = none / always vulnerable",
        "    `Kind`             TINYINT UNSIGNED NOT NULL DEFAULT 0,  -- 0 tower, 1 inhibitor, 2 core",
        "    `RespawnMs`        INT UNSIGNED NOT NULL DEFAULT 0,      -- inhibitor respawn delay; 0 = never",
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
        "(`CreatureEntry`, `Map`, `Team`, `Tier`, `GuardedByEntry`, `Kind`, `RespawnMs`, `PosX`, `PosY`, `PosZ`, `Orientation`, `AttackRange`, `AttackIntervalMs`, `AttackSpellId`)",
        "VALUES",
    ]
    data_rows = []
    for _, cfg in configs:
        for t in cfg["towers"]:
            data_rows.append(
                f"({t['entry']}, {cfg['map']}, {t['team']}, {t['tier']}, {t['guarded_by_entry']}, "
                f"{KIND_IDS[t.get('kind', 'tower')]}, {t.get('respawn_ms', 0)}, "
                f"{t['x']}, {t['y']}, {t['z']}, {t['o']}, "
                f"{t['attack_range']}, {t['attack_interval_ms']}, {t['attack_spell_id']})")
    lines.append(",\n".join(data_rows) + ";")
    return "\n".join(lines) + "\n"


def main():
    configs = load_configs()
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(emit(configs))
    total = sum(len(c["towers"]) for _, c in configs)
    print(f"Wrote {OUTPUT} ({total} structures across {len(configs)} map(s)).")
    print("If mod_moba_tower_defs.sql still exists, delete it -- this file now owns those rows.")
    print("Restart worldserver — mod_moba_towers.sql auto-applies on boot.")


if __name__ == "__main__":
    main()
