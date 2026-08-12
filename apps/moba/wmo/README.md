# WMO map toolchain

Authoring tools for the custom-terrain-as-WMO route. Why the route exists, the
format facts, and the per-step results live in `.github/MOBA_MAP_WMO_PLAN.md`
(or, once the map ships, `MOBA_GUIDE.md`) — not here. This file is what to run
and in what order.

Everything here is read-only against the client. Nothing writes to the game
install or the databases.

## The loop

Editing the map means going around this once per change, so it is worth knowing
the whole cycle before starting:

```
5.1 blockout  --OBJ-->  3.4 staging  --WBS export-->  .wmo  -->  MPQ  -->  extractors
                            ^                           |
                            +------ wmo_verify.py ------+   (catches it here, offline)
```

`wmo_verify.py` is the short-circuit. It replays what `vmap4extractor` will
decide, so a dead export is caught before the pack-and-extract round trip.

## Scripts

| Script | Where it runs | Proven |
|---|---|---|
| `blender_staging_setup.py` | Blender 3.4 Text Editor | **no** — see below |
| `blender_preflight.py` | Blender 3.4 Text Editor | yes, 2026-08-12 |
| `wmo_verify.py` | `python3.10`, standalone | yes, 2026-08-12 |
| `mpq_tool.py` | `python3.10`, standalone | yes, 2026-08-12 |

Both Blender scripts hardcode the Twisted Treeline object names, materials and
texture paths in a constants block at the top. **That block is the per-map
part** — a second map edits it and leaves the rest alone. If a third map turns
up, that is the point to move the block into a YAML config the way the SQL
generators do; two maps do not justify it yet.

### `blender_staging_setup.py` — build a staging scene from a bare OBJ import

Takes a 3.4 file holding nothing but the imported geometry and produces a
WBS-ready scene: root collection, `Outdoor`, collision vertex groups on the
colliding subset, materials wired to texture paths, and the test doodad.

**This one has never been run end to end.** It was written for a staging file
that turned out to still hold its setup in memory, so `blender_preflight.py`
was used instead. Treat the first run as something to watch, not trust — and
note it will create blank 1x1 placeholder images rather than the real BLPs,
which is fine for export (only the path string ships) but leaves an untextured
viewport.

### `blender_preflight.py` — check an existing staging scene, and add the doodad

Additive and idempotent. Verifies every preconditon the WBS exporter enforces,
repairs only what is missing, and prints each change it made under `changed:`.
Run it before every export.

It catches the four things that otherwise fail at export time or, worse,
silently: an unassigned `diff_texture_1`, an active collection outside the WMO
root, a hidden group object, and a missing `UVMap` layer.

### `wmo_verify.py` — offline check of an exported WMO

```
python3.10 wmo_verify.py path/to/Root.wmo
```

Parses the root and every group file and replays the extractor's own decisions:
`ShouldSkip`, the per-triangle collision test, texture paths resolved against
the real MPQs, doodad models probed for bounding triangles, and the
`MODR -> References -> Doodad::ExtractSet` chain. Exits non-zero on any `FAIL`.

The number that matters is `triangles kept as COLLISION`. Zero means a map that
looks perfect and cannot be walked on.

Validated against stock Blizzard WMOs (`silo`, `RuinedHumanGuardTower01`,
`Chapel`) before it was ever pointed at ours, which is worth repeating if the
parser is ever changed.

### `mpq_tool.py` — query the client archives

```
python3.10 mpq_tool.py index listfile.txt     every entry (~184k)
python3.10 mpq_tool.py find ghostlands .m2    entries matching all substrings
python3.10 mpq_tool.py probe 'World\...\X.m2' collision header of a model
python3.10 mpq_tool.py extract 'World\...\Y.wmo' outdir/
```

`probe` answers the one question that decides whether a doodad is decoration or
a wall: a model with **0 bounding triangles** renders and never collides, and
one with more than 0 always collides. There is no third option, so it is also
how to confirm a brush asset will not become an invisible wall.

`extract` pulls a WMO's group files alongside the root, which is how to get a
reference model to compare against.

## Requirements

`python3.10` specifically — these import pywowlib's StormLib binding, which is
compiled against Blender 3.4's interpreter (`storm.cpython-310-darwin.so`).
Any other python will fail the import.

Paths default to `~/Games/wow335/Data` and `~/tools/blender-wow-studio`;
override with `WOW_DATA` and `WBS_ROOT`.
