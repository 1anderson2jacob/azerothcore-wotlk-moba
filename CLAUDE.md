# CLAUDE.md

> **This is a fork.** Section 1 is upstream AzerothCore guidance. Section 2 is
> fork-specific and **wins on any conflict** — where Section 2 covers a topic
> (build, SQL, git, docs), Section 1 does not apply and is not repeated here.

---

# Section 1 — AzerothCore (upstream guidance)

AzerothCore is a C++ MMORPG server emulator for World of Warcraft 3.3.5a (WotLK), built with CMake, backed by MySQL. C++20 required.

## Agent rules

- **Do not configure or build unless explicitly asked.** Builds are slow (CMake + compile of a large C++ codebase) and rarely needed to make code changes.
- Build, SQL, git, and documentation workflows: see Section 2 — it replaces upstream's entirely.

## Repository layout

- `src/common/` — networking (Asio), crypto, config, logging, shared utilities.
- `src/server/game/` — core gameplay; compiled into worldserver.
- `src/server/scripts/` — content scripts grouped by region (`EasternKingdoms/`, `Northrend/`, …), class (`Spells/spell_mage.cpp`, …), and domain (`Commands/`, `Pet/`, `OutdoorPvP/`, `World/`).
- `src/server/database/` — DB abstraction and schema updater.
- `src/server/shared/` — code shared by auth and world servers.
- `src/server/apps/{authserver,worldserver}/` — entry points (ports 3724 and 8085).
- `src/test/` — Google Test unit tests + mocks (configure `-DBUILD_TESTING=ON`, then `ctest`).
- `data/sql/` — `base/` (historical schema), `updates/db_*/` (merged), `custom/` (**this fork's SQL** — Section 2).
- `modules/` — external modules, each with its own `CMakeLists.txt`. Disable with `-DDISABLED_AC_MODULES="mod1;mod2"`.
- `apps/` — helper scripts; `apps/codestyle/` holds the lint scripts.
- `conf/dist/` — distributed config templates; `conf/*.conf` is gitignored.
- `deps/` — vendored third-party dependencies.

## Code style

Run the linters before claiming a change is done:

```bash
python apps/codestyle/codestyle-cpp.py     # C++
python apps/codestyle/codestyle-sql.py     # SQL (compares to origin/master)
```

Hard rules (enforced by CI with `-Werror`; CI also runs `cppcheck`):

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

## Project conventions

- **Logging**: `LOG_INFO("category.sub", "msg with {}", arg)` (also `LOG_WARN`, `LOG_ERROR`, `LOG_DEBUG`, `LOG_TRACE`). Hierarchical dot-separated categories (`server.loading`, `entities.player`, `sql.dev`). No `printf`-style, no `sLog->`, no `TC_LOG_*`. Macro in `src/common/Logging/Log.h`.
- **Random**: `urand`, `irand`, `frand`, `rand32`, `rand_chance`, `roll_chance_f`, `roll_chance_i` from `src/common/Utilities/Random.h` — not `std::rand` or `<random>`.
- **Strings**: `Acore::StringFormat(fmt, args...)` (`{}` placeholders) — `src/common/Utilities/StringFormat.h`.
- **Config**: `sConfigMgr->GetOption<T>("Name", default)`.
- **Namespace**: `Acore::` (no `Trinity::` remnants — rename when porting from other forks).
- **Long-lived references**: never store a raw `Player*`/`Creature*`/`Unit*` past the current call/tick — the object can be removed and the pointer dangles. Store the `ObjectGuid`, resolve at use time (`ObjectAccessor::FindPlayer`, `Map::GetCreature`, …).
- **DB queries**: `PreparedStatement` over raw query strings. Reads that needn't block the world tick go through `_queryProcessor.AddCallback(db.AsyncQuery(stmt).WithPreparedCallback(...))`. Multi-statement writes wrap in `SQLTransaction`.
- **Timed actions in AI**: `EventMap` (event id → delay) or `TaskScheduler` (lambdas, repeats, cancellation) — both members of `CreatureAI`. Don't roll your own tick counters.

## Scripting registration

Scripts inherit a `ScriptObject` subclass (`SpellScript`, `AuraScript`, `CreatureScript`, `InstanceMapScript`, `GameObjectScript`, `CommandScript`, …):

- **Spell/aura scripts**: `RegisterSpellScript(ClassName)` / `RegisterSpellAndAuraScriptPair(...)` inside `AddSC_<name>()`.
- **Creature scripts**: prefer `RegisterCreatureAI(ClassName)` for new code; legacy zones use `new ClassName();`. Match the surrounding pattern.

Declare and call `AddSC_<name>()` from the regional loader (`Spells/spells_script_loader.cpp`, `EasternKingdoms/eastern_kingdoms_script_loader.cpp`, …).

**SmartAI** (data-driven creature behaviour) lives in the world DB's `smart_scripts` table, not C++. Engine: `src/server/game/AI/SmartScripts/`. Prefer it for new creature behaviour; reach for `CreatureScript` only when its event/action vocabulary isn't enough. `[FORK]` The MOBA tower and creep AIs are deliberately C++ — they need integration with `BattlegroundMOBA`.

**Module hooks** (`OnPlayerLogin`, `OnWorldUpdate`, `OnSpellCast`, …) are declared in `src/server/game/Scripting/ScriptDefines/*.h`. Inherit the matching base (`PlayerScript`, `WorldScript`, …) and register with `new MyClass();` inside `AddSC_<name>()`. Full list: https://www.azerothcore.org/wiki/hooks-script.

---

# Section 2 — MOBA Battleground Project (fork-specific; wins on conflict)

## Working style — READ FIRST

The developer (Jacob) makes all content and technical decisions. Claude's role is to edit code, diagnose, and advise — not to decide, and not to operate the environment.

### Scope: Claude proposes, Jacob applies

Claude MAY:
- Read any file in the repo
- Run read-only inspection commands (`grep`, `ls`, `cat`, `git status`/`diff`/`log`)
- Run builds (`cmake .`, `make`) when asked, and read build output to fix errors

Claude MUST NOT — these are Jacob's; hand him the exact command and wait for what it printed:
- **Write, edit, or delete any file.** Present complete content for Jacob to apply. When a change evolves across turns, restate each affected file or function **in full** — deltas get misapplied.
- Use git for anything that changes state: `add`, `commit`, `push`, `checkout`, `reset`, `stash`, branches, tags — even if asked mid-session
- Start, stop, or restart servers (worldserver, authserver, mysql) or run `acore.sh`
- Execute SQL or open mysql sessions — deliver SQL as a file or snippet
- Run `make install`
- Modify anything outside the repo (worldserver.conf in env/dist, system config, Homebrew)
- Run `apps/moba/setup.sh` (it writes `env/dist` — a Jacob-only op)

### Decision rules

- **Propose before acting.** Beyond trivial one-line fixes, explain the intended change and why, then wait for approval.
- **One concern at a time.** Don't bundle unrelated refactors or cleanups into a requested change — suggest them separately.
- **Design questions go to Jacob.** Gameplay values (HP, damage, ranges, timings), placement, naming, scope. Present options with tradeoffs; let him choose.
- **Flag uncertainty explicitly** (API differences between AzerothCore revisions, untested assumptions) rather than presenting a guess as fact.

### Project mode & teaching goals

- **This is a prototype.** Favor the fastest path to something playable and testable over polish; placeholder data and deferred cleanup are normal. Performance at scale and upstreamability are out of scope until the gameplay proves out.
- **Teaching is part of the job.** Jacob is using this project to learn AzerothCore, C++, SQL, and the surrounding tooling. Explain the "why" behind designs and engine mechanics, not just the "what"; when a bug hunt uncovers engine internals, spell them out; prefer walking Jacob through doing things himself over doing them invisibly.

## What this project is

A MOBA-style battleground on the Eye of the Storm map: towers, lanes, destroy-the-base win condition.

**Core approach:** the EotS battleground slot is hijacked. The client queues for EotS normally; the server instantiates `BattlegroundMOBA` instead of `BattlegroundEY`. No client patch needed. A standalone battleground ID (via a `BattlemasterList.dbc` client patch) comes only after gameplay stabilizes.

GitHub: `1anderson2jacob/azerothcore-wotlk-moba`, branch `moba-battleground` (`upstream` = official azerothcore).

- **Roadmap and what's shipped** → `.github/README.md`
- **How to change any of it** → `.github/MOBA_GUIDE.md`

Neither is duplicated here. Read them when the task needs them.

## Where things live

| System | Code | Config → generated SQL |
|---|---|---|
| Battleground class | `Battlegrounds/Zones/BattlegroundMOBA.{h,cpp}` (constructed by `BattlegroundMgr.cpp`'s `BATTLEGROUND_EY` factory) | — |
| Towers / inhibitors / base | `Zones/MobaTowerData.{h,cpp}`, `scripts/Custom/npc_moba_tower.cpp` + `moba_tower_aggro.cpp` | `tower_config.yaml` → `gen_tower_data.py` → `mod_moba_towers.sql` (creature templates + models + per-map placement) |
| Lane creeps | `Zones/MobaCreepData.{h,cpp}`, `scripts/Custom/npc_moba_creep.cpp` | `creep_config.yaml` → `gen_creep_roster.py` → `mod_moba_creeps.sql`; lanes: `lane_config.yaml` → `gen_creep_paths.py` → `mod_moba_creep_paths.sql` |
| Respawn, recall, fountain | `Zones/MobaBaseData.{h,cpp}`, `scripts/Custom/moba_respawn.cpp` + `moba_recall.cpp` | `base_config.yaml` → `gen_base.py` → `mod_moba_base.sql` |
| HUD bar | `client/addons/MobaHUD/`, `scripts/Custom/moba_hud.cpp` | — (client addon; copy into `Interface/AddOns/`) |
| Neutral camps | `Zones/MobaNeutralData.{h,cpp}`, `scripts/Custom/npc_moba_neutral.cpp` | `neutral_config.yaml` → `gen_neutral_camps.py` → `mod_moba_neutrals.sql` |
| On-death drops | `Zones/MobaDropData.{h,cpp}`, `GrantDeathDrops` in `BattlegroundMOBA.cpp` | `drops` lists in creep/neutral configs → both generators |
| Item shop | `Zones/MobaStoreData.{h,cpp}`, `scripts/Custom/npc_moba_store.cpp`, `RecordGrantedItem` + `RemovePlayer` in `BattlegroundMOBA.cpp` | `store_config.yaml` → `gen_store.py` → `mod_moba_store.sql` (vendor NPCs + spawns + gossip catalog) |


Per-map config bundles live in `apps/moba/maps/<mode>/`; generators in `apps/moba/` (see `apps/moba/README.md`). Adding a map/mode = dropping in a new `maps/<mode>/` bundle — every content table carries a `Map` column. Custom DB entries live at **900000+**. Custom SQL in data/sql/custom/db_world/ (+db_auth/db_characters) and vendored-module SQL auto-apply on worldserver boot (Updates.AutoSetup).

## Gitignore overrides (fork)

Upstream gitignores `src/server/scripts/Custom/` and `data/sql/custom/`; this fork tracks both via `.gitignore` exceptions:

```gitignore
!src/server/scripts/Custom/*
!data/sql/custom/*
```

When adding files there, confirm `git status` actually shows them.

## Build, run, test (overrides Section 1)

```bash
cd var/build/obj
cmake .        # required whenever files are added/removed (source globbing)
make -j$(sysctl -n hw.ncpu)
make install   # Jacob runs this — skipping it means running stale code
```

Configuring from scratch needs these (Homebrew keg-only libs; also in `conf/config.sh` as `CCUSTOMOPTIONS`):

```
-DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@3
-DREADLINE_INCLUDE_DIR=/opt/homebrew/opt/readline/include
-DREADLINE_LIBRARY=/opt/homebrew/opt/readline/lib/libreadline.dylib
```

- Servers: `./acore.sh run-worldserver` / `run-authserver` in separate terminals (the worldserver console takes GM commands directly)
- MySQL: user `acore`, password `acore`, DBs `acore_auth` / `acore_characters` / `acore_world`
- **Config**: committed settings live in tracked `.dist` — `modules/mod-cfbg/conf/CFBG.conf.dist` holds CFBG tuning **and** the `AllowTwoSide.Interaction.Group` core override; machine-local test knobs (`Battleground.PrepTime`, `CFBG.EvenTeams.Enabled`) live in gitignored `apps/moba/local.conf`. `apps/moba/setup.sh` builds the runtime `.conf` from both.
- **`.debug bg` must be re-run in-game after every worldserver restart** or the solo queue won't pop
- Test character: GM level 3, level 80. Useful: `.gps`, `.morph <id>` / `.demorph`, `.damage <n>`, `.character level 80`
- Claude cannot see the game client or the worldserver console — ask Jacob to relay output and in-game observations

## Conventions & lessons learned

- **Enums that size `BgObjects` must stay contiguous** — `SetupBattleground()`'s validation loops walk 0..MAX and fail the whole BG on any empty slot. When deleting entries, renumber and update `*_MAX`. This has caused two boot-on-entry bugs. Keep the slot index in those `LOG_ERROR` messages; it saves real debugging time.
- **Data stores cache once per worldserver process**, not per battleground — a SQL change needs a full restart, not `.debug bg` + requeue.
- **The compiler is the refactoring checklist**: edit headers first, then let build errors enumerate every `.cpp` to clean up. 2–3 iterations on a big cut is the workflow, not a failure.
- **When behavior contradicts the code, `grep` what's actually on disk** before deeper theories — an unsaved editor buffer caused one bug.
- `creature_template` on this revision has no `scale` column (use `creature_template_model.DisplayScale`); immunities via `CreatureImmunitiesId`.
- `AddCreature(entry, type, x, y, z, o, respawntime = 0, transport = nullptr)` — no TeamId param; faction comes from the template.
- The original `BattlegroundEY.{h,cpp}` is untouched — reference for how spawning/worldstates worked before the strip-down.

## Documentation standards

**One home per fact.** Before writing a doc line, check it isn't already one of these:

| Where | Holds | Never holds |
|---|---|---|
| Code comment | A constraint local to that code that the code can't show — engine quirks, why-not-the-obvious-thing, a trap that cost a real bug | Cross-file architecture; workflow steps |
| `MOBA_GUIDE.md` recipe | The steps to change one thing | Why the engine works that way |
| `MOBA_GUIDE.md` gotcha index | One line naming the trap + where the full explanation lives | The full explanation |
| `CLAUDE.md` | Rules, environment, and a map of where things live | Feature explanations; the roadmap |
| `.github/README.md` | The roadmap; the public-facing overview | Internal recipes |
| `.github/MOBA_*_PLAN.md` | Work not yet built; the mid-feature handoff | Anything shipped |

- **Every feature starts with a plan file.** Before writing code, create
  `.github/MOBA_<FEATURE>_PLAN.md` holding the goal, decisions already made (so
  they are not relitigated), what is built, what is left, and any hard-won facts
  discovered along the way. Keep it current as work proceeds — it is the handoff
  if a session ends mid-feature. **Delete it when the feature lands**; it is
  never committed.
- Never explain something in two places. Link instead.
- Comments state constraints, not narration — never "what the next line does", never "why this change is correct".
- Prefer deleting a stale line over updating it.
- CLAUDE.md is loaded into **every** session; everything else is opt-in. Lines added here are paid for forever.

## Deferred / known-untidy

- Tower `DisplayScale` 5.0 too large; tower positions temporary (mid-lane placement planned, via `.gps`)
- **Client-patch bundle** — all blocked on the same MPQ/DBC work, so do them together: the recall tooltip still reads "Returns you to \<bind\>"; recall and fountain have no custom spell visuals; custom battle sounds (a doors-open cue and a first-wave-only cue — two `PlaySoundToAll` calls, ~10 min once `SoundEntries.dbc` rows exist); Twisted Treeline music; the leftover EotS grey point-icons; the item shop's `custom_items` flag (`Item.dbc` rows for the `+900000` copies).
- Shop bag-space check counts empty slots only (`GetFreeInventorySpace`), so buying a stack of consumables with a full bag is refused even when a partial stack could absorb them
