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

struct MobaTowerConfig
{
    uint32 entry = 0;
    uint32 map = 0;
    TeamId team = TEAM_ALLIANCE;
    uint8 tier = 0;
    uint32 guardedByEntry = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float o = 0.0f;
    float range = 40.0f;
    uint32 intervalMs = 1500;
    uint32 spellId = 0;
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
