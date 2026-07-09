# Lane creep waves

## Context

Towers and the win condition are done and verified working. The next roadmap
item is lane creep waves: every 30s, each team spawns a small wave of minions
(2 melee + 1 caster, +1 siege every 3rd wave) that walk a fixed lane toward
the enemy tower, fighting anything hostile along the way using **standard
WoW threat/combat** (deliberately different from the tower's manual
`REACT_PASSIVE` override) so players get familiar aggro/kiting/CC rules
against them. This is still prototype work proving out patterns for the
eventual multi-lane game, so the lane path is a placeholder (straight line
between the existing tower positions) until real `.gps` waypoints are
walked later, same workflow already used for tower/door placement.

Two things surfaced during scoping that get fixed as part of this work
(both confirmed with Jacob):
1. **Win-condition gap**: `HandleKillUnit` only fires on player-attributed
   kills. Once minions can land the killing blow on a tower, that tower
   would actually die (health hits 0) without the win condition ever
   firing — a real bug now, not theoretical. Fixed by extracting the
   existing tower-destroyed logic into a shared `OnTowerDestroyed` method
   callable from both `HandleKillUnit` (player kills) and a new
   `npc_moba_tower::JustDied` override (creature/minion kills).
2. Minion stats/level must stay data-driven (not assumed level-80), since
   Jacob wants to experiment with a level-30 test cap later — same
   philosophy as the tower work. Note: existing towers are *also* hardcoded
   to level 80/80 today and would need the same treatment when that
   experiment actually happens; not fixing that now.

## Verified building blocks

All confirmed by reading the actual engine source, including a second
verification pass that caught real bugs in the first draft of this plan:

- **Spawning**: NOT `Battleground::AddCreature`/`BgCreatures` (fixed-size,
  persistent-roster registry — wrong fit for repeatedly-spawned ephemeral
  minions). Use `TempSummon`. Critical gotcha: `Map::SummonCreature`
  (`Map.h:339`, callable via `GetBgMap()->SummonCreature(...)`) does **not**
  take a `TempSummonType` parameter — only the `WorldObject::SummonCreature`
  convenience overload does (`Object.h:633-634`), and its own implementation
  shows the real two-step: call `Map::SummonCreature(...)` then
  `summon->SetTempSummonType(type)` on the result. `TempSummon`'s
  constructor defaults `m_type` to `TEMPSUMMON_MANUAL_DESPAWN`
  (`TemporarySummon.cpp:28`) — skip the explicit `SetTempSummonType` call
  and every minion silently becomes manual-despawn-only (never cleans up).
- **Despawn type**: `TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT`, not
  `TEMPSUMMON_CORPSE_TIMED_DESPAWN` — verified via `TempSummon::Update()`
  (`TemporarySummon.cpp:66-160`) that the corpse-timed variant's countdown
  never moves while alive, so a minion that never engages anything (just
  paces the repeatable lane) would never despawn under it. The
  out-of-combat variant counts down whenever not in combat and resets on
  engagement, correctly handling both "died" and "wandered unengaged"
  end-states with one mechanism. Suggest `DespawnMs` ≈ 60000, tunable via
  the new creep data table.
- **Wave timer**: reuse the already-scaffolded, currently-empty
  `_bgEvents` `EventMap` in `BattlegroundMOBA::PostUpdateImpl`
  (`BattlegroundMOBA.cpp`), self-rescheduling every 30s — same idiom as
  `BattlegroundDS::PostUpdateImpl` (`BattlegroundDS.cpp:37-71`).
- **Pathing**: `MotionMaster::MoveWaypoint(path_id, true, PathSource::WAYPOINT_MGR)`
  reads the plain `waypoint_data` DB table (`WaypointMgr::Load()`,
  `WaypointMgr.cpp:36`) — independent of SmartAI, so the lane route is pure
  SQL, retunable without a rebuild, regardless of AI framework.
- **Leash-radius gotcha (real bug caught in review, not in the original
  brief)**: `Creature::CanCreatureAttack()` (`Creature.cpp:2657-2728`, the
  live leash gate feeding `ThreatManager`) falls back to
  `IsInDist2d(&m_homePosition, CONFIG_CREATURE_LEASH_RADIUS)` once the
  in-combat grace window lapses — and `CONFIG_CREATURE_LEASH_RADIUS`
  defaults to **30.0 yards** (`WorldConfig.cpp:364`). `WaypointMovementGenerator`
  only updates `SetHomePosition` on waypoint-**node arrival**
  (`WaypointMovementGenerator.cpp:150`), not continuously along the path.
  With only 2 raw points (the two tower positions, ~233 yards apart), home
  position stays pinned at the start for nearly the entire walk, so a fight
  breaking off mid-lane would compare against a home position up to ~230
  yards away — evades far too early, not "leash at current lane position."
  **Fix**: the placeholder path needs enough interpolated points that
  consecutive nodes are well under 30 yards apart (~13 points, ~19 yards
  apart, for the current 233-yard span) — still a trivial straight-line
  computation, just not literally 2 endpoints.
- **`Reset()`-on-evade gotcha (real bug caught in review)**:
  `CreatureAI::EnterEvadeMode()` (`CreatureAI.cpp:235-277`) queues the
  home-return move, then calls `Reset()` **immediately, synchronously** —
  not after the return finishes. A naive unconditional
  `MoveWaypoint(pathId, true)` in `Reset()` would `Mutate()` a **brand-new**
  waypoint generator (starting at node 0) into the idle slot, destroying
  the one that survived underneath with its lane progress intact —
  rewinding the minion to the lane's start instead of resuming from where
  it was. Fix: guard with
  `if (me->GetMotionMaster()->GetMotionSlotType(MOTION_SLOT_IDLE) != WAYPOINT_MOTION_TYPE)`
  (confirmed method, `MotionMaster.h:269`) before calling `MoveWaypoint` —
  true on first spawn only, false on any evade-triggered re-`Reset()`.
- **Targeting/threat**: `REACT_AGGRESSIVE` + engine defaults give
  everything asked for, confirmed via `CreatureAI::MoveInLineOfSight`
  (aggro-on-sight, `CreatureAI.cpp:192`), `ThreatManager::ReselectVictim`
  (auto re-target on higher threat, `ThreatManager.cpp:623`), and
  `Spell::HandleThreatSpells` (harmful spells/CC generate threat by
  default even with zero direct damage, `Spell.cpp:5551`) — no custom
  targeting code needed, unlike the tower's manual override.
- **Melee vs. caster behavior**: `DoMeleeAttackIfReady()` is only ever
  called from inside `UpdateAI()` implementations, never by the engine's
  own loop directly — so a caster's custom `UpdateAI()` simply never
  calling it is sufficient to suppress melee, no risk of fighting engine
  defaults. `ScriptedAI` already ships the exact intended toggle for this:
  `SetAutoAttackAllowed(false)`/`IsAutoAttackAllowed()`
  (`ScriptedCreature.h:450-451`), gating the call inside
  `ScriptedAI::UpdateAI` (`ScriptedCreature.cpp:242,766`) — confirmed, not
  a repurposed flag.
- **`DoCastVictim(spellId, true)` bypasses range checks** (triggered casts
  skip validation) — the caster's periodic-cast tick needs its own
  `me->IsWithinDist(victim, range)` guard first, mirroring the tower AI's
  `IsValidTowerTarget`.
- **`JustDied`'s player/pet check simplifies to one call**:
  `killer->GetCharmerOrOwnerPlayerOrPlayerItself()` (`Unit.h:1294`) already
  returns non-null for a player or their pet/guardian, null for an
  independent hostile creature — one check covers both exclusions.

## Judgment calls flagged, not blockers

- 60s despawn duration, 30s/every-3rd-wave cadence starting at door-open —
  reasonable defaults, tunable via SQL like everything else.
- Casters close to melee-adjacent range to cast rather than holding
  distance/kiting — deliberate scope simplification (no ranged-chase helper
  exists in this codebase version); an isolated future enhancement
  (override `AttackStart` with `MoveChase`'s optional `ChaseRange`) if it
  looks wrong in playtesting.
- Two same-role minions spawning at the exact same point will visually
  stack momentarily — cosmetic, easy small-offset fix later, not blocking.

## Implementation, by file

### New: `data/sql/custom/mod_moba_creeps.sql`
- `creature_template` + `creature_template_model` DELETE/INSERT for 6 new
  custom entries `900010`-`900015` (Alliance/Horde × melee/caster/siege),
  `ScriptName = 'npc_moba_creep'` on all. Base stat columns copied from
  existing entries the same way towers copied their source models: melee
  from 2279/2280 ('Battleguard', display 164/496, faction 84/83, already
  melee-class), caster from 1914 ('Dalaran Mage', display 3559, reassign
  faction 84) / 11683 ('Warsong Shaman', display 11865, already faction 83),
  siege from 34775 ('Demolisher', display **27658** only) for both —
  explicitly do **not** copy `VehicleId` (10314 on the source) or any other
  vehicle-specific column, so it renders as a demolisher but behaves as an
  ordinary ground creature (same pattern already proven by the tower's
  Keep Cannon/Fel Cannon prop-model reuse).
- New table `mod_moba_creep_data`: `CreatureEntry` (PK), `Team`, `Role`
  (0=melee/1=caster/2=siege), `AttackRange`, `AttackIntervalMs`,
  `AttackSpellId` (caster only), `WaypointPathId`, `DespawnMs`. Seed all 6
  entries; `WaypointPathId` = `900100` for Alliance entries, `900101` for
  Horde.
- `waypoint_data` rows for path `900100` (Alliance tower → Horde tower) and
  `900101` (reverse) — ~13 linearly-interpolated points each between
  `mod_moba_tower_data`'s existing 900000/900001 positions (~19 yards
  apart, per the leash-radius fix above), `move_type = 1` (run),
  `delay = 0`, `smoothTransition = 0`. Placeholder until real `.gps`
  waypoints replace them later.

### New: `src/server/game/Battlegrounds/Zones/MobaCreepData.h` / `.cpp`
Mirrors `MobaTowerData.h`/`.cpp`'s shape exactly (same `LoadIfNeeded()`/
`GetConfig(entry)`/`GetAll()`/`_byEntry` pattern) — deliberately a separate
class, not a shared base with `MobaTowerDataStore`, since the two tables'
columns mostly diverge and one more file in this codebase's established
copy-adapted-file convention is simpler than a premature abstraction.
`MobaCreepConfig { entry, team, role, range, intervalMs, spellId, pathId,
despawnMs }`. Loaded once via `sMobaCreepDataStore->LoadIfNeeded()`.

### New: `src/server/scripts/Custom/npc_moba_creep.cpp`
Single shared AI for all six entries. `Reset()`: resolve `_cfg`; guard
`MoveWaypoint` against the evade-rewind bug (see above); if role is caster,
`SetAutoAttackAllowed(false)` and arm a `TaskScheduler` cast tick.
`UpdateAI()`: casters run their scheduler + `UpdateVictim()` and stop
(never call melee); melee/siege just call `ScriptedAI::UpdateAI(diff)`
unchanged — zero custom per-tick logic needed for those two roles. Cast
tick guards range manually before `DoCastVictim(spellId, true)`.

### Modified: `src/server/game/Battlegrounds/Zones/BattlegroundMOBA.h`
- New enum `EVENT_MOBA_SPAWN_WAVE` for `_bgEvents`.
- New `struct MobaWaveComposition { uint32 meleeEntry, casterEntry,
  siegeEntry; }`, member `MobaWaveComposition _waveComposition[2]`
  (indexed by `TeamId`), `uint32 _waveCount = 0`.
- New public methods: `void OnTowerDestroyed(Creature* tower, TeamId
  winnerTeamId);` (extracted from `HandleKillUnit`, also called from
  `npc_moba_tower::JustDied` — public so that translation unit can reach
  it via the same `dynamic_cast<BattlegroundMOBA*>` pattern
  `moba_tower_aggro.cpp` already uses), `void SpawnWave(TeamId team, bool
  includeSiege);`, `void SpawnCreep(uint32 entry);`.

### Modified: `src/server/game/Battlegrounds/Zones/BattlegroundMOBA.cpp`
- `SetupBattleground()`: load creep config, populate `_waveComposition`
  per team from `sMobaCreepDataStore->GetAll()`; fail BG creation only if a
  team is missing melee or caster (every wave needs those); log-warn and
  skip siege spawning if a team's siege entry is absent (bonus unit, not
  core).
- `StartingEventOpenDoors()`: add `_bgEvents.ScheduleEvent(EVENT_MOBA_SPAWN_WAVE, 30s);`
  alongside the existing achievement-timer line.
- `PostUpdateImpl()`: handle `EVENT_MOBA_SPAWN_WAVE` — increment
  `_waveCount`, call `SpawnWave` for both teams (`_waveCount % 3 == 0`
  gates siege), reschedule itself 30s out (mirrors `BattlegroundDS`).
- `SpawnCreep`: `GetBgMap()->SummonCreature(entry, pos)` then
  `summon->SetTempSummonType(TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT)` —
  the two-step is mandatory, not optional. Spawn position derived from
  `sWaypointMgr->GetPath(cfg->pathId)`'s first node — single source of
  truth, can't drift out of sync with the lane.
- `HandleKillUnit()`: shrink to find-the-tower + call `OnTowerDestroyed(creature,
  killer->GetTeamId())`. `OnTowerDestroyed` gets the body that used to live
  in `HandleKillUnit` (mark destroyed, unlock guarded towers, worldstate
  update, win check, `EndBattleground`), plus an `if (itr->destroyed)
  return;` idempotency guard at the top now that two call sites feed it.

### Modified: `src/server/scripts/Custom/npc_moba_tower.cpp`
Add `JustDied(Unit* killer) override`: return early if
`killer->GetCharmerOrOwnerPlayerOrPlayerItself()` is non-null (player or
their pet already handled by `HandleKillUnit`); otherwise look up the
killer's team via `sMobaCreepDataStore->GetConfig(killer->GetEntry())`,
resolve `BattlegroundMOBA*` via `me->GetMap()->ToBattlegroundMap()->GetBG()`
+ `dynamic_cast`, call `moba->OnTowerDestroyed(me, creepCfg->team)`.

### Modified: `src/server/scripts/Custom/custom_script_loader.cpp`
Register `AddSC_npc_moba_creep()`.

### Modified: `.github/MOBA_GUIDE.md`
New "How the creep wave system works" section (contrast the
`REACT_AGGRESSIVE`/real-threat model explicitly against the tower's manual
override so future readers don't assume parity); new recipes for
adding/moving a creep waypoint path, changing wave cadence/composition,
changing a creep's attack range/interval/spell; remove the now-fixed
`HandleKillUnit` player-only-kill gotcha, replace with a short note on the
`OnTowerDestroyed` shared-method pattern.

### New: `.github/MOBA_CREEP_ARCHITECTURE_PLAN.md`
Same structure/purpose as `MOBA_TOWER_ARCHITECTURE_PLAN.md`, capturing the
verified building blocks and judgment calls above for future reference.

### Modified: `.github/README.md`
Check off `- [ ] Lane creep waves` once shipped and verified in-game.

## Verification

Manual, in-game (no automated BG tests in this repo):
1. Build/install/restart, apply SQL, `.debug bg`, queue.
2. Confirm waves spawn every 30s for both teams, correct composition
   (2 melee + 1 caster; +1 siege every 3rd wave), walking the placeholder
   lane toward the enemy tower.
3. Fight a minion, break off mid-lane (not near either tower) — confirm it
   leashes back to roughly where the fight happened, not to the map's
   original spawn point, and correctly resumes forward waypoint progress
   afterward rather than restarting from the beginning.
4. Confirm melee minions auto-attack normally; casters visibly cast their
   configured spell instead of meleeing.
5. Let a minion kill a tower (no player finishing it off) — confirm the
   battleground still ends with the correct winning team.
6. Let a wave go completely unengaged for a while — confirm it eventually
   despawns rather than accumulating forever.
