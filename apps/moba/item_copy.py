#!/usr/bin/env python3
"""
Fork-owned item_template copies -- shared by every generator that hands an item out.

Two kinds. A mirror copy's entry is its SOURCE entry + 900000, so two generators
cloning one source reach one row. A baked copy writes a random suffix's stats into a
clone of its source; one source backs one copy per suffix, so its entry is allocated
through id_alloc.py instead. Why the offset rather than an id_blocks.json window, and
how the tracker table and the per-owner manifest referee a namespace with several
writers, is in apps/moba/README.md under "Item copies".

`owner` names the GENERATED SQL FILE, not the concept -- a file reclaims only its own
tracker rows.
"""

import json
import sys
from pathlib import Path

# Custom copy entry = source entry + this offset (36063 -> 936063). Deterministic, so
# re-runs are stable without a lockfile and the mapping stays readable.
ITEM_ENTRY_OFFSET = 900000
STOCK_ITEM_MAX = 56806           # the last 3.3.5a item (DBCStores.cpp:643); the mirror ends here

MANIFEST_DIR = Path("apps/moba/item_copies")

BIND_WHEN_PICKED_UP = 1          # ItemBondingType; binds in _StoreItem, no client prompt


def fail(msg):
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def _sql_str(s):
    return "'" + str(s).replace("'", "''") + "'"


def stock_entry(entry):
    """The item_template entry a mirror copy was cloned from.

    Copies have no item_template.sql row of their own, so every metadata lookup goes
    through here and the +900000 relationship lives in exactly one place. A baked copy
    sits above the mirror, where subtracting the offset names no item.
    """
    if entry > ITEM_ENTRY_OFFSET + STOCK_ITEM_MAX:
        fail(f"item {entry} is above the +{ITEM_ENTRY_OFFSET} mirror -- a baked copy has "
             f"no stock entry to look up")
    return entry - ITEM_ENTRY_OFFSET if entry >= ITEM_ENTRY_OFFSET else entry


def copy_entry(entry):
    return entry + ITEM_ENTRY_OFFSET


def _reclaim_lines(owner):
    """Create the shared tracker if absent, then reclaim this owner's previous copies."""
    tag = _sql_str(owner)
    return [
        "CREATE TABLE IF NOT EXISTS `mod_moba_item_copy` (",
        "    `Entry` INT UNSIGNED NOT NULL,",
        "    `Owner` VARCHAR(32) NOT NULL,",
        "    PRIMARY KEY (`Entry`, `Owner`)",
        ");",
        "",
        "-- Reclaim this generator's previous copies, sparing any a second owner",
        "-- still claims.",
        "DELETE it FROM `item_template` it",
        f"    JOIN `mod_moba_item_copy` mine ON mine.`Entry` = it.`entry`"
        f" AND mine.`Owner` = {tag}",
        f"    LEFT JOIN `mod_moba_item_copy` other ON other.`Entry` = it.`entry`"
        f" AND other.`Owner` <> {tag}",
        "WHERE other.`Entry` IS NULL;",
        "",
        f"DELETE FROM `mod_moba_item_copy` WHERE `Owner` = {tag};",
    ]


def emit_copy_sql(owner, copies, bonding=None):
    """SQL for one owner's copies -- and the cleanup when it has none, so an item
    dropped from a config loses its copy on the next run.

    `copies` is (source entry, per-unit sell price in copper) pairs, the price None where
    nothing prices the item; `bonding` is an ItemBondingType stamped on every copy, or
    None to keep whatever the source binds as.
    """
    by_src = {}
    for src, sell in copies:
        # An absent price is "no opinion", not zero. An unpriced drop emits no pricing
        # row at all, so a sibling that DOES price the item is the whole truth about it --
        # conflating the two would fail the run on a config the server handles fine.
        if sell is None:
            by_src.setdefault(src, None)
            continue

        prior = by_src.get(src)
        if prior is not None and prior != sell:
            fail(f"item {src} carries two sell prices ({prior} and {sell} copper); "
                 f"SellPrice is a column on the copied row, so one copy cannot hold both")
        by_src[src] = sell
    # Nothing priced it: SellPrice 0, so a tooltip cannot advertise the stock price for
    # something the shop will refuse to buy back.
    rows = [(src, sell or 0) for src, sell in sorted(by_src.items())]

    tag = _sql_str(owner)
    lines = [
        "",
        "-- ============================================================",
        f"-- Custom item copies: entry = source entry + {ITEM_ENTRY_OFFSET}.",
        "-- mod_moba_item_copy is SHARED -- several generators write into",
        "-- 900000-999999, and a source claimed by two of them resolves to ONE row.",
        "-- Hence CREATE ... IF NOT EXISTS and a per-owner DELETE; this table is never",
        "-- dropped. The updater re-applies only files whose hash changed, so a",
        "-- generator that clears another's rows may not see them rebuilt.",
        "-- ============================================================",
    ] + _reclaim_lines(owner)

    if not rows:
        return lines

    sources = ", ".join(str(src) for src, _sell in rows)
    copy_ids = ", ".join(str(copy_entry(src)) for src, _sell in rows)

    lines += [
        "",
        "-- Cloned through a temporary table rather than a column list: CREATE ... LIKE",
        "-- carries whatever schema the server actually has, so an upstream column add",
        "-- flows through untouched. DBUpdater::ApplyFile invokes the mysql CLI once per",
        "-- file, so one session spans these statements and the TEMPORARY table lives.",
        "DROP TEMPORARY TABLE IF EXISTS `_moba_item_copy`;",
        "CREATE TEMPORARY TABLE `_moba_item_copy` LIKE `item_template`;",
        "",
        f"INSERT INTO `_moba_item_copy` SELECT * FROM `item_template`"
        f" WHERE `entry` IN ({sources});",
        "",
        "UPDATE `_moba_item_copy`",
    ]

    prefix = "   SET "
    if bonding is not None:
        lines.append(f"   SET `Bonding` = {bonding},")
        prefix = "       "
    lines.append(f"{prefix}`SellPrice` = CASE `entry`")
    lines += [f"           WHEN {src} THEN {sell}" for src, sell in rows]
    lines += [
        "           ELSE `SellPrice`",
        "       END;",
        "",
        "-- Renumber LAST. MySQL evaluates a multi-column SET left to right, so a CASE",
        "-- on `entry` folded into the UPDATE above would read the incremented value.",
        f"UPDATE `_moba_item_copy` SET `entry` = `entry` + {ITEM_ENTRY_OFFSET};",
        "",
        "-- Rewrite rather than skip: another owner may already hold this entry from",
        "-- the same source, and this run's overrides are the current ones.",
        f"DELETE FROM `item_template` WHERE `entry` IN ({copy_ids});",
        "INSERT INTO `item_template` SELECT * FROM `_moba_item_copy`;",
        "DROP TEMPORARY TABLE `_moba_item_copy`;",
        "",
        "INSERT INTO `mod_moba_item_copy` (`Entry`, `Owner`) VALUES",
    ]
    lines.append(",\n".join(f"({copy_entry(src)}, {tag})" for src, _sell in rows) + ";")

    return lines


def emit_baked_sql(owner, bakes):
    """SQL for one owner's baked copies -- and the cleanup when it has none.

    `bakes` is (source entry, copy entry, name, item level, [(ITEM_MOD_*, value), ...])
    tuples. The source's random property and suffix are zeroed, or the copy would roll a
    second "of the X" on top of the baked one. SellPrice is 0 for the reason
    emit_copy_sql gives; Bonding stays the source's.
    """
    tag = _sql_str(owner)
    lines = [
        "",
        "-- ============================================================",
        "-- Baked item copies: a source cloned once per suffix, each under an entry",
        "-- allocated from this generator's item_template block, with the suffix's",
        "-- stats written into the row. Reclaimed through mod_moba_item_copy like the",
        "-- +900000 copies.",
        "-- ============================================================",
    ] + _reclaim_lines(owner)

    if not bakes:
        return lines

    lines += [
        "",
        "-- Each clone is renumbered before the next clone of the same source lands, so",
        "-- one source can back several copies.",
        "DROP TEMPORARY TABLE IF EXISTS `_moba_item_bake`;",
        "CREATE TEMPORARY TABLE `_moba_item_bake` LIKE `item_template`;",
    ]
    for src, entry, name, item_level, stats in bakes:
        stat_sets = ", ".join(f"`stat_type{i}` = {stat}, `stat_value{i}` = {value}"
                              for i, (stat, value) in enumerate(stats, 1))
        lines += [
            "",
            f"INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = {src};",
            "UPDATE `_moba_item_bake`",
            f"   SET `entry` = {entry}, `name` = {_sql_str(name)}, `ItemLevel` = {item_level},",
            f"       {stat_sets},",
            "       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0",
            f" WHERE `entry` = {src};",
        ]

    entries = ", ".join(str(entry) for _src, entry, *_rest in bakes)
    lines += [
        "",
        "-- A copy the tracker lost would otherwise fail the INSERT on its key.",
        f"DELETE FROM `item_template` WHERE `entry` IN ({entries});",
        "INSERT INTO `item_template` SELECT * FROM `_moba_item_bake`;",
        "DROP TEMPORARY TABLE `_moba_item_bake`;",
        "",
        "INSERT INTO `mod_moba_item_copy` (`Entry`, `Owner`) VALUES",
    ]
    lines.append(",\n".join(f"({entry}, {tag})" for _src, entry, *_rest in bakes) + ";")

    return lines


def write_manifest(owner, sources):
    """One manifest per owner -- the source entries the client patch needs Item.dbc
    rows for. Written even when empty; dbc_tool.py's item_copy_rows() says why."""
    path = MANIFEST_DIR / f"{owner}.json"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps({
        "owner": owner,
        "offset": ITEM_ENTRY_OFFSET,
        "sources": sorted(set(sources)),
    }, indent=2) + "\n")
    return path


def write_pair_manifest(owner, pairs):
    """write_manifest for baked copies: explicit (source, copy) pairs, since an
    allocated entry cannot be derived from its source."""
    path = MANIFEST_DIR / f"{owner}.json"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps({
        "owner": owner,
        "pairs": sorted(([src, copy] for src, copy in pairs), key=lambda p: p[1]),
    }, indent=2) + "\n")
    return path
