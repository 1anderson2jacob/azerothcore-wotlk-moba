#ifndef MOBA_DROP_DATA_H
#define MOBA_DROP_DATA_H

#include "Common.h"
#include <unordered_map>
#include <vector>

enum MobaDropType : uint8
{
    MOBA_DROP_BUFF = 0,
    MOBA_DROP_GOLD = 1,
    // The item itself comes from creature_loot_template, never from here: a
    // type-2 row exists ONLY to price the drop for the shop's sell panel, and
    // is filtered out before _byEntry.
    MOBA_DROP_ITEM = 2,
    // Paid to the killer's whole TEAM, flat per player, with no corpse -- the
    // objective payout a boss camp wants. Deliberately a drop TYPE rather than a
    // per-creature "boss" flag, so one creature can carry both this and a plain
    // gold row: a team share for everyone, corpse gold for the last hitter.
    MOBA_DROP_TEAM_GOLD = 3,
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
    // What ONE UNIT of a looted item sells back for. False = nothing in the
    // match drops it at a price, so npc_moba_store refuses the sale.
    bool GetItemSellValue(uint32 itemEntry, uint32& out) const;

private:
    MobaDropDataStore() = default;

    bool _loaded = false;
    std::unordered_map<uint32, std::vector<MobaDropInfo>> _byEntry;
    // Flattened across both tables: sell price is a property of the ITEM, not of
    // whichever creature happened to drop it.
    std::unordered_map<uint32, uint32> _sellByItem;
};

#define sMobaDropDataStore MobaDropDataStore::instance()

#endif
