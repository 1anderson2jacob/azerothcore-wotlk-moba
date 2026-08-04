#!/usr/bin/env python3
"""
MOBA item shop generator.

Reads per-map shop configs (apps/moba/maps/<mode>/store_config.yaml) and
generates data/sql/custom/db_world/mod_moba_store.sql in full:
  * creature_template / creature_template_model -- the shopkeeper NPCs (one entry
                            per team; team lives in mod_moba_store_npc, not in
                            faction -- CFBG means faction cannot express team)
  * creature              -- their per-map spawns (guid == entry; static props,
                            so they are world spawns rather than BG-lifecycle
                            AddCreature calls, which would need new BgCreatures
                            enum slots)
  * mod_moba_store_npc    -- creature entry -> (map, team)
  * mod_moba_store_menu   -- the catalog tree, arbitrary depth, keyed per tab
  * mod_moba_store_grant  -- the item(s) behind each purchase node
  * mod_moba_store_sell   -- per-unit sell-back price for every item sold here
plus client/addons/MobaHUD/Catalog.lua, which is what the shop panel browses.

ONE shopkeeper per team serves every tab, so TabId keys the menu and grant tables
but NOT the NPC: the tab bought from is picked in the panel, never inferred from
which NPC you are standing at.

A menu group is exactly one of three shapes:
  * subcategories -- a branch, nested to any depth
  * pieces        -- random-suffix bases; ONE LEAF PER SUFFIX, granting the whole
                     bundle under that suffix
  * items         -- fixed named items; ONE LEAF PER ITEM, no suffix (the blue,
                     purple and consumable tabs)

Starting-gear "of the X" pieces are stock green random-suffix bases: the shop
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

import id_alloc

MAPS_DIR = Path(__file__).parent / "maps"
OUTPUT = Path("data/sql/custom/db_world/mod_moba_store.sql")
CATALOG_OUTPUT = Path("client/addons/MobaHUD/Catalog.lua")
ITEM_TEMPLATE_SQL = Path("data/sql/base/db_world/item_template.sql")
ITEM_ENCHANT_SQL = Path("data/sql/base/db_world/item_enchantment_template.sql")

# item_template column indices (see ObjectMgr::LoadItemTemplates' SELECT).
COL_ENTRY, COL_NAME, COL_QUALITY = 0, 4, 6
COL_CLASS, COL_SUBCLASS = 1, 2
COL_INVTYPE, COL_ITEMLEVEL, COL_REQLEVEL, COL_RANDOMSUFFIX = 12, 15, 16, 110
COL_FLAGS2 = 8                   # item_template.FlagsExtra
ITEM_TEMPLATE_MIN_FIELDS = 130   # real rows have 137; guards against a false
                                 # "(entry," match inside a text column

# Shopkeeper creature invariants.
SHOPKEEPER_FACTION = 35              # friendly to all -- team is a script rule
SHOPKEEPER_NPCFLAG = 1               # UNIT_NPC_FLAG_GOSSIP: what makes it right-clickable
SHOPKEEPER_UNIT_FLAGS = 0x2 | 0x100 | 0x200   # NON_ATTACKABLE | IMMUNE_TO_PC | IMMUNE_TO_NPC
SHOPKEEPER_SCRIPT = "npc_moba_store"

# The shop is the only generator that owns `creature` SPAWN rows rather than
# templates alone, so an orphan here is a live scripted NPC standing in the base,
# not an inert row nothing references. That is what the four-vendors-to-one
# collapse had to sweep. Its blocks are declared in apps/moba/id_blocks.json --
# in TWO namespaces, since each shopkeeper spawn takes guid == entry. See
# id_alloc.sql_window for why clearing by block beats clearing by roster.

QUALITY_GREEN = 2
EXPECTED_REQ_LEVEL = range(77, 81)   # advisory only

MAX_USABLE_REQ_LEVEL = 80            # a fixed item above this can never be equipped

# ItemFlags2. Player::CanUseItem tests these against the player's NATIVE race,
# not their BG team -- so with CFBG a faction-locked item differs between two
# players on the SAME side. Both teams must see the same shop.
ITEM_FLAG2_FACTION_HORDE    = 0x1
ITEM_FLAG2_FACTION_ALLIANCE = 0x2

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


def lua_str(s):
    """Lua string literal. Item names carry apostrophes freely; escaping quotes
    and backslashes too is belt-and-braces against a future rename."""
    return '"' + str(s).replace("\\", "\\\\").replace('"', '\\"') + '"'


def stock_entry(entry):
    """The item_template entry a grant was cloned from.

    Grants carry the custom copy when custom_items is on, and those entries have
    no item_template.sql row. Every metadata lookup goes through here so the
    +900000 relationship lives in exactly one place -- see the plan's decision on
    the stock/copy mapping.
    """
    return entry - ITEM_ENTRY_OFFSET if entry >= ITEM_ENTRY_OFFSET else entry


def item_spec(spec):
    """One `items:` leaf -- a bare entry id, or a table overriding cost/count/name."""
    return {"entry": spec} if isinstance(spec, int) else dict(spec)


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
    """entry -> {name, quality, req, suffix_group, class, subclass, invtype, ilvl}."""
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
                "class": int(fields[COL_CLASS]),
                "subclass": int(fields[COL_SUBCLASS]),
                "invtype": int(fields[COL_INVTYPE]),
                "ilvl": int(fields[COL_ITEMLEVEL]),
                "flags2": int(fields[COL_FLAGS2]),
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

    sk = cfg.get("shopkeeper")
    if not isinstance(sk, dict):
        fail(f'{path}: "shopkeeper" must be a block with name/subname/display_id/teams')
    for key in ("name", "subname", "display_id", "teams"):
        if key not in sk:
            fail(f'{path}: "shopkeeper" missing "{key}"')
    for t in sk["teams"]:
        for key in ("team", "entry", "x", "y", "z", "o"):
            if key not in t:
                fail(f'{path}: shopkeeper team block missing "{key}"')
        if t["team"] not in (0, 1):
            fail(f'{path}: shopkeeper "team" must be 0 or 1')

    if not isinstance(cfg.get("tabs"), list) or not cfg["tabs"]:
        fail(f'{path}: "tabs" must be a non-empty list')

    # A menu group is a branch ("subcategories") or leaf-bearing ("pieces" for
    # suffix bundles, "items" for fixed items), never more than one -- that is
    # what allows arbitrary nesting depth.
    def validate_group(group, tab_key, trail):
        if "name" not in group:
            fail(f'{path}: tab {tab_key} group under "{trail}" needs a "name"')
        here = f'{trail} / {group["name"]}'
        modes = [k for k in ("subcategories", "pieces", "items") if group.get(k)]
        if len(modes) != 1:
            fail(f'{path}: tab {tab_key} group "{here}" needs exactly one of '
                 f'"subcategories", "pieces" or "items"')
        for spec in group.get("items", []):
            if not isinstance(spec, int) and "entry" not in spec:
                fail(f'{path}: tab {tab_key} group "{here}": every "items" entry is '
                     f'an item id or a table with an "entry"')
        for sub in group.get("subcategories", []):
            validate_group(sub, tab_key, here)

    for v in cfg["tabs"]:
        for key in ("key", "name", "categories"):
            if key not in v:
                fail(f'{path}: tab {v.get("key", "?")} missing "{key}"')
        for c in v["categories"]:
            validate_group(c, v["key"], v["key"])


def load_configs():
    configs = []
    seen_entries = {}
    for path in sorted(MAPS_DIR.glob("*/store_config.yaml")):
        cfg = yaml.safe_load(path.read_text())
        validate(cfg, path)
        for t in cfg["shopkeeper"]["teams"]:
            if t["entry"] in seen_entries:
                fail(f'{path}: shopkeeper entry {t["entry"]} already defined in '
                     f'{seen_entries[t["entry"]]} -- entries must be globally unique')
            seen_entries[t["entry"]] = path
        configs.append((path, cfg))
    if not configs:
        fail(f"no shop configs found under {MAPS_DIR}/*/store_config.yaml")
    # Two namespaces out of one config field: the creature_template entry, and
    # the `creature` spawn guid, which this generator sets equal to it. Reuse the
    # Registry so the second check does not re-read every config.
    reg = id_alloc.validate_owner(id_alloc.CREATURE_TEMPLATE, "store")
    id_alloc.validate_owner(id_alloc.CREATURE_SPAWN, "store", reg)
    return configs


def build(configs):
    """Resolve every config into flat SQL row tuples."""

    def collect(group, bases, fixed):
        bases.update(group.get("pieces", []))
        fixed.update(item_spec(s)["entry"] for s in group.get("items", []))
        for sub in group.get("subcategories", []):
            collect(sub, bases, fixed)

    suffix_bases, fixed_items = set(), set()
    for _, cfg in configs:
        for v in cfg["tabs"]:
            for c in v["categories"]:
                collect(c, suffix_bases, fixed_items)
        if cfg.get("cloak_item"):
            suffix_bases.add(cfg["cloak_item"])

    items = load_items(suffix_bases | fixed_items)
    groups = load_suffix_groups()

    def rolls(entry):
        """The suffix ids this base item can actually roll."""
        return groups.get(items[entry]["suffix_group"], set())

    # Only suffix bases must be green: a random suffix cannot roll on a blue or a
    # purple, which is exactly what the fixed-item tabs sell.
    for entry in sorted(suffix_bases):
        info = items[entry]
        if info["quality"] != QUALITY_GREEN:
            warn(f'item {entry} "{info["name"]}" is quality {info["quality"]}, '
                 f"not green -- random suffixes only roll on greens")
        if info["req"] not in EXPECTED_REQ_LEVEL:
            warn(f'item {entry} "{info["name"]}" requires level {info["req"]}, '
                 f"outside {EXPECTED_REQ_LEVEL.start}-{EXPECTED_REQ_LEVEL.stop - 1}")

    for entry in sorted(fixed_items):
        info = items[entry]
        if info["req"] > MAX_USABLE_REQ_LEVEL:
            fail(f'item {entry} "{info["name"]}" requires level {info["req"]} -- '
                 f"unusable by a level-{MAX_USABLE_REQ_LEVEL} player")

    # Every catalog item, suffix bases included -- a faction-locked base greys out
    # for half the players exactly the same way a fixed item does.
    locked = []
    for entry in sorted(suffix_bases | fixed_items):
        info = items[entry]
        if info["flags2"] & (ITEM_FLAG2_FACTION_HORDE | ITEM_FLAG2_FACTION_ALLIANCE):
            side = "Horde" if info["flags2"] & ITEM_FLAG2_FACTION_HORDE else "Alliance"
            locked.append(f'  {entry} "{info["name"]}" is {side}-only')
    if locked:
        fail("faction-locked items in the catalog -- both teams must see the same "
             "shop:\n" + "\n".join(locked))

    npc_rows, menu_rows, grant_rows, sell_rows, tabs_meta, item_copies = [], [], [], [], [], []

    for path, cfg in configs:
        map_id = cfg["map"]
        suffixes = cfg["suffixes"]
        suffix_ids = {s["id"] for s in suffixes}
        cloak = cfg.get("cloak_item")
        map_sell_ratio = cfg.get("sell_ratio", 0)

        # Stock entries render in an unpatched client; our own copies do not.
        custom_items = bool(cfg.get("custom_items", False))
        entry_offset = ITEM_ENTRY_OFFSET if custom_items else 0

        cfg_bases, cfg_fixed = set(), set()
        for v in cfg["tabs"]:
            for c in v["categories"]:
                collect(c, cfg_bases, cfg_fixed)
        if cloak:
            cfg_bases.add(cloak)
        cfg_items = cfg_bases | cfg_fixed

        item_sell = {}          # source entry -> sell price in copper

        def note_sell(item_entry, sell, here):
            # SellPrice is a column on the copied row, so one item cannot carry two
            # prices -- the cloak is shared by every armour category, so those groups
            # must agree.
            prior = item_sell.get(item_entry)
            if prior is not None and prior != sell:
                fail(f'{path}: item {item_entry} "{items[item_entry]["name"]}" resolves '
                     f'to sell price {sell} under "{here}" but {prior} elsewhere -- give '
                     f"the groups the same cost/sell_ratio, or the item its own entry")
            item_sell[item_entry] = sell

        # One shopkeeper pair per map, independent of the tab count.
        sk = cfg["shopkeeper"]
        for t in sk["teams"]:
            npc_rows.append((t["entry"], map_id, t["team"], sk, t))

        # TabId keys mod_moba_store_menu and _grant and rides the BUY protocol.
        # It is an index into `tabs`, never a creature -- see the module docstring.
        for tab_id, v in enumerate(cfg["tabs"]):
            next_node = 0
            purchases = 0

            def add_group(group, parent_id, sort_order, trail):
                """Emit `group`'s menu node, then recurse into its subcategories,
                hang one purchase node per fixed item, or hang one per suffix the
                whole bundle can roll."""
                nonlocal next_node, purchases

                next_node += 1
                node = next_node
                here = f'{trail} / {group["name"]}'
                menu_rows.append((map_id, tab_id, node, parent_id, sort_order,
                                  group["name"], 0, 0))

                subs = group.get("subcategories")
                if subs:
                    for i, sub in enumerate(subs):
                        add_group(sub, node, i, here)
                    return

                group_ratio = group.get("sell_ratio", map_sell_ratio)

                # Fixed-item group: one leaf per item, no suffix. Blues, purples and
                # consumables are named items, not random-suffix bases.
                fixed = group.get("items")
                if fixed:
                    for i, spec in enumerate(item_spec(s) for s in fixed):
                        entry = spec["entry"]
                        count = spec.get("count", 1)
                        cost  = spec.get("cost", group.get("cost", 0))
                        label = spec.get("name", items[entry]["name"])
                        if count > 1:
                            label += f" x{count}"

                        # PER UNIT, like item_template.SellPrice: one leaf may grant
                        # a stack, and the sell handler multiplies by the stack size
                        # it destroys. 5 potions for 5000 at ratio 0.25 -> 250 each.
                        sell = int(round(cost * spec.get("sell_ratio", group_ratio) / count))

                        next_node += 1
                        leaf = next_node
                        menu_rows.append((map_id, tab_id, leaf, node, i, label, 1, cost))
                        note_sell(entry, sell, here)
                        grant_rows.append((map_id, tab_id, leaf,
                                           entry + entry_offset, 0, count))
                        purchases += 1
                    return

                pieces = group["pieces"]
                cost = group.get("cost", 0)

                # A piece that rolls none of the configured suffixes is a config
                # error (wrong entry, or an id that item cannot roll).
                for p in pieces:
                    if not rolls(p) & suffix_ids:
                        fail(f'{path}: tab {v["key"]} group "{here}": item {p} '
                             f'"{items[p]["name"]}" rolls none of the configured '
                             f'suffixes (its group is {items[p]["suffix_group"]})')

                sub_sort = 0
                for s in suffixes:
                    bundle = [p for p in pieces if s["id"] in rolls(p)]
                    if not bundle:
                        continue          # e.g. cloth or wand under a physical suffix
                    if group.get("append_cloak") and cloak and s["id"] in rolls(cloak):
                        bundle.append(cloak)

                    # One node price, many items: split it across what the leaf
                    # actually grants. len(bundle), NOT len(pieces) -- the appended
                    # cloak is a piece you paid for. Should a future cloak fail to
                    # roll some suffix, sibling leaves split differently and
                    # note_sell fails the run rather than minting the difference.
                    sell = int(round(cost * group_ratio / len(bundle)))

                    next_node += 1
                    leaf = next_node
                    menu_rows.append((map_id, tab_id, leaf, node, sub_sort,
                                      s["name"], 1, cost))

                    for item_entry in bundle:
                        note_sell(item_entry, sell, here)
                        grant_rows.append((map_id, tab_id, leaf,
                                           item_entry + entry_offset, s["id"], 1))

                    sub_sort += 1
                    purchases += 1

                if sub_sort == 0:
                    fail(f'{path}: tab {v["key"]} group "{here}" produced no '
                         f"purchasable suffixes")

            for cat_sort, c in enumerate(v["categories"]):
                add_group(c, 0, cat_sort, v["key"])

            tabs_meta.append((path, map_id, tab_id, v, purchases))

        # Keyed on the entry actually GRANTED -- the +900000 copy when custom_items
        # is on -- because that is the entry the sell handler finds in the bag.
        # note_sell has already proved each one resolves to a single price.
        sell_rows.extend((map_id, entry + entry_offset, copper)
                         for entry, copper in sorted(item_sell.items()))

        if custom_items:
            item_copies.extend((entry, entry + ITEM_ENTRY_OFFSET, item_sell.get(entry, 0))
                               for entry in sorted(cfg_items))

    return npc_rows, menu_rows, grant_rows, sell_rows, tabs_meta, item_copies


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


def emit_catalog(menu_rows, grant_rows, tabs_meta):
    """Generated Lua catalog for the shop addon.

    The panel browses entirely client-side, so structure and display metadata ship
    with the addon instead of crossing the wire. The server still resolves every
    purchase from the node id, so a stale addon can only earn a refusal, never a
    wrong grant -- but it CAN display a wrong price, which is why the deploy note
    tells you to recopy the addon.

    The icon is deliberately absent: item_template.displayid indexes a client-side
    DBC, so only the client can turn it into a texture path.
    """
    by_node = {(m[0], m[1], m[2]): m for m in menu_rows}

    grants_by_node = defaultdict(list)
    for g in grant_rows:
        grants_by_node[(g[0], g[1], g[2])].append(g)

    items = load_items({stock_entry(g[3]) for g in grant_rows})

    def path_of(row):
        """Ancestor labels, root first -- the authored grouping, kept as filter
        presets now that the tree itself is flattened."""
        labels, parent = [], row[3]
        while parent:
            up = by_node[(row[0], row[1], parent)]
            labels.append(up[5])
            parent = up[3]
        labels.reverse()
        return labels

    lines = [
        "-- Generated by apps/moba/gen_store.py -- do not edit.",
        "-- Regenerate and RECOPY this addon whenever store_config.yaml changes:",
        "-- the server reads prices and grants from the DB, so a stale copy here",
        "-- displays numbers the server will not honour.",
        "",
        "MobaShopCatalog = {",
    ]

    for map_id in sorted({m[0] for m in menu_rows}):
        lines.append(f"  [{map_id}] = {{")

        lines.append("    items = {")
        # Keyed by the GRANT entry, not the stock one: the addon only ever sees
        # grant entries, so keying by stock would make every lookup miss once
        # custom_items is on. Metadata still comes from the stock row -- that
        # mapping lives in stock_entry() and nowhere else, least of all in Lua.
        for entry in sorted({g[3] for g in grant_rows if g[0] == map_id}):
            it = items[stock_entry(entry)]
            lines.append(
                f"      [{entry}] = {{ name = {lua_str(it['name'])}, "
                f"quality = {it['quality']}, slot = {it['invtype']}, "
                f"class = {it['class']}, subclass = {it['subclass']}, "
                f"ilvl = {it['ilvl']} }},")
        lines.append("    },")

        lines.append("    tabs = {")
        for _path, meta_map, tab_id, v, _purchases in tabs_meta:
            if meta_map != map_id:
                continue
            lines.append(f"      {{ tabId = {tab_id}, "
                         f"name = {lua_str(v['name'])}, leaves = {{")
            for m in menu_rows:
                if m[0] != map_id or m[1] != tab_id or not m[6]:
                    continue
                pieces = ", ".join(
                    f"{{ entry = {g[3]}, suffix = {g[4]}, count = {g[5]} }}"
                    for g in grants_by_node[(map_id, tab_id, m[2])])
                path = ", ".join(lua_str(p) for p in path_of(m))
                lines.append(
                    f"        {{ node = {m[2]}, cost = {m[7]}, "
                    f"label = {lua_str(m[5])}, path = {{ {path} }}, "
                    f"pieces = {{ {pieces} }} }},")
            lines.append("      } },")
        lines.append("    },")

        lines.append("  },")

    lines.append("}")
    return "\n".join(lines) + "\n"


def emit(npc_rows, menu_rows, grant_rows, sell_rows, item_copies, tmpl_blocks, spawn_blocks):
    # Two namespaces, deliberately resolved separately: the entry lives in
    # creature_template, the spawn guid in `creature`. They hold the same numbers
    # today only because this generator sets guid == entry.
    tmpl_window = id_alloc.sql_window(tmpl_blocks, "`entry`")
    model_window = id_alloc.sql_window(tmpl_blocks, "`CreatureID`")
    guid_window = id_alloc.sql_window(spawn_blocks, "`guid`")

    lines = [
        "-- ============================================================",
        "-- GENERATED FILE -- do not hand-edit.",
        "-- Produced by apps/moba/gen_store.py from apps/moba/maps/*/store_config.yaml.",
        "-- Owns the shopkeeper creatures (creature_template + creature_template_model),",
        "-- their spawns (creature), and the item catalog (mod_moba_store_*).",
        "-- ONE shopkeeper per team per map; TabId below is a tab in the addon",
        "-- panel, not an NPC. Browsing is client-side (Catalog.lua); these tables",
        "-- exist so the server can validate a purchase from a node id alone.",
        "-- Team is carried in mod_moba_store_npc, NOT in faction: CFBG puts players",
        "-- of either faction on either BG team, so faction cannot express team.",
        "-- Shopkeepers are faction 35 (friendly to all) and immune; npc_moba_store",
        "-- refuses players whose BG team does not match.",
        "-- ============================================================",
        "",
        "USE acore_world;",
        "",
        f"DELETE FROM `creature_template` WHERE {tmpl_window};",
        "INSERT INTO `creature_template`",
        "(`entry`, `name`, `subname`, `minlevel`, `maxlevel`, `faction`, `npcflag`,",
        " `speed_walk`, `speed_run`, `rank`, `unit_class`, `unit_flags`,",
        " `type`, `type_flags`, `MovementType`, `HealthModifier`, `ArmorModifier`,",
        " `RegenHealth`, `flags_extra`, `ScriptName`, `VerifiedBuild`)",
        "VALUES",
    ]
    ct_rows = []
    for entry, _map_id, _team, sk, t in npc_rows:
        # Team blocks override the shopkeeper's defaults, so the two sides can
        # differ in name or model without duplicating the whole block.
        ct_rows.append(
            f"({entry}, {sql_str(t.get('name', sk['name']))}, "
            f"{sql_str(t.get('subname', sk['subname']))}, 80, 80, "
            f"{SHOPKEEPER_FACTION}, {SHOPKEEPER_NPCFLAG}, "
            f"1.0, 1.14286, 0, 1, {SHOPKEEPER_UNIT_FLAGS}, "
            f"7, 0, 0, 1, 1, "
            f"1, 0, {sql_str(SHOPKEEPER_SCRIPT)}, 0)")
    lines.append(",\n".join(ct_rows) + ";")

    lines += [
        "",
        f"DELETE FROM `creature_template_model` WHERE {model_window};",
        "INSERT INTO `creature_template_model`",
        "(`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)",
        "VALUES",
    ]
    ctm_rows = [f"({entry}, 0, {t.get('display_id', sk['display_id'])}, "
                f"{t.get('display_scale', sk.get('display_scale', 1.0))}, 1, 0)"
                for entry, _m, _team, sk, t in npc_rows]
    lines.append(",\n".join(ctm_rows) + ";")

    # guid == entry: the 900000-900999 guid window is unused by core data.
    # The spawn entry column is `id`, not `id1`: upstream 2026_06_16_00.sql
    # renamed it and moved id2/id3 into creature_multispawn. base/db_world/
    # creature.sql still shows the pre-rename schema -- it is historical, and
    # the live schema is base + updates.
    lines += [
        "",
        f"DELETE FROM `creature` WHERE {guid_window};",
        "INSERT INTO `creature`",
        "(`guid`, `id`, `map`, `spawnMask`, `phaseMask`, `equipment_id`,",
        " `position_x`, `position_y`, `position_z`, `orientation`,",
        " `spawntimesecs`, `wander_distance`, `MovementType`)",
        "VALUES",
    ]
    c_rows = [f"({entry}, {entry}, {map_id}, 1, 1, 0, "
              f"{t['x']}, {t['y']}, {t['z']}, {t['o']}, 300, 0, 0)"
              for entry, map_id, _team, _sk, t in npc_rows]
    lines.append(",\n".join(c_rows) + ";")

    lines += [
        "",
        "DROP TABLE IF EXISTS `mod_moba_store_npc`;",
        "CREATE TABLE `mod_moba_store_npc` (",
        "    `CreatureEntry` INT UNSIGNED NOT NULL PRIMARY KEY,",
        "    `Map`           INT UNSIGNED NOT NULL,",
        "    `Team`          TINYINT UNSIGNED NOT NULL   -- 0 = Alliance, 1 = Horde",
        ");",
        "",
        "INSERT INTO `mod_moba_store_npc` (`CreatureEntry`, `Map`, `Team`)",
        "VALUES",
    ]
    lines.append(",\n".join(
        f"({entry}, {map_id}, {team})"
        for entry, map_id, team, _sk, _t in npc_rows) + ";")

    lines += [
        "",
        "DROP TABLE IF EXISTS `mod_moba_store_menu`;",
        "CREATE TABLE `mod_moba_store_menu` (",
        "    `Map`        INT UNSIGNED NOT NULL,",
        "    `TabId`      INT UNSIGNED NOT NULL,",
        "    `NodeId`     INT UNSIGNED NOT NULL,",
        "    `ParentId`   INT UNSIGNED NOT NULL DEFAULT 0,  -- 0 = top level",
        "    `SortOrder`  INT UNSIGNED NOT NULL DEFAULT 0,",
        "    `Label`      VARCHAR(100) NOT NULL,",
        "    `IsPurchase` TINYINT UNSIGNED NOT NULL DEFAULT 0,",
        "    `CostCopper` INT UNSIGNED NOT NULL DEFAULT 0,",
        "    PRIMARY KEY (`Map`, `TabId`, `NodeId`)",
        ");",
        "",
        "INSERT INTO `mod_moba_store_menu`",
        "(`Map`, `TabId`, `NodeId`, `ParentId`, `SortOrder`, `Label`, `IsPurchase`, `CostCopper`)",
        "VALUES",
    ]
    lines.append(",\n".join(
        f"({m}, {tid}, {nid}, {pid}, {sort}, {sql_str(label)}, {is_buy}, {cost})"
        for m, tid, nid, pid, sort, label, is_buy, cost in menu_rows) + ";")

    lines += [
        "",
        "DROP TABLE IF EXISTS `mod_moba_store_grant`;",
        "CREATE TABLE `mod_moba_store_grant` (",
        "    `Map`       INT UNSIGNED NOT NULL,",
        "    `TabId`     INT UNSIGNED NOT NULL,",
        "    `NodeId`    INT UNSIGNED NOT NULL,",
        "    `ItemEntry` INT UNSIGNED NOT NULL,",
        "    -- ItemRandomSuffix.dbc id, stored positive; 0 = no suffix. The engine",
        "    -- wants it NEGATED as randomPropertyId (see Item::GenerateItemRandomPropertyId).",
        "    `SuffixId`  INT UNSIGNED NOT NULL DEFAULT 0,",
        "    `Count`     INT UNSIGNED NOT NULL DEFAULT 1,",
        "    KEY `idx_node` (`Map`, `TabId`, `NodeId`)",
        ");",
        "",
        "INSERT INTO `mod_moba_store_grant`",
        "(`Map`, `TabId`, `NodeId`, `ItemEntry`, `SuffixId`, `Count`)",
        "VALUES",
    ]
    lines.append(",\n".join(
        f"({m}, {tid}, {nid}, {item}, {suffix}, {count})"
        for m, tid, nid, item, suffix, count in grant_rows) + ";")

    lines += [
        "",
        "DROP TABLE IF EXISTS `mod_moba_store_sell`;",
        "CREATE TABLE `mod_moba_store_sell` (",
        "    `Map`       INT UNSIGNED NOT NULL,",
        "    `ItemEntry` INT UNSIGNED NOT NULL,",
        "    -- PER UNIT, as item_template.SellPrice is: the sell handler multiplies",
        "    -- by the stack size it destroys. An entry absent from this table was",
        "    -- never sold here, and cannot be sold back.",
        "    `Copper`    INT UNSIGNED NOT NULL DEFAULT 0,",
        "    PRIMARY KEY (`Map`, `ItemEntry`)",
        ");",
        "",
        "INSERT INTO `mod_moba_store_sell` (`Map`, `ItemEntry`, `Copper`)",
        "VALUES",
    ]
    lines.append(",\n".join(
        f"({m}, {entry}, {copper})" for m, entry, copper in sell_rows) + ";")

    if item_copies:
        lines += emit_item_copies(item_copies)

    return "\n".join(lines) + "\n"


def main():
    configs = load_configs()
    npc_rows, menu_rows, grant_rows, sell_rows, tabs_meta, item_copies = build(configs)
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    reg = id_alloc.Registry()
    OUTPUT.write_text(emit(npc_rows, menu_rows, grant_rows, sell_rows, item_copies,
                           reg.blocks_of(id_alloc.CREATURE_TEMPLATE, "store"),
                           reg.blocks_of(id_alloc.CREATURE_SPAWN, "store")))
    CATALOG_OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    CATALOG_OUTPUT.write_text(emit_catalog(menu_rows, grant_rows, tabs_meta))

    print(f"Wrote {OUTPUT}")
    print(f"Wrote {CATALOG_OUTPUT}")
    for path, map_id, tab_id, v, purchases in tabs_meta:
        print(f"  map {map_id} tab {tab_id} ({v['key']}): "
              f"{len(v['categories'])} categories, {purchases} purchase nodes")
    print(f"  {len(npc_rows)} shopkeeper NPCs, {len(menu_rows)} menu nodes, "
          f"{len(grant_rows)} grant rows, {len(sell_rows)} sell prices")
    if item_copies:
        print(f"  {len(item_copies)} custom item copies (source entry + {ITEM_ENTRY_OFFSET})")
    else:
        print("  custom_items off -- grants use stock item entries")
    print("Restart worldserver -- mod_moba_store.sql auto-applies on boot, and the "
          "data store caches once per process. Recopy the MobaHUD addon too: "
          "Catalog.lua ships with it and is never sent over the wire.")


if __name__ == "__main__":
    main()
