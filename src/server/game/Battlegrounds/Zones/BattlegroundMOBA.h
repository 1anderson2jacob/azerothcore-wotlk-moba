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
#include <vector>
#include <unordered_map>
#include <string>

enum BG_MOBA_ObjectEntry
{
    BG_OBJECT_A_DOOR_EY_ENTRY           = 184719,           //Alliance door
    BG_OBJECT_H_DOOR_EY_ENTRY           = 184720,           //Horde door
};

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
    EVENT_MOBA_SPAWN_WAVE = 1
};

enum BG_MOBA_Recall
{
    BG_MOBA_RECALL_SPELL        = 8690,  // Hearthstone; redirected to base by moba_recall.cpp
    BG_MOBA_RECALL_ITEM         = 6948,  // Hearthstone item; granted in AddPlayer
    BG_MOBA_RECALL_EMPOWER_AURA = 1243   // PLACEHOLDER empower trigger (Power Word: Fortitude R1); swap for the real mechanic
};

// Tracks a spawned tower's registry data: which team it belongs to, its
// tier/guard dependency, and whether it's been destroyed. Populated from
// `mod_moba_tower_data` (see MobaTowerData.h) in SetupBattleground().
struct MobaTowerState
{
    ObjectGuid guid;
    uint32 entry = 0;
    TeamId team = TEAM_ALLIANCE;
    uint8 tier = 0;
    uint32 guardedByEntry = 0;
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
};

// Per-player LoL-style respawn countdown, started on Release Spirit and ticked
// down in PostUpdateImpl. remainingMs hits 0 -> teleport to team start + revive.
struct MobaRespawnState
{
    uint32 remainingMs = 0;
    uint32 lastAnnouncedSec = 0;
};

struct BattlegroundMOBAScore final : public BattlegroundScore
{
    friend class BattlegroundMOBA;

protected:
    BattlegroundMOBAScore(ObjectGuid playerGuid) : BattlegroundScore(playerGuid) { }

    void BuildObjectivesBlock(WorldPacket& data) final;

    uint32 CreepKills = 0;
};

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

    // Starts a player's respawn countdown (called from the OnPlayerReleasedGhost hook).
    void StartRespawnTimer(Player* player);

    // Reply to a MobaHUD client "ready" ping with this player's current HUD state.
    void SendHudStateTo(Player* player);

    // Per-map recall cast time (ms) for a player currently in a MOBA BG; 0 = no
    // override (use the spell's default). Read by Spell::prepare to retime Hearthstone.
    static uint32 GetRecallCastTimeMs(Player* player);

private:
    void PostUpdateImpl(uint32 diff) override;
    void SpawnWave(TeamId team, bool includeSiege);
    void SpawnCreep(uint32 entry);
    void FreezeAllCreeps();
    void UpdateRespawnTimers(uint32 diff);
    void RespawnAtBase(Player* player);
    void UpdateFountainHealing(uint32 diff);

    // MobaHUD addon feed (client/addons/MobaHUD). `body` is the payload after the
    // "MobaHUD\t" prefix: "T:<sec>" clock start/sync, "E" hide the bar,
    // "S:<ally>,<enemy>,<k>,<d>,<a>,<cs>" scoreboard update (values are team-relative).
    void SendHudMessage(Player* player, std::string const& body);
    void BroadcastHudMessage(std::string const& body);
    void SendScoreboard(Player* player);
    void BroadcastScoreboard();
    std::string BuildScoreboardBody(Player* player) const;

    EventMap _bgEvents;
    std::vector<MobaTowerState> _towers;
    MobaWaveComposition _waveComposition[2];
    std::vector<ObjectGuid> _spawnedCreeps;
    uint32 _waveCount = 0;
    uint32 _matchElapsedMs = 0;   // time since doors opened (excludes prep phase)
    uint32 _hudResyncMs = 0;      // accumulates toward the next periodic HUD re-broadcast
    uint32 _fountainTickMs = 0;   // accumulates toward the next fountain heal tick
    uint32 _teamPlayerKills[2] = {0, 0}; // enemy-player kills per team (the "X vs Y" score)
    std::unordered_map<ObjectGuid, MobaRespawnState> _respawnTimers;

};
#endif
