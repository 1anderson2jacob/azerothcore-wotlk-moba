#ifndef MOBA_STORE_DATA_H
#define MOBA_STORE_DATA_H

#include "Common.h"
#include "SharedDefines.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <set>

// One creature entry per team. The team lives here rather than in the creature's
// faction because CFBG puts either faction on either BG team.
struct MobaStoreNpc
{
    uint32 entry = 0;
    uint32 map = 0;
    TeamId team = TEAM_ALLIANCE;
};

// Labels, parents and ordering exist only to DRAW the shop and live in the addon's
// generated Catalog.lua. The server keeps only what validates a purchase by node id.
struct MobaStoreNode
{
    uint32 nodeId = 0;
    bool isPurchase = false;
    uint32 costCopper = 0;
};

// A starting-gear bundle is many of these sharing a node; a fixed-item tab has one.
struct MobaStoreGrant
{
    uint32 itemEntry = 0;
    uint32 suffixId = 0;        // ItemRandomSuffix id, positive; the engine wants it negated
    uint32 count = 1;
};

// Loads mod_moba_store.sql's tables once per process. A SQL change needs a restart.
class MobaStoreDataStore
{
public:
    static MobaStoreDataStore* instance();

    void LoadIfNeeded();

    MobaStoreNpc const* GetNpc(uint32 creatureEntry) const;
    MobaStoreNode const* GetNode(uint32 map, uint32 tabId, uint32 nodeId) const;
    std::vector<MobaStoreGrant> const* GetGrants(uint32 map, uint32 tabId, uint32 nodeId) const;
    // The addon needs each one's suffix factor: it lives in RandPropPoints.dbc with no
    // Lua accessor, so an addon-built item link renders +0 without it.
    void CollectSuffixedEntries(uint32 map, std::set<uint32>& out) const;

    // Usability is the server's verdict, so it needs the full set to tell the addon
    // what to grey.
    void CollectEntries(uint32 map, std::set<uint32>& out) const;

    // What ONE UNIT sells back for. False = the shop never sold it, so the sale is
    // refused; true with out == 0 means it was free and refunds nothing.
    bool GetSellValue(uint32 map, uint32 itemEntry, uint32& out) const;

    // The hash of this map's block in the addon's Catalog.lua, as gen_store.py wrote it.
    // Empty when the map has no row, and the addon then skips the check rather than
    // locking itself out.
    std::string GetCatalogVersion(uint32 map) const;

private:
    MobaStoreDataStore() = default;

    // Node ids are per-tab and small, so one composite key beats nested maps.
    static uint64 MakeKey(uint32 map, uint32 tabId, uint32 id)
    {
        return (static_cast<uint64>(map) << 40) | (static_cast<uint64>(tabId) << 20) | id;
    }

    // Per (map, item) with no tab or node, so it cannot share MakeKey's packing.
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
    std::unordered_map<uint32, std::string> _catalogVersions;
};

#define sMobaStoreDataStore MobaStoreDataStore::instance()

#endif
