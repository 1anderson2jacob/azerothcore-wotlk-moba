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

#include "MobaStoreData.h"
#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "Field.h"
#include "Log.h"

MobaStoreDataStore* MobaStoreDataStore::instance()
{
    static MobaStoreDataStore instance;
    return &instance;
}

void MobaStoreDataStore::LoadIfNeeded()
{
    if (_loaded)
        return;

    _loaded = true;

    if (QueryResult npcs = WorldDatabase.Query(
        "SELECT CreatureEntry, Map, Team FROM mod_moba_store_npc"))
    {
        do
        {
            Field* fields = npcs->Fetch();

            MobaStoreNpc npc;
            npc.entry    = fields[0].Get<uint32>();
            npc.map      = fields[1].Get<uint32>();
            npc.team     = static_cast<TeamId>(fields[2].Get<uint8>());

            _npcs[npc.entry] = npc;
        } while (npcs->NextRow());
    }
    else
        LOG_ERROR("sql.sql", "MobaStoreDataStore: table `mod_moba_store_npc` is empty or missing.");

    QueryResult menu = WorldDatabase.Query(
        "SELECT Map, TabId, NodeId, IsPurchase, CostCopper FROM mod_moba_store_menu");

    if (!menu)
    {
        LOG_ERROR("sql.sql", "MobaStoreDataStore: table `mod_moba_store_menu` is empty or missing.");
        return;
    }

    _nodes.reserve(menu->GetRowCount());

    // (map, tabId) per row, kept parallel to _nodes: the node struct itself
    // does not carry them, but the lookups below are keyed on them.
    std::vector<std::pair<uint32, uint32>> owners;
    owners.reserve(menu->GetRowCount());

    do
    {
        Field* fields = menu->Fetch();

        MobaStoreNode node;
        node.nodeId     = fields[2].Get<uint32>();
        node.isPurchase = fields[3].Get<uint8>() != 0;
        node.costCopper = fields[4].Get<uint32>();

        _nodes.push_back(node);
        owners.emplace_back(fields[0].Get<uint32>(), fields[1].Get<uint32>());
    } while (menu->NextRow());

    // Build the lookup only after every node is pushed: _nodes was reserve()'d
    // to the exact final row count, so no reallocation happens above and these
    // pointers stay stable (same rule as MobaTowerDataStore).
    for (std::size_t i = 0; i < _nodes.size(); ++i)
    {
        MobaStoreNode const& node = _nodes[i];
        auto const& [map, tabId] = owners[i];

        _byNode[MakeKey(map, tabId, node.nodeId)] = &node;
    }

    if (QueryResult grants = WorldDatabase.Query(
        "SELECT Map, TabId, NodeId, ItemEntry, SuffixId, Count FROM mod_moba_store_grant"))
    {
        do
        {
            Field* fields = grants->Fetch();

            uint32 map      = fields[0].Get<uint32>();
            uint32 tabId    = fields[1].Get<uint32>();
            uint32 nodeId   = fields[2].Get<uint32>();

            MobaStoreGrant grant;
            grant.itemEntry = fields[3].Get<uint32>();
            grant.suffixId  = fields[4].Get<uint32>();
            grant.count     = fields[5].Get<uint32>();

            _grants[MakeKey(map, tabId, nodeId)].push_back(grant);
        } while (grants->NextRow());
    }
    else
        LOG_ERROR("sql.sql", "MobaStoreDataStore: table `mod_moba_store_grant` is empty or missing.");

    if (QueryResult sells = WorldDatabase.Query(
        "SELECT Map, ItemEntry, Copper FROM mod_moba_store_sell"))
    {
        do
        {
            Field* fields = sells->Fetch();
            _sellByItem[MakeItemKey(fields[0].Get<uint32>(), fields[1].Get<uint32>())] =
                fields[2].Get<uint32>();
        } while (sells->NextRow());
    }
    else
        LOG_ERROR("sql.sql", "MobaStoreDataStore: table `mod_moba_store_sell` is empty or missing.");

    LOG_INFO("server.loading", ">> Loaded {} MOBA shopkeeper(s), {} catalog node(s), {} sell price(s).",
             _npcs.size(), _nodes.size(), _sellByItem.size());
}

void MobaStoreDataStore::CollectSuffixedEntries(uint32 map, std::set<uint32>& out) const
{
    // MakeKey packs map into the high bits; unpack rather than keeping a second index.
    for (auto const& itr : _grants)
    {
        if (uint32(itr.first >> 40) != map)
            continue;

        for (MobaStoreGrant const& grant : itr.second)
            if (grant.suffixId)
                out.insert(grant.itemEntry);
    }
}

void MobaStoreDataStore::CollectEntries(uint32 map, std::set<uint32>& out) const
{
    for (auto const& itr : _grants)
    {
        if (uint32(itr.first >> 40) != map)
            continue;

        for (MobaStoreGrant const& grant : itr.second)
            out.insert(grant.itemEntry);
    }
}

MobaStoreNpc const* MobaStoreDataStore::GetNpc(uint32 creatureEntry) const
{
    auto itr = _npcs.find(creatureEntry);
    return itr != _npcs.end() ? &itr->second : nullptr;
}

MobaStoreNode const* MobaStoreDataStore::GetNode(uint32 map, uint32 tabId, uint32 nodeId) const
{
    auto itr = _byNode.find(MakeKey(map, tabId, nodeId));
    return itr != _byNode.end() ? itr->second : nullptr;
}

std::vector<MobaStoreGrant> const* MobaStoreDataStore::GetGrants(uint32 map, uint32 tabId, uint32 nodeId) const
{
    auto itr = _grants.find(MakeKey(map, tabId, nodeId));
    return itr != _grants.end() ? &itr->second : nullptr;
}

bool MobaStoreDataStore::GetSellValue(uint32 map, uint32 itemEntry, uint32& out) const
{
    auto itr = _sellByItem.find(MakeItemKey(map, itemEntry));
    if (itr == _sellByItem.end())
        return false;

    out = itr->second;
    return true;
}
