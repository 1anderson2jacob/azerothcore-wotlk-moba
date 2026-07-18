# MOBA Battleground — How-To Guide

Recipes for changing the MOBA battleground, plus the architecture you need to
read first. Project rules and where files live: `CLAUDE.md`. Roadmap:
`.github/README.md`. Doc conventions: CLAUDE.md's "Documentation standards" —
in short, engine quirks live in code comments, and this file links to them
rather than restating them.

Add a recipe here whenever we make a kind of change not already covered.

---

## The deploy loop

Every recipe below ends here. Three kinds of change, three paths.

**C++ / header change:**

```bash
cd var/build/obj
cmake .                        # only if files were added or removed
make -j$(sysctl -n hw.ncpu)
make install                   # binaries don't reach env/dist/bin without it
```

**SQL change** — the generated files live under `data/sql/custom/db_world/` and
auto-apply on the next worldserver boot (the DB updater), so a SQL change just
means **fully restart worldserver**. The full restart is not optional: the
`Moba*DataStore` singletons load once per
*process* (`if (_loaded) return;`), so re-applying SQL and re-queueing against a
running server does nothing, and reinstalling the binary doesn't help either —
the running process keeps its in-memory copy. This has burned real debugging
time (a caster spell "not updating" that was actually a stale server).

**Client addon change** (`client/addons/*`) — copy the folder into the client's
`Interface/AddOns/`, then `/reload`. No build, no restart.

Then in-game: `.debug bg` (**required after every restart**, or the solo queue
won't pop), queue for EotS, confirm.

**Ad-hoc `UPDATE`s** on `mod_moba_*` are fine for live experimentation, but fold
the final values back into `apps/moba/maps/<mode>/*.json` — the generated SQL is
the source of truth and the next apply reverts anything not recorded there.

---

## Architecture

### Towers

`BattlegroundMOBA` holds a `std::vector<MobaTowerState>` registry (team, tier,
guard dependency, destroyed flag), built in `SetupBattleground()` from
`mod_moba_tower_data`. The slot count is runtime-sized from the row count —
no per-tower enum.

- **Win condition**: `OnTowerDestroyed` marks the tower destroyed, unlocks
  whatever it guarded, updates the worldstate counter, and ends the battleground
  once a team has no towers left. Two callers: `HandleKillUnit` (player kills)
  and `npc_moba_tower::JustDied` (creep kills).
- **Guard/tier**: a tower with `GuardedByEntry` spawns unattackable and
  unselectable, and its AI skips its tick, until its guard tower dies.
  `GuardedByEntry` is a single FK — linear lanes only, no multi-guard AND-gating.
- **Targeting** (`npc_moba_tower.cpp`), deliberately *unlike* creeps:
  `REACT_PASSIVE` plus fully manual targeting, immune to taunt and kiting.
  Nearest hostile non-player in range, else nearest hostile player; never another
  tower. Separately, `moba_tower_aggro.cpp` — a global `UnitScript`, the only one
  in this codebase — switches a tower onto an enemy player who damages or
  hard-CCs an ally in range. That override is a creep→player transition only: the
  lock never transfers between offenders, and releases when the target leaves
  range, dies, or becomes untargetable.

### Lane creeps

Waves spawn every 30s from `_bgEvents` (`EVENT_MOBA_SPAWN_WAVE`), both teams at
once, siege on every 3rd. Creeps are `TempSummon`s tracked in `_spawnedCreeps`,
not the persistent `BgCreatures` registry — see the comment on `SpawnCreep` for
the two engine traps there.

- **Targeting**, deliberately *unlike* towers: real `REACT_AGGRESSIVE`
  threat/combat, so the engine's `ThreatManager` handles re-targeting on
  death/CC/new attacker with no custom code. Casters override `AttackStart` to
  hold at cast range instead of closing to melee.
- **Formation** comes from a dedicated waypoint path per slot, not spawn offsets
  on a shared path: `WaypointMovementGenerator` always targets node 1 of whatever
  path it's given, regardless of where the creature actually spawned, so two
  units sharing a path walk to the same node and collide. Each slot's path has
  its own node 1 at that slot's spawn point.
- **Leashing** (LoL-style, no run-back): a creep only attacks players within
  `MOBA_CREEP_LANE_CORRIDOR` of its own lane, resumes the lane from where combat
  ended, never regresses past its furthest node, keeps damage between fights
  (`RegenHealth = 0`), and stands and fights at the lane's end. The engine's own
  leash and home position are both unusable here — the corridor constant and
  `ResumeLaneFromHere` in `npc_moba_creep.cpp` explain why. Mid-route resume uses
  a fork-added `MotionMaster::MoveWaypoint(WaypointPath&, bool)` overload.
  - **Assist rules**: players can heal/HoT/shield/cleanse their own minions but not
  buff them, and minion buffs survive evade. Three mechanisms, each with its own
  why-comment: `UNIT_FLAG_PLAYER_CONTROLLED` on the template (client-side
  helpful-target gate, `gen_creep_roster.py`), the `moba_creep_spell_gate`
  allow-list, and the inlined evade that skips `RemoveEvadeAuras`
  (`npc_moba_creep.cpp`).
- **End of match**: `FreezeAllCreeps()` stops the living ones; the AI separately
  suppresses `Reset()`/evade afterward via `MatchEnded()`, or an evade would
  re-arm the lane path.
- **Creature stats** are full copies of a real source creature with a small
  override list, enforced by `gen_creep_roster.py` — see `apps/moba/README.md`.

### Respawn

LoL-style individual respawn, replacing the stock shared-pulse graveyard
resurrection. No core engine edits.

- Timer starts on **Release Spirit**, not death: `moba_respawn.cpp`
  (`OnPlayerReleasedGhost`) → `StartRespawnTimer`. Never clicking Release just
  leaves you dead — WoW behavior, intended.
- Wait = `min(RespawnCapMs, RespawnBaseMs + RespawnPerMinMs × match-minutes)`,
  measured from doors-open (`_matchElapsedMs` only accrues while
  `STATUS_IN_PROGRESS`, so prep is excluded). Countdown and revive run in
  `PostUpdateImpl`; revive teleports to `GetTeamStartPosition`, so a ghost that
  wandered still lands at base.
- **No spirit healers**: the spirit guides were removed so the stock revive queue
  never populates and `_ProcessResurrect` can't race our timer. `game_graveyard`
  1103/1104 are kept — they're the start-loc that spawn-in, release-repop
  (`GetClosestGraveyard`), and respawn all share.

### Recall and fountain

Both are anchored to the team's base and configured from `base_config.json`.

- **Recall hijacks Hearthstone** (item 6948 / spell 8690). `moba_recall.cpp`
  prevents the home-bind teleport, sends the player to `GetTeamStartPosition`,
  and clears the cooldown so it repeats; outside the BG it's an ordinary
  Hearthstone. Being a real cast, movement and damage interrupt come free from
  the spell engine. Cast time is overridden per-map by a small `Spell::prepare`
  hook calling `GetRecallCastTimeMs`, and the client cast bar follows via
  `SMSG_SPELL_START`. The empowered tier is gated on placeholder aura 1243
  pending a real mechanic. `AddPlayer` grants a Hearthstone and clears its
  cooldown on entry.
- **Fountain**: `UpdateFountainHealing` (from `PostUpdateImpl`) restores a
  percentage of max health and mana per tick to players inside their own base
  bubble, in or out of combat; enemies get nothing. The bubble is
  `battleground_template.StartMaxDist`, shared with the core's prep-phase leash
  so the two can't drift — see the comment on `UpdateFountainHealing` for the
  squared-getter and 2D-vs-3D traps.
- **Cosmetic limit**: the Hearthstone's on-use tooltip still reads "Returns you
  to \<bind\>" — client-rendered from the spell's bind, not changeable
  server-side. Resolves when recall becomes its own spell in the client-patch
  phase.

### HUD bar

An on-screen bar drawn by the client addon `client/addons/MobaHUD` (`.toc` +
`.lua`) — no client patch, no DBC/MPQ. The server feeds it `LANG_ADDON` chat
messages (prefix `MobaHUD`, packet built like `ArenaSpectator::CreatePacket`);
the 3.3.5a client splits the message on a TAB into `(prefix, payload)`.

- **Payloads**: `T:<seconds>` starts/syncs the clock (the addon counts up locally
  between messages), `S:<ally>,<enemy>,<k>,<d>,<a>,<cs>` updates the scoreboard,
  `E` hides the bar. `S:` is built **per recipient** (`BuildScoreboardBody`) so
  ally/enemy are team-relative and the addon stays dumb.
- **Numbers**: team kills from `_teamPlayerKills` (`HandleKillPlayer`); K/D from
  the stock `SCORE_KILLING_BLOWS`/`SCORE_DEATHS` fields; A derived free as
  `HonorableKills − KillingBlows`, since WoW already credits an honorable kill to
  every teammate near the victim; CS from `BattlegroundMOBAScore::CreepKills`
  (`HandleKillUnit`, lane creeps only — towers aren't in the creep store, so they
  naturally don't count).
- **When it sends**: doors-open, on every player kill, to the killer on a creep
  last-hit, every 10s (`MOBA_HUD_RESYNC_MS`) as a resync, and `E` on match end
  and early leave.
- **The ready ping**: pushing state from `AddPlayer` does **not** work — the
  packet leaves while the client is still loading and is lost. The addon pings
  once on `PLAYER_ENTERING_WORLD` and `moba_hud.cpp` answers with
  `SendHudStateTo`, covering prep-join, mid-match join, and `/reload` with no
  polling. Inbound addon messages have no dedicated script hook, so it rides
  `PlayerScript::OnPlayerCanUseChat(..., Group*)` (battleground chat routes
  through it) and returns `false` to consume the ping. Keep the "should the HUD
  show?" decision on the **server** — it knows it's a `BattlegroundMOBA` on any
  map; the addon would have to hard-code zone names.

---

## Recipes: towers

**Move a tower** — `.gps` at the new spot; edit `x`/`y`/`z`/`o` for that tower in
`maps/<mode>/tower_config.json`; `python3 apps/moba/gen_tower_data.py`; deploy.

**Add a tower** — if it needs a new creature (different model/faction), add
`creature_template` + `creature_template_model` to
`data/sql/custom/db_world/mod_moba_tower_defs.sql`, copying an existing tower's block with
`ScriptName = 'npc_moba_tower'`. Then add a block to the map's
`tower_config.json` `towers` list: `entry`, `team`, `tier`, `guarded_by_entry`,
`.gps` coords, `attack_range`/`attack_interval_ms`/`attack_spell_id`. Run
`gen_tower_data.py`; deploy. No C++ changes — the registry and slot count are
data-driven.

**Add a tier/guard dependency** — set `guarded_by_entry` to the entry that must
die first. The guarded tower spawns inert and `OnTowerDestroyed` unlocks it
automatically. Verify it can't be targeted initially, then becomes attackable and
fires once its guard dies.

**Change attack range or tick rate** — `attack_range` / `attack_interval_ms` in
`tower_config.json`; `gen_tower_data.py`; deploy.

**Change the projectile/spell** — `attack_spell_id` in `tower_config.json`.
Tower damage isn't an independent stat; it's entirely whatever the spell deals.
To retune damage without changing the look, use a different rank of the same
spell family, or change `attack_interval_ms`. The current default (9053, a
"Shoot" clone) deals real damage with no weapon dead zone, but its missile
doesn't render on these prop-style display models (likely no bone attachment
point). Towers cast `triggered = true` — deliberately instant and free, matching
a turret.

**Change tower health** — `HealthModifier` for that entry in
`mod_moba_tower_defs.sql`. It's a multiplier on level-based base health, not an
absolute value, and is map-agnostic.

**Change what counts as hard CC for aggro override** — edit
`MOBA_HARD_CC_MECHANIC_MASK` at the top of `moba_tower_aggro.cpp`. Currently the
engine's `IMMUNE_TO_MOVEMENT_IMPAIRMENT_AND_LOSS_CONTROL_MASK` minus
`MECHANIC_SNARE`/`MECHANIC_DAZE` (slows aren't hard CC), plus `MECHANIC_SILENCE`
(not in the base mask, but loss of ability to act is the spirit of the rule).
A judgment call — revisit if the trigger feels loose or tight in play.

## Recipes: creeps

**Move a lane path** — walk the lane running `.gps` every ~15–20 yd (more through
curves and over bumps; node Z is interpolated linearly). Save the console
scrollback, then `python3 apps/moba/gen_creep_paths.py --extract scrollback.txt`
prints the points array. Paste it into that lane's `points` in
`lmaps/<mode>/lane_config.json`. **Order matters** — the team on the `forward` path IDs
(currently Alliance) spawns at the FIRST point; reverse the array if you walked
the other way. Run `gen_creep_paths.py`; deploy. Path IDs come from the lockfile,
so re-walking an existing lane needs no `mod_moba_creep_data` changes. Full field
and lockfile reference: `apps/moba/README.md`.

**Add a creep type** — dump the source creature to `apps/moba/sources/`, add a
block to `creep_config.json` (copy a similar role's, including equipment item IDs
from its `creature_equip_template` row — weapons aren't in `creature_template`
and melee swing unarmed without them). A new formation slot must be added to
`lmaps/<mode>/lane_config.json` and `gen_creep_paths.py` run first. Then
`gen_creep_roster.py`; deploy. Full field reference: `apps/moba/README.md`.
`BattlegroundMOBA` expects exactly 2 melee + 1 caster per team (siege optional);
the generator warns otherwise.

**Change a creep's attack range/interval/spell** — `attack_range` /
`attack_interval_ms` / `attack_spell_id` in `creep_config.json` (casters only;
melee and siege attack speed comes from the source creature's `BaseAttackTime`).
Run `gen_creep_roster.py`; deploy.

**Change wave cadence or composition** — all C++ in `BattlegroundMOBA.cpp`.
Spawn interval: the `Milliseconds(30000)` in `_bgEvents.ScheduleEvent(EVENT_MOBA_SPAWN_WAVE, …)`
— **two call sites** (`StartingEventOpenDoors` for the first wave, and the
reschedule inside `PostUpdateImpl`'s event case), both must change together.
Siege cadence: `_waveCount % 3 == 0` in `PostUpdateImpl`. Unit counts: the calls
in `SpawnWave()` — not data-driven currently.

**Change the lane-corridor width** — `MOBA_CREEP_LANE_CORRIDOR` at the top of
`npc_moba_creep.cpp` (yards from the lane; the self-evade check adds +15
headroom, and the rule gates player targets only). C++ change.

**Make a creep's attack instant/free vs. a real cast** — the `triggered` argument
of `DoCastVictim(_cfg->spellId, triggered)` in `npc_moba_creep.cpp`'s
`CastAtVictim`. `true` bypasses cast time, mana, and GCD *regardless of the
spell's own data* — right for towers, wrong for casters (we shipped that bug:
Fireball looked instant and free until it was changed to `false`). Applies to all
casters; not per-entry. C++ change.

**Change which spells players can cast on allied minions** — the allow-list
`switch` in `moba_creep_spell_gate::OnSpellCheckCast` (`npc_moba_creep.cpp`);
positive spells matching no allowed effect are rejected before mana/GCD are
spent. C++ change.

## Recipes: base (spawn, respawn, recall, fountain)

All four live in one per-map bundle: `maps/<mode>/base_config.json` →
`gen_base.py` → `mod_moba_base.sql`.

**Move the spawn / respawn / graveyard point** — `game_graveyard` 1103/1104 drive
three things at once: initial teleport-in (via `battleground_template` start-loc),
release-repop, and respawn. `.gps` at the new ground-level spot, edit that team's
`x`/`y`/`z`/`o` in the `spawn` block, run `gen_base.py`, deploy. The generator
writes both the graveyard coords and the template's `StartLoc`/`StartO` — no hand
`UPDATE`s. (The `WorldSafeLocs.dbc` gotcha below still applies.) Confirm both
teleport-in and a post-death respawn.

**Change respawn timings** — `respawn` block (`base_ms` / `per_min_ms` /
`cap_ms`). Test an early death against one a few minutes in to see the scaling.

**Change the recall cast time** — `recall` block (`cast_time_ms` normal,
`empowered_cast_time_ms` reduced tier; `0` = fall back to the spell default / to
normal). Test: cast Hearthstone in-BG for the normal bar, `.aura 1243` then cast
for empowered, `.unaura 1243` to revert. To replace the placeholder empower
trigger with a real mechanic, change the `HasAura` check in
`GetRecallCastTimeMs` and `BG_MOBA_RECALL_EMPOWER_AURA` in `BattlegroundMOBA.h`.
Don't use a haste buff as a placeholder — haste changes cast time itself and
would confound the test.

**Change fountain healing** — `fountain` block (`tick_ms` cadence, `0` disables;
`hp_pct` / `mana_pct` percent of max per tick; mana users only). Test with
`.damage 5000` in base, then walk out of the bubble and confirm it stops.

**Change the base bubble radius** — `spawn.radius`, which the generator writes to
`battleground_template.StartMaxDist`. It is **one number for two things**: the
fountain heal zone and the core's prep-phase leash ("can't leave before doors
open"). Changing it moves both — check both after.

## Recipes: HUD and map

**Retune or extend the HUD** — resync cadence is `MOBA_HUD_RESYNC_MS` (anonymous
namespace in `BattlegroundMOBA.cpp`, default 10000); it's only a safety net now
that the ping handles joins, so lower it only if you see drift. New payloads: add
a sender beside `SendHudMessage`, extend `BuildScoreboardBody`, handle it in the
addon's `HandlePayload`. Icons and layout are constants at the top of
`MobaHUD.lua` plus `Render()` — pure client, `/reload` only.

**Move the HUD on screen** — `/mobahud unlock`, drag, `/mobahud lock`. Position
and lock persist per character (`MobaHUDDB`); `/mobahud reset` recenters.

**Move the starting-area door (the visual dome)** — the door gameobject *is* the
dome. `.gps` the new spot, then compute a yaw-only quaternion (`.gps` won't give
you one): `rotation0 = 0`, `rotation1 = 0`, `rotation2 = sin(o/2)`,
`rotation3 = cos(o/2)`. Update the `AddObject(BG_MOBA_OBJECT_DOOR_A/H, …)` call
in `SetupBattleground()`. C++ change. If it looks tilted, that's the limit of the
flat-yaw approximation — the original EotS doors baked a tilted quaternion to
match sloped terrain; nudge by trial and error if it matters.

---

## Gotcha index

Traps whose full explanation lives in code — read the named comment before
touching that area:

- **Don't anchor logic to a waypoint-walker's home position** — home is stamped
  to the creature's current position every moving tick. → `npc_moba_creep.cpp`,
  `CanAIAttack` and `ResumeLaneFromHere`.
- **The engine's 30 yd leash is skipped while combat stays "fresh"** — which is
  why creeps enforce their own corridor. → `MOBA_CREEP_LANE_CORRIDOR` comment.
- **Evade undoes "stop this creature" logic** — `EnterEvadeMode` synchronously
  calls `MoveTargetedHome()` then `Reset()`, so a freeze doesn't survive a later
  evade unless the AI knows to stay down. Reuse the `MatchEnded()` pattern for any
  future "stop everything". → `npc_moba_creep.cpp`, `Reset` / `EnterEvadeMode`.
- **`TempSummon` spawning has two traps** (the missing `TempSummonType` param and
  the wrong despawn type). → `SpawnCreep` in `BattlegroundMOBA.cpp`.
- **`BgCreatures.resize()` must precede the first `AddCreature`** — it asserts the
  slot exists. → `SetupBattleground()`.
- **`GetStartMaxDist()` returns a *squared* distance.** → `UpdateFountainHealing`.
- **`DoCastVictim(id, true)` bypasses cast time, mana, and GCD** regardless of the
  spell's data. → "Make a creep's attack instant/free" above.
- **Helpful spells aimed at a plain friendly NPC never reach the server** — the
  client silently self-casts instead; `UNIT_FLAG_PLAYER_CONTROLLED` is what marks
  a unit as a valid helpful-spell target. → `gen_creep_roster.py`,
  `CREEP_UNIT_FLAG_PLAYER_CONTROLLED` comment.
- **Stat buffs do nothing on creatures** (`Creature::UpdateStats` is a no-op).
  → `npc_moba_creep.cpp`, `moba_creep_spell_gate` comment.
- **Mechanical-type creatures are hard-immune to direct heals.**
  → `gen_creep_roster.py`, `"type"` override comment.
- **An inlined evade must end with `EngagementOver()`** — omit it and the
  creature stays "engaged" forever and ignores every later enemy.
  → `npc_moba_creep.cpp`, `EnterEvadeMode`.

Traps with no single code home:

- **A chase does not end just because the target became invalid.**
  `Creature::SelectVictim` can return null *without* evading or stopping the
  attack (e.g. while anything still holds the creature on its threat list),
  leaving a chase running on a stale victim. Never assume "target unattackable ⇒
  evade fires" — this shaped the tower's lock-until-invalid design.
- **`LOG_INFO` in a custom log category is silently dropped.** `Logger.root` in
  worldserver.conf is ERROR-level; only categories with an explicit
  `Logger.<name>` line pass INFO. Add e.g.
  `Logger.bg.battleground=4,Console Server` when adding debug logging — nothing
  appears otherwise. Cost a build cycle to discover.
- **`OnTowerDestroyed` only fires from its two source hooks** — `HandleKillUnit`
  needs a player-attributed killer, `JustDied` needs a registered creep killer. A
  tower killed by anything else (environmental damage, a future non-creep source)
  wouldn't trigger the win condition. Not a real scenario today; flagged if damage
  sources expand.
- **`WorldSafeLocs.dbc`** still backs `AllianceStartLoc`/`HordeStartLoc` — the
  generator writes the `game_graveyard` row, but the DBC id must exist.

## Reference: values that live in code

Positions, timings, ranges, and spells are all in `apps/moba/maps/<mode>/*.json`
and the SQL it generates — **read those, not a table here**. Only these live in
C++ or are allocation policy:

| What | Value |
|---|---|
| BG map id (all content rows are tagged with it) | 566 (hijacked EotS) |
| Custom DB entry range | 900000+ — towers 900000–900001, creeps 900010–900017 |
| Custom waypoint path ID range | 900100–900122 (base lanes + per-formation-slot paths) |
| Graveyard DB IDs | 1103 (Alliance), 1104 (Horde) — reused vanilla EotS rows |
| Wave cadence | every 30s; every 3rd wave adds siege (`BattlegroundMOBA.cpp`) |
| Creep lane corridor | 40 yd, players only; +15 yd self-evade headroom (`npc_moba_creep.cpp`) |
| HUD resync cadence | 10s (`MOBA_HUD_RESYNC_MS`) |
| Recall trigger / empower placeholder | Hearthstone item 6948 / spell 8690; aura 1243 |
