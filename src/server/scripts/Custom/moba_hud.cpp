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

#include "Battleground.h"
#include "BattlegroundMOBA.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"

// The MobaHUD client addon sends a one-shot "ready" ping over the battleground addon
// channel (SendAddonMessage("MobaHUD", "REQ", "BATTLEGROUND")) on entering the world.
// We answer with the current HUD state so the bar populates the instant the client is
// listening -- no warmup polling. This fires via the group-chat CanUseChat hook, which
// battleground chat routes through; returning false consumes the ping.
class moba_hud_playerscript : public PlayerScript
{
public:
    moba_hud_playerscript() : PlayerScript("moba_hud_playerscript", { PLAYERHOOK_CAN_PLAYER_USE_GROUP_CHAT }) { }

    bool OnPlayerCanUseChat(Player* player, uint32 /*type*/, uint32 lang, std::string& msg, Group* /*group*/) override
    {
        // Only our addon ping; everything else (real BG chat, other addons) passes through.
        if (lang != LANG_ADDON || msg != "MobaHUD\tREQ")
            return true;

        if (BattlegroundMOBA* moba = dynamic_cast<BattlegroundMOBA*>(player->GetBattleground()))
            moba->SendHudStateTo(player);

        return false; // consume: don't broadcast the ping to battleground chat
    }
};

void AddSC_moba_hud()
{
    new moba_hud_playerscript();
}
