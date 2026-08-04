# MOBA content generators

Generators and per-map config for the battleground's data-driven content. Each
map/mode is a self-contained bundle under `apps/moba/maps/<mode>/` (e.g.
`maps/eye_of_the_storm/`) holding that map's `*_config.yaml`; the generators glob
those and emit one combined SQL file per content type into
`data/sql/custom/db_world/` (auto-applied on worldserver boot), each carrying a
"GENERATED — do not hand-edit" header.

> **Requires PyYAML** (configs are YAML): `pip3 install pyyaml`.

> **Run `apps/moba/gen_all.sh`** to regenerate everything. It cds to the repo
> root itself (generators resolve their output paths relative to it) and runs
> `gen_creep_paths.py` before `gen_creep_roster.py` — the roster reads each
> creep's `WaypointPathId` out of the lane lockfile, and it is the only ordering
> constraint between generators. Each script stays runnable alone.

**Workflows live in `.github/MOBA_GUIDE.md`** — walking a lane, adding a creep,
moving a tower. This file is the field-level reference those recipes point at:
what each config key means, and the lockfile rules.

| Generator | Reads | Writes |
|---|---|---|
| `gen_creep_roster.py` | `maps/<mode>/creep_config.yaml` + source dumps in `sources/` | `mod_moba_creeps.sql` — `creature_template`, models, equipment, `mod_moba_creep_data`, `mod_moba_creep_drops`, native `creature_loot_template` rows |
| `gen_neutral_camps.py` | `maps/<mode>/neutral_config.yaml` + source dumps in `sources/` | `mod_moba_neutrals.sql` — `creature_template`, models, camp/member/behavior/drops tables, native `creature_loot_template` rows |
| `gen_creep_paths.py` | `maps/<mode>/lane_config.yaml` | `mod_moba_creep_paths.sql` — densified, formation-offset `waypoint_data` |
| `gen_tower_data.py` | `maps/<mode>/tower_config.yaml` | `mod_moba_towers.sql` — `creature_template`, models, `mod_moba_tower_data` |
| `gen_base.py` | `maps/<mode>/base_config.yaml` | `mod_moba_base.sql` — `mod_moba_base`, the spawn-dome `gameobject_template` (+ `_addon`), plus the `game_graveyard` / `battleground_template` spawn wiring |
| `gen_store.py` | `maps/<mode>/store_config.yaml` + `data/sql/base/db_world/item_template.sql` | `mod_moba_store.sql` — shopkeeper `creature_template`, models and `creature` spawn rows, plus `mod_moba_store_npc`/`_menu`/`_grant`/`_sell` (and `_itemstage` when `custom_items` is on). **Also writes `client/addons/MobaHUD/Catalog.lua`** — the only generator emitting outside `data/sql/` |
| `gen_player_drops.py` | `maps/<mode>/player_config.yaml` | `mod_moba_player_drops.sql` — the `Map`-keyed `mod_moba_player_drops` table, granted directly to the killer (no native loot) |
| `id_alloc.py` | `id_blocks.json` + every `*.lock.json` and hand-assigned config field | nothing — it is the ID registry the others allocate through; `--audit` prints and checks the whole picture |

Pipeline constants (output paths) live in the generators, not the configs — each
generator globs `maps/*/<name>_config.yaml`, so the per-map YAML holds only that
map's content. ID blocks are the one genuinely shared thing, and they live in
`id_blocks.json` (see below). `gen_tower_data.py` owns the tower creatures
outright: there is no hand-written defs file.

## `lane_config.yaml` — human-owned, edit freely

- `max_spacing` — max yards between generated nodes. **Keep at ~5.** Sparser
  nodes break creep re-aggro: the engine's leash checks anchor to
  waypoint-generator positions, and sparse nodes leave those anchors far from the
  creature (see the guide's gotcha index).
- `slots` — the default formation, one entry per creep path per team.
  - `lateral_offset` — yards sideways from the centerline; **positive = the
    walking creep's own left**. The value mirrors automatically for the other team
    (they walk the other way), so one number describes both directions.
  - `longitudinal_offset` — yards along travel; positive = ahead. Creeps spawn at
    their path's first node, so this also staggers the formation at spawn.
- `lanes` — one per lane: `name`, `points` (walked centerline as `[x, y, z]`
  triples, in walk order), and optionally its own `slots` overriding the default
  formation for that lane only.

Direction naming: `forward` = the direction the points were walked (the team
spawning at the first point uses it); `reverse` is generated for the other team.
Only walk each lane once.

## `creep_config.yaml` — human-owned

All creeps in a file inherit its top-level `map` and spawn only on that map.

`units` define what a creep **is**: name, team, role, source dump, display,
level, modifiers, equipment, drops, and the caster fields (`attack_range` /
`attack_interval_ms` / `attack_spell_id`). `creeps` rows **place** one — a `key`,
the `unit` to use, and a `lane`/`slot` that already exists in the lane lockfile.
Team 0 walks the slot's `forward` path, team 1 `reverse`.

A row may override any field its unit sets. Overrides are wholesale per field,
never merged: a row restating `drops` replaces the list rather than adding to it,
and `drops: []` means none. A row may also omit `unit` and spell out every field
itself. Units may not set `key`, `lane`, or `slot` — those place a creep, and two
creeps resolving to the same team's lane/slot would share one waypoint path and
spawn on top of each other (the generator rejects that).

**One row = one unit per wave.** Wave size is the config's business, not the
C++'s: add a row and a slot to field more of something. Roles differ only in when
they spawn — melee and casters every wave, siege every third, super while the
enemy inhibitor is down.

Optional on either a unit or a row: `rank` (overrides the source's; siege ships
`1` = elite), `creature_type` (enum `CreatureType`), and `speed_walk` /
`speed_run` (multipliers on the source's).

## `neutral_config.yaml` — human-owned

Camps own placement, `respawn_ms`, and `aggro_range` / `leash_range`; mobs own
stats, display, and `drops`. The ranges are denormalized per creature
entry at generation time (proximity aggro is `creature_template.detection_range`
— one value per entry), so a mob key placed in camps that disagree on ranges
fails the run: give each camp its own keys. Mobs placed in no camp are skipped.

Mobs use the same `units` mechanism as creeps: `units` define what a mob **is**,
a `mobs` row names one with `unit:` and may override any field it sets, wholesale
per field. A unit may not set `key` — placement lives on the camp's members, out
of a unit's reach, so `key` is the only field that names one individual. `rank`
and `creature_type` are optional on a unit or a mob and override the source's,
same as on creeps — the jungle sources are Humanoid, so the units set
`creature_type: 1` to keep camps Beasts.

Entry lockfile rules are identical to the creep lockfile above.

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

Source dumps live in `apps/moba/sources/` as committed, immutable reference data.

## `drops` — on-death rewards (minion + player configs)

Any creep or mob block may carry a `drops` list. Each entry is one reward with
an optional `chance` coefficient in (0, 1], default 1.0, rolled independently
per kill:

- `{type: buff, spell: id, duration_ms: 0}` — aura granted directly
  to the killing-blow player; `duration_ms` 0 = the spell's own duration.
- `{type: gold, copper: n}` — coins in the corpse loot window.
- `{type: item, item: id, count: n}` — native loot. The same item id
  twice on one mob fails the run (`creature_loot_template` keys on
  (Entry, Item)) — raise `count` instead.

buff/gold rows land in `mod_moba_*_drops` and are rolled in C++ at the killing
blow; item rows land in native `creature_loot_template` (the one shared native
table the generators touch — deleted by entry, never dropped). Loot-bearing
mobs also get `lootid = entry`, zeroed `mingold`/`maxgold`, and the
`NO_PLAYER_DAMAGE_REQ` `flags_extra` bit; rationale in the generator's
docstring and drops section.

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
forward/reverse `waypoint_data` IDs. `creep_config.lock.json`,
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
`maps/<mode>/lane_config.yaml`
replace the lanes with real ones. Output path and scanned-SQL dirs are generator
constants now, not config fields. The lockfile is created next to the config on
first run.