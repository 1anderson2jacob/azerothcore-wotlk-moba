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
the final values back into `apps/moba/maps/<mode>/*.yaml` — the generated SQL is
the source of truth and the next apply reverts anything not recorded there.

---

## Architecture

### Towers

`BattlegroundMOBA` holds a `std::vector<MobaTowerState>` registry (team, tier,
guard dependency, destroyed flag), built in `SetupBattleground()` from
`mod_moba_tower_data`. The slot count is runtime-sized from the row count —
no per-tower enum.

- **Structure kinds** (`Kind` in `mod_moba_tower_data`): `tower` attacks;
  `inhibitor` and `core` are passive (the AI skips its tick). On death,
  `OnTowerDestroyed` always unlocks whatever the structure guarded and bumps the
  worldstate counter, then branches on kind: a **core** (the base) ends the
  battleground — that's the win condition; an **inhibitor** fields super minions
  for the killer's team and schedules its own respawn (`RespawnMs`), which
  re-locks the base and stops the super minions. Two callers: `HandleKillUnit`
  (player kills) and `npc_moba_tower::JustDied` (creep kills).
- **Guard/tier**: a structure with `GuardedByEntry` spawns unattackable and
  unselectable until its guard dies (an attacking tower also skips its tick while
  inert). `GuardedByEntry` is a single FK — linear chains only (tower → inhibitor
  → base), no multi-guard AND-gating.
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

### Neutral camps (jungle)

Camps hostile to both teams (faction 14), spawned by `BattlegroundMOBA` on a
per-camp initial delay after doors, whole-camp respawn once the last member
dies. Members are `TempSummon`s with `CORPSE_TIMED_DESPAWN` — the despawn type
whose countdown only runs on a corpse, so a living camp never despawns. Camp
state is `_camps` (`MobaCampState`); spawns/respawns are `_bgEvents` events
(`EVENT_MOBA_SPAWN_CAMP_FIRST + camp index`).

- **Aggro** is camp-configured: `aggro_range` 0 = pull-on-hit
  (`REACT_DEFENSIVE`); >0 = proximity pull via
  `creature_template.detection_range`, which is the exact radius at equal
  levels. Camp-link (hit one, all attack) rides `DamageTaken`, not
  `JustEngagedWith` — a one-shot kills before engagement ever starts.
- **Leashing**, deliberately *unlike* creeps: stock evade (run home + full
  heal) *is* the League camp reset, and home never drifts because no waypoint
  generator runs. `leash_range` hard-caps the chase from the camp anchor
  because the engine's leash is freshness-bypassed (see gotcha index).
- **Kill rewards**: CS and on-death drops go to the killing-blow player in
  `npc_moba_neutral::JustDied` via `GrantDeathDrops` — see "On-death drops".
- **End of match**: frozen by `FreezeAllCreeps()`; `PullCampMates` and the
  `JustDied` rewards are status-guarded so a frozen camp can't be re-activated
  or farmed.

### On-death drops

One choke point for both minion kinds: `JustDied` (creep + neutral AIs)
resolves the killing-blow player — pet blows credit the owner, the creep AI
team-guards — and calls `BattlegroundMOBA::GrantDeathDrops` (status-guarded, so
frozen post-match minions can't be farmed). Configured per mob as a `drops`
list in `creep_config.yaml` / `neutral_config.yaml`; fields and types in
`apps/moba/README.md`.

- **Presentation is native WoW loot** (sparkle, right-click, loot window, gold
  auto-split among nearby teammates, enemies see nothing), but the rules are
  ours: `buff` grants the aura instantly; `gold` is injected into the corpse's
  loot by `GrantDeathDrops` so it can carry a chance (template
  `mingold`/`maxgold` can't); `item` rides native `creature_loot_template`
  rows the engine rolls itself.
- **The engine's loot rules fight last-hit attribution** in two places, both
  deliberately defeated: loot rights follow the first *tapper's* group —
  `GrantDeathDrops` re-points them at the killer's team, or strips the corpse
  when no player landed the blow (no last hit, no loot; its comments cover the
  GROUP_LOOT round-robin trap) — and reward eligibility normally requires half
  the mob's health in player damage, so the generators stamp
  `CREATURE_FLAG_EXTRA_NO_PLAYER_DAMAGE_REQ` on loot-bearing mobs (comment in
  `gen_creep_roster.py`).

### Player kill drops

Player kills reward the killer directly — no corpse, no native loot, unlike the
minion `GrantDeathDrops` above. `BattlegroundMOBA::GrantPlayerKillDrops` grants
`buff`/`gold`/`item` straight to the credited killer (`AddAura` / `ModifyMoney` /
`AddItem`): a player has no creature entry to hang loot on, and a lootable player
corpse has only one `lootRecipient`, which couldn't extend to assists or bounties.
Configured per-map (no per-mob home) in `player_config.yaml` — same `drops` schema
as the minion configs (fields in `apps/moba/README.md`), but every type is a
rolled-and-delivered grant.

### Kill credit and assists

One choke point: `HandlePlayerDeath`, called from the `moba_kill_credit`
UnitScript's `OnUnitDeath` (which fires for *every* death — creep, tower, fall, or
player — unlike `HandleKillPlayer`, a deliberate no-op; see gotcha index). It owns
the death tally too, so deaths to non-players finally score.

- **Kill credit window.** A player who damaged or debuffed an enemy (`OnDamage` /
  negative `OnAuraApply`, tracked in `_recentAttackers`) still gets the kill if that
  enemy dies to anything within `kill_credit_window_ms` and no crediting player
  landed the blow. A real enemy killing blow always wins over the fallback.
- **Contribution assists**, replacing proximity. At death the credited killer plus
  everyone who damaged/debuffed the victim within `assist_window_ms` are the direct
  participants; then anyone who healed (`OnHeal`) or applied a *short* buff/shield to
  a participant is added, expanded to a fixed point — the "up to N hops" support
  chain, bounded by team size. Attribution only; assist gold is deferred to the
  bounty pass.
- **The buff duration gate** (`assist_buff_max_duration_ms`) separates a combat
  cooldown (Power Infusion, Bloodlust, Power Word: Shield — count) from a maintenance
  buff (Fortitude, Blessing of Wisdom — don't). Healing has no gate; overheal counts.

Windows and the gate are per-map config (`base_config.yaml` → `mod_moba_base`).

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

Both are anchored to the team's base and configured from `base_config.yaml`.

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
  percentage of max health and mana per tick to players inside their own spawn
  dome, in or out of combat; enemies get nothing. The zone is
  `mod_moba_base.FountainRadius` — the same number that sizes the dome
  gameobject, so the heal zone and the barrier you can see are one thing.
- **Cosmetic limit**: the Hearthstone's on-use tooltip still reads "Returns you
  to \<bind\>" — client-rendered from the spell's bind, not changeable
  server-side. Resolves when recall becomes its own spell in the client-patch
  phase.

### Item shop

One shopkeeper NPC per base, right-clicked to open a League-style panel drawn by
the MobaHUD addon: four tabs, an icon grid with real item tooltips, sidebar
filters. Generated per map from `store_config.yaml` → `gen_store.py` →
`mod_moba_store.sql` **and** `client/addons/MobaHUD/Catalog.lua`.
`npc_moba_store.cpp` validates, charges, and grants.

- **The addon is required; there is no gossip fallback.** The NPC keeps
  `npcflag = 1` (`UNIT_NPC_FLAG_GOSSIP`) only because that is what makes it
  right-clickable and fires `OnGossipHello` — no menu is ever sent. A player
  without the addon gets a chat message. Maintaining two front ends forever was
  judged worse than requiring the addon.
- **The catalog ships with the addon, not over the wire.** Browsing is entirely
  client-side. The server still resolves every purchase from a node id, so a
  stale `Catalog.lua` can only earn a refusal, never a wrong grant — but it *can*
  show a wrong price, which is why regenerating means recopying the addon.
- **`TabId` is a tab, not an NPC.** One shopkeeper serves all four tabs, so the
  tab bought from is client-chosen; range and team gate the purchase, and the node
  must exist for the requested tab. `mod_moba_store_npc` carries only
  (entry, map, team).
- **Team lives in `mod_moba_store_npc`, not in faction.** Shopkeepers are faction
  35 (friendly to all) and immune; CFBG puts players of either faction on either
  BG team, so faction cannot express team. The script refuses a mismatched
  `GetBgTeamId`.
- **Usability is the server's verdict, pushed once at `HELLO`.** `NU:` batches
  name the entries the player cannot use; the addon greys those cards and disables
  Purchase. It comes from the same `ItemUnusableReason` that issues the refusal,
  so greying and refusal cannot drift. Affordability is separate and purely
  client-side (`GetMoney()`) — which is why a card can be white-labelled with a
  red price.
- **Two kinds of leaf, one grant table.** A `pieces` group hangs one leaf per
  random suffix and grants a whole bundle under it; an `items` group hangs one
  leaf per fixed named item with no suffix. Which suffixes a base may legally roll
  is derived from `item_template.RandomSuffix` joined to
  `item_enchantment_template` and never hand-listed, so cloth's caster-only
  suffixes and the wand's absence from physical bundles fall out of the data.
- **Charged last, all-or-nothing.** Bag space is checked for the whole bundle,
  then every item is pre-validated, and only then does money leave — see the
  gotcha index.
- **Everything granted is tracked and stripped — by GUID *plus* a per-entry count,
  as a workaround.** Items are soulbound at grant and recorded in
  `BattlegroundMOBA::_grantedItems` / `_grantedCounts`, so every exit path destroys
  what the match handed out. The pair is only necessary because `custom_items` is
  off and grants use stock entries, which are ambiguous — and the claim can outlive
  the item, so a world-obtained copy of the same entry is not always safe
  (known-untidy in `CLAUDE.md`). Cloning the catalog *and* drop items retires the
  bookkeeping; `custom_items` alone does not, because looted drops keep stock entries.
- **Sell is drag-and-drop, and only for what the match gave you.** Dropping a bag
  item anywhere on the shop's content region refunds `sell_ratio` of what it cost;
  looted drops refund the `sell` on their drop config. The drop zone is an overlay
  that exists only while an item rides the cursor, and deliberately spares the tab
  strip and footer — while it is up, every click it covers sells. 3.3.5 gives Lua no
  item GUIDs and `GetCursorInfo` no source slot, so the addon hooks
  `PickupContainerItem` to remember where the cursor item came from and sends
  `SELL:<bag>,<slot>,<entry>`; the entry is a checksum the server refuses on
  mismatch rather than resolving, so a stale pickup can only earn a refusal. The
  slot mapping cannot name equipment slots, so equipped gear must be unequipped
  first.
- **Shopkeepers spawn from the `creature` table**, not `AddCreature` — they are
  static props, and this avoids adding `BgCreatures` enum slots (an ordering trap
  that has caused two boot bugs). It is also why this generator alone clears a
  reserved entry window; see the gotcha index.
- **`custom_items` is off.** The generator can clone every sold item under our own
  entry (`+900000`) to own `SellPrice` and `Bonding`; the machinery is written and
  gated, but the client renders an entry absent from its `Item.dbc` as a "?" icon
  with zero suffix stats. Server-side both paths work — it flips on with the
  client patch.

### HUD bar

An on-screen bar drawn by the client addon `client/addons/MobaHUD` — no client
patch, no DBC/MPQ. The server feeds it `LANG_ADDON` chat messages (prefix
`MobaHUD`, packet built like `ArenaSpectator::CreatePacket`); the 3.3.5a client
splits the message on a TAB into `(prefix, payload)`.

The addon is one file per UI over a shared namespace: `Bar.lua` (this bar),
`Feed.lua` (revive countdown, kill feed), and `Shop.lua` (the item-shop panel,
under its own `MobaShop` prefix — see Item shop). `MobaHUD.lua` loads last and
draws nothing: it owns the payload dispatch, the event frame and `/mobahud`.
Anything shared between modules must be published on the `ns` table in
`Core.lua` (Lua locals do not cross file boundaries).

- **Payloads**: `T:<seconds>` starts/syncs the clock (the addon counts up locally
  between messages), `S:<ally>,<enemy>,<k>,<d>,<a>,<cs>` updates the scoreboard,
  `R:<seconds>` starts the revive countdown (client ticks down; `R:0` hides it),
  `K:…` a player-kill feed line, `D:…` a non-player death feed line, `E` hides the
  bar. `S:`/`K:`/`D:` are built **per recipient** (`BuildScoreboardBody`,
  `BroadcastKillFeed`, `BroadcastNonPlayerDeath`) so team side and POV are
  server-resolved; the addon owns only presentation (text, colours, icons). Full
  field layouts live in the header comment of `MobaHUD.lua`.
- **Numbers**: team kills from `_teamPlayerKills` (`HandleKillPlayer`); K/D from
  the stock `SCORE_KILLING_BLOWS`/`SCORE_DEATHS` fields; A derived free as
  `HonorableKills − KillingBlows`, since WoW already credits an honorable kill to
  every teammate near the victim; CS from `BattlegroundMOBAScore::CreepKills`
  (`HandleKillUnit`, lane creeps only — towers aren't in the creep store, so they
  naturally don't count).
- **When it sends**: doors-open, on every player kill (`K:` + scoreboard), to the
  killer on a creep last-hit, on a non-player death (`D:`), a per-player `R:` on
  Release Spirit, every 10s (`MOBA_HUD_RESYNC_MS`) as a resync, and `E` on match
  end and early leave. Transient feed lines (`K:`/`D:`) are never re-sent; the
  countdown `R:` is re-sent by `SendHudStateTo`, so it survives a `/reload` while dead.
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

## Recipes: structures (towers, inhibitors, base)

**Move a tower** — `.gps` at the new spot; edit `x`/`y`/`z`/`o` for that tower in
`maps/<mode>/tower_config.yaml`; `python3 apps/moba/gen_tower_data.py`; deploy.

**Add a structure (tower, inhibitor, or base)** — add a block to the map's
`tower_config.yaml` `towers` list; `gen_tower_data.py` generates the creature
(`creature_template` + `creature_template_model`) and the placement row together,
so there's no separate SQL to touch. Fields: `key` (a stable name — the entry is
assigned for you into `tower_config.lock.json`), `team`, `kind`
(`tower`/`inhibitor`/`core`), `tier`, `guarded_by`, `name`,
`display_id`, `display_scale`, `health_modifier`, `.gps` coords, and the
`attack_*` fields (inert for passive kinds). For an inhibitor also set
`respawn_ms` and make sure the map has a `role: super` creep in
`creep_config.yaml` — otherwise taking the inhibitor fields no super minions (the
BG warns at boot). Run `gen_tower_data.py`; deploy. No C++ changes — the registry
and slot count are data-driven.

**Add a tier/guard dependency** — set `guarded_by` to the KEY of the structure
that must die first; omit it entirely for "always vulnerable". The guarded tower
die first. The guarded tower spawns inert and `OnTowerDestroyed` unlocks it
automatically. Verify it can't be targeted initially, then becomes attackable and
fires once its guard dies.

**Change attack range or tick rate** — `attack_range` / `attack_interval_ms` in
`tower_config.yaml`; `gen_tower_data.py`; deploy.

**Change the projectile/spell** — `attack_spell_id` in `tower_config.yaml`.
Tower damage isn't an independent stat; it's entirely whatever the spell deals.
To retune damage without changing the look, use a different rank of the same
spell family, or change `attack_interval_ms`. The current default (9053, a
"Shoot" clone) deals real damage with no weapon dead zone, but its missile
doesn't render on these prop-style display models (likely no bone attachment
point). Towers cast `triggered = true` — deliberately instant and free, matching
a turret.

**Change structure model, scale, or health** — `display_id` / `display_scale` /
`health_modifier` for that structure in `tower_config.yaml`; `gen_tower_data.py`;
deploy. `health_modifier` is a multiplier on level-based base health, not an
absolute value.

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
`lmaps/<mode>/lane_config.yaml`. **Order matters** — the team on the `forward` path IDs
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
`lmaps/<mode>/lane_config.yaml` first. Run `apps/moba/gen_all.sh`; deploy. Full
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
`apps/moba/README.md`). Run that config's generator; deploy — full restart,
drops load once per process. Buffs normally only on a camp's large.

**Add a mob type** — a block in `mobs`, like adding a creep; a new source dump
only if the existing baseline doesn't fit (all current camp mobs share the
creep melee source — identity is name + `display_id` + `display_scale`).

## Recipes: base (spawn, respawn, recall, fountain)

All four live in one per-map bundle: `maps/<mode>/base_config.yaml` →
`gen_base.py` → `mod_moba_base.sql`.

**Move the spawn / respawn / graveyard point** — `game_graveyard` 1103/1104 drive
three things at once: initial teleport-in (via `battleground_template` start-loc),
release-repop, and respawn. `.gps` at the new ground-level spot, edit that team's
`x`/`y`/`z`/`o` in the `spawn` block, run `gen_base.py`, deploy. The generator
writes both the graveyard coords and the template's `StartLoc`/`StartO` — no hand
`UPDATE`s. (The `WorldSafeLocs.dbc` gotcha below still applies.) Confirm both
teleport-in and a post-death respawn. The spawn dome and the fountain heal zone are
centered on this point at runtime, so both follow automatically — nothing else to move.

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

**Change the base bubble radius** — `spawn.radius`. One number drives both halves
of the bubble: the dome gameobjects' scale and `mod_moba_base.FountainRadius` (see
`DOME_MODEL_HALF_EXTENT` in `gen_base.py` for the conversion). It is deliberately
*not* written to `battleground_template.StartMaxDist` — see the gotcha index.
Regenerate and restart, then confirm the dome visibly changed size *and* that
healing reaches its new edge. Scale is uniform, so a wider dome is also a taller one.

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
- **`DoCastVictim(id, true)` bypasses cast time, mana, and GCD** regardless of the
  spell's data. → "Make a creep's attack instant/free" above.
- **Helpful spells aimed at a plain friendly NPC never reach the server** — the
  client silently self-casts instead; `UNIT_FLAG_PLAYER_CONTROLLED` is what marks
  a unit as a valid helpful-spell target. → `gen_creep_roster.py`,
  `CREEP_UNIT_FLAG_PLAYER_CONTROLLED` comment.
- **Stat buffs do nothing on creatures** (`Creature::UpdateStats` is a no-op).
  → `npc_moba_creep.cpp`, `moba_creep_spell_gate` comment.
- **Mechanical-type creatures are hard-immune to direct heals.**
  → `creep_config.yaml`, `creature_type` legend.
- **`battleground_template.StartMaxDist` must stay 0** — any non-zero value arms the
  core's prep-phase leash, which teleports players back to spawn every 9s. Shipped as
  a real bug: it read as a random position/orientation reset ~8s after loading in.
  → `MobaBaseConfig` comment in `MobaBaseData.h`.
- **A `gameobject_template` copy needs its `gameobject_template_addon` row too** —
  faction and flags are read from nowhere else, so a copy without one spawns
  faction 0 / flags 0: client-selectable, and clicking a DOOR opens it. Players could
  lift their own spawn dome (shipped as a real bug). → `DOME_ADDON_FLAGS` in `gen_base.py`.
- **A zero rotation quaternion is legal and means "derive from orientation"** —
  `SetWorldRotation` falls back to a Z-axis rotation from the orientation, so
  hand-computed `sin(o/2)`/`cos(o/2)` literals are redundant. → `SetupBattleground()`,
  spawn-dome comment.
- **An inlined evade must end with `EngagementOver()`** — omit it and the
  creature stays "engaged" forever and ignores every later enemy.
  → `npc_moba_creep.cpp`, `EnterEvadeMode`.
- **Camp-link must ride `DamageTaken`, not `JustEngagedWith`** — a one-shot
  kills before engagement starts and the pull never fires (shipped as a real
  bug). → `npc_moba_neutral.cpp`, `DamageTaken` comment.
- **`CORPSE_TIMED_DESPAWN`'s countdown only runs on a corpse** — the trap for
  lane creeps is load-bearing for camps. → `SpawnCamp` in `BattlegroundMOBA.cpp`.
- **`HandleKillPlayer` is intentionally empty** — the engine only calls it on a
  player/pet killing blow, but MOBA deaths are as often finished by a creep, tower,
  or environment, so all crediting + death tallying lives in `HandlePlayerDeath` via
  `OnUnitDeath`. → `BattlegroundMOBA.cpp`, `HandleKillPlayer` / `HandlePlayerDeath`.
- **Money must leave only after every item is pre-validated** — a
  `CanStoreNewItem` inside the grant loop is too late: a unique item the player
  already owns is refused there and the gold is already gone. The bundle path also
  needs its own free-slot check, because per-item validation can't see the slots
  the bundle's earlier pieces will take. → `npc_moba_store.cpp`, `TryPurchase`.
- **Armour proficiency is cumulative upward** — plate implies mail, leather and
  cloth, so a warrior can wear anything and the check that matters is refusing a
  *mage* the plate set, never the reverse. Getting this backwards sends you
  hunting a bug that isn't there. → `npc_moba_store.cpp`, `TryPurchase`.
- **Faction-locked items are refused against the player's NATIVE race, not their
  BG team.** `Player::CanUseItem` tests `ITEM_FLAG2_FACTION_HORDE/ALLIANCE`
  against `GetTeamId(true)`, so under CFBG one player can buy an item their own
  teammate cannot — unfairness *within* a side, invisible unless looked for. Six
  Alliance-only items shipped in the rare tier before this was caught. → the
  faction guard in `gen_store.py`'s `build()`.
- **A generator that owns `creature` rows must clear a reserved entry WINDOW**,
  not merely the entries it is about to insert. Deleting only what you insert can
  add and modify but never remove, so an NPC dropped from a config stays spawned
  forever. Only the store generator needs this — the others own templates only,
  and an orphaned template is inert; an orphaned spawn is a live scripted NPC.
  → `SHOP_ENTRY_MIN` in `gen_store.py`.
- **Copy whole rows through a staging table, not a hand-listed column set** — an
  upstream column change breaks the enumeration, and `item_template` has already
  lost one. → `emit_item_copies` in `gen_store.py`.
- **The 3.3.5 client relocates its player object on a map change rather than
  recreating it** — field changes made in the tick a player leaves never reach it,
  so stripped gear stays rendered until relog. `ForceValuesUpdateAtIndex` does not
  help; it only marks fields dirty. → `RemovePlayer` in `BattlegroundMOBA.cpp`.
  - **A Texture whose path does not resolve draws nothing at all** — no error, no
  placeholder square, so wrong art is indistinguishable from a region you forgot
  to show or size, and candidates have to be tried in the running client one at a
  time. Only verified paths belong in committed code.
  → `headerBand` in `Shop.lua`.
- **`toplevel="true"` raises only the frame that was clicked** — the bag a player
  picked an item from ends up above a shop overlay while every other open bag stays
  below it, so no frame level both clears the panel's own children and loses to all
  the bags at once. A full-panel drop target has to stand its mouse input down over
  bags rather than try to out-level them. → `sellZone`'s `OnUpdate` in `Shop.lua`.
- **Lane waypoints are emitted `move_type = RUN`**, so `speed_run` governs lane
  pacing and `speed_walk` is inert — source creatures whose `speed_run` differs
  drift out of formation. → `creep_config.yaml`, `speed_run` legend.
- **Splitting a stack CLONES it under a new GUID** (`Player::SplitItem` →
  `Item::CloneItem`), and looting MERGES into a stack the player already held. So
  an item GUID names a stack but not whose items are inside it, and a granted
  stack can end up under a GUID the match never recorded. Tracking match-granted
  items needs a GUID set *and* a per-entry count — neither alone is right.
  → `_grantedItems` / `_grantedCounts` in `BattlegroundMOBA.h`.
- **Sell prices are per unit, as `item_template.SellPrice` is** — writing a whole
  node's price onto each item it grants is a money printer. A 5-potion leaf
  refunded 6250 on a 5000 purchase; a priced 9-piece bundle would have refunded
  2.25x. → `note_sell` in `gen_store.py`.
- **A `FontString` wider than its `SetWidth` wraps — it does not clip** — the
  overflow becomes a second line that spills out of the parent's backdrop. Measure
  the widest glyph at the live scale; never assume a font's digits are tabular, and
  never hardcode 8. → `client/addons/MobaHUD/Bar.lua`, `WidestDigit`.
- **Releasing spirit is impossible during BG prep** — `SPELL_PREPARATION` (44521)
  carries `SPELL_AURA_PREVENT_RESURRECTION`, so `HandleRepopRequestOpcode` drops the
  request silently while the client auto-accepts its own death popup. Anything waiting
  on the released-ghost hook never runs, and an instanced map has neither a spirit
  healer nor `Player::Update`'s auto-release as a fallback, so a prep death strands the
  player — looking alive, but rooted and dead — for the whole match (shipped as a real
  bug). → `moba_respawn.cpp`, `OnPlayerJustDied`.

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
- **Adding a column to a generated SQL table is a two-part trap.** The C++ store's
  `SELECT` names the new column, so a *stale* generated `.sql` (which recreates the
  old schema on boot) fails the whole query — silently zeroing every field that store
  feeds (a missing `mod_moba_base` column takes down respawn/recall/fountain, not
  just the new one). Re-run the generator. And the new column shifts the
  trailing-comma: the previously-last DDL line needs a comma, the new last line must
  not. Cost two boots.
- **`AllowableClass` / `AllowableRace` encode "unrestricted" two ways** — `-1`
  *and* the all-bits-set mask (`262143` / `2147483647`). Filtering on `-1` alone
  silently drops legitimate items, and produced one confident, wrong "no such item
  exists" conclusion during the shop's item pass.
- **Consumables can be profession-gated** via `item_template.RequiredSkill` —
  bandages need First Aid, bombs Engineering, Crazy Alchemist's Potion Alchemy. A
  character without the skill simply cannot use what it bought.
- **`data/sql/base/` is the *historical* schema** — the live schema is base +
  `updates/`. `creature.id1` was renamed `id`; `item_template` lost `StatsCount`.
  When hand-writing generated SQL, trust the `SELECT` in `ObjectMgr.cpp` — it must
  match the live schema or the server wouldn't boot. Cost one failed apply.
- **The 3.3.5 client caches creature and item data in `Cache/WDB` and never
  re-asks.** A creature's name, subname and model are one cached record, so
  renaming or remodelling an entry you have already clicked keeps showing the OLD
  values until the client's `Cache/` folder is deleted — this looked like "the SQL
  didn't apply" twice. Items behave the same: until an entry is cached
  `GetItemInfo` returns nil and `SetHyperlink` renders a lone red "Retrieving item
  information" line, whose colour is **indistinguishable from a failed
  requirement** — so anything scanning a tooltip for red reads it as "cannot use",
  and caching that verdict poisons the item for the session. Never cache a
  conclusion drawn from client data that may not have arrived; better, ask the
  server (which is why shop usability is pushed as `NU:` rather than scanned).


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
