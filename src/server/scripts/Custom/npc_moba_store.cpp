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

#include "Chat.h"
#include "Creature.h"
#include "Item.h"
#include "Log.h"
#include "MobaStoreData.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "StringFormat.h"

namespace
{
    constexpr uint32 NODE_ROOT = 0;

    std::string FormatMoney(uint32 copper)
    {
        std::string out;

        if (uint32 gold = copper / 10000)
            out += Acore::StringFormat("{}g ", gold);
        if (uint32 silver = (copper % 10000) / 100)
            out += Acore::StringFormat("{}s ", silver);
        if (uint32 remainder = copper % 100)
            out += Acore::StringFormat("{}c", remainder);

        return out.empty() ? "free" : out;
    }

    void ShowNode(Player* player, Creature* creature, MobaStoreNpc const& npc, uint32 nodeId)
    {
        ClearGossipMenuFor(player);

        if (std::vector<MobaStoreNode const*> const* children =
                sMobaStoreDataStore->GetChildren(npc.map, npc.vendorId, nodeId))
        {
            for (MobaStoreNode const* child : *children)
            {
                std::string text = child->label;
                if (child->isPurchase && child->costCopper)
                    text += " - " + FormatMoney(child->costCopper);

                AddGossipItemFor(player, child->isPurchase ? GOSSIP_ICON_MONEY_BAG : GOSSIP_ICON_CHAT,
                                 text, GOSSIP_SENDER_MAIN, child->nodeId);
            }
        }

        // Every real node id is >= 1, so a Back option carrying parentId == 0
        // unambiguously means "return to the top level".
        if (nodeId != NODE_ROOT)
        {
            MobaStoreNode const* node = sMobaStoreDataStore->GetNode(npc.map, npc.vendorId, nodeId);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "<- Back", GOSSIP_SENDER_MAIN,
                             node ? node->parentId : NODE_ROOT);
        }

        SendGossipMenuFor(player, player->GetGossipTextId(creature), creature->GetGUID());
    }

    // The front-end-agnostic purchase core: validates, charges, and grants. A
    // future shop addon would call this same function with a node id.
    void TryPurchase(Player* player, MobaStoreNpc const& npc, MobaStoreNode const& node)
    {
        ChatHandler handler(player->GetSession());

        std::vector<MobaStoreGrant> const* grants =
            sMobaStoreDataStore->GetGrants(npc.map, npc.vendorId, node.nodeId);

        if (!grants || grants->empty())
        {
            LOG_ERROR("sql.sql", "npc_moba_store: purchase node {} (map {}, vendor {}) has no grant rows.",
                      node.nodeId, npc.map, npc.vendorId);
            handler.PSendSysMessage("That is unavailable.");
            return;
        }

        if (node.costCopper && !player->HasEnoughMoney(node.costCopper))
        {
            handler.PSendSysMessage("You cannot afford that ({} needed).", FormatMoney(node.costCopper));
            return;
        }

        // All-or-nothing. A bundle is unique, non-stacking equipment, so it
        // needs one free slot per piece; per-item CanStoreNewItem cannot see
        // the slots the earlier pieces of the same bundle will consume.
        if (player->GetFreeInventorySpace() < grants->size())
        {
            handler.PSendSysMessage("You need {} free bag slots for that.", grants->size());
            return;
        }

        if (node.costCopper)
            player->ModifyMoney(-static_cast<int32>(node.costCopper));

        for (MobaStoreGrant const& grant : *grants)
        {
            ItemPosCountVec dest;
            InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, grant.itemEntry, grant.count);
            if (msg != EQUIP_ERR_OK)
            {
                player->SendEquipError(msg, nullptr, nullptr, grant.itemEntry);
                continue;
            }

            // Suffix ids are stored positive but must be handed to the engine
            // negated -- see Item::GenerateItemRandomPropertyId's RandomSuffix
            // branch, which returns -int32(id).
            int32 randomPropertyId = grant.suffixId ? -static_cast<int32>(grant.suffixId) : 0;

            if (Item* item = player->StoreNewItem(dest, grant.itemEntry, true, randomPropertyId))
                player->SendNewItem(item, grant.count, true, false);
        }
    }
}

class npc_moba_store : public CreatureScript
{
public:
    npc_moba_store() : CreatureScript("npc_moba_store") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        // Caches once per worldserver process; a bool check after the first use.
        sMobaStoreDataStore->LoadIfNeeded();

        MobaStoreNpc const* npc = sMobaStoreDataStore->GetNpc(creature->GetEntry());
        if (!npc)
        {
            LOG_ERROR("sql.sql", "npc_moba_store: creature {} has no `mod_moba_store_npc` row.",
                      creature->GetEntry());
            return true;
        }

        if (player->GetBgTeamId() != npc->team)
        {
            ChatHandler(player->GetSession()).PSendSysMessage("This shopkeeper serves the enemy team.");
            CloseGossipMenuFor(player);
            return true;
        }

        ShowNode(player, creature, *npc, NODE_ROOT);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        MobaStoreNpc const* npc = sMobaStoreDataStore->GetNpc(creature->GetEntry());
        if (!npc || player->GetBgTeamId() != npc->team)
        {
            CloseGossipMenuFor(player);
            return true;
        }

        MobaStoreNode const* node = sMobaStoreDataStore->GetNode(npc->map, npc->vendorId, action);
        if (!node)
        {
            ShowNode(player, creature, *npc, NODE_ROOT);
            return true;
        }

        if (!node->isPurchase)
        {
            ShowNode(player, creature, *npc, node->nodeId);
            return true;
        }

        TryPurchase(player, *npc, *node);

        // Reopen the sibling list rather than the (childless) purchase node, so
        // several pieces can be bought without renavigating.
        ShowNode(player, creature, *npc, node->parentId);
        return true;
    }
};

void AddSC_npc_moba_store()
{
    new npc_moba_store();
}
