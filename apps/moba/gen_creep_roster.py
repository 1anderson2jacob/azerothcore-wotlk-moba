#!/usr/bin/env python3
"""
MOBA creep-roster generator.

Reads a human-owned creep config (reusable unit definitions plus per-creep
placement and overrides) plus verbatim source-creature stat dumps, and
generates data/sql/custom/db_world/mod_moba_creeps.sql wholesale:
creature_template (full-stat copy of the source with a fixed set of
deliberate overrides), creature_template_model, creature_equip_template,
and the mod_moba_creep_data config table.

Wave size is data-driven: one creep row is one unit spawned per wave, so
fielding a second siege minion is a creep row plus a lane slot to walk. A
creep may name a `unit` from the config's `units` section and override any
field it sets; overrides are wholesale per field, never merged.

WaypointPathId is resolved from each map bundle's lane generator lockfile
(lane_config.lock.json): a creep uses its lane/slot's "forward" path for
team 0 (Alliance) and "reverse" for team 1 (Horde) -- matching how
BattlegroundMOBA wires teams. Nothing is typed twice.

creature_template entries are auto-assigned from the generator's block in
apps/moba/id_blocks.json on first use and persisted to <config-stem>.lock.json.
Later runs reuse them, so re-tuning a creep never changes its entry, and
re-blocking the owner never disturbs one already assigned. Do not hand-edit
the lockfile.

Source dumps: verbatim output of
    mysql -E -u acore -pacore acore_world -e \
        "SELECT * FROM creature_template WHERE entry=<id>" > sources/creature_template_<id>.txt

Overrides ALWAYS enforced in code (the hard-won checklist from
.github/MOBA_GUIDE.md, so it can't be forgotten): entry, name/subname,
minlevel=maxlevel=level, faction from team (84/83), npcflag=0,
difficulty_entry_1/2/3=0 (no heroic-counterpart references), IconName=NULL,
pickpocketloot/skinloot=0, VehicleId=0, AIName='',
ScriptName='npc_moba_creep', HealthModifier/ArmorModifier from config,
RegenHealth=0 (LoL-style: damage persists), movementId=0,
CreatureImmunitiesId=0, VerifiedBuild=0. A per-creep "rank" field in the
config optionally overrides the source creature's rank (0=normal, 1=elite),
"creature_type" its creature type (enum CreatureType), and
"speed_walk"/"speed_run" its movement speeds.

Loot columns are driven by the optional per-creature "drops" list (machinery
shared with gen_neutral_camps.py): "item" drops become native
creature_loot_template rows (and set lootid = entry); "buff"/"gold" drops
become mod_moba_*_drops rows consumed by BattlegroundMOBA::GrantDeathDrops;
mingold/maxgold are always 0 (gold is injected in C++ so it can carry a
chance coefficient); flags_extra gains NO_PLAYER_DAMAGE_REQ on loot-bearing
mobs so a pure last hit rewards loot. Attribution (killing blow, killer's
team) is fixed up in the JustDied handlers, not here.


Usage (from the repo root):
    python3 apps/moba/gen_creep_roster.py
"""

import json
import yaml
import re
import sys
from pathlib import Path

import id_alloc

MAPS_DIR = Path(__file__).parent / "maps"
OUTPUT = Path("data/sql/custom/db_world/mod_moba_creeps.sql")

ROLE_IDS = {"melee": 0, "caster": 1, "siege": 2, "super": 3}
STRING_COLUMNS = {"name", "subname", "IconName", "AIName", "ScriptName"}
NUMBER_RE = re.compile(r"^-?\d+(\.\d+)?$")
DUMP_LINE_RE = re.compile(r"^\s*(\w+): ?(.*)$")
DUMP_ROW_HEADER_RE = re.compile(r"^\*+ *\d+\. row *\*+$")

CREEP_REQUIRED = ["key", "name", "subname", "team", "role", "source", "display_id",
                  "display_scale", "level", "health_modifier", "armor_modifier",
                  "equip", "despawn_ms", "lane", "slot"]
CASTER_REQUIRED = ["attack_range", "attack_interval_ms", "attack_spell_id"]
# Placement fields can never live in a unit definition: two creeps sharing a unit
# that carried them would resolve to the same waypoint path and spawn on top of
# each other (shipped once as super minions riding the siege slot).
CREEP_UNIT_FORBIDDEN_FIELDS = ["key", "lane", "slot"]
# unit_flags override: OR in UNIT_FLAG_PLAYER_CONTROLLED (0x8) so players can cast
# helpful spells (heals/buffs) on their own minions. The WoW client silently self-casts
# a helpful spell aimed at a plain friendly NPC; this is the flag the engine puts on the
# friendly units you CAN heal (pets, the triage/escort NPCs -- see npcs_special.cpp). It
# reroutes attack-validity to the PvP/PvC path, but creep hostility stays faction-based,
# so creep-vs-creep and tower combat are unaffected. OR'd, not set, to keep each source
# creature's own unit_flags.
CREEP_UNIT_FLAG_PLAYER_CONTROLLED = 0x8

# Optional per-creature "drops" list, shared by the creep and neutral
# generators. "buff"/"gold"/"team_gold"/"team_buff" are rolled and delivered in
# C++ at the killing blow (mod_moba_*_drops -> GrantDeathDrops); "item" rides the
# native loot system (creature_loot_template, whose Chance column the engine rolls)
# and additionally emits a type-2 drops row IF it carries a `sell` price, which
# is the only way a looted item can be sold back at the shop.
#
# "gold" goes into the CORPSE, so the last hitter walks up and collects it.
# "team_gold"/"team_buff" are paid straight to the killer's whole team with no
# corpse -- the objective payout a boss camp wants. A boss carrying a team row AND
# a plain gold row pays its team a flat share and still leaves corpse gold for
# whoever landed the blow. "team_buff" reaches LIVING players only.
DROP_REQUIRED = {"buff": "spell", "gold": "copper", "item": "item",
                 "team_gold": "copper", "team_buff": "spell"}
DROP_TYPE_IDS = {"buff": 0, "gold": 1, "item": 2, "team_gold": 3, "team_buff": 4}


# Columns the generator overrides or reads -- must exist in every source dump.
OVERRIDDEN_COLUMNS = ["entry", "name", "subname", "minlevel", "maxlevel", "faction",
                      "difficulty_entry_1", "difficulty_entry_2", "difficulty_entry_3", "IconName",
                      "npcflag", "lootid", "pickpocketloot", "skinloot", "mingold", "maxgold",
                      "flags_extra", "VehicleId", "AIName", "ScriptName", "HealthModifier",
                      "ArmorModifier", "RegenHealth", "movementId", "CreatureImmunitiesId",
                      "unit_flags", "type", "speed_walk", "speed_run", "VerifiedBuild"]


def fail(msg):
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def note(msg):
    print(f"  {msg}")


def resolve_units(cfg, path, list_key, noun, forbidden):
    """Merge each cfg[list_key] row over its named unit definition -- row wins per field.

    Shared with gen_neutral_camps.py. `forbidden` is the caller's list of fields
    that name or place one individual, so a shared unit must not carry them.
    """
    units = cfg.get("units", {})
    if not isinstance(units, dict):
        fail(f'{path}: "units" must be a mapping of unit name -> fields')
    for name, unit in units.items():
        if not isinstance(unit, dict):
            fail(f'{path}: unit "{name}" must be a mapping of fields')
        for field in forbidden:
            if field in unit:
                fail(f'{path}: unit "{name}" sets "{field}" -- a unit is shared by many '
                     f'{noun}s, and "{field}" names or places a single one; put it on '
                     f"the {noun} row")

    rows = cfg.get(list_key)
    if not isinstance(rows, list) or not rows:
        fail(f'config "{list_key}" must be a non-empty list')

    resolved = []
    used = set()
    for row in rows:
        if not isinstance(row, dict):
            fail(f'{path}: every entry under "{list_key}" must be a mapping')
        name = row.get("unit")
        if name is None:
            resolved.append(dict(row))
            continue
        if name not in units:
            fail(f'{noun} "{row.get("key")}": unknown unit "{name}" '
                 f"(defined: {', '.join(sorted(units)) or 'none'})")
        used.add(name)
        merged = dict(units[name])
        merged.update({k: v for k, v in row.items() if k != "unit"})
        resolved.append(merged)

    for name in sorted(set(units) - used):
        note(f'WARNING: unit "{name}" is defined but no {noun} uses it')
    return resolved


# ---------------------------------------------------------------- validation

def validate_config(cfg, path):
    if not isinstance(cfg.get("map"), int):
        fail('config "map" must be an integer map id (e.g. 566)')

    creeps = cfg.get("creeps")
    if not isinstance(creeps, list) or not creeps:
        fail('config "creeps" must be a non-empty list')
    keys = set()
    placements = {}
    for creep in creeps:
        key = creep.get("key")
        if not isinstance(key, str) or not key:
            fail('every creep needs a non-empty "key"')
        if key in keys:
            fail(f'duplicate creep key "{key}"')
        keys.add(key)
        for field in CREEP_REQUIRED:
            if field not in creep:
                fail(f'creep "{key}": missing "{field}"')
        if creep["team"] not in (0, 1):
            fail(f'creep "{key}": "team" must be 0 (Alliance) or 1 (Horde)')
        if creep["role"] not in ROLE_IDS:
            fail(f'creep "{key}": "role" must be one of {sorted(ROLE_IDS)}')
        if creep["role"] == "caster":
            for field in CASTER_REQUIRED:
                if field not in creep:
                    fail(f'caster creep "{key}": missing "{field}"')
        equip = creep["equip"]
        if (not isinstance(equip, list) or len(equip) != 3
                or not all(isinstance(v, int) for v in equip)):
            fail(f'creep "{key}": "equip" must be [item1, item2, item3] (0 = empty slot)')
        if "rank" in creep and not isinstance(creep["rank"], int):
            fail(f'creep "{key}": "rank" must be an integer (0=normal, 1=elite)')
        if "creature_type" in creep and not isinstance(creep["creature_type"], int):
            fail(f'creep "{key}": "creature_type" must be an integer '
                 "(enum CreatureType; 7 = Humanoid, 9 = Mechanical)")

        # One waypoint path per team/lane/slot, so two creeps sharing one would
        # spawn and walk on top of each other.
        spot = (creep["team"], creep["lane"], creep["slot"])
        if spot in placements:
            fail(f'creeps "{placements[spot]}" and "{key}" both sit at team '
                 f'{spot[0]} lane/slot "{spot[1]}/{spot[2]}" -- they would spawn on '
                 "top of each other; give one its own slot in lane_config.yaml")
        placements[spot] = key

        validate_drops(creep, f'creep "{key}"')

    validate_composition(creeps)


def validate_composition(creeps):
    # Wave size is the config's business now (one creep row = one unit per wave),
    # so there is no expected shape to check -- but a team with no creeps at all
    # fields no waves, and BattlegroundMOBA fails to create on it.
    for team, label in ((0, "Alliance"), (1, "Horde")):
        if not any(c["team"] == team for c in creeps):
            fail(f"{label} has no creeps -- BattlegroundMOBA fails to create the "
                 "battleground for a team with no wave units")


# --------------------------------------------------------------- source dumps

def parse_vertical_dump(path):
    """Parse `mysql -E` vertical output into (dict col->raw value, ordered cols)."""
    if not Path(path).is_file():
        fail(f"source dump not found: {path}")
    cols = {}
    order = []
    for line in Path(path).read_text().splitlines():
        stripped = line.strip()
        if not stripped or DUMP_ROW_HEADER_RE.match(stripped):
            continue
        m = DUMP_LINE_RE.match(line)
        if not m:
            fail(f"{path}: unparseable line: {line!r}")
        col, value = m.group(1), m.group(2)
        if col in cols:
            fail(f"{path}: column {col} appears twice -- multiple rows in the dump?")
        cols[col] = value
        order.append(col)
    if len(order) < 50:
        fail(f"{path}: only {len(order)} columns -- expected a full creature_template row")
    for col in OVERRIDDEN_COLUMNS:
        if col not in cols:
            fail(f"{path}: missing expected column {col}")
    return cols, order


# ---------------------------------------------------------------- id locking

def get_entry(lock, key, alloc, assigned_log):
    """Reuse the locked entry for `key`, or take a fresh one and lock it.

    The short-circuit is load-bearing: a locked entry is returned with NO block
    check, ever. Only new keys see the ledger, which is why re-blocking an owner
    cannot disturb anything already assigned.
    """
    entries = lock.setdefault("entries", {})
    if key in entries:
        return entries[key], False
    entry = alloc.take()[0]
    entries[key] = entry
    assigned_log.append((key, entry))
    return entry, True

# ------------------------------------------------------- on-death drops (shared)

# CREATURE_FLAG_EXTRA_NO_PLAYER_DAMAGE_REQ: without it, loot/rewards require
# players to have dealt half the mob's health (Creature::
# IsDamageEnoughForLootingAndReward) -- and lane creeps take most of their
# damage from other creeps, so a pure last hit would find an unlootable
# corpse. isAllowedToLoot re-runs the same gate at corpse-open time, so the
# C++ fixup alone can't cover it.
NO_PLAYER_DAMAGE_REQ = 0x00200000


def validate_drops(block, label):
    drops = block.get("drops", [])
    if not isinstance(drops, list):
        fail(f'{label}: "drops" must be a list')
    item_ids = set()
    for i, drop in enumerate(drops):
        where = f"{label} drops[{i}]"
        if not isinstance(drop, dict) or drop.get("type") not in DROP_REQUIRED:
            fail(f'{where}: "type" must be one of {sorted(DROP_REQUIRED)}')
        field = DROP_REQUIRED[drop["type"]]
        if not isinstance(drop.get(field), int) or drop[field] <= 0:
            fail(f'{where}: {drop["type"]} drops need an integer "{field}" > 0')
        chance = drop.get("chance", 1.0)
        if not isinstance(chance, (int, float)) or not 0 < chance <= 1:
            fail(f'{where}: "chance" must be in (0, 1] -- a coefficient, not a percent')
        if drop["type"] == "buff" and not isinstance(drop.get("duration_ms", 0), int):
            fail(f'{where}: "duration_ms" must be an integer (0 = the spell\'s default)')
        if drop["type"] != "item" and "sell" in drop:
            fail(f'{where}: "sell" applies only to item drops')
        if drop["type"] == "item":
            if not isinstance(drop.get("count", 1), int) or drop.get("count", 1) < 1:
                fail(f'{where}: "count" must be an integer >= 1')
            # Optional per-unit sell-back price. Omitted means the item has no
            # price anywhere, and npc_moba_store refuses to buy it back.
            if not isinstance(drop.get("sell", 0), int) or drop.get("sell", 0) < 0:
                fail(f'{where}: "sell" must be an integer >= 0 (copper, PER UNIT)')
            if drop["item"] in item_ids:
                fail(f'{where}: duplicate item {drop["item"]} -- creature_loot_template '
                     'keys on (Entry, Item); raise "count" instead')
            item_ids.add(drop["item"])


def apply_loot_overrides(row, entry, source_cols, drops):
    """Loot-column overrides driven by the "drops" list; call after the
    generator's own overrides so these always win."""
    row["lootid"] = str(entry) if any(d["type"] == "item" for d in drops) else "0"
    row["mingold"] = "0"
    row["maxgold"] = "0"
    flags = int(source_cols["flags_extra"])
    if any(d["type"] in ("item", "gold") for d in drops):
        flags |= NO_PLAYER_DAMAGE_REQ
    row["flags_extra"] = str(flags)


def build_drop_rows(key, entry, drops):
    """One creature's drops -> (mod_moba_*_drops rows, creature_loot_template rows)."""
    grant, loot = [], []
    for drop in drops:
        chance = f"{drop.get('chance', 1.0) * 100:g}"
        if drop["type"] == "item":
            count = drop.get("count", 1)
            loot.append(f"({entry}, {drop['item']}, 0, {chance}, 0, 1, 0, "
                        f"{count}, {count}, '{key} (moba drop)')")
            # A PRICED item drop also gets a drops row. Type 2 grants nothing --
            # the loot row above hands the item over -- it exists only to carry
            # the sell-back price. No price, no row, and the item cannot be sold.
            if drop.get("sell"):
                grant.append(f"-- {key}\n({entry}, {len(grant)}, {DROP_TYPE_IDS['item']}, "
                             f"0, 0, 0, {chance}, {drop['item']}, {drop['sell']})")
        else:
            grant.append(f"-- {key}\n({entry}, {len(grant)}, {DROP_TYPE_IDS[drop['type']]}, "
                         f"{drop.get('spell', 0)}, {drop.get('duration_ms', 0)}, "
                         f"{drop.get('copper', 0)}, {chance}, 0, 0)")
    return grant, loot


def emit_loot_template_sql(window, loot_rows):
    # creature_loot_template is a NATIVE shared table, unlike every mod_moba_*
    # table: clear only our own block, never DROP/CREATE. Clearing the BLOCK
    # rather than the roster is what removes a mob dropped from config -- a
    # roster-built DELETE can never name an entry the config no longer has.
    lines = [
        "",
        f"DELETE FROM `creature_loot_template` WHERE {window};",
    ]
    if loot_rows:
        lines += [
            "INSERT INTO `creature_loot_template`",
            "(`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`)",
            "VALUES",
            ",\n".join(loot_rows) + ";",
        ]
    return lines


def emit_drops_table_sql(table, grant_rows):
    lines = [
        "",
        "-- Drops, rolled and delivered by BattlegroundMOBA::GrantDeathDrops at",
        "-- the killing blow. Type 0 = buff (aura on the killer), 1 = gold",
        "-- (Copper injected into the corpse loot), 3 = team gold, 4 = team buff.",
        "-- The team types pay the whole killing team with no corpse; team buff",
        "-- reaches living players only. DurationMs 0 = the spell's own duration.",
        "-- Type 2 = item is the ODD ONE: the item itself comes from the",
        "-- creature_loot_template rows above, so a type-2 row grants nothing and",
        "-- carries only Item + Sell, the per-unit price npc_moba_store refunds. An",
        "-- item drop with no `sell` gets no row here and cannot be sold back.",
        "-- Chance is a percent (config x 100).",
        f"DROP TABLE IF EXISTS `{table}`;",
        f"CREATE TABLE `{table}` (",
        "    `CreatureEntry` INT UNSIGNED NOT NULL,",
        "    `Idx`           TINYINT UNSIGNED NOT NULL,",
        "    `Type`          TINYINT UNSIGNED NOT NULL,",
        "    `Spell`         INT UNSIGNED NOT NULL DEFAULT 0,",
        "    `DurationMs`    INT UNSIGNED NOT NULL DEFAULT 0,",
        "    `Copper`        INT UNSIGNED NOT NULL DEFAULT 0,",
        "    `Chance`        FLOAT NOT NULL DEFAULT 100,",
        "    `Item`          INT UNSIGNED NOT NULL DEFAULT 0,   -- type 2 only",
        "    `Sell`          INT UNSIGNED NOT NULL DEFAULT 0,   -- type 2 only, PER UNIT",
        "    PRIMARY KEY (`CreatureEntry`, `Idx`)",
        ");",
    ]
    if grant_rows:
        lines += [
            "",
            f"INSERT INTO `{table}`",
            "(`CreatureEntry`, `Idx`, `Type`, `Spell`, `DurationMs`, `Copper`, `Chance`,"
            " `Item`, `Sell`)",
            "VALUES",
            ",\n".join(grant_rows) + ";",
        ]
    return lines


# ------------------------------------------------------------------ sql emit

def sql_value(col, raw):
    if raw == "NULL":
        return "NULL"
    if col in STRING_COLUMNS:
        return "'" + raw.replace("'", "''") + "'"
    if not NUMBER_RE.match(raw):
        fail(f"column {col}: non-numeric value {raw!r} in a numeric column "
             "(new string column? add it to STRING_COLUMNS)")
    return raw


def build_template_row(creep, entry, source_cols):
    row = dict(source_cols)
    row.update({
        "entry": str(entry),
        "name": creep["name"],
        "subname": creep["subname"],
        "minlevel": str(creep["level"]),
        "maxlevel": str(creep["level"]),
        "faction": "84" if creep["team"] == 0 else "83",
        "npcflag": "0",
        "difficulty_entry_1": "0",
        "difficulty_entry_2": "0",
        "difficulty_entry_3": "0",
        "IconName": "NULL",
        "pickpocketloot": "0",
        "skinloot": "0",
        "VehicleId": "0",
        "AIName": "",
        "ScriptName": "npc_moba_creep",
        "HealthModifier": str(creep["health_modifier"]),
        "ArmorModifier": str(creep["armor_modifier"]),
        "RegenHealth": "0",
        "movementId": "0",
        "CreatureImmunitiesId": "0",
        "unit_flags": str(int(source_cols["unit_flags"]) | CREEP_UNIT_FLAG_PLAYER_CONTROLLED),
        "VerifiedBuild": "0",
    })
    # Optional per-creep overrides (default: source creature's value)
    if "rank" in creep:
        row["rank"] = str(creep["rank"])
    if "creature_type" in creep:
        row["type"] = str(creep["creature_type"])
    for field in ("speed_walk", "speed_run"):
        if field in creep:
            row[field] = str(creep[field])
    apply_loot_overrides(row, entry, source_cols, creep.get("drops", []))
    return row


def emit_sql(roster, column_order, blocks):
    ct_window = id_alloc.sql_window(blocks, "`entry`")
    ctm_window = id_alloc.sql_window(blocks, "`CreatureID`")
    loot_window = id_alloc.sql_window(blocks, "`Entry`")
    spawn_window = id_alloc.sql_window(blocks, "`id`")
    lines = [
        "-- ============================================================",
        "-- GENERATED FILE -- do not hand-edit.",
        "-- Produced by apps/moba/gen_creep_roster.py from apps/moba/maps/*/creep_config.yaml.",
        "-- Stats are full copies of real source creatures (see the config's",
        "-- \"source\" fields) with a fixed override list enforced in code --",
        "-- see the generator's docstring for the list and rationale.",
        "-- Waypoint paths live in mod_moba_creep_paths.sql (gen_creep_paths.py).",
        "-- Every DELETE clears this generator's whole ID block, not just the rows",
        "-- about to be inserted, so a creep removed from config loses its DB rows",
        "-- too. Blocks are declared in apps/moba/id_blocks.json.",
        "-- ============================================================",
        "",
        "USE acore_world;",
        "",
        f"DELETE FROM `creature_template` WHERE {ct_window};",
        "INSERT INTO `creature_template`",
        "(" + ", ".join(f"`{c}`" for c in column_order) + ")",
        "VALUES",
    ]
    rows = []
    for creep, entry, template_row in roster:
        values = ",".join(sql_value(c, template_row[c]) for c in column_order)
        rows.append(f"-- {creep['key']} (from {Path(creep['source']).name})\n({values})")
    lines.append(",\n".join(rows) + ";")

    lines += [
        "",
        "-- Creeps spawn from C++, never from `creature` rows, so this normally deletes",
        "-- nothing. It sweeps GM `.npc add` test spawns, which would otherwise sit in",
        "-- the world forever. The spawn table's entry column is `id`, not `id1`:",
        "-- upstream 2026_06_16_00.sql renamed it and moved id2/id3 to creature_multispawn.",
        f"DELETE FROM `creature` WHERE {spawn_window};",
        "",
        f"DELETE FROM `creature_template_model` WHERE {ctm_window};",
        "INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)",
        "VALUES",
        ",\n".join(f"({entry}, 0, {creep['display_id']}, {creep['display_scale']}, 1, 0)"
                   for creep, entry, _ in roster) + ";",
        "",
        f"DELETE FROM `creature_equip_template` WHERE {ctm_window};",
    ]
    equip_rows = [f"({entry}, 1, {c['equip'][0]}, {c['equip'][1]}, {c['equip'][2]}, 0)"
                  for c, entry, _ in roster if any(c["equip"])]
    if equip_rows:
        lines += [
            "INSERT INTO `creature_equip_template` (`CreatureID`, `ID`, `ItemID1`, `ItemID2`, `ItemID3`, `VerifiedBuild`)",
            "VALUES",
            ",\n".join(equip_rows) + ";",
        ]

    lines += [
        "",
        "-- Role: 0=melee, 1=caster, 2=siege, 3=super. AttackRange/AttackIntervalMs/",
        "-- AttackSpellId apply to casters only (melee/siege use default engine",
        "-- auto-attack). WaypointPathId comes from the lane generator lockfile.",
        "DROP TABLE IF EXISTS `mod_moba_creep_data`;",
        "CREATE TABLE `mod_moba_creep_data` (",
        "    `CreatureEntry`    INT UNSIGNED NOT NULL PRIMARY KEY,",
        "    `Map`              INT UNSIGNED NOT NULL,",
        "    `Team`             TINYINT UNSIGNED NOT NULL,",
        "    `Role`             TINYINT UNSIGNED NOT NULL,",
        "    `AttackRange`      FLOAT NOT NULL DEFAULT 20,",
        "    `AttackIntervalMs` INT UNSIGNED NOT NULL DEFAULT 2000,",
        "    `AttackSpellId`    INT UNSIGNED NOT NULL DEFAULT 0,",
        "    `WaypointPathId`   INT UNSIGNED NOT NULL,",
        "    `DespawnMs`        INT UNSIGNED NOT NULL DEFAULT 60000",
        ");",
        "",
        "INSERT INTO `mod_moba_creep_data`",
        "(`CreatureEntry`, `Map`, `Team`, `Role`, `AttackRange`, `AttackIntervalMs`, `AttackSpellId`, `WaypointPathId`, `DespawnMs`)",
        "VALUES",
    ]
    data_rows = []
    for creep, entry, _ in roster:
        data_rows.append(
            f"-- {creep['key']} ({creep['lane']}/{creep['slot']})\n"
            f"({entry}, {creep['_map']}, {creep['team']}, {ROLE_IDS[creep['role']]}, "
            f"{creep.get('attack_range', 20)}, {creep.get('attack_interval_ms', 2000)}, "
            f"{creep.get('attack_spell_id', 0)}, {creep['_path_id']}, {creep['despawn_ms']})")
    lines.append(",\n".join(data_rows) + ";")

    grant_rows, loot_rows = [], []
    for creep, entry, _ in roster:
        grant, loot = build_drop_rows(creep["key"], entry, creep.get("drops", []))
        grant_rows += grant
        loot_rows += loot
    lines += emit_loot_template_sql(loot_window, loot_rows)
    lines += emit_drops_table_sql("mod_moba_creep_drops", grant_rows)
    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------- main

def main():
    configs = sorted(MAPS_DIR.glob("*/creep_config.yaml"))
    if not configs:
        fail(f"no creep configs found under {MAPS_DIR}/*/creep_config.yaml")

    alloc = id_alloc.Allocator(id_alloc.Registry(), id_alloc.CREATURE_TEMPLATE, "creeps")
    locks = {}
    for cp in configs:
        lp = cp.with_suffix(".lock.json")
        locks[cp] = json.loads(lp.read_text()) if lp.is_file() else {}

    assigned_log = []
    roster = []  # (creep, entry, template_row); creep carries _map / _path_id
    column_order = None
    for cp in configs:
        cfg = yaml.safe_load(cp.read_text())
        cfg["creeps"] = resolve_units(cfg, cp, "creeps", "creep", CREEP_UNIT_FORBIDDEN_FIELDS)
        validate_config(cfg, cp)
        lock = locks[cp]

        # Waypoint paths come from this map bundle's lane lockfile.
        lane_lock_path = cp.parent / "lane_config.lock.json"
        if not lane_lock_path.is_file():
            fail(f"lane lockfile not found: {lane_lock_path} -- run gen_creep_paths.py first")
        lane_lock = json.loads(lane_lock_path.read_text()).get("path_ids", {})

        for creep in cfg["creeps"]:
            creep["_map"] = cfg["map"]
            slot_ids = lane_lock.get(creep["lane"], {}).get(creep["slot"])
            if not slot_ids:
                fail(f'creep "{creep["key"]}": lane/slot "{creep["lane"]}/{creep["slot"]}" '
                     f"not in {lane_lock_path} -- check the name, or run gen_creep_paths.py")
            creep["_path_id"] = slot_ids["forward" if creep["team"] == 0 else "reverse"]

            source_cols, order = parse_vertical_dump(creep["source"])
            if column_order is None:
                column_order = order
            elif order != column_order:
                fail(f'source dumps disagree on column order ("{creep["source"]}" vs earlier) '
                     "-- were they taken from the same schema?")

            entry, _ = get_entry(lock, creep["key"], alloc, assigned_log)
            roster.append((creep, entry, build_template_row(creep, entry, source_cols)))

        lock["_comment"] = ("Machine-generated by gen_creep_roster.py -- do not edit. "
                            "Maps creep keys to their permanently assigned "
                            "creature_template entries.")
        cp.with_suffix(".lock.json").write_text(json.dumps(lock, indent=2, sort_keys=True) + "\n")

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(emit_sql(roster, column_order, alloc.blocks))

    print(f"\nWrote {OUTPUT} ({len(roster)} creeps across {len(configs)} map(s)).")
    if assigned_log:
        print("Newly assigned creature entries (now locked):")
        for key, entry in assigned_log:
            print(f"  {key}: {entry}")
    else:
        print("All creature entries reused from lockfiles.")
    print("Restart worldserver — the SQL auto-applies from data/sql/custom/db_world on boot.")


if __name__ == "__main__":
    main()
