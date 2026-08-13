# WMO map toolchain

Everything needed to turn a Blender blockout into terrain a 3.3.5a client and
our extractors both accept. Map-agnostic: the procedure, the tools, the traps,
and the machine setup.

**Twisted Treeline's own content decisions are not here** — art direction,
texture choices, measured structure positions and the blockout's editing
constraints live in `.github/MOBA_MAP_WMO_PLAN.md` while that route is in
progress, and in `.github/MOBA_GUIDE.md` once it ships.

Everything in this directory is read-only against the client. Nothing writes to
the game install or the databases.

## Making a map, end to end

| # | Step | Proven |
|---|---|---|
| 1 | Blockout in Blender 5.1 | yes |
| 2 | Transfer to a 3.4 staging scene | yes |
| 3 | Set up the staging scene for WBS | yes |
| 4 | Export to `.wmo` | yes |
| 5 | Verify offline | yes |
| 6 | WDT + DBC rows | yes |
| 7 | MPQ patch, then run the extractors | **no** |

Steps 1–6 below are a procedure that has been run start to finish. Step 7 has
not; what is known about it is in the plan file, not written here as recipe.

### 1. Blockout — Blender 5.1

1 Blender unit = 1 WoW yard, so build at true scale. The 5.1 file is the source
of truth and never moves to 3.4 (see *Two version traps*).

Before transferring, the geometry needs:

- **UVs on every mesh.** The exporter raises on a group without a UV layer
  named exactly `UVMap`. Cube projection in world space, tiling — not a packed
  0–1 atlas, since these reference tiling terrain textures.
- **No empty material slots.** A slot with no material assigned is an export
  hazard.
- **Applied scale and rotation.** Unapplied transforms survive OBJ as baked
  geometry, so this matters less than it looks, but it keeps the two files
  comparable.
- **A decision, per object, on collide vs render-only.** This is the single
  most consequential authoring choice — see *Why collision is what goes wrong*.
  Render-only suits thin decorative pads sitting just above the floor, where
  collision would stack near-coincident surfaces, and overhead canopy.

Ngons are fine and need no pre-triangulation; the WBS batcher consumes Blender
loop triangles. Keep ngons planar.

### 2. Transfer to a 3.4 staging scene

Export the shipping objects as OBJ from 5.1, import into an empty 3.4 file.

**Set Forward = Y, Up = Z on both sides.** The exporter and importer have
*different* defaults, and the importer's (−Z forward, Y up) silently rotates
everything −90° about X. Nothing downstream complains; the map is simply
sideways.

Check it immediately, against bounds you know:

```python
import bpy
from mathutils import Vector
o = bpy.data.objects["<a large object>"]
ws = [o.matrix_world @ Vector(c) for c in o.bound_box]
print("x %.1f..%.1f  y %.1f..%.1f  z %.1f..%.1f"
      % (min(v.x for v in ws), max(v.x for v in ws),
         min(v.y for v in ws), max(v.y for v in ws),
         min(v.z for v in ws), max(v.z for v in ws)))
```

A wall reading its height in Y instead of Z is the axis bug. Undo and redo the
import rather than rotating after the fact.

**Custom properties do not survive OBJ.** Any per-object metadata — including
which objects collide — has to be re-established on the 3.4 side, which is what
`blender_staging_setup.py` exists for.

### 3. Set up the staging scene

Run `blender_staging_setup.py` in the Text Editor. It builds the WBS structure:
root collection, `Outdoor`, collision vertex groups on the colliding subset,
materials wired to texture paths, the root's `wmo_id`, and a test doodad — and
turns the scene into the server frame, for which see *Where the map lands*.

**The staging `.blend` holds nothing you cannot regenerate** — that is the point
of the script, and it is verified (see below). Treat it as disposable and keep
the source `.blend` backed up instead.

**That stops being true the moment you place a doodad**, so it is a claim with
an expiry date rather than a property of the pipeline. A placement is
hand-authored information — model path, position, rotation, scale — and it has
nowhere else to live. It cannot sit in the source `.blend`, because
`wow_wmo_doodad` is a WBS property and WBS only runs in 3.4. It cannot travel
through the OBJ, which carries geometry and no custom properties (the same
reason a per-object collide flag has to be re-established on the 3.4 side).
From the first real doodad the staging file holds original data, not derived,
and losing it means redoing the dressing pass by hand.

Settle this before authoring at scale, not after. The shape that fits this
project is a `doodad_config.yaml` plus a generator that builds the placements
in the 3.4 scene, the way `tower_config.yaml` feeds `gen_tower_data.py` — which
makes the `.blend` derived again and turns placements into diffable text. The
open question is direction: placements want to be *authored* by dragging things
around the viewport, not typed as coordinates, so the tool probably needs a
"dump the current placements to YAML" pass as much as the YAML-to-scene one.

One caveat that outlives the script: **the setup exists only in RAM until you
save.** A whole session's work was once found missing from the file on disk
while still live in the open Blender instance. To tell the two apart, open the
`.blend` from 5.1 and count vertex groups — those are real mesh data and load
without the addon registered, while `wow_wmo_*` properties do not. Zero vertex
groups means the collision pass is not in the file, whatever the running
session shows.

### 4. Export

Run `blender_preflight.py` first, every time. Then `File > Export > WMO`,
method **Full**, "Export selected objects" **off**.

Name the file what it will be called in the MPQ. The extractor derives group
filenames by chopping `.wmo` and appending `_000`, `_001`…, and the WDT
references this exact path.

Output is one root `.wmo` plus one file per group.

### 5. Verify

```bash
python3.10 wmo_verify.py path/to/Root.wmo
```

Do this before packing anything. It replays what `vmap4extractor` will decide,
so a dead map is caught here instead of after a pack-and-extract round trip.

The number that decides whether the map is playable is **`triangles kept as
COLLISION`**. Zero means terrain that looks perfect and cannot be stood on.

### 6. WDT + DBC rows

```bash
python3.10 gen_wdt.py --dump
python3.10 dbc_tool.py patch
```

`gen_wdt.py` writes `World/Maps/<Dir>/<Dir>.wdt`: MVER **18** — the WMO's own
MVER is 17, they are different formats — MPHD flags `0x1` for the global map
object, an all-zero 64x64 MAIN claiming no ADT tile, MWMO holding the one WMO
path, and a single 64-byte MODF at position `(0,0,0)`. Verified byte-for-byte
against `StormwindPrison`, `DeeprunTram` and `AlliancePVPBarracks`.

**MPHD and MAIN must sit immediately after MVER, in that order.** `map_extractor`
does not scan for them: `WDT_file::prepareLoadedData` computes
`mphd = version + version->size + 8` and `main = mphd + mphd->size + 8`, so a
chunk inserted between them is read as garbage.

`dbc_tool.py` adds the client's rows by **cloning** a comparable stock row and
overriding named fields. Cloning is the point — every field whose meaning has
not been established (locale masks, loading screen, corpse coordinates,
ambience) keeps a value already known to work on a live map. It never decodes a
record, so a column with no known type keeps its bytes, and string columns keep
valid offsets because the string block is only ever appended to.

The client cannot load a map without its `Map.dbc` row, and **`vmap4extractor`
enumerates maps from `Map.dbc` to find the WDT** — so the row has to be inside
the MPQ before extraction can see the map at all.

The server reads none of these files. Its rows go in the `*_dbc` world tables,
which ship schema-only and exist for exactly this: `DBCDatabaseLoader` merges
them additively over the extracted file. One caveat, and it is a boot-time
abort rather than a warning — the loader runs `SELECT *` and asserts the result's
column count equals the DBC format string's length, so adding or dropping a
column in one of those tables kills the worldserver at startup.

`WorldSafeLocs.dbc` is **not** part of this. AzerothCore has no
`sWorldSafeLocsStore` at all; graveyards come from the `game_graveyard` world
table.

### 7. Pack and extract

MPQ packing, then `mapextractor` / `vmap4extractor` + `vmap4assembler` /
`mmaps_generator`.

**Not yet done, so not written here.** `.github/MOBA_MAP_WMO_PLAN.md` holds what
is known and what is still unverified. Move it here once it has actually run.

## Where the map lands

A global WMO's model coordinates are not server coordinates. Every link in the
chain lives in a different file, which is why this is worth stating once:

1. WBS writes Blender coordinates into MOVT verbatim — no axis conversion.
2. `MapObject::Extract` rewrites a MODF position of `(0,0,0)` to the grid centre
   (`wmo.cpp:565`), so the spawn's `iPos` becomes `(17066.67, 17066.67, 0)`.
3. Queries reach model space as `pModel = iInvRot * (p - iPos)`
   (`ModelInstance.cpp:54`), with `p = (mid - X, mid - Y, Z)`.
4. Nothing transforms the vertices in between — the extractor writes MOVT raw,
   and the assembler transforms only M2s.

With MODF rotation `(0,0,0)`, which is what every stock global WMO uses:

    server (X, Y, Z) = (-model_x, -model_y, model_z)

So a model authored around the origin arrives **turned 180 degrees about Z**.
That is a proper rotation, not a mirror: chirality is preserved and no geometry
is wrong, but the sign of every coordinate measured in Blender is.

`blender_staging_setup.py` cancels it by turning the staging scene 180 degrees
about Z, which makes server coordinates equal the source `.blend`'s own — so
layout JSON, reference-image registration and measured anchors carry over
unchanged. The turn is guarded by a geometric probe on a known-asymmetric object
rather than a flag, so it survives a rebuild from OBJ and cannot apply twice.

Two consequences of the same chain:

- **MOHD's bounding box is `fixCoords()` of MODF's**: `MOHD = (f.z, f.x, f.y)`
  (`wmo.h:68`). `gen_wdt.py` reads the box out of the exported root and undoes
  the permutation rather than having it typed in, because a MODF bound smaller
  than the geometry silently kills collision outside it —
  `ModelInstance::GetLocationInfo` gates on `iBound.contains`.
- **MOBN/MOBR change whenever coordinates change**, so moving the scene shifts
  group file sizes by a few bytes each. Harmless: those are the client's own
  collision BSP, the extractor never reads them (`wmo.cpp:223-258`), server
  collision comes from the per-triangle MOPY flags, and vmap builds its own BIH.

## Why collision is what goes wrong

Collision defaults to **off**, and nothing warns you.

WBS's batcher flags a triangle `F_DETAIL` unless all three of its vertices are
in a vertex group named `"Collision"` — and `_is_vertex_collidable` returns
false outright when the group is absent. Our extractor then drops it:
`isRenderFace = RENDER && !DETAIL`, and a face that is neither render nor
collision is skipped (`wmo.cpp:404-408`).

Export without that vertex group and the map has **zero** collision. It looks
correct in the client, players fall through the floor, and mmaps builds nothing
walkable.

Collision is authored one of two ways:

- a `"Collision"` vertex group covering every vertex, named in
  `wow_wmo_vertex_info.vertex_group` — what `blender_staging_setup.py` does
- a separate invisible mesh in WBS's `Collision` collection, referenced by the
  group's `collision_mesh` pointer, which writes `F_COLLISION` + material id
  `0xFF` instead

Groups are the culling unit **and** the collision unit. `ShouldSkip` silently
drops any group flagged unreachable (`0x80`) or antiportal (`0x4000000`), or
literally named `antiportal`, taking its collision with it.

For reference, mmaps' `walkableClimb: 6` cells is about 1.6 yd
(`mmaps-config.yaml:50`), so a 1 yd plateau is climbable as collision.
`skipBattlegrounds` defaults to false.

## Four ways the export fails that the file won't show you

Each of these was hit for real. `blender_preflight.py` checks all four.

- **A material with no `diff_texture_1` raises** `ReferenceError` in
  `save_materials`. Loading a BLP into the scene does *not* assign it — that is
  a second, separate step, and skipping it is invisible until export. The path
  that ships comes from `image.wow_wmo_texture.path`, **not** the image's
  filepath (`wmo_scene.py:539`), so an image with a broken or absent file on
  disk still exports correctly. Store paths lowercase: the BLP→PNG import step
  does a `.replace('.blp', ...)` that misses an uppercase `.BLP`.
- **The root collection is resolved from `bpy.context.collection`**, i.e.
  whatever is selected in the Outliner. Select the Scene Collection and
  `get_current_wow_model_collection` returns `None`, and `save_root_header`
  dies on it.
- **`build_references` skips hidden objects** (`group_object.hide_get()`). A
  hidden group silently does not ship, taking its collision with it.
- **Every group mesh needs a UV layer named exactly `UVMap`**, or
  `create_batching_parameters` raises.

Two more, specific to doodads: a doodad needs `wow_wmo_doodad.enabled = True`
or WBS's depsgraph handler evicts it from the set collection, and setting its
`color` raises — `update_doodad_color` indexes
`mat.node_tree.nodes['DoodadColor']` and `KeyError`s on a plain material.

**Group membership is collection membership and nothing else.** There is no
`enabled` property on `wow_wmo_group` in this WBS revision;
`get_wmo_groups_list` reads the `Outdoor`/`Indoor` collections directly.

**No BLP export is needed** when dressing from stock assets. MOTX stores paths
only, so the client patch carries geometry and DBCs but no textures.

## Scripts

| Script | Where it runs | Proven |
|---|---|---|
| `blender_staging_setup.py` | Blender 3.4 Text Editor | yes, 2026-08-13 |
| `blender_preflight.py` | Blender 3.4 Text Editor | yes, 2026-08-13 |
| `wmo_verify.py` | `python3.10`, standalone | yes, 2026-08-13 |
| `gen_wdt.py` | `python3.10`, standalone | yes, 2026-08-13 |
| `dbc_tool.py` | `python3.10`, standalone | yes, 2026-08-13 |
| `mpq_tool.py` | `python3.10`, standalone | yes, 2026-08-12 |

All four map-specific scripts — the two Blender ones plus `gen_wdt.py` and
`dbc_tool.py` — hardcode Twisted Treeline's names, ids and paths in a constants
block at the top. **That block is the per-map part**; a second map edits it and
leaves the rest alone. If a third map turns up, that is the point to move the
block into a YAML config the way the SQL generators do; two maps do not justify
it yet.

### `blender_staging_setup.py` — build a staging scene from a bare OBJ import

Produces a WBS-ready scene: root collection, `Outdoor`, collision vertex groups
on the colliding subset, materials wired to texture paths, and the test doodad.

**Proven to reproduce the staging scene exactly.** Rebuilt from an OBJ into an
empty 3.4 file on 2026-08-12: all 47 group files came out the same size as the
hand-built original, 17 byte-identical, and every triangle count, collision
count, material path and doodad matched. This is what makes the staging
`.blend` disposable, subject to the doodad caveat in step 3 — re-run the proof
if the script changes.

Group and material *ordering* differs, because Blender sorts collection
contents case-insensitively and the script links them in ASCII order. That
moves group-file indices and material ids; it changes nothing the extractor
reads by name.

The images it creates are blank 1x1 placeholders carrying the right WoW path —
all the export needs, since MOTX ships the path string and not pixels, but the
viewport stays untextured.

### `blender_preflight.py` — check an existing staging scene, and add the doodad

Additive and idempotent. Verifies every precondition the WBS exporter enforces,
repairs only what is missing, and prints each change it made under `changed:`.
Run it before every export.

### `wmo_verify.py` — offline check of an exported WMO

```
python3.10 wmo_verify.py path/to/Root.wmo
```

Parses the root and every group file and replays the extractor's own decisions:
`ShouldSkip`, the per-triangle collision test, texture paths resolved against
the real MPQs, doodad models probed for bounding triangles, and the
`MODR -> References -> Doodad::ExtractSet` chain. Exits non-zero on any `FAIL`.

Validated against stock Blizzard WMOs (`silo`, `RuinedHumanGuardTower01`,
`Chapel`) before it was ever pointed at ours, which is worth repeating if the
parser is ever changed — `mpq_tool.py extract` is how to get them.

### `gen_wdt.py` — build the WMO-only WDT

```
python3.10 gen_wdt.py [--dump]
```

Reads the exported root's MOHD for its bounding box and rootWMOID, refuses to
run if the rootWMOID is wrong or a group file is missing, and prints where the
map lands in server coordinates plus how many tiles mmaps will build.

### `dbc_tool.py` — add this fork's rows to the client DBCs

```
python3.10 dbc_tool.py dump Map.dbc 566     one row, field by field
python3.10 dbc_tool.py patch                write the patched DBCs to the staging dir
```

Idempotent: re-running replaces the row in place instead of appending a second
copy.

### `mpq_tool.py` — query the client archives

```
python3.10 mpq_tool.py index listfile.txt     every entry (~184k)
python3.10 mpq_tool.py find ghostlands .m2    entries matching all substrings
python3.10 mpq_tool.py probe 'World\...\X.m2' collision header of a model
python3.10 mpq_tool.py extract 'World\...\Y.wmo' outdir/
```

The MPQs carry internal listfiles, so asset paths are enumerable rather than
guesswork. A `_s` suffix on a texture is a specular map — never assign one as
diffuse.

`probe` answers the one question that decides whether a doodad is decoration or
a wall: a model with **0 bounding triangles** renders and never collides, and
one with more than 0 always collides. There is no third option, which is why
"walkable but vision-blocking" brush cannot be a doodad at all.

## Toolchain setup

### Two version traps

**WBS targets Blender 3.4** (`bl_info (3, 4, 0)`, releases tagged `3.4-*`).
Install it as `/Applications/Blender-3.4.app` — do not let it replace a newer
`Blender.app`.

**A .blend cannot go backward.** Blender is not forward compatible, so a file
written by 5.x will not open in 3.4. This is why the pipeline transfers through
OBJ rather than just opening the blockout in 3.4, and why the 5.1 file stays
the source of truth.

### Building WBS on Apple Silicon

Prebuilt releases are **Windows-only** — CI runs
`C:\Python310\python.exe io_scene_wmo/build.py` with no macOS job. On Apple
Silicon `wbs_kernel` must be compiled locally: one Cython extension
(`wmo_utils`) over 6 C++17 files, with GLM and Blender headers vendored and an
explicit Darwin branch in `setup.py`.

Use Homebrew `python@3.10` to match Blender 3.4's embedded 3.10.8; the ABI is
stable across 3.10.x. Clone with `--recurse-submodules` — `pywowlib` is a
submodule.

Every failure was 2022-era code meeting a modern toolchain; none was Apple
Silicon or addon logic, and `wbs_kernel` compiled first try. **Re-apply all five
on a fresh clone:**

1. `pip install "Cython<3"` — Cython 3 crashes compiling pyimgui's `core.pyx`.
2. Build with `PIP_NO_BUILD_ISOLATION=1`, or pip fetches its own Cython 3 into
   an isolated env and (1) is silently ignored.
3. `blp/{BLP2PNG,PNG2BLP}/setup.py`: `extra_compile_args` applies `-std=c++17`
   to the bundled libpng/zlib **C** sources; clang errors. Add a `build_ext`
   subclass stripping `-std=*` for `*.c` only. Deleting the flag outright is
   wrong — Apple clang defaults to C++14.
4. Same two files: add `-DZ_HAVE_UNISTD_H` to the POSIX branches only (MSVC has
   no `<unistd.h>`). Without it zlib never includes it, `lseek` is implicitly
   declared, and clang 16+ errors. Do **not** silence with
   `-Wno-implicit-function-declaration`: an implicit `lseek` returns `int` and
   truncates 64-bit file offsets.
5. Delete `|| defined(TARGET_OS_MAC)` from `blp/include/libpng/pngpriv.h:512`
   and `blp/include/zlib/zutil.h:133`. Modern macOS defines it on every Apple
   platform; these guards predate that and mean Classic Mac OS — pulling in
   `<fp.h>` (gone ~20 years) and `#define fdopen(fd,mode) NULL` over the SDK's
   real declaration. StormLib's copy of `zutil.h` has the same latent bug but
   compiles clean; leave it.

### What cannot be automated

- **Launch Blender 3.4 from a terminal.** `print()` goes to stdout and macOS
  has no system console, so the launching terminal is the only place script
  output and exporter tracebacks appear.
- **WBS crashes Blender 3.4 under `--background`** (dies in `auto_load` at
  `ui/operators.py:557`). Headless runs need `--factory-startup`, which
  disables the addon — so headless is limited to geometry checks.
- Launching the binary directly rather than via `open` starts a second
  instance, which is how to test a rebuild against a known-good session.
- Inspecting a 3.4 `.blend` from a newer Blender works and needs no addon, but
  only reads real data — meshes, vertex groups, collections. WBS's
  `wow_wmo_*` property groups will look empty.

## Requirements

`python3.10` specifically for the standalone tools — they import pywowlib's
StormLib binding, compiled against Blender 3.4's interpreter
(`storm.cpython-310-darwin.so`). Any other python fails the import.

Paths default to `~/Games/wow335/Data` and `~/tools/blender-wow-studio`;
override with `WOW_DATA` and `WBS_ROOT`.
