#!/usr/bin/env python3
"""
MOBA item vendor generator.

Reads per-map vendor configs (apps/moba/maps/<mode>/store_config.yaml) and
generates data/sql/custom/db_world/mod_moba_store.sql in full:
  * creature_template / creature_template_model -- the vendor NPCs (one entry
                            per team; team lives in mod_moba_store_npc, not in
                            faction -- CFBG means faction cannot express team)
  * creature              -- their per-map spawns (guid == entry; static props,
                            so they are world spawns rather than BG-lifecycle
                            AddCreature calls, which would need new BgCreatures
                            enum slots)
  * mod_moba_store_npc    -- creature entry -> (map, team, vendor)
  * mod_moba_store_menu   -- the gossip tree, arbitrary depth
  * mod_moba_store_grant  -- the item(s) behind each purchase node

Starting-gear "of the X" pieces are stock green random-suffix bases: the vendor
stamps a suffix id onto the base at grant time. Which suffixes a base may
legally roll is DERIVED here from committed data --
    item_template.RandomSuffix  (base item -> suffix group)
    item_enchantment_template   (suffix group -> the suffix ids in it)
-- so a menu can never offer a combination the game itself would not roll (there
is no "cloth of the Tiger"). Cloth's caster-only suffixes and the wand's absence
from physical bundles fall out of this automatically; neither is hand-listed.

ItemRandomSuffix.dbc is deliberately NOT read: env/dist is gitignored build
output, so depending on it would leave a fresh clone unable to regenerate. The
suffix ids live in the config and are verified against the two tables above.

No lockfile: node ids are internal to the regenerated tables and nothing
persistent references them, so fresh ids each run are safe (unlike creep
entries / waypoint path ids).

Usage (from the repo root):
    python3 apps/moba/gen_store.py
"""

import re
import sys
from collections import defaultdict
from pathlib import Path

import yaml

MAPS_DIR = Path(__file__).parent / "maps"
OUTPUT = Path("data/sql/custom/db_world/mod_moba_store.sql")
ITEM_TEMPLATE_SQL = Path("data/sql/base/db_world/item_template.sql")
ITEM_ENCHANT_SQL = Path("data/sql/base/db_world/item_enchantment_template.sql")

# item_template column indices (see ObjectMgr::LoadItemTemplates' SELECT).
COL_ENTRY, COL_NAME, COL_QUALITY = 0, 4, 6
COL_INVTYPE, COL_REQLEVEL, COL_RANDOMSUFFIX = 12, 16, 110
ITEM_TEMPLATE_MIN_FIELDS = 130   # real rows have 137; guards against a false
                                 # "(entry," match inside a text column

# Vendor creature invariants.
VENDOR_FACTION = 35              # friendly to all -- team is a script rule
VENDOR_NPCFLAG = 1               # UNIT_NPC_FLAG_GOSSIP
VENDOR_UNIT_FLAGS = 0x2 | 0x100 | 0x200   # NON_ATTACKABLE | IMMUNE_TO_PC | IMMUNE_TO_NPC
VENDOR_SCRIPT = "npc_moba_store"
DEFAULT_SUBNAME = "MOBA Vendor"

QUALITY_GREEN = 2
EXPECTED_REQ_LEVEL = range(77, 81)   # advisory only

GOSSIP_MAX_MENU_ITEMS = 32       # GossipDef.h; a fuller menu is silently truncated

# Custom copy entry = source entry + this offset (36063 -> 936063). Deterministic,
# so re-runs are stable without a lockfile and the mapping stays readable.
ITEM_ENTRY_OFFSET = 900000
BIND_WHEN_PICKED_UP = 1          # ItemBondingType; binds in _StoreItem, no client prompt


def fail(msg):
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def warn(msg):
    print(f"WARNING: {msg}", file=sys.stderr)


def sql_str(s):
    return "'" + str(s).replace("'", "''") + "'"


def parse_tuple_at(data, start):
    """Split the SQL VALUES tuple beginning at data[start] == '(' into raw
    fields. Returns None if it is not a well-formed tuple."""
    if data[start] != "(":
        return None
    fields, cur, in_quote = [], [], False
    i = start + 1
    n = len(data)
    while i < n:
        c = data[i]
        if in_quote:
            if c == "\\":
                cur.append(data[i:i + 2])
                i += 2
                continue
            if c == "'":
                in_quote = False
            cur.append(c)
            i += 1
            continue
        if c == "'":
            in_quote = True
            cur.append(c)
            i += 1
            continue
        if c == ",":
            fields.append("".join(cur))
            cur = []
            i += 1
            continue
        if c == ")":
            fields.append("".join(cur))
            return fields
        if c == "\n":       # a tuple never spans rows in these dumps
            return None
        cur.append(c)
        i += 1
    return None


def unquote(s):
    s = s.strip()
    if s.startswith("'") and s.endswith("'"):
        s = s[1:-1]
    return s.replace("\\'", "'").replace("\\\\", "\\")


def load_items(entries):
    """entry -> {name, quality, req, suffix_group} for the given item entries."""
    if not ITEM_TEMPLATE_SQL.exists():
        fail(f"{ITEM_TEMPLATE_SQL} not found -- run from the repo root")
    data = ITEM_TEMPLATE_SQL.read_text(encoding="utf-8", errors="replace")
    pattern = re.compile(r"\((" + "|".join(str(e) for e in sorted(entries)) + r"),")
    found = {}
    for m in pattern.finditer(data):
        entry = int(m.group(1))
        if entry in found:
            continue
        fields = parse_tuple_at(data, m.start())
        if fields is None or len(fields) < ITEM_TEMPLATE_MIN_FIELDS:
            continue
        try:
            if int(fields[COL_ENTRY]) != entry:
                continue
            found[entry] = {
                "name": unquote(fields[COL_NAME]),
                "quality": int(fields[COL_QUALITY]),
                "req": int(fields[COL_REQLEVEL]),
                "suffix_group": int(fields[COL_RANDOMSUFFIX]),
            }
        except ValueError:
            continue
    missing = sorted(set(entries) - set(found))
    if missing:
        fail(f"item entries not found in item_template: {missing}")
    return found


def load_suffix_groups():
    """suffix group id -> set of ItemRandomSuffix ids it can roll."""
    if not ITEM_ENCHANT_SQL.exists():
        fail(f"{ITEM_ENCHANT_SQL} not found -- run from the repo root")
    text = ITEM_ENCHANT_SQL.read_text(encoding="utf-8", errors="replace")
    groups = defaultdict(set)
    for entry, ench, _chance in re.findall(r"\((\d+),\s*(\d+),\s*([\d.]+)\)", text):
        groups[int(entry)].add(int(ench))
    if not groups:
        fail(f"parsed no rows from {ITEM_ENCHANT_SQL}")
    return groups


def validate(cfg, path):
    if not isinstance(cfg.get("map"), int):
        fail(f'{path}: "map" must be an integer map id')
    if not isinstance(cfg.get("suffixes"), list) or not cfg["suffixes"]:
        fail(f'{path}: "suffixes" must be a non-empty list of {{name, id}}')
    for s in cfg["suffixes"]:
        if "name" not in s or "id" not in s:
            fail(f'{path}: every "suffixes" entry needs "name" and "id"')
    if not isinstance(cfg.get("vendors"), list) or not cfg["vendors"]:
        fail(f'{path}: "vendors" must be a non-empty list')

    # A menu group is either a branch ("subcategories") or a leaf-bearing group
    # ("pieces"), never both -- that is what allows arbitrary nesting depth.
    def validate_group(group, vendor_key, trail):
        if "name" not in group:
            fail(f'{path}: vendor {vendor_key} group under "{trail}" needs a "name"')
        here = f'{trail} / {group["name"]}'
        has_subs = bool(group.get("subcategories"))
        has_pieces = bool(group.get("pieces"))
        if has_subs == has_pieces:
            fail(f'{path}: vendor {vendor_key} group "{here}" needs exactly one of '
                 f'"subcategories" or "pieces"')
        for sub in group.get("subcategories", []):
            validate_group(sub, vendor_key, here)

    for v in cfg["vendors"]:
        for key in ("key", "name", "display_id", "teams", "categories"):
            if key not in v:
                fail(f'{path}: vendor {v.get("key", "?")} missing "{key}"')
        for t in v["teams"]:
            for key in ("team", "entry", "x", "y", "z", "o"):
                if key not in t:
                    fail(f'{path}: vendor {v["key"]} team block missing "{key}"')
            if t["team"] not in (0, 1):
                fail(f'{path}: vendor {v["key"]} "team" must be 0 or 1')
        for c in v["categories"]:
            validate_group(c, v["key"], v["key"])


def load_configs():
    configs = []
    seen_entries = {}
    for path in sorted(MAPS_DIR.glob("*/store_config.yaml")):
        cfg = yaml.safe_load(path.read_text())
        validate(cfg, path)
        for v in cfg["vendors"]:
            for t in v["teams"]:
                if t["entry"] in seen_entries:
                    fail(f'{path}: vendor entry {t["entry"]} already defined in '
                         f'{seen_entries[t["entry"]]} -- entries must be globally unique')
                seen_entries[t["entry"]] = path
        configs.append((path, cfg))
    if not configs:
        fail(f"no vendor configs found under {MAPS_DIR}/*/store_config.yaml")
    return configs


def build(configs):
    """Resolve every config into flat SQL row tuples."""

    def collect(group, into):
        into.update(group.get("pieces", []))
        for sub in group.get("subcategories", []):
            collect(sub, into)

    wanted = set()
    for _, cfg in configs:
        for v in cfg["vendors"]:
            for c in v["categories"]:
                collect(c, wanted)
        if cfg.get("cloak_item"):
            wanted.add(cfg["cloak_item"])

    items = load_items(wanted)
    groups = load_suffix_groups()

    def rolls(entry):
        """The suffix ids this base item can actually roll."""
        return groups.get(items[entry]["suffix_group"], set())

    for entry, info in sorted(items.items()):
        if info["quality"] != QUALITY_GREEN:
            warn(f'item {entry} "{info["name"]}" is quality {info["quality"]}, '
                 f"not green -- random suffixes only roll on greens")
        if info["req"] not in EXPECTED_REQ_LEVEL:
            warn(f'item {entry} "{info["name"]}" requires level {info["req"]}, '
                 f"outside {EXPECTED_REQ_LEVEL.start}-{EXPECTED_REQ_LEVEL.stop - 1}")

    npc_rows, menu_rows, grant_rows, vendors_meta, item_copies = [], [], [], [], []

    for path, cfg in configs:
        map_id = cfg["map"]
        suffixes = cfg["suffixes"]
        suffix_ids = {s["id"] for s in suffixes}
        cloak = cfg.get("cloak_item")
        map_sell_ratio = cfg.get("sell_ratio", 0)

        # Stock entries render in an unpatched client; our own copies do not.
        custom_items = bool(cfg.get("custom_items", False))
        entry_offset = ITEM_ENTRY_OFFSET if custom_items else 0

        cfg_items = set()
        for v in cfg["vendors"]:
            for c in v["categories"]:
                collect(c, cfg_items)
        if cloak:
            cfg_items.add(cloak)

        item_sell = {}          # source entry -> sell price in copper

        for vendor_id, v in enumerate(cfg["vendors"]):
            for t in v["teams"]:
                npc_rows.append((t["entry"], map_id, t["team"], vendor_id, v, t))

            next_node = 0
            purchases = 0

            def add_group(group, parent_id, sort_order, trail):
                """Emit `group`'s menu node, then recurse into its subcategories
                or hang one purchase node per available suffix off it."""
                nonlocal next_node, purchases

                next_node += 1
                node = next_node
                here = f'{trail} / {group["name"]}'
                menu_rows.append((map_id, vendor_id, node, parent_id, sort_order,
                                  group["name"], 0, 0))

                subs = group.get("subcategories")
                if subs:
                    if len(subs) > GOSSIP_MAX_MENU_ITEMS:
                        fail(f'{path}: vendor {v["key"]} group "{here}" has {len(subs)} '
                             f"subcategories -- gossip allows at most {GOSSIP_MAX_MENU_ITEMS}")
                    for i, sub in enumerate(subs):
                        add_group(sub, node, i, here)
                    return

                pieces = group["pieces"]
                cost = group.get("cost", 0)
                sell = int(round(cost * group.get("sell_ratio", map_sell_ratio)))

                if sell and not custom_items:
                    warn(f'{path}: group "{here}" resolves to sell price {sell}, but '
                         f"custom_items is off -- stock items keep their own SellPrice")

                # A piece that rolls none of the configured suffixes is a config
                # error (wrong entry, or an id that item cannot roll).
                for p in pieces:
                    if not rolls(p) & suffix_ids:
                        fail(f'{path}: vendor {v["key"]} group "{here}": item {p} '
                             f'"{items[p]["name"]}" rolls none of the configured '
                             f'suffixes (its group is {items[p]["suffix_group"]})')

                sub_sort = 0
                for s in suffixes:
                    bundle = [p for p in pieces if s["id"] in rolls(p)]
                    if not bundle:
                        continue          # e.g. cloth or wand under a physical suffix
                    if group.get("append_cloak") and cloak and s["id"] in rolls(cloak):
                        bundle.append(cloak)

                    next_node += 1
                    leaf = next_node
                    menu_rows.append((map_id, vendor_id, leaf, node, sub_sort,
                                      s["name"], 1, cost))

                    for item_entry in bundle:
                        # SellPrice is a column on the copied row, so one item
                        # cannot carry two prices -- the cloak is shared by every
                        # armour category, so those must agree.
                        prior = item_sell.get(item_entry)
                        if prior is not None and prior != sell:
                            fail(f'{path}: item {item_entry} "{items[item_entry]["name"]}" '
                                 f"resolves to sell price {sell} under \"{here}\" but "
                                 f"{prior} elsewhere -- give the groups the same "
                                 f"cost/sell_ratio, or the item its own entry")
                        item_sell[item_entry] = sell

                        grant_rows.append((map_id, vendor_id, leaf,
                                           item_entry + entry_offset, s["id"], 1))

                    sub_sort += 1
                    purchases += 1

                if sub_sort == 0:
                    fail(f'{path}: vendor {v["key"]} group "{here}" produced no '
                         f"purchasable suffixes")
                if sub_sort > GOSSIP_MAX_MENU_ITEMS:
                    fail(f'{path}: vendor {v["key"]} group "{here}" has {sub_sort} '
                         f"options -- gossip allows at most {GOSSIP_MAX_MENU_ITEMS}")

            if len(v["categories"]) > GOSSIP_MAX_MENU_ITEMS:
                fail(f'{path}: vendor {v["key"]} has {len(v["categories"])} categories '
                     f"-- gossip allows at most {GOSSIP_MAX_MENU_ITEMS}")

            for cat_sort, c in enumerate(v["categories"]):
                add_group(c, 0, cat_sort, v["key"])

            vendors_meta.append((path, map_id, vendor_id, v, purchases))

        if custom_items:
            item_copies.extend((entry, entry + ITEM_ENTRY_OFFSET, item_sell.get(entry, 0))
                               for entry in sorted(cfg_items))

    return npc_rows, menu_rows, grant_rows, vendors_meta, item_copies


def emit_item_copies(item_copies):
    """SQL cloning each source item_template row under a custom entry.

    Deliberately schema-agnostic: rows are copied with SELECT * through a staging
    table rather than a hand-listed column set. item_template has already lost a
    column upstream (StatsCount, dropped by 2025_08_01_00.sql) and creature.id1
    was renamed the same way -- enumerating columns is exactly what breaks.
    """
    sources_csv = ", ".join(str(src) for src, _cust, _sell in item_copies)
    customs_csv = ", ".join(str(cust) for _src, cust, _sell in item_copies)

    lines = [
        "",
        "-- ============================================================",
        "-- Custom item copies. Shop gear is handed out under our own entries so",
        "-- it can never collide with a world drop (the battleground strips every",
        "-- catalog entry on exit) and so we own SellPrice and Bonding.",
        "-- Overridden: entry, BuyPrice, SellPrice, bonding (BIND_WHEN_PICKED_UP --",
        "-- binds on pickup, so no client prompt and no trading), duration.",
        "-- Everything else -- stats, armour, damage, RandomSuffix -- is copied",
        "-- verbatim, so 'of the X' rolls exactly as it does on the source item.",
        "-- ============================================================",
        "DROP TABLE IF EXISTS `mod_moba_store_itemstage`;",
        "CREATE TABLE `mod_moba_store_itemstage` AS",
        f"    SELECT * FROM `item_template` WHERE `entry` IN ({sources_csv});",
        "",
        "UPDATE `mod_moba_store_itemstage` SET",
        f"    `entry` = `entry` + {ITEM_ENTRY_OFFSET},",
        "    `BuyPrice` = 0,",
        "    `SellPrice` = 0,",
        f"    `bonding` = {BIND_WHEN_PICKED_UP},",
        "    `duration` = 0;",
    ]

    priced = [(cust, sell) for _src, cust, sell in item_copies if sell]
    if priced:
        lines.append("")
        for cust, sell in priced:
            lines.append(f"UPDATE `mod_moba_store_itemstage` SET `SellPrice` = {sell} "
                         f"WHERE `entry` = {cust};")

    lines += [
        "",
        f"DELETE FROM `item_template` WHERE `entry` IN ({customs_csv});",
        "INSERT INTO `item_template` SELECT * FROM `mod_moba_store_itemstage`;",
        "DROP TABLE `mod_moba_store_itemstage`;",
        "",
        "-- item_dbc is the DB-side extension of Item.dbc. Without a row here,",
        "-- ObjectMgr::LoadItemTemplates skips this entry's DBC cross-checks.",
        "-- Reads back the rows inserted above, so it must follow them.",
        f"DELETE FROM `item_dbc` WHERE `ID` IN ({customs_csv});",
        "INSERT INTO `item_dbc`",
        "    (`ID`, `ClassID`, `SubclassID`, `Sound_Override_Subclassid`, `Material`,",
        "     `DisplayInfoID`, `InventoryType`, `SheatheType`)",
        "    SELECT `entry`, `class`, `subclass`, `SoundOverrideSubclass`, `Material`,",
        "           `displayid`, `InventoryType`, `sheath`",
        f"    FROM `item_template` WHERE `entry` IN ({customs_csv});",
    ]
    return lines


def emit(npc_rows, menu_rows, grant_rows, item_copies):
    entries_csv = ", ".join(str(r[0]) for r in npc_rows)

    lines = [
        "-- ============================================================",
        "-- GENERATED FILE -- do not hand-edit.",
        "-- Produced by apps/moba/gen_store.py from apps/moba/maps/*/store_config.yaml.",
        "-- Owns the vendor creatures (creature_template + creature_template_model),",
        "-- their spawns (creature), and the gossip catalog (mod_moba_store_*).",
        "-- Team is carried in mod_moba_store_npc, NOT in faction: CFBG puts players",
        "-- of either faction on either BG team, so faction cannot express team.",
        "-- Vendors are faction 35 (friendly to all) and immune; npc_moba_store",
        "-- refuses players whose BG team does not match.",
        "-- ============================================================",
        "",
        "USE acore_world;",
        "",
        f"DELETE FROM `creature_template` WHERE `entry` IN ({entries_csv});",
        "INSERT INTO `creature_template`",
        "(`entry`, `name`, `subname`, `minlevel`, `maxlevel`, `faction`, `npcflag`,",
        " `speed_walk`, `speed_run`, `rank`, `unit_class`, `unit_flags`,",
        " `type`, `type_flags`, `MovementType`, `HealthModifier`, `ArmorModifier`,",
        " `RegenHealth`, `flags_extra`, `ScriptName`, `VerifiedBuild`)",
        "VALUES",
    ]
    ct_rows = []
    for entry, _map_id, _team, _vid, v, _t in npc_rows:
        subname = v.get("subname", DEFAULT_SUBNAME)
        ct_rows.append(
            f"({entry}, {sql_str(v['name'])}, {sql_str(subname)}, 80, 80, "
            f"{VENDOR_FACTION}, {VENDOR_NPCFLAG}, "
            f"1.0, 1.14286, 0, 1, {VENDOR_UNIT_FLAGS}, "
            f"7, 0, 0, 1, 1, "
            f"1, 0, {sql_str(VENDOR_SCRIPT)}, 0)")
    lines.append(",\n".join(ct_rows) + ";")

    lines += [
        "",
        f"DELETE FROM `creature_template_model` WHERE `CreatureID` IN ({entries_csv});",
        "INSERT INTO `creature_template_model`",
        "(`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)",
        "VALUES",
    ]
    ctm_rows = [f"({entry}, 0, {v['display_id']}, {v.get('display_scale', 1.0)}, 1, 0)"
                for entry, _m, _team, _vid, v, _t in npc_rows]
    lines.append(",\n".join(ctm_rows) + ";")

    # guid == entry: the 900000-900999 guid window is unused by core data.
    # The spawn entry column is `id`, not `id1`: upstream 2026_06_16_00.sql
    # renamed it and moved id2/id3 into creature_multispawn. base/db_world/
    # creature.sql still shows the pre-rename schema -- it is historical, and
    # the live schema is base + updates.
    lines += [
        "",
        f"DELETE FROM `creature` WHERE `guid` IN ({entries_csv});",
        "INSERT INTO `creature`",
        "(`guid`, `id`, `map`, `spawnMask`, `phaseMask`, `equipment_id`,",
        " `position_x`, `position_y`, `position_z`, `orientation`,",
        " `spawntimesecs`, `wander_distance`, `MovementType`)",
        "VALUES",
    ]
    c_rows = [f"({entry}, {entry}, {map_id}, 1, 1, 0, "
              f"{t['x']}, {t['y']}, {t['z']}, {t['o']}, 300, 0, 0)"
              for entry, map_id, _team, _vid, _v, t in npc_rows]
    lines.append(",\n".join(c_rows) + ";")

    lines += [
        "",
        "DROP TABLE IF EXISTS `mod_moba_store_npc`;",
        "CREATE TABLE `mod_moba_store_npc` (",
        "    `CreatureEntry` INT UNSIGNED NOT NULL PRIMARY KEY,",
        "    `Map`           INT UNSIGNED NOT NULL,",
        "    `Team`          TINYINT UNSIGNED NOT NULL,  -- 0 = Alliance, 1 = Horde",
        "    `VendorId`      INT UNSIGNED NOT NULL       -- both teams' entries share one",
        ");",
        "",
        "INSERT INTO `mod_moba_store_npc` (`CreatureEntry`, `Map`, `Team`, `VendorId`)",
        "VALUES",
    ]
    lines.append(",\n".join(
        f"({entry}, {map_id}, {team}, {vid})"
        for entry, map_id, team, vid, _v, _t in npc_rows) + ";")

    lines += [
        "",
        "DROP TABLE IF EXISTS `mod_moba_store_menu`;",
        "CREATE TABLE `mod_moba_store_menu` (",
        "    `Map`        INT UNSIGNED NOT NULL,",
        "    `VendorId`   INT UNSIGNED NOT NULL,",
        "    `NodeId`     INT UNSIGNED NOT NULL,",
        "    `ParentId`   INT UNSIGNED NOT NULL DEFAULT 0,  -- 0 = top level",
        "    `SortOrder`  INT UNSIGNED NOT NULL DEFAULT 0,",
        "    `Label`      VARCHAR(100) NOT NULL,",
        "    `IsPurchase` TINYINT UNSIGNED NOT NULL DEFAULT 0,",
        "    `CostCopper` INT UNSIGNED NOT NULL DEFAULT 0,",
        "    PRIMARY KEY (`Map`, `VendorId`, `NodeId`)",
        ");",
        "",
        "INSERT INTO `mod_moba_store_menu`",
        "(`Map`, `VendorId`, `NodeId`, `ParentId`, `SortOrder`, `Label`, `IsPurchase`, `CostCopper`)",
        "VALUES",
    ]
    lines.append(",\n".join(
        f"({m}, {vid}, {nid}, {pid}, {sort}, {sql_str(label)}, {is_buy}, {cost})"
        for m, vid, nid, pid, sort, label, is_buy, cost in menu_rows) + ";")

    lines += [
        "",
        "DROP TABLE IF EXISTS `mod_moba_store_grant`;",
        "CREATE TABLE `mod_moba_store_grant` (",
        "    `Map`       INT UNSIGNED NOT NULL,",
        "    `VendorId`  INT UNSIGNED NOT NULL,",
        "    `NodeId`    INT UNSIGNED NOT NULL,",
        "    `ItemEntry` INT UNSIGNED NOT NULL,",
        "    -- ItemRandomSuffix.dbc id, stored positive; 0 = no suffix. The engine",
        "    -- wants it NEGATED as randomPropertyId (see Item::GenerateItemRandomPropertyId).",
        "    `SuffixId`  INT UNSIGNED NOT NULL DEFAULT 0,",
        "    `Count`     INT UNSIGNED NOT NULL DEFAULT 1,",
        "    KEY `idx_node` (`Map`, `VendorId`, `NodeId`)",
        ");",
        "",
        "INSERT INTO `mod_moba_store_grant`",
        "(`Map`, `VendorId`, `NodeId`, `ItemEntry`, `SuffixId`, `Count`)",
        "VALUES",
    ]
    lines.append(",\n".join(
        f"({m}, {vid}, {nid}, {item}, {suffix}, {count})"
        for m, vid, nid, item, suffix, count in grant_rows) + ";")

    if item_copies:
        lines += emit_item_copies(item_copies)

    return "\n".join(lines) + "\n"


def main():
    configs = load_configs()
    npc_rows, menu_rows, grant_rows, vendors_meta, item_copies = build(configs)
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(emit(npc_rows, menu_rows, grant_rows, item_copies))

    print(f"Wrote {OUTPUT}")
    for path, map_id, vendor_id, v, purchases in vendors_meta:
        print(f"  map {map_id} vendor {vendor_id} ({v['key']}): "
              f"{len(v['categories'])} categories, {purchases} purchase nodes")
    print(f"  {len(npc_rows)} vendor NPCs, {len(menu_rows)} menu nodes, "
          f"{len(grant_rows)} grant rows")
    if item_copies:
        print(f"  {len(item_copies)} custom item copies (source entry + {ITEM_ENTRY_OFFSET})")
    else:
        print("  custom_items off -- grants use stock item entries")
    print("Restart worldserver -- mod_moba_store.sql auto-applies on boot, and "
          "the data store caches once per process.")


if __name__ == "__main__":
    main()
