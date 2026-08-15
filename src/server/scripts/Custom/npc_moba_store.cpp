#include "Chat.h"
#include "Creature.h"
#include "Item.h"
#include "Log.h"
#include "MobaStoreData.h"
#include "MobaDropData.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "StringConvert.h"
#include "StringFormat.h"
#include "BattlegroundMOBA.h"
#include "SharedDefines.h"
#include "ItemEnchantmentMgr.h"
#include "ObjectMgr.h"
#include <algorithm>

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

    // message is empty on success; the caller decides how to report it.
    struct PurchaseResult
    {
        bool ok = false;
        std::string message;
    };

    // Both the purchase refusal and the addon's greying read this, so the two can never
    // disagree. nullptr when usable, else the reason to show.
    char const* ItemUnusableReason(Player* player, ItemTemplate const* proto)
    {
        // Covers class, race, faction, required skill/spell and level.
        if (player->CanUseItem(proto) != EQUIP_ERR_OK)
            return "You cannot use that.";

        // Proficiency lives only in the Item* overload of CanUseItem, which needs an
        // item that does not exist yet. The template exposes the same skill.
        if (uint32 skill = proto->GetSkill())
            if (!player->GetSkillValue(skill))
                return "You lack the proficiency for that.";

        return nullptr;
    }

    // Lua numbers bags 0 = backpack, 1-4 = equipped, with 1-based slots. The engine keeps
    // the backpack in pseudo-bag INVENTORY_SLOT_BAG_0 at INVENTORY_SLOT_ITEM_START..END,
    // and an equipped bag's contents at 0-based indices under its inventory slot.
    //
    // Equipped gear is unreachable BY CONSTRUCTION: bag 0 maps to slot 23 upward, so
    // nothing the client can say names EQUIPMENT_SLOT_* (0-18). Unequip to sell.
    bool ResolveBagSlot(uint32 luaBag, uint32 luaSlot, uint8& bag, uint8& slot)
    {
        if (!luaSlot)
            return false;

        if (luaBag == 0)
        {
            if (luaSlot > INVENTORY_SLOT_ITEM_END - INVENTORY_SLOT_ITEM_START)
                return false;

            bag  = INVENTORY_SLOT_BAG_0;
            slot = uint8(INVENTORY_SLOT_ITEM_START + luaSlot - 1);
            return true;
        }

        if (luaBag > INVENTORY_SLOT_BAG_END - INVENTORY_SLOT_BAG_START)
            return false;

        bag  = uint8(INVENTORY_SLOT_BAG_START + luaBag - 1);
        slot = uint8(luaSlot - 1);
        return true;
    }

    // The shop's price if the shop sold it, else a drop's. False for anything neither
    // priced, which is the whole gate on what can be sold.
    bool ResolveSellValue(uint32 map, uint32 itemEntry, uint32& out)
    {
        if (sMobaStoreDataStore->GetSellValue(map, itemEntry, out))
            return true;

        return sMobaDropDataStore->GetItemSellValue(itemEntry, out);
    }

    // Validates, charges, grants. Takes map/tab explicitly rather than the npc: one
    // shopkeeper serves every tab, so the tab is picked in the panel. The battleground
    // is the wallet -- purchases are paid out of the match, never out of real money.
    PurchaseResult TryPurchase(Player* player, BattlegroundMOBA* moba, uint32 map,
                               uint32 tabId, MobaStoreNode const& node)
    {
        std::vector<MobaStoreGrant> const* grants =
            sMobaStoreDataStore->GetGrants(map, tabId, node.nodeId);

        if (!grants || grants->empty())
        {
            LOG_ERROR("sql.sql", "npc_moba_store: purchase node {} (map {}, tab {}) has no grant rows.",
                      node.nodeId, map, tabId);
            return { false, "That is unavailable." };
        }

        if (node.costCopper && moba->GetMatchGold(player) < node.costCopper)
            return { false, Acore::StringFormat("You cannot afford that ({} needed).",
                                                FormatMoney(node.costCopper)) };

        // All-or-nothing: a bundle needs one free slot per piece, and per-item
        // CanStoreNewItem cannot see the slots its earlier pieces will consume.
        if (player->GetFreeInventorySpace() < grants->size())
            return { false, Acore::StringFormat("You need {} free bag slots for that.", grants->size()) };

        // Every per-item refusal must be caught BEFORE the money leaves. Armour
        // proficiency is cumulative upward (plate implies mail, leather, cloth), so the
        // check that matters is refusing a mage the plate set, never the reverse.
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

        // The check above already cleared the price, but this is the atomic form: the
        // money can never leave without the caller learning it did.
        if (node.costCopper && !moba->SpendMatchGold(player, node.costCopper))
            return { false, "You cannot afford that." };

        for (MobaStoreGrant const& grant : *grants)
        {
            ItemPosCountVec dest;
            InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, grant.itemEntry, grant.count);
            if (msg != EQUIP_ERR_OK)
            {
                // The pass above cleared this against an empty-handed player, so a
                // refusal here means earlier pieces of this bundle took the room -- and
                // the money is already gone.
                LOG_ERROR("scripts.moba", "npc_moba_store: item {} refused ({}) after pre-validation "
                          "on node {} (map {}, tab {}).",
                          grant.itemEntry, uint32(msg), node.nodeId, map, tabId);
                player->SendEquipError(msg, nullptr, nullptr, grant.itemEntry);
                continue;
            }

            // Stored positive, handed to the engine negated -- see
            // Item::GenerateItemRandomPropertyId's RandomSuffix branch.
            int32 randomPropertyId = grant.suffixId ? -static_cast<int32>(grant.suffixId) : 0;

            if (Item* item = player->StoreNewItem(dest, grant.itemEntry, true, randomPropertyId))
            {
                // Bind at grant, not at equip: it suppresses the client's "this will
                // bind to you" prompt and stops gear being traded to a teammate.
                // Redundant once custom_items ships with BIND_WHEN_PICKED_UP.
                item->SetBinding(true);

                // Nothing else tracks these; the battleground destroys exactly these
                // GUIDs on exit.
                moba->RecordGrantedItem(player, item, grant.count);

                player->SendNewItem(item, grant.count, true, false);
            }
        }

        return { true, "" };
    }

    // Batched so the handshake costs a handful of messages rather than one per item.
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

    // Constant for the match -- class, race and skills cannot change -- so it is pushed
    // once at HELLO. Only UNUSABLE entries go over the wire: the addon treats absent data
    // as usable, and the server revalidates every purchase anyway.
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
            // The panel is the only shop front end; there is no gossip fallback.
            ChatHandler(player->GetSession()).PSendSysMessage(
                "The MobaHUD addon is required to use the shop. Install it, then /reload.");
            return true;
        }

        // Everything the panel needs to DRAW arrived at HELLO; this only raises it.
        moba->SendShopMessage(player, "OPEN");
        return true;
    }
};

// Client->server half of the shop protocol. Lives here rather than in moba_hud.cpp so
// TryPurchase stays in the anonymous namespace above.
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

                // Every shop row is keyed on the map the player is standing on: with
                // the minimap button there may be no NPC to ask.
                sMobaStoreDataStore->LoadIfNeeded();

                uint32 map = player->GetMapId();
                // The version rides on INIT because the addon cannot ask for it later
                // without already knowing the map, and the map is what this carries.
                moba->SendShopMessage(player, Acore::StringFormat("INIT:{}:{}",
                    map, sMobaStoreDataStore->GetCatalogVersion(map)));
                SendSuffixFactors(player, moba, map);
                SendUnusableEntries(player, moba, map);
                moba->SendShopRange(player, true);
            }
            else if (payload.compare(0, 4, "BUY:") == 0)
                HandleBuy(player, moba, payload.substr(4));
            else if (payload.compare(0, 5, "SELL:") == 0)
                HandleSell(player, moba, payload.substr(5));
        }

        // Consume, so shop traffic never reaches battleground chat. Returning false for
        // any OTHER prefix would starve its owner: the boolean-hook macro stops at the
        // first script that returns false.
        return false;
    }

private:
    // "BUY:<tabId>,<nodeId>". Nothing here trusts the client beyond those two numbers;
    // the map and the range gate both come from where the player is standing.
    static void HandleBuy(Player* player, BattlegroundMOBA* moba, std::string const& args)
    {
        std::string::size_type comma = args.find(',');
        if (comma == std::string::npos)
            return;

        // Client-supplied text: a malformed pair is dropped, never coerced to 0.
        Optional<uint32> tabId  = Acore::StringTo<uint32>(args.substr(0, comma));
        Optional<uint32> nodeId = Acore::StringTo<uint32>(args.substr(comma + 1));
        if (!tabId || !nodeId)
            return;

        if (!moba->IsInShopRange(player))
        {
            // Not CLOSE: the panel is the player's, opened from their minimap. The
            // status line says why and the panel stays up.
            moba->SendShopMessage(player, "ERR:Return to your base to buy.");
            return;
        }

        sMobaStoreDataStore->LoadIfNeeded();

        uint32 map = player->GetMapId();

        // One shopkeeper sells every tab, so the tab is client-chosen. Range gates it,
        // and the node must exist for the requested tab.
        MobaStoreNode const* node = sMobaStoreDataStore->GetNode(map, *tabId, *nodeId);
        if (!node || !node->isPurchase)
        {
            moba->SendShopMessage(player, "ERR:That is unavailable.");
            return;
        }

        PurchaseResult result = TryPurchase(player, moba, map, *tabId, *node);
        moba->SendShopMessage(player, result.ok
            ? Acore::StringFormat("OK:{},{}", *tabId, *nodeId)
            : "ERR:" + result.message);
    }

    // "SELL:<luaBag>,<luaSlot>,<itemEntry>". Bag/slot names WHICH item; the entry is a
    // checksum. A mismatch is refused rather than resolved, so a desynced client can
    // never sell something other than what the player dragged.
    static void HandleSell(Player* player, BattlegroundMOBA* moba, std::string const& args)
    {
        std::string::size_type first = args.find(',');
        if (first == std::string::npos)
            return;

        std::string::size_type second = args.find(',', first + 1);
        if (second == std::string::npos)
            return;

        Optional<uint32> luaBag  = Acore::StringTo<uint32>(args.substr(0, first));
        Optional<uint32> luaSlot = Acore::StringTo<uint32>(args.substr(first + 1, second - first - 1));
        Optional<uint32> entry   = Acore::StringTo<uint32>(args.substr(second + 1));
        if (!luaBag || !luaSlot || !entry)
            return;

        // Same gate as buying: only where the player is standing can gate a trade.
        if (!moba->IsInShopRange(player))
        {
            moba->SendShopMessage(player, "ERR:Return to your base to sell.");
            return;
        }

        sMobaStoreDataStore->LoadIfNeeded();
        sMobaDropDataStore->LoadIfNeeded();

        uint32 map = player->GetMapId();

        uint8 bag = 0, slot = 0;
        if (!ResolveBagSlot(*luaBag, *luaSlot, bag, slot))
        {
            moba->SendShopMessage(player, "ERR:Sell items out of a bag.");
            return;
        }

        Item* item = player->GetItemByPos(bag, slot);
        if (!item)
        {
            moba->SendShopMessage(player, "ERR:There is nothing there.");
            return;
        }

        if (item->GetEntry() != *entry)
        {
            moba->SendShopMessage(player, "ERR:That item moved. Try again.");
            return;
        }

        uint32 owed = moba->GetGrantedCount(player, item->GetEntry());
        if (!owed)
        {
            moba->SendShopMessage(player, "ERR:Only items from this match can be sold.");
            return;
        }

        uint32 unitPrice = 0;
        if (!ResolveSellValue(map, item->GetEntry(), unitPrice))
        {
            moba->SendShopMessage(player, "ERR:That cannot be sold.");
            return;
        }

        // Sell only what the match gave; the rest of the stack is the player's own.
        // DestroyItemCount ZEROES its count argument, so price it first.
        uint32 sellCount = std::min(item->GetCount(), owed);
        uint32 payout    = unitPrice * sellCount;

        moba->ForgetGrantedItem(player, item, sellCount);
        player->DestroyItemCount(item, sellCount, true);

        if (payout)
            moba->AddMatchGold(player, payout, MOBA_GOLD_SILENT);

        moba->SendShopMessage(player, Acore::StringFormat("SOLD:{}", payout));
    }
};

// Creep and neutral item drops ride the NATIVE loot system, so they never pass through
// TryPurchase and GrantDeathDrops never sees them. Recording them here is what lets
// RemovePlayer strip them on exit, and what makes them sellable. The recall Hearthstone
// escapes it by construction: AddPlayer hands that over with AddItem, not loot.
class moba_loot_playerscript : public PlayerScript
{
public:
    moba_loot_playerscript() : PlayerScript("moba_loot_playerscript", { PLAYERHOOK_ON_LOOT_ITEM }) { }

    // For a stackable, `item` is the MERGED stack -- so a player who brought their own
    // copy of a dropped item has the whole stack recorded, and loses it on exit. Stock
    // entries stay ambiguous until custom_items ships.
    void OnPlayerLootItem(Player* player, Item* item, uint32 count, ObjectGuid /*lootguid*/) override
    {
        if (BattlegroundMOBA* moba = dynamic_cast<BattlegroundMOBA*>(player->GetBattleground()))
            moba->RecordGrantedItem(player, item, count);
    }
};

void AddSC_npc_moba_store()
{
    new npc_moba_store();
    new moba_shop_playerscript();
    new moba_loot_playerscript();
}
