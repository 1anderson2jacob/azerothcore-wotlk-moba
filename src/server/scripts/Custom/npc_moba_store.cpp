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

    // What a purchase attempt did, so the caller decides how to report it: gossip
    // prints to chat, the addon sends ERR:. message is empty on success.
    struct PurchaseResult
    {
        bool ok = false;
        std::string message;
    };

    // The single source of truth for "can this player equip or consume this".
    // Both the purchase refusal and the addon's greying read it, so the two can
    // never disagree. Returns nullptr when usable, else the reason to show.
    char const* ItemUnusableReason(Player* player, ItemTemplate const* proto)
    {
        // Covers class, race, faction, required skill/spell and level.
        if (player->CanUseItem(proto) != EQUIP_ERR_OK)
            return "You cannot use that.";

        // Armour and weapon proficiency live only in the Item* overload of
        // CanUseItem, which needs an item that does not exist yet. The template
        // exposes the same skill, so check it directly.
        if (uint32 skill = proto->GetSkill())
            if (!player->GetSkillValue(skill))
                return "You lack the proficiency for that.";

        return nullptr;
    }

    // The front-end-agnostic purchase core: validates, charges, and grants.
    // Takes map/tab explicitly rather than the npc: one shopkeeper serves every
    // tab, so the tab bought from is picked in the panel, not by where you stand.
    PurchaseResult TryPurchase(Player* player, uint32 map, uint32 tabId, MobaStoreNode const& node)
    {
        std::vector<MobaStoreGrant> const* grants =
            sMobaStoreDataStore->GetGrants(map, tabId, node.nodeId);

        if (!grants || grants->empty())
        {
            LOG_ERROR("sql.sql", "npc_moba_store: purchase node {} (map {}, tab {}) has no grant rows.",
                      node.nodeId, map, tabId);
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
        // safety net -- but the usability checks are not: armour proficiency is
        // cumulative upward (plate implies mail, leather, cloth), so the check that
        // matters is refusing a mage the plate set, never the reverse.
        for (MobaStoreGrant const& grant : *grants)
        {
            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(grant.itemEntry);
            if (!proto)
            {
                LOG_ERROR("sql.sql", "npc_moba_store: grant item {} has no item_template row.",
                          grant.itemEntry);
                return { false, "That is unavailable." };
            }

            if (char const* reason = ItemUnusableReason(player, proto))
                return { false, reason };

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
                          "on node {} (map {}, tab {}).",
                          grant.itemEntry, uint32(msg), node.nodeId, map, tabId);
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

    // Usability is per-player but constant for the match -- class, race and skills
    // cannot change -- so it is pushed once at HELLO rather than on every open.
    // Only the UNUSABLE entries go over the wire: the addon treats absent data as
    // usable, and the server revalidates every purchase regardless.
    void SendUnusableEntries(Player* player, BattlegroundMOBA* moba, uint32 map)
    {
        std::set<uint32> entries;
        sMobaStoreDataStore->CollectEntries(map, entries);

        std::string batch;
        for (uint32 entry : entries)
        {
            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry);
            if (!proto || !ItemUnusableReason(player, proto))
                continue;

            if (!batch.empty())
                batch += ',';
            batch += std::to_string(entry);

            if (batch.size() > 180)
            {
                moba->SendShopMessage(player, "NU:" + batch);
                batch.clear();
            }
        }

        if (!batch.empty())
            moba->SendShopMessage(player, "NU:" + batch);
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

        // No menu is ever sent: UNIT_NPC_FLAG_GOSSIP exists purely to make the
        // shopkeeper right-clickable and fire this hook.
        CloseGossipMenuFor(player);

        if (player->GetBgTeamId() != npc->team)
        {
            ChatHandler(player->GetSession()).PSendSysMessage("This shopkeeper serves the enemy team.");
            return true;
        }

        BattlegroundMOBA* moba = dynamic_cast<BattlegroundMOBA*>(player->GetBattleground());
        if (!moba || !moba->HasShopAddon(player))
        {
            // The panel is the only shop front end -- there is deliberately no
            // gossip fallback to keep in sync.
            ChatHandler(player->GetSession()).PSendSysMessage(
                "The MobaHUD addon is required to use the shop. Install it, then /reload.");
            return true;
        }

        moba->SetOpenShopkeeper(player, creature->GetGUID());
        moba->SendShopMessage(player, Acore::StringFormat("OPEN:{}", npc->map));
        SendSuffixFactors(player, moba, npc->map);
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
            {
                moba->SetShopAddonReady(player);

                // Every shop row is keyed on the battleground's map id, which is
                // the map the player is standing on -- there is no NPC to ask yet.
                sMobaStoreDataStore->LoadIfNeeded();
                SendUnusableEntries(player, moba, player->GetMapId());
            }
            else if (payload.compare(0, 4, "BUY:") == 0)
                HandleBuy(player, moba, payload.substr(4));
        }

        return false; // consume: never broadcast shop traffic to battleground chat
    }

private:
    // "BUY:<tabId>,<nodeId>". Nothing here trusts the client beyond those two
    // numbers -- the shopkeeper is whichever one the server remembers the player
    // opened, so a node can never be bought from across the map.
    static void HandleBuy(Player* player, BattlegroundMOBA* moba, std::string const& args)
    {
        std::string::size_type comma = args.find(',');
        if (comma == std::string::npos)
            return;

        // Parse defensively: this is client-supplied text, so a malformed pair is
        // dropped rather than coerced to 0.
        Optional<uint32> tabId  = Acore::StringTo<uint32>(args.substr(0, comma));
        Optional<uint32> nodeId = Acore::StringTo<uint32>(args.substr(comma + 1));
        if (!tabId || !nodeId)
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

        // One shopkeeper sells every tab, so the tab is client-chosen and cannot be
        // inferred from the NPC. Range and team still gate it, and the node must
        // exist for the requested tab.
        MobaStoreNode const* node = sMobaStoreDataStore->GetNode(npc->map, *tabId, *nodeId);
        if (!node || !node->isPurchase)
        {
            moba->SendShopMessage(player, "ERR:That is unavailable.");
            return;
        }

        PurchaseResult result = TryPurchase(player, npc->map, *tabId, *node);
        moba->SendShopMessage(player, result.ok
            ? Acore::StringFormat("OK:{},{}", *tabId, *nodeId)
            : "ERR:" + result.message);
    }
};

void AddSC_npc_moba_store()
{
    new npc_moba_store();
    new moba_shop_playerscript();
}
