# MOBA content generators

Generators and per-map config for the battleground's data-driven content. Each
map/mode is a self-contained bundle under `apps/moba/maps/<mode>/` (e.g.
`maps/eye_of_the_storm/`) holding that map's `*_config.json`; the generators glob
those and emit one combined SQL file per content type into
`data/sql/custom/db_world/` (auto-applied on worldserver boot), each carrying a
"GENERATED — do not hand-edit" header.

**Workflows live in `.github/MOBA_GUIDE.md`** — walking a lane, adding a creep,
moving a tower. This file is the field-level reference those recipes point at:
what each config key means, and the lockfile rules.

| Generator | Reads | Writes |
|---|---|---|
| `gen_creep_roster.py` | `maps/<mode>/creep_config.json` + source dumps in `sources/` | `mod_moba_creeps.sql` — `creature_template`, models, equipment, `mod_moba_creep_data` |
| `gen_neutral_camps.py` | `maps/<mode>/neutral_config.json` + source dumps in `sources/` | `mod_moba_neutrals.sql` — `creature_template`, models, camp/member/behavior tables |
| `gen_creep_paths.py` | `maps/<mode>/lane_config.json` | `mod_moba_creep_paths.sql` — densified, formation-offset `waypoint_data` |
| `gen_tower_data.py` | `maps/<mode>/tower_config.json` | `mod_moba_towers.sql` — `mod_moba_tower_data` |
| `gen_base.py` | `maps/<mode>/base_config.json` | `mod_moba_base.sql` — `mod_moba_base`, plus the `game_graveyard` / `battleground_template` spawn wiring |

Pipeline constants (output paths, id ranges, scanned SQL dirs) live in the
generators, not the configs — each generator globs `maps/*/<name>_config.json`, so
the per-map JSON holds only that map's content. Tower *creatures* are the exception
to the generated rule: shared and hand-written in
`data/sql/custom/db_world/mod_moba_tower_defs.sql`.

## `lane_config.json` — human-owned, edit freely

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

## `creep_config.json` — human-owned

All creeps in a file inherit its top-level `map` and spawn only on that map. Each
creep block names its `lane`/`slot` (both must already exist in the lane
lockfile), its role, and its tuning — `attack_range` / `attack_interval_ms` /
`attack_spell_id` for casters, plus modifiers, level, equipment, and display.
Team 0 uses the slot's `forward` path, team 1 `reverse`. An optional per-creep
`rank` overrides the source creature's (siege ships with 1 = elite).

## `neutral_config.json` — human-owned

Camps own placement, `respawn_ms`, and `aggro_range` / `leash_range`; mobs own
stats, display, and `kill_buff_spell`. The ranges are denormalized per creature
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
`minlevel = maxlevel = level`. It also warns when a map's wave composition
deviates from what `BattlegroundMOBA` expects — exactly 2 melee + 1 caster per
team, siege optional.

Source dumps live in `apps/moba/sources/` as committed, immutable reference data.

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

Copy `lane_config.example.json` into a map bundle as `maps/<mode>/lane_config.json`,
adjust `id_range` / `max_spacing` / `slots`, and fill in real lanes. Output path and
scanned-SQL dirs are generator constants now, not config fields. The lockfile is
created next to the config on first run.
