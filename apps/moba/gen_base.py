#!/usr/bin/env python3
"""
MOBA base generator.

Reads per-map base configs (apps/moba/maps/<mode>/base_config.yaml) and
generates data/sql/custom/db_world/mod_moba_base.sql: the map-keyed mod_moba_base table
(respawn timing, recall cast times, fountain healing), the per-map spawn wiring
(game_graveyard coordinates plus the battleground_template start-location /
orientation that back them), and the spawn dome gameobject_template (+ _addon) rows.

"Base" here means the team's base -- everything anchored to it. Tunables live in
the table; the base LOCATION is written into game_graveyard /
battleground_template, which the server already reads via GetTeamStartPosition /
GetClosestGraveyard (see MobaBaseData.{h,cpp}).

spawn.radius drives BOTH halves of the base bubble from one number: the dome GOs'
gameobject_template.size, and mod_moba_base.FountainRadius. It is deliberately NOT
written to battleground_template.StartMaxDist -- that field arms the core's
prep-phase leash, which teleports players back to spawn every 9s.

Usage (from the repo root):
    python3 apps/moba/gen_base.py
"""

import yaml
import sys
from pathlib import Path

import id_alloc

MAPS_DIR = Path(__file__).parent / "maps"
OUTPUT = Path("data/sql/custom/db_world/mod_moba_base.sql")

# The spawn dome is a COPY of the EotS force-field GO (184719/184720), not those
# entries themselves: `size` is per-entry, so per-map radii need per-map entries,
# and editing the core rows would resize stock EotS's own barriers too. Those
# entries are assigned from this generator's block in apps/moba/id_blocks.json.
DOME_DISPLAY_ID = 7203             # NS_BioDome_BG.mdx
# GameObjectDisplayInfo GeoBox for 7203 is +-172.4 horizontally, so dome radius in
# yards == this * gameobject_template.size. Re-derive if the display id changes.
DOME_MODEL_HALF_EXTENT = 172.4
# Faction and flags come ONLY from gameobject_template_addon -- GameObject's ctor
# reads them nowhere else, so a template copy with no addon row spawns faction 0 /
# flags 0: client-selectable, and GameObject::Use opens a DOOR unconditionally (no
# lock or faction check), letting players lift their own dome before the match.
DOME_ADDON_FACTION = 1375   # the faction the source rows 184719/184720 use
# GO_FLAG_NODESPAWN (0x20), as on the source rows -- inert for type DOOR, since the
# only check on it is GOOBER-only, so SpawnBGObject still lifts the dome at start.
# GO_FLAG_NOT_SELECTABLE (0x10) is what actually removes the tooltip and the click.
DOME_ADDON_FLAGS = 48

REQUIRED_RESPAWN = ["base_ms", "per_min_ms", "cap_ms"]
REQUIRED_SPAWN_TEAM = ["graveyard_id", "x", "y", "z", "o"]


def fail(msg):
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def validate(cfg, path):
    if not isinstance(cfg.get("map"), int):
        fail(f'{path}: "map" must be an integer map id')
    if not isinstance(cfg.get("battleground_template_id"), int):
        fail(f'{path}: "battleground_template_id" must be an integer')
    window = cfg.get("kill_credit_window_ms")
    if window is not None and (not isinstance(window, int) or window < 0):
        fail(f'{path}: "kill_credit_window_ms" must be a non-negative integer (ms; 0 = disabled)')
    aw = cfg.get("assist_window_ms")
    if aw is not None and (not isinstance(aw, int) or aw < 0):
        fail(f'{path}: "assist_window_ms" must be a non-negative integer (ms; 0 = disabled)')
    abd = cfg.get("assist_buff_max_duration_ms")
    if abd is not None and (not isinstance(abd, int) or abd < 0):
        fail(f'{path}: "assist_buff_max_duration_ms" must be a non-negative integer (ms)')
    respawn = cfg.get("respawn")
    if not isinstance(respawn, dict) or any(k not in respawn for k in REQUIRED_RESPAWN):
        fail(f'{path}: "respawn" must contain {REQUIRED_RESPAWN}')
    spawn = cfg.get("spawn")
    if not isinstance(spawn, dict) or "alliance" not in spawn or "horde" not in spawn:
        fail(f'{path}: "spawn" must have "alliance" and "horde"')
    # Positive, because 0 would silently disable the fountain heal zone.
    if not isinstance(spawn.get("radius"), (int, float)) or spawn["radius"] <= 0:
        fail(f'{path}: "spawn.radius" must be a positive number (yards)')
    dome = spawn.get("dome")
    if not isinstance(dome, dict):
        fail(f'{path}: "spawn.dome" must be a block with "alliance_entry" and "horde_entry"')
    for k in ("alliance_entry", "horde_entry"):
        if not isinstance(dome.get(k), int):
            fail(f'{path}: "spawn.dome.{k}" must be an integer gameobject_template entry')
    if dome["alliance_entry"] == dome["horde_entry"]:
        fail(f'{path}: "spawn.dome" entries must differ')
    for team in ("alliance", "horde"):
        for k in REQUIRED_SPAWN_TEAM:
            if k not in spawn[team]:
                fail(f'{path}: spawn.{team} missing "{k}"')
    recall = cfg.get("recall")
    if recall is not None:
        if not isinstance(recall, dict) or not isinstance(recall.get("cast_time_ms"), int):
            fail(f'{path}: "recall.cast_time_ms" must be an integer (ms)')
        emp = recall.get("empowered_cast_time_ms")
        if emp is not None and not isinstance(emp, int):
            fail(f'{path}: "recall.empowered_cast_time_ms" must be an integer (ms)')
    fountain = cfg.get("fountain")
    if fountain is not None:
        if not isinstance(fountain, dict):
            fail(f'{path}: "fountain" must be an object')
        for k in ("tick_ms", "hp_pct", "mana_pct"):
            if fountain.get(k) is not None and not isinstance(fountain[k], int):
                fail(f'{path}: "fountain.{k}" must be an integer')
    gold = cfg.get("gold")
    if gold is not None:
        if not isinstance(gold, dict):
            fail(f'{path}: "gold" must be an object')
        for k in ("starting_stipend", "passive_tick_ms", "passive_per_tick",
                  "first_blood", "shutdown_per_streak", "shutdown_cap"):
            v = gold.get(k, 0)
            if not isinstance(v, int) or v < 0:
                fail(f'{path}: "gold.{k}" must be a non-negative integer')
    streaks = cfg.get("streaks")
    if streaks is not None:
        if not isinstance(streaks, dict):
            fail(f'{path}: "streaks" must be an object')
        for k in ("multi_kill_window_ms", "spree_min", "ace_min_team"):
            v = streaks.get(k, 0)
            if not isinstance(v, int) or v < 0:
                fail(f'{path}: "streaks.{k}" must be a non-negative integer')


def load_configs():
    configs = []
    seen_domes = {}
    for path in sorted(MAPS_DIR.glob("*/base_config.yaml")):
        cfg = yaml.safe_load(path.read_text())
        validate(cfg, path)
        for k in ("alliance_entry", "horde_entry"):
            entry = cfg["spawn"]["dome"][k]
            if entry in seen_domes:
                fail(f'{path}: dome entry {entry} is already used by {seen_domes[entry]}')
            seen_domes[entry] = path
        configs.append((path, cfg))
    if not configs:
        fail(f"no base configs found under {MAPS_DIR}/*/base_config.yaml")
    id_alloc.validate_owner(id_alloc.GAMEOBJECT_TEMPLATE, "base_dome")
    return configs


def emit(configs, blocks):
    dome_window = id_alloc.sql_window(blocks, "`entry`")
    lines = [
        "-- ============================================================",
        "-- GENERATED FILE -- do not hand-edit.",
        "-- Produced by apps/moba/gen_base.py from apps/moba/maps/*/base_config.yaml.",
        "-- Tunables live in mod_moba_base; the base LOCATION is written into",
        "-- game_graveyard / battleground_template (read at runtime via",
        "-- GetTeamStartPosition / GetClosestGraveyard).",
        "-- ============================================================",
        "",
        "USE acore_world;",
        "",
        "DROP TABLE IF EXISTS `mod_moba_base`;",
        "CREATE TABLE `mod_moba_base` (",
        "    `Map`             INT UNSIGNED NOT NULL PRIMARY KEY,      -- BG map id",
        "    `RespawnBaseMs`   INT UNSIGNED NOT NULL DEFAULT 10000,    -- base respawn wait",
        "    `RespawnPerMinMs` INT UNSIGNED NOT NULL DEFAULT 1500,     -- added per elapsed match-minute",
        "    `RespawnCapMs`    INT UNSIGNED NOT NULL DEFAULT 60000,    -- maximum respawn wait",
        "    `RecallCastMs`          INT UNSIGNED NOT NULL DEFAULT 0,  -- recall cast time (ms); 0 = spell default",
        "    `RecallEmpoweredCastMs` INT UNSIGNED NOT NULL DEFAULT 0,  -- empowered recall cast time (ms); 0 = fall back to normal",
        "    `FountainTickMs`  INT UNSIGNED NOT NULL DEFAULT 0,        -- fountain heal cadence (ms); 0 = fountain healing off",
        "    `FountainHpPct`   INT UNSIGNED NOT NULL DEFAULT 0,        -- % of max health restored per tick",
        "    `FountainManaPct` INT UNSIGNED NOT NULL DEFAULT 0,        -- % of max mana restored per tick (mana users only)",
        "    `FountainRadius`  FLOAT NOT NULL DEFAULT 0,               -- spawn-dome radius (yards) = heal zone; 0 = fountain healing off",
        "    `KillCreditWindowMs` INT UNSIGNED NOT NULL DEFAULT 15000,  -- window after enemy-player damage/debuff in which a death still credits that player (0 = off)",
        "    `AssistWindowMs` INT UNSIGNED NOT NULL DEFAULT 10000,      -- window before a death in which damage/debuff/support earns an assist (0 = off)",
        "    `AssistBuffMaxDurationMs` INT UNSIGNED NOT NULL DEFAULT 60000, -- max buff/shield duration (ms) counting as a fight buff for assists; longer = maintenance buff, ignored",
        "    `DomeEntryAlliance` INT UNSIGNED NOT NULL DEFAULT 0,  -- gameobject_template entry of the Alliance spawn dome",
        "    `DomeEntryHorde`    INT UNSIGNED NOT NULL DEFAULT 0,  -- gameobject_template entry of the Horde spawn dome",
        "    `StartingGold`  INT UNSIGNED NOT NULL DEFAULT 0,      -- copper in the match wallet on entry (0 = none)",
        "    `PassiveTickMs` INT UNSIGNED NOT NULL DEFAULT 0,      -- passive income cadence (ms); 0 = passive income off",
        "    `PassiveCopper` INT UNSIGNED NOT NULL DEFAULT 0,      -- copper per tick, per player",
        "    `FirstBloodGold`      INT UNSIGNED NOT NULL DEFAULT 0,     -- bonus copper for the match's first player kill (0 = off)",
        "    `ShutdownPerStreak`   INT UNSIGNED NOT NULL DEFAULT 0,     -- bounty copper per kill on the victim's streak (0 = no bounty)",
        "    `ShutdownCapGold`     INT UNSIGNED NOT NULL DEFAULT 0,     -- ceiling on that bounty (0 = uncapped)",
        "    `MultiKillWindowMs`   INT UNSIGNED NOT NULL DEFAULT 10000, -- a kill this soon after the last extends the multi-kill (0 = multi-kills off)",
        "    `SpreeMin`            INT UNSIGNED NOT NULL DEFAULT 3,     -- consecutive kills that announce a spree AND mark a shutdown target (0 = both off)",
        "    `AceMinTeam`          INT UNSIGNED NOT NULL DEFAULT 2      -- smallest wiped team that counts as an ace (0 = ace off)",
        ");",
        "",
        "INSERT INTO `mod_moba_base` (`Map`, `RespawnBaseMs`, `RespawnPerMinMs`, `RespawnCapMs`, `RecallCastMs`, `RecallEmpoweredCastMs`, `FountainTickMs`, `FountainHpPct`, `FountainManaPct`, `FountainRadius`, `KillCreditWindowMs`, `AssistWindowMs`, `AssistBuffMaxDurationMs`, `DomeEntryAlliance`, `DomeEntryHorde`, `StartingGold`, `PassiveTickMs`, `PassiveCopper`, `FirstBloodGold`, `ShutdownPerStreak`, `ShutdownCapGold`, `MultiKillWindowMs`, `SpreeMin`, `AceMinTeam`)",
        "VALUES",
    ]
    rows = []
    for _, cfg in configs:
        t = cfg["respawn"]
        recall = cfg.get("recall") or {}
        recall_ms = recall.get("cast_time_ms", 0)
        recall_emp_ms = recall.get("empowered_cast_time_ms", 0)
        f = cfg.get("fountain") or {}
        gold = cfg.get("gold") or {}
        streaks = cfg.get("streaks") or {}
        rows.append(
            f"({cfg['map']}, {t['base_ms']}, {t['per_min_ms']}, {t['cap_ms']}, {recall_ms}, {recall_emp_ms}, "
            f"{f.get('tick_ms', 0)}, {f.get('hp_pct', 0)}, {f.get('mana_pct', 0)}, "
            f"{cfg['spawn']['radius']}, "
            f"{cfg.get('kill_credit_window_ms', 15000)}, "
            f"{cfg.get('assist_window_ms', 10000)}, "
            f"{cfg.get('assist_buff_max_duration_ms', 60000)}, "
            f"{cfg['spawn']['dome']['alliance_entry']}, "
            f"{cfg['spawn']['dome']['horde_entry']}, "
            f"{gold.get('starting_stipend', 0)}, "
            f"{gold.get('passive_tick_ms', 0)}, "
            f"{gold.get('passive_per_tick', 0)}, "
            f"{gold.get('first_blood', 0)}, "
            f"{gold.get('shutdown_per_streak', 0)}, "
            f"{gold.get('shutdown_cap', 0)}, "
            f"{streaks.get('multi_kill_window_ms', 10000)}, "
            f"{streaks.get('spree_min', 3)}, "
            f"{streaks.get('ace_min_team', 2)})")
    lines.append(",\n".join(rows) + ";")

    # One dome pair per map, sized from that map's spawn.radius. Copies of
    # 184719/184720: type 0 (DOOR) with every Data field zero, so behaviour is
    # identical to the barrier the EotS battleground uses.
    lines += [
        "",
        "-- Spawn dome gameobjects. The whole block is cleared, not just the entries",
        "-- being inserted, so a dome dropped from a config is dropped from the DB too.",
        f"DELETE FROM `gameobject_template` WHERE {dome_window};",
        "INSERT INTO `gameobject_template`",
        "(`entry`, `type`, `displayId`, `name`, `IconName`, `castBarCaption`, `unk1`, `size`,",
        " `Data0`, `Data1`, `Data2`, `Data3`, `Data4`, `Data5`, `Data6`, `Data7`, `Data8`, `Data9`,",
        " `Data10`, `Data11`, `Data12`, `Data13`, `Data14`, `Data15`, `Data16`, `Data17`, `Data18`,",
        " `Data19`, `Data20`, `Data21`, `Data22`, `Data23`, `AIName`, `ScriptName`, `VerifiedBuild`)",
        "VALUES",
    ]
    dome_entries = []
    dome_rows = []
    zeros = ", ".join(["0"] * 24)
    for path, cfg in configs:
        mode = path.parent.name
        size = cfg["spawn"]["radius"] / DOME_MODEL_HALF_EXTENT
        for team, key in (("Alliance", "alliance_entry"), ("Horde", "horde_entry")):
            entry = cfg["spawn"]["dome"][key]
            dome_entries.append(entry)
            dome_rows.append(
                f"({entry}, 0, {DOME_DISPLAY_ID}, '{mode} spawn dome ({team})', '', '', '', "
                f"{size:.6f}, {zeros}, '', '', 0)")
    lines.append(",\n".join(dome_rows) + ";")

    lines += [
        "",
        "-- Dome faction/flags. GameObject reads both ONLY from this table, so a template",
        "-- copy with no row here is selectable and clickable -- and clicking a DOOR opens it.",
        f"DELETE FROM `gameobject_template_addon` WHERE {dome_window};",
        "INSERT INTO `gameobject_template_addon`",
        "(`entry`, `faction`, `flags`, `mingold`, `maxgold`, `artkit0`, `artkit1`, `artkit2`, `artkit3`)",
        "VALUES",
    ]
    lines.append(",\n".join(
        f"({entry}, {DOME_ADDON_FACTION}, {DOME_ADDON_FLAGS}, 0, 0, 0, 0, 0, 0)"
        for entry in dome_entries) + ";")

    for path, cfg in configs:
        mode = path.parent.name
        spawn = cfg["spawn"]
        a, h = spawn["alliance"], spawn["horde"]
        lines += [
            "",
            f"-- Spawn wiring for map {cfg['map']} ({mode})",
            "-- StartMaxDist stays 0 ON PURPOSE. It is the core's prep-phase leash",
            "-- (Battleground::_CheckSafePositions), which teleports players back to spawn",
            "-- every 9s -- wrong for a base you are meant to walk around in. The dome holds",
            "-- players in; the radius lives in mod_moba_base.FountainRadius.",
            (f"UPDATE battleground_template SET "
             f"AllianceStartLoc = {a['graveyard_id']}, AllianceStartO = {a['o']}, "
             f"HordeStartLoc = {h['graveyard_id']}, HordeStartO = {h['o']}, "
             f"StartMaxDist = 0 "
             f"WHERE ID = {cfg['battleground_template_id']};"),
            f"UPDATE game_graveyard SET x = {a['x']}, y = {a['y']}, z = {a['z']} WHERE ID = {a['graveyard_id']};",
            f"UPDATE game_graveyard SET x = {h['x']}, y = {h['y']}, z = {h['z']} WHERE ID = {h['graveyard_id']};",
        ]
    return "\n".join(lines) + "\n"


def main():
    configs = load_configs()
    blocks = id_alloc.Registry().blocks_of(id_alloc.GAMEOBJECT_TEMPLATE, "base_dome")
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(emit(configs, blocks))
    maps = ", ".join(str(c["map"]) for _, c in configs)
    print(f"Wrote {OUTPUT} ({len(configs)} map(s): {maps}).")
    print("Restart worldserver — the SQL auto-applies from data/sql/custom/db_world on boot.")


if __name__ == "__main__":
    main()
