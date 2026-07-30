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
| `gen_tower_data.py` | `maps/<mode>/tower_config.yaml` | `mod_moba_towers.sql` — `mod_moba_tower_data` |
| `gen_base.py` | `maps/<mode>/base_config.yaml` | `mod_moba_base.sql` — `mod_moba_base`, plus the `game_graveyard` / `battleground_template` spawn wiring |
| `gen_player_drops.py` | `maps/<mode>/player_config.yaml` | `mod_moba_player_drops.sql` — the `Map`-keyed `mod_moba_player_drops` table, granted directly to the killer (no native loot) |

Pipeline constants (output paths, id ranges, scanned SQL dirs) live in the
generators, not the configs — each generator globs `maps/*/<name>_config.yaml`, so
the per-map YAML holds only that map's content. Tower *creatures* are the exception
to the generated rule: shared and hand-written in
`data/sql/custom/db_world/mod_moba_tower_defs.sql`.

## `lane_config.yaml` — human-owned, edit freely

- `id_range` — pool for auto-assigned `waypoint_data` IDs (`[low, high]`).
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

## Lockfiles — machine-owned, committed, never hand-edited

`lane_config.lock.json` maps each lane/slot to its permanently assigned
forward/reverse `waypoint_data` IDs. `creep_config.lock.json` does the same for
auto-assigned `creature_template` entries.

This is what makes re-runs safe: geometry changes — re-walks, offset tuning,
spacing changes — regenerate the same IDs, so `mod_moba_creep_data.WaypointPathId`
references never silently break.

**Deleting a lockfile makes the next run assign fresh IDs to everything**, which
orphans every reference already in the DB. Don't.

## Reusing `gen_creep_paths.py` elsewhere

Copy an existing `lane_config.yaml` into a new map bundle as
`maps/<mode>/lane_config.yaml`, adjust `id_range` / `max_spacing` / `slots`, and
replace the lanes with real ones. Output path and scanned-SQL dirs are generator
constants now, not config fields. The lockfile is created next to the config on
first run.