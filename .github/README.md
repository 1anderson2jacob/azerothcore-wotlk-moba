# AzerothCore MOBA Battleground

A fork of [AzerothCore](https://github.com/azerothcore/azerothcore-wotlk) (WotLK 3.3.5a) that replaces Eye of the Storm with a custom MOBA-style battleground — lanes, attackable towers, and a destroy-the-base win condition, played on the existing Eye of the Storm map.

## How it works

Rather than patching client DBC files, this project **hijacks the Eye of the Storm battleground slot**: the client queues for EotS as normal, but the server runs a custom `BattlegroundMOBA` class instead of `BattlegroundEY`. No client modification is required to play.

A separate battleground ID (with a distributable client MPQ patch) is planned once gameplay stabilizes — see the roadmap.

CFBG rides the same EotS-slot hijack; the earlier GetBgTeamId audit is why the MOBA respected the BG team automatically.

## Roadmap

Shipped:

- [x] Clone `BattlegroundEY` → `BattlegroundMOBA`, wire into `BattlegroundMgr` on the EotS slot
- [x] Strip the netherstorm flag system
- [x] Strip the capture-point scoring system
- [x] Attackable towers (custom creatures with turret AI)
- [x] Win condition: destroy the enemy base tower
- [x] Lane creep waves
- [x] LoL-style individual, game-length-scaling respawn timers (replaces shared-interval graveyard resurrection)
- [x] LoL-style recall to base (Hearthstone hijacked; no client patch)
- [x] Per-map content bundles (map-keyed tables; a new map = a new `apps/moba/maps/<mode>/` bundle)
- [x] On-screen HUD bar — match clock, team score, KDA, creep score (client addon; no client patch)
- [x] Fountain healing
- [x] Enable cross-faction MOBA - via mod-cfbg (mixed-faction teams) + AllowTwoSide.Interaction.Group (cross-faction parties).
- [x] Heal/shield allied minions — heals, HoTs, absorbs, and cleanses land on your own minions (cross-faction included); other buffs are rejected ("Invalid target") — stat buffs are inert on NPCs
- [x] Last-hit kill credit — minion kills (CS) go to the player who landed the killing blow, not the first tapper; a friendly-creep last hit credits nobody
- [x] Neutral minions — four jungle camps, hostile to both teams; camp-level aggro/leash/respawn config, whole-camp linking, League-style reset; last-hit CS and an on-death buff to the killing-blow player
- [x] Minion on-death drops infrastructure
- [x] Player on-death drops infrastructure — player kills grant configurable gold/buff/item to the killer via direct grant (no corpse loot); per-map `player_config.json`
- [x] Kill credit window — a death within a configured window of enemy-player damage/debuff still credits that player, even when a creep/tower/environment lands the blow; deaths to non-players now score
- [x] Contribution-based assists — damage/debuff/heal/short-buff within the assist window earns an assist via a fixed-point support chain (replaces proximity); duration-gated so combat buffs count and maintenance buffs don't
- [x] HUD revive countdown + kill feed — center-screen respawn countdown (client-ticked, re-synced on `/reload` while dead); transient feed for player kills (per-POV text, team-relative colours, class emblems) and non-player deaths (creep/tower/neutral/environment, category label + icon)

Next, in order:

- [ ] Change config file type to yaml - json doesn't allow comments, add helpful example comments to configs after switching
- [ ] Gold / itemization mid-match
- [ ] Inhibs / Super minions
- [ ] Custom map/terrain — move onto the Twisted Treeline map (WMO route, ADT fallback; see `MOBA_MAP_WMO_PLAN.md`)
- [ ] Standalone battleground ID via `BattlemasterList.dbc` patch (client MPQ distributed to players) — bundles with the custom-map work
- [ ] Player kill rewards and bounties — gold values, bounty scaling, and assist-gold split on top of the shipped kill/assist attribution
- [ ] Client-patch bundle - recall tooltip / animation, fountain heal visuals, custom battle sounds, music

## Housekeeping

Code/doc chores — cleanups, audits, convention passes — that don't change gameplay. Not the feature roadmap above; not CLAUDE.md's "Deferred / known-untidy", which tracks live traps an editor must know while changing code.

- [x] Docs / code-comment pass — cut duplication and per-session context cost
- [x] `GetBgTeamId` vs `GetTeamId` audit across `BattlegroundMOBA`
- [ ] `BG_MOBA_Score` enum holds only the Flurry achievement ID — decide whether EotS achievements should fire at all in this mode
- [ ] Remove leftover `m_BuffChange = true` from the `BattlegroundMOBA` constructor (buffs were removed; harmless)

## Key changed files

| File | Change |
|---|---|
| `src/server/game/Battlegrounds/Zones/BattlegroundMOBA.{h,cpp}` | New battleground class (cloned from EotS, being reshaped) |
| `src/server/game/Battlegrounds/BattlegroundMgr.cpp` | `BATTLEGROUND_EY` factory entries point to `BattlegroundMOBA` |
| `src/server/game/Movement/MotionMaster.{h,cpp}` | Added public `MoveWaypoint(WaypointPath&, bool)` overload (mid-route path resume for lane creeps) |
| `client/addons/MobaHUD/` | Client addon: HUD bar (KDA, CS, clock), revive countdown, and kill/death feed — fed by server `LANG_ADDON` messages |
| `src/server/scripts/Custom/moba_hud.cpp` | Answers the addon's "ready" ping with current HUD state (group-chat `OnPlayerCanUseChat` hook) |

## Building (macOS, Apple Silicon)

Standard AzerothCore build, with two extra CMake hints because Homebrew's OpenSSL and GNU readline are keg-only on macOS (the system provides libedit, which lacks symbols the worldserver console needs):

```bash
brew install openssl@3 readline mysql cmake boost

mkdir -p var/build/obj && cd var/build/obj
cmake ../../../ \
  -DCMAKE_INSTALL_PREFIX=$(pwd)/../../../env/dist \
  -DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@3 \
  -DREADLINE_INCLUDE_DIR=/opt/homebrew/opt/readline/include \
  -DREADLINE_LIBRARY=/opt/homebrew/opt/readline/lib/libreadline.dylib
make -j$(sysctl -n hw.ncpu)
make install
```
Then, from the repo root, run `./apps/moba/setup.sh` — it generates the runtime `worldserver.conf` and module confs from their tracked `.dist` templates and layers on machine-local overrides from `apps/moba/local.conf`. Databases self-populate on the first worldserver boot (base schema, updates, then the fork's custom SQL under `data/sql/custom/db_world/`).

If using the acore.sh dashboard instead, the same flags can go in `conf/config.sh` as `CCUSTOMOPTIONS`.

For other platforms, follow the standard [AzerothCore installation guide](https://www.azerothcore.org/wiki/installation) — nothing in this fork changes the build process itself.

## Testing the battleground locally

1. Build + install, then run `./apps/moba/setup.sh` (see Building above)
2. Start `authserver` and `worldserver`, log in with a GM account
3. In-game: `.debug bg` (must be re-run after every worldserver restart), then queue for Eye of the Storm — `.debug bg` forces the per-team minimum to 1, so a solo/small queue pops
4. Level requirement is 61+; use `.character level 80` on a test character

For solo (1-client) testing, add `CFBG.EvenTeams.Enabled` = 0 to `apps/moba/local.conf` and re-run `setup.sh` — a lone player is otherwise held as an uneven team. Cross-faction only shows itself with two characters (CFBG distributes multiple players across teams).

## Syncing with upstream AzerothCore

```bash
git fetch upstream
git rebase upstream/master   # or merge, per preference
```

`upstream` should point at `https://github.com/azerothcore/azerothcore-wotlk`.

## License

GNU AGPL v3, inherited from AzerothCore. See [LICENSE](LICENSE).