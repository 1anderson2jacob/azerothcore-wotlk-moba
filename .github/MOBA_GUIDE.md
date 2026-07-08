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

Then restart worldserver, run `.debug bg` in-game (required every restart or
solo queue won't pop), queue for EotS, and confirm the change.

SQL changes (`data/sql/custom/mod_moba_towers.sql` or ad-hoc `UPDATE`
statements) are applied to `acore_world` separately — that's Jacob's step,
not part of the build.

**Since the tower architecture generalization**: almost everything about a
tower (position, tier/guard dependency, attack range/interval/spell) now
lives in the `mod_moba_tower_data` DB table, loaded once by
`MobaTowerDataStore` at battleground creation — not hardcoded in C++
anymore. This means most tower tuning is now a pure SQL change (no rebuild
needed), but **the data is loaded once and cached** — a running worldserver
won't pick up new/changed rows until it's restarted.

---

## How the tower system works (read this before changing behavior)

- **Registry**: `BattlegroundMOBA` keeps a `std::vector<MobaTowerState>`
  (`BattlegroundMOBA.h`), one entry per spawned tower — team, tier,
  guard dependency, destroyed flag. Built in `SetupBattleground()` from
  `mod_moba_tower_data` (via `MobaTowerDataStore`, `MobaTowerData.h`/`.cpp`).
  Towers no longer get one-named-enum-slot-per-tower; the slot count is
  computed at runtime from however many rows exist.
- **Win condition**: `BattlegroundMOBA::HandleKillUnit` (an engine hook,
  fires automatically whenever a player kills a creature in the BG) marks
  the tower destroyed, unlocks anything it was guarding, updates the
  worldstate counter, and ends the battleground once a team has no
  towers left.
- **Guard/tier dependency**: a tower with `GuardedByEntry` set spawns
  inert (`UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE`, and its AI
  skips its attack tick) until the tower it's guarded by is destroyed.
  `GuardedByEntry` is a single FK — one guard per guarded tower, which
  covers a linear lane (T1 → T2 → T3) but not true multi-guard AND-gating.
- **Targeting rule** (`npc_moba_tower.cpp`), LoL-style: a tower attacks the
  nearest hostile non-player unit ("creep") in range if any are present
  (other towers are explicitly excluded from this so towers never shoot
  each other); otherwise the nearest hostile player. Separately, a global
  hook (`moba_tower_aggro.cpp`, a `UnitScript` — the only one in this
  codebase, watching `OnDamage`/`OnAuraApply` server-wide) detects an enemy
  player damaging or hard-CC'ing an allied player within a tower's range,
  and switches the tower onto that offender. This override only fires as a
  creep → player transition — once locked onto an offender, a *different*
  offending player can't steal aggro; the lock releases when the current
  target leaves range, dies, or becomes untargetable, at which point the
  tower re-evaluates normally (and could lock onto someone else if they
  re-trigger the condition).

---

## How to move a tower

1. In-game (GM character), stand at the new spot and run `.gps`. Note
   `X`, `Y`, `Z`, `Orientation`.
2. `UPDATE mod_moba_tower_data SET PosX = .., PosY = .., PosZ = .., Orientation = .. WHERE CreatureEntry = <entry>;`
3. Apply the SQL, then restart worldserver (data loads once at startup).
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
4. Apply the SQL, restart worldserver (data loads once at startup — this
   is a behavior change from the old "everything is a compiled constant"
   world), `.debug bg`, queue, confirm it spawns.

## How to add a tower with a tier/guard dependency

1. Follow "How to add a new tower" above, but set `GuardedByEntry` to the
   entry of the tower that must die first.
2. The guarded tower spawns inert (unattackable, unselectable, and its AI
   won't fire) until the guard tower is destroyed — no extra steps needed,
   `BattlegroundMOBA::HandleKillUnit` handles the unlock automatically.
3. Apply the SQL, restart worldserver, `.debug bg`, queue, and verify:
   the guarded tower can't be targeted/damaged initially, then becomes
   attackable and starts firing once the guard tower dies.

## How to change tower attack range or tick rate

1. `UPDATE mod_moba_tower_data SET AttackRange = .., AttackIntervalMs = .. WHERE CreatureEntry = <entry>;`
2. Apply the SQL, restart worldserver, `.debug bg`, queue, confirm.

## How to change the tower projectile/spell

1. `UPDATE mod_moba_tower_data SET AttackSpellId = .. WHERE CreatureEntry = <entry>;`
2. Note: the current default (9053, a "Shoot" clone) was chosen because it
   deals real damage with no weapon dead zone, but its missile doesn't
   render on the current tower display models (likely no bone attachment
   point on these prop-style models). If your replacement spell has the
   same visual problem, that's a display-model limitation, not a data
   issue — update this note with whatever you find that does render.
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

## How to move the graveyard / player spawn-in point

The graveyard location is *also* where players first teleport in when they
queue into the battleground — one DB row drives both.

1. In-game, `.gps` at the new ground-level spawn spot for
   `X, Y, Z, Orientation`.
2. Write (don't run — deliver as SQL for Jacob to apply) an `UPDATE`
   against `game_graveyard`:
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
4. Apply the SQL to `acore_world`.
5. Build, install & test (see above) — confirm both the spirit guide and
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

Tower position/tier/range/interval/spell are now seeded via SQL — see
`data/sql/custom/mod_moba_towers.sql`'s `mod_moba_tower_data` INSERT for
current values, not this table.

| What | Value |
|---|---|
| Alliance door/spawn/graveyard | `2387.529, 1587.426, 1174.763`, o=`3.0222116` |
| Horde door/spawn/graveyard | `1942.9327, 1547.6229, 1176.458`, o=`0.32122585` |
| Graveyard DB IDs | 1103 (Alliance), 1104 (Horde) — reused vanilla EotS rows |
| Custom creature entry range | 900000+ (900000 = Alliance tower, 900001 = Horde tower) |

## Known gotchas (not tied to one recipe)

- `BgObjects` (doors) and the 2 fixed spirit-guide slots are still sized
  by a contiguous enum — towers are no longer part of that (they're a
  runtime-sized range computed from `mod_moba_tower_data`'s row count).
- If you ever touch `SetupBattleground()` again: `BgCreatures.resize(...)`
  must happen *before* the first `AddCreature`/`AddSpiritGuide` call
  (`AddCreature` asserts the slot already exists) — it's the first thing
  the function does now, don't move it below the door/graveyard calls.
- `mod_moba_tower_data` (and anything else `MobaTowerDataStore` loads) is
  read once at battleground creation and cached — a running worldserver
  needs a restart to see new/changed rows, it's not a live-reloadable
  table.
- `creature_template` on this DB revision has no `scale` column; scale
  lives in `creature_template_model.DisplayScale`.
- `AddCreature` signature: `(entry, type, x, y, z, o, respawntime = 0,
  transport = nullptr)` — no TeamId param; faction comes from
  `creature_template`.
- `HandleKillUnit` (the win-condition/tower-destroyed hook) only fires on
  player-attributed kills. Once lane creeps exist and might land the
  killing blow on a tower, this won't fire for that case — known
  limitation, not yet fixed.
- If behavior contradicts what the code says it should do, `grep` to
  confirm what's actually on disk before going deeper — bitten once by an
  unsaved editor buffer.
