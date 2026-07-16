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
#include "MobaRespawnData.h"
#include "Chat.h"
#include "StringFormat.h"
#include "ObjectAccessor.h"
#include "TemporarySummon.h"
#include "WaypointMgr.h"
#include "MotionMaster.h"
#include <algorithm>
#include "Opcodes.h"

namespace
{
    // Shared with the client-side MobaHUD addon (client/addons/MobaHUD). The server
    // sends "MobaHUD\t<payload>" as a LANG_ADDON chat message; the 3.3.5a client
    // splits on the TAB into (prefix, payload) for the CHAT_MSG_ADDON event.
    constexpr char MOBA_HUD_ADDON_PREFIX[] = "MobaHUD";
    constexpr uint32 MOBA_HUD_RESYNC_MS    = 10000; // re-broadcast cadence for /reload + late joiners
}

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
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    _matchElapsedMs += diff;

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

    UpdateRespawnTimers(diff);

    _hudResyncMs += diff;
    if (_hudResyncMs >= MOBA_HUD_RESYNC_MS)
    {
        _hudResyncMs = 0;
        BroadcastHudMessage(Acore::StringFormat("T:{}", _matchElapsedMs / 1000));
        BroadcastScoreboard();
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

    // Match starts now (doors open): show the HUD bar at 0:00 with a zeroed scoreboard.
    BroadcastHudMessage("T:0");
    BroadcastScoreboard();
}

void BattlegroundMOBA::EndBattleground(TeamId winnerTeamId)
{
    // Hide the client-side HUD bar as the match ends.
    BroadcastHudMessage("E");

    Battleground::EndBattleground(winnerTeamId);
}

void BattlegroundMOBA::AddPlayer(Player* player)
{
    Battleground::AddPlayer(player);
    PlayerScores.emplace(player->GetGUID().GetCounter(), new BattlegroundMOBAScore(player->GetGUID()));

    // Recall (moba_recall.cpp) is triggered by casting Hearthstone, redirected to
    // base while in this BG. Ensure the player is holding one, and clear any
    // pre-existing cooldown so recall is available the moment they enter (each
    // recall cast resets it thereafter -- see spell_moba_hearthstone_recall::HandleTeleport).
    if (!player->HasItemCount(BG_MOBA_RECALL_ITEM))
        player->AddItem(BG_MOBA_RECALL_ITEM, 1);

    player->RemoveSpellCooldown(BG_MOBA_RECALL_SPELL, true);
}

void BattlegroundMOBA::RemovePlayer(Player* player)
{
    // Hide the HUD bar for anyone leaving the match early (Leave button, logout, GM
    // removal). The normal win-condition path hides it via EndBattleground.
    SendHudMessage(player, "E");
}

void BattlegroundMOBA::HandleAreaTrigger(Player* player, uint32 trigger)
{
    if (GetStatus() != STATUS_IN_PROGRESS || !player->IsAlive())
        return;
}

bool BattlegroundMOBA::SetupBattleground()
{
    sMobaTowerDataStore->LoadIfNeeded();
    sMobaRespawnDataStore->LoadIfNeeded();
    std::vector<MobaTowerConfig> towerConfigs = sMobaTowerDataStore->GetForMap(GetMapId());
    if (towerConfigs.empty())
    {
        LOG_ERROR("sql.sql", "BattlegroundMOBA: `mod_moba_tower_data` has no rows for map {}, battleground not created!", GetMapId());
        return false;
    }

    // Must resize before any AddCreature call below.
    BgCreatures.resize(BG_MOBA_CREATURE_FIXED_MAX + towerConfigs.size());

    // doors (ground-level starting areas)
    AddObject(BG_MOBA_OBJECT_DOOR_A, BG_OBJECT_A_DOOR_EY_ENTRY, 2387.529f, 1587.426f, 1174.763f, 3.0222116f, 0.0f, 0.0f, 0.998219f, 0.059655f, RESPAWN_IMMEDIATELY);
    AddObject(BG_MOBA_OBJECT_DOOR_H, BG_OBJECT_H_DOOR_EY_ENTRY, 1942.9327f, 1547.6229f, 1176.458f, 0.32122585f, 0.0f, 0.0f, 0.159923f, 0.987129f, RESPAWN_IMMEDIATELY);

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

    for (MobaTowerState const& tower : _towers)
        if (!tower.guid)
        {
            LOG_ERROR("sql.sql", "BattlegroundMOBA: tower entry {} failed to spawn, battleground not created!", tower.entry);
            return false;
        }

    // creep wave composition (data-driven; see mod_moba_creep_data / MobaCreepData.h)
    sMobaCreepDataStore->LoadIfNeeded();
    for (MobaCreepConfig const& cfg : sMobaCreepDataStore->GetForMap(GetMapId()))
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
            LOG_ERROR("sql.sql", "BattlegroundMOBA: map {} team {} is missing a melee or caster entry in `mod_moba_creep_data`, battleground not created!", GetMapId(), team);
            return false;
        }

        if (!_waveComposition[team].siegeEntry)
            LOG_WARN("sql.sql", "BattlegroundMOBA: map {} team {} has no siege entry in `mod_moba_creep_data` -- siege waves will be skipped for that team.", GetMapId(), team);
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

    Battleground::HandleKillPlayer(player, killer); // updates deaths / killing blows / honorable kills

    // Team kill score (the "X vs Y" segment).
    if (killer && killer != player)
        ++_teamPlayerKills[killer->GetBgTeamId()];

    // Team score plus several players' K/D/A changed -> refresh everyone.
    BroadcastScoreboard();
}

void BattlegroundMOBA::HandleKillUnit(Creature* creature, Player* killer)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    // Lane creep last-hit -> +1 creep score (CS) for the killer. Towers aren't in the
    // creep data store, so this naturally skips them; tower kills fall through to
    // OnTowerDestroyed below.
    if (creature && killer && sMobaCreepDataStore->GetConfig(creature->GetEntry()))
    {
        auto itr = PlayerScores.find(killer->GetGUID().GetCounter());
        if (itr != PlayerScores.end())
        {
            static_cast<BattlegroundMOBAScore*>(itr->second)->CreepKills++;
            SendScoreboard(killer);
        }
    }

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

void BattlegroundMOBA::StartRespawnTimer(Player* player)
{
    if (!player)
        return;

    // One timer per player; re-clicking Release must not restart the countdown.
    if (_respawnTimers.find(player->GetGUID()) != _respawnTimers.end())
        return;

    uint32 baseMs = 10000, perMinMs = 1500, capMs = 60000;
    if (MobaRespawnConfig const* cfg = sMobaRespawnDataStore->GetConfig(GetMapId()))
    {
        baseMs   = cfg->baseMs;
        perMinMs = cfg->perMinMs;
        capMs    = cfg->capMs;
    }

    // Grows continuously with match time (measured from doors-open, so the prep
    // phase is excluded), clamped to the cap.
    uint32 waitMs = std::min<uint32>(capMs,
        baseMs + static_cast<uint32>(static_cast<uint64>(perMinMs) * _matchElapsedMs / 60000));

    MobaRespawnState state;
    state.remainingMs = waitMs;
    _respawnTimers[player->GetGUID()] = state;

    ChatHandler(player->GetSession()).SendSysMessage(
        Acore::StringFormat("You have died. Respawning in {} seconds.", (waitMs + 999) / 1000).c_str());
}

void BattlegroundMOBA::UpdateRespawnTimers(uint32 diff)
{
    for (auto itr = _respawnTimers.begin(); itr != _respawnTimers.end();)
    {
        Player* player = ObjectAccessor::FindPlayer(itr->first);
        if (!player || player->GetBattleground() != this || player->IsAlive())
        {
            itr = _respawnTimers.erase(itr);
            continue;
        }

        MobaRespawnState& state = itr->second;
        if (state.remainingMs <= diff)
        {
            RespawnAtBase(player);
            itr = _respawnTimers.erase(itr);
            continue;
        }

        state.remainingMs -= diff;

        uint32 secondsLeft = (state.remainingMs + 999) / 1000;
        if (secondsLeft != state.lastAnnouncedSec &&
            (secondsLeft == 5 || secondsLeft == 3 || secondsLeft == 2 || secondsLeft == 1))
        {
            state.lastAnnouncedSec = secondsLeft;
            ChatHandler(player->GetSession()).SendSysMessage(
                Acore::StringFormat("Respawning in {}...", secondsLeft).c_str());
        }

        ++itr;
    }
}

void BattlegroundMOBA::RespawnAtBase(Player* player)
{
    if (Position const* startPos = GetTeamStartPosition(player->GetTeamId()))
        player->TeleportTo(GetMapId(), startPos->GetPositionX(), startPos->GetPositionY(),
            startPos->GetPositionZ(), startPos->GetOrientation());

    // Same restore the stock BG resurrection uses (Battleground::_ProcessResurrect).
    player->ResurrectPlayer(1.0f);
    player->CastSpell(player, 6962, true);   // full health
    player->CastSpell(player, 44535, true);  // full mana
    player->SpawnCorpseBones(false);
}

void BattlegroundMOBA::SendHudMessage(Player* player, std::string const& body)
{
    if (!player)
        return;

    // LANG_ADDON marks this as an addon message client-side; the chat-type byte is
    // irrelevant to delivery. Body is "<prefix>\t<payload>" (see the MobaHUD addon).
    std::string message = Acore::StringFormat("{}\t{}", MOBA_HUD_ADDON_PREFIX, body);

    WorldPacket data(SMSG_MESSAGECHAT, 1 + 4 + 8 + 4 + 8 + 4 + message.size() + 2);
    data << uint8(CHAT_MSG_WHISPER);
    data << uint32(LANG_ADDON);
    data << uint64(0);                    // sender GUID (0 = server)
    data << uint32(0);
    data << uint64(0);                    // receiver GUID
    data << uint32(message.size() + 1);
    data << message;
    data << uint8(0);
    player->SendDirectMessage(&data);
}

void BattlegroundMOBA::BroadcastHudMessage(std::string const& body)
{
    for (auto const& itr : GetPlayers())
        if (Player* player = itr.second)
            SendHudMessage(player, body);
}

// Builds the per-recipient scoreboard payload. Team kills are team-relative
// (ally = the recipient's team) so the addon can colour segment 1 as "you".
std::string BattlegroundMOBA::BuildScoreboardBody(Player* player) const
{
    TeamId team  = player->GetBgTeamId();
    TeamId other = GetOtherTeamId(team);

    uint32 k = 0, d = 0, a = 0, cs = 0;
    auto itr = PlayerScores.find(player->GetGUID().GetCounter());
    if (itr != PlayerScores.end())
    {
        // Must go through BattlegroundMOBAScore* (not the base pointer): GetDeaths /
        // GetHonorableKills are protected on BattlegroundScore, reachable here only
        // because BattlegroundMOBA is a friend of BattlegroundMOBAScore, and the
        // protected-member rule requires access via the friended (derived) type.
        BattlegroundMOBAScore* score = static_cast<BattlegroundMOBAScore*>(itr->second);
        k  = score->GetKillingBlows();
        d  = score->GetDeaths();
        uint32 hk = score->GetHonorableKills();   // credited kills (own + proximity)
        a  = hk > k ? hk - k : 0;                  // proximity assist = credited minus own killing blows
        cs = score->CreepKills;
    }

    return Acore::StringFormat("S:{},{},{},{},{},{}",
        _teamPlayerKills[team], _teamPlayerKills[other], k, d, a, cs);
}

void BattlegroundMOBA::SendScoreboard(Player* player)
{
    if (player)
        SendHudMessage(player, BuildScoreboardBody(player));
}

void BattlegroundMOBA::BroadcastScoreboard()
{
    for (auto const& itr : GetPlayers())
        if (Player* player = itr.second)
            SendHudMessage(player, BuildScoreboardBody(player));
}

void BattlegroundMOBA::SendHudStateTo(Player* player)
{
    if (!player)
        return;

    SendScoreboard(player);
    // Only start the clock if the match is live; during warmup it stays frozen at 0:00
    // until StartingEventOpenDoors sends T:0.
    if (GetStatus() == STATUS_IN_PROGRESS)
        SendHudMessage(player, Acore::StringFormat("T:{}", _matchElapsedMs / 1000));
}

uint32 BattlegroundMOBA::GetRecallCastTimeMs(Player* player)
{
    if (!player)
        return 0;

    // The dynamic_cast doubles as the "is this a MOBA BG" test. Returning 0 tells
    // Spell::prepare to keep the spell's default cast time.
    BattlegroundMOBA* moba = dynamic_cast<BattlegroundMOBA*>(player->GetBattleground());
    if (!moba)
        return 0;

    MobaRespawnConfig const* cfg = sMobaRespawnDataStore->GetConfig(moba->GetMapId());
    if (!cfg)
        return 0;

    // PLACEHOLDER empowered-recall trigger: until a real mechanic exists, a player
    // carrying BG_MOBA_RECALL_EMPOWER_AURA gets the reduced cast time. Replace this
    // HasAura check with the real condition when it lands. (Empowered falls back to
    // normal if it isn't configured, i.e. recallEmpoweredCastMs == 0.)
    if (cfg->recallEmpoweredCastMs && player->HasAura(BG_MOBA_RECALL_EMPOWER_AURA))
        return cfg->recallEmpoweredCastMs;

    return cfg->recallCastMs;
}
