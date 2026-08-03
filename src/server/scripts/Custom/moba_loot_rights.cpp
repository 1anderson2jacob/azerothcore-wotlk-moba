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
#include "Creature.h"
#include "LootMgr.h"
#include "Player.h"
#include "ScriptMgr.h"

// Who a kill belongs to, enforced. GrantDeathDrops points the corpse at the
// killing blow; these two scripts are what make that stick for items and for
// gold respectively. Item BOOKKEEPING (what the match handed a player, and what
// it takes back on exit) is a different concern and lives in npc_moba_store.cpp.

// The server-side authority for "only the killing blow may loot".
//
// The visual narrowing in GrantDeathDrops is best-effort -- LootHandler clears
// roundRobinPlayer when the killer closes a corpse they did not empty, which would
// otherwise hand the remainder to the whole team. This check cannot be cleared, so
// it is what actually holds. Player::SendLoot calls it after computing permission
// and refuses with LOOT_ERROR_DIDNT_KILL, which also means SetLootGUID never runs --
// so the money and autostore opcodes, which both require it, are closed as well.
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
        // The same hook guards bag containers and gameobjects, which this rule has
        // nothing to say about.
        if (!player || !source.IsCreatureOrVehicle())
            return false;

        if (!dynamic_cast<BattlegroundMOBA*>(player->GetBattleground()))
            return false;

        Creature* corpse = player->GetMap()->GetCreature(source);
        if (!corpse)
            return false;

        // An empty recipient is GrantDeathDrops' no-last-hit path having stripped
        // the corpse, so this correctly denies everyone rather than admitting all.
        return corpse->GetLootRecipientGUID() != player->GetGUID();
    }
};

// Corpse gold belongs to the match wallet, never to the character.
//
// Fires from HandleLootMoneyOpcode BEFORE both the group-split branch and the solo
// branch, and both read loot->gold after it -- so zeroing it here suppresses the
// split with nothing to fight. That split is why the hook is needed at all: it
// branches on the LOOTING player's group rather than the recipient's, so without
// this every nearby teammate would take an equal cut of a last-hit reward.
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

        // Creature corpses are the only designed income. A player's insignia also
        // carries gold in a battleground, and that is deliberately dropped rather
        // than banked -- the invariant is the blunt one, that inside a MOBA match no
        // loot moves real money, and only what the config priced pays out.
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
