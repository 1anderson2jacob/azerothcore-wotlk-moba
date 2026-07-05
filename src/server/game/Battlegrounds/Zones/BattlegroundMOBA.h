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
    BG_MOBA_SPIRIT_MAIN_ALLIANCE    = 0,
    BG_MOBA_SPIRIT_MAIN_HORDE       = 1,

    BG_MOBA_CREATURES_MAX           = 2
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

struct BattlegroundMOBAScore final : public BattlegroundScore
{
    friend class BattlegroundMOBA;

protected:
    BattlegroundMOBAScore(ObjectGuid playerGuid) : BattlegroundScore(playerGuid) { }

    void BuildObjectivesBlock(WorldPacket& data) final;
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
    GraveyardStruct const* GetClosestGraveyard(Player* player) override;
    bool SetupBattleground() override;
    void Init() override;
    void EndBattleground(TeamId winnerTeamId) override;
    bool UpdatePlayerScore(Player* player, uint32 type, uint32 value, bool doAddHonor = true) override;
    void FillInitialWorldStates(WorldPackets::WorldState::InitWorldStates& packet) override;

private:
    void PostUpdateImpl(uint32 diff) override;

    EventMap _bgEvents;
};
#endif
