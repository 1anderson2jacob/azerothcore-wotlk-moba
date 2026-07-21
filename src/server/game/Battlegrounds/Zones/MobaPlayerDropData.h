#ifndef MOBA_PLAYER_DROP_DATA_H
#define MOBA_PLAYER_DROP_DATA_H

#include "Common.h"
#include <unordered_map>
#include <vector>

enum MobaPlayerDropType : uint8
{
    MOBA_PLAYER_DROP_BUFF = 0,
    MOBA_PLAYER_DROP_GOLD = 1,
    MOBA_PLAYER_DROP_ITEM = 2,
};

struct MobaPlayerDropInfo
{
    MobaPlayerDropType type = MOBA_PLAYER_DROP_BUFF;
    uint32 spell = 0;
    uint32 durationMs = 0;
    uint32 copper = 0;
    uint32 item = 0;
    uint32 count = 1;
    float chance = 100.0f;   // percent, rolled per kill
};

// Loads mod_moba_player_drops once per process, keyed by map (no per-mob home
// for player-kill rewards). Consumed by BattlegroundMOBA::GrantPlayerKillDrops,
// which grants directly to the killer -- no corpse, no native loot -- so unlike
// MobaDropDataStore's buff/gold-only split, "item" is a rolled-and-delivered
// type here too (AddItem).
class MobaPlayerDropDataStore
{
public:
    static MobaPlayerDropDataStore* instance();

    void LoadIfNeeded();
    std::vector<MobaPlayerDropInfo> const* GetDrops(uint32 mapId) const;

private:
    MobaPlayerDropDataStore() = default;

    bool _loaded = false;
    std::unordered_map<uint32, std::vector<MobaPlayerDropInfo>> _byMap;
};

#define sMobaPlayerDropDataStore MobaPlayerDropDataStore::instance()

#endif
