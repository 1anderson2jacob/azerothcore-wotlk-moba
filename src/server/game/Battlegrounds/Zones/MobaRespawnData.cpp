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

#include "MobaRespawnData.h"
#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "Field.h"
#include "Log.h"

MobaRespawnDataStore* MobaRespawnDataStore::instance()
{
    static MobaRespawnDataStore instance;
    return &instance;
}

void MobaRespawnDataStore::LoadIfNeeded()
{
    if (_loaded)
        return;

    _loaded = true;

    QueryResult result = WorldDatabase.Query(
        "SELECT Map, BaseMs, PerMinMs, CapMs FROM mod_moba_respawn");

    if (!result)
    {
        LOG_ERROR("sql.sql", "MobaRespawnDataStore: table `mod_moba_respawn` is empty or missing.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        MobaRespawnConfig cfg;
        cfg.map      = fields[0].Get<uint32>();
        cfg.baseMs   = fields[1].Get<uint32>();
        cfg.perMinMs = fields[2].Get<uint32>();
        cfg.capMs    = fields[3].Get<uint32>();

        _byMap[cfg.map] = cfg;
    } while (result->NextRow());
}

MobaRespawnConfig const* MobaRespawnDataStore::GetConfig(uint32 mapId) const
{
    auto itr = _byMap.find(mapId);
    return itr != _byMap.end() ? &itr->second : nullptr;
}
