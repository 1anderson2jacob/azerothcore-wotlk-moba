#ifndef MOBA_QUEST_DATA_H
#define MOBA_QUEST_DATA_H

#include "Common.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Loads `mod_moba_quest_block` and `mod_moba_quest_credit` once per process.
class MobaQuestDataStore
{
public:
    static MobaQuestDataStore* instance();

    void LoadIfNeeded();
    std::vector<uint32> const& GetQuests(uint32 mapId) const;   // empty for a map with no quests
    uint32 GetKillCredit(uint32 creatureEntry) const;           // 0 = feeds no quest
    bool ResetsChain(uint32 questId) const;

private:
    MobaQuestDataStore() = default;

    bool _loaded = false;
    std::unordered_map<uint32, std::vector<uint32>> _questsByMap;
    std::unordered_set<uint32> _chainResets;
    std::unordered_map<uint32, uint32> _killCredits;   // creature entry -> credit entry
};

#define sMobaQuestDataStore MobaQuestDataStore::instance()

#endif
