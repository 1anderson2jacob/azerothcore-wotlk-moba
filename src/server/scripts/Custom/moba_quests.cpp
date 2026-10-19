#include "BattlegroundMOBA.h"
#include "MobaQuestData.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestDef.h"
#include "ScriptMgr.h"
#include <algorithm>

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

// A creep kill reaches KillRewarder with isBattleGround false (Unit.cpp:14093), which
// credits the first tapper's group for the victim's KillCredit1 -- set only so the
// client's tooltip can match a target to its quest. Inside a match a MOBA quest
// advances only on the battleground's own credit.
class moba_quest_credit_playerscript : public PlayerScript
{
public:
    moba_quest_credit_playerscript() : PlayerScript("moba_quest_credit_playerscript",
        { PLAYERHOOK_PASSED_QUEST_KILLED_MONSTER_CREDIT }) { }

    // false refuses the credit -- the opposite of moba_loot_rights' hook
    bool OnPlayerPassedQuestKilledMonsterCredit(Player* player, Quest const* quest,
        uint32 /*entry*/, uint32 /*real_entry*/, ObjectGuid /*guid*/) override
    {
        BattlegroundMOBA* moba = dynamic_cast<BattlegroundMOBA*>(player->GetBattleground());
        if (!moba || moba->IsCreditingQuests())
            return true;

        std::vector<uint32> const& quests = sMobaQuestDataStore->GetQuests(moba->GetMapId());
        return std::find(quests.begin(), quests.end(), quest->GetQuestId()) == quests.end();
    }
};

void AddSC_moba_quests()
{
    new moba_quests_playerscript();
    new moba_quest_credit_playerscript();
}
