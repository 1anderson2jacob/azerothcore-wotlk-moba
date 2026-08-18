# Custom map: Twisted Treeline terrain via WMO

## Status

**Steps 0 and 1 are done** (2026-08-18). `python3 apps/moba/gen_blockout.py`
takes the painted trace to a finished blockout `.blend` in about 12 seconds.

**Step 2 will abort on its first run** — see *Step 2 is blocked* below. That is
the next work.

The bundle is `apps/moba/maps/twisted_treeline_v2/`. The pass-1 bundle stays in
place until v2 replaces it — see *Two bundles, one slot*.

**Delete this file when the map ships.** Map-agnostic procedure lives in
`apps/moba/wmo/README.md` and survives; steps 0 and 1 there were rewritten to
match what actually shipped.

## Why the restart

Pass 1 produced a playable map that missed on look. Four causes:

1. **Traced from a low-res minimap** — pass 1's `layout.json` records
   `traced_from: Riot Data Dragon 9.22.1 map10.png`, 512x512. Moot in v2: the
   trace is hand-painted, so source resolution no longer bounds accuracy.
2. **No dressing.** Step 9 never started; one doodad on the whole map.
   `TT_Forest` — 198 identical 7-sided frustums wearing a canopy BLP authored
   for alpha-tested cards — was a placeholder that shipped.
3. **Scale and proportion wrong.** Lanes too narrow, walls too short, one lane
   tower per lane where the reference shows two.
4. **Purple cast.** `dbc_tool.py` clones Eye of the Storm's `Light.dbc` row 591
   and `AreaTable` row 3820, so map 900 wears Netherstorm's lighting. Real, but
   **deferred** — not what kept the map from reading right.

**Worth separating:** only (3) is a tracing-accuracy failure. Pass 1's contour
was probably fine — supported in v2, where the painted trace's aspect (2.09)
landed within 1% of pass 1's `map_bounds` (2.07). Do not re-derive the
restart's justification from "the trace was wrong."

**The source `.blend` never contained step 1's prep** (verified 2026-08-15):
`TT_WMO_Export` absent, `wmo_collide` on 0 objects, 34 meshes without UVs.
Step 1's work existed only in an OBJ outside the repo. v2 fixes this
structurally — there is no hand-authored `.blend`, and `.gitignore` now
excludes `var/blender/*.blend` because it is a build artifact.

## Source of truth

`var/blender/tt_v2_trace.png` (hand-painted) + `map_source.yaml`.

`geometry.json`, `heights.png` and the `.blend` are **derived and regenerable**.
Nothing hand-authored may live only in a `.blend` again.

### Reference images

| File | What | Use |
|---|---|---|
| `var/blender/tt_v2_trace.png` | 752x752 hand paint | **the trace** |
| `var/blender/tt_minimap.png` | 752x752 LoL wiki minimap | what it was painted over |
| `var/blender/tt_topdown.webp` | 1920x1080 lit render | art direction only |

`TTmap.jpg` (2048x1068, LoL wiki) is higher-res art reference if wanted.
CommunityDragon holds raw patch dumps back to 7.1; `/9.22/game/assets/maps/` is
**not** the right path — it holds only kitpieces, lightmaps, particles, skyboxes.

## Decision ledger — 2026-08-15/18

| What | Value | Why |
|---|---|---|
| Scale | **432 yd map width** | gives 431.4 x 187.2 yd. *Smaller* than pass 1's 456 x 220, but the paint's lanes are proportionally wider (9.0% of map width vs 4.8%), so the ledger's original "lanes too narrow" goal is met at a smaller footprint. Comfortably 4 mmaps tiles |
| Lanes | **25-39 yd** | as painted. Supersedes the earlier 33/36 |
| Walls | 16 yd ±4 | varies 12-20 per ~70 yd segment, cosine-interpolated so no vertical seam |
| Wall construction | **extruded rings, no boolean** | islands are solid capped prisms; the outer loop is one ribbon whose outer edge is the convex hull pushed out 30 yd. Deletes the fragile step and the cutter-vertex ceiling with it. Rationale and the alternatives are in README step 1 |
| Wall silhouette | terraced + bevelled | 8 yd drop, 2.5 yd ledge, 0.75 yd chamfer — arithmetic on rings, not a face selection |
| Base platform | **3 yd** | must exceed `walkableClimb` 1.60 yd or the edge is climbable from any side, which is what pass 1's 1.0 yd nexus plateau was. Chosen against the painted 6.6 yd ramp run to land near 24 degrees; measured 25.8 |
| Base entrances | **~36 yd, not chokes** | pass 1's were 26.1-29.2 yd. Accepted as painted; defenders have no pinch point |
| Symmetry | left-right mirror | 0.9601 IoU on the paint (0.4373 on the render). Not enforced — the paint is taken as drawn |
| Tree mass | placeholder cones | seeded scatter on wall tops, flat grey, obviously placeholder; replaced by M2 doodads in dressing |
| Wall texture | off `TILESET/` | `GhostLandsRock01.blp` is a ground texture with no vertical strata. `DUNGEONS\TEXTURES\ROCK\` has 273 authored for vertical faces. Pick deferred — a material path, blocks nothing |
| Lane towers | 2 per lane per side | up from 1 |
| Lighting | deferred | real, not the blocker |
| Content bundle | unchanged design | the config YAMLs are a human-authored designer surface, not generated from geometry |

## What steps 0 and 1 produce

`python3 apps/moba/gen_blockout.py` (add `--stage trace` or `--stage build` to
run one half). Verified output:

```
trace   432.0 yd across 767 boundary points
        mirror IoU 0.9601, 49 ambiguous px, 0 specks dropped
        smoothing max deviation 0.74 yd, area -0.307%
        slope 25.8deg peak (limit 60.0)
build   18 objects, 19813 verts, 16486 faces
        floor 14565 verts, 10523 faces, 50287 yd2
        walls 16, 20 offset vertices clamped
        z range -8.0 .. 33.83 yd
```

- **Geometry:** 1 outer loop + 15 island loops. Extent x [-216.0, 215.4],
  y [-93.6, 93.6]. One connected walkable region.
- **Bases** at (-172.6, -1.4) and (172.0, -1.5), ~9100 yd² each.
- **Ramps:** four, run 6.0-7.4 yd, mouth 35.6-37.3 yd.
- **Objects:** `TT_Floor`, `TT_OuterWall`, `TT_JWall_00`…`TT_JWall_14`,
  `TT_Trees`.
- `floor_area 50287 yd²` against the loops' 50123 confirms the 15 islands are
  holes rather than floored over.

Two islands (~207 yd²) are too small to terrace and build as plain capped
prisms — reported as `walls_unterraced`, working as intended.

### Do not retry: uniform erosion to narrow lanes

Measured. The map's median floor half-width is 9 yd, so half the floor sits in
corridors 18 yd or narrower. Eroding 10 yd — the depth that hits a 38 yd lane
target — fragments the map into 5 disconnected regions and halves the ramps.
Lane width is a repaint or a scale change, never a filter.

## Step 2 is blocked

`blender_staging_setup.py:53` asserts the scene's `TT_*` mesh set matches
`SHIPPING` exactly and **raises `SystemExit` otherwise**. Its sets are pass 1's
names, so v2 aborts immediately. Three things need updating, none testable
without the GUI 3.4 session:

1. **`COLLIDE` / `RENDER_ONLY`.** v2 ships 18 objects: `TT_Floor`,
   `TT_OuterWall` and `TT_JWall_00..14` collide; `TT_Trees` is render-only.
   Pass 1's sets name `TT_Ground`, `TT_Walls`, `TT_FrontWall_*`, `TT_FWFill_*`,
   `TT_Patch_*`, the nexus plateaus, `TT_JWall_00..15` (sixteen, not fifteen),
   and a pile of `_Pad` and `_Camp_` objects that no longer exist.
2. **`ORIENT_PROBE`** is `TT_AltarEast_Pad` at x = 152.5, which v2 does not
   have. It exists to verify the server-frame flip, so it needs a v2 object
   with a known centre x.
3. **`TEXTURES`** has no entry for `TT_Floor` or `TT_Tree` (v2's material
   names), so the material check errors. `TT_Wall` already maps.

**Collide vs render-only travels by object name**, not by property — custom
properties do not survive OBJ (README step 2). `build_blockout.py` sets a
`wmo_collide` custom property, but only to drive its own gate; it never reaches
3.4.

## Two bundles, one slot

`apps/moba/maps/twisted_treeline/` (pass 1) and `twisted_treeline_v2` both
target map 900. Before v2 gains content configs, settle:

- **`active:` in `base_config.yaml`** — one bundle owns the battleground slot.
  Two with `active: true` will conflict.
- **ID allocation.** Pass 1 filled 900000-900009 (towers), 900020+ (creeps),
  900209+ (neutrals), and the allocator carved 900400-900499. v2 either reuses
  those keys or needs its own block in `apps/moba/id_blocks.json`.

Neither is decided.

## What carries over untouched

Steps 3-8 are tooling and none of it cares about the geometry:
`blender_preflight.py`, `wmo_verify.py`, `gen_wdt.py`, `dbc_tool.py`,
`mpq_pack`, `mpq_tool.py`, and the extractor procedure. Only
`blender_staging_setup.py` (step 2/3) carries per-map names and needs the edit
above.

DBC and ID allocations live in `dbc_tool.py` as constants, reasoning in
`apps/moba/wmo/README.md` step 6. `mod_moba_bg_map.sql` is unaffected by geometry.

## Structure positions come from in-game `.gps`, not from geometry

Lane centrelines, tower and camp positions are **not** derived from the trace,
and a painted marker layer was designed and rejected:

- The configs carry z; a flat image cannot.
- `lane_config.yaml`'s header records a traced waypoint that sat 0.2 yd from
  `TT_JWall_12`'s face and had to be hand-repaired. A point recorded by standing
  on it cannot have that defect.
- You cannot `.gps` a map that does not exist yet, so geometry comes first.

`layout.json` is therefore dead for v2 — it held exactly those positions.

## Pipeline

| # | Step | State |
|---|---|---|
| 0 | Trace the boundary | **done** — hand paint, read by `gen_blockout.py` |
| 1 | Blockout | **done** — `build_blockout.py`, no booleans |
| 2 | Transfer to 3.4 staging | **blocked** — see *Step 2 is blocked* |
| 3-8 | staging -> export -> verify -> WDT/DBC -> MPQ -> extractors | proven, unchanged |
| 9 | Dressing | blocked on where doodad placements are stored (README step 3) |

## Division of labor

- **Claude:** scripts, SQL and C++ authored as files **for Jacob to apply**.
  Claude does not write files. Steps 0-1 run headless, so no Blender GUI session
  is needed for them.
- **Jacob:** applies files, runs the generators, the GUI 3.4 session, MPQ
  packing, extractors, servers.

## Open items

- **The ribbon walls are untested through the exporter.** Pass 1 only ever
  pushed boolean output through WBS. Step 4 is the first place the new topology
  could surprise us.
- **The WMO 16-bit vertex cap is asserted, not verified.** `wmo_verify.py` does
  not parse MOVI and the 65535 hits in WBS are Cython boilerplate. The gate in
  `gen_blockout.py` assumes 65535; nothing local confirms it.
- **Three failure gates in `gen_blockout.py` are unexercised** —
  `floor_regions`, slope, and `base_yd` below `walkableClimb`. The happy path is
  verified end to end; those branches are not.
- Cones overhang island edges slightly. Placeholder geometry that dressing
  replaces, so left alone.

## Next concrete step

Update `blender_staging_setup.py`'s `COLLIDE`, `RENDER_ONLY`, `ORIENT_PROBE`
and `TEXTURES` for v2's 18 objects, then run step 2 in the GUI 3.4 session.
