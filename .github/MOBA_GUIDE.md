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

**Ad-hoc `UPDATE`s** are fine for live experimentation, but fold the values back into
`apps/moba/maps/<mode>/*.yaml`. Two different risks, and the second is the nasty one:

- `mod_moba_*` tables are dropped and recreated every boot, so an unrecorded change is
  reverted and you find out quickly.
- Core tables (`battleground_template`, `creature_template`, `game_graveyard`) are only
  ever touched by targeted `UPDATE`s, so an unrecorded change persists on your machine
  forever and diverges silently from the repo. It will never fail for you, and will
  always be wrong on a fresh database. `MinPlayersPerTeam` lived this way for a while.

---

## Architecture

### Towers

`BattlegroundMOBA` holds a `std::vector<MobaTowerState>` registry built in
`SetupBattleground()` from `mod_moba_tower_data`. The slot count is runtime-sized from
the row count, so there is no per-tower enum.

- **Structure kinds** (`Kind` in `mod_moba_tower_data`): `tower` attacks; `inhibitor` and
  `core` are passive. What each does on death is `OnTowerDestroyed`, whose only caller is
  `npc_moba_tower::JustDied` — the engine's own kill hook cannot serve, see the gotcha
  index. Destroying a core ends the battleground; that is the win condition.
- **Guard/tier**: a structure with `GuardedByEntry` spawns unattackable and unselectable
  until its guard dies. It is a single FK, so chains are linear (tower → inhibitor →
  base) with no multi-guard AND-gating.
- **Targeting is deliberately *unlike* creeps**: `REACT_PASSIVE` plus fully manual
  targeting, immune to taunt and kiting. Separately `moba_tower_aggro.cpp` — the only
  global `UnitScript` in this codebase — switches a tower onto an enemy player who
  damages or hard-CCs an ally in range.

### Lane creeps

Waves spawn every 30s from `_bgEvents`, both teams at once, siege on every 3rd. Creeps
are `TempSummon`s tracked in `_spawnedCreeps`, not the persistent `BgCreatures` registry.

- **Targeting is deliberately *unlike* towers**: real `REACT_AGGRESSIVE` threat, so the
  engine's `ThreatManager` handles re-targeting on death, CC and new attackers with no
  custom code. Casters override `AttackStart` to hold at cast range.
- **Formation is one waypoint path per slot**, not spawn offsets on a shared path:
  `WaypointMovementGenerator` always walks to node 1 of whatever path it is given,
  wherever the creature actually spawned, so two units sharing a path collide. Slots are
  declared in `lane_config.yaml` and their paths generated from it.
- **Leashing is LoL-style, with no run-back** — a creep stays within a corridor of its
  own lane, resumes from where combat ended, never regresses past its furthest node, and
  stands and fights at the lane's end. Neither the engine's leash nor its home position
  can express that; `npc_moba_creep.cpp` explains why, and mid-route resume needs the
  fork-added `MotionMaster::MoveWaypoint(WaypointPath&, bool)` overload.
- **Assist rules span three files**: `UNIT_FLAG_PLAYER_CONTROLLED` on the template
  (`gen_creep_roster.py`) is what makes a creep a valid helpful-spell target at all, the
  `moba_creep_spell_gate` allow-list decides which spells land, and the inlined evade in
  `npc_moba_creep.cpp` is what lets minion buffs outlive a skirmish.
- **End of match**: `FreezeAllCreeps()` stops the living ones, and the AI separately
  suppresses `Reset()`/evade via `MatchEnded()`, or an evade would re-arm the lane.
- **Creature stats** are full copies of a real source creature with a small override
  list, enforced by `gen_creep_roster.py` — see `apps/moba/README.md`.

### Neutral camps (jungle)

Camps hostile to both teams (faction 14), spawned by `BattlegroundMOBA` on a per-camp
initial delay after doors, whole-camp respawn once the last member dies. Camp state is
`_camps`; spawns and respawns are `_bgEvents` events.

- **Aggro is camp-configured** in `neutral_config.yaml`: `aggro_range` 0 means
  pull-on-hit, above 0 rides the engine's proximity aggro via
  `creature_template.detection_range`.
- **Leashing is deliberately *unlike* creeps**: stock evade — run home, full heal — *is*
  the League camp reset, and home never drifts here because no waypoint generator runs.
  `leash_range` hard-caps the chase because the engine's own leash is freshness-bypassed
  (gotcha index).
- **Kill rewards** go through the same `GrantDeathDrops` choke point as lane creeps — see
  "On-death drops" — but with no team guard, since either team may take any camp.
- **End of match**: frozen by `FreezeAllCreeps()`, and the camp's own entry points are
  status-guarded so a frozen camp cannot be re-activated or farmed.

### On-death drops

One choke point for both minion kinds: `JustDied` (creep + neutral AIs)
resolves the killing-blow player — pet blows credit the owner, the creep AI
team-guards — and calls `BattlegroundMOBA::GrantDeathDrops` (status-guarded, so
frozen post-match minions can't be farmed). Configured per mob as a `drops`
list in `creep_config.yaml` / `neutral_config.yaml`; fields and types in
`apps/moba/README.md`.

- **Presentation is native WoW loot** — sparkle, right-click, loot window — but the
  rules are ours: `buff` grants instantly, `gold` is injected into the corpse so it can
  carry a chance (template `mingold`/`maxgold` cannot), and `item` rides native
  `creature_loot_template` rows the engine rolls itself.
- **The engine's loot rules fight last-hit attribution** in two places, defeated in two
  different files: loot rights follow the first *tapper's* group, which `GrantDeathDrops`
  re-points or strips; and reward eligibility normally demands half the mob's health in
  player damage, so the generators stamp `CREATURE_FLAG_EXTRA_NO_PLAYER_DAMAGE_REQ` on
  loot-bearing mobs (`gen_creep_roster.py`). Enforcement of "only the killing blow may
  loot" is a third file again — `moba_loot_rights.cpp`.

### Player kill drops

Player kills reward the killer directly — no corpse, no native loot, unlike the minion
path above — because a player has no creature entry to hang loot on, and a lootable
corpse has only one `lootRecipient`, which could never extend to assists or bounties.
Configured per-map in `player_config.yaml`, sharing the minion `drops` schema (fields in
`apps/moba/README.md`) but with every type rolled and delivered rather than looted.

### Kill credit and assists

All policy lives on `BattlegroundMOBA`; `moba_kill_credit.cpp` supplies the global
observation points the battleground cannot see on its own, and its `OnUnitDeath` fires
for *every* death — creep, tower, fall or player — which is why the death tally lives
there rather than in the engine's kill hooks (gotcha index).

- **Kill credit window.** A player who damaged or debuffed an enemy still gets the kill
  if that enemy dies to anything within the window and no crediting player landed the
  blow. A real enemy killing blow always beats the fallback.
- **Contribution assists**, replacing proximity: direct damage and debuffs, then anyone
  who healed or short-buffed a participant, expanded to a fixed point. Attribution only —
  assist gold is deferred to the bounty pass.
- **A duration gate** separates a combat cooldown (Power Infusion, Power Word: Shield —
  count) from a maintenance buff (Fortitude, Blessing of Wisdom — don't). Healing has no
  gate; overheal counts.

All three windows are per-map config (`base_config.yaml` → `mod_moba_base`).

### Respawn

LoL-style individual respawn, replacing the stock shared-pulse graveyard
resurrection. No core engine edits.

- **The timer starts on Release Spirit, not death.** `moba_respawn.cpp` hands the death
  to `StartRespawnTimer`; never clicking Release just leaves you dead, which is stock WoW
  behaviour and intended. A prep-phase death cannot release at all — gotcha index.
- **The wait scales with match time**, measured from doors-open, and is tuned per map in
  `base_config.yaml`.
- **No spirit healers**: the spirit guides were removed so the stock revive queue never
  populates and `_ProcessResurrect` cannot race our timer. `game_graveyard` 1103/1104 are
  kept — they are the one start-loc that spawn-in, release-repop and respawn all share.

### Recall and fountain

Both are anchored to the team's base and configured from `base_config.yaml`.

- **Recall hijacks Hearthstone** (item 6948 / spell 8690). `moba_recall.cpp` redirects
  the teleport; outside the BG it stays an ordinary Hearthstone. Being a real cast,
  movement and damage interrupt come free from the spell engine. The cast time is the
  one place this reaches into core code — a small `Spell::prepare` hook calling
  `GetRecallCastTimeMs`, so the client cast bar follows automatically.
- **The fountain heals inside the spawn dome**, in or out of combat, enemies excepted.
  Its radius is `mod_moba_base.FountainRadius` — the same number that sizes the dome
  gameobject and gates the shop, so the barrier you can see, the heal zone and the
  trading zone are one thing by construction.
- **Cosmetic limit**: the Hearthstone tooltip still reads "Returns you to \<bind\>",
  client-rendered and not changeable server-side. Resolves in the client-patch phase.

### Surrender

A team ends the match by vote. `EndBattleground(TeamId)` stays the only exit — a
passed vote is that call with the other team, so the victory/defeat pair comes
free and no new result path exists.

- **One entry point.** `.surrender` (alias `.ff`) starts a vote when none is open and
  casts a yes when one is. Every rule — match running, time gate, cooldown, threshold —
  lives on `HandleSurrenderRequest`, so `moba_surrender.cpp` owns nothing but argument
  parsing and the refusals personal to whoever typed it. An addon button would call the
  same method and need no rules of its own.
- **The threshold is all-but-one, floored**, and deliberately not config: a duo needs
  both, because at a plain `size - 1` a two-player team surrenders on one say-so, which
  is the unilateral behaviour the vote exists to remove.
- **Failure is early**, not only on the deadline — a vote closes the moment the threshold
  is out of reach and earns that team a cooldown.
- **Vote traffic is system chat, not the kill feed**, because the feed's wording lives
  entirely in the addon and its notice payload carries one number: it cannot say "2 of 4"
  or name the initiator. The enemy learns nothing until the vote passes, at which point
  both sides get the usual notice pair ahead of victory/defeat.

### Item shop

One shopkeeper NPC per base, right-clicked to open a League-style panel drawn by
the MobaHUD addon: four tabs, an icon grid with real item tooltips, sidebar
filters. Generated per map from `store_config.yaml` → `gen_store.py` →
`mod_moba_store.sql` **and** `client/addons/MobaHUD/Catalog.lua`.
`npc_moba_store.cpp` validates, charges, and grants.

- **The addon is required; there is no gossip fallback.** The NPC keeps its gossip flag
  only because that is what makes it right-clickable — no menu is ever sent. Maintaining
  two front ends forever was judged worse than requiring the addon.
- **The catalog ships with the addon, not over the wire.** Browsing is entirely
  client-side, and the server still resolves every purchase from a node id, so a stale
  `Catalog.lua` can only earn a refusal, never a wrong grant. It *can* show a wrong
  price, which is why regenerating means recopying the addon.
- **A tab is not an NPC.** One shopkeeper serves all four, so the tab is client-chosen;
  range and team gate the purchase.
- **Team lives in `mod_moba_store_npc`, not in faction.** CFBG puts players of either
  faction on either BG team, so faction cannot express team membership — the same reason
  the kill feed sends team-relative sides rather than factions.
- **Usability is the server's verdict, pushed once at `HELLO`**, and comes from the same
  check that issues the refusal, so greying and refusal cannot drift. Affordability is
  separate and purely client-side, which is why a card can be white-labelled with a red
  price.
- **Which suffixes a base may legally roll is derived**, not hand-listed — from
  `item_template.RandomSuffix` joined to `item_enchantment_template` — so cloth's
  caster-only suffixes and the wand's absence from physical bundles fall out of the data.
- **Charged last, all-or-nothing** — bag space for the whole bundle, then every item
  pre-validated, and only then does money leave. Gotcha index.
- **Everything granted is tracked and stripped**, by GUID *plus* a per-entry count. The
  pair is a workaround for `custom_items` being off: grants use stock entries, which are
  ambiguous. Cloning the catalog *and* drop items retires the bookkeeping entirely;
  `custom_items` alone does not, because looted drops keep stock entries. Known-untidy in
  `CLAUDE.md`.
- **Sell is drag-and-drop, and only for what the match gave you.** 3.3.5 gives Lua no
  item GUIDs and `GetCursorInfo` no source slot, so the addon hooks
  `PickupContainerItem` to remember where the cursor item came from and sends bag, slot
  and entry; the entry is a checksum the server refuses on mismatch rather than
  resolving. The mapping cannot name equipment slots, so gear must be unequipped first.
- **Shopkeepers spawn from the `creature` table**, not `AddCreature` — they are static
  props, and this avoids adding `BgCreatures` enum slots, an ordering trap that has cost
  two boot bugs.
- **`custom_items` is off.** The generator can clone every sold item under our own entry
  to own `SellPrice` and `Bonding`; the machinery is written and gated, but the client
  renders an entry absent from its `Item.dbc` as a "?" icon. It flips on with the client
  patch.

### HUD bar

An on-screen bar drawn by the client addon `client/addons/MobaHUD` — no client
patch, no DBC/MPQ. The server feeds it `LANG_ADDON` chat messages (prefix
`MobaHUD`, packet built like `ArenaSpectator::CreatePacket`); the 3.3.5a client
splits the message on a TAB into `(prefix, payload)`.

The addon is one file per UI over a shared namespace: `Bar.lua` (this bar),
`Feed.lua` (revive countdown, kill feed), `Gold.lua` (the floating gold numbers)
and `Shop.lua` (the item-shop panel, under its own `MobaShop` prefix — see Item
shop). `MobaHUD.lua` loads last and draws nothing: it owns the payload dispatch,
the event frame and `/mobahud`. Anything shared between modules must be published
on the `ns` table in `Core.lua` (Lua locals do not cross file boundaries).

- **Payloads** are documented once, in the header comment of `MobaHUD.lua` — the file
  that parses them. Everything carrying a side or a subject is built **per recipient**,
  so team-relative colour and POV are server-resolved and the addon owns only
  presentation (text, colours, icons).
- **Numbers**: team kills from `_teamPlayerKills` (`HandlePlayerDeath`); K/D from the
  stock `SCORE_KILLING_BLOWS`/`SCORE_DEATHS` fields; A derived free as
  `HonorableKills − KillingBlows`; CS from `BattlegroundMOBAScore::CreepKills`, credited
  by `CreditCreepKill` from both minion AIs, so jungle camps count as well as lane
  creeps. Gold is the match wallet, never the character's money.
- **Gold floats split two ways** on the server's `BG_MOBA_GoldSource`: corpse loot draws
  bare above your own character, every other source draws at the bar's gold column via
  `ns.Bar.GoldAnchor` with a per-source icon. `MOBA_GOLD_SILENT` sends no payload at all
  — passive income, starting gold and sell payouts move the wallet with no float, because
  a number that pops every five seconds all match trains the eye to ignore the ones that
  matter.
- **When it sends**: doors-open, every feed-worthy event (kills, structures, streaks,
  bosses, match flow), a per-player `R:` on Release Spirit, every 10s
  (`MOBA_HUD_RESYNC_MS`) as a resync, and `E` on match end and early leave. Transient
  feed lines are never re-sent; `R:` is, by `SendHudStateTo`, so it survives a
  `/reload` while dead.
- **The ready ping**: pushing state from `AddPlayer` does **not** work — the packet
  leaves while the client is still loading and is lost. The addon pings once on
  `PLAYER_ENTERING_WORLD` and `moba_hud.cpp` answers, covering prep-join, mid-match join
  and `/reload` with no polling. Inbound addon messages have no dedicated script hook, so
  it rides the group-chat hook, which battleground chat routes through. Keep the "should
  the HUD show?" decision on the **server** — it knows it is a `BattlegroundMOBA` on any
  map, whereas the addon would have to hard-code zone names.

---

## Recipes: structures (towers, inhibitors, base)

**Move a tower** — `.gps` at the new spot; edit `x`/`y`/`z`/`o` for that tower in
`maps/<mode>/tower_config.yaml`; `python3 apps/moba/gen_tower_data.py`; deploy.

**Add a structure (tower, inhibitor, or base)** — add a block to the map's
`tower_config.yaml` `towers` list, run `gen_tower_data.py`, deploy. The generator emits
the creature rows and the placement row together, so there is no separate SQL, and no
C++ changes are needed — the registry is data-driven. Field reference:
`apps/moba/README.md`. For an inhibitor also set `respawn_ms` and give the map a
`role: super` creep in `creep_config.yaml`, or taking it fields no super minions (the
BG warns at boot).

**Add a tier/guard dependency** — set `guarded_by` to the KEY of the structure
that must die first; omit it entirely for "always vulnerable". The guarded structure
spawns inert and is unlocked automatically. Verify it cannot be targeted initially,
then becomes attackable and fires once its guard dies.

**Change attack range or tick rate** — `attack_range` / `attack_interval_ms` in
`tower_config.yaml`; `gen_tower_data.py`; deploy.

**Change the projectile/spell** — `attack_spell_id` in `tower_config.yaml`. Tower damage
is not a separate stat; it is whatever the spell deals, so retune with a different rank
of the same family or with `attack_interval_ms`. The default 9053 renders no missile on
these prop-style models.

**Change structure model, scale, or health** — `display_id` / `display_scale` /
`health_modifier` for that structure in `tower_config.yaml`; `gen_tower_data.py`;
deploy. `health_modifier` is a multiplier on level-based base health, not an
absolute value.

**Change what counts as hard CC for aggro override** — `MOBA_HARD_CC_MECHANIC_MASK` at
the top of `moba_tower_aggro.cpp`; its comment records why the mask is neither the
engine's nor a fresh one. C++ change.

## Recipes: creeps

**Move a lane path** — walk the lane running `.gps` every ~15–20 yd (more through
curves and over bumps; node Z is interpolated linearly). Save the console
scrollback, then `python3 apps/moba/gen_creep_paths.py --extract scrollback.txt`
prints the points array. Paste it into that lane's `points` in
`maps/<mode>/lane_config.yaml`. **Order matters** — the team on the `forward` path IDs
(currently Alliance) spawns at the FIRST point; reverse the array if you walked
the other way. Run `gen_creep_paths.py`; deploy. Path IDs come from the lockfile,
so re-walking an existing lane needs no `mod_moba_creep_data` changes. Full field
and lockfile reference: `apps/moba/README.md`.

**Add a creep type** — dump the source creature to `apps/moba/sources/`, add a
`units:` entry to `creep_config.yaml` (equipment item IDs come from the source's
`creature_equip_template` row — weapons aren't in `creature_template`, and melee
swing unarmed without them), then a `creeps:` row placing it: `key`, `unit`,
`lane`, `slot`. A unit says what a creep is, the row says where it walks, so
`lane`/`slot` inside a unit is rejected. New slots go in
`maps/<mode>/lane_config.yaml` first. Run `apps/moba/gen_all.sh`; deploy. Full
field reference: `apps/moba/README.md`.

**Field more of a creep** — one `creeps:` row is one unit per wave. Add a row
with its own `key` pointing at the same `unit`, and give it a slot no other creep
on that team uses — two rows sharing a team's lane/slot is rejected, because they
would spawn on one waypoint path on top of each other. Run
`apps/moba/gen_all.sh`; deploy.

**Change a creep's movement speed** — `speed_run` in `creep_config.yaml`, a
multiplier where 1.0 is baseline. Every unit in a wave needs the same value or the
formation pulls apart as it walks. Run `apps/moba/gen_all.sh`; deploy.

**Change a creep's attack range/interval/spell** — `attack_range` /
`attack_interval_ms` / `attack_spell_id` in `creep_config.yaml` (casters only;
melee and siege attack speed comes from the source creature's `BaseAttackTime`).
Run `gen_creep_roster.py`; deploy.

**Change wave cadence or composition** — cadence is C++ in
`BattlegroundMOBA.cpp`. Spawn interval: the `Milliseconds(30000)` in
`_bgEvents.ScheduleEvent(EVENT_MOBA_SPAWN_WAVE, …)` — **two call sites**
(`StartingEventOpenDoors` for the first wave, and the reschedule inside
`PostUpdateImpl`'s event case), both must change together. Siege cadence:
`_waveCount % 3 == 0` in `PostUpdateImpl`. Unit counts are not C++ — see "Field
more of a creep".

**Change the lane-corridor width** — `MOBA_CREEP_LANE_CORRIDOR` at the top of
`npc_moba_creep.cpp`; its comment covers what the number gates and what it does not.
C++ change.

**Make a creep's attack instant/free vs. a real cast** — the `triggered` argument of
`DoCastVictim` in `npc_moba_creep.cpp`'s `CastAtVictim`. `true` bypasses cast time, mana
and GCD *regardless of the spell's own data* — right for a turret, wrong for a caster.
We shipped that bug: Fireball was instant and free until it became `false`. Applies to
all casters, not per-entry. C++ change.

**Change which spells players can cast on allied minions** — the allow-list
`switch` in `moba_creep_spell_gate::OnSpellCheckCast` (`npc_moba_creep.cpp`);
positive spells matching no allowed effect are rejected before mana/GCD are
spent. C++ change.

## Recipes: neutral camps

**Move or add a camp** — `.gps` each member spot, edit `camps` in
`maps/<mode>/neutral_config.yaml` (members reference mob keys; positions are
absolute). Run `gen_neutral_camps.py`; deploy. `CampId` is positional in config
order — safe, nothing external references it.

**Tune aggro / leash / respawn / spawn timing** — all camp-level:
`aggro_range`, `leash_range`, `respawn_ms`, and top-level `initial_spawn_ms` in
`neutral_config.yaml`. Run `gen_neutral_camps.py`; deploy. A mob key shared by
camps with different ranges fails the generator — use distinct keys.

**Add or change an on-death drop** — edit the mob's `drops` list in
`maps/<mode>/creep_config.yaml` or `neutral_config.yaml` (types and fields:
`apps/moba/README.md`). Run that config's generator; deploy — full restart, since drops
load once per process. By convention only a camp's `_large` mob carries a buff.

**Add a mob type** — a block in `mobs`, like adding a creep; a new source dump
only if the existing baseline doesn't fit (all current camp mobs share the
creep melee source — identity is name + `display_id` + `display_scale`).

## Recipes: base (spawn, respawn, recall, fountain, surrender)

All four live in one per-map bundle: `maps/<mode>/base_config.yaml` →
`gen_base.py` → `mod_moba_base.sql`.

**Move the spawn / respawn / graveyard point** — `.gps` at the new ground-level spot,
edit that team's `x`/`y`/`z`/`o` in the `spawn` block, run `gen_base.py`, deploy. One
point drives teleport-in, release-repop and respawn, and the dome and heal zone are
centred on it at runtime, so nothing else has to move. The generator writes both the
graveyard coords and the template's start-loc, so no hand `UPDATE`s. Confirm teleport-in
*and* a post-death respawn. (The `WorldSafeLocs.dbc` trap still applies.)

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

**Change the base bubble radius** — `spawn.radius`, which drives the dome's scale, the
heal zone and the shop range together (`DOME_MODEL_HALF_EXTENT` in `gen_base.py` does the
conversion). Regenerate and restart, then confirm the dome visibly changed size *and*
that healing reaches its new edge. Scale is uniform, so a wider dome is also taller.

**Change surrender timings** — `surrender` block. `min_match_ms` is the earliest a
vote may start, measured from doors open (the clock on the HUD bar, not from
entry); `vote_duration_ms` is how long a vote stays open before silence fails it
(must be > 0); `vote_cooldown_ms` is how long a team waits after a *failed* vote.
Test `.surrender` before the gate for the countdown refusal, then after it. The
threshold is deliberately not config — see the architecture section.

## Recipes: item shop

Everything lives in one per-map bundle: `maps/<mode>/store_config.yaml` →
`gen_store.py` → `mod_moba_store.sql` + `client/addons/MobaHUD/Catalog.lua`.
Deploy is a **full restart** — the data store caches once per worldserver process,
so `.debug bg` and a requeue won't do — **and a recopy of the addon**, since the
catalog ships with it and never crosses the wire.

**Add an item to a tab** — append its entry to the category's `items` list: a
bare id, or `{ entry, count, cost, name }` to override. The leaf label defaults to
the item's own `item_template` name, so most entries need nothing else. Run
`gen_store.py`; deploy.

**Add a category** — a block under that tab's `categories` holding exactly one of
`subcategories` (a branch), `pieces` (suffix bundles) or `items` (fixed items).
A top-level category's `name` becomes a sidebar filter button. Deeper nesting is
carried in each leaf's `path` but the panel reads only `path[1]`, so a
subcategory label is invisible — the second filter row derives from the item's own
class/subclass instead. Nest only where the grammar needs it: under `pieces`, a
subcategory is what makes each item its own per-suffix leaf rather than one giant
bundle.

**Add a tab** — a new `tabs` block: `key` (generator error messages only), `name`
(the tab button label) and `categories`. List order is `TabId` and the panel opens
on the first. The addon draws six tab buttons; a seventh tab generates fine and
never appears.

**Change the shopkeeper** — the `shopkeeper` block owns `name`, `subname`,
`display_id` and `display_scale`; any of them may be repeated on a `teams` line to
differ per side. Creature entries must sit inside 900300–900399 and positions come
from `.gps`. A changed name, subname or model **will not show until the client's
`Cache/` folder is deleted** — gotcha index.

**Change a price** — `cost` in copper, on the group or per item. `sell_ratio`
(map-level, overridable per group and per item) sets what it refunds, emitted to
`mod_moba_store_sell`. Both are **per unit**: a leaf granting a stack divides by
its `count`, and a `pieces` bundle divides by how many items the leaf actually
grants. Getting that wrong mints gold — gotcha index.

**Price a looted drop** — `sell:` on an `item` drop in `creep_config.yaml` or
`neutral_config.yaml`, in copper per unit. Omit it and the item cannot be sold
back at all. One entry carries one price globally: the same item priced
differently by two creatures logs an error and keeps the first.

**Pick items out of `item_template`** — filter on `Quality`, `RequiredLevel` and
`InventoryType`. Three traps, all in the gotcha index: `AllowableClass` /
`AllowableRace` encode "unrestricted" two different ways, consumables can be
profession-gated, and faction-locked items are refused against the player's
*native* race — the generator now fails the run on those rather than letting them
ship.

**Verify a generator change without touching the DB** — `build()` is pure
computation, so a run can be diffed against the committed SQL:

```bash
python3 -c "
import sys; sys.path.insert(0,'apps/moba')
import gen_store as g
cfgs=g.load_configs(); npc,menu,grant,sell,meta,copies=g.build(cfgs)
print('sql matches disk:', g.emit(npc,menu,grant,sell,copies)==open('data/sql/custom/db_world/mod_moba_store.sql').read())
print('lua matches disk:', g.emit_catalog(menu,grant,meta)==open('client/addons/MobaHUD/Catalog.lua').read())
"
```

Always confirm the committed SQL matches its generator before committing.

## Recipes: HUD and map

**Retune or extend the HUD** — resync cadence is `MOBA_HUD_RESYNC_MS` (anonymous
namespace in `BattlegroundMOBA.cpp`, default 10000); it's only a safety net now
that the ping handles joins, so lower it only if you see drift. New payloads: add
a sender beside `SendHudMessage`, extend `BuildScoreboardBody`, then handle it in
`HandlePayload` (`MobaHUD.lua`) and add the drawing to the owning module —
`Bar.lua`'s `RenderStatic()`/`RenderClock()` for bar content. Pure client,
`/reload` only.

**Move the HUD on screen** — `/mobahud unlock`, drag, `/mobahud lock`. Position
and lock persist per character (`MobaHUDDB`); `/mobahud reset` recenters.

---

## Gotcha index

One line per trap, pointing at the code comment that holds the explanation. About to
touch one of these areas? Read the named comment first.

- **Home position drifts on a waypoint walker** — restamped to the creature's current position every moving tick. → `npc_moba_creep.cpp`, `CanAIAttack` / `ResumeLaneFromHere`
- **The engine's 30 yd leash is skipped while combat stays "fresh"** → `MOBA_CREEP_LANE_CORRIDOR`
- **Evade undoes "stop this creature" logic** — reuse the `MatchEnded()` pattern for any future freeze. → `npc_moba_creep.cpp`, `Reset` / `EnterEvadeMode`
- **An inlined evade must end with `EngagementOver()`** → `npc_moba_creep.cpp`, `EnterEvadeMode`
- **`TempSummon` spawning has two traps** — the missing type param and the wrong despawn type. → `SpawnCreep`
- **`CORPSE_TIMED_DESPAWN` only counts down on a corpse** → `SpawnCamp`
- **`BgCreatures.resize()` must precede the first `AddCreature`** → `SetupBattleground`
- **A zero rotation quaternion means "derive from orientation"** → `SetupBattleground`, spawn-dome comment
- **`battleground_template.StartMaxDist` must stay 0** → `MobaBaseConfig` in `MobaBaseData.h`
- **Camp-link must ride `DamageTaken`, not `JustEngagedWith`** → `npc_moba_neutral.cpp`
- **`HandleKillPlayer` and `HandleKillUnit` are intentionally empty** → `BattlegroundMOBA.cpp`
- **Releasing spirit is impossible during BG prep** → `moba_respawn.cpp`, `OnPlayerJustDied`
- **The no-winner line is unreachable at `MinPlayersPerTeam = 1`** → `BroadcastMatchResult`
- **`DoCastVictim(id, true)` bypasses cast time, mana and GCD** → "Make a creep's attack instant/free" above
- **Helpful spells aimed at a plain friendly NPC never reach the server** → `CREEP_UNIT_FLAG_PLAYER_CONTROLLED` in `gen_creep_roster.py`
- **Stat buffs do nothing on creatures** — `Creature::UpdateStats` is a no-op. → `moba_creep_spell_gate` in `npc_moba_creep.cpp`
- **Mechanical-type creatures are hard-immune to direct heals** → `creep_config.yaml`, `creature_type` legend
- **Lane waypoints are emitted `move_type = RUN`**, so `speed_walk` is inert. → `creep_config.yaml`, `speed_run` legend
- **A `gameobject_template` copy needs its `gameobject_template_addon` row** → `DOME_ADDON_FLAGS` in `gen_base.py`
- **Money must leave only after every item is pre-validated** → `npc_moba_store.cpp`, `TryPurchase`
- **Armour proficiency is cumulative upward** — the check that matters is refusing a mage the plate set. → `TryPurchase`
- **Faction-locked items test the player's NATIVE race, not their BG team** → the faction guard in `gen_store.py`'s `build()`
- **Sell prices are per unit** — a whole node's price on each item it grants is a money printer. → `note_sell` in `gen_store.py`
- **Splitting a stack clones it under a new GUID** — tracking granted items needs a GUID set *and* a per-entry count. → `_grantedItems` / `_grantedCounts` in `BattlegroundMOBA.h`
- **Generators clear by reserved BLOCK, not by current roster** — a `DELETE` built from the roster can never name an entry the config no longer has, so a dropped mob's rows would live forever. All six generators do this. → `sql_window` in `id_alloc.py`
- **Copy whole rows through a staging table, not a hand-listed column set** → `emit_item_copies` in `gen_store.py`
- **The 3.3.5 client relocates its player object on a map change** — field changes made in the exit tick never arrive. → `RemovePlayer`
- **A Texture whose path does not resolve draws nothing at all** — no error, no placeholder square. → `headerBand` in `Shop.lua`
- **`toplevel="true"` raises only the frame that was clicked** → `sellZone`'s `OnUpdate` in `Shop.lua`
- **A `FontString` wider than its `SetWidth` wraps rather than clipping** → `Bar.lua`, `WidestDigit`
- **A 3.3.5 addon will have difficulties projecting a world position onto the screen** — corpse gold floats above your own character, not the corpse. → `PlayerPoint` in `Gold.lua`
- **A core is gated on EVERY inhibitor on its side, not on `guarded_by`** — that column names one structure. Both gates AND. → `IsStructureLocked` in `BattlegroundMOBA.cpp`
- **Structure locks are derived, never pushed** — with several inhibitors a side, "unlock what this one guarded" stops being a local decision and one respawn must re-lock what another's death opened. → `RefreshStructureLocks`
- **A creep's `lane` must match an inhibitor's for super minions to field** — a lane name outside `LANE_IDS` silently fields none. → `LANE_IDS` in `gen_creep_roster.py`

## Traps with no code home

No single line of code to hang these on, so this section is their home rather than an
index to somewhere else.

- **A chase does not end just because the target became invalid.**
  `Creature::SelectVictim` can return null *without* evading or stopping the attack
  (e.g. while anything still holds the creature on its threat list), leaving a chase
  running on a stale victim. Never assume "target unattackable ⇒ evade fires" — this
  shaped the tower's lock-until-invalid design.
- **`LOG_INFO` in a custom log category is silently dropped.** `Logger.root` in
  worldserver.conf is ERROR-level; only categories with an explicit `Logger.<name>` line
  pass INFO. Add e.g. `Logger.bg.battleground=4,Console Server` when adding debug
  logging — nothing appears otherwise. Cost a build cycle to discover.
- **`OnTowerDestroyed` only fires from its two source hooks.** A tower killed by anything
  else — environmental damage, a future non-creep source — wouldn't trigger the win
  condition. Not a real scenario today; flagged if damage sources expand.
- **`AllianceStartLoc`/`HordeStartLoc` are `game_graveyard` ids, not `WorldSafeLocs.dbc`
  ids** — despite what `BattlegroundMgr`'s own error message says on the failure branch.
  The row must exist before the template loads, and its `Map` must be the BG's map.
- **`game_graveyard.Map` moves with the coordinates.** `Player::RepopAtGraveyard`
  teleports to `ClosestGrave->Map` (`Player.cpp:4881`), so a graveyard reused on a new
  map with only its x/y/z updated throws a releasing player clean out of the
  battleground onto the old one. `gen_base.py` emits `Map` for exactly this reason.
- **One battleground slot serves exactly one map.** `battleground_template` is keyed by
  battleground *type id*, its start locations resolve once at load
  (`BattlegroundMgr.cpp:527`), and the map comes from `BattlemasterList.dbc` `mapid[0]`.
  Content tables are all `Map`-keyed and coexist happily; the slot does not. Two maps
  queueable at once needs a second battleground type id, which needs a client DBC patch.
  `base_config.yaml`'s `active:` flag is what keeps one bundle owning the slot.
- **Adding a column to a generated SQL table is a two-part trap.** The C++ store's
  `SELECT` names the new column, so a *stale* generated `.sql` (which recreates the old
  schema on boot) fails the whole query — silently zeroing every field that store feeds.
  A missing `mod_moba_base` column takes down respawn, recall and fountain, not just the
  new one. Re-run the generator. And the new column shifts the trailing comma: the
  previously-last DDL line needs one, the new last line must not. Cost two boots.
- **`AllowableClass` / `AllowableRace` encode "unrestricted" two ways** — `-1` *and* the
  all-bits-set mask (`262143` / `2147483647`). Filtering on `-1` alone silently drops
  legitimate items, and produced one confident, wrong "no such item exists" conclusion.
- **Consumables can be profession-gated** via `item_template.RequiredSkill` — bandages
  need First Aid, bombs Engineering, Crazy Alchemist's Potion Alchemy. A character
  without the skill simply cannot use what it bought.
- **`data/sql/base/` is the *historical* schema** — the live schema is base + `updates/`.
  `creature.id1` was renamed `id`; `item_template` lost `StatsCount`. When hand-writing
  SQL, trust the `SELECT` in `ObjectMgr.cpp`: it must match the live schema or the server
  wouldn't boot. Cost one failed apply.
- **Stale server data and stale client cache present identically**, and the obvious
  suspect is usually the wrong one. The 3.3.5 client caches creature and item records in
  `Cache/WDB` and never re-asks, so renaming or remodelling an entry you have already
  clicked keeps showing the OLD values until `Cache/` is deleted. But a creature whose
  drops, timers and position are all correct and whose *name* is wrong is the inverse:
  those come from tables that reloaded and the name from one that did not, so the fix is
  regenerating the SQL *and* restarting the worldserver. Both looked like "the SQL didn't
  apply". Items add a third face: until an entry is cached, `GetItemInfo` returns nil and
  `SetHyperlink` renders a lone red "Retrieving item information" line whose colour is
  **indistinguishable from a failed requirement** — so anything scanning a tooltip for red
  reads it as "cannot use", and caching that verdict poisons the item for the session.
  Never cache a conclusion drawn from client data that may not have arrived; better, ask
  the server (which is why shop usability is pushed as `NU:`).
- **A rename can *add* occurrences** — the same field is `snake_case` in yaml and Python,
  `camelCase` in C++ and `PascalCase` in SQL, so a case-insensitive find/replace also
  rewrites unrelated SCREAMING_CASE constants. Renaming `respawn_warn_ms` silently turned
  `MOBA_INHIB_RESPAWN_WARN_MS` into `MOBA_INHIB_spawn_warn_ms`, which still compiled and
  still linked. Confirming the old token is gone proves nothing — count the new one too,
  and a *rise* means something was over-matched.

## Reference: values that live in code

Positions, timings, ranges, and spells are all in `apps/moba/maps/<mode>/*.yaml`
and the SQL it generates — **read those, not a table here**. Only these live in
C++ or are allocation policy:

| What | Value |
|---|---|
| BG map id (all content rows are tagged with it) | 566 (hijacked EotS) |
| Graveyard DB IDs | 1103 (Alliance), 1104 (Horde) — reused vanilla EotS rows |
| Wave cadence | every 30s; every 3rd wave adds siege (`BattlegroundMOBA.cpp`) |
| Creep lane corridor | 40 yd, players only; +15 yd self-evade headroom (`npc_moba_creep.cpp`) |
| HUD resync cadence | 10s (`MOBA_HUD_RESYNC_MS`) |
| Recall trigger / empower placeholder | Hearthstone item 6948 / spell 8690; aura 1243 |
| Custom DB ID blocks | `apps/moba/id_blocks.json` — one owner per block, per namespace. `python3 apps/moba/id_alloc.py --audit` prints the live picture |


## Fun ideas: a list of interesting ideas that may or may not be implemented

- creep waves have a buff (uncleansable) that reduces AOE dmg by 50% when corresponding lane inhib is up
- creep waves reform after skirmishes
