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
#include "ObjectAccessor.h"
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

    return true;
}


void BattlegroundMOBA::Init()
{
    //call parent's class reset
    Battleground::Init();

    _bgEvents.Reset();
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

    auto itr = std::find_if(_towers.begin(), _towers.end(), [&creature](MobaTowerState const& tower)
    {
        return tower.guid == creature->GetGUID();
    });

    if (itr == _towers.end())
        return;

    itr->destroyed = true;

    // Unlock any towers this one was guarding.
    for (MobaTowerState& tower : _towers)
    {
        if (tower.guardedByEntry != itr->entry || tower.destroyed)
            continue;

        if (Creature* guarded = ObjectAccessor::GetCreature(*creature, tower.guid))
            guarded->RemoveUnitFlag(UnitFlags(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE));
    }

    TeamId winnerTeamId = killer->GetTeamId();
    m_TeamScores[winnerTeamId]++;
    UpdateWorldState(winnerTeamId == TEAM_ALLIANCE ? WORLD_STATE_BATTLEGROUND_EY_ALLIANCE_RESOURCES : WORLD_STATE_BATTLEGROUND_EY_HORDE_RESOURCES,
        static_cast<uint32>(m_TeamScores[winnerTeamId]));

    // Known limitation: only fires on player-attributed kills; once lane
    // creeps exist and might land the killing blow, this won't fire for them.
    TeamId loserTeamId = itr->team;
    bool anyTowersRemaining = std::any_of(_towers.begin(), _towers.end(), [loserTeamId](MobaTowerState const& tower)
    {
        return tower.team == loserTeamId && !tower.destroyed;
    });

    if (!anyTowersRemaining)
        EndBattleground(winnerTeamId);
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