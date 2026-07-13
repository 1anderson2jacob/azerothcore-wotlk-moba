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

#include "BattlegroundMOBA.h"
#include "Player.h"
#include "ScriptMgr.h"

// LoL-style resurrection: the timer starts when the player clicks "Release
// Spirit" (this hook), not at the moment of death. BattlegroundMOBA owns the
// per-player countdown and the revive; this is just the trigger that forwards
// a MOBA release into it.
class moba_resurrection_playerscript : public PlayerScript
{
public:
    moba_resurrection_playerscript() : PlayerScript("moba_resurrection_playerscript", { PLAYERHOOK_ON_PLAYER_RELEASED_GHOST }) { }

    void OnPlayerReleasedGhost(Player* player) override
    {
        if (!player)
            return;

        Battleground* bg = player->GetBattleground();
        if (!bg || bg->GetStatus() != STATUS_IN_PROGRESS)
            return;

        if (BattlegroundMOBA* moba = dynamic_cast<BattlegroundMOBA*>(bg))
            moba->StartRespawnTimer(player);
    }
};

void AddSC_moba_resurrection()
{
    new moba_resurrection_playerscript();
}
