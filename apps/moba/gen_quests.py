#!/usr/bin/env python3
"""
MOBA quest generator.

Reads per-map quest configs (apps/moba/maps/<mode>/quest_config.yaml) and generates
data/sql/custom/db_world/mod_moba_quests.sql in full:
  * creature_template / creature_template_model -- the givers, one entry per team,
                          and one invisible credit entry per animal
  * creature            -- the givers' spawns, guid == entry as gen_store.py spawns
                          shopkeepers
  * quest_template, quest_template_addon, quest_details, quest_request_items,
    quest_offer_reward  -- one quest per tier of every (reward set, animal) chain
  * creature_queststarter / creature_questender
  * conditions          -- at most one quest per animal in the log
  * mod_moba_quest_block  -- every quest by map, and which ones restart their chain
  * mod_moba_quest_credit -- the creatures whose last hit feeds a credit entry
  * item_template       -- the baked rewards, through item_copy.emit_baked_sql
plus apps/moba/item_copies/quests.json, the Item.dbc manifest for the client patch.

A quest is written once per animal and expanded per reward set: every chain of an
animal shares its objective, text and credit entry, while emotes belong to the giver.
Every id is allocated through id_alloc.py and recorded in <config-stem>.lock.json, one
{key: id} section per namespace. A last_hit quest's `targets` resolve through the
bundle's creep and neutral configs and their lockfiles, so gen_creep_roster.py and
gen_neutral_camps.py run first.

Usage (from the repo root):
    python3 apps/moba/gen_quests.py
"""

import json
import re
import sys
from pathlib import Path

import yaml

import id_alloc
from item_copy import emit_baked_sql, write_pair_manifest
from sql_dump import parse_tuple_at, unquote

MAPS_DIR = Path(__file__).parent / "maps"
OUTPUT = Path("data/sql/custom/db_world/mod_moba_quests.sql")
ITEM_TEMPLATE_SQL = Path("data/sql/base/db_world/item_template.sql")
SUFFIX_POINTS = Path(__file__).parent / "suffix_points.json"

# The id_blocks.json owner in every namespace, and the baked copies' tracker tag.
OWNER = "quests"

QUEST_TYPE_NORMAL = 2                    # quest_template.QuestType; 0 auto-completes
QUEST_INFO_RAID = 62                     # QuestInfoID; the type a battleground's raid group
                                         # can progress (Quest::IsAllowedInRaid)
CONDITION_SOURCE_QUEST_AVAILABLE = 19
CONDITION_QUESTSTATE = 47
QUEST_HELD = (1 << 1) | (1 << 3)         # QUEST_STATUS_COMPLETE | QUEST_STATUS_INCOMPLETE

TEAM_FACTION = {0: 84, 1: 83}            # the towers'; reaction is what keeps the other
                                         # team from talking to a giver
GIVER_NPCFLAG = 2                        # UNIT_NPC_FLAG_QUESTGIVER
GIVER_UNIT_FLAGS = 0x2 | 0x100 | 0x200   # NON_ATTACKABLE | IMMUNE_TO_PC | IMMUNE_TO_NPC
CREDIT_DISPLAY_ID = 11686                # invisible, as stock kill-credit creatures use
CREDIT_FLAGS_EXTRA = 0x80                # CREATURE_FLAG_EXTRA_TRIGGER
CREATURE_TYPE_HUMANOID = 7
CREATURE_TYPE_NOT_SPECIFIED = 10

# The InventoryType each slot key must carry, and its RandPropPoints column -- the
# switch in GenerateEnchSuffixFactor.
SLOT_INVTYPE = {"wrist": 9, "hands": 10, "waist": 6, "feet": 8, "neck": 2, "ring": 11}
SUFFIX_SLOT_CLASS = {6: 1, 8: 1, 10: 1, 2: 2, 9: 2, 11: 2}
QUALITY_POINTS = {2: "uncommon", 3: "rare", 4: "epic"}
MAX_ITEM_STATS = 10
MAX_ITEM_SPELLS = 5
MAX_REWARD_CHOICES = 6                   # QUEST_REWARD_CHOICES_COUNT

BUILT_OBJECTIVES = {"last_hit"}          # each other type arrives with its feed
EMOTE_SCREENS = ("offer", "incomplete", "turn_in")
DIALOGUE_FIELDS = ("summary", "offer", "progress", "complete")

# A last_hit quest's targets.<kind> names rows of <config>.yaml by `field`.
TARGET_SOURCES = {
    "creeps": {"config": "creep_config", "rows": "creeps", "field": "unit",
               "generator": "gen_creep_roster.py"},
    "neutrals": {"config": "neutral_config", "rows": "mobs", "field": "key",
                 "generator": "gen_neutral_camps.py"},
}


def fail(msg):
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def sql_str(s):
    return "'" + str(s).replace("\\", "\\\\").replace("'", "''") + "'"


def load_bases(entries):
    """entry -> {column: value}, read from the committed item_template dump."""
    if not ITEM_TEMPLATE_SQL.exists():
        fail(f"{ITEM_TEMPLATE_SQL} not found -- run from the repo root")
    text = ITEM_TEMPLATE_SQL.read_text(encoding="utf-8", errors="replace")
    m = re.search(r"CREATE TABLE `item_template` \((.*?)\n\) ENGINE", text, re.S)
    if not m:
        fail(f"{ITEM_TEMPLATE_SQL}: no CREATE TABLE to read the columns from")
    cols = re.findall(r"^\s*`([A-Za-z0-9_]+)`\s", m.group(1), re.M)
    rows = {}
    for m in re.finditer(r"\((" + "|".join(str(e) for e in sorted(entries)) + r"),", text):
        fields = parse_tuple_at(text, m.start())
        # An entry also turns up as another row's value, so a tuple counts only when
        # it starts with the entry and has every column.
        if fields and len(fields) == len(cols) and fields[0] == m.group(1):
            rows.setdefault(int(m.group(1)), dict(zip(cols, (unquote(f) for f in fields))))
    if missing := sorted(set(entries) - set(rows)):
        fail(f"base items not found in {ITEM_TEMPLATE_SQL}: {missing}")
    return rows


def validate(cfg, path, suffixes, unsupported, points):
    def need(ok, msg):
        if not ok:
            fail(f"{path}: {msg}")

    # Lockfile keys join names with "/", and a giver's key is told from a credit
    # entry's by containing one.
    def name_ok(key):
        return isinstance(key, str) and key and "/" not in key

    for key in ("map", "quest_sort_id", "quest_level", "item_level"):
        need(isinstance(cfg.get(key), int), f'"{key}" must be an integer')
    need(str(cfg["item_level"]) in points,
         f'item_level {cfg["item_level"]} has no RandPropPoints row in {SUFFIX_POINTS.name}')
    need(isinstance(cfg.get("repeat_chains"), bool), '"repeat_chains" must be true or false')

    quests = cfg.get("quests")
    need(isinstance(quests, list) and quests, '"quests" must be a non-empty list')
    quest_keys = set()
    for q in quests:
        key = q.get("key")
        need(name_ok(key), 'every quest needs a "key", without "/"')
        need(key not in quest_keys, f'quest "{key}" is defined twice')
        quest_keys.add(key)
        sid = str(q.get("suffix"))
        need(sid not in unsupported,
             f'quest "{key}": suffix {sid} cannot be baked -- {unsupported.get(sid)}')
        need(sid in suffixes, f'quest "{key}": suffix {sid} is not in {SUFFIX_POINTS.name}')
        need(isinstance(q.get("title"), str) and q["title"], f'quest "{key}" needs a "title"')
        obj = q.get("objective") or {}
        need(obj.get("type") in BUILT_OBJECTIVES,
             f'quest "{key}": objective type {obj.get("type")!r} is not built yet '
             f'(built: {sorted(BUILT_OBJECTIVES)})')
        need(isinstance(obj.get("text"), str) and obj["text"],
             f'quest "{key}": objective needs "text"')
        if obj["type"] == "last_hit":
            targets = obj.get("targets")
            need(isinstance(targets, dict) and targets and set(targets) <= set(TARGET_SOURCES),
                 f'quest "{key}": a last_hit objective needs "targets", a mapping whose keys '
                 f'are among {sorted(TARGET_SOURCES)}')
            for kind, names in targets.items():
                need(isinstance(names, list) and names and all(isinstance(n, str) for n in names),
                     f'quest "{key}": targets.{kind} must be a non-empty list of names')
                need(len(set(names)) == len(names),
                     f'quest "{key}": targets.{kind} names something twice')
        counts = q.get("counts")
        need(isinstance(counts, list) and counts
             and all(isinstance(c, int) and c > 0 for c in counts),
             f'quest "{key}": "counts" must be a non-empty list of positive integers')
        dialogue = q.get("dialogue") or {}
        for field in DIALOGUE_FIELDS:
            need(isinstance(dialogue.get(field), str) and dialogue[field],
                 f'quest "{key}": dialogue needs "{field}"')

    sets = cfg.get("reward_sets")
    need(isinstance(sets, dict) and sets, '"reward_sets" must be a non-empty mapping')
    offered = set()
    for set_key, rs in sets.items():
        need(name_ok(set_key), f'reward set "{set_key}": a key may not contain "/"')
        animals = rs.get("animals")
        need(isinstance(animals, list) and animals, f'reward set "{set_key}" needs "animals"')
        need(len(set(animals)) == len(animals), f'reward set "{set_key}" lists an animal twice')
        for a in animals:
            need(a in quest_keys, f'reward set "{set_key}": "{a}" is not a quest key')
        offered.update(animals)
        bases = rs.get("bases")
        need(isinstance(bases, dict) and 0 < len(bases) <= MAX_REWARD_CHOICES,
             f'reward set "{set_key}" needs 1-{MAX_REWARD_CHOICES} "bases" -- a quest '
             f'offers at most {MAX_REWARD_CHOICES} reward choices')
        for slot, base in bases.items():
            need(slot in SLOT_INVTYPE,
                 f'reward set "{set_key}": slot "{slot}" must be one of {sorted(SLOT_INVTYPE)}')
            need(isinstance(base, int), f'reward set "{set_key}": "{slot}" must be an item entry')
    for key in sorted(quest_keys - offered):
        need(False, f'quest "{key}" is in no reward set\'s "animals"')

    givers = cfg.get("givers")
    need(isinstance(givers, list) and givers, '"givers" must be a non-empty list')
    giver_keys, served = set(), set()
    for g in givers:
        key = g.get("key")
        need(name_ok(key), 'every giver needs a "key", without "/"')
        need(key not in giver_keys, f'giver "{key}" is defined twice')
        giver_keys.add(key)
        for field in ("reward_set", "name", "subname", "emotes", "teams"):
            need(field in g, f'giver "{key}" missing "{field}"')
        need(g["reward_set"] in sets, f'giver "{key}": no reward set "{g["reward_set"]}"')
        need(g["reward_set"] not in served, f'reward set "{g["reward_set"]}" has two givers')
        served.add(g["reward_set"])
        need(isinstance(g["emotes"], dict) and all(
                 isinstance(g["emotes"].get(s), int) for s in EMOTE_SCREENS),
             f'giver "{key}": emotes needs an integer for each of {EMOTE_SCREENS} (0 = none)')
        need(len(g["teams"]) == 2 and {t.get("team") for t in g["teams"]} == {0, 1},
             f'giver "{key}" needs one team 0 block and one team 1 block')
        for t in g["teams"]:
            for field in ("display_id", "x", "y", "z", "o"):
                need(field in t, f'giver "{key}" team {t["team"]} missing "{field}"')
    for set_key in sets:
        need(set_key in served, f'reward set "{set_key}" has no giver')


def bake(base, row, slot, suffix, item_level, points, where):
    """(name, [(ITEM_MOD_*, value), ...]) for `row` with `suffix` written in.

    value = AllocationPct * RandPropPoints / 10000, truncated -- what ApplyEnchantment
    computes for a rolled suffix (PlayerStorage.cpp:4494), evaluated once here.
    """
    label = f'{where}: base {base} "{row["name"]}"'
    invtype = int(row["InventoryType"])
    if invtype != SLOT_INVTYPE[slot]:
        fail(f"{label} is InventoryType {invtype}, but {slot} is {SLOT_INVTYPE[slot]}")
    if int(row["RandomSuffix"]):
        fail(f'{label} rolls from suffix group {row["RandomSuffix"]}; a base must not')
    if any(int(row[f"stat_value{i}"]) for i in range(1, MAX_ITEM_STATS + 1)) \
            or any(int(row[f"spellid_{i}"]) for i in range(1, MAX_ITEM_SPELLS + 1)):
        fail(f"{label} carries stats or spells of its own; a base must be blank")
    quality = int(row["Quality"])
    if quality not in QUALITY_POINTS:
        fail(f"{label} is quality {quality}; suffix points exist for {sorted(QUALITY_POINTS)}")
    factor = points[str(item_level)][QUALITY_POINTS[quality]][SUFFIX_SLOT_CLASS[invtype]]
    stats = [(stat, pct * factor // 10000) for stat, pct in suffix["effects"]]
    if not 0 < len(stats) <= MAX_ITEM_STATS or any(value <= 0 for _stat, value in stats):
        fail(f'{label} {suffix["name"]} bakes to {stats} at item level {item_level}')
    return f'{row["name"]} {suffix["name"]}', stats


def take(lock, alloc, key, log):
    """`key`'s locked id in `alloc`'s namespace, or a fresh one, locked.

    A locked id returns before any block check, so moving a block renumbers nothing.
    """
    ids = lock.setdefault(alloc.namespace, {})
    if key not in ids:
        ids[key] = alloc.take()[0]
        log.append((alloc.namespace, key, ids[key]))
    return ids[key]


def render(text, count, where):
    out = text.replace("{count}", str(count))
    if m := re.search(r"\{[^}]*\}", out):
        fail(f"{where}: unknown placeholder {m.group(0)} -- only {{count}} is filled in")
    return out


def load_targets(path, kind):
    """(rows, lock entries, config path, lock path) that `path`'s targets.<kind> resolve
    through, read from its bundle."""
    src = TARGET_SOURCES[kind]
    cfg_path = path.parent / f'{src["config"]}.yaml'
    lock_path = cfg_path.with_suffix(".lock.json")
    for needed in (cfg_path, lock_path):
        if not needed.is_file():
            fail(f"{needed} not found -- {path}'s targets.{kind} resolve through it; "
                 f'run {src["generator"]} first')
    rows = (yaml.safe_load(cfg_path.read_text()) or {}).get(src["rows"]) or []
    return rows, json.loads(lock_path.read_text()).get("entries", {}), cfg_path, lock_path


def resolve_kill_credits(path, cfg, credits):
    """(creature entry, credit entry, row key) for every creep or neutral row a
    last_hit quest's `targets` names."""
    out, claimed = [], {}
    for q in cfg["quests"]:
        if q["objective"]["type"] != "last_hit":
            continue
        for kind, names in q["objective"]["targets"].items():
            src = TARGET_SOURCES[kind]
            rows, entries, cfg_path, lock_path = load_targets(path, kind)
            for name in names:
                keys = [r["key"] for r in rows if r.get(src["field"]) == name]
                if not keys:
                    fail(f'{path}: quest "{q["key"]}": no row under "{src["rows"]}" in '
                         f'{cfg_path} has {src["field"]} "{name}"')
                for key in keys:
                    if key not in entries:
                        fail(f'{path}: "{key}" is not in {lock_path} -- run {src["generator"]}')
                    entry = entries[key]
                    if entry in claimed:
                        fail(f'{path}: "{key}" is a target of both "{claimed[entry]}" and '
                             f'"{q["key"]}" -- mod_moba_quest_credit gives a creature one credit')
                    claimed[entry] = q["key"]
                    out.append((entry, credits[q["key"]], key))
    return out


def build_map(path, cfg, lock, allocs, log, suffixes, points, bases):
    """One config's rows. Ids are taken in config order, so a first run numbers
    predictably."""
    creatures, items, quest_ids = (allocs[ns] for ns in (
        id_alloc.CREATURE_TEMPLATE, id_alloc.ITEM_TEMPLATE, id_alloc.QUEST_TEMPLATE))
    quests = {q["key"]: q for q in cfg["quests"]}
    giver_of = {g["reward_set"]: g for g in cfg["givers"]}

    givers = []
    for g in cfg["givers"]:
        for t in sorted(g["teams"], key=lambda t: t["team"]):
            key = f'{g["key"]}/{t["team"]}'
            givers.append({"key": key, "entry": take(lock, creatures, key, log),
                           "map": cfg["map"], "giver": g, "team": t})
    credits = {key: take(lock, creatures, key, log) for key in quests}

    bakes, chains = [], {}
    for set_key, rs in cfg["reward_sets"].items():
        for animal in rs["animals"]:
            q = quests[animal]
            rewards = []
            for slot, base in rs["bases"].items():
                entry = take(lock, items, f"{set_key}/{slot}/{animal}", log)
                name, stats = bake(base, bases[base], slot, suffixes[str(q["suffix"])],
                                   cfg["item_level"], points, f'{path}: reward set "{set_key}"')
                bakes.append((base, entry, name, cfg["item_level"], stats))
                rewards.append(entry)
            ids = [take(lock, quest_ids, f"{set_key}/{animal}/{tier}", log)
                   for tier in range(1, len(q["counts"]) + 1)]
            chains[(set_key, animal)] = (ids, rewards)

    quest_rows, conditions, relations = [], [], []
    for (set_key, animal), (ids, rewards) in chains.items():
        q, g = quests[animal], giver_of[set_key]
        where = f'{path}: quest "{animal}"'
        for tier, (qid, count) in enumerate(zip(ids, q["counts"]), 1):
            quest_rows.append({
                "id": qid, "key": f"{set_key}/{animal}/{tier}", "map": cfg["map"],
                "level": cfg["quest_level"], "sort": cfg["quest_sort_id"],
                "title": f'{q["title"]} ({tier})', "objective": q["objective"]["text"],
                "credit": credits[animal], "count": count, "rewards": rewards,
                "prev": ids[tier - 2] if tier > 1 else 0,
                "next": ids[tier] if tier < len(ids) else 0,
                "resets": cfg["repeat_chains"] and tier == len(ids),
                "emotes": g["emotes"],
                **{field: render(q["dialogue"][field], count, where)
                   for field in DIALOGUE_FIELDS},
            })
        for (other_set, other_animal), (other_ids, _rewards) in chains.items():
            if other_animal == animal and other_set != set_key:
                conditions += [(qid, oid, f"{set_key}/{animal}: not while {other_set}/{animal} is held")
                               for qid in ids for oid in other_ids]
        relations += [(gv["entry"], qid) for gv in givers
                      if gv["giver"]["reward_set"] == set_key for qid in ids]

    kill_credits = [(entry, cfg["map"], credit, key)
                    for entry, credit, key in resolve_kill_credits(path, cfg, credits)]

    return {"givers": givers, "credits": list(credits.items()), "quests": quest_rows,
            "conditions": conditions, "relations": relations, "bakes": bakes,
            "kill_credits": kill_credits}


def emit(rows, blocks):
    win = id_alloc.sql_window
    creatures = blocks[id_alloc.CREATURE_TEMPLATE]
    quest_win = win(blocks[id_alloc.QUEST_TEMPLATE], "`ID`")

    def values(table, columns, tuples):
        return [f"INSERT INTO `{table}`", "(" + ", ".join(f"`{c}`" for c in columns) + ")",
                "VALUES", ",\n".join(tuples) + ";"]

    lines = [
        "-- ============================================================",
        "-- GENERATED FILE -- do not hand-edit.",
        "-- Produced by apps/moba/gen_quests.py from apps/moba/maps/*/quest_config.yaml.",
        "-- Owns the quest givers and their spawns, the invisible credit entries, every",
        "-- quest row and its text, the one-quest-per-animal conditions, the quest block",
        "-- and kill-credit tables, and the baked reward items. Every DELETE clears this",
        "-- generator's whole block (apps/moba/id_blocks.json), so anything dropped from",
        "-- config goes too.",
        "-- ============================================================",
        "",
        "USE acore_world;",
        "",
        f"DELETE FROM `creature_template` WHERE {win(creatures, '`entry`')};",
    ]
    templates = []
    for gv in rows["givers"]:
        g, t = gv["giver"], gv["team"]
        templates.append(
            f'-- {gv["key"]}\n'
            f'({gv["entry"]}, {sql_str(t.get("name", g["name"]))}, {sql_str(g["subname"])}, '
            f'80, 80, {TEAM_FACTION[t["team"]]}, {GIVER_NPCFLAG}, 1.0, 1.14286, 0, 1, '
            f"{GIVER_UNIT_FLAGS}, {CREATURE_TYPE_HUMANOID}, 0, 0, 1, 1, 1, 0, '', 0)")
    templates += [
        f"-- {key} credit\n"
        f"({entry}, {sql_str(key + ' quest credit')}, '', 1, 1, 35, 0, 1.0, 1.14286, 0, 1, "
        f"0, {CREATURE_TYPE_NOT_SPECIFIED}, 0, 0, 1, 1, 1, {CREDIT_FLAGS_EXTRA}, '', 0)"
        for key, entry in rows["credits"]]
    lines += values("creature_template",
                    ["entry", "name", "subname", "minlevel", "maxlevel", "faction", "npcflag",
                     "speed_walk", "speed_run", "rank", "unit_class", "unit_flags", "type",
                     "type_flags", "MovementType", "HealthModifier", "ArmorModifier",
                     "RegenHealth", "flags_extra", "ScriptName", "VerifiedBuild"], templates)

    lines += ["", f"DELETE FROM `creature_template_model` WHERE {win(creatures, '`CreatureID`')};"]
    lines += values("creature_template_model",
                    ["CreatureID", "Idx", "CreatureDisplayID", "DisplayScale", "Probability",
                     "VerifiedBuild"],
                    [f'({gv["entry"]}, 0, {gv["team"]["display_id"]}, 1, 1, 0)'
                     for gv in rows["givers"]]
                    + [f"({entry}, 0, {CREDIT_DISPLAY_ID}, 1, 1, 0)"
                       for _key, entry in rows["credits"]])

    # The `id` term sweeps GM `.npc add` test spawns, whose guids fall outside the block.
    lines += ["", f"DELETE FROM `creature` WHERE "
                  f"{win(blocks[id_alloc.CREATURE_SPAWN], '`guid`')} OR {win(creatures, '`id`')};"]
    lines += values("creature",
                    ["guid", "id", "map", "spawnMask", "phaseMask", "equipment_id",
                     "position_x", "position_y", "position_z", "orientation",
                     "spawntimesecs", "wander_distance", "MovementType"],
                    [f'({gv["entry"]}, {gv["entry"]}, {gv["map"]}, 1, 1, 0, {gv["team"]["x"]}, '
                     f'{gv["team"]["y"]}, {gv["team"]["z"]}, {gv["team"]["o"]}, 300, 0, 0)'
                     for gv in rows["givers"]])

    # RewardXPDifficulty 0 grants no XP at any level -- QuestXP.dbc's column 0 is all
    # zeros -- and so no max-level gold, which is derived from XP. AllowableRaces 0
    # lets one row serve both teams.
    choice_cols = [c for i in range(1, MAX_REWARD_CHOICES + 1)
                   for c in (f"RewardChoiceItemID{i}", f"RewardChoiceItemQuantity{i}")]
    quest_tuples = []
    for q in rows["quests"]:
        choices = q["rewards"] + [0] * (MAX_REWARD_CHOICES - len(q["rewards"]))
        quest_tuples.append(
            f'-- {q["key"]}\n'
            f'({q["id"]}, {QUEST_TYPE_NORMAL}, {q["level"]}, 0, {q["sort"]}, {QUEST_INFO_RAID}, '
            f'0, 0, '
            + ", ".join(f"{entry}, {1 if entry else 0}" for entry in choices)
            + f', {q["credit"]}, {q["count"]}, {sql_str(q["title"])}, {sql_str(q["summary"])}, '
            f'{sql_str(q["offer"])}, {sql_str(q["objective"])}, 0)')
    lines += ["", f"DELETE FROM `quest_template` WHERE {quest_win};"]
    lines += values("quest_template",
                    ["ID", "QuestType", "QuestLevel", "MinLevel", "QuestSortID", "QuestInfoID",
                     "RewardXPDifficulty", "AllowableRaces", *choice_cols,
                     "RequiredNpcOrGo1", "RequiredNpcOrGoCount1", "LogTitle",
                     "LogDescription", "QuestDescription", "ObjectiveText1", "VerifiedBuild"],
                    quest_tuples)

    lines += ["", f"DELETE FROM `quest_template_addon` WHERE {quest_win};"]
    lines += values("quest_template_addon", ["ID", "PrevQuestID", "NextQuestID"],
                    [f'({q["id"]}, {q["prev"]}, {q["next"]})' for q in rows["quests"]])

    # Text lives in quest_template, quest_request_items and quest_offer_reward;
    # quest_details carries only the offer screen's emotes.
    lines += ["", f"DELETE FROM `quest_details` WHERE {quest_win};"]
    lines += values("quest_details", ["ID", "Emote1"],
                    [f'({q["id"]}, {q["emotes"]["offer"]})' for q in rows["quests"]])

    lines += ["", f"DELETE FROM `quest_request_items` WHERE {quest_win};"]
    lines += values("quest_request_items", ["ID", "EmoteOnIncomplete", "CompletionText"],
                    [f'({q["id"]}, {q["emotes"]["incomplete"]}, {sql_str(q["progress"])})'
                     for q in rows["quests"]])

    lines += ["", f"DELETE FROM `quest_offer_reward` WHERE {quest_win};"]
    lines += values("quest_offer_reward", ["ID", "Emote1", "RewardText"],
                    [f'({q["id"]}, {q["emotes"]["turn_in"]}, {sql_str(q["complete"])})'
                     for q in rows["quests"]])

    relation_win = (f"{win(blocks[id_alloc.QUEST_TEMPLATE], '`quest`')} OR "
                    f"{win(creatures, '`id`')}")
    for table in ("creature_queststarter", "creature_questender"):
        lines += ["", f"DELETE FROM `{table}` WHERE {relation_win};"]
        lines += values(table, ["id", "quest"],
                        [f"({entry}, {qid})" for entry, qid in rows["relations"]])

    lines += ["", f"DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = "
                  f"{CONDITION_SOURCE_QUEST_AVAILABLE} AND "
                  f"{win(blocks[id_alloc.QUEST_TEMPLATE], '`SourceEntry`')};"]
    if rows["conditions"]:
        lines += values("conditions",
                        ["SourceTypeOrReferenceId", "SourceGroup", "SourceEntry", "SourceId",
                         "ElseGroup", "ConditionTypeOrReference", "ConditionTarget",
                         "ConditionValue1", "ConditionValue2", "ConditionValue3",
                         "NegativeCondition", "Comment"],
                        [f"({CONDITION_SOURCE_QUEST_AVAILABLE}, 0, {qid}, 0, 0, "
                         f"{CONDITION_QUESTSTATE}, 0, {oid}, {QUEST_HELD}, 0, 1, {sql_str(note)})"
                         for qid, oid, note in rows["conditions"]])

    lines += ["", "DROP TABLE IF EXISTS `mod_moba_quest_block`;",
              "CREATE TABLE `mod_moba_quest_block` (",
              "    `QuestId`     INT UNSIGNED NOT NULL,",
              "    `Map`         INT UNSIGNED NOT NULL,",
              "    `ResetsChain` TINYINT UNSIGNED NOT NULL DEFAULT 0,   -- 1 = restarts its chain",
              "    PRIMARY KEY (`QuestId`)",
              ");", ""]
    lines += values("mod_moba_quest_block", ["QuestId", "Map", "ResetsChain"],
                    [f'-- {q["key"]}\n({q["id"]}, {q["map"]}, {int(q["resets"])})'
                     for q in rows["quests"]])

    lines += ["", "DROP TABLE IF EXISTS `mod_moba_quest_credit`;",
              "CREATE TABLE `mod_moba_quest_credit` (",
              "    `CreatureEntry` INT UNSIGNED NOT NULL,",
              "    `Map`           INT UNSIGNED NOT NULL,",
              "    `CreditEntry`   INT UNSIGNED NOT NULL,",
              "    PRIMARY KEY (`CreatureEntry`)",
              ");"]
    if rows["kill_credits"]:
        lines += [""] + values("mod_moba_quest_credit", ["CreatureEntry", "Map", "CreditEntry"],
                               [f"-- {key}\n({entry}, {map_id}, {credit})"
                                for entry, map_id, credit, key in rows["kill_credits"]])

    lines += emit_baked_sql(OWNER, rows["bakes"])
    return "\n".join(lines) + "\n"


def generate(loaded, reg):
    """(SQL, {lock path: lock}, bakes, newly assigned ids) for every loaded config."""
    data = json.loads(SUFFIX_POINTS.read_text())
    suffixes, points = data["suffixes"], data["rand_prop_points"]
    for path, cfg in loaded:
        validate(cfg, path, suffixes, data["unsupported"], points)
    bases = load_bases({base for _path, cfg in loaded
                        for rs in cfg["reward_sets"].values() for base in rs["bases"].values()})

    allocs = {ns: id_alloc.Allocator(reg, ns, OWNER) for ns in (
        id_alloc.CREATURE_TEMPLATE, id_alloc.ITEM_TEMPLATE, id_alloc.QUEST_TEMPLATE)}
    rows = {k: [] for k in ("givers", "credits", "quests", "conditions", "relations", "bakes",
                            "kill_credits")}
    locks, log = {}, []
    for path, cfg in loaded:
        lock_path = path.with_suffix(".lock.json")
        lock = json.loads(lock_path.read_text()) if lock_path.is_file() else {}
        for k, v in build_map(path, cfg, lock, allocs, log, suffixes, points, bases).items():
            rows[k] += v
        locks[lock_path] = lock

    # guid == entry, so each giver's number must be legal as a spawn guid as well.
    for gv in rows["givers"]:
        if why := reg.conflicts(id_alloc.CREATURE_SPAWN, OWNER, gv["entry"],
                                id_alloc.spawn_label(gv["key"])):
            fail(f'giver {gv["key"]}: entry {gv["entry"]} cannot also be its spawn guid -- '
                 + "; ".join(why))

    blocks = {ns: alloc.blocks for ns, alloc in allocs.items()}
    blocks[id_alloc.CREATURE_SPAWN] = reg.blocks_of(id_alloc.CREATURE_SPAWN, OWNER)
    return emit(rows, blocks), locks, rows["bakes"], log


def main():
    paths = sorted(MAPS_DIR.glob("*/quest_config.yaml"))
    if not paths:
        fail(f"no quest configs found under {MAPS_DIR}/*/quest_config.yaml")
    loaded = [(path, yaml.safe_load(path.read_text())) for path in paths]
    sql, locks, bakes, log = generate(loaded, id_alloc.Registry())

    for lock_path, lock in locks.items():
        lock["_comment"] = ("Machine-generated by gen_quests.py -- do not edit. One section "
                            "per namespace, mapping content keys to permanently assigned ids.")
        lock_path.write_text(json.dumps(lock, indent=2, sort_keys=True) + "\n")
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(sql)
    manifest = write_pair_manifest(OWNER, [(src, entry) for src, entry, *_rest in bakes])

    print(f"Wrote {OUTPUT}")
    print(f"Wrote {manifest} ({len(bakes)} baked items)")
    if log:
        print("Newly assigned ids (now locked):")
        for namespace, key, id_ in log:
            print(f"  {namespace:<18} {key:<24} {id_}")
    else:
        print("All ids reused from lockfiles.")
    print("Restart worldserver -- mod_moba_quests.sql auto-applies on boot. New baked items "
          "need the client patch rebuilt (apps/moba/wmo/README.md step 7), and the client "
          "caches quest text and item data by id: clear its Cache folder after a change.")


if __name__ == "__main__":
    main()
