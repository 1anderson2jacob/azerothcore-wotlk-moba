#!/usr/bin/env python3
"""
MOBA creep-roster generator.

Reads a human-owned creep config (per-creep choices: stats tuning, role,
team, display, equipment, lane/formation slot) plus verbatim source-creature
stat dumps, and generates data/sql/custom/mod_moba_creeps.sql wholesale:
creature_template (full-stat copy of the source with a fixed set of
deliberate overrides), creature_template_model, creature_equip_template,
and the mod_moba_creep_data config table.

WaypointPathId is resolved from the lane generator's lockfile
(lane_config.lock.json): a creep uses its lane/slot's "forward" path for
team 0 (Alliance) and "reverse" for team 1 (Horde) -- matching how
BattlegroundMOBA wires teams. Nothing is typed twice.

creature_template entries are auto-assigned from the config's id_range on
first use and persisted to <config-stem>.lock.json. Later runs reuse them,
so re-tuning a creep never changes its entry. Do not hand-edit the lockfile.

Source dumps: verbatim output of
    mysql -E -u acore -pacore acore_world -e \
        "SELECT * FROM creature_template WHERE entry=<id>" > sources/creature_template_<id>.txt

Overrides ALWAYS enforced in code (the hard-won checklist from
.github/MOBA_GUIDE.md, so it can't be forgotten): entry, name/subname,
minlevel=maxlevel=level, faction from team (84/83), npcflag=0,
difficulty_entry_1/2/3=0 (no heroic-counterpart references), IconName=NULL,
lootid/pickpocketloot/skinloot=0, VehicleId=0, AIName='',
ScriptName='npc_moba_creep', HealthModifier/ArmorModifier from config,
RegenHealth=0 (LoL-style: damage persists), movementId=0,
CreatureImmunitiesId=0, VerifiedBuild=0.
...RegenHealth=0 (LoL-style: damage persists), movementId=0,
CreatureImmunitiesId=0, VerifiedBuild=0. A per-creep "rank" field in the
config optionally overrides the source creature's rank (0=normal, 1=elite).


Usage (from the repo root):
    python3 apps/moba/gen_creep_roster.py [path/to/creep_config.json]
"""

import json
import re
import sys
from pathlib import Path

DEFAULT_CONFIG = Path(__file__).parent / "creep_config.json"

ROLE_IDS = {"melee": 0, "caster": 1, "siege": 2}
STRING_COLUMNS = {"name", "subname", "IconName", "AIName", "ScriptName"}
NUMBER_RE = re.compile(r"^-?\d+(\.\d+)?$")
DUMP_LINE_RE = re.compile(r"^\s*(\w+): ?(.*)$")
DUMP_ROW_HEADER_RE = re.compile(r"^\*+ *\d+\. row *\*+$")

CREEP_REQUIRED = ["key", "name", "subname", "team", "role", "source", "display_id",
                  "display_scale", "level", "health_modifier", "armor_modifier",
                  "equip", "despawn_ms", "lane", "slot"]
CASTER_REQUIRED = ["attack_range", "attack_interval_ms", "attack_spell_id"]

# Columns the generator overrides -- must exist in every source dump.
OVERRIDDEN_COLUMNS = ["entry", "name", "subname", "minlevel", "maxlevel", "faction",
                      "difficulty_entry_1", "difficulty_entry_2", "difficulty_entry_3", "IconName",
                      "npcflag", "lootid", "pickpocketloot", "skinloot", "VehicleId",
                      "AIName", "ScriptName", "HealthModifier", "ArmorModifier",
                      "RegenHealth", "movementId", "CreatureImmunitiesId", "VerifiedBuild"]


def fail(msg):
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def note(msg):
    print(f"  {msg}")


# ---------------------------------------------------------------- validation

def validate_config(cfg):
    id_range = cfg.get("id_range")
    if (not isinstance(id_range, list) or len(id_range) != 2
            or not all(isinstance(v, int) for v in id_range) or id_range[0] > id_range[1]):
        fail('config "id_range" must be [low, high] with low <= high')
    if not isinstance(cfg.get("output"), str) or not cfg["output"]:
        fail('config "output" must be a file path string')
    if not isinstance(cfg.get("lane_config"), str) or not cfg["lane_config"]:
        fail('config "lane_config" must point at the lane generator config')

    creeps = cfg.get("creeps")
    if not isinstance(creeps, list) or not creeps:
        fail('config "creeps" must be a non-empty list')
    keys = set()
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


    warn_composition(creeps)


def warn_composition(creeps):
    # BattlegroundMOBA::SetupBattleground requires 2 melee + 1 caster per
    # team (fails BG creation otherwise) and warns if siege is missing.
    for team, label in ((0, "Alliance"), (1, "Horde")):
        roles = [c["role"] for c in creeps if c["team"] == team]
        if roles.count("melee") != 2 or roles.count("caster") != 1:
            note(f"WARNING: {label} has {roles.count('melee')} melee / "
                 f"{roles.count('caster')} caster -- BattlegroundMOBA expects "
                 f"exactly 2 melee + 1 caster per team and will fail to create "
                 f"the battleground otherwise")
        if roles.count("siege") != 1:
            note(f"WARNING: {label} has {roles.count('siege')} siege units -- "
                 f"expected 1 (BG only warns, waves just spawn without siege)")


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

def collect_used_entries(scan_dirs):
    used = set()
    delete_re = re.compile(
        r"DELETE\s+FROM\s+`?creature_template`?\s+WHERE\s+`?entry`?\s+IN\s*\(([^)]*)\)", re.I)
    for d in scan_dirs:
        path = Path(d)
        if not path.is_dir():
            note(f'scan dir "{d}" not found -- skipping')
            continue
        for sql_file in sorted(path.glob("*.sql")):
            for m in delete_re.finditer(sql_file.read_text()):
                used.update(int(t) for t in re.findall(r"\d+", m.group(1)))
    return used


def get_entry(lock, key, used, id_range, assigned_log):
    entries = lock.setdefault("entries", {})
    if key in entries:
        return entries[key], False
    candidate = id_range[0]
    while candidate in used:
        candidate += 1
        if candidate > id_range[1]:
            fail(f"id_range {id_range} exhausted -- no free creature entries left")
    entries[key] = candidate
    used.add(candidate)
    assigned_log.append((key, candidate))
    return candidate, True


# ------------------------------------------------------------------ sql emit

def sql_value(col, raw):
    if raw == "NULL":
        return "NULL"
    if col in STRING_COLUMNS:
        return "'" + raw.replace("'", "''") + "'"
    if not NUMBER_RE.match(raw):
        fail(f"column {col}: non-numeric value {raw!r} in a numeric column "
             f"(new string column? add it to STRING_COLUMNS)")
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
        "lootid": "0",
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
        "VerifiedBuild": "0",
    })
    # Optional per-creep overrides (default: source creature's value)
    if "rank" in creep:
        row["rank"] = str(creep["rank"])
    return row


def emit_sql(cfg, config_path, roster, column_order):
    try:
        config_path = config_path.resolve().relative_to(Path.cwd())
    except ValueError:
        pass  # config outside the repo -- keep the path as given

    entries = ", ".join(str(entry) for _, entry, _ in roster)
    lines = [
        "-- ============================================================",
        "-- GENERATED FILE -- do not hand-edit.",
        f"-- Produced by apps/moba/gen_creep_roster.py from {config_path}.",
        "-- Stats are full copies of real source creatures (see the config's",
        "-- \"source\" fields) with a fixed override list enforced in code --",
        "-- see the generator's docstring for the list and rationale.",
        "-- Waypoint paths live in mod_moba_creep_paths.sql (gen_creep_paths.py).",
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
    for creep, entry, template_row in roster:
        values = ",".join(sql_value(c, template_row[c]) for c in column_order)
        rows.append(f"-- {creep['key']} (from {Path(creep['source']).name})\n({values})")
    lines.append(",\n".join(rows) + ";")

    lines += [
        "",
        f"DELETE FROM `creature_template_model` WHERE `CreatureID` IN ({entries});",
        "INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)",
        "VALUES",
        ",\n".join(f"({entry}, 0, {creep['display_id']}, {creep['display_scale']}, 1, 0)"
                   for creep, entry, _ in roster) + ";",
        "",
        f"DELETE FROM `creature_equip_template` WHERE `CreatureID` IN ({entries});",
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
        "-- Role: 0=melee, 1=caster, 2=siege. AttackRange/AttackIntervalMs/",
        "-- AttackSpellId apply to casters only (melee/siege use default engine",
        "-- auto-attack). WaypointPathId comes from the lane generator lockfile.",
        "DROP TABLE IF EXISTS `mod_moba_creep_data`;",
        "CREATE TABLE `mod_moba_creep_data` (",
        "    `CreatureEntry`    INT UNSIGNED NOT NULL PRIMARY KEY,",
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
        "(`CreatureEntry`, `Team`, `Role`, `AttackRange`, `AttackIntervalMs`, `AttackSpellId`, `WaypointPathId`, `DespawnMs`)",
        "VALUES",
    ]
    data_rows = []
    for creep, entry, _ in roster:
        data_rows.append(
            f"-- {creep['key']} ({creep['lane']}/{creep['slot']})\n"
            f"({entry}, {creep['team']}, {ROLE_IDS[creep['role']]}, "
            f"{creep.get('attack_range', 20)}, {creep.get('attack_interval_ms', 2000)}, "
            f"{creep.get('attack_spell_id', 0)}, {creep['_path_id']}, {creep['despawn_ms']})")
    lines.append(",\n".join(data_rows) + ";")
    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------- main

def main():
    config_path = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_CONFIG
    if not config_path.is_file():
        fail(f"config not found: {config_path}")
    cfg = json.loads(config_path.read_text())
    validate_config(cfg)

    lane_lock_path = Path(cfg["lane_config"]).with_suffix(".lock.json")
    if not lane_lock_path.is_file():
        fail(f"lane lockfile not found: {lane_lock_path} -- run gen_creep_paths.py first")
    lane_lock = json.loads(lane_lock_path.read_text()).get("path_ids", {})

    lock_path = config_path.with_suffix(".lock.json")
    lock = json.loads(lock_path.read_text()) if lock_path.is_file() else {}

    used = collect_used_entries(cfg.get("scan_sql_dirs", ["data/sql/custom"]))
    used.update(lock.get("entries", {}).values())

    assigned_log = []
    roster = []  # (creep, entry, template_row)
    column_order = None
    for creep in cfg["creeps"]:
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
                 f"-- were they taken from the same schema?")

        entry, _ = get_entry(lock, creep["key"], used, cfg["id_range"], assigned_log)
        roster.append((creep, entry, build_template_row(creep, entry, source_cols)))

    output_path = Path(cfg["output"])
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(emit_sql(cfg, config_path, roster, column_order))

    lock["_comment"] = ("Machine-generated by gen_creep_roster.py -- do not edit. "
                        "Maps creep keys to their permanently assigned "
                        "creature_template entries.")
    lock_path.write_text(json.dumps(lock, indent=2, sort_keys=True) + "\n")

    print(f"\nWrote {output_path} ({len(roster)} creeps).")
    if assigned_log:
        print("Newly assigned creature entries (now locked):")
        for key, entry in assigned_log:
            print(f"  {key}: {entry}")
    else:
        print("All creature entries reused from lockfile.")
    print("Apply the SQL to acore_world, then fully restart worldserver.")


if __name__ == "__main__":
    main()
