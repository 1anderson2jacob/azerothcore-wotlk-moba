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

## Code style

Run the linters before claiming a change is done:

```bash
python apps/codestyle/codestyle-cpp.py     # C++
python apps/codestyle/codestyle-sql.py     # SQL (compares to origin/master)
```

Hard rules (enforced by CI with `-Werror`; CI also runs `cppcheck`):

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
- Write, delete, execute SQL, or run any of the above **through `ctx_execute` / `ctx_execute_file` / `ctx_batch_execute`** — the sandbox executes outside the permission deny-list, which structurally cannot see these tools. Every MUST NOT above holds inside them. They are for read-only analysis only: grep, parse, count, summarize.

### Decision rules

- **Propose before acting.** Beyond trivial one-line fixes, explain the intended change and why, then wait for approval.
- **One concern at a time.** Don't bundle unrelated refactors or cleanups into a requested change — suggest them separately.
- **Design questions go to Jacob.** Gameplay values (HP, damage, ranges, timings), placement, naming, scope. Present options with tradeoffs; let him choose.
- **Flag uncertainty explicitly** (API differences between AzerothCore revisions, untested assumptions) rather than presenting a guess as fact.

### Project mode & teaching goals

- **Past prototype.** The gameplay has proven out, so correctness and polish count. Placeholder data and deferred cleanup are no longer free — they need a reason and an entry in "Deferred / known-untidy".
- **Teaching is part of the job.** Jacob is using this project to learn AzerothCore, C++, SQL, and the surrounding tooling. Explain the "why" behind designs and engine mechanics, not just the "what"; when a bug hunt uncovers engine internals, spell them out; prefer walking Jacob through doing things himself over doing them invisibly.

## What this project is

A MOBA-style battleground on Twisted Treeline, a custom map: towers, lanes, destroy-the-base win condition.

**Core approach:** Twisted Treeline is its own battleground — `BATTLEGROUND_TT` = 12, map 900, with its own `BattlemasterList.dbc` and `PvpDifficulty.dbc` rows in the MPQ client patch. Eye of the Storm is stock and untouched. The client patch is required, not optional: without those two rows the PvP frame either cannot list the battleground or lists it unqueueable.

GitHub: `1anderson2jacob/azerothcore-wotlk-moba`, branch `moba-battleground` (`upstream` = official azerothcore).

- **Roadmap and what's shipped** → `.github/README.md`
- **How to change any of it** → `.github/MOBA_GUIDE.md`

Neither is duplicated here. Read them when the task needs them.

## Where things live

| System | Code | Config → generated SQL |
|---|---|---|
| Battleground class | `Battlegrounds/Zones/BattlegroundMOBA.{h,cpp}` (constructed by `BattlegroundMgr.cpp`'s `BATTLEGROUND_TT` factory) | — |
| Towers / inhibitors / base | `Zones/MobaTowerData.{h,cpp}`, `scripts/Custom/npc_moba_tower.cpp` + `moba_tower_aggro.cpp` | `tower_config.yaml` → `gen_tower_data.py` → `mod_moba_towers.sql` (creature templates + models + per-map placement) |
| Lane creeps | `Zones/MobaCreepData.{h,cpp}`, `scripts/Custom/npc_moba_creep.cpp` | `creep_config.yaml` → `gen_creep_roster.py` → `mod_moba_creeps.sql`; lanes: `lane_config.yaml` → `gen_creep_paths.py` → `mod_moba_creep_paths.sql` |
| Respawn, recall, fountain, spawn dome | `Zones/MobaBaseData.{h,cpp}`, `scripts/Custom/moba_respawn.cpp` + `moba_recall.cpp` | `base_config.yaml` → `gen_base.py` → `mod_moba_base.sql` |
| Neutral camps | `Zones/MobaNeutralData.{h,cpp}`, `scripts/Custom/npc_moba_neutral.cpp` | `neutral_config.yaml` → `gen_neutral_camps.py` → `mod_moba_neutrals.sql` |
| On-death drops | `Zones/MobaDropData.{h,cpp}`, `GrantDeathDrops` in `BattlegroundMOBA.cpp` | `drops` lists in creep/neutral configs → both generators |
| Item shop (server) | `Zones/MobaStoreData.{h,cpp}`, `scripts/Custom/npc_moba_store.cpp`, `RecordGrantedItem` + `RemovePlayer` in `BattlegroundMOBA.cpp` | `store_config.yaml` → `gen_store.py` → `mod_moba_store.sql` (shopkeeper NPCs + spawns + catalog tables) **and** `client/addons/MobaHUD/Catalog.lua` |
| Client addon (HUD bar, feeds, gold floats, shop panel) | `client/addons/MobaHUD/` — `Core.lua` (shared `ns`), `Bar.lua`, `Feed.lua`, `Gold.lua`, `Shop.lua`, `Minimap.lua` (shop button), `MobaHUD.lua` (orchestrator); server side in `scripts/Custom/moba_hud.cpp` | — (copy the whole folder into `Interface/AddOns/`; `Catalog.lua` is generated into it) |
| Surrender vote | `Zones/BattlegroundMOBA.{h,cpp}` (vote state + rules), `scripts/Custom/moba_surrender.cpp` (`.surrender` / `.ff`) | `surrender` block in `base_config.yaml` → `gen_base.py`; command help in hand-written `mod_moba_commands.sql` |

Per-map config bundles live in `apps/moba/maps/<mode>/`; generators in `apps/moba/` (see `apps/moba/README.md`). Adding a map/mode = dropping in a new `maps/<mode>/` bundle — every content table carries a `Map` column. Custom DB IDs are allocated by `apps/moba/id_alloc.py` from blocks declared in `apps/moba/id_blocks.json` (`--audit` to inspect). Custom SQL in data/sql/custom/db_world/ (+db_auth/db_characters) and vendored-module SQL auto-apply on worldserver boot (Updates.AutoSetup).

## Gitignore overrides (fork)

Upstream gitignores `src/server/scripts/Custom/` and `data/sql/custom/`; this fork tracks both via `.gitignore` exceptions.

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

- Unit tests: configure `-DBUILD_TESTING=ON`, then `ctest`
- Disable modules: `-DDISABLED_AC_MODULES="mod1;mod2"`
- Servers: `./acore.sh run-worldserver` / `run-authserver` in separate terminals (the worldserver console takes GM commands directly)
- MySQL: user `acore`, password `acore`, DBs `acore_auth` / `acore_characters` / `acore_world`
- **Config**: committed settings live in tracked `.dist` — `modules/mod-cfbg/conf/CFBG.conf.dist` holds CFBG tuning **and** the `AllowTwoSide.Interaction.Group` core override; machine-local test knobs (`Battleground.PrepTime`, `CFBG.EvenTeams.Enabled`) live in gitignored `apps/moba/local.conf`. `apps/moba/setup.sh` builds the runtime `.conf` from both.
- **`.debug bg` must be re-run in-game after every worldserver restart** or the solo queue won't pop. It also FREEZES the premature-finish countdown — the decrement lives inside the `!isTesting()` announce branch (`Battleground.cpp:466`), so an under-strength test match never auto-ends and only a core kill or a passed surrender vote reaches `EndBattleground`
- **DEFEAT is not observable solo.** `npc_moba_tower::JustDied` hands `OnTowerDestroyed` the KILLER's team, so any core you destroy — your own included — resolves the winner to your team and sends you VICTORY. Testing the defeat path needs a second player on the losing side, or enemy creeps finishing your base unassisted.
- Test character: GM level 3, level 80. Useful: `.gps`, `.morph <id>` / `.demorph`, `.damage <n>`, `.character level 80`
- **Addon Lua**: `luajit -e "assert(loadfile('<f>'))"` (a syntax error and a never-loaded file look identical in-game: silence, then a nil) and `cd client/addons/MobaHUD && luacheck .` — from the repo root it never finds `.luacheckrc` and reports hundreds of phantom warnings (0-warning baseline). Not Homebrew `lua` — it is 5.5 and accepts syntax 3.3.5 rejects.
- **Search with `rg`, never Bash `grep`/`find`** — the `Grep`/`Glob` tools are already ripgrep; `rg` is allowlisted so Bash search stays prompt-free (`rg --files -g '<glob>'` for filename matching, `rg -P` for lookarounds — BSD grep has no `-P`). **Pass `--hidden` when the target may be in `.github/`** — plain `rg` skips hidden dirs, so `MOBA_GUIDE.md` and every `MOBA_*_PLAN.md` are invisible without it. `rg` obeys `.gitignore`, which correctly includes the `Custom/` re-inclusions on `.gitignore:121-122`.
- Claude cannot see the game client or the worldserver console — ask Jacob to relay output and in-game observations

## Conventions & lessons learned

- **Enums that size `BgObjects` must stay contiguous** — `SetupBattleground()`'s validation loops walk 0..MAX and fail the whole BG on any empty slot. When deleting entries, renumber and update `*_MAX`. This has caused two boot-on-entry bugs. Keep the slot index in those `LOG_ERROR` messages; it saves real debugging time.
- **Data stores cache once per worldserver process**, not per battleground — a SQL
  change needs a full restart, not `.debug bg` + requeue. They load lazily from `SetupBattleground()`, which the core calls from
  `Battleground::_ProcessJoin` — on the first BG *tick*, after players have already
  ported in. So `AddPlayer` and anything else running before that first tick must
  call `LoadIfNeeded()` themselves or they read an empty store on the first match of a process, and only that one. Cost a real bug in the gold stipend.
- **A missing column takes the whole battleground down, not just the feature.** A `SELECT` naming a column that does not exist errors rather than returning partial rows, so the store loads *nothing*, `GetConfig` returns nullptr for every map, and `SetupBattleground` fails with "battleground not created!". Symptom and cause look unrelated. Always regenerate the SQL before restarting after a C++ store change — and confirm the running binary matches the DB before trusting either.
- **The compiler is the refactoring checklist**: edit headers first, then let build errors enumerate every `.cpp` to clean up. 2–3 iterations on a big cut is the workflow, not a failure.
- **When behavior contradicts the code, `grep` what's actually on disk** before
  deeper theories — an unsaved editor buffer caused one bug, and a skipped
  `make install` sent us hunting a phantom item-tracking bug for two rounds.
  Confirm the running binary is current before believing a symptom.
- `creature_template` on this revision has no `scale` column (use `creature_template_model.DisplayScale`); immunities via `CreatureImmunitiesId`.
- `AddCreature(entry, type, x, y, z, o, respawntime = 0, transport = nullptr)` — no TeamId param; faction comes from the template. **That `respawntime = 0` does not mean "never respawn"**: the setter runs only when the argument is non-zero, so the default leaves `Creature`'s own `m_respawnDelay(300)` + `m_corpseDelay(60)` in place and the creature quietly returns ~6 minutes after dying. Structures pass `DAY` to suppress it. One that came back this way was still flagged `destroyed` in `_towers` — alive and attackable, but inert to every code path that mattered, which is why it went unnoticed for so long.
- **"Who destroyed it" and "whose enemy benefits" are different questions.** `npc_moba_tower::JustDied` hands `OnTowerDestroyed` the *killer's* team, not the owner's enemy. The two agree in a real push and diverge the moment an own-team unit lands the blow — so any flag set on destruction and cleared later (super minions) must derive **both** ends from the structure's owner, or it leaks for the rest of the match.
- Fork-authored files carry **no GPL header** — the root `LICENSE` covers them. Upstream files we modify keep theirs; add one back only when upstreaming.
- The original `BattlegroundEY.{h,cpp}` is untouched and live again on slot 7 — also the reference for how spawning/worldstates worked before the strip-down.

## Documentation standards

**One home per fact.** Before writing a doc line, check it isn't already one of these:

| Where | Holds | Never holds |
|---|---|---|
| Code comment | An engine behaviour or constraint visible at that line — a quirk, a trap that cost a real bug | Who calls this; what the other side of a protocol does; a rule enforced in another file |
| `MOBA_GUIDE.md` architecture | Facts that span two or more files — wiring, deliberate contrasts between subsystems, invariants no single file can state | Anything true of one file: payload layouts, field lists, which function does what |
| `MOBA_GUIDE.md` recipe | The steps to change one thing | Why the engine works that way |
| `MOBA_GUIDE.md` gotcha index | One line naming the trap + where the full explanation lives | The full explanation |
| `CLAUDE.md` | Rules, environment, and a map of where things live | Feature explanations; the roadmap |
| `apps/moba/README.md` | The generator pipeline: what reads what, ID allocation, lockfile rules, policies spanning configs | Per-field semantics — those live in each config's own YAML header |
| `.github/README.md` | The roadmap; the public-facing overview | Internal recipes |
| `.github/MOBA_*_PLAN.md` | Work not yet built; the mid-feature handoff | Anything shipped |

- **Every feature starts with a plan file.** Before writing code, create `.github/MOBA_<FEATURE>_PLAN.md` holding the goal, decisions already made (so they are not relitigated), what is built, what is left, and any hard-won facts discovered along the way. Keep it current as work proceeds — it is the handoff if a session ends mid-feature. **Delete it when the feature lands**; it is never committed.
- Never explain something in two places. Link instead.
- **The comment test: delete it — does something silently break?** If not, it goes. Not "is it true", not "is it useful" — a true, useful comment that guards nothing is precisely the kind that goes stale and then misleads. Four things always fail it: justifying a choice against a rejected alternative (the commit message holds that), naming a function's callers (the compiler holds that), restating what another file does (link instead), and a framing sentence in front of the fact.
- Prefer a trailing annotation on the declaration over a paragraph above it — it dies with the thing it describes, so it cannot outlive it.
- Apply the test on a **reread**, not while writing. At write time every explanation feels load-bearing; that is how the rule this one replaced got ignored for a whole feature set. Re-read a change's comments alongside running the linters, before calling it done.
- Prefer deleting a stale line over updating it.
- CLAUDE.md is loaded into **every** session; everything else is opt-in. Lines added here are paid for forever.

## Deferred / known-untidy

- Tower `DisplayScale` 5.0 too large; tower positions temporary (mid-lane placement planned, via `.gps`)
- **Client-patch bundle** — the DBC/MPQ pipeline is live now (`apps/moba/wmo/dbc_tool.py` plus step 7 of `apps/moba/wmo/README.md`), so these are unbuilt rather than blocked: the recall tooltip still reads "Returns you to \<bind\>"; recall and fountain have no custom spell visuals; custom battle sounds (a doors-open cue and a first-wave-only cue — two `PlaySoundToAll` calls, ~10 min once `SoundEntries.dbc` rows exist); Twisted Treeline music; the item shop's `custom_items` flag (`Item.dbc` rows for the `+900000` copies) **and the same treatment for drop items, which retires the match-granted ledger entirely** — correct tooltip sell prices come free with that (3.3.5 reads them from the server's item query, and `gen_store.py` already stamps each copy's `SellPrice`), and cannot be right before it, since until then the value is per-grant rather than per-entry.
- Shop bag-space check counts empty slots only (`GetFreeInventorySpace`), so buying a stack of consumables with a full bag is refused even when a partial stack could absorb them
- **The match-granted ledger drifts** — `_grantedCounts` grows on grant and shrinks only on sell or exit, so consuming or destroying a granted item leaves the claim behind, and it is then spent on the player's own copies of the same entry. Reproduced both on the sell path and the exit sweep. Deferred on purpose: an in-server ratchet was designed and rejected in favour of cloning drop items with the client patch, which deletes the ledger instead of patching it. Repro and rejected approaches in `.github/MOBA_SHOP_SELL_PLAN.md`.
- Equipped items cannot be sold — the Lua→engine slot mapping cannot name EQUIPMENT_SLOT_*` by construction. Unequip first.
- **Formation speed compensation lapses for one leg after a mid-leg slow** — the aura makes `Unit::UpdateSpeed` rebuild a clean rate, dropping the corner multiplier until the next node re-applies it (~0.6s). Accepted deliberately: the slow itself lands instantly and at full strength, and only the compensation pauses. Closing it would need a hook inside `UpdateSpeed`, i.e. a core edit.
