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
#include "StringConvert.h"
#include "StringFormat.h"
#include "BattlegroundMOBA.h"
#include "SharedDefines.h"
#include "ItemEnchantmentMgr.h"
#include "ObjectMgr.h"

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

    // What a purchase attempt did, so the caller decides how to report it: gossip
    // prints to chat, the addon sends ERR:. message is empty on success.
    struct PurchaseResult
    {
        bool ok = false;
        std::string message;
    };

    // The front-end-agnostic purchase core: validates, charges, and grants.
    // Takes map/vendor explicitly rather than the npc: with a tabbed panel the tab
    // being bought from is not necessarily the shopkeeper you are standing at.
    PurchaseResult TryPurchase(Player* player, uint32 map, uint32 vendorId, MobaStoreNode const& node)
    {
        std::vector<MobaStoreGrant> const* grants =
            sMobaStoreDataStore->GetGrants(map, vendorId, node.nodeId);

        if (!grants || grants->empty())
        {
            LOG_ERROR("sql.sql", "npc_moba_store: purchase node {} (map {}, vendor {}) has no grant rows.",
                      node.nodeId, map, vendorId);
            return { false, "That is unavailable." };
        }

        if (node.costCopper && !player->HasEnoughMoney(node.costCopper))
            return { false, Acore::StringFormat("You cannot afford that ({} needed).",
                                                FormatMoney(node.costCopper)) };

        // All-or-nothing. A bundle is unique, non-stacking equipment, so it
        // needs one free slot per piece; per-item CanStoreNewItem cannot see
        // the slots the earlier pieces of the same bundle will consume.
        if (player->GetFreeInventorySpace() < grants->size())
            return { false, Acore::StringFormat("You need {} free bag slots for that.", grants->size()) };

        // Every per-item refusal must be caught BEFORE the money leaves. Nothing in
        // the catalog is maxcount-limited today, so the storage check is a no-op
        // safety net -- but the usability checks are not: the shop must not sell a
        // warrior a cloth set.
        for (MobaStoreGrant const& grant : *grants)
        {
            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(grant.itemEntry);
            if (!proto)
            {
                LOG_ERROR("sql.sql", "npc_moba_store: grant item {} has no item_template row.",
                          grant.itemEntry);
                return { false, "That is unavailable." };
            }

            // Covers class, race, faction, required skill/spell and level.
            if (player->CanUseItem(proto) != EQUIP_ERR_OK)
                return { false, "You cannot use that." };

            // Armour and weapon proficiency live only in the Item* overload of
            // CanUseItem, which needs an item that does not exist yet. The template
            // exposes the same skill, so check it directly.
            if (uint32 skill = proto->GetSkill())
                if (!player->GetSkillValue(skill))
                    return { false, "You lack the proficiency for that." };

            ItemPosCountVec dest;
            InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, grant.itemEntry, grant.count);
            if (msg != EQUIP_ERR_OK)
            {
                player->SendEquipError(msg, nullptr, nullptr, grant.itemEntry);
                return { false, "You cannot carry that." };
            }
        }

        if (node.costCopper)
            player->ModifyMoney(-static_cast<int32>(node.costCopper));

        for (MobaStoreGrant const& grant : *grants)
        {
            ItemPosCountVec dest;
            InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, grant.itemEntry, grant.count);
            if (msg != EQUIP_ERR_OK)
            {
                // The pass above cleared this item against an empty-handed
                // player, so a refusal here means earlier pieces of this same
                // bundle took the room. The money is already gone, so it must
                // not fail silently.
                LOG_ERROR("scripts.moba", "npc_moba_store: item {} refused ({}) after pre-validation "
                          "on node {} (map {}, vendor {}).",
                          grant.itemEntry, uint32(msg), node.nodeId, map, vendorId);
                player->SendEquipError(msg, nullptr, nullptr, grant.itemEntry);
                continue;
            }

            // Suffix ids are stored positive but must be handed to the engine
            // negated -- see Item::GenerateItemRandomPropertyId's RandomSuffix
            // branch, which returns -int32(id).
            int32 randomPropertyId = grant.suffixId ? -static_cast<int32>(grant.suffixId) : 0;

            if (Item* item = player->StoreNewItem(dest, grant.itemEntry, true, randomPropertyId))
            {
                // Bind at grant rather than at equip: the client only prompts
                // "this will bind to you" for an unbound bind-on-equip item, and
                // soulbound shop gear also cannot be traded to a teammate. Once
                // custom_items is on, the copies carry BIND_WHEN_PICKED_UP and
                // this becomes redundant.
                item->SetBinding(true);

                // Nothing else tracks these -- the battleground destroys exactly
                // these item GUIDs when the player leaves.
                if (BattlegroundMOBA* moba = dynamic_cast<BattlegroundMOBA*>(player->GetBattleground()))
                    moba->RecordGrantedItem(player, item);

                player->SendNewItem(item, grant.count, true, false);
            }
        }

        return { true, "" };
    }

    // Batched so a panel open costs a handful of messages rather than one per item.
    void SendSuffixFactors(Player* player, BattlegroundMOBA* moba, uint32 map)
    {
        std::set<uint32> entries;
        sMobaStoreDataStore->CollectSuffixedEntries(map, entries);

        std::string batch;
        for (uint32 entry : entries)
        {
            uint32 factor = GenerateEnchSuffixFactor(entry);
            if (!factor)
                continue;

            if (!batch.empty())
                batch += ',';
            batch += Acore::StringFormat("{}:{}", entry, factor);

            if (batch.size() > 180)
            {
                moba->SendShopMessage(player, "SF:" + batch);
                batch.clear();
            }
        }

        if (!batch.empty())
            moba->SendShopMessage(player, "SF:" + batch);
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

        // Addon players get the panel. The gossip walk below is the temporary
        // fallback and is deleted once the panel ships.
        if (BattlegroundMOBA* moba = dynamic_cast<BattlegroundMOBA*>(player->GetBattleground()))
        {
            if (moba->HasShopAddon(player))
            {
                moba->SetOpenShopkeeper(player, creature->GetGUID());
                moba->SendShopMessage(player, Acore::StringFormat("OPEN:{},{}", npc->map, npc->vendorId));
                SendSuffixFactors(player, moba, npc->map);
                CloseGossipMenuFor(player);
                return true;
            }
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

        PurchaseResult result = TryPurchase(player, npc->map, npc->vendorId, *node);
        if (!result.ok)
            ChatHandler(player->GetSession()).PSendSysMessage("{}", result.message);

        // Reopen the sibling list rather than the (childless) purchase node, so
        // several pieces can be bought without renavigating.
        ShowNode(player, creature, *npc, node->parentId);
        return true;
    }
};

// Client->server half of the shop protocol. Lives here rather than in moba_hud.cpp
// so TryPurchase stays in the anonymous namespace above.
//
// INVARIANT: return false ONLY for MobaShop. ScriptMgr's boolean-hook macro stops
// at the first script returning false, so consuming another prefix here would
// starve the HUD's hook.
class moba_shop_playerscript : public PlayerScript
{
public:
    moba_shop_playerscript() : PlayerScript("moba_shop_playerscript", { PLAYERHOOK_CAN_PLAYER_USE_GROUP_CHAT }) { }

    bool OnPlayerCanUseChat(Player* player, uint32 /*type*/, uint32 lang, std::string& msg, Group* /*group*/) override
    {
        if (lang != LANG_ADDON)
            return true;

        std::string::size_type tab = msg.find('\t');
        if (tab == std::string::npos || msg.compare(0, tab, MOBA_SHOP_ADDON_PREFIX) != 0)
            return true;

        std::string payload = msg.substr(tab + 1);

        if (BattlegroundMOBA* moba = dynamic_cast<BattlegroundMOBA*>(player->GetBattleground()))
        {
            if (payload == "HELLO")
                moba->SetShopAddonReady(player);
            else if (payload.compare(0, 4, "BUY:") == 0)
                HandleBuy(player, moba, payload.substr(4));
        }

        return false; // consume: never broadcast shop traffic to battleground chat
    }

private:
    // "BUY:<vendorId>,<nodeId>". Nothing here trusts the client beyond those two
    // numbers -- the shopkeeper is whichever one the server remembers the player
    // opened, so a node can never be bought from across the map.
    static void HandleBuy(Player* player, BattlegroundMOBA* moba, std::string const& args)
    {
        std::string::size_type comma = args.find(',');
        if (comma == std::string::npos)
            return;

        // Parse defensively: this is client-supplied text, so a malformed pair is
        // dropped rather than coerced to 0.
        Optional<uint32> vendorId = Acore::StringTo<uint32>(args.substr(0, comma));
        Optional<uint32> nodeId   = Acore::StringTo<uint32>(args.substr(comma + 1));
        if (!vendorId || !nodeId)
            return;

        Creature* creature = player->GetNPCIfCanInteractWith(moba->GetOpenShopkeeper(player), UNIT_NPC_FLAG_GOSSIP);
        if (!creature)
        {
            // The status line lives inside the panel we are about to hide, so the
            // reason has to ride along with CLOSE and land in the chat frame.
            moba->SendShopMessage(player, "CLOSE:You are too far from the shopkeeper.");
            return;
        }

        sMobaStoreDataStore->LoadIfNeeded();

        MobaStoreNpc const* npc = sMobaStoreDataStore->GetNpc(creature->GetEntry());
        if (!npc || player->GetBgTeamId() != npc->team)
        {
            moba->SendShopMessage(player, "ERR:That shopkeeper cannot sell you this.");
            return;
        }

        // Any shopkeeper sells any tab: the panel is tabbed, so the tab bought from
        // is not necessarily the NPC standing in front of you. Range and team still
        // gate it, and the node must exist for the requested vendor.
        MobaStoreNode const* node = sMobaStoreDataStore->GetNode(npc->map, *vendorId, *nodeId);
        if (!node || !node->isPurchase)
        {
            moba->SendShopMessage(player, "ERR:That is unavailable.");
            return;
        }

        PurchaseResult result = TryPurchase(player, npc->map, *vendorId, *node);
        moba->SendShopMessage(player, result.ok
            ? Acore::StringFormat("OK:{},{}", *vendorId, *nodeId)
            : "ERR:" + result.message);
    }
};

void AddSC_npc_moba_store()
{
    new npc_moba_store();
    new moba_shop_playerscript();
}
