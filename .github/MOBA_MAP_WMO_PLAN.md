# Custom map: Twisted Treeline terrain via WMO

## Status

**Steps 0–9 are done** (2026-08-18). The v2 map is traced, built, exported,
packed, extracted and walkable on map 900. The bundle is
`apps/moba/maps/twisted_treeline_v2/`.

What remains is dressing, two undecided bundle questions, and lighting.

**Delete this file when those land.** The procedure itself is not here — it
lives in `apps/moba/wmo/README.md`, which is map-agnostic and survives.

## Why the restart

Pass 1 produced a playable map that missed on look. Recorded so it is not
re-derived, and specifically so it is not re-derived as "the trace was wrong":

1. **No dressing.** One doodad on the whole map. `TT_Forest` — 198 identical
   7-sided frustums wearing a canopy BLP authored for alpha-tested cards — was
   a placeholder that shipped.
2. **Scale and proportion wrong.** Lanes too narrow, walls too short, one lane
   tower per lane where the reference shows two. This is the only
   tracing-accuracy failure of the four.
3. **Purple cast.** `dbc_tool.py` clones Eye of the Storm's `Light.dbc` row 591
   and `AreaTable` row 3820, so map 900 wears Netherstorm's lighting.
4. **Traced from a low-res minimap.** Moot in v2 — the trace is hand-painted,
   so source resolution no longer bounds accuracy.

Pass 1's contour was probably fine: v2's painted aspect (2.09) landed within 1%
of pass 1's `map_bounds` (2.07).

**The source `.blend` never contained step 1's prep** (verified 2026-08-15):
`TT_WMO_Export` absent, `wmo_collide` on 0 objects, 34 meshes without UVs.
Step 1's work existed only in an OBJ outside the repo. v2 fixes this
structurally — there is no hand-authored `.blend`, and `.gitignore` excludes
`var/blender/*.blend` because it is a build artifact.

## Source of truth

`var/blender/tt_v2_trace.png` (hand-painted) + `map_source.yaml`.

`geometry.json`, `heights.png` and the `.blend` are **derived and regenerable**.
Nothing hand-authored may live only in a `.blend` again.

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
| Scale | **432 yd map width** | gives 431.4 x 187.2 yd. *Smaller* than pass 1's 456 x 220, but the paint's lanes are proportionally wider (9.0% of map width vs 4.8%), so the "lanes too narrow" goal is met at a smaller footprint. Comfortably 4 mmaps tiles |
| Lanes | **25-39 yd** | as painted. Supersedes the earlier 33/36 |
| Walls | 16 yd ±4 | varies 12-20 per ~70 yd segment, cosine-interpolated so no vertical seam |
| Wall construction | **extruded rings, no boolean** | islands are solid capped prisms; the outer loop is one ribbon whose outer edge is the convex hull pushed out 30 yd. Rationale and alternatives in README step 1 |
| Wall silhouette | terraced + bevelled | 8 yd drop, 2.5 yd ledge, 0.75 yd chamfer — arithmetic on rings, not a face selection |
| Base platform | **3 yd** | must exceed `walkableClimb` 1.60 yd or the edge is climbable from any side, which is what pass 1's 1.0 yd nexus plateau was. Measured 25.8 degrees against the painted ramp run |
| Base entrances | **~36 yd, not chokes** | pass 1's were 26.1-29.2 yd. Accepted as painted; defenders have no pinch point |
| Symmetry | left-right mirror | 0.9601 IoU on the paint. Not enforced — the paint is taken as drawn |
| Tree mass | placeholder cones | seeded scatter on wall tops, flat grey; replaced by M2 doodads in dressing |
| Lane towers | 2 per lane per side | up from 1 |
| Content bundle | unchanged design | the config YAMLs are a human-authored designer surface, not generated from geometry |

### Do not retry: uniform erosion to narrow lanes

Measured. The map's median floor half-width is 9 yd, so half the floor sits in
corridors 18 yd or narrower. Eroding 10 yd — the depth that hits a 38 yd lane
target — fragments the map into 5 disconnected regions and halves the ramps.
Lane width is a repaint or a scale change, never a filter.

## Geometry the content pass will need

Stable against retessellation, so these survive a rebuild:

- 1 outer loop + 15 island loops. Extent x [-216.0, 215.4], y [-93.6, 93.6].
  One connected walkable region.
- **Bases** at (-172.6, -1.4) and (172.0, -1.5), ~9100 yd² each.
- **Ramps:** four, run 6.0-7.4 yd, mouth 35.6-37.3 yd.
- Two islands (~207 yd²) are too small to terrace and build as plain capped
  prisms — reported as `walls_unterraced`, working as intended.

Object and triangle counts are *not* here: they move with the tessellation and
the floor's batch-ceiling grid. The current ones are in `gen_blockout.py`'s
report, bracketed in README step 0-1.

## Structure positions come from in-game `.gps`, not from geometry

Lane centrelines, tower and camp positions are **not** derived from the trace,
and a painted marker layer was designed and rejected:

- The configs carry z; a flat image cannot.
- `lane_config.yaml`'s header records a traced waypoint that sat 0.2 yd from
  `TT_JWall_12`'s face and had to be hand-repaired. A point recorded by standing
  on it cannot have that defect.
- You cannot `.gps` a map that does not exist yet, so geometry comes first.

`layout.json` is therefore dead for v2 — it held exactly those positions. The
map now exists, so this pass is unblocked.

## Outstanding

### Two bundles, one slot — undecided

`apps/moba/maps/twisted_treeline/` (pass 1) and `twisted_treeline_v2` both
target map 900. Before v2 gains content configs, settle:

- **`active:` in `base_config.yaml`** — one bundle owns the battleground slot.
  Two with `active: true` will conflict.
- **ID allocation.** Pass 1 filled 900000-900009 (towers), 900020+ (creeps),
  900209+ (neutrals), and the allocator carved 900400-900499. v2 either reuses
  those keys or needs its own block in `apps/moba/id_blocks.json`.

### Dressing — blocked on where placements are stored

A doodad placement is hand-authored data with nowhere to live: it cannot sit in
the source `.blend` (`wow_wmo_doodad` is a WBS property, WBS is 3.4-only) and it
cannot travel through the OBJ. From the first real doodad the staging `.blend`
holds original data rather than derived, and the "treat it as disposable" claim
expires. Full reasoning in README step 3; the shape that fits is a
`doodad_config.yaml` plus a generator, with an open question about whether it
also needs a dump-the-scene-to-YAML direction.

### Lighting — deferred, not blocked

Map 900 wears Netherstorm's via the cloned `Light.dbc` row. Real, and not what
kept the map from reading right.

### Wall texture — deferred pick

`GhostLandsRock01.blp` is a ground texture with no vertical strata.
`DUNGEONS\TEXTURES\ROCK\` has 273 authored for vertical faces. A material path
in `blender_staging_setup.py`'s `TEXTURES`; blocks nothing.

### Housekeeping

- **Nine orphan `_023.wmo`…`_031.wmo` are in the MPQ**, left from a 15-chunk
  export that preceded the 23-group one. MOHD says 23 groups so the map works
  and nothing reads them. Fix is `rm` + re-pack; no re-extract, since the
  extractor never saw them either.
- Cones overhang island edges slightly. Placeholder geometry that dressing
  replaces, so left alone.

### Untested paths

- **Three failure gates in `gen_blockout.py` are unexercised** —
  `floor_regions`, slope, and `base_yd` below `walkableClimb`. The happy path is
  verified end to end; those branches are not.
