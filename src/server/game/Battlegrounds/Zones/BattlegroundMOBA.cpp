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
#include "CreatureAI.h"
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
#include "MobaBaseData.h"
#include "MobaNeutralData.h"
#include "MobaDropData.h"
#include "Timer.h"
#include "Random.h"
#include "SpellAuras.h"
#include "Chat.h"
#include "StringFormat.h"
#include "ObjectAccessor.h"
#include "TemporarySummon.h"
#include "WaypointMgr.h"
#include "MotionMaster.h"
#include <algorithm>
#include "Opcodes.h"
#include <unordered_set>

namespace
{
    // Shared with the client-side MobaHUD addon (client/addons/MobaHUD). The server
    // sends "MobaHUD\t<payload>" as a LANG_ADDON chat message; the 3.3.5a client
    // splits on the TAB into (prefix, payload) for the CHAT_MSG_ADDON event.
    constexpr char MOBA_HUD_ADDON_PREFIX[] = "MobaHUD";
    constexpr uint32 MOBA_HUD_RESYNC_MS    = 10000; // re-broadcast cadence for /reload + late joiners
    constexpr uint32 MOBA_NEUTRAL_CORPSE_DESPAWN_MS = 15000; // camp-member corpse cleanup (see SpawnCamp)
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
    {
        if (eventId == EVENT_MOBA_SPAWN_WAVE)
        {
            ++_waveCount;
            bool includeSiege = (_waveCount % 3 == 0);
            SpawnWave(TEAM_ALLIANCE, includeSiege);
            SpawnWave(TEAM_HORDE, includeSiege);
            _bgEvents.ScheduleEvent(EVENT_MOBA_SPAWN_WAVE, Milliseconds(30000));
        }
        else if (eventId >= EVENT_MOBA_SPAWN_CAMP_FIRST)
            SpawnCamp(eventId - EVENT_MOBA_SPAWN_CAMP_FIRST);
    }

    UpdateRespawnTimers(diff);
    UpdateFountainHealing(diff);

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

    for (size_t i = 0; i < _camps.size(); ++i)
        _bgEvents.ScheduleEvent(EVENT_MOBA_SPAWN_CAMP_FIRST + static_cast<uint32>(i), Milliseconds(_camps[i].initialSpawnMs));

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

    if (player)
    {
        _recentAttackers.erase(player->GetGUID());
        _allySupport.erase(player->GetGUID());
    }
}

void BattlegroundMOBA::HandleAreaTrigger(Player* player, uint32 trigger)
{
    if (GetStatus() != STATUS_IN_PROGRESS || !player->IsAlive())
        return;
}

bool BattlegroundMOBA::SetupBattleground()
{
    sMobaTowerDataStore->LoadIfNeeded();
    sMobaBaseDataStore->LoadIfNeeded();
    sMobaDropDataStore->LoadIfNeeded();
    sMobaPlayerDropDataStore->LoadIfNeeded();
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

            // Inert/guarded towers start unattackable until their guard tower falls (cleared in OnTowerDestroyed).
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

    // neutral camps (data-driven; see mod_moba_neutral_* / MobaNeutralData.h).
    // Optional content: a map with no camps is legal, so warn rather than fail.
    sMobaNeutralDataStore->LoadIfNeeded();
    _camps.clear();
    for (MobaNeutralCamp const& cfg : sMobaNeutralDataStore->GetCampsForMap(GetMapId()))
    {
        MobaCampState camp;
        camp.campId = cfg.campId;
        camp.initialSpawnMs = cfg.initialSpawnMs;
        camp.respawnMs = cfg.respawnMs;
        camp.members = cfg.members;
        _camps.push_back(std::move(camp));
    }
    if (_camps.empty())
        LOG_WARN("sql.sql", "BattlegroundMOBA: map {} has no rows in `mod_moba_neutral_camps` -- no jungle camps this match.", GetMapId());

    return true;
}

void BattlegroundMOBA::Init()
{
    Battleground::Init();

    _bgEvents.Reset();
    _waveCount = 0;
}

void BattlegroundMOBA::HandleKillPlayer(Player* /*player*/, Player* /*killer*/)
{
    // Intentionally empty. The engine only calls this on a player/pet killing
    // blow, but MOBA deaths are just as often finished by a creep, tower, or the
    // environment -- so ALL kill crediting and death tallying is centralized in
    // HandlePlayerDeath, driven by the moba_kill_credit UnitScript's OnUnitDeath
    // (which fires for every death regardless of killer). Scoring here too would
    // double-count the player-blow case.
}

void BattlegroundMOBA::HandleKillUnit(Creature* creature, Player* killer)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    // killer here is the loot recipient (first player to tap), not the killing
    // blow -- Unit::Kill overwrites it before calling us. Fine for towers, whose
    // credit is team-level (OnTowerDestroyed no-ops on non-tower creatures, so
    // lane creeps pass through harmlessly). Lane-creep CS needs the actual last
    // hit and is credited in npc_moba_creep::JustDied instead.
    OnTowerDestroyed(creature, killer->GetBgTeamId());
}

void BattlegroundMOBA::CreditCreepKill(Player* killer)
{
    if (GetStatus() != STATUS_IN_PROGRESS || !killer)
        return;

    auto itr = PlayerScores.find(killer->GetGUID().GetCounter());
    if (itr != PlayerScores.end())
    {
        static_cast<BattlegroundMOBAScore*>(itr->second)->CreepKills++;
        SendScoreboard(killer); // CS is shown only to its owner -> refresh just them
    }
}

void BattlegroundMOBA::GrantDeathDrops(Creature* victim, Player* killer)
{
    // LoL rule: no last hit, no reward. The engine already filled native loot
    // for the first TAPPER's group (Unit::Kill runs before JustDied), so a
    // creep-finished or post-match kill must strip the corpse, not just skip.
    if (!killer || GetStatus() != STATUS_IN_PROGRESS)
    {
        victim->loot.clear();
        victim->RemoveDynamicFlag(UNIT_DYNFLAG_LOOTABLE);
        victim->SetLootRecipient(nullptr);
        return;
    }

    // Native loot rights follow the tapper's group; ours follow the killing
    // blow. Re-point rights at the killer + their BG raid (= the whole team)
    // and clear the round-robin looter: BG raids default to GROUP_LOOT, whose
    // pre-picked round-robin looter may not even be on the killer's team
    // after the re-point, which would lock the corpse for everyone.
    victim->SetLootRecipient(killer);
    victim->loot.roundRobinPlayer.Clear();

    if (std::vector<MobaDropInfo> const* drops = sMobaDropDataStore->GetDrops(victim->GetEntry()))
        for (MobaDropInfo const& drop : *drops)
        {
            if (!roll_chance_f(drop.chance))
                continue;

            if (drop.type == MOBA_DROP_BUFF)
            {
                if (Aura* aura = killer->AddAura(drop.spell, killer))
                    if (drop.durationMs)
                    {
                        aura->SetMaxDuration(int32(drop.durationMs));
                        aura->SetDuration(int32(drop.durationMs));
                    }
            }
            else if (drop.type == MOBA_DROP_GOLD)
                victim->loot.gold += drop.copper;
        }

    // Gold-only minions have lootid 0, so Unit::Kill saw empty loot and never
    // flagged the corpse lootable (it marked it fully-looted instead); flag it
    // now that gold was injected. Item drops that all missed their roll stay
    // unflagged -- native behavior for an empty corpse.
    if (!victim->loot.isLooted())
        victim->SetDynamicFlag(UNIT_DYNFLAG_LOOTABLE);
}

void BattlegroundMOBA::GrantPlayerKillDrops(Player* killer)
{
    std::vector<MobaPlayerDropInfo> const* drops = sMobaPlayerDropDataStore->GetDrops(GetMapId());
    if (!drops)
        return;

    for (MobaPlayerDropInfo const& drop : *drops)
    {
        if (!roll_chance_f(drop.chance))
            continue;

        switch (drop.type)
        {
            case MOBA_PLAYER_DROP_BUFF:
                if (Aura* aura = killer->AddAura(drop.spell, killer))
                    if (drop.durationMs)
                    {
                        aura->SetMaxDuration(int32(drop.durationMs));
                        aura->SetDuration(int32(drop.durationMs));
                    }
                break;
            case MOBA_PLAYER_DROP_GOLD:
                killer->ModifyMoney(int32(drop.copper));
                break;
            case MOBA_PLAYER_DROP_ITEM:
                killer->AddItem(drop.item, drop.count);
                break;
        }
    }
}

void BattlegroundMOBA::RecordPlayerDamage(Player* victim, Player* attacker)
{
    if (GetStatus() != STATUS_IN_PROGRESS || !victim || !attacker
        || attacker == victim || attacker->GetBgTeamId() == victim->GetBgTeamId())
        return;

    _recentAttackers[victim->GetGUID()][attacker->GetGUID()] = GameTime::GetGameTimeMS().count();
}

void BattlegroundMOBA::RecordAllyHeal(Player* ally, Player* healer)
{
    // Healing (direct or HoT tick) links the healer to any fight the healed ally
    // is in -- no duration gate, and overheal counts (OnHeal fires regardless of
    // effective healing; LoL credits the attempt).
    if (GetStatus() != STATUS_IN_PROGRESS || !ally || !healer
        || ally == healer || ally->GetBgTeamId() != healer->GetBgTeamId())
        return;

    _allySupport[ally->GetGUID()][healer->GetGUID()] = GameTime::GetGameTimeMS().count();
}

void BattlegroundMOBA::RecordAllyBuff(Player* ally, Player* buffer, int32 buffMaxDurationMs)
{
    if (GetStatus() != STATUS_IN_PROGRESS || !ally || !buffer
        || ally == buffer || ally->GetBgTeamId() != buffer->GetBgTeamId())
        return;

    // Only a SHORT buff/shield counts as a fight buff -- a combat cooldown (Power
    // Infusion, Bloodlust, Power Word: Shield), not a maintenance buff (Fortitude,
    // Blessing of Wisdom). Permanent auras report -1. Threshold is per-map config.
    uint32 const maxDur = GetAssistBuffMaxDurationMs();
    if (buffMaxDurationMs <= 0 || uint32(buffMaxDurationMs) > maxDur)
        return;

    _allySupport[ally->GetGUID()][buffer->GetGUID()] = GameTime::GetGameTimeMS().count();
}

uint32 BattlegroundMOBA::GetAssistWindowMs() const
{
    if (MobaBaseConfig const* cfg = sMobaBaseDataStore->GetConfig(GetMapId()))
        return cfg->assistWindowMs;
    return 0;
}

uint32 BattlegroundMOBA::GetAssistBuffMaxDurationMs() const
{
    if (MobaBaseConfig const* cfg = sMobaBaseDataStore->GetConfig(GetMapId()))
        return cfg->assistBuffMaxDurationMs;
    return 0;
}

uint32 BattlegroundMOBA::GetKillCreditWindowMs() const
{
    if (MobaBaseConfig const* cfg = sMobaBaseDataStore->GetConfig(GetMapId()))
        return cfg->killCreditWindowMs;
    return 0;
}

Player* BattlegroundMOBA::ResolveKillCredit(Player* victim, Unit* killer)
{
    // The true blow-lander, resolved to its controlling player (pets/totems
    // credit the owner). If that's an enemy player still in the match, it wins
    // outright -- the window only matters when no crediting player finished it.
    Player* direct = killer ? killer->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
    if (direct && direct != victim && direct->GetBgTeamId() != victim->GetBgTeamId()
        && IsPlayerInBattleground(direct->GetGUID()))
        return direct;

    auto itr = _recentAttackers.find(victim->GetGUID());
    if (itr == _recentAttackers.end())
        return nullptr;

    // Otherwise (creep/tower/environment/suicide): the most recent enemy player
    // who damaged or debuffed the victim within the window. getMSTimeDiff is
    // wraparound-safe; "most recent" = smallest elapsed.
    uint32 const windowMs = GetKillCreditWindowMs();
    uint32 const now = GameTime::GetGameTimeMS().count();
    Player* best = nullptr;
    uint32 bestElapsed = windowMs + 1;

    for (auto const& rec : itr->second)
    {
        uint32 elapsed = getMSTimeDiff(rec.second, now);
        if (elapsed > windowMs)
            continue;

        Player* attacker = ObjectAccessor::FindPlayer(rec.first);
        if (!attacker || attacker == victim || attacker->GetBgTeamId() == victim->GetBgTeamId()
            || !IsPlayerInBattleground(attacker->GetGUID()))
            continue;

        if (elapsed < bestElapsed)
        {
            best = attacker;
            bestElapsed = elapsed;
        }
    }
    return best;
}

void BattlegroundMOBA::HandlePlayerDeath(Player* victim, Unit* killer)
{
    if (!victim || GetStatus() != STATUS_IN_PROGRESS)
        return;

    // Death always counts, whatever landed the blow -- the engine scores deaths
    // only through HandleKillPlayer, which we no-op'd, so the tally lives here.
    UpdatePlayerScore(victim, SCORE_DEATHS, 1);

    Player* creditKiller = ResolveKillCredit(victim, killer);
    if (creditKiller && creditKiller != victim)
    {
        UpdatePlayerScore(creditKiller, SCORE_HONORABLE_KILLS, 1);
        UpdatePlayerScore(creditKiller, SCORE_KILLING_BLOWS, 1);

        // Contribution-based assists (LoL-style), replacing proximity. Build the
        // set of kill participants on the killer's team: the killer, plus everyone
        // who damaged/debuffed the victim within the assist window, then -- expanded
        // to a fixed point -- everyone who healed or short-buffed a participant
        // within the window. Each pass adds only distinct players, so the fixed
        // point can't exceed team size: that's the "up to N hops" chain, self-bounding.
        TeamId const team = creditKiller->GetBgTeamId();
        uint32 const windowMs = GetAssistWindowMs();
        uint32 const now = GameTime::GetGameTimeMS().count();

        auto inWindow = [&](uint32 t) { return getMSTimeDiff(t, now) <= windowMs; };
        auto teammateInBg = [&](ObjectGuid guid) -> Player*
        {
            Player* p = ObjectAccessor::FindPlayer(guid);
            return (p && p->GetBgTeamId() == team && IsPlayerInBattleground(guid)) ? p : nullptr;
        };

        std::unordered_set<ObjectGuid> participants;
        participants.insert(creditKiller->GetGUID());

        // Direct damage/debuff assistors.
        if (auto itr = _recentAttackers.find(victim->GetGUID()); itr != _recentAttackers.end())
            for (auto const& rec : itr->second)
                if (inWindow(rec.second))
                    if (Player* a = teammateInBg(rec.first))
                        participants.insert(a->GetGUID());

        // Support chain: add anyone who healed/short-buffed a participant, repeat
        // until the set stops growing (bounded by team size).
        bool grew = true;
        while (grew)
        {
            grew = false;
            std::vector<ObjectGuid> const current(participants.begin(), participants.end());
            for (ObjectGuid const& supported : current)
            {
                auto itr = _allySupport.find(supported);
                if (itr == _allySupport.end())
                    continue;
                for (auto const& rec : itr->second)
                {
                    if (!inWindow(rec.second) || participants.count(rec.first))
                        continue;
                    if (Player* s = teammateInBg(rec.first))
                        if (participants.insert(s->GetGUID()).second)
                            grew = true;
                }
            }
        }

        // Everyone but the killer gets an assist (honorable kill); the killer
        // already has both HK and KB above, so their assist column stays 0.
        for (ObjectGuid const& guid : participants)
            if (guid != creditKiller->GetGUID())
                if (Player* p = ObjectAccessor::FindPlayer(guid))
                    UpdatePlayerScore(p, SCORE_HONORABLE_KILLS, 1);

        ++_teamPlayerKills[team];
        GrantPlayerKillDrops(creditKiller);
    }

    // Both tracking lists are per-life; the victim is dead now.
    _recentAttackers.erase(victim->GetGUID());
    _allySupport.erase(victim->GetGUID());

    BroadcastScoreboard();
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

// Creeps are TempSummons, not Battleground::AddCreature/BgCreatures -- that
// registry is a fixed-size, persistent roster, wrong for repeatedly-spawned
// ephemerals. Two engine traps live here:
//   * Map::SummonCreature takes no TempSummonType (only WorldObject's
//     convenience overload does), and TempSummon's constructor defaults to
//     TEMPSUMMON_MANUAL_DESPAWN -- skip the SetTempSummonType call below and
//     every creep silently never cleans up.
//   * TIMED_DESPAWN_OUT_OF_COMBAT, not CORPSE_TIMED_DESPAWN: the corpse
//     variant's countdown never advances while the creep is alive, so one that
//     paces the lane unengaged would never despawn.
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

// Camp members are TempSummons for the same reason lane creeps are (see
// SpawnCreep) but with CORPSE_TIMED_DESPAWN: that type's countdown only runs
// once the creature is dead -- the trap documented above for creeps is the
// point here. A living camp never despawns; a corpse vanishes shortly after
// death, long before the respawn event re-summons the whole camp.
void BattlegroundMOBA::SpawnCamp(uint32 campIndex)
{
    if (campIndex >= _camps.size())
        return;

    MobaCampState& camp = _camps[campIndex];
    camp.memberGuids.clear();
    camp.aliveCount = 0;

    for (MobaNeutralMember const& member : camp.members)
    {
        Position pos(member.x, member.y, member.z, member.o);
        if (TempSummon* summon = GetBgMap()->SummonCreature(member.entry, pos, nullptr, MOBA_NEUTRAL_CORPSE_DESPAWN_MS))
        {
            summon->SetTempSummonType(TEMPSUMMON_CORPSE_TIMED_DESPAWN);
            camp.memberGuids.push_back(summon->GetGUID());
            ++camp.aliveCount;
        }
    }
}

MobaCampState* BattlegroundMOBA::FindCampOf(ObjectGuid guid)
{
    for (MobaCampState& camp : _camps)
        if (std::find(camp.memberGuids.begin(), camp.memberGuids.end(), guid) != camp.memberGuids.end())
            return &camp;
    return nullptr;
}

// League camp-link: the whole camp fights as one. AttackStart works on a
// REACT_DEFENSIVE mate -- react states gate only self-initiated aggro. The
// status guard matters: post-match camps are frozen passive, and DamageTaken
// still fires on them, so without it poking a frozen camp would wake it.
void BattlegroundMOBA::PullCampMates(Creature* member, Unit* attacker)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    MobaCampState* camp = FindCampOf(member->GetGUID());
    if (!camp)
        return;

    for (ObjectGuid const& guid : camp->memberGuids)
    {
        if (guid == member->GetGUID())
            continue;

        Creature* mate = GetBgMap()->GetCreature(guid);
        if (mate && mate->IsAlive() && !mate->IsEngaged() && mate->AI())
            mate->AI()->AttackStart(attacker);
    }
}

void BattlegroundMOBA::NotifyNeutralDied(Creature* member)
{
    MobaCampState* camp = FindCampOf(member->GetGUID());
    if (!camp || !camp->aliveCount)
        return;

    if (--camp->aliveCount == 0 && GetStatus() == STATUS_IN_PROGRESS)
    {
        uint32 campIndex = static_cast<uint32>(camp - _camps.data());
        _bgEvents.ScheduleEvent(EVENT_MOBA_SPAWN_CAMP_FIRST + campIndex, Milliseconds(camp->respawnMs));
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

    // Neutral camps freeze under the same end-of-match rules.
    for (MobaCampState const& camp : _camps)
        for (ObjectGuid const& guid : camp.memberGuids)
        {
            Creature* mob = GetBgMap()->GetCreature(guid);
            if (!mob || !mob->IsAlive())
                continue;

            mob->CombatStop();
            mob->SetReactState(REACT_PASSIVE);
            mob->GetMotionMaster()->MoveIdle();
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
    return sGraveyard->GetGraveyard(player->GetBgTeamId() == TEAM_ALLIANCE
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
    if (MobaBaseConfig const* cfg = sMobaBaseDataStore->GetConfig(GetMapId()))
    {
        baseMs   = cfg->respawnBaseMs;
        perMinMs = cfg->respawnPerMinMs;
        capMs    = cfg->respawnCapMs;
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
    if (Position const* startPos = GetTeamStartPosition(player->GetBgTeamId()))
        player->TeleportTo(GetMapId(), startPos->GetPositionX(), startPos->GetPositionY(),
            startPos->GetPositionZ(), startPos->GetOrientation());

    // Same restore the stock BG resurrection uses (Battleground::_ProcessResurrect).
    player->ResurrectPlayer(1.0f);
    player->CastSpell(player, 6962, true);   // full health
    player->CastSpell(player, 44535, true);  // full mana
    player->SpawnCorpseBones(false);
}

// Fountain heal: players inside their own base bubble regain a % of max
// health/mana per tick. The bubble is battleground_template.StartMaxDist, shared
// with the core's prep-phase leash (_CheckSafePositions) so the two can't drift.
// Two traps: GetStartMaxDist() returns that distance ALREADY SQUARED (BattlegroundMgr
// stores MaxStartDistSq), hence the squared compare; and the core leash is 3D while
// this is 2D, so the heal zone is a cylinder -- same radius, forgiving of terrain.
void BattlegroundMOBA::UpdateFountainHealing(uint32 diff)
{
    MobaBaseConfig const* cfg = sMobaBaseDataStore->GetConfig(GetMapId());
    if (!cfg || !cfg->fountainTickMs || (!cfg->fountainHpPct && !cfg->fountainManaPct))
        return;

    float radiusSq = GetStartMaxDist();
    if (!radiusSq)
        return;

    _fountainTickMs += diff;
    if (_fountainTickMs < cfg->fountainTickMs)
        return;

    _fountainTickMs = 0;

    for (auto const& itr : GetPlayers())
    {
        Player* player = itr.second;
        if (!player || !player->IsAlive())
            continue;

        Position const* startPos = GetTeamStartPosition(player->GetBgTeamId());
        if (!startPos || player->GetExactDist2dSq(startPos) > radiusSq)
            continue;

        if (cfg->fountainHpPct)
            player->ModifyHealth(player->CountPctFromMaxHealth(cfg->fountainHpPct));

        // Rage/energy/runic power have their own regen rules -- only mana refills.
        if (cfg->fountainManaPct && player->getPowerType() == POWER_MANA)
        {
            uint32 maxMana = player->GetMaxPower(POWER_MANA);
            uint32 gain    = CalculatePct(maxMana, cfg->fountainManaPct);
            player->SetPower(POWER_MANA, std::min<uint32>(maxMana, player->GetPower(POWER_MANA) + gain));
        }
    }
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

    MobaBaseConfig const* cfg = sMobaBaseDataStore->GetConfig(moba->GetMapId());
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
