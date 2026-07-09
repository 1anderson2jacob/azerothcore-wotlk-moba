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

#include "BattlegroundMOBA.h"
#include "BattlegroundMgr.h"
#include "Creature.h"
#include "GameGraveyard.h"
#include "GameTime.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Util.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "WorldStatePackets.h"
#include "MobaTowerData.h"
#include "MobaCreepData.h"
#include "ObjectAccessor.h"
#include "TemporarySummon.h"
#include "WaypointMgr.h"
#include "MotionMaster.h"
#include <algorithm>

void BattlegroundMOBAScore::BuildObjectivesBlock(WorldPacket& data)
{
    data << uint32(1); // Objectives Count
    data << uint32(0);
}

BattlegroundMOBA::BattlegroundMOBA()
{
    m_BuffChange = true;
    BgObjects.resize(BG_MOBA_OBJECT_MAX);
}

BattlegroundMOBA::~BattlegroundMOBA()
{
}

void BattlegroundMOBA::PostUpdateImpl(uint32 diff)
{
    if (GetStatus() == STATUS_IN_PROGRESS)
    {
        _bgEvents.Update(diff);
        while (uint32 eventId = _bgEvents.ExecuteEvent())
            switch (eventId)
            {
                case EVENT_MOBA_SPAWN_WAVE:
                {
                    ++_waveCount;
                    bool includeSiege = (_waveCount % 3 == 0);
                    SpawnWave(TEAM_ALLIANCE, includeSiege);
                    SpawnWave(TEAM_HORDE, includeSiege);
                    _bgEvents.ScheduleEvent(EVENT_MOBA_SPAWN_WAVE, Milliseconds(30000));
                    break;
                }
            }
    }
}

void BattlegroundMOBA::StartingEventCloseDoors()
{
    SpawnBGObject(BG_MOBA_OBJECT_DOOR_A, RESPAWN_IMMEDIATELY);
    SpawnBGObject(BG_MOBA_OBJECT_DOOR_H, RESPAWN_IMMEDIATELY);
}

void BattlegroundMOBA::StartingEventOpenDoors()
{
    SpawnBGObject(BG_MOBA_OBJECT_DOOR_A, RESPAWN_ONE_DAY);
    SpawnBGObject(BG_MOBA_OBJECT_DOOR_H, RESPAWN_ONE_DAY);

    // Achievement: Flurry
    StartTimedAchievement(ACHIEVEMENT_TIMED_TYPE_EVENT, BG_MOBA_EVENT_START_BATTLE);

    _bgEvents.ScheduleEvent(EVENT_MOBA_SPAWN_WAVE, Milliseconds(30000));
}

void BattlegroundMOBA::EndBattleground(TeamId winnerTeamId)
{
    Battleground::EndBattleground(winnerTeamId);
}

void BattlegroundMOBA::AddPlayer(Player* player)
{
    Battleground::AddPlayer(player);
    PlayerScores.emplace(player->GetGUID().GetCounter(), new BattlegroundMOBAScore(player->GetGUID()));
}

void BattlegroundMOBA::RemovePlayer(Player* /*player*/)
{
}

void BattlegroundMOBA::HandleAreaTrigger(Player* player, uint32 trigger)
{
    if (GetStatus() != STATUS_IN_PROGRESS || !player->IsAlive())
        return;
}

bool BattlegroundMOBA::SetupBattleground()
{
    sMobaTowerDataStore->LoadIfNeeded();
    std::vector<MobaTowerConfig> const& towerConfigs = sMobaTowerDataStore->GetAll();
    if (towerConfigs.empty())
    {
        LOG_ERROR("sql.sql", "BattlegroundMOBA: `mod_moba_tower_data` has no rows, battleground not created!");
        return false;
    }

    // Must resize before any AddCreature/AddSpiritGuide call below.
    BgCreatures.resize(BG_MOBA_CREATURE_FIXED_MAX + towerConfigs.size());

    // doors (ground-level starting areas)
    AddObject(BG_MOBA_OBJECT_DOOR_A, BG_OBJECT_A_DOOR_EY_ENTRY, 2387.529f, 1587.426f, 1174.763f, 3.0222116f, 0.0f, 0.0f, 0.998219f, 0.059655f, RESPAWN_IMMEDIATELY);
    AddObject(BG_MOBA_OBJECT_DOOR_H, BG_OBJECT_H_DOOR_EY_ENTRY, 1942.9327f, 1547.6229f, 1176.458f, 0.32122585f, 0.0f, 0.0f, 0.159923f, 0.987129f, RESPAWN_IMMEDIATELY);

    GraveyardStruct const* sg = nullptr;
    sg = sGraveyard->GetGraveyard(BG_MOBA_GRAVEYARD_MAIN_ALLIANCE);
    AddSpiritGuide(BG_MOBA_SPIRIT_MAIN_ALLIANCE, sg->x, sg->y, sg->z, 3.0222116f, TEAM_ALLIANCE);

    sg = sGraveyard->GetGraveyard(BG_MOBA_GRAVEYARD_MAIN_HORDE);
    AddSpiritGuide(BG_MOBA_SPIRIT_MAIN_HORDE, sg->x, sg->y, sg->z, 0.32122585f, TEAM_HORDE);

    // towers (data-driven; see mod_moba_tower_data / MobaTowerData.h)
    _towers.clear();
    _towers.reserve(towerConfigs.size());
    for (size_t i = 0; i < towerConfigs.size(); ++i)
    {
        MobaTowerConfig const& cfg = towerConfigs[i];
        uint32 slot = BG_MOBA_CREATURE_FIXED_MAX + static_cast<uint32>(i);
        AddCreature(cfg.entry, slot, cfg.x, cfg.y, cfg.z, cfg.o);

        MobaTowerState state;
        state.entry = cfg.entry;
        state.team = cfg.team;
        state.tier = cfg.tier;
        state.guardedByEntry = cfg.guardedByEntry;

        if (Creature* creature = GetBGCreature(slot))
        {
            state.guid = creature->GetGUID();

            // Inert/guarded towers start unattackable until their guard tower falls (see HandleKillUnit).
            if (cfg.guardedByEntry)
                creature->SetUnitFlag(UnitFlags(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE));
        }

        _towers.push_back(state);
    }

    for (uint32 i = BG_MOBA_OBJECT_DOOR_A; i < BG_MOBA_OBJECT_MAX; ++i)
        if (!BgObjects[i])
        {
            LOG_ERROR("sql.sql", "BattlegroundMOBA: object slot {} failed to spawn, battleground not created!", i);
            return false;
        }

    for (uint32 i = BG_MOBA_SPIRIT_MAIN_ALLIANCE; i < BG_MOBA_CREATURE_FIXED_MAX; ++i)
        if (!BgCreatures[i])
        {
            LOG_ERROR("sql.sql", "BattlegroundMOBA: creature slot {} failed to spawn, battleground not created!", i);
            return false;
        }

    for (MobaTowerState const& tower : _towers)
        if (!tower.guid)
        {
            LOG_ERROR("sql.sql", "BattlegroundMOBA: tower entry {} failed to spawn, battleground not created!", tower.entry);
            return false;
        }

    // creep wave composition (data-driven; see mod_moba_creep_data / MobaCreepData.h)
    sMobaCreepDataStore->LoadIfNeeded();
    for (MobaCreepConfig const& cfg : sMobaCreepDataStore->GetAll())
    {
        MobaWaveComposition& comp = _waveComposition[cfg.team];
        switch (cfg.role)
        {
            case MOBA_CREEP_ROLE_MELEE:
                if (!comp.meleeEntry)
                    comp.meleeEntry = cfg.entry;
                else
                    comp.meleeEntry2 = cfg.entry;
                break;
            case MOBA_CREEP_ROLE_CASTER: comp.casterEntry = cfg.entry; break;
            case MOBA_CREEP_ROLE_SIEGE:  comp.siegeEntry  = cfg.entry; break;
        }
    }

    for (uint32 team = 0; team < 2; ++team)
    {
        if (!_waveComposition[team].meleeEntry || !_waveComposition[team].meleeEntry2 || !_waveComposition[team].casterEntry)
        {
            LOG_ERROR("sql.sql", "BattlegroundMOBA: team {} is missing a melee or caster entry in `mod_moba_creep_data`, battleground not created!", team);
            return false;
        }

        if (!_waveComposition[team].siegeEntry)
            LOG_WARN("sql.sql", "BattlegroundMOBA: team {} has no siege entry in `mod_moba_creep_data` -- siege waves will be skipped for that team.", team);
    }

    return true;
}

void BattlegroundMOBA::Init()
{
    //call parent's class reset
    Battleground::Init();

    _bgEvents.Reset();
    _waveCount = 0;
}

void BattlegroundMOBA::HandleKillPlayer(Player* player, Player* killer)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    Battleground::HandleKillPlayer(player, killer);
}

void BattlegroundMOBA::HandleKillUnit(Creature* creature, Player* killer)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    OnTowerDestroyed(creature, killer->GetTeamId());
}

void BattlegroundMOBA::OnTowerDestroyed(Creature* tower, TeamId winnerTeamId)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    auto itr = std::find_if(_towers.begin(), _towers.end(), [&tower](MobaTowerState const& t)
    {
        return t.guid == tower->GetGUID();
    });

    if (itr == _towers.end() || itr->destroyed)
        return;

    itr->destroyed = true;

    // Unlock any towers this one was guarding.
    for (MobaTowerState& other : _towers)
    {
        if (other.guardedByEntry != itr->entry || other.destroyed)
            continue;

        if (Creature* guarded = ObjectAccessor::GetCreature(*tower, other.guid))
            guarded->RemoveUnitFlag(UnitFlags(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE));
    }

    m_TeamScores[winnerTeamId]++;
    UpdateWorldState(winnerTeamId == TEAM_ALLIANCE ? WORLD_STATE_BATTLEGROUND_EY_ALLIANCE_RESOURCES : WORLD_STATE_BATTLEGROUND_EY_HORDE_RESOURCES,
        static_cast<uint32>(m_TeamScores[winnerTeamId]));

    TeamId loserTeamId = itr->team;
    bool anyTowersRemaining = std::any_of(_towers.begin(), _towers.end(), [loserTeamId](MobaTowerState const& t)
    {
        return t.team == loserTeamId && !t.destroyed;
    });

    if (!anyTowersRemaining)
    {
        FreezeAllCreeps();
        EndBattleground(winnerTeamId);
    }
}

void BattlegroundMOBA::SpawnWave(TeamId team, bool includeSiege)
{
    MobaWaveComposition const& comp = _waveComposition[team];

    SpawnCreep(comp.meleeEntry);
    SpawnCreep(comp.meleeEntry2);
    SpawnCreep(comp.casterEntry);

    if (includeSiege && comp.siegeEntry)
        SpawnCreep(comp.siegeEntry);
}


void BattlegroundMOBA::SpawnCreep(uint32 entry)
{
    MobaCreepConfig const* cfg = sMobaCreepDataStore->GetConfig(entry);
    if (!cfg)
        return;

    WaypointPath const* path = sWaypointMgr->GetPath(cfg->pathId);
    if (!path || path->Nodes.empty())
        return;

    WaypointNode const& start = path->Nodes.front();
    Position pos(start.X, start.Y, start.Z);

    if (TempSummon* summon = GetBgMap()->SummonCreature(entry, pos, nullptr, cfg->despawnMs))
    {
        summon->SetTempSummonType(TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT);
        _spawnedCreeps.push_back(summon->GetGUID());
    }
}

void BattlegroundMOBA::FreezeAllCreeps()
{
    for (ObjectGuid const& guid : _spawnedCreeps)
    {
        Creature* creep = GetBgMap()->GetCreature(guid);
        if (!creep || !creep->IsAlive())
            continue;

        creep->CombatStop();
        creep->SetReactState(REACT_PASSIVE);
        creep->GetMotionMaster()->MoveIdle();
    }
}

bool BattlegroundMOBA::UpdatePlayerScore(Player* player, uint32 type, uint32 value, bool doAddHonor)
{
    if (!Battleground::UpdatePlayerScore(player, type, value, doAddHonor))
        return false;

    switch (type)
    {
        default:
            break;
    }

    return true;
}

void BattlegroundMOBA::FillInitialWorldStates(WorldPackets::WorldState::InitWorldStates& packet)
{
    packet.Worldstates.reserve(2);
    packet.Worldstates.emplace_back(WORLD_STATE_BATTLEGROUND_EY_HORDE_RESOURCES, GetTeamScore(TEAM_HORDE));
    packet.Worldstates.emplace_back(WORLD_STATE_BATTLEGROUND_EY_ALLIANCE_RESOURCES, GetTeamScore(TEAM_ALLIANCE));
}

GraveyardStruct const* BattlegroundMOBA::GetClosestGraveyard(Player* player)
{
    return sGraveyard->GetGraveyard(player->GetTeamId() == TEAM_ALLIANCE
        ? BG_MOBA_GRAVEYARD_MAIN_ALLIANCE
        : BG_MOBA_GRAVEYARD_MAIN_HORDE);
}
