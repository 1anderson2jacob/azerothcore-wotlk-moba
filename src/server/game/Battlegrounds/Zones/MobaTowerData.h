/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MOBA_TOWER_DATA_H
#define MOBA_TOWER_DATA_H

#include "Common.h"
#include "SharedDefines.h"
#include <unordered_map>
#include <vector>

// A structure's role in the MOBA push. Towers attack; inhibitors and cores are
// passive (npc_moba_tower skips the attack tick for them). What differs is what
// happens on death -- see BattlegroundMOBA::OnTowerDestroyed:
//   TOWER     -- just unlocks whatever it guarded.
//   INHIBITOR -- unlocks + grants the enemy super minions + respawns itself.
//   CORE      -- the base; its destruction ends the match.
enum MobaStructureKind : uint8
{
    MOBA_STRUCTURE_TOWER     = 0,
    MOBA_STRUCTURE_INHIBITOR = 1,
    MOBA_STRUCTURE_CORE      = 2,
};

// Which lane a structure sits on. Kill-feed wording only -- nothing in the push
// logic reads it. NONE is what a core carries: a base belongs to no lane, and
// the HUD renders no lane word for 0, so that needs no special casing.
// Mirrored by LANE_IDS in apps/moba/gen_tower_data.py and LANE_NAMES in
// client/addons/MobaHUD/Feed.lua -- all three must agree.
enum MobaLane : uint8
{
    MOBA_LANE_NONE = 0,
    MOBA_LANE_TOP  = 1,
    MOBA_LANE_MID  = 2,
    MOBA_LANE_BOT  = 3,
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

// Loads data/sql/custom/mod_moba_towers.sql's `mod_moba_tower_data` table once,
// shared by BattlegroundMOBA::SetupBattleground() (needs the full row set to
// spawn towers) and npc_moba_tower::Reset() (needs its own row, cheaply).
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
