# CLAUDE.md

> **This is a fork.** Sections 1 (AzerothCore general) is inherited from upstream. Section 2 (MOBA Project) is fork-specific. **Where the two conflict, Section 2 wins** — conflicts are annotated inline with `[FORK]` notes.

---

# Section 1 — AzerothCore (upstream guidance)

AzerothCore is a C++ MMORPG server emulator for World of Warcraft 3.3.5a (WotLK), built with CMake, backed by MySQL.

## Agent rules

- **Do not configure or build unless explicitly asked.** Builds are slow (CMake + compile of a large C++ codebase) and rarely needed to make code changes.
- **Never edit SQL files outside `data/sql/updates/pending_db_*/`.** `data/sql/base/`, `data/sql/archive/`, and `data/sql/updates/db_*/` are immutable (do not modify).
  - `[FORK]` Exception: this fork keeps its SQL in `data/sql/custom/` (see Section 2). The `pending_db_*` workflow is for upstream contributions, which this fork does not make.
- **Do not run git commands that modify repo state** (commit, branch, merge, rebase, reset, push, …) unless explicitly requested, and do not include them in plans. Read-only git (status, diff, log) is fine.
  - `[FORK]` Stricter here: state-changing git is never run by Claude, even if requested — see Section 2.

## Build

- `[FORK]` The out-of-source `build/` workflow below is upstream's. **This fork builds in `var/build/obj` with machine-specific flags — use the Build section in Section 2 instead.**

Out-of-source build is required (in-source is blocked by CMake).

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$HOME/azeroth-server -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DSCRIPTS=static -DMODULES=static
make -j$(nproc) && make install
```

Compiler: **C++20** required (`CMAKE_CXX_STANDARD 20`). Useful CMake flags: `BUILD_TESTING=ON` (Google Test), `NOPCH=1` (disable precompiled headers). Full flag set in `conf/dist/config.cmake`. `compile_commands.json` is exported automatically.

Tests (Google Test, in `src/test/`): configure with `-DBUILD_TESTING=ON`, then `ctest` or `./src/test/unit_tests` from the build dir.

## Repository layout

- `src/common/` — networking (Asio), crypto, config, logging, shared utilities.
- `src/server/game/` — core gameplay; compiled into worldserver.
- `src/server/scripts/` — content scripts grouped by region (`EasternKingdoms/`, `Northrend/`, …), class (`Spells/spell_mage.cpp`, …), and domain (`Commands/`, `Pet/`, `OutdoorPvP/`, `World/`).
- `src/server/database/` — DB abstraction and schema updater.
- `src/server/shared/` — code shared by auth and world servers.
- `src/server/apps/{authserver,worldserver}/` — entry points (ports 3724 and 8085).
- `src/test/` — Google Test unit tests + mocks.
- `data/sql/` — `base/` (historical schema), `updates/db_*/` (merged), `updates/pending_db_*/` (in-flight), `custom/` (gitignored upstream — `[FORK]` un-ignored in this fork, see Section 2).
- `modules/` — external modules (each a subdir with its own `CMakeLists.txt`). Disable with `-DDISABLED_AC_MODULES="mod1;mod2"`. See `modules/how_to_make_a_module.md`.
- `apps/` — helper scripts; `apps/codestyle/` holds the lint scripts (see below).
- `conf/dist/` — distributed config templates; `conf/*.conf` is gitignored.
- `deps/` — vendored third-party dependencies.

## Code style

Run the linters before claiming a change is done:

```bash
python apps/codestyle/codestyle-cpp.py     # C++
python apps/codestyle/codestyle-sql.py     # SQL (compares to origin/master)
```

Hard rules (also enforced by CI with `-Werror`):

- 4-space indent for C++ (tabs forbidden); 2-space for JSON/YAML/sh/ts/js. UTF-8, LF, max 120 cols, trailing newline.
- Allman braces. No braces around single-line statements. `if (x)` — never `if(x)` or `if ( x )`.
- `auto const&` (not `const auto&`); `Type const*` (not `const Type*`).
- Use `{}` format specifiers (`fmt`-style), not `%u`/`%s`.
- Use the typed helpers, not raw flag access:
  - `IsPlayer()`, `IsCreature()`, `IsItem()`, … instead of `GetTypeId() == TYPEID_*`.
  - `GetNpcFlags()`, `HasNpcFlag()`, `SetNpcFlag()`, `RemoveNpcFlag()`, `ReplaceAllNpcFlags()` instead of `*Flag(UNIT_NPC_FLAGS, …)`.
  - `IsRefundable()`, `IsBOPTradable()`, `IsWrapped()` instead of `HasFlag(ITEM_FIELD_FLAGS, …)`.
  - `HasFlag(ItemFlag)` / `HasFlag2(ItemFlag2)` / `HasFlagCu(ItemFlagsCustom)` instead of bitwise `Flags & ITEM_FLAG…`.
  - `ObjectGuid::ToString().c_str()` instead of `ObjectGuid::GetCounter()`.

CI also runs `cppcheck`.

## Project conventions

- **Logging**: `LOG_INFO("category.sub", "msg with {}", arg)` (also `LOG_WARN`, `LOG_ERROR`, `LOG_DEBUG`, `LOG_TRACE`). Categories are hierarchical, dot-separated (e.g. `server.loading`, `entities.player`, `sql.dev`). No `printf`-style; no `sLog->`; no `TC_LOG_*`. Macro in `src/common/Logging/Log.h`.
- **Random**: use project helpers from `src/common/Utilities/Random.h` — `urand`, `irand`, `frand`, `rand32`, `rand_chance`, `roll_chance_f`, `roll_chance_i`. Do not use `std::rand` or `<random>` directly.
- **Strings**: `Acore::StringFormat(fmt, args...)` (wraps `fmt::format`, `{}` placeholders) — `src/common/Utilities/StringFormat.h`.
- **Config**: read options with `sConfigMgr->GetOption<T>("Name", default)`.
- **Namespace**: project-wide is `Acore::` (no `Trinity::` remnants — agents porting from upstream forks must rename).
- **Long-lived references**: do not store a raw `Player*` / `Creature*` / `Unit*` past the current call/tick — the object can be removed (logout, despawn, instance unload) and the pointer dangles. Store the `ObjectGuid` and resolve at use time via `ObjectAccessor::FindPlayer(guid)`, `ObjectAccessor::GetCreature(*from, guid)`, `Map::GetCreature(guid)`, etc.
- **DB queries**: use `PreparedStatement` (via `WorldDatabase` / `CharacterDatabase` / `LoginDatabase` and the prepared-statement enums) rather than raw query strings. Reads that don't need to block the world tick go through the async path: `_queryProcessor.AddCallback(db.AsyncQuery(stmt).WithPreparedCallback(...))` (or `WithCallback` for non-prepared). Multi-statement writes wrap in `SQLTransaction` + `Execute` / `AppendPreparedStatement`.
- **Timed actions in AI**: use `EventMap` (event id → delay; simple) or `TaskScheduler` (lambdas, repeats, cancellation). Both are members of `CreatureAI`; see any boss script under `src/server/scripts/` for examples — don't roll your own tick counters.

## Scripting registration

Scripts inherit from a `ScriptObject` subclass (`SpellScript`, `AuraScript`, `CreatureScript`, `InstanceMapScript`, `GameObjectScript`, `CommandScript`, …). Two registration styles coexist:

- **Spell / aura scripts**: use the `RegisterSpellScript(ClassName)` (or `RegisterSpellAndAuraScriptPair(...)`) macro inside `AddSC_<name>()`.
- **Creature scripts**: prefer `RegisterCreatureAI(ClassName)` for new code; legacy zones still use `new ClassName();`. Match the surrounding pattern.

Then declare and call `AddSC_<name>()` from the regional loader: `Spells/spells_script_loader.cpp`, `EasternKingdoms/eastern_kingdoms_script_loader.cpp`, etc.

**SmartAI** (data-driven creature behaviour) lives in the world DB's `smart_scripts` table — not in C++. Engine: `src/server/game/AI/SmartScripts/`. For new creature behaviour prefer SmartAI (added via the SQL update workflow); reach for `CreatureScript` only when SmartAI's event/action vocabulary isn't enough.

- `[FORK]` For the tower AI specifically, `CreatureScript`/`CreatureAI` in C++ is the chosen approach (not SmartAI) — the turret behavior and its `JustDied()` → battleground win-condition hook need C++ integration with `BattlegroundMOBA`.

**Module hooks** (e.g. `OnPlayerLogin`, `OnWorldUpdate`, `OnSpellCast`) are declared in `src/server/game/Scripting/ScriptDefines/*.h`. Implement by inheriting the matching base (`PlayerScript`, `WorldScript`, …) and registering with `new MyClass();` (or its `RegisterXxxScript` macro where one exists) inside `AddSC_<name>()`. Full hook list: https://www.azerothcore.org/wiki/hooks-script.

Custom (non-upstream) scripts go in `src/server/scripts/Custom/` (gitignored upstream — `[FORK]` un-ignored in this fork, see Section 2).

---

# Section 2 — MOBA Battleground Project (fork-specific; wins on conflict)

## Working style — READ FIRST

The developer (Jacob) makes all content and technical decisions. Claude's role is to edit code, diagnose, and advise — not to decide, and not to operate the environment.

### Scope: Claude edits code. Everything else is Jacob's.

Claude MAY:
- Read any file in the repo
- Edit/create source files (C++, headers, SQL files in `data/sql/custom/`, docs) — with approval per the rules below
- Run read-only inspection commands (`grep`, `ls`, `cat`, `git status`, `git diff`, `git log`)
- Run builds (`cmake .`, `make`) when asked, and read build output to fix errors

Claude MUST NOT (these are Jacob's tasks — instead, tell him what to run and wait):
- Use git for anything that changes state: no `add`, `commit`, `push`, `checkout`, `reset`, `stash`, tags, branches — even if requested mid-session; hand the command to Jacob instead
- Start, stop, or restart servers (worldserver, authserver, mysql) or run `acore.sh`
- Execute SQL against the database or open mysql sessions — DB changes are delivered as SQL in a file/snippet for Jacob to apply
- Run `make install` — Jacob installs binaries himself
- Modify anything outside the repo (worldserver.conf in env/dist, system config, Homebrew, etc.)
- Delete files or run any destructive shell command

When a task reaches a step in the MUST NOT list, stop and hand off: state exactly what Jacob should run and what output/observation to bring back.

### Decision rules

- **Propose before acting.** For anything beyond trivial single-line fixes, explain what you intend to change and why, and wait for approval.
- **One concern at a time.** Don't bundle unrelated refactors, cleanups, or "improvements" into a requested change. Suggest them separately instead.
- **Design questions go to Jacob.** Gameplay values (tower HP, damage, ranges, timings), placement, naming, and scope decisions are his. Present options with tradeoffs; let him choose.
- **When something is uncertain** (API differences between AzerothCore revisions, untested assumptions), say so explicitly rather than presenting a guess as fact.

### Project mode & teaching goals

- **This is a prototype.** Favor the fastest path to something playable and
  testable over polish or upstream-quality engineering; placeholder data and
  deferred cleanup are normal (tracked in "Deferred / known-untidy").
  Production concerns (performance at scale, upstreamability) are out of
  scope until the gameplay proves out.
- **Teaching is part of the job.** Jacob is using this project to learn
  AzerothCore, C++, SQL, and the surrounding tooling. Explain the "why"
  behind designs and engine mechanics, not just the "what"; when a bug hunt
  uncovers engine internals, spell them out; prefer walking Jacob through
  doing things himself over doing them invisibly.

## What this project is

A fork of AzerothCore (WotLK 3.3.5a server) building a MOBA-style battleground on the Eye of the Storm map. Towers, lanes, destroy-the-base win condition.

**Core approach:** the EotS battleground slot is hijacked. The client queues for EotS normally; the server instantiates a custom `BattlegroundMOBA` class instead of `BattlegroundEY`. No client patch needed. A standalone battleground ID (via `BattlemasterList.dbc` client patch) is planned only after gameplay stabilizes.

- GitHub: `1anderson2jacob/azerothcore-wotlk-moba`, branch `moba-battleground` (upstream remote = official azerothcore repo)
- Machine: MacBook Air M3, Apple Silicon. Client runs in a Parallels Windows VM (ChromieCraft 3.3.5a client), connecting to `10.211.55.2`.

## Gitignore overrides (fork)

Upstream gitignores `src/server/scripts/Custom/` (except the loader) and `data/sql/custom/` — on this fork those directories contain **project source that must be tracked**. The fork's `.gitignore` carries un-ignore exceptions for them (see below if missing). When adding files there, verify they're actually tracked (`git status` must show them; if not, the exceptions are missing from `.gitignore`):

```gitignore
!src/server/scripts/Custom/*
!data/sql/custom/*
```

## Current state

Done (see git history + the `.github/MOBA_*_PLAN.md` docs for detail):
- `BattlegroundMOBA.{h,cpp}` cloned from `BattlegroundEY`, wired into `BattlegroundMgr.cpp` (the `BATTLEGROUND_EY` factory constructs `BattlegroundMOBA`). Flag and capture-point systems fully removed.
- **Towers** — data-driven registry (`src/server/game/Battlegrounds/Zones/MobaTowerData.{h,cpp}`); creature rows **900000** (Alliance, displayID 27101 Keep Cannon) / **900001** (Horde, displayID 18505 Fel Cannon), faction 84/83, no regen. AI in `src/server/scripts/Custom/npc_moba_tower.cpp` (+ `moba_tower_aggro.cpp`): stationary, "lock target until invalid". Win condition fires on both player and minion kills (`HandleKillUnit` + `npc_moba_tower::JustDied`). Positions moved to map center.
- **Lane creeps** — waves walk generated waypoint paths with LoL-style leashing (attack only players within 40yd of the lane; resume from where combat ended; never regress past furthest progress; keep damage between fights; stop at lane end). `npc_moba_creep.cpp` + `MobaCreepData.{h,cpp}`; `data/sql/custom/mod_moba_creeps.sql` is **generated** by `apps/moba/gen_creep_roster.py`, not hand-edited.
- **Respawn** — LoL-style: per-player death timer that starts on Release Spirit (not on death), fixed respawn at the team's start position, no shared-pulse graveyard res. `src/server/scripts/Custom/moba_respawn.cpp` (`OnPlayerReleasedGhost` hook) → `BattlegroundMOBA::StartRespawnTimer`; countdown + revive in `PostUpdateImpl`. Config is per-map (`apps/moba/maps/<mode>/respawn_config.json` → `gen_respawn.py` → `mod_moba_respawn.sql` → `MobaRespawnData.{h,cpp}`): timing (`BaseMs + PerMinMs × match-minutes`, capped, measured from doors-open) plus the per-team spawn coords, which the generator writes into `game_graveyard`/`battleground_template` (read at runtime via `GetTeamStartPosition`). Spirit-guide NPCs removed so the base revive queue never populates; graveyard rows 1103/1104 kept as the start-loc / spawn bubble.
- **Recall to base** — LoL-style, by hijacking Hearthstone (item 6948 / spell 8690): a `SpellScript` (`src/server/scripts/Custom/moba_recall.cpp`, bound via `data/sql/custom/mod_moba_recall.sql`) redirects the teleport to `GetTeamStartPosition` when the caster is in a `BattlegroundMOBA` and resets the cooldown so it's repeatable; outside the BG it's a normal Hearthstone. The 10s cast means movement/damage interrupt come free from the spell engine. Cast time is per-map and tiered (normal + empowered), stored in the respawn bundle (`recall` section → `mod_moba_respawn.RecallCastMs`/`RecallEmpoweredCastMs`) and applied by a small `Spell::prepare` override (`BattlegroundMOBA::GetRecallCastTimeMs`); the client cast bar follows via `SMSG_SPELL_START`. Empowered is currently gated on a placeholder aura (1243) pending a real mechanic. `AddPlayer` grants a Hearthstone if missing and clears its cooldown on entry.
- **Custom map (Twisted Treeline) blockout** — pixel-traced in Blender to WoW scale (`var/blender/twisted_treeline_blockout.blend`); layout exported to `var/blender/twisted_treeline_layout.json` (structure coords + per-lane waypoints). Still runs on the hijacked EotS map; custom terrain is a later pass.
- **Per-map config layout** — all MOBA content is authored as per-map JSON under `apps/moba/maps/<mode>/` (e.g. `eye_of_the_storm/`, map 566) and globbed by generators into combined, map-keyed SQL: `gen_creep_roster.py` (creeps), `gen_tower_data.py` (tower positions; shared creature defs hand-written in `data/sql/custom/mod_moba_tower_defs.sql`), `gen_respawn.py` (respawn timing + spawn wiring), `gen_creep_paths.py` (lane waypoints, still single). All content tables (`mod_moba_tower_data` / `mod_moba_creep_data` / `mod_moba_respawn`) carry a `Map` column; adding a map/mode = dropping in a `maps/<mode>/` bundle.

Next (in order):

Smaller gameplay items to knock out first (unordered):
- **Start horn on wave spawn** — move the BG start horn / gate sound to fire when the first minion wave spawns, not at doors-open.
- **UI match clock** — show elapsed match time on-screen.
- **Fountain healing** — heal players while they stand in the starting zone.
- **Last-hit kill credit** — award minion/boss kill credit to the team that landed the killing blow, not the team that first aggro'd it.
- **Super minions** — a reinforced minion variant.

Then:
1. **Gold / items** economy.
2. **Custom terrain** — move off EotS onto the Twisted Treeline map via the **WMO route** (fall back to ADT if not good enough). Bundles the standalone BG id + client MPQ patch.

## Build (this fork, this machine — overrides Section 1)

```bash
cd var/build/obj
cmake .        # required whenever files are added/removed (source globbing) — plain make misses new files
make -j$(sysctl -n hw.ncpu)
make install   # Jacob runs this — REQUIRED for binaries to reach env/dist/bin; skipping it = running stale code
```

If configuring from scratch, these flags are mandatory on this machine (Homebrew keg-only libs):
```
-DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@3
-DREADLINE_INCLUDE_DIR=/opt/homebrew/opt/readline/include
-DREADLINE_LIBRARY=/opt/homebrew/opt/readline/lib/libreadline.dylib
```
(Also present in `conf/config.sh` as `CCUSTOMOPTIONS` for the acore.sh dashboard path.)

## Run / test loop

- Servers: `./acore.sh run-worldserver` and `./acore.sh run-authserver` in separate terminals (worldserver console is interactive — GM commands typed directly into it)
- MySQL: user `acore`, password `acore`, DBs `acore_auth` / `acore_characters` / `acore_world`
- `worldserver.conf`: `Battleground.PrepTime = 15` (local-only change, conf is gitignored)
- `battleground_template` (acore_world): EotS row ID 7 has `MinPlayersPerTeam = 1`
- **`.debug bg` must be re-run in-game after every worldserver restart** or the solo queue won't pop (this has bitten us repeatedly)
- Test character: GM level 3, level 80. Useful: `.gps` (coordinates), `.morph <id>` / `.demorph` (model preview), `.damage <n>`, `.character level 80`
- Claude cannot see the game client or worldserver console — Jacob relays in-game observations and console output. Ask for them when needed.

## Conventions & lessons learned

- **Custom DB entries live at 900000+.** Custom SQL is saved to `data/sql/custom/mod_moba_towers.sql` and applied manually (no auto-import wiring yet). Keep the file in sync with any DB changes. (Upstream's `pending_db_*` SQL workflow and its DELETE-before-INSERT convention are still good practice — follow the idempotency rule in custom SQL too.)
- **Enums that size `BgObjects`/`BgCreatures` must stay contiguous** — `SetupBattleground()` validation loops walk 0..MAX and fail the whole BG on any empty slot ("object slot N failed to spawn"). When deleting entries, renumber and update `*_MAX`. This has caused two boot-on-entry bugs already.
- **Both validation loops include the slot index in their LOG_ERROR messages** — keep it that way; it saves real debugging time.
- **The compiler is the refactoring checklist**: header edits first, then let build errors enumerate every `.cpp` reference to clean up. Expect 2–3 iterations on big cuts; that's the workflow, not a failure.
- **Editor gotcha:** at least one bug came from an unsaved VS Code buffer / missed edit — when behavior contradicts the code, verify what's actually on disk (`grep`) before deeper theories.
- **`creature_template` schema on this revision:** no `scale` column (use `creature_template_model.DisplayScale`); immunities via `CreatureImmunitiesId` (currently 0 = none; CC-immunity for towers is a known TODO, possibly via AI-applied aura instead).
- **AddCreature signature:** `(entry, type, x, y, z, o, respawntime = 0, transport = nullptr)` — no TeamId param; faction comes from the template.
- The original `BattlegroundEY.{h,cpp}` still exists untouched — use it as reference for how spawning/worldstates/events worked before the strip-down.
- EotS client UI shows leftover grey point icons — cosmetic, unfixable server-side, resolves when the project moves to its own battleground ID.

## Deferred / known-untidy

- Tower `DisplayScale` 5.0 too large; tune later
- Tower positions temporary (mid-lane placement planned, via `.gps`)
- `BG_MOBA_Score` enum holds only the Flurry achievement ID — decide later whether EotS achievements should fire at all in this mode
- `m_BuffChange = true` left in constructor; buffs were removed — harmless, clean up opportunistically
- README roadmap checkboxes need updating as steps complete
- **Recall tooltip** — in-BG, the Hearthstone item still reads "Returns you to Durotar". That line is the Hearthstone spell's client-rendered, bind-based on-use tooltip (same class as the EotS grey point-icons — unfixable server-side). Resolves when recall becomes its own custom spell in the standalone-BG-id / client-MPQ-patch phase.