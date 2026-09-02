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

| # | Step |
|---|---|
| 0 | Trace the boundary from a reference image |
| 1 | Blockout in Blender 5.1 |
| 2 | Transfer to a 3.4 staging scene |
| 3 | Set up the staging scene for WBS |
| 4 | Export to `.wmo` |
| 5 | Verify offline |
| 6 | WDT + DBC rows |
| 7 | Pack the MPQ client patch |
| 8 | Run the extractors |
| 9 | Walk it in game |

Steps 0–1 are headless; 3 and 4 need a Blender 3.4 GUI session. The whole
sequence has been run start to finish, most recently for Twisted Treeline v2 on
2026-08-18 — step 0 is the newest of them but settled, a painted trace read by
a script.

Every check compares against **what the previous step reported** rather than a
literal, and this file deliberately carries no numbers for any map: they move
with the trace, the tessellation and the dressing. A stale one does not read as
stale, it reads as a real failure — a z range left here outlived the wall heights
that produced it and sent a session hunting geometry that had never moved.

### Set these once

Paste into the shell you run steps 2–8 from. This block is the whole per-map
part of the runbook; everything below reads from it.

```bash
BUNDLE=twisted_treeline_v2                       # apps/moba/maps/<bundle>/
MAP=TwistedTreeline                              # gen_wdt.py / dbc_tool.py MAP_DIR
MAPID=900                                        # dbc_tool.py MAP_ID
PROJECT=~/tools/wbs-project                      # dbc_tool.py WBS_PROJECT
BLEND=var/blender/${BUNDLE}_blockout.blend       # gen_blockout.py writes this
XFER=$PROJECT/${BUNDLE}.obj                      # the 5.1 -> 3.4 handoff
STAGING=~/tools/tt-transfer/${BUNDLE}_34.blend   # the 3.4 scene, saved by hand
WMODIR=$PROJECT/World/wmo/$MAP
EXTRACT=~/tools/wmo-extract                      # scratch, wiped every step 8
```

`MAP`, `MAPID` and `PROJECT` are not free choices — they must equal the
constants of the same name in `gen_wdt.py` and `dbc_tool.py`, which are what the
WDT filename, the MWMO path and the DBC rows actually ship. Change one there and
re-run `dbc_tool.py patch`.

The two Blender scripts carry their own per-map constants block —
`RENDER_ONLY`, `SINGLETON`, `ORIENT_PROBE`, `TEXTURES`, `DOODAD_*`. A new map
edits those before step 3, and so does a repaint that renames or removes the
orientation probe. Object *counts* are not in that block: both scripts count the
numbered series out of the scene.

Blender 5.1, Blender 3.4 and the client install are machine setup rather than
per-map; their paths are in *Toolchain setup*.

### 0-1. Blockout

```bash
python3 apps/moba/gen_blockout.py
```

Builds every bundle under `apps/moba/maps/*/map_source.yaml`. `--stage build` is
enough when only `build_blockout.py` or the `blockout:` block of
`map_source.yaml` changed. Anything touching the paint, `blockout_trace.py`, or
the trace-side keys needs both stages.

Read off `$BUNDLE`'s report the four numbers a later step consults: **objects**,
**floor chunks and island walls**, **z range**, and **doodads**. Steps 3 and 4
compare each against what you noted. None of them has a correct value — they
follow the trace, the batch-ceiling grid and the `dressing` block.

The rest of the report needs no note. Everything carrying a hard limit — batch
triangles, non-planar floor faces, the 16-bit vertex cap — `gen_blockout.py`
gates itself and fails the build over, so there is nothing left to eyeball.

### 2. OBJ to the staging side

```bash
/Applications/Blender.app/Contents/MacOS/Blender --background "$BLEND" \
  --python-expr "import bpy; bpy.ops.wm.obj_export(filepath='$XFER', forward_axis='Y', up_axis='Z')"
```

Leave `export_materials` at its default -- the `.mtl` carries the material names
and must stay beside the `.obj`.

### 3. Staging scene, Blender 3.4

```bash
/Applications/Blender-3.4.app/Contents/MacOS/Blender
```

Foreground, so `print()` lands in that terminal.

1. Delete Cube, Camera, Light.
2. Outliner -> right-click Scene Collection -> **New Collection**, rename it to
   `$MAP`'s value, **left-click it** so it is active. Not `M > New Collection`,
   which moves the selection into it.
3. File > Import > **Wavefront (.obj)** -- not legacy -- pick `$XFER`,
   **Forward Y, Up Z**.
4. Python Console:

   ```python
   import bpy; print(len([o for o in bpy.data.objects if o.type=='MESH']),
                     sorted(m.name for m in bpy.data.materials))
   ```

   The object count must equal step 1's. Then the axis check:

   ```python
   from mathutils import Vector
   ws = [o.matrix_world @ Vector(c) for o in bpy.data.objects
         if o.type == 'MESH' for c in o.bound_box]
   print("z %.1f..%.1f" % (min(v.z for v in ws), max(v.z for v in ws)))
   ```

   Must match step 1's reported z range. A z instead spanning about half the
   map's width is the -90 deg about X import bug; undo and redo the import
   rather than rotating after.
5. Text Editor -> **Open** `apps/moba/wmo/blender_staging_setup.py` -> Run
   Script. Wants `PASS`, `Outdoor holds N` equal to step 1's object count,
   `derived: N floor chunks, N island walls` equal to step 1's grid, and
   `doodads: N placements` equal to step 1's doodad count.
6. **File > Save As** -> `$STAGING`. The setup exists only in RAM until you do.

### 4. Export

1. Text Editor -> Open `blender_preflight.py` -> Run Script -> `PASS`.
2. Clear the export directory. A leftover group file from a longer export is
   invisible to the client -- MOHD caps how many are read -- and rides into the
   MPQ anyway:

   ```bash
   rm $WMODIR/*.wmo
   ```
3. `File > Export > WMO (.wmo)`, method **Full**, *Export selected objects*
   **off**, to `$WMODIR/$MAP.wmo`.
4. ```bash
   ls $WMODIR | wc -l
   ```

   Must be step 1's object count **+ 1**.

### 5. Verify

```bash
python3.10 apps/moba/wmo/wmo_verify.py $WMODIR/$MAP.wmo
```

`PASS`, `MOHD groups` matching step 4, and a nonzero **triangles kept as
COLLISION**. A render-only object reported as contributing no collision is
working as intended, not a fault.

### 6. WDT

```bash
python3.10 apps/moba/wmo/gen_wdt.py --dump
```

`dbc_tool.py patch` only on a first build, or if the DBC constants change -- the
rows are already in the staging tree.

### 7. Pack

Client closed on that install.

```bash
apps/moba/wmo/mpq_pack ~/Games/wow335/Data/enUS/patch-enUS-4.MPQ \
                       $PROJECT World DBFilesClient
```

File count = groups + root + WDT + 4 DBCs.

### 8. Extract

```bash
rm -rf $EXTRACT && mkdir -p $EXTRACT && cd $EXTRACT
~/code/azerothcore-wotlk/env/dist/bin/vmap4_extractor -d ~/Games/wow335/Data/
~/code/azerothcore-wotlk/env/dist/bin/vmap4_assembler Buildings vmaps
cp vmaps/${MAPID}.vmtree vmaps/*.vmo ~/code/azerothcore-wotlk/env/dist/bin/vmaps/
rm ~/code/azerothcore-wotlk/env/dist/bin/mmaps/${MAPID}*.mmtile
cd ~/code/azerothcore-wotlk/env/dist/bin && ./mmaps_generator $MAPID
```

The `.vmo` files are globbed rather than named, and the glob has to be that wide.
The extractor recases the WMO its own way (`Twistedtreeline.wmo.vmo`, not
`$MAP`), and **a doodad model that probes bounding triangles contributes a
`<Model>.m2.vmo` of its own** — a category that did not exist before the map
carried dressing, and that `*.wmo.vmo` silently misses. The wall-face families
all probe zero, so they render and never reach this directory.

Both `rm`s are load-bearing: `vmap4_extractor` refuses a non-empty output
directory, and `mmaps_generator` skips any tile whose existing `.mmtile` matches
the current config, without ever looking at the geometry.

The four tools are already built and installed and `TOOLS_BUILD=maps-only` is in
the CMake cache, so the `cmake` / `make` / `make install` block in step 8 below
applies only to a fresh checkout.

### 9. In game

Restart the worldserver, `.debug bg`, queue in. Then `.go xyz <x> <y>` for a
point step 1's report says is floor -- **give it a z of 5** and let yourself drop
onto the surface, which doubles as a vmap check. Omitting z cannot work on a
WMO-only map: there is no terrain for `Map::GetHeight` to fall back on, and its
vmap search runs only `DEFAULT_HEIGHT_SEARCH` 50 yd down from `MAX_HEIGHT`.

### Rebuilding after a texture-only change

Textures live entirely in the client patch — MOTX ships path strings and the
server never reads them, so `vmap4_extractor` and `mmaps_generator` consume
geometry that has not moved.

Run **2, 3, 4, 5, 7**. Skip 6 and 8, and relaunch the client rather than the
worldserver.

- **6** writes the WDT and patches the DBCs. Neither changes while the WMO path
  does not, and step 7 packs the copies already sitting in `$PROJECT` anyway.
- **7 is not optional.** Steps 2–5 only write into `$PROJECT`; nothing reaches
  the client until the MPQ is packed. Skipping it relaunches to the map you
  already had.
- **8** rebuilds vmaps and mmaps. The existing `.vmo` and `.mmtile` still
  describe the map correctly.

The test for whether this applies is `gen_blockout.py`'s own report: re-run 8
when the **geometry** numbers change, skip it when only the material legend
does. **A dressing change may or may not be one of these** — a doodad reaches
the vmaps through `Doodad::ExtractSet` only if it has bounding triangles, and
wall dressing deliberately uses models with none. `wmo_verify.py`'s `doodads
emitted to vmaps` line is the test: unchanged from the last build means the
extractors would produce the same bytes, so skip 8 and relaunch the client.

**Both of those are COUNT tests, and a count cannot see a change that moves what
it counts.** Two got through. The outer plate was changed to follow the crest
instead of pinning to its global maximum, which moved vertices up to 10 yd while
every object, face, doodad and z-range figure held steady — `hull_ring` and the
tessellation are 2D and never see z. And a doodad `scale` change rewrites every
spawn's transform in the `.vmtree` while the placement count is identical. When
a change alters *values* rather than *quantities*, run 8 and do not consult the
report.

### Previewing a texture without shipping it

The 3.4 viewport shows real art, so a candidate can be judged without touching
steps 4-9. To try one:

1. Repoint an existing material key's `texture:` in `map_source.yaml`.
2. `python3 apps/moba/gen_blockout.py --stage build`.
3. Re-run the staging script in the Blender session already open.

No OBJ export and no re-import. Material *names* travel through the OBJ but a
key's texture path does not — that reaches the 3.4 side through the materials
sidecar, which is also why step 2 is not optional: Blender's python has no
`yaml`. Adding a key, or changing which surface wears which key, does change the
names and needs step 2 and a fresh import as well.

Shading mode decides what you see. **Material Preview** draws the texture;
**Solid** draws `diffuse_color`, the per-material hue that catches a wall
wearing the wrong candidate from a top-down look. Both are worth keeping.

## Why each step is what it is

The runbook above is the commands. This is the reasoning behind them — read it
when a step fails, when you are adapting the pipeline, or before changing any of
the scripts.

### 0. Trace the boundary from a reference image

Skip this if the blockout is designed rather than traced.

**Paint the trace by hand; do not derive it.** Automated segmentation of art
was tried twice and abandoned both times, for reasons inherent to the sources:

- **A lit 3D render does not threshold into floor versus wall.** Measured on a
  1920x1080 orthographic render: an Otsu threshold (0.251 luminance, 23.7% of
  pixels) recovers real structure, but the mask breaks into 131 components with
  the top three holding only 71% of the bright area. Floor beside a wall falls
  into shadow while lit wall tops rise above the threshold, so the mask
  disagrees with the truth exactly where the boundaries are.
- **A flat-lit schematic segments on hue where luminance fails** — walls run
  warm (`R-B > 0`) and floor cool. That recovers the interior cleanly, but it
  still cannot separate a grey wall ring from a grey base platform, because
  there is no colour cue between them.

A hand paint sidesteps both, and carries semantics no derivation can: which
floor is a base, where a ramp is, where an elevation changes.

**Paint over the reference on an opaque black layer**, one flat colour per
class, exported as PNG. Two things that are *not* required, contrary to
instinct:

- **Anti-aliasing is fine, and better than a hard edge.** Classify by *nearest*
  key colour rather than exact match, and a blended edge pixel splits at the 50%
  point — which is the true sub-pixel boundary. Demanding a hard-edged brush
  buys nothing and costs accuracy.
- **Canvas size and framing are free.** Derive the registration from the paint
  itself rather than fitting it against the reference frame.

**Registration is derived, not fitted:**

```
world_x = (px_x               - cx) * S
world_y = (px_row_from_bottom - cy) * S
```

- `S` = the map's intended width in yards ÷ the painted floor's width in pixels
- `cx` = the paint's own mirror-symmetry optimum, where the map is symmetric
- `cy` = the painted floor's bbox centre

Image rows count down from the top while world Y counts up, so the row index
flips before the multiply. **Rescaling a finished map is a change to the
intended width alone** — no repaint, no re-trace. Nothing references the
reference image's frame, so cropping or repainting at another resolution stays
correct. An earlier attempt to *fit* this transform against a render is worth
not repeating: correlating region luminance against a traced outline came out
flat across ±8%, and a left-right flipped null model scored 0.2822 against
0.2823 — an objective that cannot distinguish a mirrored map from the correct
one measures nothing.

**Smooth the traced contour with a low-pass filter, not Douglas-Peucker.** DP
preserves maximum-deviation points, and on a hand trace those are precisely the
stylus tremor — it keeps the jitter and discards the smooth runs. Use a circular
Gaussian convolution on the arc-length parameterised loop. A wiggle of
wavelength `L` is attenuated by `exp(-2*pi^2*sigma^2/L^2)`, so `sigma = 2 yd`
removes 99% of a 4 yd wobble while keeping 93% of a 32 yd curve. Gaussian
smoothing shrinks convex curves by roughly `sigma^2 * curvature`; report the
area change and the max deviation so it stays a measured quantity.

**A uniform offset of a traced contour folds** wherever the curve turns tighter
than the offset distance, and a fold does not change the loop's total signed
area — so an area check cannot see it. Guard by testing whether each vertex's
move reversed one of its own edges, and pull back the offenders.

**Extend the height field past its own mask before writing it.** The field is
zero outside the paint, but the traced contour is smoothed afterwards, so rim
vertices land slightly *outside* the mask and sample that zero. A base platform
then blends from its real height down to nothing across its last few yards and
meets the wall across a gap. Dilate the field a few pixels into the empty region
first (`blockout_trace.py`'s `extend_past_edge`) — the smoothing's reported max
deviation is what sets how many.

**Output:** closed polygons in world yards — the outer playable boundary plus
one loop per interior wall island — plus a grayscale height field if any region
is raised. Expect hundreds to thousands of points, so they want their own file
rather than a block inside a config.

### 1. Blockout — Blender 5.1

1 Blender unit = 1 WoW yard, so build at true scale. The 5.1 file never moves to
3.4 (see *Two version traps*). It is **generated from the trace and a config**,
which are the source; the `.blend` is a build artifact.

Before transferring, the geometry needs:

- **UVs on every mesh.** The exporter raises on a group without a UV layer
  named exactly `UVMap`. Cube projection in world space, tiling — not a packed
  0–1 atlas, since these reference tiling terrain textures.
  **Tiling rate is per material, not global.** A ground texture is
  scale-agnostic, but a masonry one depicts a known real-world span — stonework
  at 8 yd/tile renders at twice its intended size. Once any surface wears a
  `DUNGEONS\TEXTURES\WALLS\` asset, `uv_scale_yd` has to vary per material:
  `map_source.yaml`'s `materials` block carries the override and
  `blockout.uv_scale_yd` is only the fallback.
- **No empty material slots.** A slot with no material assigned is an export
  hazard.
- **Applied scale and rotation.** Unapplied transforms survive OBJ as baked
  geometry, so this matters less than it looks, but it keeps the two files
  comparable.
- **A decision, per object, on collide vs render-only.** This is the single
  most consequential authoring choice — see *Why collision is what goes wrong*.
  Render-only suits thin decorative pads sitting just above the floor, where
  collision would stack near-coincident surfaces, and overhead canopy. **It
  travels by object name**, because custom properties do not survive OBJ
  (step 2) — the names must match `blender_staging_setup.py`'s `COLLIDE` and
  `RENDER_ONLY` sets exactly or step 3 aborts.
- **Under 65,536 vertices per object.** A WMO group indexes its vertices with
  16 bits. Tessellate the floor coarsely and refine only where a height field
  actually varies; a uniform target fine enough for a 6 yd ramp puts tens of
  thousands of vertices on flat ground for nothing.

**Prefer extruded rings to a slab-and-boolean.** Booleans are the fragile step —
a cutter of a few thousand near-coplanar faces has destroyed a slab outright —
and they hand back topology nothing predicts, which then has to be *selected*
to terrace. Extruding each boundary loop into rings gives quads you placed, so
terracing and bevelling become arithmetic on a ring rather than a face
selection. Islands want solid capped prisms; the outer boundary wants one
ribbon whose outer edge is the **convex hull** pushed outward, since a convex
loop cannot fold and a bounding rectangle leaves huge flat corners nowhere near
the play area.

**Triangulate the floor explicitly before assigning heights.** WBS consumes
Blender loop triangles, so a planar ngon exports fine — but
`bmesh.ops.subdivide_edges` splits edges without re-triangulating the faces
around them, so refining a tessellation *grows* ngons instead of dividing quads.
One floor reached 105-vertex faces spanning 365 yd. Loop triangulation fans a
face that large from a single corner, and once the vertices carry per-vertex
heights the fan's long edges cut across the slope instead of along it: a ridge
you have to climb, running the length of the lane. Call `bmesh.ops.triangulate`
after the last subdivision and before z is assigned; `gen_blockout.py` gates on
`floor_max_face_verts > 3`.

**Sample the height field bilinearly for floor vertices, not nearest-pixel.**
Nearest-pixel quantises a slope to the field's pixel size, so two vertices a
yard apart on the same ramp can land on different plateaus and leave the surface
locally non-monotonic — a ramp that catches you when you run across it. Wall
*feet* still want a max filter over a 3x3 neighbourhood: there the goal is to
sit no lower than any nearby floor, not to follow it.

### 2. Transfer to a 3.4 staging scene

Export the shipping objects as OBJ from 5.1, import into an empty 3.4 file.

**Set Forward = Y, Up = Z on both sides.** The exporter and importer have
*different* defaults, and the importer's (−Z forward, Y up) silently rotates
everything −90° about X. Nothing downstream complains; the map is simply
sideways.

A wall reading its height in Y instead of Z is the axis bug. Undo and redo the
import rather than rotating after the fact.

**Custom properties do not survive OBJ.** Any per-object metadata — including
which objects collide — has to be re-established on the 3.4 side, which is what
`blender_staging_setup.py` exists for.

### 3. Set up the staging scene

Run `blender_staging_setup.py` in the Text Editor. It builds the WBS structure:
root collection, `Outdoor`, collision vertex groups on the colliding subset,
materials wired to texture paths, the root's `wmo_id`, and the doodad set — and
turns the scene into the server frame, for which see *Where the map lands*.

**The staging `.blend` holds nothing you cannot regenerate** — that is the point
of the script, and it is verified (see below). Treat it as disposable and keep
the source `.blend` backed up instead.

Doodads are the case that threatens this, and why they do not is worth stating.
A placement is hand-authored information — model path, position, rotation,
scale — and two of the three obvious homes are closed to it. It cannot sit in
the source `.blend`, because `wow_wmo_doodad` is a WBS property and WBS only
runs in 3.4. It cannot travel through the OBJ, which carries geometry and no
custom properties — the same reason a per-object collide flag has to be
re-established on the 3.4 side.

**The third home is the one the material paths already use.** Placements resolve
on the 5.1 side, from `map_source.yaml`'s `dressing` block against the geometry
being built, and land in `var/blender/<bundle>_doodads.json` beside the
materials sidecar; this script instantiates them. Nothing in the 3.4 scene is
original data, so the disposability claim has no expiry date.

That is also why the resolver lives in `build_blockout.py` rather than
`gen_blockout.py`: a wall top's height comes out of `height_profile()`'s seeded
run along the resampled loop and exists nowhere else, so whatever places a
doodad on one has to be the process that built it.

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

**The export is a plain function underneath, so step 4 is scriptable.** The menu
is a thin wrapper over `io_scene_wmo/wmo/export_wmo.py:12`:

```python
from io_scene_wmo.wmo.export_wmo import export_wmo_from_blender_scene
import bpy

export_wmo_from_blender_scene(
    "/path/to/Root.wmo",
    int(bpy.context.scene.wow_scene.version),   # WoWVersions.WOTLK == 2
    False,                                      # export_selected
    'FULL',                                     # export_method: 'FULL' | 'PARTIAL'
)
```

That turns "method Full, export selected off" from two controls to click
correctly into two arguments. Read the version from the scene rather than
hardcoding 2, which is what the operator itself does. The session still has to
be a GUI 3.4 one — see *What cannot be automated*.

### 5. Verify

Do this before packing anything. It replays what `vmap4_extractor` will decide,
so a dead map is caught here instead of after a pack-and-extract round trip.

The number that decides whether the map is playable is **`triangles kept as
COLLISION`**. Zero means terrain that looks perfect and cannot be stood on.

### 6. WDT + DBC rows

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

The client cannot load a map without its `Map.dbc` row, and **`vmap4_extractor`
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

**A new map needs a `mapdifficulty_dbc` row or every static spawn on it logs an
error.** `ObjectMgr::LoadCreatures` builds `spawnMasks[mapId]` by ORing a bit per
difficulty that has a row (`ObjectMgr.cpp:2345`), so a map with no row gets mask 0 and
any spawn's `spawnMask` trips "wrong spawn mask ... not supported difficulty modes".
Log-only — nothing at spawn time reads `spawnMask`, and the creatures spawn fine — but
it is an ERROR on every boot, and a real failure hiding in that noise is expensive.
Stock battleground rows carry nothing but ID, MapID, Difficulty and the locale mask.

### 7. Pack the client patch

One archive holds the whole patch: the `.wmo` set, the WDT, and the patched DBCs.
No textures — MOTX ships paths, and everything referenced is a stock asset.

**It has to be the locale archive.** Both extractors search the most recently
opened archive first (`MPQArchive`'s ctor does `push_front`), and they build that
order in opposite directions: `map_extractor` opens locale archives then base
ones, so `Data/patch-N.MPQ` ends up ahead; `vmap4_extractor` appends the locale
patch scan last, so the locale chain ends up ahead.

For the `.wmo` and the WDT that disagreement is harmless — new filenames, nothing
else provides them. For the DBCs it decides the map, because they shadow files
the client already ships, and **every stock DBC lives only in the locale chain —
not one base archive holds a single one.** Pack them into `Data/patch-4.MPQ` and
`vmap4_extractor` resolves `Map.dbc` to the stock locale patch, never sees the new
map id, and builds no vmaps for it. `map_extractor` meanwhile reads the new row
fine, so the map half-works and the failure surfaces much later as missing
collision.

In the locale chain both extractors agree, and so does the client — a higher
patch number winning is the same mechanism that makes stock `patch-<loc>-3.MPQ`
override `patch-<loc>-2.MPQ`.

Files go in **locale-neutral (0), zlib-compressed**, matching how Blizzard stores
DBCs inside its own locale patches (flags `0x84000200` =
`EXISTS | SECTOR_CRC | COMPRESS`).

Re-packing rebuilds the archive from scratch, so re-export → re-pack → re-extract
carries no incremental state that can drift.

**The archive this writes is the extraction source, not the play copy.** The path
above is whatever install `vmap4_extractor` will read. A client you actually play
on that is a different install — another machine, or a VM — needs its own copy of
the same file, and nothing in this pipeline puts it there. Copy it with the client
closed; MPQs are held open.

Three things have to hold on that copy, and all three fail silently: it goes in the
**locale** folder (`Data\enUS\`, not `Data\`), for the same reason the pack does;
the patch number must not collide with one the install already ships (stock 3.3.5a
ends at `patch-<loc>-3`); and the locale in the folder and filename must match the
client's own. The bytes are locale-independent — files are packed locale-neutral —
so a rename is all a different locale needs.

### 8. Run the extractors

The tools are not built by default — `TOOLS_BUILD` defaults to `none`:

```bash
cd var/build/obj
cmake -DTOOLS_BUILD=maps-only .
make -j$(sysctl -n hw.ncpu)
make install
```

That whitelists the four needed and installs them, plus `mmaps-config.yaml`, into
`env/dist/bin/`. The binaries are the lowercased source directory names —
`map_extractor`, `vmap4_extractor`, `vmap4_assembler`, `mmaps_generator` — not the
un-underscored spellings upstream uses for its release archives.

Into `env/dist/bin/`, not `var/extractors/` — those five `.gitkeep` directories are
upstream's Docker layout and nothing in a local build reads them.

**A WMO-only map produces no `.map` files and no `.vmtile`.** The global WMO's
spawn lives in the `.vmtree` itself, and the model sits beside it as
`<Model>.wmo.vmo` — basename with only the first letter capitalised. The whole
footprint is that pair plus `mmaps/<id>.mmap` and one `.mmtile` per tile.
`map_extractor` need not run at all: it has no per-map flag, and there are no map
tiles to extract.

- **`vmap4_extractor` refuses a non-empty output directory.** It stats
  `Buildings/dir` and `Buildings/dir_bin` and quits with "Your output directory
  seems to be polluted". Clear the scratch dir before re-running.
- **The two extractors mean different things by their path argument.**
  `map_extractor -i` takes the game *root* and appends `/Data/` itself;
  `vmap4_extractor -d` takes the *Data* directory.
- **The `.vmtree` must be installed before `mmaps_generator` runs.**
  `discoverTiles` finds a tile-less map only through it
  (`MapBuilder.cpp:116-125`) — with no `.map` files that is the sole discovery
  path.
- **`mmaps_generator` skips a tile it has already built.** `shouldSkipTile`
  compares the existing `.mmtile`'s magic, `dtVersion`, `mmapVersion` and the
  serialised recast config and returns true on a match
  (`MapBuilder.cpp:1035-1061`) — it never looks at the geometry. New terrain
  under an unchanged config is silently ignored, and the fresh `.mmap` written
  beside the stale tiles makes it look like a rebuild happened. Delete
  `mmaps/<id>*.mmtile` before every re-run.
- `checkDirectories` wants `maps/` non-empty *globally* and `vmaps/` holding at
  least one `.vmtree`, so `mmaps_generator` runs from the install directory, not
  the scratch dir. It needs `mmaps-config.yaml` in the CWD, or `--config`.
- `Couldn't open RootWmo!!!` on `World\Wmo\Band\Final_Stage.wmo` is expected;
  `apps/extractor/extractor.sh` prints a banner saying so. Do not chase it.
- Do not use that script for one map: every function in it opens with `rm -rf`
  over the target directories, and its `mmaps_generator` call passes no map id, so
  even the cheapest option rebuilds everything.

Verify by parsing, not eyeballing. The `.vmtree`'s `GOBJ` spawn should sit at the
grid centre `(17066.67, 17066.67, 0)` with the model's bounds around it, and every
`.mmtile` should report `DNAV` v7, `mmapVersion 19` and a **nonzero polygon
count** — a tile that builds with zero polygons is terrain nothing can walk.

**Every model the `.vmtree` names must have a `.vmo` beside it**, because the
tree stores names and the `.vmo` files store the geometry:

```bash
cd env/dist/bin/vmaps
strings -a ${MAPID}.vmtree | grep -E '\.(m2|wmo)$' | sort -u |
  while read m; do [ -f "$m.vmo" ] || echo "NO VMO: $m"; done
```

About 8% of spawned models across the whole world fail to convert and always
have — the misses are spread evenly through the alphabet, so they are individual
failures rather than one abort. Harmless where nothing walks. **Worth checking
anyway because of how the assembler fails**: `convertWorld2` breaks its
conversion loop on the first error (`TileAssembler.cpp:203-208`), so a model that
fails hard strands every model sorting after it, and the only symptom is
collision quietly missing.

### Getting onto the finished map

Parsing proves the files are right. It does not prove the client renders the map or
that a player can stand on it — that needs `.go xyz <x> <y> <z> <mapid>` in-game.

**If the map's `Map.dbc` `InstanceType` is 3 or 4, `.go` refuses it silently.**
`Player::TeleportTo` returns false with no message and no packet when
`mEntry->IsBattlegroundOrArena()` and the player is not already inside one
(`Player.cpp:1375`); GM level is irrelevant. Enter a match on that map first — from
inside, `.go` works normally and is the way to walk terrain that has no content on
it yet. A map registered as a normal world map (`InstanceType 0`) has no such guard.

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

## Five ways the export fails that the file won't show you

Each of these was hit for real. `blender_preflight.py` checks the first four;
the fifth is upstream of the export, gated in `gen_blockout.py` and caught again
by `wmo_verify.py`.

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
- **A group over 21,845 triangles draws only part of itself.** WBS emits one
  MOBA render batch per material per group, and a batch counts MOVI *indices* in
  a `uint16` (`wmo_format_group.py:169`). Three indices per triangle puts the
  wrap at 21,845, and past it the client draws only the remainder — 84,891
  indices became 19,355, a floor 77% transparent. Nothing else notices: the file
  parses clean and MOPY and MOBN are untouched, so collision is complete and you
  fall through nothing. This ceiling binds well before the 65,535-vertex one in
  step 1. `build_blockout.py` splits the floor into a grid sized to stay under
  it, and `wmo_verify.py` fails any group whose batches do not cover its whole
  MOVI.

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
| `blender_staging_setup.py` | Blender 3.4 Text Editor | yes, 2026-08-18 |
| `blender_preflight.py` | Blender 3.4 Text Editor | yes, 2026-08-18 |
| `wmo_verify.py` | `python3.10`, standalone | yes, 2026-08-18 |
| `gen_wdt.py` | `python3.10`, standalone | yes, 2026-08-18 |
| `dbc_tool.py` | `python3.10`, standalone | yes, 2026-08-18 |
| `mpq_tool.py` | `python3.10`, standalone | yes, 2026-08-12 |
| `mpq_pack.cpp` | compiled, standalone | yes, 2026-08-18 |

Five of these carry a per-map constants block at the top — the two Blender
scripts, `wmo_verify.py`, `gen_wdt.py` and `dbc_tool.py`. **That block is the
per-map part**; a second map edits it and leaves the rest alone. If a third map
turns up, that is the point to move the block into a YAML config the way the SQL
generators do; two maps do not justify it yet. `mpq_tool.py` and `mpq_pack` are
not in that set: both take what they operate on as arguments, so neither has a
per-map part at all.

**A constant belongs in the block only if a human chose it.** Anything the
blockout derives is counted at runtime instead: the two Blender scripts walk
their numbered object series out of the scene, because island walls follow the
paint and floor chunks follow `gen_blockout.py`'s batch-ceiling grid. A declared
count would be a copy of a number neither file can see, and it would go stale
silently — the loud failure it produces is an object-set mismatch that reads
like a bad import.

`ORIENT_PROBE_Y` is stored in the *blockout* frame by the two Blender scripts
and in the *server* frame by `wmo_verify.py` — the same measurement with
opposite signs. Copy one into the other and the check inverts: a correct export
fails, an unturned scene passes.

### `blender_staging_setup.py` — build a staging scene from a bare OBJ import

Produces a WBS-ready scene: root collection, `Outdoor`, collision vertex groups
on the colliding subset, materials wired to texture paths, and the doodad set.

**Proven to reproduce the staging scene exactly.** Rebuilt from an OBJ into an
empty 3.4 file on 2026-08-12: all 47 group files came out the same size as the
hand-built original, 17 byte-identical, and every triangle count, collision
count, material path and doodad matched. This is what makes the staging
`.blend` disposable — re-run the proof if the script changes.

Group and material *ordering* differs, because Blender sorts collection
contents case-insensitively and the script links them in ASCII order. That
moves group-file indices and material ids; it changes nothing the extractor
reads by name.

It loads the real BLPs. Each material's texture is pulled from the client
archives through `mpq_tool.open_archives`, decoded by WBS's `BlpConverter`, and
cached as PNG under `var/blender/texcache/` — so the 3.4 viewport shows the
actual art in Material Preview while `diffuse_color` keeps the per-material hue
Solid shading uses. That needs arm64 fix 6 below; without it every material
falls back to a blank 1x1 placeholder, which is reported and harmless, because
MOTX ships the path string and the export never reads pixels.

Doodads carry their real geometry too. Each unique model's `.m2` and every
`.skin` profile it declares are extracted to `var/blender/m2cache/` and read
with pywowlib's `M2File`, giving one mesh per model, shared across that model's
placements and textured per submesh. Anything unreadable falls back to the box
proxy and is reported. **The export is unaffected either way** — MODD is written
from the object's transform and `wow_wmo_doodad.path` and never reads the mesh
(`wmo_scene.py:616-638`), which is what makes real geometry a free preview
rather than a shipping decision. Model coordinates go in raw, matching the box
proxies' own convention; verified against the two Dalaran banners, which are
asymmetric and face out of their walls in the viewport as they do in game.

### `blender_preflight.py` — check an existing staging scene

Additive and idempotent. Verifies every precondition the WBS exporter enforces,
repairs only what is missing, and prints each change it made under `changed:`.
Run it before every export.

It counts the doodad set against the sidecar rather than building it; building
is `blender_staging_setup.py`'s job.

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
python3.10 mpq_tool.py index listfile.txt     every entry
python3.10 mpq_tool.py find ghostlands .m2    entries matching all substrings
python3.10 mpq_tool.py probe 'World\...\X.m2' collision header of a model
python3.10 mpq_tool.py extract 'World\...\Y.wmo' outdir/
```

The MPQs carry internal listfiles, so asset paths are enumerable rather than
guesswork. A `_s` suffix on a texture is a specular map — never assign one as
diffuse.

**Doodad paths are not consistently spelled, and a sweep filtered on
`passivedoodads` silently loses 19% of them.** 1871 models live under
`PASSIVE DOODADS` — with a space — including the whole `HangingLantern` family.
Match `passive ?doodads`, or take every `.m2` under `World\` and accept the wider
net. Nothing warns; the missing models simply never appear as candidates.

**WMOs shop in `DUNGEONS\TEXTURES\`, not `TILESET\`.** The latter is ADT terrain
art, authored to be *structureless* so it tiles and alpha-blends without visible
repetition — which is exactly what makes it read as flat noise stood up a 16 yd
wall. Blizzard's own outdoor rock WMOs use the dungeon tree:
`HillsbradTerraceWall.wmo`, an open-world terrace, carries a single texture and
it is `DUNGEONS\TEXTURES\WALLS\MM_STRMWND_WALL_04.BLP`. `TILESET\` is right for
the one case it was authored for — a horizontal surface seen from above, so
floors and nothing else.

**A texture authored for vertex tinting ships at full brightness.** WBS writes
no MOCV on an outdoor group — `wmo_scene_group.py` sets `mocv = None` for
anything `is_indoor` returns false for, which is every group this pipeline
produces. A texture painted light and flat, expecting the WMO to darken it,
renders blinding here. `MM_STRMWND_WALL_04.BLP` decodes nearly white on its own
and is what Blizzard's outdoor `HillsbradTerraceWall.wmo` wears, so finding a
texture on a real WMO says nothing about how it will look on one of ours. Judge
a candidate from its decoded BLP.

Sets are authored as families sharing a prefix with the surface class in the
name — `JLO_MCAVEG_GROUND` / `_WALL` / `_LEDGE` / `_CEILING`. Picking a *set*
gets a matched floor-and-wall pair for free; picking individual textures does
not. Read a reference WMO's MOTX chunk to see what a real one uses.

`probe` answers the one question that decides whether a doodad is decoration or
a wall: a model with **0 bounding triangles** renders and never collides, and
one with more than 0 always collides. There is no third option, which is why
"walkable but vision-blocking" brush cannot be a doodad at all.

**For shape, not collision, the asset browser's `/api/model/` is the tool** — it
returns the vertex list quantised against `span`, alongside `lo`/`hi`/`size`, so
the width across any z-slice is a few lines of arithmetic. That is the
difference between "this model is 8 yd wide" and "its ROOTS are 5 yd and its
canopy 16", which a bounding box cannot tell you and which is usually the
question a placement turns on: canopy overhanging a cliff is wanted, root flare
hanging in the air is not.

**Scaling a family to a common height leaves its width free**, and stock models
vary enough in proportion that this bites repeatedly. Two trees of one family
at 26 yd tall came out 10.9 and 25.6 yd wide. A per-surface multiplier shrinks
both equally and can never bring them into line — the fix is to split them into
families with their own `height_yd`, picked so the dimension that meets the
surface is what matches.

### `mpq_pack` — build the client patch archive

```bash
STORM="${WBS_ROOT:-$HOME/tools/blender-wow-studio}/io_scene_wmo/pywowlib/archives/mpq/native"
clang++ -std=c++17 -O2 -I "$STORM/include" \
  apps/moba/wmo/mpq_pack.cpp "$STORM/lib/libstorm.a" -lz -lbz2 \
  -o apps/moba/wmo/mpq_pack
```

```
mpq_pack OUT.MPQ SRCDIR [SUBDIR ...]
```

Packs a directory tree into a fresh MPQ v1 archive, turning `/` into `\` and
skipping dotfiles. Replaces the output if it already exists.

**pywowlib's storm binding cannot do this** — `SFileCreateArchive` is commented
out of its method table and there is no add-file wrapper at all, so the module is
read-only. The write API comes instead from `lib/libstorm.a`, a byproduct of the
*same* pywowlib build that produces the `storm` module every other tool here
imports: no extra setup step, and a rebuilt WBS restores both together. The
binary is gitignored — rebuild it with the command above.

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
6. `blp/BLP2PNG/setup.py`: add `-DPNG_ARM_NEON_OPT=0` to the Darwin
   `extra_compile_args`. The vendored libpng has no `arm/` directory at all, but
   `pngpriv.h:132` auto-enables NEON on an arm target and `pngpriv.h:142` then
   points `PNG_FILTER_OPTIMIZATIONS` at `png_init_filter_functions_neon` — a
   symbol whose source file was never vendored, so the extension builds and
   fails at *import* with `symbol not found in flat namespace`. libpng's own
   comment three lines above names this flag as the fix. `PNG2BLP/setup.py:49`
   carries the same latent bug; nothing here needs it.

### What cannot be automated

- **Launch Blender 3.4 from a terminal.** `print()` goes to stdout and macOS
  has no system console, so the launching terminal is the only place script
  output and exporter tracebacks appear.
- **WBS crashes Blender 3.4 under `--background`** (dies in `auto_load` at
  `ui/operators.py:557`). Headless runs need `--factory-startup`, which
  disables the addon — so headless is limited to geometry checks.
- **That `--background` limit is WBS's, not Blender's.** Blockout work needs no
  addon, so step 1 runs fully headless under Blender 5.1 —
  `blender --background --python build.py` — with all of `bpy` and numpy
  available. Only the 3.4 side, steps 2 through 4, is bound to a GUI session.
- Launching the binary directly rather than via `open` starts a second
  instance, which is how to test a rebuild against a known-good session.
- Inspecting a 3.4 `.blend` from a newer Blender works and needs no addon, but
  only reads real data — meshes, vertex groups, collections. WBS's
  `wow_wmo_*` property groups will look empty.

## Requirements

`python3.10` specifically for the standalone *python* tools — they import
pywowlib's StormLib binding, compiled against Blender 3.4's interpreter
(`storm.cpython-310-darwin.so`). Any other python fails the import. `mpq_pack` is
C++ and needs only clang plus `libstorm.a` from that same build.

**BLP pixels are reachable outside Blender too.** `BLP2PNG.cpython-310-darwin.so`
is built against the same 3.10 ABI as the storm binding, so a standalone
`python3.10` importing `BlpConverter` from
`~/tools/blender-wow-studio/io_scene_wmo/pywowlib/blp/BLP2PNG` decodes an archive
BLP straight to PNG — no Blender, no GUI session, arm64 fix 6 above still
required. Verified 2026-08-25 on a 44,876-byte BLP out to a 256x256 RGBA PNG.
Contact sheets and headless texture passes therefore need nothing from the 3.4
side.

**The toolchain spans two pythons and neither is sufficient alone.** Blender's
bundled python has numpy (2.3.4) and reads packed image pixels through
`Image.pixels.foreach_get`, but has no `yaml`. The system python 3.9 has `yaml`,
which every generator needs, but no numpy or PIL. That split — not preference —
is why `gen_blockout.py` resolves config and shells out to Blender-side tools
that take JSON.

Paths default to `~/Games/wow335/Data` and `~/tools/blender-wow-studio`;
override with `WOW_DATA`, `WBS_ROOT` and `WOW_LOCALE`.
