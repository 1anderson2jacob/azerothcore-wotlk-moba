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

void BattlegroundMOBAScore::BuildObjectivesBlock(WorldPacket& data)
{
    data << uint32(1); // Objectives Count
    data << uint32(0);
}

BattlegroundMOBA::BattlegroundMOBA()
{
    m_BuffChange = true;
    BgObjects.resize(BG_MOBA_OBJECT_MAX);
    BgCreatures.resize(BG_MOBA_CREATURES_MAX);
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
    // doors
    AddObject(BG_MOBA_OBJECT_DOOR_A, BG_OBJECT_A_DOOR_EY_ENTRY, 2527.6f, 1596.91f, 1262.13f, -3.12414f, -0.173642f, -0.001515f, 0.98477f, -0.008594f, RESPAWN_IMMEDIATELY);
    AddObject(BG_MOBA_OBJECT_DOOR_H, BG_OBJECT_H_DOOR_EY_ENTRY, 1803.21f, 1539.49f, 1261.09f, 3.14159f, 0.173648f, 0, 0.984808f, 0, RESPAWN_IMMEDIATELY);

    GraveyardStruct const* sg = nullptr;
    sg = sGraveyard->GetGraveyard(BG_MOBA_GRAVEYARD_MAIN_ALLIANCE);
    AddSpiritGuide(BG_MOBA_SPIRIT_MAIN_ALLIANCE, sg->x, sg->y, sg->z, 3.124139f, TEAM_ALLIANCE);

    sg = sGraveyard->GetGraveyard(BG_MOBA_GRAVEYARD_MAIN_HORDE);
    AddSpiritGuide(BG_MOBA_SPIRIT_MAIN_HORDE, sg->x, sg->y, sg->z, 3.193953f, TEAM_HORDE);

    for (uint32 i = BG_MOBA_OBJECT_DOOR_A; i < BG_MOBA_OBJECT_MAX; ++i)
        if (!BgObjects[i])
        {
            LOG_ERROR("sql.sql", "BattlegroundMOBA: object slot {} failed to spawn, battleground not created!", i);
            return false;
        }

    for (uint32 i = BG_MOBA_SPIRIT_MAIN_ALLIANCE; i <= BG_MOBA_SPIRIT_MAIN_HORDE; ++i)
        if (!BgCreatures[i])
        {
            LOG_ERROR("sql.sql", "BattlegroundMOBA: Failed to spawn spirit guides Battleground not created!");
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