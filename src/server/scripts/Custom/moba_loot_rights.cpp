#include "BattlegroundMOBA.h"
#include "Creature.h"
#include "LootMgr.h"
#include "Player.h"
#include "ScriptMgr.h"

// GrantDeathDrops points the corpse at the killing blow; these two scripts make that
// stick, for items and for gold. Item bookkeeping lives in npc_moba_store.cpp.

// The server-side authority for "only the killing blow may loot". GrantDeathDrops'
// narrowing is best-effort -- LootHandler clears roundRobinPlayer when the killer closes
// a corpse they did not empty -- so this check is what actually holds.
//
// INVERTED RETURN: CALL_ENABLED_BOOLEAN_HOOKS turns a script returning true into
// "denied". true here means BLOCK this player.
class moba_loot_rights_globalscript : public GlobalScript
{
public:
    moba_loot_rights_globalscript() : GlobalScript("moba_loot_rights_globalscript",
        { GLOBALHOOK_ON_ALLOWED_TO_LOOT_CONTAINER_CHECK }) { }

    bool OnAllowedToLootContainerCheck(Player const* player, ObjectGuid source) override
    {
        // The same hook guards bag containers and gameobjects.
        if (!player || !source.IsCreatureOrVehicle())
            return false;

        if (!dynamic_cast<BattlegroundMOBA*>(player->GetBattleground()))
            return false;

        Creature* corpse = player->GetMap()->GetCreature(source);
        if (!corpse)
            return false;

        // An empty recipient means GrantDeathDrops' no-last-hit path stripped the
        // corpse, so this denies everyone rather than admitting all.
        return corpse->GetLootRecipientGUID() != player->GetGUID();
    }
};

// Corpse gold belongs to the match wallet, never to the character. Fires from
// HandleLootMoneyOpcode ahead of both the group-split and solo branches, which read
// loot->gold after it -- so zeroing it here suppresses the split. That split branches on
// the LOOTING player's group, not the recipient's, which is why the hook is needed.
class moba_loot_money_playerscript : public PlayerScript
{
public:
    moba_loot_money_playerscript() : PlayerScript("moba_loot_money_playerscript",
        { PLAYERHOOK_ON_BEFORE_LOOT_MONEY }) { }

    void OnPlayerBeforeLootMoney(Player* player, Loot* loot) override
    {
        BattlegroundMOBA* moba = dynamic_cast<BattlegroundMOBA*>(player->GetBattleground());
        if (!moba || !loot || !loot->gold)
            return;

        // Creature corpses are the only designed income. A player insignia's gold is
        // dropped rather than banked: inside a match no loot moves real money.
        ObjectGuid lootGuid = player->GetLootGUID();
        if (lootGuid.IsCreatureOrVehicle())
            if (Creature* corpse = player->GetMap()->GetCreature(lootGuid))
                if (corpse->GetLootRecipientGUID() == player->GetGUID())
                    moba->AddMatchGold(player, loot->gold);

        loot->gold = 0;
    }
};

void AddSC_moba_loot_rights()
{
    new moba_loot_rights_globalscript();
    new moba_loot_money_playerscript();
}
