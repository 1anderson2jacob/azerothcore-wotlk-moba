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

#include "MobaBaseData.h"
#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "Field.h"
#include "Log.h"

MobaBaseDataStore* MobaBaseDataStore::instance()
{
    static MobaBaseDataStore instance;
    return &instance;
}

void MobaBaseDataStore::LoadIfNeeded()
{
    if (_loaded)
        return;

    _loaded = true;

    QueryResult result = WorldDatabase.Query(
        "SELECT Map, RespawnBaseMs, RespawnPerMinMs, RespawnCapMs, RecallCastMs, RecallEmpoweredCastMs, "
        "FountainTickMs, FountainHpPct, FountainManaPct, KillCreditWindowMs, AssistWindowMs, AssistBuffMaxDurationMs FROM mod_moba_base");

    if (!result)
    {
        LOG_ERROR("sql.sql", "MobaBaseDataStore: table `mod_moba_base` is empty or missing.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        MobaBaseConfig cfg;
        cfg.map                   = fields[0].Get<uint32>();
        cfg.respawnBaseMs         = fields[1].Get<uint32>();
        cfg.respawnPerMinMs       = fields[2].Get<uint32>();
        cfg.respawnCapMs          = fields[3].Get<uint32>();
        cfg.recallCastMs          = fields[4].Get<uint32>();
        cfg.recallEmpoweredCastMs = fields[5].Get<uint32>();
        cfg.fountainTickMs        = fields[6].Get<uint32>();
        cfg.fountainHpPct         = fields[7].Get<uint32>();
        cfg.fountainManaPct       = fields[8].Get<uint32>();
        cfg.killCreditWindowMs      = fields[9].Get<uint32>();
        cfg.assistWindowMs          = fields[10].Get<uint32>();
        cfg.assistBuffMaxDurationMs = fields[11].Get<uint32>();

        _byMap[cfg.map] = cfg;
    } while (result->NextRow());
}

MobaBaseConfig const* MobaBaseDataStore::GetConfig(uint32 mapId) const
{
    auto itr = _byMap.find(mapId);
    return itr != _byMap.end() ? &itr->second : nullptr;
}
