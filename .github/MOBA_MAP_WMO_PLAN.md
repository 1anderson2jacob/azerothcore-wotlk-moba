# Custom map: Twisted Treeline terrain via WMO

## Status

**The map is done** (2026-08-18). Steps 0–9 all run: v2 is traced, built,
exported, packed, extracted and walkable on map 900, from
`apps/moba/maps/twisted_treeline_v2/`. The map-agnostic procedure is in
`apps/moba/wmo/README.md`.

**The content on it is placed** (2026-08-19) — every coordinate in all four
positional configs is derived and mirror-symmetric.

**Chosen and shipped** (2026-09-01): the perimeter wall wears rock on both
bands, the floor is one grass across all six chunks, the plateau carries
continuous canopy, and both terrace shelves carry felwood alongside the
lamppost catalogue.

**Still scaffolding, and the next thing to do**: island wall textures are a
30-material comparison and island cap trees a 15-family one. Both want the loop
that settled the perimeter and the ledges — walk, `.gps` at anything that
stands out, resolve it against `var/blender/<bundle>_doodads.json` or the build
legend, narrow the list. Lighting is still Netherstorm's cloned row, so every
colour call above is provisional. Lane-vs-jungle floor stays blocked on a
second raster.

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

### Dressing — built and run 2026-08-20

Where placements live is settled, and the answer is not a `doodad_config.yaml`
in the bundle. They resolve on the 5.1 side out of `map_source.yaml`'s
`dressing` block and reach the 3.4 scene as a JSON sidecar, the way material
paths already do — so the staging `.blend` stays derived. Mechanism in README
step 3.

**Confirmed reaching the server**: 242 doodad spawns in `900.vmtree`, so tree
collision is vmap-backed and not merely the client refusing to walk through an
M2. Only models carrying bounding triangles get there — see *Wall faces* below.

What ships is a **comparison build, not a chosen look**, same as the wall
textures: one candidate tree family per island, 15 of them, plus a control
family on the outer plateau ring. Mirror twins deliberately differ — dressing
symmetry is not a fairness property, and a distinct family per island is more
information per walk.

**One island takes no trees at all.** `TT_JWall_13`'s cap is 117 yd2, which at
6 per 1000 yd2 rounds to zero. Density, not a fault. The claim recorded here
before — two islands, a 55 yd2 cap, `zuldrak` reaching nothing — predates
ordering `island_caps` widest-family-first, which is what fixed it.

### Wall faces — built, walked, and cleared 2026-08-24

Everything here shipped and was judged. **The walls are deliberately bare now** —
a future session must not read that as never-built.

What survives in code: `scatter_on_faces` (mass on a riser), `rail_on` +
`contiguous` + `run_segments` (a catalogue: one model per slot at a known
place), `base_facing` (which faces front a base), and `place_explicit`. Only the
scatter and `place_explicit` have config using them.

**Tried and dropped, so it is not re-derived:**

- **Pitched ground mats.** `pitch_deg` laid a flat-authored model against the
  wall. Removed, and not on art grounds: `height_yd` constrained the axis that
  becomes vertical after the turn, which for these models is their *thinnest*,
  leaving protrusion unbounded — `elwynn_ivy` shipped 2.1 yd tall and **10.5 yd
  out of the wall**. Correctly parameterised they are still 0.5–3 yd ground
  clumps on a 16 yd cliff, so the fix does not rescue them.
- **`deadwind_curtain`.** A 47 yd native card scaled to 5 yd height ships
  **19–22 yd wide**, a 4:1 smear. Scaling to a height leaves width free; that is
  a property of the model, not a setting.
- **Eight vine families, plus roots and webs on the perimeter.** All removed to
  leave the risers clear for the texture pick. What reached the map was
  `swamp_vines`, `bt_vines`, `deadwind_weeds`, `deadwind_strands`,
  `deadwind_curtain`, `sholazar_vine`, `zuldrak_vineplane`, `azjol_fern`, then
  `ghostlands_roots` and `terokkar_webs`.
- **113 tapestries and banners**, as a catalogue on the base walls at one model
  per 8 yd slot. Two survived: `dalaran_banner_alliance_02` and
  `dalaran_banner_horde_02`.

**Facts bought along the way:**

- **Zero-collision doodads never reach the vmaps.** 423 wall props emitted
  nothing; only the 242 trees did. So a dressing-only change is a **2/3/4/5/7
  rebuild with a client relaunch** — `wmo_verify`'s `doodads emitted to vmaps`
  line is the test.
- **Coverage ≈ `width × band_height × density / 1000`.** At `per_1000_yd2: 10` a
  5 yd vine covers 40% of the wall's length, which is why the first build read as
  scattered accents. 25–30 gives continuous cover. The hard ceiling is
  `1000 / (min_spacing² × 0.866)` — about 46 at 5 yd spacing — and rejection
  sampling reaches 60–70% of it before warning.
- **A riser is exactly vertical by construction**, since both rings of a strip
  share one xy loop. The band spans are therefore the whole face selector and no
  slope test is needed — superseding the "invert the slope test" decision
  recorded here before anyone checked.
- **Shape predicts a curtain better than a name.** Aspect (`dy / z_extent`) plus
  where the geometry sits relative to the origin sorted 7918 models far better
  than the word "vine": `deadwind_curtain` reads vine-ish and is 3.3;
  `genericseaweed10` reads like nothing and is 0.09.
- **Height is measured differently by surface** — a cap buries everything below
  the origin, a wall face buries nothing — which is why one family cannot serve
  both kinds. Hanging models put every vertex *below* the origin;
  `azjol_hangingfern_01` tops out at 0.27, which read as a height scales it 96x.

**Still open:** textures for the base walls and the inside of the perimeter;
whether wall dressing returns at all, and at what density.

### Terrace ledges — shipped 2026-08-25

87 lampposts, one per slot, along all 14 terraced shelves. `TT_JWall_11` and
`TT_JWall_12` are the unterraced prisms, have no shelf, and are correctly bare.
Recorded so it is not rebuilt.

**The same shelves also carry ~259 felwood trees** (2026-09-01), from two
surfaces — `terrace_ledge_trees` scoped to the islands and
`terrace_ledge_trees_outer` to the perimeter, so their mixes and densities can
diverge. Both are FILLS rather than catalogues: the model is drawn per slot
rather than consumed from a list, so a two-model family still covers 3000 yd of
shelf. `felwood` is split into `felwood_tall` and `felwood_wide` at different
`height_yd` because one height left the two models 2.4x apart in canopy width.

| | |
|---|---|
| Spacing | **34 yd**, picked so 92 slots meet an 87-model catalogue |
| Height | 5.0 yd, held constant across the family |
| Order | islands before the outer ring, so the catalogue lands where the walk goes |
| Placement | shelf centreline, yaw from the riser foot out over the lip |

`rail_on_ledge` is a sibling of `rail_on` rather than a mode on it, for the same
reason `scatter_on_faces` is separate from `scatter_on`: the shelf is
horizontal, so the model stands on its ORIGIN and a lamppost's sunk base plate
buries the way a tree's root flare does.

`face_columns` was the named blocker; the fix is `ledge_columns`. A shelf quad's
four corners pair by VERTEX INDEX — `Shell.ring` appends `inner_ledge` before
`ledge_out` over one loop, so the two lowest indices are the drop lip and the
two highest the riser foot. Self-checking, because `wall_rings` hands both rings
the same z list.

**Two claims this section made that the model probe corrected:**

- **No footprint gate, and `edge_margin_yd` is unused.** The box a footprint
  test reads spans the model at its widest point, which on a lamppost is the
  head, not the foot — gating on it rejects posts whose arm overhangs the drop,
  which is the thing worth having over a 12-20 yd cliff. 26 of the 87 are wider
  than their own 2.5 yd shelf; all land on the outer ring, and the count is
  reported rather than acted on.
- **Hanging lanterns barely exist as models.** Only 9 of 151 candidates put all
  geometry below their origin, and they are 11-17 yd Ironforge chandeliers.
  `fk_lamphanging` and `wt_lanternhanging01` both stand on their origin, exactly
  as predicted here.

**Not started:** the second surface for true hanging lanterns on the upper
riser. It needs a wall-kind rail plus a mount-height-within-band field.

### Floor dressing — not built, no mechanism

50,200 yd2 of lane, jungle, base and ramp, larger than every wall riser
combined, and nothing has ever been on it. `dress()` is handed the wall objects
only — `{"outer_plateau": [outer], "island_caps": islands, ...}` — so the floor
chunks never reach it and no surface name exists for them.

**Not the same problem as wall faces.** A riser carries zero navmesh risk; the
floor is walkable, so a prop with bounding triangles reaches the vmaps *and* the
mmaps and `mmaps_generator` carves navmesh around it. A bush that blocks a lane
is a gameplay change. The 0-collision rule that was a preference on walls is a
requirement here — unless a prop is *meant* to obstruct, which is a design
decision and not dressing.

Wants the lane-vs-jungle split too: clutter should differ between them, which is
the same `regions.png` dependency as the floor texture pick and the trace-driven
placements.

### Doodads from a trace — half built

The scatter is the right tool for mass and the wrong one for intent: it cannot
put a specific model at a specific place, and marking a base or a chokepoint is
exactly that.

**The explicit half exists** (2026-08-23). `dressing.placements` takes a model,
a height and an `anchor` [x, y]; `place_explicit` finds the nearest riser face
and derives yaw, height within the band and the push out of the wall from it.
Only the anchor is hand-authored, so a retessellation moves the model with the
wall rather than stranding it. The two base banners use it.

**The trace-driven half is still wanted** — a hand-painted raster saying *put
one here*, the way the map boundary already is, so intent scales past what
anyone will type by hand.

That shares a dependency with the lane-vs-jungle floor split: a second raster on
the same registration, kept separate from `tt_v2_trace.png` because that file is
load-bearing for ~120 derived content coordinates and repainting it risks moving
them. `classify()` is already N-way over palette names. Whatever pays for one
pays for the other.

### Lighting — deferred, not blocked

Map 900 wears Netherstorm's via the cloned `Light.dbc` row. Real, and not what
kept the map from reading right.

### Surface texturing — perimeter and floor chosen, islands still a comparison

**The pipeline expresses it now** (2026-08-19): per-material texture path and UV
scale, floor material per height class, wall material per island and per band.
The config surface is `map_source.yaml`'s `materials` + `surfaces` blocks, whose
own header carries the per-field semantics.

**The perimeter and the floor are decided; the islands are not.** Both calls
were made under two known distortions, so neither is beyond revisiting:

- **Dressing** — canopy occludes much of what is being judged, and at the
  shipped ledge scale it covers most of the perimeter's upper band.
- **Lighting** — map 900 still wears Netherstorm's cloned `Light.dbc` row, so
  every colour judgement is made under the wrong light. Deferred rather than
  blocked.

**What is on the map is a comparison build, not a chosen look.** A future session
must not read it as one:

| | |
|---|---|
| 15 island walls | a distinct texture per island per band — 30 in all, lower and upper paired within a family |
| outer wall | **chosen 2026-09-01** — `perimeter_lower` / `perimeter_upper`, rock on both bands |
| 6 floor chunks | **chosen 2026-09-01** — `lane_darkshore_moss` on all six, which also makes the chunk seams vanish |
| bases + ramps | `walkway`, shared |

`TT_JWall_11` and `TT_JWall_12` are the unterraced prisms, so they carry no upper
band on any vertical face and their `upper` texture lands only on the top cap.
Not a fault.

**You can preview a texture in Blender now** (2026-08-25) —
`blender_staging_setup.py` decodes each material's BLP out of the archives, so
Material Preview shows the real art while Solid still shows the per-material hue
that catches a wall wearing the wrong candidate. Doodads carry real M2 geometry
too. The loop, and the arm64 fix that unblocked it, are in
`apps/moba/wmo/README.md`.

**Rendering the whole map headless is also possible, and unbuilt.** Demonstrated
2026-08-25 but never scripted: Blender 5.1 opens the blockout `.blend`, wires the
`texcache` PNGs onto its materials by name, and renders Workbench with
`color_type='TEXTURE'` from any camera — no OBJ, no staging scene, no MPQ, no
client. One top-down pass showed the six floor chunks reading as hard-edged
rectangles and the island caps spanning an implausible tonal range; a lane-level
pass showed `ghostlands_rock`'s repeat is plainly countable at `uv_scale 4.0`.
Worth turning into a script only if the texture pass actually leans on it.

**Walking it.** Geometry has not moved across any texture build, so these hold.
**Append ` 5` to each pair below** — `.go xyz` needs a z on this map, and 5
works everywhere since all walkable floor sits between 0 and the 3 yd base
platform and you drop onto it. Omitting z *cannot* work here: `Map::GetHeight`
finds no terrain to fall back on and searches the vmaps only 50 yd down from
`MAX_HEIGHT`, so it returns invalid and the command answers "target map or
coordinates is invalid". Ordered as a loop; the nearest wall face is always the
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

### Untested paths

- **Three failure gates in `gen_blockout.py` are unexercised** —
  `floor_regions`, slope, and `base_yd` below `walkableClimb`. The happy path is
  verified end to end; those branches are not.
