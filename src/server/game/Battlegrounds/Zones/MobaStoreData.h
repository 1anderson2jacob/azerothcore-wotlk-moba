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
#include <unordered_set>
#include <vector>

// A vendor NPC. One creature entry per team; the team lives here rather than in
// the creature's faction because CFBG puts players of either faction on either
// BG team, so faction cannot express team membership.
struct MobaStoreNpc
{
    uint32 entry = 0;
    uint32 map = 0;
    TeamId team = TEAM_ALLIANCE;
    uint32 vendorId = 0;
};

// One gossip node: either a category (has children, no grants) or a purchase
// (has grants). Depth is unbounded -- ParentId chains upward to 0.
struct MobaStoreNode
{
    uint32 nodeId = 0;
    uint32 parentId = 0;        // 0 = top level
    std::string label;
    bool isPurchase = false;
    uint32 costCopper = 0;
};

// One item a purchase node hands over. A starting-gear bundle is many of these
// sharing a node; a fixed-item vendor has exactly one.
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
    MobaStoreNode const* GetNode(uint32 map, uint32 vendorId, uint32 nodeId) const;
    // Children of parentId (0 = top level), in config order. Null if none.
    std::vector<MobaStoreNode const*> const* GetChildren(uint32 map, uint32 vendorId, uint32 parentId) const;
    std::vector<MobaStoreGrant> const* GetGrants(uint32 map, uint32 vendorId, uint32 nodeId) const;
    // Every item entry this map's vendors can hand out, so the battleground can
    // strip shop gear when a player leaves.
    std::unordered_set<uint32> const* GetCatalogItems(uint32 map) const;

private:
    MobaStoreDataStore() = default;

    // Node ids are per-vendor and small, so one composite key beats three
    // levels of nested maps.
    static uint64 MakeKey(uint32 map, uint32 vendorId, uint32 id)
    {
        return (static_cast<uint64>(map) << 40) | (static_cast<uint64>(vendorId) << 20) | id;
    }

    bool _loaded = false;
    std::unordered_map<uint32, MobaStoreNpc> _npcs;
    std::vector<MobaStoreNode> _nodes;
    std::unordered_map<uint64, MobaStoreNode const*> _byNode;
    std::unordered_map<uint64, std::vector<MobaStoreNode const*>> _byParent;
    std::unordered_map<uint64, std::vector<MobaStoreGrant>> _grants;
    std::unordered_map<uint32, std::unordered_set<uint32>> _catalogByMap;
};

#define sMobaStoreDataStore MobaStoreDataStore::instance()

#endif
