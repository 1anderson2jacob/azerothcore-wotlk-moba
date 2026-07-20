#include "MobaDropData.h"
#include "DatabaseEnv.h"
#include "Field.h"
#include "Log.h"
#include "QueryResult.h"

MobaDropDataStore* MobaDropDataStore::instance()
{
    static MobaDropDataStore instance;
    return &instance;
}

void MobaDropDataStore::LoadIfNeeded()
{
    if (_loaded)
        return;

    _loaded = true;

    // Two tables with one schema so each generated SQL file stays fully
    // self-contained; they merge here. Unlike the other Moba* stores, an
    // empty result is NOT an error -- a map may simply define no drops --
    // and a genuinely missing table already screams in the DB layer's log.
    uint32 count = 0;
    auto load = [&](char const* sql)
    {
        QueryResult result = WorldDatabase.Query(sql);
        if (!result)
            return;

        do
        {
            Field* fields = result->Fetch();

            MobaDropInfo drop;
            uint32 entry    = fields[0].Get<uint32>();
            drop.type       = MobaDropType(fields[1].Get<uint8>());
            drop.spell      = fields[2].Get<uint32>();
            drop.durationMs = fields[3].Get<uint32>();
            drop.copper     = fields[4].Get<uint32>();
            drop.chance     = fields[5].Get<float>();

            _byEntry[entry].push_back(drop);
            ++count;
        } while (result->NextRow());
    };

    load("SELECT CreatureEntry, Type, Spell, DurationMs, Copper, Chance "
         "FROM mod_moba_creep_drops ORDER BY CreatureEntry, Idx");
    load("SELECT CreatureEntry, Type, Spell, DurationMs, Copper, Chance "
         "FROM mod_moba_neutral_drops ORDER BY CreatureEntry, Idx");

    LOG_INFO("server.loading", "MobaDropDataStore: loaded {} on-death drop rows.", count);
}

std::vector<MobaDropInfo> const* MobaDropDataStore::GetDrops(uint32 entry) const
{
    auto itr = _byEntry.find(entry);
    return itr != _byEntry.end() ? &itr->second : nullptr;
}
