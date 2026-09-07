# AzerothCore MOBA Battleground

A MOBA built inside World of Warcraft 3.3.5a — lanes, creep waves, towers, a jungle, an item shop, and a destroy-the-enemy-base win condition — on a fork of [AzerothCore](https://github.com/azerothcore/azerothcore-wotlk).

## What a match looks like

Two teams spawn at opposite ends of the map. Every 30 seconds each side sends a wave of AI minions down the lane; they meet in the middle and fight. Landing the killing blow on a minion pays you gold, and so do jungle camps, enemy players, and objectives.

You spend that gold at a shop in your own base — starting gear, consumables, rare and epic tiers — and use what you buy to push. Each side's lane is held by a tower, then an inhibitor, then the core. Taking an inhibitor sends stronger minions at the enemy until it regrows; destroying the core ends the match. A team that is clearly beaten can `.surrender` rather than play it out.

The gold is a match wallet, not your character's money, and everything the match hands you is taken back when you leave. Nothing that happens in here follows your character out.

**Live today:** one lane (mid), three structures a side, five jungle camps. Level 61–80, 3v3.

## How it works

Twisted Treeline is its own battleground: type id 12, on custom map 900. Eye of the Storm is stock and untouched. The client needs the MPQ patch — `apps/moba/wmo/README.md` builds it — which carries the map plus the `BattlemasterList.dbc` and `PvpDifficulty.dbc` rows the PvP frame needs to offer the battleground at all. The **MobaHUD** addon (`client/addons/MobaHUD/`) is optional: without it the match plays, but the shop is unreachable.

## Roadmap

Shipped:

- [x] `BattlegroundEY` cloned to `BattlegroundMOBA`, wired onto its own battleground slot
- [x] Netherstorm flag system stripped
- [x] Capture-point scoring stripped
- [x] Attackable towers with turret AI
- [x] Lane creep waves
- [x] Inhibitors and super minions; the core is the win condition
- [x] Neutral jungle camps — aggro, leash, camp-link, League-style reset
- [x] Respawn timers that scale with match length, replacing graveyard pulses
- [x] Recall to base (Hearthstone hijacked)
- [x] Fountain healing
- [x] Cross-faction teams, via mod-cfbg
- [x] Heal, shield and cleanse your own minions; stat buffs rejected as inert
- [x] Last-hit kill credit (CS) for minions and neutrals
- [x] Kill-credit window — a death to a creep or tower still credits a recent attacker
- [x] Contribution-based assists — a fixed-point support chain, replacing proximity
- [x] On-death drops for minions and neutrals
- [x] Player kill drops — granted directly, no corpse
- [x] Match economy — a per-match wallet fed by CS, kills, objectives and a passive tick
- [x] Item shop — four tabs, addon panel, real tooltips, server-pushed usability greying
- [x] Sell items back — only what the match gave you, only from your base
- [x] Shop access — minimap button, keybind, and a base-circle gate on trading
- [x] HUD bar — clock, team score, KDA, creep score, gold
- [x] Revive countdown
- [x] Kill feed — kills, structures, streaks, bosses, match flow
- [x] Gold floats — every earned coin announces itself; passive income and shop payouts stay silent
- [x] Surrender vote — `.surrender` / `.ff`, all-but-one, team-only until it passes
- [x] Per-map content bundles; YAML configs generate the SQL

Next, in order:

- [x] Custom map — Twisted Treeline, map 900 (dressing pass outstanding)
- [x] Standalone battleground ID — type id 12, its own `BattlemasterList.dbc` and `PvpDifficulty.dbc` rows
- [ ] Client-patch bundle — recall tooltip and animation, fountain visuals, sounds, music
- [ ] Lane and neutral mob gold pass
- [ ] Player kill rewards and bounties — values, scaling, assist-gold split
- [ ] Itemization pass
- [ ] Boss steal line — needs per-camp damage attribution and a contested execute
- [ ] Character creation and level-up automation for test characters

## Housekeeping

Code and doc chores that don't change gameplay. Not the roadmap above, and not CLAUDE.md's "Deferred / known-untidy", which tracks live traps an editor must know while changing code.

- [x] Docs and code-comment pass — cut duplication and per-session context cost
- [x] `GetBgTeamId` vs `GetTeamId` audit across `BattlegroundMOBA`
- [x] Split `MobaHUD.lua` into one file per UI over a shared `Core` namespace
- [ ] Remove all creep types being set to beast
- [x] `BG_MOBA_Score` and its Flurry achievement timer removed — EotS achievements don't fire in Twisted Treeline
- [x] Removed leftover `m_BuffChange = true` from the `BattlegroundMOBA` constructor

## Building and running

Standard AzerothCore — see the [installation guide](https://www.azerothcore.org/wiki/installation). Nothing in this fork changes the build. Fork-specific build flags, test setup and config live in `CLAUDE.md`; how to change any of the content is in [`MOBA_GUIDE.md`](MOBA_GUIDE.md).

## License

GNU GPL v2, inherited from AzerothCore. See [LICENSE](../LICENSE).
