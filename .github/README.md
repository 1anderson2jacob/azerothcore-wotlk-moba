# AzerothCore MOBA Battleground

A fork of [AzerothCore](https://github.com/azerothcore/azerothcore-wotlk) (WotLK 3.3.5a) that replaces Eye of the Storm with a custom MOBA-style battleground — lanes, attackable towers, and a destroy-the-base win condition, played on the existing Eye of the Storm map.

## How it works

Rather than patching client DBC files, this project **hijacks the Eye of the Storm battleground slot**: the client queues for EotS as normal, but the server runs a custom `BattlegroundMOBA` class instead of `BattlegroundEY`. No client modification is required to play.

A separate battleground ID (with a distributable client MPQ patch) is planned once gameplay stabilizes — see the roadmap.

## Roadmap

- [x] Clone `BattlegroundEY` → `BattlegroundMOBA`, wire into `BattlegroundMgr` on the EotS slot
- [x] Strip the netherstorm flag system
- [x] Strip the capture-point scoring system
- [x] Attackable towers (custom creatures with turret AI)
- [x] Win condition: destroy the enemy base tower
- [x] Lane creep waves
- [x] LoL-style individual, game-length-scaling resurrection timers (replaces shared-interval graveyard resurrection)
- [x] LoL-style recall to base (Hearthstone hijacked; no client patch)
- [x] Per-map content bundles (map-keyed tower/creep/respawn tables; a new map = a new `apps/moba/maps/<mode>/` bundle)
- [x] On-screen match clock (client addon; no client patch)
- [ ] UI scoreboard
- [ ] Fountain healing
- [ ] Gold / itemization mid-match
- [ ] Custom map/terrain
- [ ] Standalone battleground ID via `BattlemasterList.dbc` patch (client MPQ distributed to players)

## Key changed files

| File | Change |
|---|---|
| `src/server/game/Battlegrounds/Zones/BattlegroundMOBA.{h,cpp}` | New battleground class (cloned from EotS, being reshaped) |
| `src/server/game/Battlegrounds/BattlegroundMgr.cpp` | `BATTLEGROUND_EY` factory entries point to `BattlegroundMOBA` |
| `src/server/game/Movement/MotionMaster.{h,cpp}` | Added public `MoveWaypoint(WaypointPath&, bool)` overload (mid-route path resume for lane creeps) |
| `client/addons/MobaClock/` | Client addon: on-screen match clock, fed by server `LANG_ADDON` messages |

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

If using the acore.sh dashboard instead, the same flags can go in `conf/config.sh` as `CCUSTOMOPTIONS`.

For other platforms, follow the standard [AzerothCore installation guide](https://www.azerothcore.org/wiki/installation) — nothing in this fork changes the build process itself.

## Testing the battleground locally

1. Set EotS minimum players to 1:
   ```sql
   UPDATE acore_world.battleground_template SET MinPlayersPerTeam=1 WHERE ID=7;
   ```
2. Start `authserver` and `worldserver`, log in with a GM account
3. In-game: `.debug bg` (must be re-run after every worldserver restart), then queue for Eye of the Storm
4. Level requirement is 61+; use `.character level 80` on a test character

## Syncing with upstream AzerothCore

```bash
git fetch upstream
git rebase upstream/master   # or merge, per preference
```

`upstream` should point at `https://github.com/azerothcore/azerothcore-wotlk`.

## License

GNU AGPL v3, inherited from AzerothCore. See [LICENSE](LICENSE).