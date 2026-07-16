# Custom map: Twisted Treeline terrain via WMO

## Status of this doc

**Forward plan, not yet verified against tooling or engine source.** The steps
below are the intended pipeline and carry real unknowns — exporter quirks, exact
DBC columns, extractor behavior on a WMO-only map. Treat each step as a thing to
prove in its own focused session, not a settled recipe.

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

Best implemented as a **WMO-only map**: a WDT that references a single global
map object (the `WDT_USES_GLOBAL_MAP_OBJECT` flag) with no ADT terrain tiles —
the way arenas / some instances are built. The whole playable space is one WMO.

## Source of truth

The blockout lives in `var/blender/twisted_treeline_blockout.blend`
(collection `TwistedTreeline`, 1 unit = 1 WoW yard, pixel-traced from the real
TT minimap — see the `moba-map-pipeline` memory for calibration constants).

Structure coordinates + lane waypoints are exported to
`var/blender/twisted_treeline_layout.json` — the feed for the creep
waypoint/roster generators (`apps/moba/gen_creep_roster.py`) once real map
coordinates exist. The placeholder marker meshes (towers, camps, boss, altars)
are position anchors only; they do not ship.

## Pipeline (each step is its own session-sized chunk)

| # | Step | Who | Notes / unknowns |
|---|------|-----|------------------|
| 1 | Prep scene for export: separate collision geometry, assign materials, split into WMO groups, confirm scale/axis | Claude drives, Jacob runs Blender | 1u=1yd is already correct for WoW |
| 2 | Export to `.wmo` (+ BLP textures) via a Blender WMO exporter (e.g. WoW Blender Studio addon) | Jacob installs addon; iterate together | **fiddliest step**; WMO v17 for 3.3.5a; expect format/axis iteration |
| 3 | Build the WMO-only map: WDT with global-WMO flag; DBC rows — `Map.dbc` (new map + directory), `WorldSafeLocs.dbc` (graveyards), likely `AreaTable`/`WMOAreaTable` | Claude (as file/DBC edits) | exact columns unverified — research when we get here |
| 4 | Pack `.wmo` + textures + WDT + DBCs into an MPQ client patch | Jacob | client can't load the map without it |
| 5 | Re-run extractors server-side: `mapextractor`, `vmap4extractor` + `vmap4assembler`, `mmaps_generator` | Jacob | **mmaps is load-bearing** — creep pathfinding needs the navmesh built from WMO collision |
| 6 | Register the map server-side + point `BattlegroundMOBA` at the new map id | Claude (C++/SQL) | ties into existing BG code; also gets the standalone BG id + `BattlemasterList.dbc` client patch that was always planned |

## Division of labor (per CLAUDE.md env rules)

- **Claude does:** Blender scene prep, DBC/SQL/C++ authored as files.
- **Jacob does:** install the exporter addon, MPQ packing, run the extractors,
  restart servers — everything that operates the environment.

## First concrete step

Get a *valid* `.wmo` out of the blockout (steps 1–2). Everything downstream
depends on it and the tooling is least predictable there, so do it as a
dedicated session rather than bundling it with other work.