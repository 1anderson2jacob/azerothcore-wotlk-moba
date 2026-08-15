#ifndef MOBA_TOWER_DATA_H
#define MOBA_TOWER_DATA_H

#include "Common.h"
#include "SharedDefines.h"
#include <unordered_map>
#include <vector>

// Towers attack; inhibitors and cores are passive (npc_moba_tower skips the attack tick
// for them). What differs is what happens on death -- see OnTowerDestroyed.
enum MobaStructureKind : uint8
{
    MOBA_STRUCTURE_TOWER     = 0,
    MOBA_STRUCTURE_INHIBITOR = 1,
    MOBA_STRUCTURE_CORE      = 2,
};

// Read by the push logic, not just the kill feed: a super minion spawns only while the
// enemy inhibitor ON ITS OWN LANE is down. Mirrored by LANE_IDS in
// apps/moba/gen_creep_roster.py and LANE_NAMES in client/addons/MobaHUD/Feed.lua; all
// three must agree. MOBA_LANE_MAX is a sentinel that sizes arrays, not a lane, so it has
// no counterpart in either mirror.
enum MobaLane : uint8
{
    MOBA_LANE_NONE = 0,
    MOBA_LANE_TOP  = 1,
    MOBA_LANE_MID  = 2,
    MOBA_LANE_BOT  = 3,
    MOBA_LANE_MAX  = 4,
};

struct MobaTowerConfig
{
    uint32 entry = 0;
    uint32 map = 0;
    TeamId team = TEAM_ALLIANCE;
    uint8 lane = MOBA_LANE_NONE;
    uint8 tier = 0;
    uint32 guardedByEntry = 0;
    uint8 kind = MOBA_STRUCTURE_TOWER;
    uint32 respawnMs = 0;    // inhibitor respawn delay; 0 = never respawns (towers, cores)
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float o = 0.0f;
    float range = 40.0f;
    uint32 intervalMs = 1500;
    uint32 spellId = 0;
    uint32 teamGoldCopper = 0;      // paid to EVERY player on the destroying team
    uint32 lastHitGoldCopper = 0;   // paid to the killing-blow player only; 0 = none
};

// Loads `mod_moba_tower_data` once per process. Two accessors because the battleground
// needs the full row set and a tower AI needs only its own row.
class MobaTowerDataStore
{
public:
    static MobaTowerDataStore* instance();

    void LoadIfNeeded();
    MobaTowerConfig const* GetConfig(uint32 entry) const;
    std::vector<MobaTowerConfig> GetForMap(uint32 mapId) const;

private:
    MobaTowerDataStore() = default;

    bool _loaded = false;
    std::vector<MobaTowerConfig> _configs;
    std::unordered_map<uint32, MobaTowerConfig const*> _byEntry;
};

#define sMobaTowerDataStore MobaTowerDataStore::instance()

#endif
