#include "Battleground.h"
#include "BattlegroundMOBA.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"

// The addon pings "MobaHUD\tREQ" on entering the world; we answer with current HUD state
// so the bar populates the instant the client is listening. Battleground chat routes
// through the group-chat hook.
class moba_hud_playerscript : public PlayerScript
{
public:
    moba_hud_playerscript() : PlayerScript("moba_hud_playerscript", { PLAYERHOOK_CAN_PLAYER_USE_GROUP_CHAT }) { }

    bool OnPlayerCanUseChat(Player* player, uint32 /*type*/, uint32 lang, std::string& msg, Group* /*group*/) override
    {
        if (lang != LANG_ADDON)
            return true;

        std::string::size_type tab = msg.find('\t');
        if (tab == std::string::npos || msg.compare(0, tab, MOBA_HUD_ADDON_PREFIX) != 0)
            return true;

        if (msg.compare(tab + 1, std::string::npos, "REQ") == 0)
            if (BattlegroundMOBA* moba = dynamic_cast<BattlegroundMOBA*>(player->GetBattleground()))
                moba->SendHudStateTo(player);

        // Consume, so HUD traffic never reaches battleground chat. Returning false for
        // any OTHER prefix would starve its owner: the boolean-hook macro stops at the
        // first script that returns false.
        return false;
    }
};

void AddSC_moba_hud()
{
    new moba_hud_playerscript();
}
