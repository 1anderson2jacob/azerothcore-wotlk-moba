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

#ifndef MOBA_STORE_DATA_H
#define MOBA_STORE_DATA_H

#include "Common.h"
#include "SharedDefines.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <set>

// The shopkeeper NPC. One creature entry per team; the team lives here rather than
// in the creature's faction because CFBG puts players of either faction on either
// BG team, so faction cannot express team membership.
struct MobaStoreNpc
{
    uint32 entry = 0;
    uint32 map = 0;
    TeamId team = TEAM_ALLIANCE;
};

// One catalog node. Labels, parents and ordering exist only to DRAW the shop, and
// the addon ships that in its generated Catalog.lua -- the server keeps only what
// it needs to validate a purchase named by node id.
struct MobaStoreNode
{
    uint32 nodeId = 0;
    bool isPurchase = false;
    uint32 costCopper = 0;
};

// One item a purchase node hands over. A starting-gear bundle is many of these
// sharing a node; a fixed-item tab has exactly one.
struct MobaStoreGrant
{
    uint32 itemEntry = 0;
    uint32 suffixId = 0;        // ItemRandomSuffix id, positive; the engine wants it negated
    uint32 count = 1;
};

// Loads mod_moba_store.sql's three tables once per worldserver process, shared
// by every npc_moba_store instance. A SQL change needs a full restart.
class MobaStoreDataStore
{
public:
    static MobaStoreDataStore* instance();

    void LoadIfNeeded();

    MobaStoreNpc const* GetNpc(uint32 creatureEntry) const;
    MobaStoreNode const* GetNode(uint32 map, uint32 tabId, uint32 nodeId) const;
    std::vector<MobaStoreGrant> const* GetGrants(uint32 map, uint32 tabId, uint32 nodeId) const;
    // Every item entry this map's shop hands out with a random suffix. The shop
    // addon needs each one's suffix factor: the client multiplies a suffix's
    // allocation by it to get real stat values, and it lives in RandPropPoints.dbc
    // with no Lua accessor, so an addon-built item link renders +0 without it.
    void CollectSuffixedEntries(uint32 map, std::set<uint32>& out) const;

    // Every item entry this map's shop can hand out. Usability is the server's
    // verdict, so it needs the full set to tell the addon what to grey.
    void CollectEntries(uint32 map, std::set<uint32>& out) const;

    // What ONE UNIT of an item sells back for on this map. False = the shop never
    // sold it, so npc_moba_store refuses the sale; true with out == 0 means it was
    // free and refunds nothing.
    bool GetSellValue(uint32 map, uint32 itemEntry, uint32& out) const;

private:
    MobaStoreDataStore() = default;

    // Node ids are per-tab and small, so one composite key beats three
    // levels of nested maps.
    static uint64 MakeKey(uint32 map, uint32 tabId, uint32 id)
    {
        return (static_cast<uint64>(map) << 40) | (static_cast<uint64>(tabId) << 20) | id;
    }

    // Sell prices are per (map, item) with no tab or node, so they cannot share
    // MakeKey's node-shaped packing.
    static uint64 MakeItemKey(uint32 map, uint32 itemEntry)
    {
        return (static_cast<uint64>(map) << 32) | itemEntry;
    }

    bool _loaded = false;
    std::unordered_map<uint32, MobaStoreNpc> _npcs;
    std::vector<MobaStoreNode> _nodes;
    std::unordered_map<uint64, MobaStoreNode const*> _byNode;
    std::unordered_map<uint64, std::vector<MobaStoreGrant>> _grants;
    std::unordered_map<uint64, uint32> _sellByItem;
};

#define sMobaStoreDataStore MobaStoreDataStore::instance()

#endif
