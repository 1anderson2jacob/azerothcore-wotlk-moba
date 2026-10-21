#ifndef MOBA_QUEST_DATA_H
#define MOBA_QUEST_DATA_H

#include "Common.h"
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// The bit each sets in `mod_moba_quest_volume`.Feeds. VOLUME_FEEDS in
// apps/moba/gen_quests.py numbers them too; the two must agree.
enum MobaQuestFeed : uint8
{
    MOBA_FEED_DAMAGE_DEALT  = 0,
    MOBA_FEED_HEALING_DONE  = 1,
    MOBA_FEED_DAMAGE_TAKEN  = 2,
    MOBA_FEED_PLAYER_DAMAGE = 3,
    MOBA_FEED_MAX           = 4,
};

struct MobaVolumeCredit
{
    uint32 creditEntry = 0;
    uint32 chunk = 0;   // points per credit
};

// Loads `mod_moba_quest_block`, `mod_moba_quest_credit` and `mod_moba_quest_volume`
// once per process.
class MobaQuestDataStore
{
public:
    static MobaQuestDataStore* instance();

    void LoadIfNeeded();
    std::vector<uint32> const& GetQuests(uint32 mapId) const;   // empty for a map with no quests
    uint32 GetKillCredit(uint32 creatureEntry) const;           // 0 = feeds no quest
    bool ResetsChain(uint32 questId) const;
    std::vector<MobaVolumeCredit> const& GetVolumeCredits(uint32 mapId, MobaQuestFeed feed) const;

private:
    MobaQuestDataStore() = default;

    bool _loaded = false;
    std::unordered_map<uint32, std::vector<uint32>> _questsByMap;
    std::unordered_set<uint32> _chainResets;
    std::unordered_map<uint32, uint32> _killCredits;   // creature entry -> credit entry
    std::unordered_map<uint32, std::array<std::vector<MobaVolumeCredit>, MOBA_FEED_MAX>> _volumeByMap;
};

#define sMobaQuestDataStore MobaQuestDataStore::instance()

#endif
