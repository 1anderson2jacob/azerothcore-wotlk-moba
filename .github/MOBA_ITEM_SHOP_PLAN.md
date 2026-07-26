# MOBA Item Shop — plan / handoff

**Status:** partially complete. The starting-gear vendor is shipped and tested.
Three vendors and the documentation pass remain. **Delete this file when the
feature lands** — it is never committed.

## What this is

Gossip-driven shop NPCs standing in each base. A player right-clicks a vendor,
navigates a nested menu, and buys gear. Roadmap item: "Gear and items
infrastructure".

## Shipped and tested

- `starting_gear` vendor, creature entries 900300 (team 0) / 900301 (team 1)
- Arbitrary-depth gossip tree: category → optional subcategory → purchase leaf
- Suffix availability **derived**, never hand-listed: `item_template.RandomSuffix`
  joined to `item_enchantment_template` decides which "of the X" a base can roll
- Bundle grants: armour category + suffix hands over 8 slots + a cloak (9 items);
  weapons are type → suffix → a single item
- All-or-nothing purchase (bag space checked for the whole bundle)
- Team-gated: `player->GetBgTeamId()` vs the vendor's team
- Items are soulbound at grant, and every exit path destroys exactly the items
  the shop handed out
- 7 categories, 207 menu nodes, 470 grant rows, 51 distinct items

Files: `apps/moba/gen_store.py`, `apps/moba/maps/eye_of_the_storm/store_config.yaml`,
`src/server/game/Battlegrounds/Zones/MobaStoreData.{h,cpp}`,
`src/server/scripts/Custom/npc_moba_store.cpp`, plus `RecordGrantedItem` /
`RemovePlayer` in `BattlegroundMOBA.{h,cpp}`.

## Remaining

### 1. Three more vendors

`consumables` (900302/03), `rare_gear` (900304/05), `epic_gear` (900306/07).

Shape Jacob specified:

- Consumables: 10 items including health and mana potions
- Rare and Epic: cloth / leather / mail / plate / shield (2 pieces each),
  weapons (1 of each type), accessories (2 rings + 1 neck)
- **No "of the X" suffixes** — these are fixed named blues and purples
- Pricing: rare 1g, epic 2g, consumables 50s. `sell_ratio` (default 0.25) is
  already plumbed; sell price only takes effect when `custom_items` is on

**This needs a generator change, not just config.** Every `pieces` group today
produces one purchase leaf *per suffix*. These vendors need leaves that are
**individual fixed items with no suffix** — roughly an `items:` group mode
alongside the existing `pieces:`. The C++ needs nothing: a purchase node with a
single grant row already works.

Jacob supplies positions (`.gps`) and `display_id` (`.morph`).

### 2. Documentation

- `MOBA_GUIDE.md`: a recipe for adding a vendor / category / item, plus gotcha
  index entries for the traps listed below
- `CLAUDE.md`: a row in the "Where things live" table for the shop
- `.github/README.md`: tick the roadmap item

## Decisions already made — do not relitigate

- **Gossip, not a native vendor.** `npc_vendor` is keyed on creature entry, so
  one NPC gets one flat list — it cannot filter per gossip branch.
- **Gossip first, addon maybe later.** `TryPurchase` is deliberately
  front-end-agnostic so an addon can call it without a rewrite.
- **No lockfile for menu node ids** — nothing persistent references them, so
  fresh ids each run are safe. (Item entries *would* need one; see below.)
- **`custom_items: false`.** Custom item copies (`entry + 900000`) work fully
  server-side but the client renders them as a "?" icon with +0 suffix stats,
  because it has no `Item.dbc` row for the entry. The generator machinery is
  written and gated; flipping the flag on is a one-line change once the client
  patch ships those rows. Clearing the client's `Cache/` folder does **not** fix
  it — that was tested.
- **GUID tracking, not entry sweeping.** While `custom_items` is off the shop
  hands out stock entries, so sweeping by entry would destroy a player's own
  world-obtained copies. `BattlegroundMOBA::_grantedItems` records exactly what
  was handed out. Once `custom_items` is on, a stateless entry sweep can be
  restored as a catch-all.
- **Vendors are faction 35 (friendly to all)**, with team carried in
  `mod_moba_store_npc`. CFBG puts players of either faction on either BG team,
  so faction cannot express team.
- **Vendors spawn via the `creature` table**, not `AddCreature` — they are
  static props, and this avoids adding `BgCreatures` enum slots (a trap that has
  caused two boot bugs).

## Hard-won facts

- **`data/sql/base/` is the *historical* schema.** The live schema is base +
  `updates/`. `creature.id1` was renamed to `id` (2026_06_16_00) and
  `item_template` lost `StatsCount` (2025_08_01_00). When in doubt, trust the
  `SELECT` in `ObjectMgr.cpp` — it must match the live schema or the server
  would not boot. Getting this wrong cost one failed SQL apply.
- **Generated SQL that copies whole rows should be schema-agnostic**: copy via a
  staging table (`CREATE TABLE x AS SELECT *`) rather than hand-listing columns,
  so an upstream column change cannot break it. `emit_item_copies` does this.
- `item_template` field indices, verified against the dump's own DDL (138
  columns): entry 0, name 4, displayid 5, Quality 6, BuyPrice 10, SellPrice 11,
  InventoryType 12, ItemLevel 15, RequiredLevel 16, bonding 100,
  RandomProperty 109, RandomSuffix 110, duration 128.
- **Suffix ids**: Tiger 14, Bear 7, Monkey 5, Eagle 6, Owl 9, Gorilla 10,
  Falcon 11, Boar 12, Wolf 13, Whale 8.
- **Cloth body armour, wands and caster off-hands only roll Eagle / Owl / Whale**
  (caster suffixes). This falls out of the data automatically — never hand-list it.
- **`randomPropertyId` must be negated** for suffixes — see
  `Item::GenerateItemRandomPropertyId`.
- `ObjectMgr::LoadItemTemplates` inserts the template into the store *before*
  the `Item.dbc` check, so a missing `item_dbc` row only skips cross-checks; the
  item still works server-side.
- **The 3.3.5 client relocates its own player object on a map change rather than
  recreating it.** Field changes made in the tick a player leaves the world never
  reach it, so stripped gear stays rendered until relog. Hence the explicit
  `BuildValuesUpdateBlockForPlayer` + `SendDirectMessage` in `RemovePlayer`.
  `ForceValuesUpdateAtIndex` does *not* work here — it only marks fields dirty.
- The DB still holds **51 orphan `936xxx` `item_template` rows** from the
  custom-items experiment. Harmless (nothing references them) and self-healing
  when `custom_items` is re-enabled, since that SQL deletes then re-inserts.

## Verifying generator changes without touching the DB

`build()` is pure computation, so it can be dry-run and the output compared to
the committed SQL:

```bash
python3 -c "
import sys; sys.path.insert(0,'apps/moba')
import gen_store as g
cfgs=g.load_configs(); npc,menu,grant,meta,copies=g.build(cfgs)
print('matches disk:', g.emit(npc,menu,grant,copies)==open('data/sql/custom/db_world/mod_moba_store.sql').read())
print('menu',len(menu),'grant',len(grant),'copies',len(copies))
"
```

Always confirm the committed SQL matches its generator before committing.

## Testing

`.debug bg` after every restart, queue Eye of the Storm, walk to the vendor.
Worth re-running whenever the grant or sweep path changes:

1. Icon and real suffix stats show in the bag
2. Tooltip reads Soulbound; equipping raises no bind prompt
3. Leave button strips gear from bags *and* equipped slots, model updates
4. Logout mid-match strips gear
5. Buying two bundles then leaving (exercises multiple tracked GUIDs)
6. `.additem 36173` before entering, buy Leather → of the Tiger, leave — the
   `.additem`'d copy must survive (proves GUID tracking, not entry sweeping)
