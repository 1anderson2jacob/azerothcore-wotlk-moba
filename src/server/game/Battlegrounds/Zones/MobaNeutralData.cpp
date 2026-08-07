#include "MobaNeutralData.h"
#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "Field.h"
#include "Log.h"
#include <algorithm>

MobaNeutralDataStore* MobaNeutralDataStore::instance()
{
    static MobaNeutralDataStore instance;
    return &instance;
}

void MobaNeutralDataStore::LoadIfNeeded()
{
    if (_loaded)
        return;

    _loaded = true;

    QueryResult result = WorldDatabase.Query(
        "SELECT CreatureEntry, Map, AggroRange, LeashRange "
        "FROM mod_moba_neutral_data ORDER BY CreatureEntry");

    if (!result)
    {
        LOG_ERROR("sql.sql", "MobaNeutralDataStore: table `mod_moba_neutral_data` is empty or missing.");
        return;
    }

    _configs.reserve(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();

        MobaNeutralConfig cfg;
        cfg.entry      = fields[0].Get<uint32>();
        cfg.map        = fields[1].Get<uint32>();
        cfg.aggroRange = fields[2].Get<float>();
        cfg.leashRange = fields[3].Get<float>();

        _configs.push_back(cfg);
    } while (result->NextRow());

    // Built only after all rows are loaded: _configs was reserve()'d to the
    // exact final row count above, so no reallocation happens during the
    // push_back loop and these pointers stay stable.
    for (MobaNeutralConfig const& cfg : _configs)
        _byEntry[cfg.entry] = &cfg;

    QueryResult camps = WorldDatabase.Query(
        "SELECT Map, CampId, Tier, InitialSpawnMs, RespawnMs, SpawnWarnMs "
        "FROM mod_moba_neutral_camps ORDER BY Map, CampId");

    if (!camps)
    {
        LOG_ERROR("sql.sql", "MobaNeutralDataStore: table `mod_moba_neutral_camps` is empty or missing.");
        return;
    }

    do
    {
        Field* fields = camps->Fetch();

        MobaNeutralCamp camp;
        camp.map            = fields[0].Get<uint32>();
        camp.campId         = fields[1].Get<uint32>();
        camp.tier           = fields[2].Get<uint8>();
        camp.initialSpawnMs = fields[3].Get<uint32>();
        camp.respawnMs      = fields[4].Get<uint32>();
        camp.spawnWarnMs  = fields[5].Get<uint32>();

        _camps.push_back(camp);
    } while (camps->NextRow());

    QueryResult members = WorldDatabase.Query(
        "SELECT Map, CampId, CreatureEntry, X, Y, Z, O "
        "FROM mod_moba_neutral_members ORDER BY Map, CampId, Idx");

    if (!members)
    {
        LOG_ERROR("sql.sql", "MobaNeutralDataStore: table `mod_moba_neutral_members` is empty or missing.");
        return;
    }

    do
    {
        Field* fields = members->Fetch();

        uint32 map    = fields[0].Get<uint32>();
        uint32 campId = fields[1].Get<uint32>();

        MobaNeutralMember member;
        member.entry = fields[2].Get<uint32>();
        member.x     = fields[3].Get<float>();
        member.y     = fields[4].Get<float>();
        member.z     = fields[5].Get<float>();
        member.o     = fields[6].Get<float>();

        auto camp = std::find_if(_camps.begin(), _camps.end(), [&](MobaNeutralCamp const& c)
        {
            return c.map == map && c.campId == campId;
        });
        if (camp == _camps.end())
        {
            LOG_ERROR("sql.sql", "MobaNeutralDataStore: member entry {} references unknown camp {} on map {}.",
                member.entry, campId, map);
            continue;
        }
        camp->members.push_back(member);
    } while (members->NextRow());
}

MobaNeutralConfig const* MobaNeutralDataStore::GetConfig(uint32 entry) const
{
    auto itr = _byEntry.find(entry);
    return itr != _byEntry.end() ? itr->second : nullptr;
}

std::vector<MobaNeutralCamp> MobaNeutralDataStore::GetCampsForMap(uint32 mapId) const
{
    std::vector<MobaNeutralCamp> result;
    for (MobaNeutralCamp const& camp : _camps)
        if (camp.map == mapId)
            result.push_back(camp);
    return result;
}
