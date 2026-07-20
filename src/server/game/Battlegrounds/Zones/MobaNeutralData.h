#ifndef MOBA_NEUTRAL_DATA_H
#define MOBA_NEUTRAL_DATA_H

#include "Common.h"
#include <unordered_map>
#include <vector>

struct MobaNeutralConfig
{
    uint32 entry = 0;
    uint32 map = 0;
    float aggroRange = 0.0f;        // 0 = pull-on-hit (AI goes REACT_DEFENSIVE)
    float leashRange = 20.0f;       // hard evade cap from the camp anchor; 0 = engine leash only
};

struct MobaNeutralMember
{
    uint32 entry = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float o = 0.0f;
};

struct MobaNeutralCamp
{
    uint32 campId = 0;
    uint32 map = 0;
    uint32 initialSpawnMs = 0;
    uint32 respawnMs = 0;
    std::vector<MobaNeutralMember> members;
};

// Loads data/sql/custom/mod_moba_neutrals.sql's three tables once, shared by
// BattlegroundMOBA (camps + members, to spawn/respawn) and npc_moba_neutral
// (its own entry's behavior row, cheaply). Deliberately separate from
// MobaCreepDataStore: "is this a lane creep?" checks -- the heal/buff spell
// gate above all -- must never match hostile neutrals.
class MobaNeutralDataStore
{
public:
    static MobaNeutralDataStore* instance();

    void LoadIfNeeded();
    MobaNeutralConfig const* GetConfig(uint32 entry) const;
    std::vector<MobaNeutralCamp> GetCampsForMap(uint32 mapId) const;

private:
    MobaNeutralDataStore() = default;

    bool _loaded = false;
    std::vector<MobaNeutralConfig> _configs;
    std::unordered_map<uint32, MobaNeutralConfig const*> _byEntry;
    std::vector<MobaNeutralCamp> _camps;
};

#define sMobaNeutralDataStore MobaNeutralDataStore::instance()

#endif
