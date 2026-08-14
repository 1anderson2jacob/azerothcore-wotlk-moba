# Custom map: Twisted Treeline terrain via WMO

## Status of this doc

Steps 1 through 4 are complete. Steps 5–6 remain **unverified against tooling or
engine source** — extractor behaviour on a WMO-only map, then registering it
server-side. Treat each as a thing to prove in its own focused session.

**This file is scheduled for deletion when the map ships.** Anything here that
is not specific to Twisted Treeline belongs in `apps/moba/wmo/README.md`, which
survives — the procedure, the toolchain, the export traps, the coordinate chain
and the WDT/DBC mechanics have already moved. As each remaining step is proven,
move its *how* there and leave only the decisions and results here.

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
  collision wall. Check any candidate with `mpq_tool.py probe`.
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

**Dressing is also what ends the staging scene's disposability** — placements
exist only in the 3.4 file and cannot round-trip through the OBJ. Decide how
they are stored *before* authoring hundreds of them; see the step 3 notes in
`apps/moba/wmo/README.md`.

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

## Export mechanics

**Moved to `apps/moba/wmo/README.md`** — the collision chain, the four silent
export failures, and the doodad rules are map-agnostic and must outlive this
file. Measured on the real export: 16,476 of 32,620 triangles survive as
collision, matching the Blender-side collide/render split exactly.

## Toolchain

**Moved to `apps/moba/wmo/README.md`** — the two version traps, the five Apple
Silicon build patches, and what cannot be automated. None of it is specific to
this map.

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

`~/tools/tt-transfer/tt_staging_34.blend`, built from
`~/tools/tt-transfer/tt_blockout.obj` — 47 objects, 8 materials, 32,620 tris,
bounds byte-exact. Disposable *while it holds no doodads*, and proven so:
`blender_staging_setup.py` reproduces it from the OBJ (see the README).

WMO root = collection `TwistedTreeline` (`dir_path = World\wmo\TwistedTreeline\`),
child collection `Outdoor` holds all 47. The `"Collision"` vertex group covers
every vertex on the 32 collide objects; the 15 render-only ones have it
removed. All 8 materials carry `diff_texture_1`, pointing at the paths in
*Texture assignment* below.

The procedure, the axis trap, the save-vs-RAM hazard and the export
preconditions are in `apps/moba/wmo/README.md`.

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

## Step 3 — complete 2026-08-13

### IDs, decided

| What | Value | Why this one |
|---|---|---|
| Map id | **900** | stock Map.dbc tops out at 724. Deliberately not the fork's usual 900000 range: `DBCStorage` sizes its index table to max(id)+1, so a six-digit map id costs ~7 MB of null pointers |
| Directory | **TwistedTreeline** | must match the WDT folder and filename; the core never reads it (`MapEntryfmt` marks field 1 `x`), the client and both extractors do |
| rootWMOID | **9000** | stock WMOIDs top out at 5949. Must stay <= 32767: `GetWMOAreaTableEntryByTripple` narrows the key to `int16` |
| AreaTable id | **5000** | free; AreaBit **3000**, also free — AreaBit indexes the client's exploration bitmask, so reusing a stock one marks another zone explored |
| WMOAreaTable rows | **51200, 51201** | stock tops out at 51118 |
| Light id | **3000** | stock tops out at 2538 |

Leaving rootWMOID at 0 would have collided with stock row 47479
(WMOID 0, NameSet 0, group 0 → AreaTableID 0).

Area flags are `AREA_FLAG_OUTSIDE` (0x04000000), deliberately **not** Eye of the
Storm's 0x4000 — that is `AREA_FLAG_OUTLAND2`, which no line of the core reads.

### Results

The re-export changed nothing but what it was meant to. Per-group triangle,
collision, vertex and doodad-reference counts are identical to the pre-rotation
export; only MOBN/MOBR moved.

| | |
|---|---|
| groups kept by `ShouldSkip` | 47 / 47 |
| triangles | 32,620, **16,476 kept as collision** |
| rootWMOID | 9000 |
| orientation | `TT_AltarEast_Pad` at x = −152.5, i.e. the server frame |
| WDT | 32,954 bytes; MVER 18, MPHD 0x1, MAIN with no exist bits, MODF at (0,0,0) |
| server extent | X[−245, 245] Y[−130, 130] Z[−1, 16.89] — **4 mmaps tiles** |
| DBC rows | Map 900, AreaTable 5000, WMOAreaTable 51200/51201, Light 3000 |

**`MOHD.flags = 0x4`** (`UseLiquidTypeDBCId`) is still set — WBS does this
unconditionally for WotLK when no group carries a liquid mesh. Harmless, since
`GetLiquidTypeId(0)` returns 0 and `liquflags` stays 0, but it is the first
thing to look at if water ever appears where it should not.

One export defect, client-side only: **`MOGI[0]` carries the root bounding box**
instead of its own group's. The group *file* is correct and the server reads
group files, so the only effect is that one group is never frustum-culled.

## Step 4 — complete 2026-08-13

`~/Games/wow335/Data/enUS/patch-enUS-4.MPQ` — 1,111,788 bytes from 4,965,170 in,
built by `apps/moba/wmo/mpq_pack`. Why the locale archive rather than
`Data/patch-4.MPQ` is in `apps/moba/wmo/README.md` step 7; it is map-agnostic and
not a per-map choice.

| | |
|---|---|
| entries | 54 — 53 payload + `(listfile)` |
| fidelity | 53 / 53 byte-identical to `~/tools/wbs-project` |
| DBC rows in-archive | Map 900 (`Directory='TwistedTreeline'`, InstanceType 3, Flags 0x1), AreaTable 5000, WMOAreaTable 51200/51201, Light 3000; map 566 still intact |
| WDT | MVER 18, MPHD 0x1, MAIN 32768, MWMO 46, MODF 64 |
| MWMO -> WMO | `World\wmo\TwistedTreeline\TwistedTreeline.wmo` resolves in-archive |
| root MOHD | nTextures 8, nGroups 47, RootWMOID 9000, bbox X[−245, 245] Y[−130, 130] Z[−1, 16.89] |
| derived groups | 47 / 47 present, `_047` correctly absent |

Both extractors' archive search order was replayed against the real install: all
six of our files resolve to `patch-enUS-4.MPQ` in each, with no divergence.

### What step 5 inherits

**The extractors are not built.** `var/build/obj/CMakeCache.txt` carries
`TOOLS_BUILD=none`, and `env/dist/bin/` holds only `authserver` and
`worldserver` — the installed map data came from AzerothCore's prebuilt v19
release, never from a local run. So step 5 opens with a reconfigure:

```bash
cd var/build/obj
cmake -DTOOLS_BUILD=maps-only .
make -j$(sysctl -n hw.ncpu)
make install
```

`maps-only` whitelists exactly the four needed. The binaries keep their source
directory names lowercased — **`map_extractor`, `vmap4_extractor`,
`vmap4_assembler`, `mmaps_generator`**, with underscores, not the `mapextractor` /
`vmap4extractor` spellings used in upstream release archives. `make install` puts
them plus `mmaps-config.yaml` into `env/dist/bin/`.

**Format compatibility is already confirmed**, so new files can sit beside the
v19 set instead of forcing a full regeneration: the source's `VMAP_MAGIC` is
`VMAP_4.8`, matching the header of the installed `369.vmtree`, and an installed
`.mmtile` reads `mmapVersion 19` / `dtVersion 7`, matching `MMAP_VERSION` and
`DT_NAVMESH_VERSION` in `MapDefines.h`.

#### What a WMO-only map produces

Measured against the three stock global-WMO maps already installed here —
DeeprunTram (369), StormwindPrison (035), AlliancePVPBarracks (449): **0 files in
`maps/`, one `.vmtree` and no `.vmtile` in `vmaps/`, 5–13 files in `mmaps/`.** A
global WMO's spawn lives in the `.vmtree` itself; the model sits beside it as
`Subway.wmo.vmo` / `Stormwindprison.wmo.vmo` — basename, first letter capitalised
only.

Map 900's whole footprint should therefore be:

- `vmaps/900.vmtree`
- `vmaps/Twistedtreeline.wmo.vmo`
- `mmaps/900.mmap` + `900*.mmtile` — the WDT predicts 4 tiles
- nothing in `maps/`

#### `map_extractor` is not needed

It has no per-map flag (`-e` picks MAP/DBC/Camera, nothing finer), it would
reprocess all 5744 tiles, and a WMO-only map yields no `.map` files anyway. Map
900 reaches the server through `mod_moba_map.sql` and the `*_dbc` override
tables, which is what those tables are for.

**Decided 2026-08-13 — leave the extracted DBCs alone.** `mod_moba_map.sql` is
the only server-side source and stays that way. All 112 DBC loads in
`DBCStores.cpp` have an override table, `pvpdifficulty_dbc` among them, so
nothing in step 6 or later forces a DBC file edit.

Two things decided it past mere convenience:

- **A SQL row survives a client-data bump; a patched file does not.**
  `inst_download_client_data` guards on `env/dist/bin/data-version` — `v19` today,
  matching its own hardcoded `VERSION`, so it is currently a no-op. When upstream
  bumps that version the guard fails and it runs `unzip -o` over
  `env/dist/bin/`. DBC files are inside that archive and would silently revert to
  stock; map 900's `.vmtree`, `.vmo` and `.mmtile` are not in it and survive.
- **Editing SQL is one restart.** Editing a DBC is `dbc_tool.py` → `mpq_pack` →
  `map_extractor` → restart, for a value the client never reads.

The cost is that `env/dist/bin/dbc/Map.dbc` will never contain map 900, which
looks like a missing registration and is not. That is called out in
`mod_moba_map.sql` itself, so the confusion is answered where it arises.

#### Why not `apps/extractor/extractor.sh`

The repo ships a driver, and it is where the underscored binary names above are
confirmed. It is the wrong tool here: every one of its functions opens with
`rm -rf` over the target directories, and its `mmaps_generator` call passes no map
id — so even its cheapest option rebuilds every map from scratch, in the script's
own words "may take a few hours". It also guards on `[ -d "./Data" ]`, so it only
runs from the WoW folder and could never write into `env/dist/bin/` regardless.

One thing worth taking from it: `Couldn't open RootWmo!!!` on
`World\Wmo\Band\Final_Stage.wmo` during vmap extraction is **expected and
harmless** — the script prints a banner saying exactly that. Do not chase it.

#### The run

```bash
cd <empty scratch dir>
vmap4_extractor -d ~/Games/wow335/Data/    # -> ./Buildings, ./temp_gameobject_models
vmap4_assembler Buildings vmaps            # -> ./vmaps
cp vmaps/900.vmtree vmaps/Twistedtreeline.wmo.vmo <repo>/env/dist/bin/vmaps/
cd <repo>/env/dist/bin && ./mmaps_generator 900
```

Traps, all read out of the source rather than guessed:

- **`vmap4_extractor` refuses a non-empty output directory.** It stats
  `Buildings/dir` and `Buildings/dir_bin` and quits with "Your output directory
  seems to be polluted, please use an empty directory!". Start from an empty one.
- **The two extractors mean different things by their path argument.**
  `map_extractor -i` takes the game *root* and appends `/Data/` itself
  (`System.cpp:1169`); `vmap4_extractor -d` takes the *Data* directory
  (`vmapexport.cpp:240`). Same install, different string.
- **`900.vmtree` has to be in `vmaps/` before `mmaps_generator` runs.**
  `discoverTiles` picks a tile-less map up only through the `.vmtree`
  (`MapBuilder.cpp:116-125`); with no `.map` files that is the sole discovery path.
- `checkDirectories` wants `maps/` non-empty *globally* and `vmaps/` holding at
  least one `.vmtree`, so `mmaps_generator` must run from `env/dist/bin/`, not the
  scratch dir. It also needs `mmaps-config.yaml` in the CWD, or `--config`.
- `mmaps_generator` reads no DBCs — it neither knows nor cares that 900 is custom.
  It takes a bare map id as its positional argument.
- Copy only the two vmap files across. `vmap4_assembler` regenerates every map's
  `.vmo`, and `temp_gameobject_models` is absent from this install; nothing else
  under `env/dist/bin/` should change.

#### Done when

- `env/dist/bin/mmaps/900.mmap` plus ~4 `900*.mmtile` exist
- a new `.mmtile` header reads `mmapVersion 19`, `dtVersion 7`
- `git status` is clean — every artefact above lands outside the repo

## Pipeline (each step is its own session-sized chunk)

| # | Step | Who | Notes / unknowns |
|---|------|-----|------------------|
| 1 | Prep scene for export: separate collision geometry, assign materials, split into WMO groups, confirm scale/axis | Claude drives, Jacob runs Blender | 1u=1yd is already correct for WoW. Groups are the culling unit **and** the collision unit — `ShouldSkip` drops any group flagged unreachable (0x80) or antiportal (0x4000000), silently |
| 2 | ~~Export to `.wmo`~~ **done 2026-08-12** | — | see *Step 2 — complete* |
| 3 | ~~WDT + DBC rows~~ **done 2026-08-13** | — | see *Step 3 — complete* |
| 4 | ~~Pack `.wmo` + WDT + DBCs into an MPQ client patch~~ **done 2026-08-13** | — | see *Step 4 — complete* |
| 5 | Run extractors server-side: `vmap4_extractor` + `vmap4_assembler`, then `mmaps_generator 900` | Jacob | **mmaps is load-bearing** — creep pathfinding needs the navmesh built from WMO collision. Doodads inside a global WMO *are* extracted (`WDTFile` MODF branch calls `Doodad::ExtractSet`). The tools are not built yet — see *What step 5 inherits* |
| 6 | Register the map server-side + point `BattlegroundMOBA` at the new map id | Claude (C++/SQL) | `mod_moba_map.sql` already carries the DBC rows. Still needs `battleground_template.MapID`, and **`PvpDifficulty.dbc` rows keyed to map 900** or `GetBattlegroundBracketByLevel` returns null |

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

Step 5: build the map tools (`cmake -DTOOLS_BUILD=maps-only .`), then run
`vmap4_extractor` → `vmap4_assembler` → `mmaps_generator 900`, installing only map
900's output into `env/dist/bin/`. Full handoff in *What step 5 inherits*.

Two claims from earlier sessions were wrong and are corrected here:
`var/extractors/{dbc,maps,mmaps,vmaps}` holds nothing but `.gitkeep` and is
referenced nowhere, but the extractors **have** run — the data lives in
`env/dist/bin/` (248 DBCs, 5744 `.map`, 12494 vmap files, 3780 mmap files,
including 36/11/25 for map 566). The prototype has had full vmaps and mmaps all
along. New map data has to be installed there, not into `var/extractors/`.
