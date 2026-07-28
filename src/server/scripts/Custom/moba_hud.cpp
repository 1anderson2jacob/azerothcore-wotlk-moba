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

// The MobaHUD addon sends a one-shot "ready" ping ("MobaHUD\tREQ") over the
// battleground addon channel on entering the world; we answer with current HUD
// state so the bar populates the instant the client is listening. Fires via the
// group-chat CanUseChat hook, which battleground chat routes through.
//
// INVARIANT: return false ONLY for our own prefix. ScriptMgr's boolean-hook macro
// stops at the first script that returns false, so consuming another addon's
// prefix here would silently starve whichever script actually owns it --
// npc_moba_store's shop hook is the one that matters.
class moba_hud_playerscript : public PlayerScript
{
public:
    moba_hud_playerscript() : PlayerScript("moba_hud_playerscript", { PLAYERHOOK_CAN_PLAYER_USE_GROUP_CHAT }) { }

    bool OnPlayerCanUseChat(Player* player, uint32 /*type*/, uint32 lang, std::string& msg, Group* /*group*/) override
    {
        if (lang != LANG_ADDON)
            return true;

        // "<prefix>\t<payload>" -- the same framing the server sends back.
        std::string::size_type tab = msg.find('\t');
        if (tab == std::string::npos || msg.compare(0, tab, MOBA_HUD_ADDON_PREFIX) != 0)
            return true;

        if (msg.compare(tab + 1, std::string::npos, "REQ") == 0)
            if (BattlegroundMOBA* moba = dynamic_cast<BattlegroundMOBA*>(player->GetBattleground()))
                moba->SendHudStateTo(player);

        return false; // consume: never broadcast HUD traffic to battleground chat
    }
};

void AddSC_moba_hud()
{
    new moba_hud_playerscript();
}
