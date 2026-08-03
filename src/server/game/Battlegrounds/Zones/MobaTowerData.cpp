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
        "TeamGold, LastHitGold FROM mod_moba_tower_data "
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

        _configs.push_back(cfg);
    } while (result->NextRow());

    // Build the entry->pointer lookup only after all rows are loaded: _configs
    // was reserve()'d to the exact final row count above, so no reallocation
    // happens during the push_back loop and these pointers stay stable.
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
