#ifndef MOBA_DROP_DATA_H
#define MOBA_DROP_DATA_H

#include "Common.h"
#include <unordered_map>
#include <vector>

enum MobaDropType : uint8
{
    MOBA_DROP_BUFF = 0,
    MOBA_DROP_GOLD = 1,
};

struct MobaDropInfo
{
    MobaDropType type = MOBA_DROP_BUFF;
    uint32 spell = 0;        // buff: aura granted to the killing-blow player
    uint32 durationMs = 0;   // buff: 0 = the spell's own duration
    uint32 copper = 0;       // gold: amount injected into the corpse loot
    float chance = 100.0f;   // percent, rolled per kill
};

// Loads mod_moba_creep_drops + mod_moba_neutral_drops once per process,
// consumed by BattlegroundMOBA::GrantDeathDrops. Only buff/gold rows live
// here -- "item" drops are native creature_loot_template rows the engine
// rolls itself. One store for both minion kinds: by the time a drop is
// granted, which AI died no longer matters.
class MobaDropDataStore
{
public:
    static MobaDropDataStore* instance();

    void LoadIfNeeded();
    std::vector<MobaDropInfo> const* GetDrops(uint32 entry) const;

private:
    MobaDropDataStore() = default;

    bool _loaded = false;
    std::unordered_map<uint32, std::vector<MobaDropInfo>> _byEntry;
};

#define sMobaDropDataStore MobaDropDataStore::instance()

#endif
