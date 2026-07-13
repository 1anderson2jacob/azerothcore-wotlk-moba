#ifndef MOBA_CREEP_DATA_H
#define MOBA_CREEP_DATA_H

#include "Common.h"
#include "SharedDefines.h"
#include <unordered_map>
#include <vector>

enum MobaCreepRole : uint8
{
    MOBA_CREEP_ROLE_MELEE  = 0,
    MOBA_CREEP_ROLE_CASTER = 1,
    MOBA_CREEP_ROLE_SIEGE  = 2,
};

struct MobaCreepConfig
{
    uint32 entry = 0;
    uint32 map = 0;
    TeamId team = TEAM_ALLIANCE;
    MobaCreepRole role = MOBA_CREEP_ROLE_MELEE;
    float range = 20.0f;
    uint32 intervalMs = 2000;
    uint32 spellId = 0;
    uint32 pathId = 0;
    uint32 despawnMs = 60000;
};

// Loads data/sql/custom/mod_moba_creeps.sql's `mod_moba_creep_data` table once,
// shared by BattlegroundMOBA (needs the full row set to spawn waves) and
// npc_moba_creep::Reset() (needs its own row, cheaply).
class MobaCreepDataStore
{
public:
    static MobaCreepDataStore* instance();

    void LoadIfNeeded();
    MobaCreepConfig const* GetConfig(uint32 entry) const;
    std::vector<MobaCreepConfig> GetForMap(uint32 mapId) const;

private:
    MobaCreepDataStore() = default;

    bool _loaded = false;
    std::vector<MobaCreepConfig> _configs;
    std::unordered_map<uint32, MobaCreepConfig const*> _byEntry;
};

#define sMobaCreepDataStore MobaCreepDataStore::instance()

#endif
