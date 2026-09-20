#include "MobaDropData.h"
#include "MobaPlayerDropData.h"
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

    auto notePrice = [&](char const* ownerKind, uint32 owner, uint32 itemEntry, uint32 sell)
    {
        auto itr = _sellByItem.find(itemEntry);
        if (itr == _sellByItem.end())
            _sellByItem[itemEntry] = sell;
        else if (itr->second != sell)
            LOG_ERROR("sql.sql", "MobaDropDataStore: {} {} prices item {} at {}, "
                      "but another drop prices it at {}; keeping {}.",
                      ownerKind, owner, itemEntry, sell, itr->second, itr->second);
    };

    // Two tables with one schema, so each generated SQL file stays self-contained. An
    // empty result is NOT an error -- a map may define no drops.
    uint32 count = 0;
    auto load = [&](char const* sql)
    {
        QueryResult result = WorldDatabase.Query(sql);
        if (!result)
            return;

        do
        {
            Field* fields = result->Fetch();

            uint32 entry      = fields[0].Get<uint32>();
            MobaDropType type = MobaDropType(fields[1].Get<uint8>());

            // Grants nothing at the killing blow, so GrantDeathDrops must never see it.
            if (type == MOBA_DROP_ITEM)
            {
                notePrice("creature", entry, fields[6].Get<uint32>(), fields[7].Get<uint32>());
                continue;
            }

            MobaDropInfo drop;
            drop.type       = type;
            drop.spell      = fields[2].Get<uint32>();
            drop.durationMs = fields[3].Get<uint32>();
            drop.copper     = fields[4].Get<uint32>();
            drop.chance     = fields[5].Get<float>();

            _byEntry[entry].push_back(drop);
            ++count;
        } while (result->NextRow());
    };

    load("SELECT CreatureEntry, Type, Spell, DurationMs, Copper, Chance, Item, Sell "
         "FROM mod_moba_creep_drops ORDER BY CreatureEntry, Idx");
    load("SELECT CreatureEntry, Type, Spell, DurationMs, Copper, Chance, Item, Sell "
         "FROM mod_moba_neutral_drops ORDER BY CreatureEntry, Idx");

    // A player-kill drop row is Map-keyed and IS the grant, so MobaPlayerDropDataStore
    // owns it and only the price is read here. Filtered in C++ rather than SQL so the
    // type constant is that table's own enum, not this file's identically-valued one.
    if (QueryResult result = WorldDatabase.Query(
            "SELECT Map, Type, Item, Sell FROM mod_moba_player_drops ORDER BY Map, Idx"))
    {
        do
        {
            Field* fields = result->Fetch();
            if (fields[1].Get<uint8>() != MOBA_PLAYER_DROP_ITEM)
                continue;

            notePrice("map", fields[0].Get<uint32>(), fields[2].Get<uint32>(),
                      fields[3].Get<uint32>());
        } while (result->NextRow());
    }

    LOG_INFO("server.loading", "MobaDropDataStore: loaded {} on-death drop rows, {} sell price(s).",
             count, _sellByItem.size());
}

std::vector<MobaDropInfo> const* MobaDropDataStore::GetDrops(uint32 entry) const
{
    auto itr = _byEntry.find(entry);
    return itr != _byEntry.end() ? &itr->second : nullptr;
}

bool MobaDropDataStore::GetItemSellValue(uint32 itemEntry, uint32& out) const
{
    auto itr = _sellByItem.find(itemEntry);
    if (itr == _sellByItem.end())
        return false;

    out = itr->second;
    return true;
}
