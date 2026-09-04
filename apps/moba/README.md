# MOBA content generators

Generators and per-map config for the battleground's data-driven content. Each
map/mode is a self-contained bundle under `apps/moba/maps/<mode>/` (e.g.
`maps/twisted_treeline_v2/`) holding that map's `*_config.yaml`; the generators glob
those and emit one combined SQL file per content type into
`data/sql/custom/db_world/` (auto-applied on worldserver boot), each carrying a
"GENERATED — do not hand-edit" header.

> **Requires PyYAML** (configs are YAML): `pip3 install pyyaml`.

> **Run `apps/moba/gen_all.sh`** to regenerate everything. It cds to the repo
> root itself (generators resolve their output paths relative to it) and runs
> `gen_creep_paths.py` before `gen_creep_roster.py` — the roster reads each creep's
> `WaypointPathId` and `ReferencePathId` out of the lane lockfile, and it is the only
> ordering constraint between generators. Each script stays runnable alone.

**Workflows live in `.github/MOBA_GUIDE.md`** — walking a lane, adding a creep,
moving a tower. **Per-field semantics live in each config's own YAML header**, next to
the values they describe. This file holds what no single config can say: the pipeline,
the policies spanning configs, and the ID rules.

**Client-side map authoring lives in `apps/moba/wmo/`** — the blockout builder
plus the WMO export and verification toolchain for the custom-terrain route,
driven from `apps/moba/gen_blockout.py` off each bundle's `map_source.yaml`. It
emits a `.blend` and a client patch rather than SQL, is not part of
`gen_all.sh`, and carries its own README.

| Generator | Reads | Writes |
|---|---|---|
| `gen_creep_roster.py` | `maps/<mode>/creep_config.yaml` + `data/sql/base/db_world/creature_template.sql` | `mod_moba_creeps.sql` — `creature_template`, models, equipment, `mod_moba_creep_data`, `mod_moba_creep_drops`, native `creature_loot_template` rows |
| `gen_neutral_camps.py` | `maps/<mode>/neutral_config.yaml` + `data/sql/base/db_world/creature_template.sql` | `mod_moba_neutrals.sql` — `creature_template`, models, camp/member/behavior/drops tables, native `creature_loot_template` rows |
| `gen_creep_paths.py` | `maps/<mode>/lane_config.yaml` | `mod_moba_creep_paths.sql` — densified, formation-offset `waypoint_data`, plus one bare centreline path per lane per direction as the speed-compensation reference |
| `gen_tower_data.py` | `maps/<mode>/tower_config.yaml` | `mod_moba_towers.sql` — `creature_template`, models, `mod_moba_tower_data` |
| `gen_base.py` | `maps/<mode>/base_config.yaml` | `mod_moba_base.sql` — `mod_moba_base`, the spawn-dome `gameobject_template` (+ `_addon`), plus the `game_graveyard` / `battleground_template` wiring (spawn locations and `MinPlayersPerTeam`) |
| `gen_store.py` | `maps/<mode>/store_config.yaml` + `data/sql/base/db_world/item_template.sql` | `mod_moba_store.sql` — shopkeeper `creature_template`, models and `creature` spawn rows, plus `mod_moba_store_npc`/`_menu`/`_grant`/`_sell` (and `_itemstage` when `custom_items` is on). **Also writes `client/addons/MobaHUD/Catalog.lua`** — the only generator emitting outside `data/sql/` |
| `gen_player_drops.py` | `maps/<mode>/player_config.yaml` | `mod_moba_player_drops.sql` — the `Map`-keyed `mod_moba_player_drops` table, granted directly to the killer (no native loot) |
| `id_alloc.py` | `id_blocks.json` + every `*.lock.json` and hand-assigned config field | nothing — it is the ID registry the others allocate through; `--audit` prints and checks the whole picture |

Pipeline constants (output paths) live in the generators, not the configs — each
generator globs `maps/*/<name>_config.yaml`, so the per-map YAML holds only that
map's content. ID blocks are the one genuinely shared thing, and they live in
`id_blocks.json` (see below). `gen_tower_data.py` owns the tower creatures
outright: there is no hand-written defs file.

## Config field reference

Each config documents its own fields, as an opening legend (`creep_config.yaml`,
`neutral_config.yaml`, `lane_config.yaml`, `tower_config.yaml`, `store_config.yaml`) or as
per-field comments (`base_config.yaml`). That is deliberately the home rather than this
file: documentation read while editing the values stays honest, and documentation read a
directory away does not.

Two policies span those configs and so live here instead: the full-copy rule below, and
the `drops` schema after it.

### The full-copy + override policy

Creeps are **full copies of every `creature_template` column** from a real source
creature, with only a deliberate override list — not a hand-picked subset. The
reduced-list approach caused a real bug (`BaseAttackTime` defaulting to 0, which
broke melee swing timing) and would have caused two more (an `AIName`/SmartAI
conflict on caster sources, a leftover mount flag on vehicle-flavored sources).
Copying everything and overriding only what's needed removes that whole bug class.

`gen_creep_roster.py` enforces the override list in code so it can't be
forgotten: `AIName`/`ScriptName`, loot columns, `npcflag`, `VehicleId`,
difficulty-entry references and `IconName` cleared, `RegenHealth = 0` (damage
persists between fights, LoL-style), faction from team, and
`minlevel = maxlevel = level`.

Source creatures are named by `creature_template` entry id and read from the
committed base dump `data/sql/base/db_world/creature_template.sql`. The baseline
therefore tracks upstream: each generator pins the rows it used by digest in its
lockfile, so a merge that retunes a source creature fails the run until
`--rebless-sources`.

## `drops` — on-death rewards (minion + player configs)

Any creep or mob block may carry a `drops` list. Each entry is one reward with
an optional `chance` coefficient in (0, 1], default 1.0, rolled independently
per kill:

Personal — to the killing-blow player alone:

- `{type: buff, spell: id, duration_ms: 0}` — aura granted directly; `duration_ms` 0 =
  the spell's own duration.
- `{type: gold, copper: n}` — coins in the corpse loot window, so the last hitter walks
  up and collects them.
- `{type: item, item: id, count: n, sell: copper}` — native loot. `sell` is what one unit
  refunds at the shop, and without it the item cannot be sold back at all. The same item
  id twice on one mob fails the run (`creature_loot_template` keys on (Entry, Item)) —
  raise `count` instead.

Team-wide — to the killer's whole team, flat per player, with no corpse. The objective
payout a boss camp wants:

- `{type: team_gold, copper: n}` — paid to every player on that team.
- `{type: team_buff, spell: id, duration_ms: 0}` — the same delivery as an aura, and it
  reaches **living players only**; nothing re-grants it on respawn.

A boss may carry a team row *and* a plain `gold` row: the team gets its flat share and
the last hitter still finds corpse gold.

Everything except `item` lands in `mod_moba_*_drops` and is rolled in C++ at the killing
blow. `item` rows land in native `creature_loot_template` (the one shared native table
the generators touch — deleted by entry, never dropped), plus a pricing row when they
carry `sell`. Loot-bearing mobs also get `lootid = entry`, zeroed `mingold`/`maxgold`,
and the `NO_PLAYER_DAMAGE_REQ` `flags_extra` bit; rationale in the generator's docstring.

Player kill drops (`player_config.yaml`) reuse this exact schema but skip native
loot entirely — `item` is a rolled `AddItem` grant too, landing in
`mod_moba_player_drops` alongside buff/gold instead of `creature_loot_template`.
Delivered by `GrantPlayerKillDrops` at the resolved kill.

## ID allocation — `id_blocks.json`

Every custom ID this fork assigns is owned by exactly one generator, and every
owner's claim is a **block** declared in `apps/moba/id_blocks.json`. Blocks and
lockfiles answer different questions:

| | Lives in | Answers |
|---|---|---|
| **Block** | `id_blocks.json` — one file, global | which IDs may this generator use? |
| **Assignment** | `<config>.lock.json` — one per map bundle | which ID did this key get? |

Blocks are global because every bundle's creeps draw from one range; assignments
are per-bundle because two maps may both have an `alliance_tower`.

`gen_all.sh` ends with `python3 apps/moba/id_alloc.py --audit`, which prints
every allocation and fails on a collision, a block trespass, an ID outside every
block, or two owners' blocks overlapping. Run it any time to see the live picture.

Namespaces are independent: `waypoint_data` 900206 and `creature_template` 900206
are unrelated IDs and both legal. The waypoint block 900100-900499 numerically
overlaps the neutral, store and dome blocks for exactly that reason.

**A full block grows itself.** When an owner's blocks fill, the allocator carves
another — 100 wide, aligned to a 100 boundary, above every block already declared
in that namespace — records it in `id_blocks.json`, and says so in the generator's
output. No two owners may share a block: each clears its own range on every
regen, so an overlap would let one generator wipe the other's rows. The allocator
re-runs the overlap check before saving and refuses to write a ledger that breaks
it.

**Generated SQL clears by block, not by roster.** A `DELETE ... WHERE entry IN
(<current roster>)` can never name an entry the config no longer has, so a mob
dropped from config used to leave its `creature_template` row live in
`acore_world` forever. Clearing the whole block needs no memory of what was
stranded.

## Lockfiles — machine-owned, committed, never hand-edited

`lane_config.lock.json` maps each lane/slot to its permanently assigned
forward/reverse `waypoint_data` IDs, plus a reserved `_centerline` pseudo-slot per
lane holding the speed-compensation reference path (`gen_creep_paths.py` fails a
config that claims that slot name). `creep_config.lock.json`,
`neutral_config.lock.json` and `tower_config.lock.json` do the same for
auto-assigned `creature_template` entries.

This is what makes re-runs safe: a locked ID is returned *before* any block check
runs, so re-walks, offset tuning, spacing changes — and even moving an owner's
block — all regenerate the same IDs, and
`mod_moba_creep_data.WaypointPathId` references never silently break.

**Deleting a lockfile makes the next run assign fresh IDs to everything**, which
orphans every reference already in the DB. Don't.

## Reusing `gen_creep_paths.py` elsewhere

Copy an existing `lane_config.yaml` into a new map bundle as
`maps/<mode>/lane_config.yaml`, then replace the lanes with real ones. Output paths and
scanned-SQL directories are generator constants, not config fields. The lockfile is
created next to the config on first run.