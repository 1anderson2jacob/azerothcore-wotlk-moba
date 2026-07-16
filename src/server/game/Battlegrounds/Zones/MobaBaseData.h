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

#ifndef MOBA_BASE_DATA_H
#define MOBA_BASE_DATA_H

#include "Common.h"
#include <unordered_map>

// One row per MOBA map: the tunables anchored to a team's base -- respawn wait,
// recall cast time, fountain healing.
// Two things deliberately live elsewhere, both written from the same per-map
// config (see gen_base.py):
//   * the base LOCATION -- game_graveyard / battleground_template, read at
//     runtime via GetTeamStartPosition / GetClosestGraveyard;
//   * the base RADIUS -- battleground_template.StartMaxDist, read via
//     Battleground::GetStartMaxDist(). It is both the core's prep-phase leash
//     and the fountain heal zone, so the two can't drift apart.
struct MobaBaseConfig
{
    uint32 map = 0;
    uint32 respawnBaseMs = 10000;
    uint32 respawnPerMinMs = 1500;
    uint32 respawnCapMs = 60000;
    uint32 recallCastMs = 0;
    uint32 recallEmpoweredCastMs = 0;
    uint32 fountainTickMs = 0;      // 0 = fountain healing off
    uint32 fountainHpPct = 0;
    uint32 fountainManaPct = 0;
};

// Loads data/sql/custom/mod_moba_base.sql's `mod_moba_base` table once, keyed
// by map id. Read by BattlegroundMOBA::StartRespawnTimer() and
// ::GetRecallCastTimeMs().
class MobaBaseDataStore
{
public:
    static MobaBaseDataStore* instance();

    void LoadIfNeeded();
    MobaBaseConfig const* GetConfig(uint32 mapId) const;

private:
    MobaBaseDataStore() = default;

    bool _loaded = false;
    std::unordered_map<uint32, MobaBaseConfig> _byMap;
};

#define sMobaBaseDataStore MobaBaseDataStore::instance()

#endif
