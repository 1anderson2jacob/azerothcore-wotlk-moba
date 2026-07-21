#include "MobaPlayerDropData.h"
#include "DatabaseEnv.h"
#include "Field.h"
#include "Log.h"
#include "QueryResult.h"

MobaPlayerDropDataStore* MobaPlayerDropDataStore::instance()
{
    static MobaPlayerDropDataStore instance;
    return &instance;
}

void MobaPlayerDropDataStore::LoadIfNeeded()
{
    if (_loaded)
        return;

    _loaded = true;

    // An empty result is NOT an error -- a map may define no player drops -- and
    // a genuinely missing table already screams in the DB layer's log.
    QueryResult result = WorldDatabase.Query(
        "SELECT Map, Type, Spell, DurationMs, Copper, Item, Count, Chance "
        "FROM mod_moba_player_drops ORDER BY Map, Idx");
    if (!result)
    {
        LOG_INFO("server.loading", "MobaPlayerDropDataStore: loaded 0 player kill drop rows.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        MobaPlayerDropInfo drop;
        uint32 mapId    = fields[0].Get<uint32>();
        drop.type       = MobaPlayerDropType(fields[1].Get<uint8>());
        drop.spell      = fields[2].Get<uint32>();
        drop.durationMs = fields[3].Get<uint32>();
        drop.copper     = fields[4].Get<uint32>();
        drop.item       = fields[5].Get<uint32>();
        drop.count      = fields[6].Get<uint32>();
        drop.chance     = fields[7].Get<float>();

        _byMap[mapId].push_back(drop);
        ++count;
    } while (result->NextRow());

    LOG_INFO("server.loading", "MobaPlayerDropDataStore: loaded {} player kill drop rows.", count);
}

std::vector<MobaPlayerDropInfo> const* MobaPlayerDropDataStore::GetDrops(uint32 mapId) const
{
    auto itr = _byMap.find(mapId);
    return itr != _byMap.end() ? &itr->second : nullptr;
}
