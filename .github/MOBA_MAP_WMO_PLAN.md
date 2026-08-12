# Custom map: Twisted Treeline terrain via WMO

## Status of this doc

Steps 1 and 2 are complete. Steps 3–6 remain **unverified against tooling or
engine source** — exact DBC columns, MPQ packing, extractor behaviour on a
WMO-only map. Treat each as a thing to prove in its own focused session.

## Context / decision

The MOBA currently hijacks the Eye of the Storm map (client queues EotS, server
runs `BattlegroundMOBA`). The plan is to move it onto a custom
**Twisted Treeline-style** map. Terrain route, **decided 2026-07-12**:

- **WMO route first.** The approved Blender blockout geometry already *is* the
  walls and collision, so a WMO reuses it directly — no terrain re-sculpting.
- **ADT route (Noggit re-sculpt) only as fallback**, if the WMO result isn't
  good enough. The likely reason it wouldn't be: outdoor look — ground-texture
  blending and fog/lighting are less flexible in a pure-WMO map than in ADT
  terrain. That is exactly what the ADT fallback would fix.
- **GameObject kitbash rejected, 2026-08-11.** Assembling the blockout from
  existing WoW gameobjects placed by SQL on an existing map needs no client
  patch, so it is the only route playable today. It dies on mmaps: `MMapMgr`
  loads prebuilt `.mmtile` files and nothing rebuilds them at runtime, while a
  spawned gameobject reaches only the dynamic VMap tree
  (`Map::InsertGameObjectModel`) — players would stand on kitbashed platforms,
  creeps would path over the terrain underneath.

Best implemented as a **WMO-only map**: a WDT that references a single global
map object (the `WDT_USES_GLOBAL_MAP_OBJECT` flag) with no ADT terrain tiles —
the way arenas / some instances are built. The whole playable space is one WMO.

## Art direction — decided 2026-08-11

**WoW-native, not TT-faithful.** Keep Twisted Treeline's layout; let a dark WoW
biome carry the look. Primary families: Ghostlands / Plaguelands / Duskwood for
dead-but-elegant vegetation, Icecrown and the `DUNGEONS/TEXTURES` crypt/bone
sets for bone and spike work, Duskwood for webbing. Asset paths are enumerable
from the client MPQs — see *Texture assignment*.

**Brush is decoration for now.** Two consequences:

- Its M2 must have **zero bounding triangles**, or it silently becomes a
  collision wall (`Model::open` rejects models with no collision geometry, so
  the ones it *accepts* are exactly the ones that block).
- Making brush a real LoL-style feature later is **C++ work in
  `BattlegroundMOBA`**, not a doodad swap: WoW collision blocks movement and
  line-of-sight together, so "walkable but vision-blocking" has no static
  primitive. It needs zone checks plus a stealth-like aura.

## Source of truth

The blockout lives in `var/blender/twisted_treeline_blockout.blend`
(collection `TwistedTreeline`, 132 objects, 1 unit = 1 WoW yard, pixel-traced
from the real TT minimap). Structure coordinates + lane waypoints export to
`var/blender/twisted_treeline_layout.json`. The placeholder marker meshes
(towers, camps, boss, altars) are position anchors only; they do not ship —
but **do not delete them**: they are what `twisted_treeline_layout.json` is
generated from. They are kept out of the WMO by absence from `TT_WMO_Export`,
not by removal.

### High-res reference + registration (2026-08-11)

`var/blender/tt_topdown.webp` — 1920×1080 orthographic render, Shadow Isles era,
~4× the linear resolution of the minimap the blockout was traced from.

```
world_x = (px_x               - 959) * 0.2395
world_y = (px_row_from_bottom - 525) * 0.2395
```

Fitted by aligning the blockout's traced playable boundary against image
gradient magnitude (edge ratio 1.44 vs frame average, sharp scale peak; map
spans 1904 of 1920 px).

**Region/luminance correlation cannot fit scale.** The first attempt was flat
across ±8% — +8% scored *higher* than its own optimum — and the left-right
flipped null model scored identically (0.2822 vs 0.2823). Boundaries carry the
scale information; regions don't. Use an edge objective for this class of
registration.

**The wall trace validates against this 4× source — do not re-trace.** The
authored geometry was resampled at 3 yd and smoothed, so it is already coarser
than even the old source; and a 2 yd resample is known to break Blender's
boolean. A re-trace would also discard the hand-authored base geometry and the
verified entrance widths, neither of which came from tracing.

TT_Reference in the .blend carries this image, packed and registered to the transform above (459.84 × 258.66 yd). Unhide it to eyeball geometry against the reference.

## What the layout feeds — and doesn't

**Nothing, yet.** Verified 2026-08-11: nothing under `apps/` or `src/`
references `twisted_treeline`. The only map bundle is
`apps/moba/maps/eye_of_the_storm/`, whose configs and `*.lock.json` are
EotS-prototype placeholders in a *different* coordinate space.

So TT anchors have never been played and cost ~nothing to move. Do not argue
against moving them on "it's tuned" or "it's in the lock files" grounds — both
are false, and asserting them once manufactured a decision deadlock. The ~50s
lane pacing is arithmetic from blockout lane length, not a played-and-tuned
value.

**Mounts are allowed** (decided 2026-07-11) — players move 1.6–2× creep speed,
which is what the blockout's lane lengths were sized against.

The gate is `Spell.cpp:6673-6678`: battlegrounds allow mounts by default, but a
matching `instance_template` row **overwrites** that default outright
(`allowMount = it->AllowMount`, not an `&&`). So mounts in the MOBA hinge on
whether `instance_template` has a row for map 566 and what its `allowMount`
says — check with
`SELECT allowMount FROM instance_template WHERE map = 566;`. If it is 0, a
one-row custom SQL fixes it; nothing in `BattlegroundMOBA` needs to change.

A dismount-on-tower-hit rule is likely unnecessary: taking damage already
strips `SPELL_AURA_MOUNTED` through normal interrupt handling. Test before
building it.

What **is** verified is the *wall geometry*: reachability flood-fill, entrance
widths (29.2 / 26.3 / 28.8 / 26.1 yd), the plugged-mouth isolation proof.
Anchors sit in open space; moving them disturbs none of it.

## Measured structure positions (2026-08-11)

Read off a 20 yd world grid projected onto the reference, ±3 yd, east side:

| Structure in the render | Measured | Blockout anchor | Δ |
|---|---|---|---|
| Large platform, bright core, deepest in base | (192, +2) | `nexus_great_tower` (185, −5) | ~9 yd |
| Ornate dais wrapped by curved walls | (136, +3) | `altar` (152.5, −5) | ~18 yd |
| Glowing pad, north | (160, +34) | `nexus_turret` (172, +12) | ~25 yd |
| Glowing pad, south | (164, −35) | `nexus_turret` (172, −22) | ~15 yd |
| Dark pad, far north | (155, +60) | — | — |
| Dark pad, far south | (167, −66) | — | — |

Mid-map: **TT's real Altars of Harmony sit at (±69, −18)** — the blockout has no
anchor for them, and `BattlegroundMOBA` has no altar mechanic. Park until the
feature exists. Central shrine at (0, −18); brush tufts at (±36, −12).

The earlier reading that the minimap's cyan diamonds were the altars was a
512×512 misread — they are the Nexus daises.

Two caveats: every measured structure reads ~7 yd north of the blockout's
y = −5, which is far likelier a small `cy` registration bias than six
independent placement errors — **do not apply it as a correction**. And the role
labels are interpretation, not measurement; the positions are solid, the names
are a proposal.

Lane towers and camps are **not yet measured**.

## Doodad placement rule, derived from the reference

TT keeps the playable floor deliberately clear — all visual mass rides the
walls. Every zone below is derivable from the existing `TT_JWall_*` / `TT_Walls`
polygons by inset/outset, so the image supplies character and spacing, not
positions.

| Zone | Derivation | Contents |
|---|---|---|
| Floor interior | >3 yd from any wall | nothing |
| Wall-base hem | 0–2 yd inward from wall edge | sparse low growth |
| Wall edge line | wall perimeter, playable side | **lanterns/braziers, ~15–20 yd spacing** |
| Wall top | wall polygon interior | dead trees, fungus, roots — dense |
| Outer dead zone | outside the boundary loop | largest backdrop masses |
| Landmarks | hand-placed | centre shrine, camps, boss mouth, nexus |

Floor exceptions are landmark-anchored only — never free-standing in a lane.
Nothing gameplay-load-bearing may be a doodad: a doodad's collision depends on
an asset we don't control, and its group being dropped by `WMOGroup::ShouldSkip`
takes its collision with it.

**Timing: dressing is a post-step-6 pass.** Doodads are polish by the rule
above, and each change costs a re-export → re-pack → re-extract cycle, so that
loop wants proving first. The single test doodad pulled forward into step 2
confirmed the export half of the chain (see *Step 2 — complete*); the MPQ and
`Doodad::ExtractSet` halves are still unproven until steps 4–5 run.

Check any candidate asset with `mpq_tool.py probe` before authoring with it:
**0 bounding triangles renders and never collides, anything above 0 always
collides.** There is no third option, which is why brush cannot be a doodad.

## Editing the blockout — hard-won constraints

Live the moment step 1 touches the scene:

- **Keep the outline cutter resampled at 3.0 yd.** A 2.0 yd resample
  (~4500-vert cutter) makes Blender's EXACT boolean *silently destroy* the slab
  — it went to 0 verts once. Sanity-check vertex counts after every apply.
- **Cap carves must take their lane-side control points FROM the traced wall**
  (3 anchor points, not raw point splicing), or the union seam forms a bad
  corner.
- **Verification kit that works:** 2 yd flood grid (3 yd aliases through
  diagonal walls and false-fails); plugged-mouth test with plugs spanning the
  FULL map height (a y−100 plug missed lane floor at y=−102); check the seed
  cell is actually open; and when a proof fails, BFS-with-parents to print the
  actual leak path before touching geometry.
- Verify every terrain change with the ray-cast flood-fill (seed a jungle camp;
  probe graveyards / nexus / altar / boss / lane mids) **plus** seal probes at
  the front-wall midpoints. The flood caught 2 disconnections; the seal probes
  caught a hairpin wall that left flanks open.
- `TT_Patch_*` (knob patches) and `TT_FWFill_*` (front-wall fillers) are
  deliberate geometry — don't "clean" them.

## Export mechanics — proven end to end 2026-08-12

**Collision defaults to OFF.** WBS's batcher sets `F_DETAIL` on any triangle
whose three vertices aren't all in a vertex group named `"Collision"`
(`_is_vertex_collidable` returns false outright when the group is absent).
Our extractor then drops it: `isRenderFace = RENDER && !DETAIL`, and a
non-render non-collision face is skipped (`wmo.cpp:404-408`). Export the
blockout without that vertex group and the map has **zero** collision — it
looks perfect in the client, players fall through the floor, mmaps builds
nothing. Measured on the real export: 16,476 of 32,620 triangles survive as
collision, matching the Blender-side collide/render split exactly.

Collision is authored one of two ways: a `"Collision"` vertex group covering
every vertex (assigned to `wow_wmo_vertex_info.vertex_group`), or a separate
invisible mesh in WBS's `Collision` collection referenced by the group's
`collision_mesh` pointer (that path writes `F_COLLISION` + material id `0xFF`).

**One Blender mesh object = one WMO group.** Sorting is by membership in WBS's
special collections: `Outdoor` / `Indoor` / `Collision` / `Portals` / `Lights`
/ `Doodads` / `Liquids` / `Fogs`.

Ngons need no pre-triangulation — the batcher consumes Blender loop triangles,
and every ngon in the blockout is planar to 2e-5 yd.

`walkableClimb: 6` cells ~= 1.6 yd (`mmaps-config.yaml:50`), so the 1 yd nexus
plateaus are climbable as collision. `skipBattlegrounds` defaults to false.

### Four ways the export fails that the file itself won't show you

Each of these was hit for real. `apps/moba/wmo/blender_preflight.py` checks all
four; run it before every export rather than rediscovering them.

- **A material with no `diff_texture_1` raises** `ReferenceError` in
  `save_materials`. Loading a BLP into the scene does *not* assign it — that is
  a second, separate step, and skipping it is invisible until export.
- **The root collection is resolved from `bpy.context.collection`**, i.e.
  whatever is selected in the Outliner. Select the Scene Collection and
  `get_current_wow_model_collection` returns `None`, and `save_root_header`
  dies on it.
- **`build_references` skips hidden objects** (`group_object.hide_get()`). A
  hidden group silently does not ship, taking its collision with it.
- **Every group mesh needs a UV layer named exactly `UVMap`**, or
  `create_batching_parameters` raises.

A doodad additionally needs `wow_wmo_doodad.enabled = True`, or WBS's depsgraph
handler evicts it from the set collection for "not matching required custom
object types". Do **not** set its `color`: `update_doodad_color` indexes
`mat.node_tree.nodes['DoodadColor']` and `KeyError`s on a plain material.

**No BLP export is needed at all.** MOTX stores paths only, and the eight
chosen textures are stock Blizzard assets, so the client patch carries geometry
and DBCs but no textures. This holds for any map dressed from stock assets.

## Toolchain — the two version traps

WBS targets **Blender 3.4** (`bl_info` `(3, 4, 0)`, all releases tagged `3.4-*`,
WoW 3.3.5 supported). Prebuilt releases are **Windows-only** — CI runs
`C:\Python310\python.exe io_scene_wmo/build.py` with no macOS job. On Apple
Silicon, `wbs_kernel` must be compiled locally: one Cython extension
(`wmo_utils`) over 6 C++17 files, GLM and Blender headers vendored, and
`setup.py` has an explicit Darwin branch. Needs Python 3.10 to match Blender
3.4's embedded interpreter. Clone with `--recurse-submodules` — `pywowlib` is a
submodule.

**The .blend cannot go backward.** Written with 5.1.30; Blender is not forward
compatible. 5.1 stays the source of truth. Prep happens in 5.1, then the 47
objects in `TT_WMO_Export` transfer as OBJ (Forward=Y, Up=Z) into a disposable
3.4 staging file where only the WBS assignment happens. Verify the transfer
against known bounds: `TT_Walls` +/-235/+/-120/0-8, `TT_Ground` +/-245/+/-130/-1-0.

### Building WBS on Apple Silicon — five patches, 2026-08-11

Blender 3.4.1 (`blender-3.4.1-macos-arm64.dmg`, installed as
`/Applications/Blender-3.4.app` — do NOT let it replace the 5.1 `Blender.app`)
embeds Python 3.10.8. Build with Homebrew `python@3.10`; ABI is stable across
3.10.x. Clone at `~/tools/blender-wow-studio`, dist at `~/tools/wbs-dist`.

Every failure was 2022-era code meeting a 2026 toolchain — none was Apple
Silicon or addon logic, and `wbs_kernel` compiled first try. Re-apply all five
on a fresh clone:

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

## Step 1 — complete 2026-08-11

### 5.1 blockout prep

- UVs on all 34 terrain meshes (they had none) — cube projection, world-space,
  1 repeat / 8 yd. Tiling, not a packed 0–1 atlas.
- `TT_Walls` empty material slot removed (0 faces referenced it; all 1673 on
  `TT_Wall`). A slot with no material is an export hazard.
- Collection `TT_WMO_Export`: the 47 shipping objects, **linked** not moved.
  32 collide + 15 render-only, 32,620 triangles.
- Custom property `wmo_collide` on each — 1 = collide, 0 = render-only. **Does
  not survive OBJ**; the names are listed below.
- Scale applied on `TT_Camp_WraithsE/W` — the only unapplied scale in the set.

Render-only by choice: the 3 path decals + canopy, and the 11 thin pads
(GY / altar / pit / camp) sitting 0.4–0.5 yd over the floor, where collision
would stack near-coincident surfaces. Both nexus plateaus collide.

The 32 needing a `"Collision"` vertex group: `TT_Ground`, `TT_Walls`,
`TT_JWall_00`–`15`, `TT_FrontWall_E/W`, `TT_FrontWall_EPocket/WPocket`,
`TT_FWFill_EN/ES/WN/WS`, `TT_Patch_EN/ES/WN/WS`, `TT_Blue_NexusPlateau`,
`TT_Red_NexusPlateau`. Everything else is render-only — the engine default,
needing no action.

### 3.4 staging scene

`~/tools/tt-transfer/tt_staging_34.blend` is disposable; the 5.1 file stays
source of truth. To rebuild: re-export the OBJ from 5.1, re-import, run
`apps/moba/wmo/blender_staging_setup.py`.

**The setup exists only in RAM until the .blend is saved.** A whole session's
WBS assignment was found missing from the file on disk while still live in the
open Blender instance — the geometry import had been saved, the setup had not.
Inspecting the .blend from 5.1 is the way to tell the two apart: vertex groups
are real mesh data and survive without the addon registered, so
`0 objects with vertex groups` is proof the collision pass is not in the file,
whatever the running session shows.

- Transferred via `~/tools/tt-transfer/tt_blockout.obj` — 47 objects, 8
  materials, 32,620 tris, bounds byte-exact. **Exporter and importer axis
  defaults differ**; both need Forward Y / Up Z. The importer's default
  (−Z/Y) silently rotates −90° about X. Caught only by the `TT_Walls`
  bounds check — always run it after transfer.
- WMO root = collection `TwistedTreeline` (`wow_wmo.enabled = True`,
  `dir_path = World\wmo\TwistedTreeline\`); child collection `Outdoor` holds
  all 47. **Group membership is collection membership and nothing else** —
  `wow_wmo_group` has no `enabled` property in this WBS revision, and
  `get_wmo_groups_list` reads the `Outdoor`/`Indoor` collections directly. (An
  earlier note here claimed a three-part condition including an `enabled` flag;
  that was wrong.)
- `"Collision"` vertex group covering every vertex on the 32 collide objects,
  assigned to `wow_wmo_vertex_info.vertex_group`. The 15 render-only ones have
  it explicitly removed.
- All 8 materials carry `diff_texture_1`. Export takes the path from
  `image.wow_wmo_texture.path`, **not** the image filepath
  (`wmo_scene.py:539`). Store paths lowercase — the BLP→PNG step does a
  `.replace('.blp', ...)` that misses an uppercase `.BLP`.

**Tooling limits.** The Blender MCP bridge is extensions-format
(`blender_version_min 5.1.0`, no `bl_info`), so 3.4 cannot be driven
programmatically — scripts go into its Text Editor and output lands in the
launching terminal. WBS also *crashes* Blender 3.4 under `--background` (dies
in `auto_load` at `ui/operators.py:557`), so headless verification requires
`--factory-startup`, limiting it to geometry checks only.

### Texture assignment — decided 2026-08-11, all paths verified in-client

Indexed via pywowlib/StormLib against `~/Games/wow335/Data` (7 MPQs, 210,782
entries — 110,419 BLP, 25,113 M2). The MPQs **do** carry internal listfiles, so
asset paths are enumerable rather than guesswork. Ghostlands lives under
`TILESET/EXPANSION01/GHOSTLANDS/`; there is no ICECROWN tileset (WotLK ground is
`TILESET/EXPANSION02/`). A `_s` suffix is a specular map — never assign one as
diffuse.

| Material | BLP |
|---|---|
| `TT_Wall` | `TILESET/EXPANSION01/GHOSTLANDS/GhostLandsRock01.blp` |
| `TT_Ground` | `TILESET/EXPANSION01/GHOSTLANDS/GhostLandsGrass01.blp` |
| `TT_Lane` | `TILESET/EXPANSION01/GHOSTLANDS/GhostlandsPath01.blp` |
| `TT_JunglePath` | `TILESET/EXPANSION01/GHOSTLANDS/GHOSTLANDSDIRT01.BLP` |
| `TT_Camp` | `TILESET/EXPANSION01/GHOSTLANDS/GhostlandsCreep01.blp` |
| `TT_Stone` | `TILESET/DUSKWOOD/DuskwoodCobblestone.blp` |
| `TT_Pine` | `World/AZEROTH/DUSKWOOD/PASSIVEDOODADS/Trees/DuskTallCanopy_New03.blp` |
| `TT_PitBoss` | `TILESET/PlagueLands/PlaguedEarthRed01.blp` |

**Naxxramas yields no architecture textures** — all 23 hits are capes, shields
and weapons. Bone/spike work comes from `DUNGEONS/TEXTURES/` instead:
`BRICK/JACRYPTBRICK01-08`, `AZJOL/AZJOLBONEPILE`,
`DECORATION/JLO_UDERCITY_SKULL`, `DALARAN/DAL_ROCK_SPIKE`.

## Step 2 — complete 2026-08-12

`~/tools/wbs-project/World/wmo/TwistedTreeline/TwistedTreeline.wmo` + 47 group
files, exported from the 3.4 staging scene via File > Export > WMO (Full, not
"selected"). Checked by `apps/moba/wmo/wmo_verify.py`, which replays the
extractor's own decisions offline:

| | |
|---|---|
| MVER | 17, root and every group — WBS hardcodes it, so format version was never a risk |
| groups kept by `ShouldSkip` | 47 / 47 |
| triangles | 32,620 total, **16,476 kept as collision** |
| materials | 8, every MOTX path resolving in the client MPQs |
| doodad | 1 emitted through the full `MODN -> MODD -> MODR -> ExtractSet` chain |

The 15 groups contributing no collision are exactly the 15 render-only objects.

**The doodad pipeline works.** `BE_Lamppost_Ghostlands01` (570 bounding
triangles) exports with its path rewritten `.m2 -> .MDX` by WBS, lands in
`Set_$DefaultGlobal`, and `find_nearest_object` attached its MODR reference to
the `TT_Ground` group — which correspondingly gained flag `0x800`. So the
dressing pass is unblocked on the tooling side.

### Two things step 3 inherits

- **`rootWMOID` is 0.** That is `MOHD.id`, the `WMOAreaTable` foreign key, left
  at zero because `wow_wmo.wmo_id` was never set. The extractor writes it
  straight into the vmap root. Choosing the area id means a re-export, so
  decide it before doing much else.
- **`MOHD.flags = 0x4`** (`UseLiquidTypeDBCId`). WBS sets this unconditionally
  for WotLK when no group carries a liquid mesh. Harmless here —
  `GetLiquidTypeId(0)` returns 0, so `liquflags` stays 0 — but it is the first
  thing to look at if water ever appears where it shouldn't.

## Pipeline (each step is its own session-sized chunk)

| # | Step | Who | Notes / unknowns |
|---|------|-----|------------------|
| 1 | Prep scene for export: separate collision geometry, assign materials, split into WMO groups, confirm scale/axis | Claude drives, Jacob runs Blender | 1u=1yd is already correct for WoW. Groups are the culling unit **and** the collision unit — `ShouldSkip` drops any group flagged unreachable (0x80) or antiportal (0x4000000), silently |
| 2 | ~~Export to `.wmo`~~ **done 2026-08-12** | — | see *Step 2 — complete* |
| 3 | Build the WMO-only map: WDT with global-WMO flag; DBC rows — `Map.dbc` (new map + directory), `WorldSafeLocs.dbc` (graveyards), likely `AreaTable`/`WMOAreaTable` | Claude (as file/DBC edits) | exact columns unverified — research when we get here |
| 4 | Pack `.wmo` + WDT + DBCs into an MPQ client patch | Jacob | client can't load the map without it. **No textures** — all 8 are stock assets referenced by path |
| 5 | Run extractors server-side: `mapextractor`, `vmap4extractor` + `vmap4assembler`, `mmaps_generator` | Jacob | **mmaps is load-bearing** — creep pathfinding needs the navmesh built from WMO collision. Doodads inside a global WMO *are* extracted (`WDTFile` MODF branch calls `Doodad::ExtractSet`) |
| 6 | Register the map server-side + point `BattlegroundMOBA` at the new map id | Claude (C++/SQL) | ties into existing BG code; also gets the standalone BG id + `BattlemasterList.dbc` client patch that was always planned |

## Division of labor (per CLAUDE.md env rules)

- **Claude does:** Blender scene prep, DBC/SQL/C++ authored as files.
- **Jacob does:** install the exporter addon, MPQ packing, run the extractors,
  restart servers — everything that operates the environment.

## Loose ends found 2026-08-11

- `TT_Forest` exists at z 8→16.9, 2772 verts, but covers only ~6% of wall area
  and overhangs 0.28% of playable — a partial canopy pass, already obeying the
  "props ride the walls" rule.
- 13 stray open cells at the extreme south row (y = −109.5, x −136..+142) —
  boolean slivers or genuine pinholes in the outer wall. Worth a look given the
  flood-fill history.

## Next concrete step

Step 3: the WDT + DBC rows. Decide `rootWMOID` first — it is stamped into the
WMO itself, so changing it later costs a re-export (see *Two things step 3
inherits*).

Nothing downstream has been exercised yet: the `.wmo` has never been inside an
MPQ, no extractor has run against it, and `var/extractors/{dbc,maps,mmaps,vmaps}`
are empty — which means the MOBA prototype has been running with no vmaps or
mmaps at all. Worth understanding what that implies for current creep pathing
before assuming the new map will behave differently.
