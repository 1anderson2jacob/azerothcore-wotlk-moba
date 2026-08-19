# Custom map: Twisted Treeline terrain via WMO

## Status

**The map is done** (2026-08-18). Steps 0–9 all run: v2 is traced, built,
exported, packed, extracted and walkable on map 900, from
`apps/moba/maps/twisted_treeline_v2/`. The map-agnostic procedure is in
`apps/moba/wmo/README.md`.

**The content on it is placed** (2026-08-19) — every coordinate in all four
positional configs is derived and mirror-symmetric. Dressing, lighting and the
wall-texture pick are still open behind it, plus the tower model asymmetry below.

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
- Two islands build as plain capped prisms rather than terraced —
  `TT_JWall_11` (335.9 yd²) and `TT_JWall_12` (314.3 yd²), reported as
  `walls_unterraced`, working as intended. It is **shape, not size**: the 207.0
  and 202.4 yd² islands terrace fine, and these two do not.

Object and triangle counts are *not* here: they move with the tessellation and
the floor's batch-ceiling grid. The current ones are in `gen_blockout.py`'s
report, bracketed in README step 0-1.

## Structure positions are derived from geometry, validated by `.gps`

Pass 1 rejected deriving positions, on the grounds that a flat image cannot carry
z. That reasoning was sound about the *trace* and wrong about the *build outputs*:
`geometry.json` carries the loops and `heights.png` carries the height field, and
between them every coordinate a config needs is computable.

`geometry.json`'s frame **is** the world frame on map 900 — no offset, rotation or
axis swap. Confirmed against three `.gps` readings: two base platforms (predicted
3.000000, read 3.000014 / 3.000007) and one lane floor (predicted 0.000000, read
0.000483). So `.gps` is what *validates* the frame, not what supplies the points —
two readings were enough to license deriving the other ~120 coordinates.

`layout.json` is dead for v2 and nothing replaces it; the configs are the only
home for placement.

## Outstanding

### The content pass — positions derived 2026-08-19

All four positional configs carry derived x/y/z/o. `creep_config.yaml` and
`player_config.yaml` needed nothing — they reference lanes and slots by name.
Creeps are still downstream of the lanes though: `gen_creep_roster.py` resolves
each `WaypointPathId` from the lane lockfile, so `gen_all.sh` (never a hand-picked
subset) is what regenerates this bundle.

What was derived, and how:

- **Lanes** — corridor-hugging path search over the walkable mask, smoothed onto
  the local clearance maximum, mirrored about x=0. Worst clearance 11.3 / 12.0 yd
  against pass 1's 14 bad points on bot alone, 4 of them inside walls.
- **Structures** — 14, up from 10. The ledger's second lane tower per lane per
  side landed as new `*_inner` keys from 900406+; the **existing keys stayed on
  the outer towers** so 900006/900007/900401/900402 keep their assignments.
- **Camps** — nearest point to each pass-1 anchor with 7.5+ yd clearance holding
  18+ yd off both lanes, members snapped individually.

Two traps found, both of which cost a wrong result before being caught:

- **`geometry.json`'s `bases[].centre_yd` is a centroid, and each base is a ring
  around a central island** — so the centroid lands *on* the island. (-172.62,
  -1.44) reads as the obvious core spot and has 0.5 yd of wall clearance.
- **`heights.png` is stored vertically flipped** relative to what `cy_px` indexes.
  Recorded where it bites, on the registration dict in `blockout_trace.py`.

### Tower models are not equivalent between the two teams

`combat_reach` x `display_scale` is 21.0 for the Alliance tower (27101 x3.0) and
2.0 for the Horde tower (18505 x2.0) — a 10x difference in the range a melee
player can attack one from. Inhibitors (5.625) and cores (5.46) are symmetric;
only the towers are off. Not a placement problem: it needs two display ids with
comparable `combat_reach`. Jacob parked this on 2026-08-19.

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

### Surface texturing — pipeline built, art decision parked

**The pipeline expresses it now** (2026-08-19): per-material texture path and UV
scale, floor material per height class, wall material per island and per band.
The config surface is `map_source.yaml`'s `materials` + `surfaces` blocks, whose
own header carries the per-field semantics.

**Nothing about the look is decided.** Jacob parked the call pending two things
that change what any texture looks like:

- **Dressing** — canopy and overgrowth occlude much of what is being judged.
  Blocked; see below.
- **Lighting** — map 900 still wears Netherstorm's cloned `Light.dbc` row, so
  every colour judgement made now is made under the wrong light. Deferred rather
  than blocked, and cheaper than dressing.

**What is on the map is a comparison build, not a chosen look.** A future session
must not read it as one:

| | |
|---|---|
| 15 island walls | a distinct texture per island per band — 30 in all, lower and upper paired within a family |
| outer wall | `ghostlands_rock`, the incumbent, as a constant control |
| 6 floor chunks | one grass candidate each; `lane_ghostlands` is the control |
| bases + ramps | `walkway`, shared |

`TT_JWall_11` and `TT_JWall_12` are the unterraced prisms, so they carry no upper
band on any vertical face and their `upper` texture lands only on the top cap.
Not a fault.

**You still cannot preview a texture in Blender** — `blender_staging_setup.py`
stamps `wow_wmo_texture.path` and never loads the BLP. What the 5.1 blend *does*
show is the assignment: every material gets a distinct viewport hue, so a
top-down look catches a wall wearing the wrong candidate before a pack-and-relaunch.

**Walking it.** Geometry has not moved across any texture build, so these hold.
`.go xyz` with z omitted, ordered as a loop; the nearest wall face is always the
target, with at least 20 yd to the next island.

| # | `.go xyz` | lower / upper |
|---|---|---|
| 1 | `-207.9 -30.7` | shdwfang_outer / shdwfang_outer03 — **prism** |
| 2 | `-158.8 -57.0` | ne_01 / ne_03 |
| 3 | `-97.3 -82.9` | kzn_green / kzn_black |
| 4 | `-46.5 -74.6` | barrow_dirt / barrow_stair |
| 5 | `5.1 -66.5` | kzn_outer / kzn_outer02 |
| 6 | `46.6 -72.2` | barrow_den / barrow_den02 |
| 7 | `98.0 -83.0` | kzn_large / kzn_plain |
| 8 | `209.2 -30.5` | shdwfang_stones / shdwfang_stones02 — **prism** |
| 9 | `158.5 58.5` | ne_02 / ne_ds_02 |
| 10 | `105.5 78.9` | mrdn_rockwall / mrdn_rockwall02 |
| 11 | `61.8 55.1` | dmaul_01 / dmaul_base |
| 12 | `-0.5 73.0` | ne_tower_01 / ne_tower_03 |
| 13 | `-59.8 51.4` | dmaul_02 / dmaul_hall |
| 14 | `-89.9 74.0` | mcave_rock / mcave_crock |
| 15 | `-60.6 -16.4` | ne_ds_01 / ne_ds_03 |

Twins sit 130–360 yd apart, so a within-family A/B is sequential, judged across a
walk. What you get simultaneously is the *neighbour* comparison, which is
cross-family — cheaper for eliminating whole families first.

**Still open, all Jacob's:**

- What the surfaces should look like at all.
- **Lane floor vs jungle floor cannot be distinguished today.** Both are
  `#00FF00` at z=0, and `heights.png` is the only raster surviving the trace, so
  the height rule that separates base from ramp cannot separate these. Costed
  2026-08-19 and declined: the cheap route is a second `regions.png` on the same
  registration, kept *separate from* `tt_v2_trace.png` because that file is
  load-bearing for ~120 derived content coordinates and repainting it risks
  moving them. `classify()` is already N-way over palette names; what is missing
  is any per-pixel class output.
- Whether wall `lower` and `upper` should differ once a family is chosen, and
  whether the horizontal surfaces — ledge and cap — want terrain rather than wall
  texture. The face spans already exist; only the config shape is missing.

### Housekeeping

- Cones overhang island edges slightly. Placeholder geometry that dressing
  replaces, so left alone.

### Untested paths

- **Three failure gates in `gen_blockout.py` are unexercised** —
  `floor_regions`, slope, and `base_yd` below `walkableClimb`. The happy path is
  verified end to end; those branches are not.
