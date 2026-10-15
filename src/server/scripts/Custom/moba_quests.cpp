#include "BattlegroundMOBA.h"
#include "MobaQuestData.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestDef.h"
#include "ScriptMgr.h"

// Despite its name the hook fires at the end of RewardQuest (PlayerQuest.cpp:880),
// after the turn-in is recorded, so the walk clears the tier just turned in as well.
class moba_quests_playerscript : public PlayerScript
{
public:
    moba_quests_playerscript() : PlayerScript("moba_quests_playerscript",
        { PLAYERHOOK_ON_PLAYER_COMPLETE_QUEST }) { }

    void OnPlayerCompleteQuest(Player* player, Quest const* quest) override
    {
        if (!dynamic_cast<BattlegroundMOBA*>(player->GetBattleground()))
            return;

        if (!sMobaQuestDataStore->ResetsChain(quest->GetQuestId()))
            return;

        for (Quest const* tier = quest; tier;
             tier = sObjectMgr->GetQuestTemplate(uint32(tier->GetPrevQuestId())))
            player->RemoveRewardedQuest(tier->GetQuestId());
    }
};

void AddSC_moba_quests()
{
    new moba_quests_playerscript();
}
