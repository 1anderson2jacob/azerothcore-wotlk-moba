# MOBA content generators

Generators + per-map config for the MOBA battleground's data-driven content.
Each map/mode is a self-contained bundle under `apps/moba/maps/<mode>/` (e.g.
`maps/eye_of_the_storm/`) holding that map's `*_config.json` files; the
generators glob those and emit one combined SQL file per content type. Generated
SQL all carries a "GENERATED — do not hand-edit" header.

- `gen_creep_roster.py` — per-creep choices + source-creature stat dumps →
  `mod_moba_creeps.sql` (`creature_template`, models, equipment, and the
  `mod_moba_creep_data` table). Uses a per-map lockfile
  (`creep_config.lock.json`) to pin auto-assigned `creature_template` entries.
- `gen_creep_paths.py` — walked lane points → densified, formation-offset
  `waypoint_data` SQL (`mod_moba_creep_paths.sql`), from `lane_config.json`
  (still shared/single until a second map's lanes exist).
- `gen_tower_data.py` — per-map tower positions → `mod_moba_towers.sql`
  (`mod_moba_tower_data`). Tower *creatures* are shared and hand-written in
  `data/sql/custom/mod_moba_tower_defs.sql`; entries are hand-assigned, no lockfile.
- `gen_base.py` — per-map base tunables + spawn coords → `mod_moba_base.sql`
  (the `mod_moba_base` table — respawn timing, recall cast times — plus the
  `game_graveyard` / `battleground_template` spawn wiring). No lockfile.

Pipeline constants (output paths, id ranges, the `lane_config` path) live in the
generators, not the configs — the per-map JSON files hold only that map's content.

## Workflow: (re)defining a lane

1. In-game, walk the lane centerline and run `.gps` every ~15–20 yards
   (more often through curves). Copy the console scrollback into a text
   file.
2. Extract the points: `python3 apps/moba/gen_creep_paths.py --extract scrollback.txt`
   prints a JSON array ready to paste into the config's `points` field.
3. Paste it into the lane's `points` in `lane_config.json`.
4. From the repo root: `python3 apps/moba/gen_creep_paths.py`
5. Review the generated SQL (path in the config's `output`), apply it to
   `acore_world`, and fully restart worldserver.
6. If the run printed **newly assigned** path IDs (only happens for a new
   lane or new slot), wire them into the matching
   `mod_moba_creep_data.WaypointPathId` rows. Re-runs of existing
   lanes/slots reuse their IDs — no creature changes needed.

## Config fields (`lane_config.json` — human-owned, edit freely)

- `id_range` — pool for auto-assigned `waypoint_data` IDs (`[low, high]`).
- `max_spacing` — max yards between generated nodes. Keep at ~5; nodes
  much sparser than this break creep re-aggro (leash/home-position gotcha,
  see `.github/MOBA_GUIDE.md`).
- `output` — where the generated SQL is written (relative to where you run
  the script from — run from the repo root).
- `scan_sql_dirs` — directories scanned for already-used waypoint IDs so
  fresh allocation never collides with hand-written SQL.
- `slots` — the default formation, one entry per creep path per team.
  - `lateral_offset` — yards sideways from the centerline; **positive =
    the walking creep's own left**. The same value automatically mirrors
    to the opposite physical side for the other team (they walk the other
    way), so one number describes both directions.
  - `longitudinal_offset` — yards along the direction of travel; positive
    = ahead, negative = behind. Since creeps spawn at their path's first
    node, this is also what staggers the formation at spawn.
- `lanes` — one entry per lane: `name`, `points` (walked centerline as
  `[x, y, z]` triples, in walk order), and optionally its own `slots` to
  override the default formation for that lane only.

Direction naming: `forward` = the direction the points were walked (the
team spawning at the first point uses it); `reverse` is auto-generated for
the other team. Only walk each lane once.

## The lockfile (`lane_config.lock.json` — machine-owned, do NOT edit)

Maps each lane/slot to its permanently assigned forward/reverse
`waypoint_data` IDs. This is what makes re-runs safe: geometry changes
(re-walks, offset tuning, spacing changes) regenerate the same IDs, so
`mod_moba_creep_data.WaypointPathId` references never silently break.
Check it into git alongside the config. Deleting it makes the next run
assign fresh IDs to everything — which orphans every existing creature →
path reference. Don't.

## Reusing in another project

Copy `gen_creep_paths.py` + `lane_config.example.json`, rename the example
to `lane_config.json`, adjust `id_range`/`scan_sql_dirs`/`output` to the
project's conventions, and fill in real lanes. The lockfile is created on
first run.

## Workflow: re-tuning an existing creep (`gen_creep_roster.py`)

1. Edit its entry in `apps/moba/maps/<mode>/creep_config.json` (modifiers, level, spell,
   equipment, display, rank, ...).
2. From the repo root: `python3 apps/moba/gen_creep_roster.py`
3. Apply the SQL to `acore_world`, fully restart worldserver.

Ad-hoc `UPDATE`s against the DB are fine for live experimentation, but
record the final values in `creep_config.json` — the generated file is
the source of truth, and the next apply reverts anything not in it.

## Workflow: adding a new creep type

1. Dump the source creature whose stats you're basing it on:
   `mysql -E -u acore -pacore acore_world -e "SELECT * FROM creature_template WHERE entry=<id>" > apps/moba/sources/creature_template_<id>.txt`
   (`apps/moba/sources/` holds these verbatim dumps as committed,
   immutable reference data.)
2. Add a block to `apps/moba/maps/<mode>/creep_config.json`'s `creeps` list (copy a similar
   role's). `lane`/`slot` must exist in the lane lockfile — for a
   brand-new formation slot, add it to `lane_config.json` and run
   `gen_creep_paths.py` first. Team 0 uses the slot's `forward` path,
   team 1 `reverse`.
3. Run the generator — it auto-assigns and locks a `creature_template`
   entry, and warns if the wave composition deviates from what
   `BattlegroundMOBA` expects (exactly 2 melee + 1 caster per team,
   siege optional).
4. Apply the SQL, fully restart worldserver.

The generator enforces the override checklist from
`.github/MOBA_GUIDE.md` in code so it can't be forgotten:
`AIName`/`ScriptName`, loot columns, `npcflag`, `VehicleId`,
difficulty-entry references and `IconName` cleared, `RegenHealth = 0`
(damage persists, LoL-style), faction from team,
`minlevel = maxlevel = level`. An optional per-creep `"rank"` overrides
the source creature's rank (siege ships with 1 = elite).

`apps/moba/maps/<mode>/creep_config.lock.json` follows the same rules as the lane lockfile:
machine-owned, committed, never hand-edited — deleting it makes the next
run assign fresh entries and orphans everything already in the DB.
