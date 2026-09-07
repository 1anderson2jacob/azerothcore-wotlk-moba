#ifndef MOBA_BASE_DATA_H
#define MOBA_BASE_DATA_H

#include "Common.h"
#include <unordered_map>

// One row per MOBA map: every per-map tunable the battleground reads.
// The base LOCATION lives elsewhere -- game_graveyard / battleground_template,
// written from the same config (gen_base.py), read via GetTeamStartPosition and
// GetClosestGraveyard.
// battleground_template.StartMaxDist must stay 0: non-zero arms the core's prep-phase
// leash (Battleground::_CheckSafePositions), which yanks players back to spawn every
// 9s. The spawn dome does the holding-in; FountainRadius carries the radius.
struct MobaBaseConfig
{
    uint32 map = 0;
    uint32 respawnBaseMs = 10000;
    uint32 respawnPerMinMs = 1500;
    uint32 respawnCapMs = 60000;
    uint32 recallCastMs = 0;
    uint32 recallEmpoweredCastMs = 0;
    uint32 fountainTickMs = 0;      // 0 = fountain healing off
    uint32 fountainHpPct = 0;
    uint32 fountainManaPct = 0;
    float fountainRadius = 0.0f;    // spawn-dome radius in yards = heal zone = shop range; 0 = off
    uint32 killCreditWindowMs = 15000;  // ms after enemy-player damage/debuff a death still credits them; 0 = off
    uint32 assistWindowMs = 10000;          // ms before a death in which contribution earns an assist; 0 = off
    uint32 assistBuffMaxDurationMs = 60000; // max buff/shield duration (ms) counting as a fight buff for assists
    // Sized from the same spawn.radius as FountainRadius. 0 = battleground not created.
    uint32 domeEntryAlliance = 0;
    uint32 domeEntryHorde = 0;
    uint32 startingGold = 0;    // copper granted once on entry; 0 = none
    uint32 passiveTickMs = 0;   // passive income cadence (ms); 0 = passive income off
    uint32 passiveCopper = 0;   // copper per tick, per player
    // Streak bounties, paid on top of the flat per-kill reward in mod_moba_player_drops.
    uint32 firstBloodGold = 0;      // bonus copper for the match's first player kill; 0 = off
    uint32 shutdownPerStreak = 0;   // bounty copper per kill on the victim's streak; 0 = no bounty
    uint32 shutdownCapGold = 0;     // ceiling on that bounty; 0 = uncapped
    uint32 multiKillWindowMs = 10000; // a kill this soon after the last extends the multi-kill; 0 = multi-kills off
    // Doubles as the shutdown threshold: a victim is "on a spree" exactly when the feed
    // has already said so. 0 = sprees and shutdowns both off.
    uint32 spreeMin = 3;
    uint32 aceMinTeam = 2;          // smallest wiped team that counts as an ace; 0 = ace off
    uint32 surrenderMinMs = 0;          // earliest a surrender vote may start, from doors open; 0 = no gate
    uint32 surrenderVoteMs = 15000;     // how long a vote stays open before silence fails it
    uint32 surrenderCooldownMs = 60000; // after a failed vote, before that team may start another
    uint32 graveyardAlliance = 0;       // game_graveyard id the team releases to
    uint32 graveyardHorde = 0;
};

// Loads `mod_moba_base` once per process, keyed by map id.
class MobaBaseDataStore
{
public:
    static MobaBaseDataStore* instance();

    void LoadIfNeeded();
    MobaBaseConfig const* GetConfig(uint32 mapId) const;

private:
    MobaBaseDataStore() = default;

    bool _loaded = false;
    std::unordered_map<uint32, MobaBaseConfig> _byMap;
};

#define sMobaBaseDataStore MobaBaseDataStore::instance()

#endif
