# MOBA lane tooling

Generators for the MOBA battleground's data-driven content. Currently one
tool: `gen_creep_paths.py`, which turns a small set of walked lane points
into the full densified, formation-offset `waypoint_data` SQL that lane
creeps follow. (A creep-roster generator is planned next.)

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
