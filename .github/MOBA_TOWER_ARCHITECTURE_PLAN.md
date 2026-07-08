# MOBA tower architecture generalization

## Context

Today there's exactly one tower per team, hardcoded via an enum + literal
`AddCreature` calls, with hardcoded AI constants. The real target is
multiple towers per team with tier/guard dependencies and LoL-style
targeting. This plan generalizes the architecture now, seeded so behavior
with today's 2 towers is unchanged.

## Verified building blocks (all confirmed by reading the actual code, not guessed)

- `AddCreature`'s only requirement is `ASSERT(type < BgCreatures.size())`
  — the resize can be runtime/DB-driven as long as it happens before the
  first `AddCreature`/`AddSpiritGuide` call (so it moves from the
  constructor to the top of `SetupBattleground()`).
- The "inert until prerequisite falls" pattern already exists twice in
  this codebase: `BattlegroundSA::DemolisherStartState` and
  `BattlegroundIC`'s hangar cannons, both toggling
  `UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE`.
- Win condition uses `Battleground::HandleKillUnit`, mirroring
  `BattlegroundIC::HandleKillUnit`.
- Objective feedback reuses `BattlegroundEY::AddPoints`'s exact idiom
  (`m_TeamScores` + `UpdateWorldState`).
- Aggro-override detection uses two confirmed-real global hooks:
  `UnitScript::OnDamage` and `UnitScript::OnAuraApply` — no existing
  `UnitScript` subclass exists in this codebase, so this is new ground,
  registered like any other script (`new ClassName()` in an `AddSC_*`
  loader).
- Reaching a specific tower's AI from the new global hook uses the
  existing `ENSURE_AI` macro.
- "Nearest enemy creep" search mirrors the existing player-search pattern
  with `Acore::CreatureListSearcher` + `Acore::AnyUnitInObjectRangeCheck`.
- Hard-CC detection uses `SpellInfo::GetAllEffectsMechanicMask()` against
  a new constant (not the existing movement-impairment mask as-is, since
  that includes mere snares/dazes and excludes silence — flagged as a
  judgment call, recommend including silence).

**Two flagged judgment calls, not blockers**:

1. `GuardedByEntry` is a single FK — fine for a linear lane, not true
   multi-guard AND-gating; upgrade path is a join table later if needed.
2. `HandleKillUnit` only fires on player-attributed kills — once creeps
   exist and might land the killing blow, this silently won't fire; not
   fixable meaningfully today.

## Implementation, by file

1. `data/sql/custom/mod_moba_towers.sql` — new table
   `mod_moba_tower_data` (`CreatureEntry`, `Team`, `Tier`,
   `GuardedByEntry`, `PosX`/`PosY`/`PosZ`/`Orientation`, `AttackRange`,
   `AttackIntervalMs`, `AttackSpellId`), seeded with today's 2 towers
   unchanged.
2. New `MobaTowerData.h`/`.cpp` — `MobaTowerDataStore` singleton, loads
   the table once, `GetConfig(entry)`/`GetAll()`.
3. `BattlegroundMOBA.h` — trim the tower-specific enum values, add
   `MobaTowerState`/`_towers` registry, add `HandleKillUnit` override.
4. `BattlegroundMOBA.cpp` — dynamic `BgCreatures` resize in
   `SetupBattleground()` (moved out of the constructor), spawn towers
   from the data store into the registry, apply guard/inert flags,
   implement `HandleKillUnit` (mark destroyed, unlock guarded towers,
   update worldstate, `EndBattleground` when a team has none left).
5. New `npc_moba_tower.h` — one exported function, `MobaTowerAggroOverride`.
6. `npc_moba_tower.cpp` — per-instance config from the data store, inert
   check, unified target-validity/reselection logic, new creep-search,
   `TryAggroOverride`.
7. New `moba_tower_aggro.cpp` — the `UnitScript` subclass wiring
   `OnDamage`/`OnAuraApply` to the aggro-override, with cheap early-outs
   before any registry walk.
8. `custom_script_loader.cpp` — register the new script.
9. `.github/README.md` — add a roadmap line for custom map/terrain,
   flagged as a likely prerequisite for the standalone-battleground-ID
   line.
10. `.github/MOBA_GUIDE.md` — new recipes for tier/guard towers and
    changing the hard-CC mask; update the now-obsolete enum-editing
    recipes to point at the DB table instead.

## Verification

Manual, in-game — no existing automated BG tests in this repo:

1. Build/install/restart, confirm both towers unchanged at their
   positions.
2. Kill a tower and confirm the correct team wins.
3. Seed a test 3rd tower with a `GuardedByEntry` to confirm
   inert→active transition.
4. Verify the aggro-override lock/release behavior, including the
   no-stealing-between-offenders rule.
5. Confirm the worldstate counter ticks up.