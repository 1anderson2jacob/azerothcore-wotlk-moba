#include "BattlegroundMOBA.h"
#include "Player.h"
#include "ScriptMgr.h"

// Zero is enough: Player::GiveXP returns on an amount under 1 before it logs or levels
// anything, and a hunter pet's share is cut from the same amount.
class moba_no_xp_playerscript : public PlayerScript
{
public:
    moba_no_xp_playerscript() : PlayerScript("moba_no_xp_playerscript",
        { PLAYERHOOK_ON_GIVE_EXP }) { }

    void OnPlayerGiveXP(Player* player, uint32& amount, Unit* /*victim*/, uint8 /*xpSource*/) override
    {
        if (dynamic_cast<BattlegroundMOBA*>(player->GetBattleground()))
            amount = 0;
    }
};

void AddSC_moba_no_xp()
{
    new moba_no_xp_playerscript();
}
