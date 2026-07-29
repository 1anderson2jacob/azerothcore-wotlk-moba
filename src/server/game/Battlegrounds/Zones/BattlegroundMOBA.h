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

#ifndef __BATTLEGROUNDMOBA_H
#define __BATTLEGROUNDMOBA_H

#include "Battleground.h"
#include "BattlegroundScore.h"
#include "EventMap.h"
#include "WorldStateDefines.h"
#include "ObjectGuid.h"
#include "MobaNeutralData.h"
#include "MobaTowerData.h"
#include "MobaPlayerDropData.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>

// Shared with the client addon (client/addons/MobaHUD). The server sends
// "<prefix>\t<payload>" as a LANG_ADDON chat message; the 3.3.5a client splits on
// the TAB into (prefix, payload) for CHAT_MSG_ADDON. Two namespaces over one
// transport: the shop panel's payloads never mix with the HUD bar's.
constexpr char MOBA_HUD_ADDON_PREFIX[]  = "MobaHUD";
constexpr char MOBA_SHOP_ADDON_PREFIX[] = "MobaShop";

enum BG_MOBA_Graveyards
{
    BG_MOBA_GRAVEYARD_MAIN_ALLIANCE     = 1103,
    BG_MOBA_GRAVEYARD_MAIN_HORDE        = 1104,
};

enum BG_MOBA_CreatureTypes
{
    BG_MOBA_CREATURE_FIXED_MAX      = 0 // no fixed creatures; towers occupy dynamic slots from 0, see SetupBattleground()
};

enum BG_MOBA_ObjectTypes
{
    BG_MOBA_OBJECT_DOOR_A                         = 0,
    BG_MOBA_OBJECT_DOOR_H                         = 1,
    BG_MOBA_OBJECT_MAX                            = 2
};

enum BG_MOBA_Score
{
    BG_MOBA_EVENT_START_BATTLE            = 13180, // Achievement: Flurry
};

// _bgEvents event IDs.
enum BG_MOBA_Events
{
    EVENT_MOBA_SPAWN_WAVE = 1,
    // Two ranges keyed by a small container index:
    //   EVENT_MOBA_SPAWN_CAMP_FIRST + camp index (into _camps)   -- jungle camp (re)spawn.
    //   EVENT_MOBA_RESPAWN_INHIB_FIRST + tower index (into _towers) -- inhibitor return.
    // PostUpdateImpl dispatches by threshold, so keep the two bases far apart and
    // above any realistic camp/tower count.
    EVENT_MOBA_SPAWN_CAMP_FIRST    = 100,
    EVENT_MOBA_RESPAWN_INHIB_FIRST = 1000
};

enum BG_MOBA_Recall
{
    BG_MOBA_RECALL_SPELL        = 8690,  // Hearthstone; redirected to base by moba_recall.cpp
    BG_MOBA_RECALL_ITEM         = 6948,  // Hearthstone item; granted in AddPlayer
    BG_MOBA_RECALL_EMPOWER_AURA = 1243   // PLACEHOLDER empower trigger (Power Word: Fortitude R1); swap for the real mechanic
};

// Tracks a spawned tower's registry data: which team it belongs to, its
// tier/guard dependency, its structure kind, and whether it's been destroyed.
// Populated from `mod_moba_tower_data` (see MobaTowerData.h) in SetupBattleground().
struct MobaTowerState
{
    ObjectGuid guid;
    uint32 entry = 0;
    TeamId team = TEAM_ALLIANCE;
    uint8 tier = 0;
    uint32 guardedByEntry = 0;
    uint8 kind = MOBA_STRUCTURE_TOWER;
    uint32 respawnMs = 0;
    bool destroyed = false;
};

// Cached once in SetupBattleground() from mod_moba_creep_data: which entry
// to spawn for each role, per team, so wave-spawn doesn't need to re-query.
struct MobaWaveComposition
{
    uint32 meleeEntry = 0;
    uint32 meleeEntry2 = 0;
    uint32 casterEntry = 0;
    uint32 siegeEntry = 0; // 0 = not configured, skip even on siege waves
    uint32 superEntry = 0; // 0 = none; spawned per wave while the enemy inhibitor is down
};

// Runtime state of one neutral (jungle) camp: the static member list from
// mod_moba_neutral_members plus the live summon GUIDs, rebuilt on every
// (re)spawn. aliveCount hitting 0 schedules the camp's respawn event.
struct MobaCampState
{
    uint32 campId = 0;
    uint32 initialSpawnMs = 0;
    uint32 respawnMs = 0;
    std::vector<MobaNeutralMember> members;
    std::vector<ObjectGuid> memberGuids;
    uint32 aliveCount = 0;
};

// Per-player LoL-style respawn countdown, started on Release Spirit and ticked
// down in PostUpdateImpl. remainingMs hits 0 -> teleport to team start + revive.
// The remaining time is mirrored on the client HUD via the "R:" payload.
struct MobaRespawnState
{
    uint32 remainingMs = 0;
};

struct BattlegroundMOBAScore final : public BattlegroundScore
{
    friend class BattlegroundMOBA;

protected:
    BattlegroundMOBAScore(ObjectGuid playerGuid) : BattlegroundScore(playerGuid) { }

    void BuildObjectivesBlock(WorldPacket& data) final;

    uint32 CreepKills = 0;
};

class Item;

class AC_GAME_API BattlegroundMOBA : public Battleground
{
public:
    BattlegroundMOBA();
    ~BattlegroundMOBA() override;

    /* inherited from BattlegroundClass */
    void AddPlayer(Player* player) override;
    void StartingEventCloseDoors() override;
    void StartingEventOpenDoors() override;

    void RemovePlayer(Player* player) override;
    void HandleAreaTrigger(Player* player, uint32 trigger) override;
    void HandleKillPlayer(Player* player, Player* killer) override;
    void HandleKillUnit(Creature* creature, Player* killer) override;
    GraveyardStruct const* GetClosestGraveyard(Player* player) override;
    bool SetupBattleground() override;
    void Init() override;
    void EndBattleground(TeamId winnerTeamId) override;
    bool UpdatePlayerScore(Player* player, uint32 type, uint32 value, bool doAddHonor = true) override;
    void FillInitialWorldStates(WorldPackets::WorldState::InitWorldStates& packet) override;

    std::vector<MobaTowerState>& GetTowers() { return _towers; }

    // Shared by HandleKillUnit (player-attributed tower kills) and
    // npc_moba_tower::JustDied (creature/creep-attributed tower kills) --
    // see npc_moba_tower::JustDied for why both paths exist.
    void OnTowerDestroyed(Creature* tower, TeamId winnerTeamId);

    // Credit a lane-creep last-hit (CS) to the killing-blow player. Called from
    // npc_moba_creep::JustDied -- unlike HandleKillUnit, whose killer is the loot
    // recipient (first player to aggro), this is the true last hit.
    void CreditCreepKill(Player* killer);

    // On-death drops for any minion (lane creep or neutral), called from both
    // JustDied choke points with the killing-blow player (nullptr = none, or
    // rejected by the caller's own policy). Grants buff drops, injects gold
    // into the corpse loot, and re-points native loot rights -- the tapper's
    // group by default -- at the killer's team; with no rewarded killer it
    // strips the corpse instead.
    void GrantDeathDrops(Creature* victim, Player* killer);

    // Grant a resolved kill's drops directly to the killer (buff/gold/item) --
    // no corpse, no native loot, unlike GrantDeathDrops. Called from
    // HandlePlayerDeath once the effective killer is known.
    void GrantPlayerKillDrops(Player* killer);

    // Kill-credit + assist tracking, driven by the moba_kill_credit UnitScript:
    //   RecordPlayerDamage -- an enemy player damaged/debuffed the victim (kill credit + direct assist).
    //   RecordAllyHeal     -- a teammate healed an ally (assist-chain link; no duration gate, overheal counts).
    //   RecordAllyBuff     -- a teammate applied a short combat buff/shield to an ally (assist-chain link).
    //   HandlePlayerDeath  -- victim died (to anything); tally the death, credit the kill, and
    //                         resolve contribution-based assists (fixed-point support chain, see .cpp).
    void RecordPlayerDamage(Player* victim, Player* attacker);
    void RecordAllyHeal(Player* ally, Player* healer);
    void RecordAllyBuff(Player* ally, Player* buffer, int32 buffMaxDurationMs);
    void HandlePlayerDeath(Player* victim, Unit* killer);

    // Remember an item the shop handed a player, so RemovePlayer can destroy
    // exactly those items on exit. Called from npc_moba_store.
    void RecordGrantedItem(Player* player, Item* item);

    // League camp-link: called from npc_moba_neutral::JustEngagedWith so
    // hitting one camp member pulls the rest onto the attacker.
    void PullCampMates(Creature* member, Unit* attacker);

    // Called from npc_moba_neutral::JustDied; starts the camp's respawn timer
    // once its last member is down.
    void NotifyNeutralDied(Creature* member);

    // Starts a player's respawn countdown (called from the OnPlayerReleasedGhost hook).
    void StartRespawnTimer(Player* player);

    // Reply to a MobaHUD client "ready" ping with this player's current HUD state.
    void SendHudStateTo(Player* player);

    // MobaShop handshake and panel handoff. The panel is addon-only: a player who
    // never sent HELLO has no UI to open, so npc_moba_store falls back to gossip.
    void SetShopAddonReady(Player* player);
    bool HasShopAddon(Player* player) const;
    void SendShopMessage(Player* player, std::string const& body);

    // The shopkeeper a player currently has open; ObjectGuid::Empty if none.
    // Remembered server-side so a BUY: never names a vendor -- the client cannot
    // reach one it isn't standing at.
    void SetOpenShopkeeper(Player* player, ObjectGuid creatureGuid);
    ObjectGuid GetOpenShopkeeper(Player* player) const;

    // Per-map recall cast time (ms) for a player currently in a MOBA BG; 0 = no
    // override (use the spell's default). Read by Spell::prepare to retime Hearthstone.
    static uint32 GetRecallCastTimeMs(Player* player);

private:
    void PostUpdateImpl(uint32 diff) override;
    void SpawnWave(TeamId team, bool includeSiege);
    void SpawnCreep(uint32 entry);
    void RespawnInhibitor(uint32 towerIndex);
    void SpawnCamp(uint32 campIndex);
    MobaCampState* FindCampOf(ObjectGuid guid);
    void FreezeAllCreeps();
    void UpdateRespawnTimers(uint32 diff);
    void RespawnAtBase(Player* player);
    void UpdateFountainHealing(uint32 diff);
    Player* ResolveKillCredit(Player* victim, Unit* killer);
    uint32 GetKillCreditWindowMs() const;
    uint32 GetAssistWindowMs() const;
    uint32 GetAssistBuffMaxDurationMs() const;

    // MobaHUD addon feed (client/addons/MobaHUD). `body` is the payload after the
    // "MobaHUD\t" prefix: "T:<sec>" clock start/sync, "E" hide the bar,
    // "S:<ally>,<enemy>,<k>,<d>,<a>,<cs>" scoreboard, "R:<sec>" revive-countdown
    // start (0 = hide), "K:<pov>,<killer>,<kClass>,<kSide>,<victim>,<vClass>,<vSide>"
    // a player kill line, "D:<pov>,<vSide>,<vClass>,<victim>,<cat>" a non-player death
    // line (cat 0=env 1=tower 2=creep 3=neutral). K:/D: are built per recipient.
    void SendAddonPacket(Player* player, char const* prefix, std::string const& body);
    void SendHudMessage(Player* player, std::string const& body);
    void BroadcastHudMessage(std::string const& body);
    void BroadcastKillFeed(Player* killer, Player* victim);
    void BroadcastNonPlayerDeath(Player* victim, Unit* killer);
    uint32 ClassifyKiller(Unit* killer) const;   // 0 env, 1 tower, 2 lane creep, 3 neutral
    void SendScoreboard(Player* player);
    void BroadcastScoreboard();
    std::string BuildScoreboardBody(Player* player) const;

    EventMap _bgEvents;
    std::vector<MobaTowerState> _towers;
    MobaWaveComposition _waveComposition[2];
    bool _superMinionsActive[2] = {false, false}; // per beneficiary team: enemy inhibitor down -> super minions in waves
    std::vector<ObjectGuid> _spawnedCreeps;
    std::vector<MobaCampState> _camps;
    uint32 _waveCount = 0;
    uint32 _matchElapsedMs = 0;   // time since doors opened (excludes prep phase)
    uint32 _hudResyncMs = 0;      // accumulates toward the next periodic HUD re-broadcast
    uint32 _fountainTickMs = 0;   // accumulates toward the next fountain heal tick
    uint32 _teamPlayerKills[2] = {0, 0}; // enemy-player kills per team (the "X vs Y" score)
    // victim GUID -> (enemy-attacker GUID -> last damage/debuff time, ms). Per life:
    // cleared on the victim's death and when they leave. Keeps every recent attacker
    // (not just the latest) so assist-split and bounties can read it later.
    std::unordered_map<ObjectGuid, std::unordered_map<ObjectGuid, uint32>> _recentAttackers;

    // Items the shop handed each player, destroyed when they leave. Keyed by
    // item GUID rather than entry: the shop hands out stock entries while
    // custom_items is off, so an entry-based sweep would also destroy a
    // player's own world-obtained copies.
    std::unordered_map<ObjectGuid, std::vector<ObjectGuid>> _grantedItems;

    // ally GUID -> (supporter GUID -> last heal/short-buff time, ms). Feeds the LoL
    // assist chain: healing or a short combat buff on a kill participant links the
    // supporter to the kill. Same per-life lifetime as _recentAttackers.
    std::unordered_map<ObjectGuid, std::unordered_map<ObjectGuid, uint32>> _allySupport;

    std::unordered_map<ObjectGuid, MobaRespawnState> _respawnTimers;

    // Players whose client announced the shop panel (HELLO). Everyone else gets
    // the gossip fallback, which goes away once the panel ships.
    std::unordered_set<ObjectGuid> _shopAddonPlayers;

    // player GUID -> the shopkeeper creature they currently have open.
    std::unordered_map<ObjectGuid, ObjectGuid> _openShopkeeper;

};
#endif
