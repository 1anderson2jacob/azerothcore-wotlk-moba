#!/usr/bin/env python3
"""
MOBA neutral-camp (jungle) generator.

Reads a human-owned neutral config per map bundle (camps: placement, respawn
timing, aggro/leash ranges; mobs: stats/display/buff tuning) plus verbatim
source-creature dumps, and generates
data/sql/custom/db_world/mod_moba_neutrals.sql wholesale: creature_template
(full-stat copy of the source with a fixed override list -- shared machinery
with gen_creep_roster.py), creature_template_model, creature_equip_template,
and three data tables:

    mod_moba_neutral_camps    -- per camp: spawn timing
    mod_moba_neutral_members  -- per placement: camp, entry, position
    mod_moba_neutral_camps    -- per camp: spawn timing
    mod_moba_neutral_members  -- per placement: camp, entry, position
    mod_moba_neutral_data     -- per entry: behavior (aggro, leash)
    mod_moba_neutral_drops    -- per entry: on-death buff/gold drops

"item" drops additionally emit native creature_loot_template rows -- the
drops machinery is shared with gen_creep_roster.py; see its docstring.

aggro_range / leash_range are authored on the CAMP and denormalized onto each
member's per-entry rows here, because proximity aggro lives in
creature_template.detection_range -- one value per entry. A mob key placed in
two camps with different ranges is therefore ambiguous and fails the run
(give each camp its own mob keys instead). Mobs placed in no camp are
warned about and skipped entirely.

Override deltas vs the creep generator:
  - faction 14 (hostile-all), never team-derived
  - RegenHealth = 1 (creeps use 0): camps reset to full out of combat,
    League-style
  - detection_range = the camp's aggro_range: 0 disables engine proximity
    aggro AND the AI goes REACT_DEFENSIVE on it (pull-on-hit); >0 is an
    exact-radius pull at equal levels (Creature::GetAggroRange's level-diff
    term cancels)
  - unit_flags copied verbatim: no PLAYER_CONTROLLED OR-in -- that flag
    exists so players can heal their own lane minions; neutrals are hostile
  - type defaults to 1 (Beast); per-mob "creature_type" overrides
  - ScriptName = 'npc_moba_neutral'
  - equip optional (beasts carry nothing), default [0, 0, 0]

creature_template entries are auto-assigned from ID_RANGE on first use and
persisted to neutral_config.lock.json -- same rules as the creep lockfile:
committed, machine-owned, never hand-edited, never deleted.

Usage (from the repo root):
    python3 apps/moba/gen_neutral_camps.py
"""

import json
from pathlib import Path

# Shared machinery: dump parsing, SQL quoting, entry locking, drops. Note
# parse_vertical_dump validates the CREEP generator's override columns; the
# extra columns this generator stamps are checked in build_template_row.
from gen_creep_roster import (apply_loot_overrides, build_drop_rows,
                              collect_used_entries, emit_drops_table_sql,
                              emit_loot_template_sql, fail, get_entry, note,
                              parse_vertical_dump, sql_value, validate_drops)

MAPS_DIR = Path(__file__).parent / "maps"
OUTPUT = Path("data/sql/custom/db_world/mod_moba_neutrals.sql")
ID_RANGE = [900200, 900249]  # towers 900000+, creeps 900010+, waypoints 900100+
SCAN_SQL_DIRS = ["data/sql/custom/db_world"]

MOB_REQUIRED = ["key", "name", "subname", "source", "display_id", "display_scale",
                "level", "health_modifier", "armor_modifier"]
CAMP_REQUIRED = ["key", "respawn_ms", "aggro_range", "leash_range", "members"]


# ---------------------------------------------------------------- validation

def validate_config(cfg, path):
    if not isinstance(cfg.get("map"), int):
        fail('config "map" must be an integer map id (e.g. 566)')

    mobs = cfg.get("mobs")
    if not isinstance(mobs, list) or not mobs:
        fail('config "mobs" must be a non-empty list')
    mob_keys = set()
    for mob in mobs:
        key = mob.get("key")
        if not isinstance(key, str) or not key:
            fail('every mob needs a non-empty "key"')
        if key in mob_keys:
            fail(f'duplicate mob key "{key}"')
        mob_keys.add(key)
        for field in MOB_REQUIRED:
            if field not in mob:
                fail(f'mob "{key}": missing "{field}"')
        validate_drops(mob, f'mob "{key}"')
        if "equip" in mob:
            equip = mob["equip"]
            if (not isinstance(equip, list) or len(equip) != 3
                    or not all(isinstance(v, int) for v in equip)):
                fail(f'mob "{key}": "equip" must be [item1, item2, item3] (0 = empty slot)')
        if mob["display_id"] == 0:
            note(f'WARNING: mob "{key}" has display_id 0 (placeholder) -- '
                 f'invisible in-game; pick one with .morph and fill it in')

    camps = cfg.get("camps")
    if not isinstance(camps, list) or not camps:
        fail('config "camps" must be a non-empty list')
    camp_keys = set()
    placed_mobs = set()
    for camp in camps:
        key = camp.get("key")
        if not isinstance(key, str) or not key:
            fail('every camp needs a non-empty "key"')
        if key in camp_keys:
            fail(f'duplicate camp key "{key}"')
        camp_keys.add(key)
        for field in CAMP_REQUIRED:
            if field not in camp:
                fail(f'camp "{key}": missing "{field}"')
        if not isinstance(camp["respawn_ms"], int):
            fail(f'camp "{key}": "respawn_ms" must be an integer')
        if not isinstance(camp["aggro_range"], (int, float)) or camp["aggro_range"] < 0:
            fail(f'camp "{key}": "aggro_range" must be >= 0 (0 = pull-on-hit)')
        if not isinstance(camp["leash_range"], (int, float)) or camp["leash_range"] < 0:
            fail(f'camp "{key}": "leash_range" must be >= 0 (0 = engine leash only)')
        if 0 < camp["leash_range"] <= camp["aggro_range"]:
            note(f'WARNING: camp "{key}" has leash_range <= aggro_range -- members '
                 f'will evade the moment a proximity pull starts')
        members = camp.get("members")
        if not isinstance(members, list) or not members:
            fail(f'camp "{key}": "members" must be a non-empty list')
        for member in members:
            if member.get("mob") not in mob_keys:
                fail(f'camp "{key}": member mob "{member.get("mob")}" not in "mobs"')
            pos = member.get("pos")
            if (not isinstance(pos, list) or len(pos) != 4
                    or not all(isinstance(v, (int, float)) for v in pos)):
                fail(f'camp "{key}": every member needs "pos": [x, y, z, o]')
            placed_mobs.add(member["mob"])
    for key in sorted(mob_keys - placed_mobs):
        note(f'WARNING: mob "{key}" defined but placed in no camp -- skipped')


def resolve_camp_ranges(cfg):
    """Map each placed mob key -> (aggro_range, leash_range) from its camp(s).

    detection_range is a creature_template column (one value per entry), so a
    mob key shared by camps that disagree on ranges cannot be honored -- fail
    and ask for distinct keys.
    """
    ranges = {}
    for camp in cfg["camps"]:
        pair = (camp["aggro_range"], camp["leash_range"])
        for member in camp["members"]:
            prev = ranges.setdefault(member["mob"], (camp["key"], pair))
            if prev[1] != pair:
                fail(f'mob "{member["mob"]}" is placed in camps "{prev[0]}" and '
                     f'"{camp["key"]}" with different aggro/leash ranges -- one '
                     f'creature entry holds one detection_range; use distinct mob keys')
    return {key: pair for key, (_, pair) in ranges.items()}


# ------------------------------------------------------------------ sql emit

def build_template_row(mob, entry, source_cols):
    for col in ("detection_range", "type"):
        if col not in source_cols:
            fail(f'{mob["source"]}: missing expected column {col}')
    row = dict(source_cols)
    row.update({
        "entry": str(entry),
        "name": mob["name"],
        "subname": mob["subname"],
        "minlevel": str(mob["level"]),
        "maxlevel": str(mob["level"]),
        "faction": "14",  # hostile to both teams, neutral to its own camp
        "npcflag": "0",
        "difficulty_entry_1": "0",
        "difficulty_entry_2": "0",
        "difficulty_entry_3": "0",
        "IconName": "NULL",
        "pickpocketloot": "0",
        "skinloot": "0",
        "VehicleId": "0",
        "AIName": "",
        "ScriptName": "npc_moba_neutral",
        "HealthModifier": str(mob["health_modifier"]),
        "ArmorModifier": str(mob["armor_modifier"]),
        "RegenHealth": "1",
        "movementId": "0",
        "CreatureImmunitiesId": "0",
        "detection_range": str(mob["_aggro_range"]),
        "type": str(mob.get("creature_type", 1)),  # CREATURE_TYPE_BEAST
        "VerifiedBuild": "0",
    })
    if "rank" in mob:
        row["rank"] = str(mob["rank"])
    apply_loot_overrides(row, entry, source_cols, mob.get("drops", []))
    return row


def emit_sql(roster, camps, column_order):
    entries = ", ".join(str(entry) for _, entry, _ in roster)
    lines = [
        "-- ============================================================",
        "-- GENERATED FILE -- do not hand-edit.",
        "-- Produced by apps/moba/gen_neutral_camps.py from apps/moba/maps/*/neutral_config.json.",
        "-- Stats are full copies of real source creatures with a fixed override",
        "-- list enforced in code -- see the generator's docstring.",
        "-- ============================================================",
        "",
        "USE acore_world;",
        "",
        f"DELETE FROM `creature_template` WHERE `entry` IN ({entries});",
        "INSERT INTO `creature_template`",
        "(" + ", ".join(f"`{c}`" for c in column_order) + ")",
        "VALUES",
    ]
    rows = []
    for mob, entry, template_row in roster:
        values = ",".join(sql_value(c, template_row[c]) for c in column_order)
        rows.append(f"-- {mob['key']} (from {Path(mob['source']).name})\n({values})")
    lines.append(",\n".join(rows) + ";")

    lines += [
        "",
        f"DELETE FROM `creature_template_model` WHERE `CreatureID` IN ({entries});",
        "INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)",
        "VALUES",
        ",\n".join(f"({entry}, 0, {mob['display_id']}, {mob['display_scale']}, 1, 0)"
                   for mob, entry, _ in roster) + ";",
        "",
        f"DELETE FROM `creature_equip_template` WHERE `CreatureID` IN ({entries});",
    ]
    equip_rows = [f"({entry}, 1, {m['equip'][0]}, {m['equip'][1]}, {m['equip'][2]}, 0)"
                  for m, entry, _ in roster if any(m.get("equip", [0, 0, 0]))]
    if equip_rows:
        lines += [
            "INSERT INTO `creature_equip_template` (`CreatureID`, `ID`, `ItemID1`, `ItemID2`, `ItemID3`, `VerifiedBuild`)",
            "VALUES",
            ",\n".join(equip_rows) + ";",
        ]

    lines += [
        "",
        "-- CampId is positional (config order) and scoped to Map; nothing",
        "-- outside this file references it.",
        "DROP TABLE IF EXISTS `mod_moba_neutral_camps`;",
        "CREATE TABLE `mod_moba_neutral_camps` (",
        "    `Map`            INT UNSIGNED NOT NULL,",
        "    `CampId`         INT UNSIGNED NOT NULL,",
        "    `InitialSpawnMs` INT UNSIGNED NOT NULL DEFAULT 90000,",
        "    `RespawnMs`      INT UNSIGNED NOT NULL DEFAULT 120000,",
        "    PRIMARY KEY (`Map`, `CampId`)",
        ");",
        "",
        "INSERT INTO `mod_moba_neutral_camps` (`Map`, `CampId`, `InitialSpawnMs`, `RespawnMs`)",
        "VALUES",
        ",\n".join(f"-- {c['key']}\n({c['map']}, {c['camp_id']}, {c['initial_spawn_ms']}, {c['respawn_ms']})"
                   for c in camps) + ";",
        "",
        "DROP TABLE IF EXISTS `mod_moba_neutral_members`;",
        "CREATE TABLE `mod_moba_neutral_members` (",
        "    `Map`           INT UNSIGNED NOT NULL,",
        "    `CampId`        INT UNSIGNED NOT NULL,",
        "    `Idx`           TINYINT UNSIGNED NOT NULL,",
        "    `CreatureEntry` INT UNSIGNED NOT NULL,",
        "    `X` FLOAT NOT NULL,",
        "    `Y` FLOAT NOT NULL,",
        "    `Z` FLOAT NOT NULL,",
        "    `O` FLOAT NOT NULL,",
        "    PRIMARY KEY (`Map`, `CampId`, `Idx`)",
        ");",
        "",
        "INSERT INTO `mod_moba_neutral_members` (`Map`, `CampId`, `Idx`, `CreatureEntry`, `X`, `Y`, `Z`, `O`)",
        "VALUES",
    ]
    member_rows = []
    for c in camps:
        for idx, (entry, mob_key, pos) in enumerate(c["members"]):
            member_rows.append(f"-- {c['key']}/{mob_key}\n"
                               f"({c['map']}, {c['camp_id']}, {idx}, {entry}, "
                               f"{pos[0]}, {pos[1]}, {pos[2]}, {pos[3]})")
    lines.append(",\n".join(member_rows) + ";")

    lines += [
        "",
        "-- Per-entry behavior, looked up by npc_moba_neutral. Ranges are",
        "-- authored per CAMP in the config and denormalized here per entry.",
        "-- AggroRange 0 = pull-on-hit (AI goes REACT_DEFENSIVE); >0 is also",
        "-- stamped into creature_template.detection_range, giving exact-radius",
        "-- proximity aggro at equal levels. LeashRange is the hard evade cap",
        "-- measured from the camp anchor (0 = engine leash only).",
        "DROP TABLE IF EXISTS `mod_moba_neutral_data`;",
        "CREATE TABLE `mod_moba_neutral_data` (",
        "    `CreatureEntry`      INT UNSIGNED NOT NULL PRIMARY KEY,",
        "    `Map`                INT UNSIGNED NOT NULL,",
        "    `AggroRange`         FLOAT NOT NULL DEFAULT 0,",
        "    `LeashRange`         FLOAT NOT NULL DEFAULT 20",
        ");",
        "",
        "INSERT INTO `mod_moba_neutral_data`",
        "(`CreatureEntry`, `Map`, `AggroRange`, `LeashRange`)",
        "VALUES",
    ]
    data_rows = []
    for mob, entry, _ in roster:
        data_rows.append(f"-- {mob['key']}\n"
                         f"({entry}, {mob['_map']}, {mob['_aggro_range']}, {mob['_leash_range']})")
    lines.append(",\n".join(data_rows) + ";")

    grant_rows, loot_rows = [], []
    for mob, entry, _ in roster:
        grant, loot = build_drop_rows(mob["key"], entry, mob.get("drops", []))
        grant_rows += grant
        loot_rows += loot
    lines += emit_loot_template_sql(entries, loot_rows)
    lines += emit_drops_table_sql("mod_moba_neutral_drops", grant_rows)
    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------- main

def main():
    configs = sorted(MAPS_DIR.glob("*/neutral_config.json"))
    if not configs:
        fail(f"no neutral configs found under {MAPS_DIR}/*/neutral_config.json")

    used = collect_used_entries(SCAN_SQL_DIRS)
    locks = {}
    for cp in configs:
        lp = cp.with_suffix(".lock.json")
        locks[cp] = json.loads(lp.read_text()) if lp.is_file() else {}
        used.update(locks[cp].get("entries", {}).values())

    assigned_log = []
    roster = []  # (mob, entry, template_row); mob carries _map/_aggro_range/_leash_range
    camps_out = []
    column_order = None
    for cp in configs:
        cfg = json.loads(cp.read_text())
        validate_config(cfg, cp)
        ranges = resolve_camp_ranges(cfg)
        lock = locks[cp]
        default_initial = cfg.get("initial_spawn_ms", 90000)

        entries_by_key = {}
        for mob in cfg["mobs"]:
            if mob["key"] not in ranges:
                continue  # placed in no camp; warned in validate_config

            source_cols, order = parse_vertical_dump(mob["source"])
            if column_order is None:
                column_order = order
            elif order != column_order:
                fail(f'source dumps disagree on column order ("{mob["source"]}" vs earlier) '
                     f"-- were they taken from the same schema?")

            entry, _ = get_entry(lock, mob["key"], used, ID_RANGE, assigned_log)
            entries_by_key[mob["key"]] = entry
            mob["_map"] = cfg["map"]
            mob["_aggro_range"], mob["_leash_range"] = ranges[mob["key"]]
            roster.append((mob, entry, build_template_row(mob, entry, source_cols)))

        for camp_id, camp in enumerate(cfg["camps"], start=1):
            camps_out.append({
                "map": cfg["map"],
                "camp_id": camp_id,
                "key": camp["key"],
                "initial_spawn_ms": camp.get("initial_spawn_ms", default_initial),
                "respawn_ms": camp["respawn_ms"],
                "members": [(entries_by_key[m["mob"]], m["mob"], m["pos"])
                            for m in camp["members"]],
            })

        lock["_comment"] = ("Machine-generated by gen_neutral_camps.py -- do not edit. "
                            "Maps mob keys to their permanently assigned "
                            "creature_template entries.")
        cp.with_suffix(".lock.json").write_text(json.dumps(lock, indent=2, sort_keys=True) + "\n")

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(emit_sql(roster, camps_out, column_order))

    print(f"\nWrote {OUTPUT} ({len(roster)} mobs, {len(camps_out)} camps across {len(configs)} map(s)).")
    if assigned_log:
        print("Newly assigned creature entries (now locked):")
        for key, entry in assigned_log:
            print(f"  {key}: {entry}")
    else:
        print("All creature entries reused from lockfiles.")
    print("Restart worldserver -- the SQL auto-applies from data/sql/custom/db_world on boot.")


if __name__ == "__main__":
    main()
