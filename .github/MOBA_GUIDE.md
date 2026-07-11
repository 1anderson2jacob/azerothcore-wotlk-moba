# MOBA Battleground — How-To Guide

Step-by-step recipes for common changes to the MOBA battleground. This is a
living document — add a new recipe here whenever we make a change of a kind
not already covered. For project rules and working style, see `CLAUDE.md`.
For the roadmap, see `.github/README.md`.

Every recipe ends with the same deploy/test loop — see **Build, Install &
Test** below, referenced from each recipe instead of repeated in full.

---

## Build, Install & Test

Run after any code or header change described below:

```bash
cd var/build/obj
make -j$(sysctl -n hw.ncpu)   # add `cmake .` first only if files were added/removed
make install                  # required — binaries don't reach env/dist/bin without it
```

Then **fully restart worldserver** (not just `.debug bg` + requeue — see the
data-caching gotcha below), run `.debug bg` in-game (required every restart
or solo queue won't pop), queue for EotS, and confirm the change.

SQL changes (`data/sql/custom/mod_moba_towers.sql`, `mod_moba_creeps.sql`, or
ad-hoc `UPDATE` statements) are applied to `acore_world` as a separate step,
outside the build.

**Both towers and creeps are almost entirely data-driven** — position,
tier/guard dependency, attack range/interval/spell all live in
`mod_moba_tower_data`/`mod_moba_creep_data`, loaded by
`MobaTowerDataStore`/`MobaCreepDataStore`. This means most tuning is a pure
SQL change, no rebuild needed — **but see the caching gotcha below before
assuming a SQL-only change takes effect on the next queue.**

---

## How the tower system works (read this before changing behavior)

- **Registry**: `BattlegroundMOBA` keeps a `std::vector<MobaTowerState>`
  (`BattlegroundMOBA.h`), one entry per spawned tower — team, tier, guard
  dependency, destroyed flag. Built in `SetupBattleground()` from
  `mod_moba_tower_data` (via `MobaTowerDataStore`, `MobaTowerData.h`/`.cpp`).
  Towers no longer get one-named-enum-slot-per-tower; the slot count is
  computed at runtime from however many rows exist.
- **Win condition**: `BattlegroundMOBA::OnTowerDestroyed` (called from both
  `HandleKillUnit`, for player-attributed kills, and `npc_moba_tower::JustDied`,
  for creep-attributed kills — see the creep section below) marks the tower
  destroyed, unlocks anything it was guarding, updates the worldstate
  counter, and ends the battleground once a team has no towers left.
- **Guard/tier dependency**: a tower with `GuardedByEntry` set spawns inert
  (`UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE`, and its AI skips
  its attack tick) until the tower it's guarded by is destroyed.
  `GuardedByEntry` is a single FK — one guard per guarded tower, which
  covers a linear lane (T1 → T2 → T3) but not true multi-guard AND-gating.
- **Targeting rule** (`npc_moba_tower.cpp`), LoL-style, and **deliberately
  different from creeps** (see below): `REACT_PASSIVE` + fully manual
  targeting, immune to taunt/kiting. A tower attacks the nearest hostile
  non-player unit ("creep") in range if any are present (other towers are
  explicitly excluded from this so towers never shoot each other);
  otherwise the nearest hostile player. Separately, a global hook
  (`moba_tower_aggro.cpp`, a `UnitScript` — the only one in this codebase,
  watching `OnDamage`/`OnAuraApply` server-wide) detects an enemy player
  damaging or hard-CC'ing an allied player within a tower's range, and
  switches the tower onto that offender. This override only fires as a
  creep → player transition — once locked onto an offender, a *different*
  offending player can't steal aggro; the lock releases when the current
  target leaves range, dies, or becomes untargetable, at which point the
  tower re-evaluates normally.

## How the creep wave system works (read this before changing behavior)

- **Wave timer**: `BattlegroundMOBA`'s `_bgEvents` `EventMap` schedules
  `EVENT_MOBA_SPAWN_WAVE` every 30s (starting when the doors open), spawning
  a wave for both teams simultaneously. Every 3rd wave (`_waveCount % 3 == 0`)
  also includes a siege unit.
- **Spawning**: creeps are `TempSummon`s (`TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT`,
  auto-despawn after `DespawnMs` of not being in combat), **not** the
  tower's persistent `AddCreature`/`BgCreatures` registry — that mechanism
  is for a small, permanent roster, wrong fit for repeatedly-spawned,
  short-lived minions. `BattlegroundMOBA::_spawnedCreeps` tracks every
  spawned guid (for the end-of-game freeze, see below) but there's no
  "tower-style" per-creep state tracking beyond that.
- **Formation**: melee spawns front and side-by-side, caster behind, siege
  further back on siege waves. Achieved via **dedicated waypoint paths per
  formation slot** (`900110`/`900111`/`900112` etc.), not spawn-position
  offsets on a shared path — `WaypointMovementGenerator` always targets
  node 1 of whatever path it's given first, *regardless of where the
  creature actually spawned*, so any two units sharing one path with
  different spawn offsets will both walk toward that same first node and
  collide/reorder. Each formation slot's path has its own node 1 positioned
  exactly at that slot's spawn point to avoid this.
- **Targeting rule**, LoL-style, and **deliberately different from
  towers**: creeps use the engine's real `REACT_AGGRESSIVE` threat/combat
  (not a manual override) — attacks nearest hostile "creep" if present,
  else nearest hostile player; automatically re-targets on death/CC/attack
  from a new source, all via the engine's default `ThreatManager`, no
  custom code needed for that part. Casters override `AttackStart` to call
  `AttackStartCaster(victim, range)` instead of the default (which would
  otherwise close to melee range like a normal mob) — holds at their
  configured cast range instead.
- **Leashing — lane corridor, no run-back (LoL-style)**: a creep only
  attacks players within 40 yd of its own lane (`MOBA_CREEP_LANE_CORRIDOR`
  in `npc_moba_creep.cpp`, measured to the nearest node of its waypoint
  path — deliberately NOT to home position, see gotchas). Post-combat
  there is no WoW-style run-back: the creep resumes its lane from the
  node nearest to where combat ended, and never behind the furthest node
  it already reached (waves can't be walked backwards for free;
  re-dragging by actively attacking still works, as in LoL). Damage
  persists between fights (`RegenHealth = 0`), and lane paths are
  non-repeating — a creep that reaches the enemy end stands and fights
  there instead of walking the lane home. Mid-route resume uses a
  fork-added engine API, `MotionMaster::MoveWaypoint(WaypointPath&, bool)`
  (the id-based overload can only start at node 1).
- **Win condition tie-in**: `npc_moba_tower::JustDied` checks whether the
  killer is a non-player creature (`killer->GetCharmerOrOwnerPlayerOrPlayerItself()`
  is null) and, if the killer is a registered creep, calls the same
  `BattlegroundMOBA::OnTowerDestroyed` that `HandleKillUnit` calls for
  player kills — so a tower killed entirely by minions still ends the
  battleground correctly.
- **End-of-game freeze**: `BattlegroundMOBA::FreezeAllCreeps()` (called from
  `OnTowerDestroyed` right before `EndBattleground`) iterates
  `_spawnedCreeps` and calls `CombatStop()` + `SetReactState(REACT_PASSIVE)`
  + `MoveIdle()` on each living one — stops all movement and combat the
  instant the match ends. New wave spawning already stops on its own
  (`PostUpdateImpl`'s event loop is gated on `STATUS_IN_PROGRESS`, which
  `EndBattleground` clears immediately), so this only had to handle
  *existing* creeps. The creep AI also suppresses `Reset()`/evade behavior once the match is over (`MatchEnded()`), because a post-freeze evade would otherwise re-arm the lane path — see the evade gotcha below.
- **Creature stats — full-copy philosophy**: all 8 creep entries (melee ×2
  per team, caster, siege) are **full copies of every `creature_template`
  column** from a real reference creature (Battleguard for melee, Dalaran
  Mage/Warsong Shaman for casters, IC's Demolisher for siege), with only a
  small, deliberate set of fields overridden — not a minimal/reduced
  column list. This was a deliberate correction after the reduced-list
  approach (originally copied from how towers were built) caused a real
  bug (`BaseAttackTime` defaulting to 0, breaking melee auto-attack
  timing) — copying everything and overriding only what's necessary avoids
  a whole class of "forgot to carry over an obscure but load-bearing
  field" bugs. See "How to add a new creep type" below for the exact
  override list and the two additional gotchas this approach caught
  (`AIName`/SmartAI conflict, siege's mount flag).
  The full-copy + override checklist is now enforced mechanically by
  `apps/moba/gen_creep_roster.py`, which owns `mod_moba_creeps.sql` — see
  `apps/moba/README.md`.


---

## How to move a tower

1. In-game (GM character), stand at the new spot and run `.gps`. Note
   `X`, `Y`, `Z`, `Orientation`.
2. `UPDATE mod_moba_tower_data SET PosX = .., PosY = .., PosZ = .., Orientation = .. WHERE CreatureEntry = <entry>;`
3. Apply the SQL, then fully restart worldserver (see caching gotcha).
4. `.debug bg`, queue, confirm the new position.

## How to add a new tower

1. In-game, `.gps` at the new spot for `X, Y, Z, Orientation`.
2. Add rows to `data/sql/custom/mod_moba_towers.sql`, following the
   existing DELETE-before-INSERT pattern:
   - A `creature_template` row for the new entry (copy an existing tower
     row's columns: faction, `HealthModifier`, `unit_flags`, etc.,
     `ScriptName = 'npc_moba_tower'`).
   - A `creature_template_model` row (display ID + `DisplayScale`).
   - A `mod_moba_tower_data` row: `CreatureEntry`, `Team`, `Tier` (0 unless
     it's guarded by another tower), `GuardedByEntry` (0 unless gated),
     your `.gps` position, and attack range/interval/spell (or leave the
     column defaults).
3. No C++ or enum changes needed — the tower registry and `BgCreatures`
   slot count are both fully data-driven now.
4. Apply the SQL, fully restart worldserver, `.debug bg`, queue, confirm
   it spawns.

## How to add a tower with a tier/guard dependency

1. Follow "How to add a new tower" above, but set `GuardedByEntry` to the
   entry of the tower that must die first.
2. The guarded tower spawns inert (unattackable, unselectable, and its AI
   won't fire) until the guard tower is destroyed — no extra steps needed,
   `BattlegroundMOBA::OnTowerDestroyed` handles the unlock automatically.
3. Apply the SQL, restart worldserver, `.debug bg`, queue, and verify: the
   guarded tower can't be targeted/damaged initially, then becomes
   attackable and starts firing once the guard tower dies.

## How to change tower attack range or tick rate

1. `UPDATE mod_moba_tower_data SET AttackRange = .., AttackIntervalMs = .. WHERE CreatureEntry = <entry>;`
2. Apply the SQL, restart worldserver, `.debug bg`, queue, confirm.

## How to change the tower projectile/spell

1. `UPDATE mod_moba_tower_data SET AttackSpellId = .. WHERE CreatureEntry = <entry>;`
2. Tower damage isn't an independent stat — it's entirely whatever the
   configured spell deals. To tune damage without changing the visual,
   pick a different rank/tier of the same spell family (same look,
   different numbers), or adjust `AttackIntervalMs` (faster/slower tick
   rate changes effective DPS without touching per-hit damage).
3. Note: the current default (9053, a "Shoot" clone) was chosen because it
   deals real damage with no weapon dead zone, but its missile doesn't
   render on the current tower display models (likely no bone attachment
   point on these prop-style models). Towers also cast it with
   `triggered=true` (see "How to make a creep's attack instant/free vs. a
   real cast" below) — deliberately instant and free, matching a
   stationary turret's flavor.
4. Apply the SQL, restart worldserver, `.debug bg`, queue, confirm.

## How to change tower health

1. `UPDATE creature_template SET HealthModifier = .. WHERE entry = <entry>;`
2. `HealthModifier` is a multiplier on the creature's level-based base
   health — not an absolute HP value.
3. Apply the SQL, restart worldserver, `.debug bg`, queue, confirm.

## How to change what counts as a hard-CC trigger for tower aggro

1. Open `src/server/scripts/Custom/moba_tower_aggro.cpp`, edit the
   `MOBA_HARD_CC_MECHANIC_MASK` constant near the top of the file.
2. Current definition: the engine's `IMMUNE_TO_MOVEMENT_IMPAIRMENT_AND_LOSS_CONTROL_MASK`
   minus `MECHANIC_SNARE`/`MECHANIC_DAZE` (mere slows, not "hard" CC), plus
   `MECHANIC_SILENCE` (not in the base mask, but included here since loss
   of ability to act is the spirit of the rule) — this is a judgment call,
   revisit if the aggro-override trigger feels too loose or too tight in
   practice.
3. Build, install & test (see above) — this is a C++ change, needs a
   rebuild, not just a SQL update.

## How to move a creep waypoint path

Lane paths are generated by `apps/moba/gen_creep_paths.py` — don't
hand-write `waypoint_data` rows.

1. Walk the lane in-game, running `.gps` every ~15–20 yards (more often
   through curves, and over bumps/dips — node Z is interpolated linearly
   between your points). Copy the console scrollback to a text file.
2. `python3 apps/moba/gen_creep_paths.py --extract scrollback.txt` prints
   the points array; paste it into the lane's `points` in
   `apps/moba/lane_config.json`. **Order matters**: the team wired to the
   `forward` path IDs (currently Alliance) spawns at the FIRST point —
   reverse the array if you walked the other way.
3. Run `python3 apps/moba/gen_creep_paths.py` from the repo root and
   review the output (`data/sql/custom/mod_moba_creep_paths.sql`). Path
   IDs are reused from the lockfile, so re-walking an existing lane needs
   no `mod_moba_creep_data` changes.
4. Apply the SQL, fully restart worldserver, `.debug bg`, queue, confirm.

Full config/lockfile reference: `apps/moba/README.md`.

## How to add a new creep type

`mod_moba_creeps.sql` is GENERATED by `apps/moba/gen_creep_roster.py` —
don't hand-edit it. Creeps are described in `apps/moba/creep_config.json`;
the generator full-copies a real source creature's stats and enforces the
override checklist that used to live in this recipe (AIName/ScriptName,
loot/npcflag/VehicleId/difficulty-entry references and IconName cleared,
RegenHealth=0, faction from team, level pinning), so none of it can be
forgotten by hand.

1. Dump the source creature:
   `mysql -E -u acore -pacore acore_world -e "SELECT * FROM creature_template WHERE entry=<id>" > apps/moba/sources/creature_template_<id>.txt`
2. Add a block to `creep_config.json` (copy a similar role's), including
   equipment item IDs from the source's `creature_equip_template` row if
   it has one — weapons aren't part of `creature_template`, and without
   them melee swing unarmed.
3. If it needs a new formation slot, add the slot to `lane_config.json`
   and run `gen_creep_paths.py` first — the roster generator resolves
   `WaypointPathId` from the lane lockfile (team 0 = forward path,
   team 1 = reverse).
4. `python3 apps/moba/gen_creep_roster.py`, apply the SQL, fully restart
   worldserver, `.debug bg`, queue, confirm.

Full workflow and config field reference: `apps/moba/README.md`. Note
`BattlegroundMOBA` expects exactly 2 melee + 1 caster per team (siege
optional) — the generator warns when the composition differs.

## How to change wave cadence or composition

1. Spawn interval: `_bgEvents.ScheduleEvent(EVENT_MOBA_SPAWN_WAVE, Milliseconds(30000));`
   in `BattlegroundMOBA.cpp` (two call sites: `StartingEventOpenDoors()` for
   the first wave, and the reschedule at the end of the
   `EVENT_MOBA_SPAWN_WAVE` case in `PostUpdateImpl()` — both need to change
   together).
2. Siege cadence: `bool includeSiege = (_waveCount % 3 == 0);` in
   `PostUpdateImpl()` — change the `3` to whatever cadence you want.
3. Unit counts per wave: `SpawnWave()` currently hardcodes 2 melee + 1
   caster + (conditionally) 1 siege — to change counts, edit the calls in
   that function directly (not data-driven currently).
4. Build, install & test (see above) — this is a C++ change.

## How to change a creep's attack range/interval/spell

1. Edit the creep's entry in `apps/moba/creep_config.json`
   (`attack_range`/`attack_interval_ms`/`attack_spell_id` — casters only;
   melee/siege attack speed comes from the source creature's
   `BaseAttackTime`).
2. `python3 apps/moba/gen_creep_roster.py`, apply the SQL, restart
   worldserver, `.debug bg`, queue, confirm.
3. Ad-hoc `UPDATE mod_moba_creep_data ...` against the DB works for live
   experimentation, but fold final values back into `creep_config.json` —
   the next apply of the generated file reverts anything not recorded
   there.

## How to change the lane-corridor width (creep anti-kite leash)

1. Edit `MOBA_CREEP_LANE_CORRIDOR` at the top of
   `src/server/scripts/Custom/npc_moba_creep.cpp` (yards from the lane;
   the self-evade safety check adds +15 headroom on top of it, and the
   rule gates player targets only — creeps/towers are always attackable).
2. Build, install & test (see above) — this is a C++ change.

## How to make a creep's attack instant/free vs. a real cast

1. In `npc_moba_creep.cpp`'s `CastAtVictim`, the call is
   `DoCastVictim(_cfg->spellId, triggered)`.
2. `triggered = true` bypasses cast time, mana cost, and GCD entirely —
   *regardless of what the spell's own data says* — this is why it was the
   right call for towers (want instant/free turret fire) but the *wrong*
   call for casters wanting a real cast bar and mana cost (we shipped this
   bug initially: real spells like Fireball/Lightning Bolt looked instant
   and free until this was changed to `false`).
3. `triggered = false` lets the spell behave normally (real cast time,
   real mana cost, and does its own internal range check — keep the
   `IsWithinDist` pre-check too, it's a cheap filter before even attempting
   the cast).
4. Build, install & test (see above) — this is a C++ change, applies to
   *all* casters (not data-driven per-entry currently).

## How to move the graveyard / player spawn-in point

The graveyard location is *also* where players first teleport in when they
queue into the battleground — one DB row drives both.

1. In-game, `.gps` at the new ground-level spawn spot for
   `X, Y, Z, Orientation`.
2. Run against `acore_world`:
   ```sql
   UPDATE game_graveyard SET x = <X>, y = <Y>, z = <Z> WHERE ID = 1103; -- Alliance
   UPDATE game_graveyard SET x = <X>, y = <Y>, z = <Z> WHERE ID = 1104; -- Horde
   ```
   (1103/1104 are our reused vanilla-EotS graveyard IDs — no need to touch
   `battleground_template.AllianceStartLoc`/`HordeStartLoc`, they already
   point at 1103/1104 and don't need to change.)
3. Open `BattlegroundMOBA.cpp`'s `SetupBattleground()` and update the
   orientation literal in the matching `AddSpiritGuide(...)` call (it's a
   separate hardcoded float, not read from `game_graveyard`).
4. Build, install & test (see above) — confirm both the spirit guide and
   your initial teleport-in land at the new spot.

**Gotcha**: `battleground_template.AllianceStartLoc`/`HordeStartLoc`
(row ID 7 = EotS) looks like it should reference `WorldSafeLocs.dbc`
(that's even what the log message says on a bad ID), but it doesn't — it's
resolved via `sGraveyard->GetGraveyard()`, which only reads the
`game_graveyard` DB table. **Don't edit `WorldSafeLocs.dbc` for this** —
wasted a whole detour learning that.

## How to move the starting-area door (the visual "dome")

The door gameobject *is* the dome/forcefield players see before the match
starts.

1. In-game, `.gps` at the new spot for `X, Y, Z, Orientation`.
2. Compute a yaw-only quaternion from the orientation (`.gps` doesn't give
   you one directly):
   `rotation0 = 0`, `rotation1 = 0`,
   `rotation2 = sin(o/2)`, `rotation3 = cos(o/2)`.
3. Open `BattlegroundMOBA.cpp`'s `SetupBattleground()` and update the
   `AddObject(BG_MOBA_OBJECT_DOOR_A/H, ...)` call: `x, y, z, o` and the
   four rotation values.
4. Build, install & test (see above) — check the dome position and angle.
   If it looks tilted wrong, that's the limit of the flat-yaw
   approximation — the original EotS doors used a tilted quaternion baked
   to match sloped terrain, which `.gps` can't give you; nudge visually
   in-game if it matters enough to fix by trial and error.

---

## Reference: current values

Tower and creep position/tier/range/interval/spell are seeded via SQL —
see `data/sql/custom/mod_moba_towers.sql` (`mod_moba_tower_data`) and
`mod_moba_creeps.sql` (`mod_moba_creep_data`) for current values, not this
table.

| What | Value |
|---|---|
| Alliance door/spawn/graveyard | `2387.529, 1587.426, 1174.763`, o=`3.0222116` |
| Horde door/spawn/graveyard | `1942.9327, 1547.6229, 1176.458`, o=`0.32122585` |
| Graveyard DB IDs | 1103 (Alliance), 1104 (Horde) — reused vanilla EotS rows |
| Custom tower entry range | 900000-900001 |
| Custom creep entry range | 900010-900017 (melee ×2/team, caster, siege) |
| Custom waypoint path ID range | 900100-900122 (base lanes + per-formation-slot paths) |
| Wave cadence | Every 30s; every 3rd wave adds a siege unit |
| Creep lane corridor | 40 yd from lane, players only; +15 yd self-evade headroom |
| Creep health regen | RegenHealth = 0 — damage persists between fights |

## Known gotchas (not tied to one recipe)

- **Data stores cache once per *worldserver process*, not per battleground
  instance.** `MobaTowerDataStore`/`MobaCreepDataStore` both guard their
  load with `if (_loaded) return;` on a singleton that lives for the whole
  process lifetime — so re-applying SQL and just `.debug bg` + requeuing on
  an already-running worldserver does **nothing**; the in-memory data is
  frozen from whenever it first loaded. Reinstalling a new binary doesn't
  help either (the already-running process keeps using its old in-memory
  data regardless of what's on disk). **A full worldserver restart is
  required after any SQL change to these tables** — this bit us for real
  during creep-wave testing (looked like a caster spell wasn't updating;
  the SQL was actually fine, the running server just hadn't reloaded it).
- **`WaypointMovementGenerator` always targets node 1 of its path first**,
  regardless of the creature's actual spawn position — don't put multiple
  units on the same shared path with different spawn offsets expecting
  them to hold formation; they'll all walk to that first node and collide.
  Use separate paths per formation slot instead (see the creep formation
  note above).
- **Don't anchor logic to a waypoint-walker's home position.** On this
  revision `WaypointMovementGenerator::DoUpdate` stamps home = the
  creature's *current position* on every moving tick (plus on node
  arrivals) — "home" is wherever the creature last walked, not a stable
  lane anchor. A corridor/leash check measured from home follows the
  creature wherever a player drags it (shipped as a real bug: the check
  re-passed after every ~30 yd hop, all the way to the map edge). Measure
  against the path geometry instead (`DistanceFromLane2d` in
  `npc_moba_creep.cpp`). Dense (~5 yd) waypoint nodes still matter: the
  engine's own leash checks anchor to waypoint-generator positions, and
  sparse nodes leave those anchors far from the creature.
- **The engine's 30 yd leash check is skipped while combat stays
  "fresh".** `Creature::CanCreatureAttack` deliberately allows kiting:
  taking/dealing damage, being within melee range, and failing to reach
  the target all refresh a ~17 s extension window during which the
  home-distance check never runs — a fleeing player at run speed keeps it
  refreshed indefinitely. That's why creeps enforce their own corridor in
  `CanAIAttack`, which `CanCreatureAttack` consults *before* the
  extension window.
- **A chase does not end just because the target became invalid.**
  `Creature::SelectVictim` can return null *without* evading or stopping
  the attack (e.g. while anything still has the creature on its threat
  list), leaving the chase running with a stale victim. Never assume
  "target unattackable ⇒ evade fires."
- **`LOG_INFO` in a custom log category is silently dropped.**
  `Logger.root` in worldserver.conf is ERROR-level; only categories with
  explicit `Logger.<name>` lines (like `server.*`) pass INFO. When adding
  debug logging, also add e.g. `Logger.bg.battleground=4,Console Server`
  to worldserver.conf — nothing appears otherwise (cost us a build cycle
  to discover).
- `BgObjects` (doors) and the 2 fixed spirit-guide slots are still sized
  by a contiguous enum — towers/creeps are not part of that (towers are a
  runtime-sized range from `mod_moba_tower_data`'s row count; creeps are
  ephemeral `TempSummon`s, not registered in `BgCreatures` at all).
- If you ever touch `SetupBattleground()` again: `BgCreatures.resize(...)`
  must happen *before* the first `AddCreature`/`AddSpiritGuide` call
  (`AddCreature` asserts the slot already exists) — it's the first thing
  the function does now, don't move it below the door/graveyard calls.
- `creature_template` on this DB revision has no `scale` column; scale
  lives in `creature_template_model.DisplayScale`.
- `AddCreature` signature: `(entry, type, x, y, z, o, respawntime = 0,
  transport = nullptr)` — no TeamId param; faction comes from
  `creature_template`.
- `DoCastVictim(spellId, triggered)` — `triggered = true` bypasses cast
  time, mana, and GCD entirely regardless of the spell's own data. See
  "How to make a creep's attack instant/free vs. a real cast" above.
- When basing a new custom creature on a real reference creature, prefer
  copying its **full** `creature_template` row and overriding only what's
  necessary, rather than a minimal/reduced column list — a reduced list
  already caused one real bug (`BaseAttackTime` silently defaulting to 0)
  and would have caused at least two more (`AIName`/SmartAI conflict on
  caster sources, a leftover mount flag on vehicle-flavored sources) had
  they not been caught by tracing through a full copy. See "How to add a
  new creep type" above for the exact override list.
- `OnTowerDestroyed` (shared by `HandleKillUnit` for player kills and
  `npc_moba_tower::JustDied` for creep kills) only fires when the source
  hooks actually run — `HandleKillUnit` requires a player-attributed
  killer, `JustDied`'s creep path requires the killer to be a registered
  `mod_moba_creep_data` entry. A tower killed by something outside both
  categories (environmental damage, a future non-creep source) wouldn't
  trigger the win condition — not currently a real scenario, flagging for
  awareness if the damage sources here ever expand.
- If behavior contradicts what the code says it should do, `grep` to
  confirm what's actually on disk before going deeper — bitten once by an
  unsaved editor buffer.
- **Evade undoes "stop this creature" logic.** `CreatureAI::EnterEvadeMode`
  synchronously calls `MoveTargetedHome()` (run back to the stale
  last-reached waypoint node) and then `Reset()` — so freezing a creature
  (e.g. `FreezeAllCreeps()` at match end) does not survive a later evade
  unless the AI itself knows to stay down: a creep still in combat at the
  freeze (or attacked afterward) will evade and un-freeze itself. This
  shipped as a real bug once. `npc_moba_creep` guards `Reset()`,
  `UpdateAI()`, and `EnterEvadeMode` behind a `MatchEnded()` check (BG
  status no longer `STATUS_IN_PROGRESS`) — reuse that pattern for any
  future "stop everything" behavior.