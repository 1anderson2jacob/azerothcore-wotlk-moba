#include "MobaTowerData.h"
#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "Field.h"
#include "Log.h"

MobaTowerDataStore* MobaTowerDataStore::instance()
{
    static MobaTowerDataStore instance;
    return &instance;
}

void MobaTowerDataStore::LoadIfNeeded()
{
    if (_loaded)
        return;

    _loaded = true;

    QueryResult result = WorldDatabase.Query(
        "SELECT CreatureEntry, Team, Tier, GuardedByEntry, PosX, PosY, PosZ, Orientation, "
        "AttackRange, AttackIntervalMs, AttackSpellId, Map, Kind, RespawnMs, "
        "TeamGold, LastHitGold, Lane FROM mod_moba_tower_data "
        "ORDER BY Team, Tier, CreatureEntry");

    if (!result)
    {
        LOG_ERROR("sql.sql", "MobaTowerDataStore: table `mod_moba_tower_data` is empty or missing.");
        return;
    }

    _configs.reserve(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();

        MobaTowerConfig cfg;
        cfg.entry          = fields[0].Get<uint32>();
        cfg.team           = static_cast<TeamId>(fields[1].Get<uint8>());
        cfg.tier           = fields[2].Get<uint8>();
        cfg.guardedByEntry = fields[3].Get<uint32>();
        cfg.x              = fields[4].Get<float>();
        cfg.y              = fields[5].Get<float>();
        cfg.z              = fields[6].Get<float>();
        cfg.o              = fields[7].Get<float>();
        cfg.range          = fields[8].Get<float>();
        cfg.intervalMs     = fields[9].Get<uint32>();
        cfg.spellId        = fields[10].Get<uint32>();
        cfg.map            = fields[11].Get<uint32>();
        cfg.kind           = fields[12].Get<uint8>();
        cfg.respawnMs         = fields[13].Get<uint32>();
        cfg.teamGoldCopper    = fields[14].Get<uint32>();
        cfg.lastHitGoldCopper = fields[15].Get<uint32>();
        cfg.lane              = fields[16].Get<uint8>();

        _configs.push_back(cfg);
    } while (result->NextRow());

    // Built after the loop: the reserve() above is what keeps these pointers stable.
    for (MobaTowerConfig const& cfg : _configs)
        _byEntry[cfg.entry] = &cfg;
}

MobaTowerConfig const* MobaTowerDataStore::GetConfig(uint32 entry) const
{
    auto itr = _byEntry.find(entry);
    return itr != _byEntry.end() ? itr->second : nullptr;
}

std::vector<MobaTowerConfig> MobaTowerDataStore::GetForMap(uint32 mapId) const
{
    std::vector<MobaTowerConfig> result;
    for (MobaTowerConfig const& cfg : _configs)
        if (cfg.map == mapId)
            result.push_back(cfg);
    return result;
}
