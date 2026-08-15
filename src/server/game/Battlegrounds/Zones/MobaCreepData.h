#ifndef MOBA_CREEP_DATA_H
#define MOBA_CREEP_DATA_H

#include "Common.h"
#include "SharedDefines.h"
#include "MobaTowerData.h"   // MobaLane -- a creep's lane is matched against an inhibitor's
#include <unordered_map>
#include <vector>

enum MobaCreepRole : uint8
{
    MOBA_CREEP_ROLE_MELEE  = 0,
    MOBA_CREEP_ROLE_CASTER = 1,
    MOBA_CREEP_ROLE_SIEGE  = 2,
    MOBA_CREEP_ROLE_SUPER  = 3,
    MOBA_CREEP_ROLE_MAX    = 4,
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
    uint8 lane = MOBA_LANE_NONE;
    uint32 despawnMs = 60000;
};

// Loads `mod_moba_creep_data` once per process. Two accessors for the same reason as
// MobaTowerDataStore: full row set for the battleground, one row for a creep AI.
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
