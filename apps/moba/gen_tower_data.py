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
scale, health -- is a config field, mirroring gen_creep_roster.py.

Structures are named by `key`, never by entry. creature_template entries are
auto-assigned from this generator's block in apps/moba/id_blocks.json on first
use and persisted to tower_config.lock.json, so `guarded_by` names a sibling's
key and the numbers never leave the generator. Keys are per-bundle, like creep
and neutral keys, so two maps may both have an "alliance_tower". Do not
hand-edit the lockfile.

Usage (from the repo root):
    python3 apps/moba/gen_tower_data.py
"""

import json
import yaml
import sys
from pathlib import Path

import id_alloc
from gen_creep_roster import get_entry

MAPS_DIR = Path(__file__).parent / "maps"
OUTPUT = Path("data/sql/custom/db_world/mod_moba_towers.sql")

REQUIRED_TOWER = ["key", "team", "tier",
                  "name", "display_id", "display_scale", "health_modifier",
                  "x", "y", "z", "o",
                  "attack_range", "attack_interval_ms", "attack_spell_id"]

# Structure kind -> mod_moba_tower_data.Kind. Matches MobaStructureKind in
# MobaTowerData.h. "kind" is optional in config (default "tower").
KIND_IDS = {"tower": 0, "inhibitor": 1, "core": 2}

# Lane -> mod_moba_tower_data.Lane. Matches MobaLane in MobaTowerData.h and
# LANE_NAMES in client/addons/MobaHUD/Feed.lua -- all three must agree.
# "lane" is optional (default "none"): 0 is what a core carries, and the HUD
# renders no lane word for it.
LANE_IDS = {"none": 0, "top": 1, "mid": 2, "bot": 3}

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
    keys = set()
    for t in towers:
        for k in REQUIRED_TOWER:
            if k not in t:
                fail(f'{path}: structure {t.get("key", "?")} missing "{k}"')
        if t["key"] in keys:
            fail(f'{path}: duplicate structure key "{t["key"]}"')
        keys.add(t["key"])
        if t["team"] not in (0, 1):
            fail(f'{path}: structure {t["key"]} "team" must be 0 or 1')
        if t.get("kind", "tower") not in KIND_IDS:
            fail(f'{path}: structure {t["key"]} "kind" must be one of {sorted(KIND_IDS)}')
        if t.get("lane", "none") not in LANE_IDS:
            fail(f'{path}: structure {t["key"]} "lane" must be one of {sorted(LANE_IDS)}')
        for k in ("gold", "gold_last_hit"):
            v = t.get(k, 0)
            if not isinstance(v, int) or v < 0:
                fail(f'{path}: structure {t["key"]} "{k}" must be a non-negative integer (copper)')
    for t in towers:
        guard = t.get("guarded_by")
        if guard is None:
            continue
        if guard == t["key"]:
            fail(f'{path}: structure {t["key"]} "guarded_by" points at itself')
        if guard not in keys:
            fail(f'{path}: structure {t["key"]} "guarded_by" names "{guard}", which is '
                 f"not a structure key in this config")


def load_configs():
    configs = []
    for path in sorted(MAPS_DIR.glob("*/tower_config.yaml")):
        cfg = yaml.safe_load(path.read_text())
        validate(cfg, path)
        configs.append((path, cfg))
    if not configs:
        fail(f"no structure configs found under {MAPS_DIR}/*/tower_config.yaml")
    return configs


def assign_entries(configs):
    """Stash each structure's entry and guard entry on its dict as _entry /
    _guarded_by_entry -- the gen_creep_roster pattern.

    Two passes per config, because `guarded_by` may name a structure declared
    later in the file.
    """
    registry = id_alloc.Registry()
    # Validate what the lockfiles ALREADY hold before adding to them. The
    # allocator only ever issues inside the block, so an out-of-block id here
    # can only come from a hand-edit or a block that moved under a live lockfile.
    id_alloc.validate_owner(id_alloc.CREATURE_TEMPLATE, "towers", registry)
    alloc = id_alloc.Allocator(registry, id_alloc.CREATURE_TEMPLATE, "towers")

    assigned_log = []
    for path, cfg in configs:
        lock_path = path.with_suffix(".lock.json")
        lock = json.loads(lock_path.read_text()) if lock_path.is_file() else {}

        entries_by_key = {}
        for t in cfg["towers"]:
            t["_entry"], _ = get_entry(lock, t["key"], alloc, assigned_log)
            entries_by_key[t["key"]] = t["_entry"]
        for t in cfg["towers"]:
            guard = t.get("guarded_by")
            t["_guarded_by_entry"] = entries_by_key[guard] if guard else 0

        lock["_comment"] = ("Machine-generated by gen_tower_data.py -- do not edit. "
                            "Maps structure keys to their permanently assigned "
                            "creature_template entries.")
        lock_path.write_text(json.dumps(lock, indent=2, sort_keys=True) + "\n")
    return assigned_log, alloc.blocks


def emit(configs, blocks):
    ct_window = id_alloc.sql_window(blocks, "`entry`")
    ctm_window = id_alloc.sql_window(blocks, "`CreatureID`")
    spawn_window = id_alloc.sql_window(blocks, "`id`")
    structures = [t for _, cfg in configs for t in cfg["towers"]]

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
        "-- Lane: 0 = none (cores), 1 = top, 2 = mid, 3 = bot. Kill-feed wording only.",
        "-- ============================================================",
        "",
        "USE acore_world;",
        "",
        f"DELETE FROM `creature_template` WHERE {ct_window};",
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
            f"({t['_entry']}, {sql_str(t['name'])}, {sql_str(subname)}, 80, 80, {faction}, 0, "
            f"1.0, 1.14286, 1, 1, 32768, 2048, "
            f"9, 0, 0, {t['health_modifier']}, {armor}, "
            f"0, 0, 0, 'npc_moba_tower', 0)")
    lines.append(",\n".join(ct_rows) + ";")

    lines += [
        "",
        "-- Structures spawn from C++, never from `creature` rows, so this normally",
        "-- deletes nothing. It sweeps GM `.npc add` test spawns. The spawn table's",
        "-- entry column is `id`, not `id1`: upstream 2026_06_16_00.sql renamed it.",
        f"DELETE FROM `creature` WHERE {spawn_window};",
        "",
        f"DELETE FROM `creature_template_model` WHERE {ctm_window};",
        "INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)",
        "VALUES",
    ]
    ctm_rows = [f"({t['_entry']}, 0, {t['display_id']}, {t['display_scale']}, 1, 0)"
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
        "    `Lane`             TINYINT UNSIGNED NOT NULL DEFAULT 0,  -- 0 none, 1 top, 2 mid, 3 bot",
        "    `GuardedByEntry`   INT UNSIGNED NOT NULL DEFAULT 0,      -- 0 = none / always vulnerable",
        "    `Kind`             TINYINT UNSIGNED NOT NULL DEFAULT 0,  -- 0 tower, 1 inhibitor, 2 core",
        "    `RespawnMs`        INT UNSIGNED NOT NULL DEFAULT 0,      -- inhibitor respawn delay; 0 = never",
        "    `PosX`             FLOAT NOT NULL,",
        "    `PosY`             FLOAT NOT NULL,",
        "    `PosZ`             FLOAT NOT NULL,",
        "    `Orientation`      FLOAT NOT NULL,",
        "    `AttackRange`      FLOAT NOT NULL DEFAULT 40,",
        "    `AttackIntervalMs` INT UNSIGNED NOT NULL DEFAULT 1500,",
        "    `AttackSpellId`    INT UNSIGNED NOT NULL DEFAULT 9053,",
        "    `TeamGold`         INT UNSIGNED NOT NULL DEFAULT 0,      -- copper to EVERY player on the destroying team",
        "    `LastHitGold`      INT UNSIGNED NOT NULL DEFAULT 0       -- copper to the killing-blow player only",
        ");",
        "",
        "INSERT INTO `mod_moba_tower_data`",
        "(`CreatureEntry`, `Map`, `Team`, `Tier`, `Lane`, `GuardedByEntry`, `Kind`, `RespawnMs`, `PosX`, `PosY`, `PosZ`, `Orientation`, `AttackRange`, `AttackIntervalMs`, `AttackSpellId`, `TeamGold`, `LastHitGold`)",
        "VALUES",
    ]
    data_rows = []
    for _, cfg in configs:
        for t in cfg["towers"]:
            data_rows.append(
                f"({t['_entry']}, {cfg['map']}, {t['team']}, {t['tier']}, "
                f"{LANE_IDS[t.get('lane', 'none')]}, {t['_guarded_by_entry']}, "
                f"{KIND_IDS[t.get('kind', 'tower')]}, {t.get('respawn_ms', 0)}, "
                f"{t['x']}, {t['y']}, {t['z']}, {t['o']}, "
                f"{t['attack_range']}, {t['attack_interval_ms']}, {t['attack_spell_id']}, "
                f"{t.get('gold', 0)}, {t.get('gold_last_hit', 0)})")
    lines.append(",\n".join(data_rows) + ";")
    return "\n".join(lines) + "\n"


def main():
    configs = load_configs()
    assigned_log, blocks = assign_entries(configs)
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(emit(configs, blocks))
    total = sum(len(c["towers"]) for _, c in configs)
    print(f"Wrote {OUTPUT} ({total} structures across {len(configs)} map(s)).")
    if assigned_log:
        print("Newly assigned creature entries (now locked):")
        for key, entry in assigned_log:
            print(f"  {key}: {entry}")
    else:
        print("All creature entries reused from lockfiles.")
    print("Restart worldserver — mod_moba_towers.sql auto-applies on boot.")


if __name__ == "__main__":
    main()
