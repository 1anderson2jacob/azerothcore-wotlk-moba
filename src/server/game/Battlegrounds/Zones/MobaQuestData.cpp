#include "MobaQuestData.h"
#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "Field.h"
#include "Log.h"

MobaQuestDataStore* MobaQuestDataStore::instance()
{
    static MobaQuestDataStore instance;
    return &instance;
}

void MobaQuestDataStore::LoadIfNeeded()
{
    if (_loaded)
        return;

    _loaded = true;

    QueryResult result = WorldDatabase.Query("SELECT QuestId, Map, ResetsChain FROM mod_moba_quest_block");
    if (!result)
    {
        LOG_ERROR("sql.sql", "MobaQuestDataStore: table `mod_moba_quest_block` is empty or missing.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();
        uint32 questId = fields[0].Get<uint32>();
        _questsByMap[fields[1].Get<uint32>()].push_back(questId);
        if (fields[2].Get<uint8>())
            _chainResets.insert(questId);
    } while (result->NextRow());

    result = WorldDatabase.Query("SELECT CreatureEntry, CreditEntry FROM mod_moba_quest_credit");
    if (result)   // empty when no quest has a last_hit objective
    {
        do
        {
            Field* fields = result->Fetch();
            _killCredits[fields[0].Get<uint32>()] = fields[1].Get<uint32>();
        } while (result->NextRow());
    }

    result = WorldDatabase.Query("SELECT CreditEntry, Map, Feeds, Chunk FROM mod_moba_quest_volume");
    if (!result)
        return;   // no quest has a volume objective

    do
    {
        Field* fields = result->Fetch();
        MobaVolumeCredit credit;
        credit.creditEntry = fields[0].Get<uint32>();
        uint32 const mapId = fields[1].Get<uint32>();
        uint8 const feeds = fields[2].Get<uint8>();
        credit.chunk = fields[3].Get<uint32>();

        // Crediting subtracts a chunk per credit, so a zero would never stop.
        if (!credit.chunk)
        {
            LOG_ERROR("sql.sql", "MobaQuestDataStore: credit entry {} in `mod_moba_quest_volume` has Chunk 0, skipped.", credit.creditEntry);
            continue;
        }

        for (uint8 feed = 0; feed < MOBA_FEED_MAX; ++feed)
            if (feeds & (1 << feed))
                _volumeByMap[mapId][feed].push_back(credit);
    } while (result->NextRow());
}

std::vector<uint32> const& MobaQuestDataStore::GetQuests(uint32 mapId) const
{
    static std::vector<uint32> const none;
    auto itr = _questsByMap.find(mapId);
    return itr != _questsByMap.end() ? itr->second : none;
}

uint32 MobaQuestDataStore::GetKillCredit(uint32 creatureEntry) const
{
    auto itr = _killCredits.find(creatureEntry);
    return itr != _killCredits.end() ? itr->second : 0;
}

bool MobaQuestDataStore::ResetsChain(uint32 questId) const
{
    return _chainResets.count(questId) != 0;
}

std::vector<MobaVolumeCredit> const& MobaQuestDataStore::GetVolumeCredits(uint32 mapId, MobaQuestFeed feed) const
{
    static std::vector<MobaVolumeCredit> const none;
    auto itr = _volumeByMap.find(mapId);
    return itr != _volumeByMap.end() ? itr->second[feed] : none;
}
