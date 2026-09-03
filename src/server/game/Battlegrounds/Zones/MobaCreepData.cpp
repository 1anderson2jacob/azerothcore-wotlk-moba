#include "MobaCreepData.h"
#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "Field.h"
#include "Log.h"

MobaCreepDataStore* MobaCreepDataStore::instance()
{
    static MobaCreepDataStore instance;
    return &instance;
}

void MobaCreepDataStore::LoadIfNeeded()
{
    if (_loaded)
        return;

    _loaded = true;

    QueryResult result = WorldDatabase.Query(
        "SELECT CreatureEntry, Team, Role, AttackRange, AttackIntervalMs, AttackSpellId, WaypointPathId, DespawnMs, Map, Lane, ReferencePathId "
        "FROM mod_moba_creep_data ORDER BY Team, Role, CreatureEntry");

    if (!result)
    {
        LOG_ERROR("sql.sql", "MobaCreepDataStore: table `mod_moba_creep_data` is empty or missing.");
        return;
    }

    _configs.reserve(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();

        MobaCreepConfig cfg;
        cfg.entry      = fields[0].Get<uint32>();
        cfg.team       = static_cast<TeamId>(fields[1].Get<uint8>());
        cfg.role       = static_cast<MobaCreepRole>(fields[2].Get<uint8>());
        cfg.range      = fields[3].Get<float>();
        cfg.intervalMs = fields[4].Get<uint32>();
        cfg.spellId    = fields[5].Get<uint32>();
        cfg.pathId     = fields[6].Get<uint32>();
        cfg.despawnMs  = fields[7].Get<uint32>();
        cfg.map        = fields[8].Get<uint32>();
        cfg.lane       = fields[9].Get<uint8>();
        cfg.refPathId  = fields[10].Get<uint32>();

        _configs.push_back(cfg);
    } while (result->NextRow());

    // Built after the loop: the reserve() above is what keeps these pointers stable.
    for (MobaCreepConfig const& cfg : _configs)
        _byEntry[cfg.entry] = &cfg;
}

MobaCreepConfig const* MobaCreepDataStore::GetConfig(uint32 entry) const
{
    auto itr = _byEntry.find(entry);
    return itr != _byEntry.end() ? itr->second : nullptr;
}

std::vector<MobaCreepConfig> MobaCreepDataStore::GetForMap(uint32 mapId) const
{
    std::vector<MobaCreepConfig> result;
    for (MobaCreepConfig const& cfg : _configs)
        if (cfg.map == mapId)
            result.push_back(cfg);
    return result;
}
