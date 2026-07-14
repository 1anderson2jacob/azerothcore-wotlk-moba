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

#ifndef MOBA_RESPAWN_DATA_H
#define MOBA_RESPAWN_DATA_H

#include "Common.h"
#include <unordered_map>

// One row per MOBA map: how long a released player waits before respawning.
// The respawn LOCATION is the battleground's team start position, not stored
// here -- it already lives in game_graveyard / battleground_template and is
// shared with player spawn-in (see mod_moba_respawn.sql).
struct MobaRespawnConfig
{
    uint32 map = 0;
    uint32 baseMs = 10000;
    uint32 perMinMs = 1500;
    uint32 capMs = 60000;
};

// Loads data/sql/custom/mod_moba_respawn.sql's `mod_moba_respawn`
// table once, keyed by map id. Read by BattlegroundMOBA::StartRespawnTimer().
class MobaRespawnDataStore
{
public:
    static MobaRespawnDataStore* instance();

    void LoadIfNeeded();
    MobaRespawnConfig const* GetConfig(uint32 mapId) const;

private:
    MobaRespawnDataStore() = default;

    bool _loaded = false;
    std::unordered_map<uint32, MobaRespawnConfig> _byMap;
};

#define sMobaRespawnDataStore MobaRespawnDataStore::instance()

#endif
