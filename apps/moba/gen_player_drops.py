#!/usr/bin/env python3
"""
MOBA player-kill-drops generator.

Reads per-map player configs (apps/moba/maps/<mode>/player_config.yaml) and
generates data/sql/custom/db_world/mod_moba_player_drops.sql: the
Map-keyed mod_moba_player_drops table, rolled and delivered directly to the
killer by BattlegroundMOBA::GrantPlayerKillDrops at the killing blow.

Reuses validate_drops from gen_creep_roster.py -- the "drops" list shape
(buff/gold/item, chance coefficient) is identical to the creep/neutral
configs. What's NOT reused: build_drop_rows / emit_loot_template_sql /
emit_drops_table_sql. Those assume a creature entry to hang native
creature_loot_template rows off of; players have none, and kill rewards are
direct grants (AddAura / ModifyMoney / AddItem), not corpse loot. So here
ALL three drop types -- including "item" -- land in one grant table keyed by
Map instead of CreatureEntry.

Usage (from the repo root):
    python3 apps/moba/gen_player_drops.py
"""

import yaml
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from gen_creep_roster import DROP_TYPE_IDS, validate_drops
from item_copy import copy_entry, emit_copy_sql, write_manifest

MAPS_DIR = Path(__file__).parent / "maps"
OUTPUT = Path("data/sql/custom/db_world/mod_moba_player_drops.sql")
# Ownership tag in mod_moba_item_copy, and the manifest's filename.
COPY_OWNER = "player_drops"


def fail(msg):
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def load_configs():
    configs = []
    for path in sorted(MAPS_DIR.glob("*/player_config.yaml")):
        cfg = yaml.safe_load(path.read_text())
        if not isinstance(cfg.get("map"), int):
            fail(f'{path}: "map" must be an integer map id')
        validate_drops(cfg, f'"{path}"')
        configs.append((path, cfg))
    if not configs:
        fail(f"no player configs found under {MAPS_DIR}/*/player_config.yaml")
    return configs


def build_rows(map_id, drops):
    rows, copies = [], []
    for drop in drops:
        chance = f"{drop.get('chance', 1.0) * 100:g}"
        item, sell = 0, 0
        if drop["type"] == "item":
            # The killer is handed the fork-owned COPY, never the stock entry -- that is
            # what lets the exit sweep take it back without touching a player's own stock.
            # The copy carries `sell` as its SellPrice, so the tooltip and the refund the
            # shop actually pays are the same number.
            item = copy_entry(drop["item"])
            sell = drop.get("sell", 0)
            copies.append((drop["item"], drop.get("sell")))   # None, not 0 -- absent
                                                              # means "no opinion"
        rows.append(
            f"({map_id}, {len(rows)}, {DROP_TYPE_IDS[drop['type']]}, "
            f"{drop.get('spell', 0)}, {drop.get('duration_ms', 0)}, "
            f"{drop.get('copper', 0)}, {item}, {drop.get('count', 1)}, {sell}, {chance})")
    return rows, copies


def emit(configs):
    lines = [
        "-- ============================================================",
        "-- GENERATED FILE -- do not hand-edit.",
        "-- Produced by apps/moba/gen_player_drops.py from apps/moba/maps/*/player_config.yaml.",
        "-- Rolled and delivered by BattlegroundMOBA::GrantPlayerKillDrops at the",
        "-- killing blow, straight to the killer -- no corpse, no native loot.",
        "-- Type 0 = buff (aura on the killer; DurationMs 0 = the spell's",
        "-- default), 1 = gold (ModifyMoney), 2 = item (AddItem, Count each).",
        "-- Sell is type 2 only, PER UNIT: the buy-back price npc_moba_store refunds,",
        "-- 0 meaning the item cannot be sold back.",
        "-- Chance is a percent (config coefficient x 100).",
        "-- ============================================================",
        "",
        "USE acore_world;",
        "",
        "DROP TABLE IF EXISTS `mod_moba_player_drops`;",
        "CREATE TABLE `mod_moba_player_drops` (",
        "    `Map`        INT UNSIGNED NOT NULL,",
        "    `Idx`        TINYINT UNSIGNED NOT NULL,",
        "    `Type`       TINYINT UNSIGNED NOT NULL,",
        "    `Spell`      INT UNSIGNED NOT NULL DEFAULT 0,",
        "    `DurationMs` INT UNSIGNED NOT NULL DEFAULT 0,",
        "    `Copper`     INT UNSIGNED NOT NULL DEFAULT 0,",
        "    `Item`       INT UNSIGNED NOT NULL DEFAULT 0,",
        "    `Count`      INT UNSIGNED NOT NULL DEFAULT 1,",
        "    `Sell`       INT UNSIGNED NOT NULL DEFAULT 0,   -- type 2 only, PER UNIT",
        "    `Chance`     FLOAT NOT NULL DEFAULT 100,",
        "    PRIMARY KEY (`Map`, `Idx`)",
        ");",
    ]
    all_rows, copies = [], []
    for _, cfg in configs:
        rows, copied = build_rows(cfg["map"], cfg.get("drops", []))
        all_rows += rows
        copies += copied
    if all_rows:
        lines += [
            "",
            "INSERT INTO `mod_moba_player_drops`",
            "(`Map`, `Idx`, `Type`, `Spell`, `DurationMs`, `Copper`, `Item`, `Count`, `Sell`, `Chance`)",
            "VALUES",
            ",\n".join(all_rows) + ";",
        ]
    lines += emit_copy_sql(COPY_OWNER, copies)
    return "\n".join(lines) + "\n", copies


def main():
    configs = load_configs()
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    sql, copies = emit(configs)
    OUTPUT.write_text(sql)
    sources = sorted({src for src, _sell in copies})
    manifest = write_manifest(COPY_OWNER, sources)
    maps = ", ".join(str(c["map"]) for _, c in configs)
    print(f"Wrote {OUTPUT} ({len(configs)} map(s): {maps}).")
    print(f"Wrote {manifest} ({len(sources)} item copies).")
    print("Restart worldserver — the SQL auto-applies from data/sql/custom/db_world on boot.")


if __name__ == "__main__":
    main()
