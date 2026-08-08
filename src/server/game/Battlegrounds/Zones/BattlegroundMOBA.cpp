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
#include "Chat.h"
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
#include "MobaStoreData.h"
#include "UpdateData.h"
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
    constexpr uint32 MOBA_HUD_RESYNC_MS    = 10000; // re-broadcast cadence for /reload + late joiners
    constexpr uint32 MOBA_NEUTRAL_CORPSE_DESPAWN_MS = 15000; // camp-member corpse cleanup (see SpawnCamp)
    constexpr uint32 MOBA_SHOP_RANGE_POLL_MS = 1000; // shop buy/sell affordance refresh
    constexpr uint32 MOBA_INHIB_RESPAWN_WARN_MS = 10000; // "respawning soon" lead time
    constexpr uint32 MOBA_WAVE_INTERVAL_MS = 30000; // lane-creep wave cadence
    constexpr uint32 MOBA_WAVE_WARN_MS     = 10000; // "minions incoming" lead time
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
    // Ahead of the status guard on purpose: buying starting gear during the prep
    // phase is intended, so the panel's buy/sell affordance has to be live before
    // the doors open. Everything below here is match-time only.
    UpdateShopRange(diff);

    // Also ahead of it, for a harsher reason: players can die during prep, and an
    // instanced map offers no way back on its own -- no spirit healer, and
    // Player::Update skips its auto-release entirely on instanceable maps. Let
    // this stop ticking before the doors open and a prep-phase death is permanent.
    UpdateRespawnTimers(diff);

    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    _matchElapsedMs += diff;

    // Below the status guard deliberately: a vote deadline is match time, and the
    // two calls above that guard run during prep, where no vote can exist.
    UpdateSurrenderVotes();

    _bgEvents.Update(diff);
    while (uint32 eventId = _bgEvents.ExecuteEvent())
    {
        if (eventId == EVENT_MOBA_SPAWN_WAVE)
        {
            ++_waveCount;
            bool includeSiege = (_waveCount % 3 == 0);
            SpawnWave(TEAM_ALLIANCE, includeSiege);
            SpawnWave(TEAM_HORDE, includeSiege);
            _bgEvents.ScheduleEvent(EVENT_MOBA_SPAWN_WAVE, Milliseconds(MOBA_WAVE_INTERVAL_MS));

            // Announced from here rather than SpawnWave, which runs once per team and
            // would say it twice. First wave only: after that the cadence is the
            // clock's job, and a pair of lines every 30s would crowd out real events.
            if (_waveCount == 1)
                BroadcastNotice(MOBA_NOTICE_MINIONS_SPAWNED);
        }
        else if (eventId == EVENT_MOBA_WAVE_WARN)
            BroadcastNotice(MOBA_NOTICE_MINIONS_SOON, MOBA_WAVE_WARN_MS / 1000);
        // Highest base first: each test is a >=, so an out-of-order branch swallows
        // every base above it.
        else if (eventId >= EVENT_MOBA_BOSS_WARN_FIRST)
            WarnBossRespawn(eventId - EVENT_MOBA_BOSS_WARN_FIRST);
        else if (eventId >= EVENT_MOBA_INHIB_WARN_FIRST)
            WarnInhibitorRespawn(eventId - EVENT_MOBA_INHIB_WARN_FIRST);
        else if (eventId >= EVENT_MOBA_RESPAWN_INHIB_FIRST)
            RespawnInhibitor(eventId - EVENT_MOBA_RESPAWN_INHIB_FIRST);
        else if (eventId >= EVENT_MOBA_SPAWN_CAMP_FIRST)
            SpawnCamp(eventId - EVENT_MOBA_SPAWN_CAMP_FIRST);
    }

    UpdateFountainHealing(diff);
    UpdatePassiveGold(diff);

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

    _bgEvents.ScheduleEvent(EVENT_MOBA_SPAWN_WAVE, Milliseconds(MOBA_WAVE_INTERVAL_MS));

    // Scheduled once and never re-armed: only the first wave is announced. Derived
    // from the interval rather than written out, so the warning cannot drift off
    // the spawn it announces when the cadence changes.
    _bgEvents.ScheduleEvent(EVENT_MOBA_WAVE_WARN, Milliseconds(MOBA_WAVE_INTERVAL_MS - MOBA_WAVE_WARN_MS));

    for (size_t i = 0; i < _camps.size(); ++i)
    {
        uint32 index = static_cast<uint32>(i);
        _bgEvents.ScheduleEvent(EVENT_MOBA_SPAWN_CAMP_FIRST + index, Milliseconds(_camps[i].initialSpawnMs));

        // A boss gets the same lead on its first appearance as on every respawn.
        if (_camps[i].tier && _camps[i].spawnWarnMs && _camps[i].initialSpawnMs > _camps[i].spawnWarnMs)
            _bgEvents.ScheduleEvent(EVENT_MOBA_BOSS_WARN_FIRST + index,
                Milliseconds(_camps[i].initialSpawnMs - _camps[i].spawnWarnMs));
    }

    // Match starts now (doors open): show the HUD bar at 0:00 with a zeroed scoreboard.
    BroadcastHudMessage("T:0");
    BroadcastScoreboard();
}

void BattlegroundMOBA::EndBattleground(TeamId winnerTeamId)
{
    // The core's own double-end guard sits one level below, in
    // Battleground::EndBattleground(PvPTeamId) -- past the broadcasts. Repeat it
    // here or a second caller doubles the feed lines while the core work stays single.
    if (GetStatus() == STATUS_WAIT_LEAVE)
        return;

    // Ahead of "E", and deliberately a separate payload rather than a field on it:
    // the client tests E by exact match, so anything appended stops matching. The
    // feed outlives the match, so this line needs no timer of its own.
    BroadcastMatchResult(winnerTeamId);

    // Hide the client-side HUD bar as the match ends.
    BroadcastHudMessage("E");

    Battleground::EndBattleground(winnerTeamId);
}

// A player asking to surrender. Starting a vote and agreeing to one are the same
// intent, so one entry point covers both and the command needs no branch of its own.
BG_MOBA_SurrenderResult BattlegroundMOBA::HandleSurrenderRequest(Player* player, bool agree, uint32& secondsRemaining)
{
    secondsRemaining = 0;

    if (!player || GetStatus() != STATUS_IN_PROGRESS)
        return MOBA_SURRENDER_NOT_IN_MATCH;

    TeamId const team = player->GetBgTeamId();
    MobaSurrenderVote& vote = _surrenderVote[team];
    ObjectGuid const guid = player->GetGUID();

    if (!vote.endsAtMs)
    {
        if (!agree)
            return MOBA_SURRENDER_NO_VOTE;

        // The gate reads the HUD's own clock, not Battleground::GetStartTime(), which
        // also counts the prep phase. "Available at 1:00" has to mean the 1:00 on the bar.
        uint32 const gateMs = GetSurrenderMinMs();
        if (_matchElapsedMs < gateMs)
        {
            secondsRemaining = (gateMs - _matchElapsedMs + 999) / 1000;
            return MOBA_SURRENDER_TOO_EARLY;
        }

        if (_matchElapsedMs < vote.blockedUntilMs)
        {
            secondsRemaining = (vote.blockedUntilMs - _matchElapsedMs + 999) / 1000;
            return MOBA_SURRENDER_ON_COOLDOWN;
        }

        vote.initiator = guid;
        vote.yes.clear();
        vote.no.clear();
        vote.yes.insert(guid);
        vote.endsAtMs = _matchElapsedMs + GetSurrenderVoteMs();

        // Resolved before announcing: a team small enough to meet the threshold on
        // the initiator alone never has a vote worth talking about.
        if (ResolveSurrenderVote(team) == MOBA_SURRENDER_PASSED)
            return MOBA_SURRENDER_PASSED;

        AnnounceToTeam(team, Acore::StringFormat(
            "{} wants to surrender -- .surrender to agree, .surrender no to refuse ({}s).",
            player->GetName(), GetSurrenderVoteMs() / 1000));
        AnnounceSurrenderTally(team);
        return MOBA_SURRENDER_VOTE_STARTED;
    }

    if (vote.yes.count(guid) || vote.no.count(guid))
        return MOBA_SURRENDER_ALREADY_VOTED;

    if (agree)
        vote.yes.insert(guid);
    else
        vote.no.insert(guid);

    BG_MOBA_SurrenderResult const result = ResolveSurrenderVote(team);
    if (result == MOBA_SURRENDER_VOTE_COUNTED)
        AnnounceSurrenderTally(team);

    return result;
}

// League's all-but-one, floored so a two-player team still needs both: at a plain
// size - 1 a duo surrenders on one player's say-so, which is the unilateral
// behaviour the vote exists to remove. A lone player meets it unaided.
uint32 BattlegroundMOBA::GetSurrenderVotesNeeded(TeamId team) const
{
    uint32 const size = GetPlayersCountByTeam(team);
    return (size <= 2) ? size : size - 1;
}

// Ballots from players who have since left do not count: the roster a vote is
// measured against is the one standing now, not the one that started it.
uint32 BattlegroundMOBA::CountSurrenderVotes(TeamId team, bool agree) const
{
    GuidUnorderedSet const& ballots = agree ? _surrenderVote[team].yes : _surrenderVote[team].no;

    uint32 count = 0;
    for (ObjectGuid const& guid : ballots)
        if (IsPlayerInBattleground(guid))
            ++count;

    return count;
}

// Ends the match on a pass, closes the vote and starts the cooldown once the
// threshold is out of reach, otherwise leaves it running.
BG_MOBA_SurrenderResult BattlegroundMOBA::ResolveSurrenderVote(TeamId team)
{
    uint32 const needed = GetSurrenderVotesNeeded(team);

    if (CountSurrenderVotes(team, true) >= needed)
    {
        CloseSurrenderVote(team, false);
        ExecuteSurrender(team);
        return MOBA_SURRENDER_PASSED;
    }

    // Out of reach: every player who has not already refused voting yes still falls
    // short. Refusals only ever come from players on this team, so the subtraction
    // cannot underflow.
    if (GetPlayersCountByTeam(team) - CountSurrenderVotes(team, false) < needed)
    {
        AnnounceToTeam(team, "The surrender vote failed.");
        CloseSurrenderVote(team, true);
        return MOBA_SURRENDER_VOTE_FAILED;
    }

    return MOBA_SURRENDER_VOTE_COUNTED;
}

void BattlegroundMOBA::CloseSurrenderVote(TeamId team, bool startCooldown)
{
    MobaSurrenderVote& vote = _surrenderVote[team];

    if (startCooldown)
        vote.blockedUntilMs = _matchElapsedMs + GetSurrenderCooldownMs();

    vote.initiator.Clear();
    vote.yes.clear();
    vote.no.clear();
    vote.endsAtMs = 0;
}

void BattlegroundMOBA::UpdateSurrenderVotes()
{
    for (uint8 i = 0; i < 2; ++i)
    {
        // A pass ends the match, and anything still open on the other team dies with
        // it -- resolving that one too would send a second N: pair.
        if (GetStatus() != STATUS_IN_PROGRESS)
            return;

        TeamId const team = TeamId(i);
        if (!_surrenderVote[i].endsAtMs)
            continue;

        // An empty team's threshold is zero, which every vote trivially meets.
        // Premature finish already owns the abandoned-team case.
        if (!GetPlayersCountByTeam(team))
        {
            CloseSurrenderVote(team, false);
            continue;
        }

        // Re-evaluated every tick rather than only when someone votes: a player
        // leaving shrinks the team and the threshold with it, so a vote can pass
        // with no new ballot cast.
        if (ResolveSurrenderVote(team) != MOBA_SURRENDER_VOTE_COUNTED)
            continue;

        if (_matchElapsedMs >= _surrenderVote[i].endsAtMs)
        {
            AnnounceToTeam(team, "The surrender vote failed.");
            CloseSurrenderVote(team, true);
        }
    }
}

void BattlegroundMOBA::ExecuteSurrender(TeamId loser)
{
    TeamId const winner = (loser == TEAM_ALLIANCE) ? TEAM_HORDE : TEAM_ALLIANCE;

    // Ahead of EndBattleground, which sends the VICTORY/DEFEAT pair itself: this
    // line says WHY, that one says what. Per recipient for the same reason.
    for (auto const& itr : GetPlayers())
    {
        Player* recipient = itr.second;
        if (!recipient)
            continue;

        uint32 const code = (recipient->GetBgTeamId() == loser)
            ? MOBA_NOTICE_SURRENDER_OWN : MOBA_NOTICE_SURRENDER_ENEMY;
        SendHudMessage(recipient, Acore::StringFormat("N:{},0", code));
    }

    EndBattleground(winner);
}

// Vote traffic is system chat, not the kill feed: the feed's wording lives entirely
// in the addon and "N:<code>,<arg>" carries one number, which cannot say "2 of 4"
// or name the initiator. The enemy team is told nothing until the vote passes.
void BattlegroundMOBA::AnnounceToTeam(TeamId team, std::string const& text)
{
    for (auto const& itr : GetPlayers())
    {
        Player* recipient = itr.second;
        if (!recipient || recipient->GetBgTeamId() != team)
            continue;

        ChatHandler(recipient->GetSession()).SendSysMessage(text.c_str());
    }
}

void BattlegroundMOBA::AnnounceSurrenderTally(TeamId team)
{
    AnnounceToTeam(team, Acore::StringFormat("Surrender vote: {} of {} needed.",
        CountSurrenderVotes(team, true), GetSurrenderVotesNeeded(team)));
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

    // The store is NOT loaded yet on the first match of a worldserver process.
    // SetupBattleground -- which is what normally loads it -- runs from
    // Battleground::_ProcessJoin, i.e. on the first BG tick AFTER a player has
    // already ported in, so this hook beats it. Cheap to repeat: LoadIfNeeded is a
    // bool check once loaded.
    sMobaBaseDataStore->LoadIfNeeded();

    // Opening buy, so the prep phase is a decision rather than a wait. AddPlayer
    // runs again on a reconnect and the wallet deliberately outlives RemovePlayer,
    // so paying unconditionally would pay twice; an absent wallet entry is the
    // "never been paid" test, because AddMatchGold is the only thing that creates one.
    if (MobaBaseConfig const* cfg = sMobaBaseDataStore->GetConfig(GetMapId()))
        if (cfg->startingGold && _wallets.find(player->GetGUID()) == _wallets.end())
            AddMatchGold(player, cfg->startingGold);
}

void BattlegroundMOBA::RecordGrantedItem(Player* player, Item* item, uint32 count)
{
    if (!player || !item || !count)
        return;

    _grantedItems[player->GetGUID()].push_back(item->GetGUID());
    _grantedCounts[player->GetGUID()][item->GetEntry()] += count;
}

uint32 BattlegroundMOBA::GetGrantedCount(Player* player, uint32 itemEntry) const
{
    if (!player)
        return 0;

    auto counts = _grantedCounts.find(player->GetGUID());
    if (counts == _grantedCounts.end())
        return 0;

    auto itr = counts->second.find(itemEntry);
    return itr != counts->second.end() ? itr->second : 0;
}

void BattlegroundMOBA::ForgetGrantedItem(Player* player, Item* item, uint32 count)
{
    if (!player || !item || !count)
        return;

    auto counts = _grantedCounts.find(player->GetGUID());
    if (counts != _grantedCounts.end())
    {
        auto itr = counts->second.find(item->GetEntry());
        if (itr != counts->second.end())
            itr->second -= std::min(itr->second, count);
    }

    // Drop the GUID only when the whole stack went, so the exit pass has less to
    // walk. A partial sale leaves the item -- and our claim on the rest of it.
    if (item->GetCount() <= count)
    {
        auto guids = _grantedItems.find(player->GetGUID());
        if (guids != _grantedItems.end())
            guids->second.erase(std::remove(guids->second.begin(), guids->second.end(), item->GetGUID()),
                                guids->second.end());
    }
}

uint32 BattlegroundMOBA::GetMatchGold(Player* player) const
{
    if (!player)
        return 0;

    auto itr = _wallets.find(player->GetGUID());
    return itr != _wallets.end() ? itr->second : 0;
}

void BattlegroundMOBA::AddMatchGold(Player* player, uint32 copper)
{
    if (!player || !copper)
        return;

    _wallets[player->GetGUID()] += copper;
    SendScoreboard(player);   // the bar column and the shop header read the same payload
}

bool BattlegroundMOBA::SpendMatchGold(Player* player, uint32 copper)
{
    if (!player)
        return false;

    if (!copper)
        return true;   // free is always affordable, and must not open a wallet entry

    auto itr = _wallets.find(player->GetGUID());
    if (itr == _wallets.end() || itr->second < copper)
        return false;

    itr->second -= copper;
    SendScoreboard(player);
    return true;
}

void BattlegroundMOBA::AwardTeamGold(TeamId team, uint32 copper)
{
    if (!copper)
        return;

    for (auto const& itr : GetPlayers())
        if (Player* player = itr.second)
            if (player->GetBgTeamId() == team)
                AddMatchGold(player, copper);
}

void BattlegroundMOBA::AwardTeamBuff(TeamId team, uint32 spell, uint32 durationMs)
{
    if (!spell)
        return;

    for (auto const& itr : GetPlayers())
    {
        Player* player = itr.second;
        if (!player || player->GetBgTeamId() != team || !player->IsAlive())
            continue;

        Aura* aura = player->AddAura(spell, player);
        if (aura && durationMs)
        {
            aura->SetMaxDuration(int32(durationMs));
            aura->SetDuration(int32(durationMs));
        }
    }
}

void BattlegroundMOBA::SetShopAddonReady(Player* player)
{
    if (player)
        _shopAddonPlayers.insert(player->GetGUID());
}

bool BattlegroundMOBA::HasShopAddon(Player* player) const
{
    return player && _shopAddonPlayers.count(player->GetGUID()) != 0;
}

// Arithmetic against a cached Position, not a creature lookup: the shopkeeper is
// decoration and a convenience click, and the base circle is what actually gates
// trading. Deliberately NOT merged into UpdateFountainHealing despite the
// identical test -- that one is paced by a config tunable, and the shop
// affordance must not become a hostage of a healing knob.
bool BattlegroundMOBA::IsInShopRange(Player* player) const
{
    if (!player)
        return false;

    MobaBaseConfig const* cfg = sMobaBaseDataStore->GetConfig(GetMapId());
    if (!cfg || cfg->fountainRadius <= 0.0f)
        return false;

    Position const* startPos = GetTeamStartPosition(player->GetBgTeamId());
    if (!startPos)
        return false;

    return player->GetExactDist2dSq(startPos) <= cfg->fountainRadius * cfg->fountainRadius;
}

void BattlegroundMOBA::SendShopRange(Player* player, bool force)
{
    if (!player || !HasShopAddon(player))
        return;

    bool inRange = IsInShopRange(player);

    auto known = _shopInRange.find(player->GetGUID());
    if (!force && known != _shopInRange.end() && known->second == inRange)
        return;

    _shopInRange[player->GetGUID()] = inRange;
    SendShopMessage(player, inRange ? "RANGE:1" : "RANGE:0");
}

void BattlegroundMOBA::UpdateShopRange(uint32 diff)
{
    _shopRangeMs += diff;
    if (_shopRangeMs < MOBA_SHOP_RANGE_POLL_MS)
        return;

    _shopRangeMs = 0;

    for (auto const& itr : GetPlayers())
        SendShopRange(itr.second);
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
        _streaks.erase(player->GetGUID());
        _shopAddonPlayers.erase(player->GetGUID());
        _shopInRange.erase(player->GetGUID());

        // Match-granted items are match-only, and this hook covers every exit
        // path. Two passes, because the ledger alone would let DestroyItemCount
        // pick the player's own copy of a shared entry while a soulbound one of
        // ours escapes: the GUID pass takes exactly what we handed over, and the
        // ledger pass mops up whatever was split off it under a new GUID.
        auto guids  = _grantedItems.find(player->GetGUID());
        auto counts = _grantedCounts.find(player->GetGUID());
        bool destroyed = false;

        if (guids != _grantedItems.end())
        {
            for (ObjectGuid itemGuid : guids->second)
            {
                Item* item = player->GetItemByGuid(itemGuid);
                if (!item)
                    continue;

                uint32 entry = item->GetEntry();
                uint32 owed  = 0;
                if (counts != _grantedCounts.end())
                {
                    auto itr = counts->second.find(entry);
                    if (itr != counts->second.end())
                        owed = itr->second;
                }

                // Never take more of a stack than the match gave: StoreLootItem
                // merges ours into one the player already held, so this GUID can
                // cover their potions as well as ours.
                uint32 take = std::min(item->GetCount(), owed);
                if (!take)
                    continue;

                counts->second[entry] -= take;
                player->DestroyItemCount(item, take, true);
                destroyed = true;
            }

            _grantedItems.erase(guids);
        }

        if (counts != _grantedCounts.end())
        {
            for (auto const& pair : counts->second)
                if (pair.second)
                {
                    player->DestroyItemCount(pair.first, pair.second, true);
                    destroyed = true;
                }

            _grantedCounts.erase(counts);
        }

        if (destroyed)
        {
            // DestroyItem zeroes the PLAYER_VISIBLE_ITEM fields, but this runs in
            // the same tick the player is pulled from the world, so the normal
            // flush never reaches the client -- and the client relocates its own
            // player object on a map change rather than recreating it, so the
            // stripped gear stays rendered on the model until relog. Send the
            // changed values synchronously instead.
            UpdateData upd;
            WorldPacket packet;
            player->BuildValuesUpdateBlockForPlayer(&upd, player);
            upd.BuildPacket(packet);
            player->SendDirectMessage(&packet);
        }
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
    sMobaStoreDataStore->LoadIfNeeded();
    std::vector<MobaTowerConfig> towerConfigs = sMobaTowerDataStore->GetForMap(GetMapId());
    if (towerConfigs.empty())
    {
        LOG_ERROR("sql.sql", "BattlegroundMOBA: `mod_moba_tower_data` has no rows for map {}, battleground not created!", GetMapId());
        return false;
    }

    // Must resize before any AddCreature call below.
    BgCreatures.resize(BG_MOBA_CREATURE_FIXED_MAX + towerConfigs.size());

    MobaBaseConfig const* baseCfg = sMobaBaseDataStore->GetConfig(GetMapId());
    if (!baseCfg || !baseCfg->domeEntryAlliance || !baseCfg->domeEntryHorde)
    {
        LOG_ERROR("sql.sql", "BattlegroundMOBA: `mod_moba_base` has no spawn dome entries for map {}, battleground not created!", GetMapId());
        return false;
    }

    // Spawn domes (the prep-phase barrier), each centered on its team's start
    // position -- the same source respawn and the fountain read, so a dome cannot
    // drift off the spawn point. Entries are per-map so each mode's dome is sized
    // to its own spawn.radius (see gen_base.py).
    // The zero quaternion is deliberate: SetWorldRotation derives the rotation from
    // the orientation when the quat is zero, which is the Z-axis spin a dome wants.
    Position const* allianceStart = GetTeamStartPosition(TEAM_ALLIANCE);
    Position const* hordeStart    = GetTeamStartPosition(TEAM_HORDE);

    AddObject(BG_MOBA_OBJECT_DOOR_A, baseCfg->domeEntryAlliance,
        allianceStart->GetPositionX(), allianceStart->GetPositionY(), allianceStart->GetPositionZ(),
        allianceStart->GetOrientation(), 0.0f, 0.0f, 0.0f, 0.0f, RESPAWN_IMMEDIATELY);
    AddObject(BG_MOBA_OBJECT_DOOR_H, baseCfg->domeEntryHorde,
        hordeStart->GetPositionX(), hordeStart->GetPositionY(), hordeStart->GetPositionZ(),
        hordeStart->GetOrientation(), 0.0f, 0.0f, 0.0f, 0.0f, RESPAWN_IMMEDIATELY);

    // towers (data-driven; see mod_moba_tower_data / MobaTowerData.h)
    _towers.clear();
    _towers.reserve(towerConfigs.size());
    for (size_t i = 0; i < towerConfigs.size(); ++i)
    {
        MobaTowerConfig const& cfg = towerConfigs[i];
        uint32 slot = BG_MOBA_CREATURE_FIXED_MAX + static_cast<uint32>(i);
        // Battleground::AddCreature applies a respawn delay only when one is passed,
        // so the default 0 leaves Creature's own 300s default in place and every
        // structure quietly returns ~6 minutes after dying -- alive, but still
        // flagged destroyed here, so inert to every code path that matters.
        // Inhibitors are unaffected: RespawnInhibitor brings them back with
        // Creature::Respawn(true), a forced respawn that ignores this timer.
        AddCreature(cfg.entry, slot, cfg.x, cfg.y, cfg.z, cfg.o, DAY);

        MobaTowerState state;
        state.entry = cfg.entry;
        state.team = cfg.team;
        state.tier = cfg.tier;
        state.lane = cfg.lane;
        state.guardedByEntry = cfg.guardedByEntry;
        state.kind = cfg.kind;
        state.respawnMs = cfg.respawnMs;
        state.teamGoldCopper    = cfg.teamGoldCopper;
        state.lastHitGoldCopper = cfg.lastHitGoldCopper;

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
        if (cfg.role < MOBA_CREEP_ROLE_MAX && cfg.team < 2)
            _waveComposition[cfg.team].byRole[cfg.role].push_back(cfg.entry);

    bool hasInhibitor[2] = {false, false};
    for (MobaTowerConfig const& cfg : towerConfigs)
        if (cfg.kind == MOBA_STRUCTURE_INHIBITOR && cfg.team < 2)
            hasInhibitor[cfg.team] = true;

    for (uint32 team = 0; team < 2; ++team)
    {
        MobaWaveComposition const& comp = _waveComposition[team];

        std::size_t unitCount = 0;
        for (uint32 role = 0; role < MOBA_CREEP_ROLE_MAX; ++role)
            unitCount += comp.byRole[role].size();

        if (!unitCount)
        {
            LOG_ERROR("sql.sql", "BattlegroundMOBA: map {} team {} has no rows in `mod_moba_creep_data`, battleground not created!", GetMapId(), team);
            return false;
        }

        if (hasInhibitor[team] && comp.byRole[MOBA_CREEP_ROLE_SUPER].empty())
            LOG_WARN("sql.sql", "BattlegroundMOBA: map {} team {} has an inhibitor but no super creep (role=super) in `mod_moba_creep_data` -- taking that inhibitor will field no super minions.", GetMapId(), team);
    }

    // neutral camps (data-driven; see mod_moba_neutral_* / MobaNeutralData.h).
    // Optional content: a map with no camps is legal, so warn rather than fail.
    sMobaNeutralDataStore->LoadIfNeeded();
    _camps.clear();
    for (MobaNeutralCamp const& cfg : sMobaNeutralDataStore->GetCampsForMap(GetMapId()))
    {
        MobaCampState camp;
        camp.campId = cfg.campId;
        camp.tier = cfg.tier;
        camp.initialSpawnMs = cfg.initialSpawnMs;
        camp.respawnMs = cfg.respawnMs;
        camp.spawnWarnMs = cfg.spawnWarnMs;
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
    _superMinionsActive[0] = false;
    _superMinionsActive[1] = false;
    _streaks.clear();
    _firstBlood = false;
    _surrenderVote[0] = MobaSurrenderVote();
    _surrenderVote[1] = MobaSurrenderVote();
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

void BattlegroundMOBA::HandleKillUnit(Creature* /*creature*/, Player* /*killer*/)
{
    // Intentionally empty, for the same shape of reason as HandleKillPlayer. The
    // engine hands this hook the LOOT RECIPIENT rather than the killing blow --
    // Unit::Kill reassigns player = creature->GetLootRecipient() before calling us
    // -- which is wrong for a tower's last-hit bonus and wrong for creep CS. Both
    // are credited from the creature AI's JustDied, which receives the real killer.
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

void BattlegroundMOBA::GrantDeathDrops(Creature* victim, Player* killer, Unit* killerUnit)
{
    // Post-match kills reward nothing at all, which is what keeps the frozen
    // creeps and camps farmproof. The engine already filled native loot for the
    // first TAPPER's group (Unit::Kill runs before JustDied), so this must strip
    // the corpse rather than merely skip.
    if (GetStatus() != STATUS_IN_PROGRESS)
    {
        victim->loot.clear();
        victim->RemoveDynamicFlag(UNIT_DYNFLAG_LOOTABLE);
        victim->SetLootRecipient(nullptr);
        return;
    }

    // Team-wide drops follow the killing BLOW's side, which need not be a
    // player's: a creep or tower that finishes a boss still pays its own team,
    // as in League where a minion-executed Baron still buffs that side. Personal
    // drops below stay strictly last-hit.
    TeamId rewardTeam = killer ? killer->GetBgTeamId() : ResolveKillerTeam(killerUnit);

    if (killer)
    {
        // Native loot rights follow the tapper's group; ours follow the killing blow,
        // and only the killing blow. Three lines, each covering a different half:
        //
        //   SetLootRecipient(killer) is group-wide ON PURPOSE. Narrowing it with
        //   withGroup=false zeroes the recipient GROUP, and Player::isAllowedToLoot
        //   rejects any looter who HAS a group against a corpse that has none -- which
        //   is every player in a battleground, the killer included. They would never be
        //   sent UNIT_DYNFLAG_LOOTABLE and so could not click their own kill.
        //
        //   roundRobinPlayer must be ASSIGNED, never cleared: BG raids are GROUP_LOOT,
        //   whose isAllowedToLoot branch admits anyone when no round-robin looter is
        //   set. Setting it to the killer is what hides the corpse from teammates -- and
        //   what shows the killer every item, over-threshold ones included. It is
        //   cosmetic only; LootHandler clears it again if the killer closes a corpse
        //   they did not empty, which is why moba_loot_rights_globalscript is what
        //   actually enforces this.
        //
        //   loot_type suppresses the group roll. Player::SendLoot broadcasts a GroupLoot
        //   window for every over-threshold item to the whole nearby raid, guarded only
        //   by loot_type == LOOT_NONE -- and it fires on the KILLER's own first open,
        //   before any permission check gets a say. Stamping the value SendLoot would
        //   assign at its tail anyway skips that branch entirely.
        victim->SetLootRecipient(killer);
        victim->loot.roundRobinPlayer = killer->GetGUID();
        victim->loot.loot_type        = LOOT_CORPSE;
    }
    else
    {
        // LoL rule: no last hit, no CORPSE. The team-wide rows below are NOT
        // forfeited with it -- splitting this out of the early return is the
        // whole point, so a boss finished by a stray creep still pays.
        victim->loot.clear();
        victim->RemoveDynamicFlag(UNIT_DYNFLAG_LOOTABLE);
        victim->SetLootRecipient(nullptr);
    }

    if (std::vector<MobaDropInfo> const* drops = sMobaDropDataStore->GetDrops(victim->GetEntry()))
        for (MobaDropInfo const& drop : *drops)
        {
            if (!roll_chance_f(drop.chance))
                continue;

            if (drop.type == MOBA_DROP_BUFF)
            {
                if (!killer)
                    continue;

                if (Aura* aura = killer->AddAura(drop.spell, killer))
                    if (drop.durationMs)
                    {
                        aura->SetMaxDuration(int32(drop.durationMs));
                        aura->SetDuration(int32(drop.durationMs));
                    }
            }
            else if (drop.type == MOBA_DROP_GOLD)
            {
                if (killer)
                    victim->loot.gold += drop.copper;
            }
            else if (drop.type == MOBA_DROP_TEAM_GOLD)
            {
                if (rewardTeam != TEAM_NEUTRAL)
                    AwardTeamGold(rewardTeam, drop.copper);
            }
            else if (drop.type == MOBA_DROP_TEAM_BUFF)
            {
                if (rewardTeam != TEAM_NEUTRAL)
                    AwardTeamBuff(rewardTeam, drop.spell, drop.durationMs);
            }
        }

    // Gold-only minions have lootid 0, so Unit::Kill saw empty loot and never
    // flagged the corpse lootable (it marked it fully-looted instead); flag it
    // now that gold was injected. Item drops that all missed their roll stay
    // unflagged -- native behavior for an empty corpse. Guarded on `killer`: the
    // no-last-hit branch above just stripped the corpse and must not re-flag it.
    if (killer && !victim->loot.isLooted())
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
                AddMatchGold(killer, drop.copper);
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

uint32 BattlegroundMOBA::GetSurrenderMinMs() const
{
    if (MobaBaseConfig const* cfg = sMobaBaseDataStore->GetConfig(GetMapId()))
        return cfg->surrenderMinMs;
    return 0;
}

uint32 BattlegroundMOBA::GetSurrenderVoteMs() const
{
    if (MobaBaseConfig const* cfg = sMobaBaseDataStore->GetConfig(GetMapId()))
        return cfg->surrenderVoteMs;
    return 0;
}

uint32 BattlegroundMOBA::GetSurrenderCooldownMs() const
{
    if (MobaBaseConfig const* cfg = sMobaBaseDataStore->GetConfig(GetMapId()))
        return cfg->surrenderCooldownMs;
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

    MobaBaseConfig const* cfg = sMobaBaseDataStore->GetConfig(GetMapId());

    // Read BEFORE the erase at the bottom: the victim's streak is the whole
    // definition of a shutdown, and it is gone the moment this death is booked.
    uint32 victimSpree = 0;
    if (auto itr = _streaks.find(victim->GetGUID()); itr != _streaks.end())
        victimSpree = itr->second.spree;

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

        // The two flags cannot collide: first blood means no kill has landed yet,
        // so no victim can be carrying a spree. The `else` is documentation, not a
        // tie-break. The line and the money are separately gated -- a map that
        // configures no bounty still gets told a shutdown happened.
        uint32 flag = MOBA_KILL_FLAG_NONE;
        if (!_firstBlood)
        {
            _firstBlood = true;
            flag = MOBA_KILL_FLAG_FIRST_BLOOD;
            if (cfg)
                AddMatchGold(creditKiller, cfg->firstBloodGold);
        }
        else if (cfg && cfg->spreeMin && victimSpree >= cfg->spreeMin)
        {
            flag = MOBA_KILL_FLAG_SHUTDOWN;

            uint32 bounty = cfg->shutdownPerStreak * victimSpree;
            if (cfg->shutdownCapGold && bounty > cfg->shutdownCapGold)
                bounty = cfg->shutdownCapGold;
            AddMatchGold(creditKiller, bounty);   // guards 0 itself
        }

        BroadcastKillFeed(creditKiller, victim, flag);
        UpdateKillStreak(creditKiller);
    }
    else
    {
        // No enemy player credited: creep / tower / neutral / environment / suicide.
        BroadcastNonPlayerDeath(victim, killer);
    }

    // All three lists are per-life; the victim is dead now. The streak erase sits
    // OUTSIDE the credited-kill branch on purpose -- dying to a creep ends a spree
    // exactly as surely as dying to a player does.
    _recentAttackers.erase(victim->GetGUID());
    _allySupport.erase(victim->GetGUID());
    _streaks.erase(victim->GetGUID());

    // Last, so the sweep sees a fully-booked death.
    CheckAce(victim->GetBgTeamId());

    BroadcastScoreboard();
}

// Advance the killer's two counters and announce whatever they crossed. Split out
// of HandlePlayerDeath only because that function is already the longest here; it
// has exactly one caller.
void BattlegroundMOBA::UpdateKillStreak(Player* killer)
{
    if (!killer)
        return;

    MobaBaseConfig const* cfg = sMobaBaseDataStore->GetConfig(GetMapId());
    uint32 const now = GameTime::GetGameTimeMS().count();

    MobaStreakState& st = _streaks[killer->GetGUID()];
    ++st.spree;

    // The window runs from the PREVIOUS kill, not from the first of the chain, so
    // a steady stream keeps extending one multi-kill. That is what "rolling"
    // means here, and it is how LoL counts. A zero window disables multi-kills
    // outright: `multi` then never leaves 1 and the >= 2 test below never fires.
    uint32 const windowMs = cfg ? cfg->multiKillWindowMs : 0;
    if (st.multi && windowMs && getMSTimeDiff(st.lastKillMs, now) <= windowMs)
        ++st.multi;
    else
        st.multi = 1;
    st.lastKillMs = now;

    TeamId const team = killer->GetBgTeamId();

    // Spree first, so a kill that is both lands the multi-kill on top of it: the
    // multi-kill is the rarer of the two and the one worth reading.
    if (cfg && cfg->spreeMin && st.spree >= cfg->spreeMin)
        BroadcastStreak(killer, team, MOBA_STREAK_SPREE, st.spree);

    if (st.multi >= 2)
        BroadcastStreak(killer, team, MOBA_STREAK_MULTI, st.multi);
}

// An ace is a whole team down at once. Swept after every death rather than kept
// as a counter: respawns, disconnects and mid-match joins all move the number,
// and a walk over a handful of players costs less than keeping a tally honest
// against all three. Called with the team that just LOST someone -- the only
// team whose alive-count can have reached zero on this death.
void BattlegroundMOBA::CheckAce(TeamId wipedTeam)
{
    MobaBaseConfig const* cfg = sMobaBaseDataStore->GetConfig(GetMapId());
    uint32 const minTeam = cfg ? cfg->aceMinTeam : 0;
    if (!minTeam)
        return;

    uint32 total = 0;
    for (auto const& itr : GetPlayers())
    {
        Player* player = itr.second;
        if (!player || player->GetBgTeamId() != wipedTeam)
            continue;

        // The player who just died already reads dead: OnUnitDeath is the last
        // statement in Unit::Kill, long after setDeathState. No special case.
        if (player->IsAlive())
            return;

        ++total;
    }

    // A solo player wiping is not an ace, it is a kill. minTeam is what keeps the
    // line meaningful in a 1v1 test match.
    if (total < minTeam)
        return;

    BroadcastStreak(nullptr, GetOtherTeamId(wipedTeam), MOBA_STREAK_ACE, 0);
}

void BattlegroundMOBA::OnTowerDestroyed(Creature* tower, TeamId winnerTeamId, Player* lastHitter)
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

    // Behind the `destroyed` guard so it cannot double-fire, and ahead of the core
    // branch's early return below or a base kill would announce nothing.
    BroadcastStructureEvent(*itr, MOBA_STRUCT_EVENT_DESTROYED, lastHitter);

    // Objective gold, paid behind the `destroyed` guard so nothing can double-pay,
    // and unconditionally on team so a creep-finished structure still rewards the
    // push. A re-killed inhibitor pays AGAIN on purpose -- RespawnInhibitor clears
    // the flag, and taking the same objective twice is worth the same twice.
    AwardTeamGold(winnerTeamId, itr->teamGoldCopper);
    if (lastHitter)
        AddMatchGold(lastHitter, itr->lastHitGoldCopper);

    // Unlock any structures this one was guarding (the next tier becomes attackable).
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

    // Destroying the enemy base (core) ends the match.
    if (itr->kind == MOBA_STRUCTURE_CORE)
    {
        FreezeAllCreeps();
        EndBattleground(winnerTeamId);
        return;
    }

    // Destroying an inhibitor: the enemy of its owner fields super minions until
    // this inhibitor respawns, and the inhibitor schedules its own return.
    if (itr->kind == MOBA_STRUCTURE_INHIBITOR)
    {
        // The beneficiary is the enemy of the OWNER, never winnerTeamId (the
        // killer's team, per npc_moba_tower::JustDied). Those agree in a real push
        // and diverge on an own-team kill -- and since RespawnInhibitor clears the
        // flag from the owner, any mismatch here leaks super minions permanently.
        TeamId beneficiary = (itr->team == TEAM_ALLIANCE) ? TEAM_HORDE : TEAM_ALLIANCE;
        _superMinionsActive[beneficiary] = true;

        if (itr->respawnMs)
        {
            uint32 towerIndex = static_cast<uint32>(std::distance(_towers.begin(), itr));
            _bgEvents.ScheduleEvent(EVENT_MOBA_RESPAWN_INHIB_FIRST + towerIndex, Milliseconds(itr->respawnMs));

            // Skipped when the respawn is shorter than the lead time: a warning that
            // fires at or after the thing it warns about is worse than none.
            if (itr->respawnMs > MOBA_INHIB_RESPAWN_WARN_MS)
                _bgEvents.ScheduleEvent(EVENT_MOBA_INHIB_WARN_FIRST + towerIndex,
                    Milliseconds(itr->respawnMs - MOBA_INHIB_RESPAWN_WARN_MS));
        }
    }
}

// The pre-warning half of the inhibitor respawn, scheduled alongside it in
// OnTowerDestroyed. Silent if the inhibitor is already back -- the warn is a
// separate scheduled event and nothing cancels it if the timeline changes under it.
void BattlegroundMOBA::WarnInhibitorRespawn(uint32 towerIndex)
{
    if (GetStatus() != STATUS_IN_PROGRESS || towerIndex >= _towers.size())
        return;

    MobaTowerState const& inhib = _towers[towerIndex];
    if (!inhib.destroyed)
        return;

    BroadcastStructureEvent(inhib, MOBA_STRUCT_EVENT_RESPAWNING, nullptr);
}

// Inhibitor respawn (scheduled by OnTowerDestroyed). Brings the structure back,
// re-locks the base it guards, and ends the beneficiary team's super minions.
void BattlegroundMOBA::RespawnInhibitor(uint32 towerIndex)
{
    if (GetStatus() != STATUS_IN_PROGRESS || towerIndex >= _towers.size())
        return;

    MobaTowerState& inhib = _towers[towerIndex];
    inhib.destroyed = false;

    if (Creature* creature = GetBgMap()->GetCreature(inhib.guid))
    {
        creature->Respawn(true);
        creature->SetFullHealth();
    }

    // Re-lock the base behind it: attackable again only after another inhibitor kill.
    for (MobaTowerState& other : _towers)
    {
        if (other.guardedByEntry != inhib.entry || other.destroyed)
            continue;

        if (Creature* guarded = GetBgMap()->GetCreature(other.guid))
            guarded->SetUnitFlag(UnitFlags(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE));
    }

    // Super minions stop for the team that had knocked this inhibitor down (the enemy of its team).
    TeamId beneficiary = (inhib.team == TEAM_ALLIANCE) ? TEAM_HORDE : TEAM_ALLIANCE;
    _superMinionsActive[beneficiary] = false;
    BroadcastStructureEvent(inhib, MOBA_STRUCT_EVENT_RESPAWNED, nullptr);
}

void BattlegroundMOBA::SpawnWave(TeamId team, bool includeSiege)
{
    MobaWaveComposition const& comp = _waveComposition[team];

    for (uint32 entry : comp.byRole[MOBA_CREEP_ROLE_MELEE])
        SpawnCreep(entry);

    for (uint32 entry : comp.byRole[MOBA_CREEP_ROLE_CASTER])
        SpawnCreep(entry);

    if (includeSiege)
        for (uint32 entry : comp.byRole[MOBA_CREEP_ROLE_SIEGE])
            SpawnCreep(entry);

    // While the enemy inhibitor is down, this team fields its super minions each wave.
    if (_superMinionsActive[team])
        for (uint32 entry : comp.byRole[MOBA_CREEP_ROLE_SUPER])
            SpawnCreep(entry);
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
        _spawnedCreeps[summon->GetGUID()] = cfg->team;
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

    // Announced on the first spawn as well as every respawn -- a boss nobody was
    // told about is a boss nobody contests. Gated on aliveCount so a camp whose
    // summons all failed announces nothing.
    if (camp.tier && camp.aliveCount)
        BroadcastBossEvent(camp, MOBA_BOSS_EVENT_SPAWNED, TEAM_NEUTRAL);
}

// Scheduled twice over a camp's life: by StartingEventOpenDoors ahead of the
// first spawn, and by NotifyNeutralDied ahead of every respawn. Both subtract
// the same lead from the delay they are pacing, so the warning cannot drift off
// the spawn it announces when the camp is retuned.
void BattlegroundMOBA::WarnBossRespawn(uint32 campIndex)
{
    if (GetStatus() != STATUS_IN_PROGRESS || campIndex >= _camps.size())
        return;

    MobaCampState const& camp = _camps[campIndex];
    if (!camp.tier)
        return;

    BroadcastBossEvent(camp, MOBA_BOSS_EVENT_SPAWNING, TEAM_NEUTRAL, camp.spawnWarnMs / 1000);
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

void BattlegroundMOBA::NotifyNeutralDied(Creature* member, Unit* killer)
{
    MobaCampState* camp = FindCampOf(member->GetGUID());
    if (!camp || !camp->aliveCount)
        return;

    if (--camp->aliveCount == 0 && GetStatus() == STATUS_IN_PROGRESS)
    {
        uint32 campIndex = static_cast<uint32>(camp - _camps.data());
        _bgEvents.ScheduleEvent(EVENT_MOBA_SPAWN_CAMP_FIRST + campIndex, Milliseconds(camp->respawnMs));

        if (camp->tier)
        {
            // The killing BLOW's side, which is the same rule GrantDeathDrops pays
            // the team-wide rows on -- so the line and the payout can never name
            // different teams. ResolveKillerTeam answers TEAM_NEUTRAL when nothing
            // resolves, and the payload has a side value for that.
            BroadcastBossEvent(*camp, MOBA_BOSS_EVENT_SLAIN, ResolveKillerTeam(killer));

            // The generator already rejects a lead >= respawnMs; this guard is what
            // keeps hand-edited SQL from scheduling the warning in the past.
            if (camp->spawnWarnMs && camp->respawnMs > camp->spawnWarnMs)
                _bgEvents.ScheduleEvent(EVENT_MOBA_BOSS_WARN_FIRST + campIndex,
                    Milliseconds(camp->respawnMs - camp->spawnWarnMs));
        }
    }
}

void BattlegroundMOBA::FreezeAllCreeps()
{
    for (auto const& itr : _spawnedCreeps)
    {
        Creature* creep = GetBgMap()->GetCreature(itr.first);
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

void BattlegroundMOBA::StartRespawnTimer(Player* player, bool instant /*= false*/)
{
    if (!player)
        return;

    // One timer per player; re-clicking Release must not restart the countdown.
    if (_respawnTimers.find(player->GetGUID()) != _respawnTimers.end())
        return;

    uint32 waitMs = 0;
    if (!instant)
    {
        uint32 baseMs = 10000, perMinMs = 1500, capMs = 60000;
        if (MobaBaseConfig const* cfg = sMobaBaseDataStore->GetConfig(GetMapId()))
        {
            baseMs   = cfg->respawnBaseMs;
            perMinMs = cfg->respawnPerMinMs;
            capMs    = cfg->respawnCapMs;
        }

        // Grows continuously with match time (measured from doors-open, so the prep
        // phase is excluded), clamped to the cap.
        waitMs = std::min<uint32>(capMs,
            baseMs + static_cast<uint32>(static_cast<uint64>(perMinMs) * _matchElapsedMs / 60000));
    }

    MobaRespawnState state;
    state.remainingMs = waitMs;
    _respawnTimers[player->GetGUID()] = state;

    // Seed the client-side revive countdown; the addon ticks it down locally
    // (like the T: clock) and shows the center-screen number.
    SendHudMessage(player, Acore::StringFormat("R:{}", (waitMs + 999) / 1000));
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

    // Dismiss the client countdown (it self-hides at 0, but nail it here in case
    // the local tick hasn't quite reached 0 at the moment of revive).
    SendHudMessage(player, "R:0");
}

// Fountain heal: players inside their own spawn dome regain a % of max
// health/mana per tick. The radius is the dome's (mod_moba_base.FountainRadius),
// so the heal zone and the visible dome cannot drift apart. The compare is 2D,
// making the zone a cylinder -- forgiving of the base's terrain slope.
void BattlegroundMOBA::UpdateFountainHealing(uint32 diff)
{
    MobaBaseConfig const* cfg = sMobaBaseDataStore->GetConfig(GetMapId());
    if (!cfg || !cfg->fountainTickMs || (!cfg->fountainHpPct && !cfg->fountainManaPct))
        return;

    if (cfg->fountainRadius <= 0.0f)
        return;

    float radiusSq = cfg->fountainRadius * cfg->fountainRadius;

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

// The trickle that funds a build even for a player farming badly, and the reason a
// losing lane is not a dead one. Paid to EVERYONE including the dead: respawning
// already costs time on the map, and taxing it twice is what stops a team that is
// behind from ever coming back.
void BattlegroundMOBA::UpdatePassiveGold(uint32 diff)
{
    MobaBaseConfig const* cfg = sMobaBaseDataStore->GetConfig(GetMapId());
    if (!cfg || !cfg->passiveTickMs || !cfg->passiveCopper)
        return;

    _passiveGoldMs += diff;
    if (_passiveGoldMs < cfg->passiveTickMs)
        return;

    // Catch up rather than drop ticks. A world update longer than the cadence would
    // otherwise silently pay less, making income depend on server load -- the kind
    // of thing nobody notices until the economy has been tuned around it.
    uint32 ticks = _passiveGoldMs / cfg->passiveTickMs;
    _passiveGoldMs %= cfg->passiveTickMs;

    // Not AwardTeamGold: passive income has no team concept, and going through it
    // twice would walk the player list twice to say the same thing.
    for (auto const& itr : GetPlayers())
        if (Player* player = itr.second)
            AddMatchGold(player, cfg->passiveCopper * ticks);
}

// One addon packet. LANG_ADDON marks this as addon traffic client-side; the
// chat-type byte is irrelevant to delivery.
void BattlegroundMOBA::SendAddonPacket(Player* player, char const* prefix, std::string const& body)
{
    if (!player)
        return;

    std::string message = Acore::StringFormat("{}\t{}", prefix, body);

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

void BattlegroundMOBA::SendHudMessage(Player* player, std::string const& body)
{
    SendAddonPacket(player, MOBA_HUD_ADDON_PREFIX, body);
}

void BattlegroundMOBA::SendShopMessage(Player* player, std::string const& body)
{
    SendAddonPacket(player, MOBA_SHOP_ADDON_PREFIX, body);
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
        uint32 hk = score->GetHonorableKills();   // credited kills (own killing blows + assists)
        a  = hk > k ? hk - k : 0;                  // assists = credited kills minus own killing blows
        cs = score->CreepKills;
    }

    return Acore::StringFormat("S:{},{},{},{},{},{},{}",
        _teamPlayerKills[team], _teamPlayerKills[other], k, d, a, cs, GetMatchGold(player));
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

    // Re-arm the revive countdown for a player who reloaded / rejoined while dead.
    // Persistent per-player state, unlike the transient kill feed (never re-sent).
    auto itr = _respawnTimers.find(player->GetGUID());
    if (itr != _respawnTimers.end())
        SendHudMessage(player, Acore::StringFormat("R:{}", (itr->second.remainingMs + 999) / 1000));
}

// Emit a transient kill-feed line to every player, tailored per recipient: a POV
// flag (you got the kill / you died / bystander) and team-relative sides so the
// addon colours names blue/red without guessing factions (CFBG-safe). Player
// kills only -- called from HandlePlayerDeath with a resolved killer. `flag` is a
// BG_MOBA_KillFlag, which the addon renders as a tag on this line rather than a
// second one, so a shutdown cannot claim two of the feed's five slots.
void BattlegroundMOBA::BroadcastKillFeed(Player* killer, Player* victim, uint32 flag)
{
    if (!killer || !victim)
        return;

    TeamId killerTeam = killer->GetBgTeamId();
    TeamId victimTeam = victim->GetBgTeamId();
    std::string killerName = killer->GetName();
    std::string victimName = victim->GetName();
    // uint32 (not uint8) on purpose: fmt renders uint8 as a character.
    uint32 killerClass = killer->getClass();
    uint32 victimClass = victim->getClass();

    for (auto const& itr : GetPlayers())
    {
        Player* recipient = itr.second;
        if (!recipient)
            continue;

        TeamId team = recipient->GetBgTeamId();
        uint32 pov = 2; // bystander
        if (recipient->GetGUID() == killer->GetGUID())
            pov = 0;
        else if (recipient->GetGUID() == victim->GetGUID())
            pov = 1;

        uint32 killerSide = (killerTeam == team) ? 0u : 1u; // 0 = recipient's team (blue)
        uint32 victimSide = (victimTeam == team) ? 0u : 1u;

        SendHudMessage(recipient, Acore::StringFormat("K:{},{},{},{},{},{},{},{}",
            pov, killerName, killerClass, killerSide, victimName, victimClass, victimSide, flag));
    }
}

// Classify a non-player killer into a HUD category, using the BG's own guid state
// (a pet-landed blow never reaches here -- ResolveKillCredit credits its owner).
uint32 BattlegroundMOBA::ClassifyKiller(Unit* killer) const
{
    if (!killer)
        return 0; // environment (fall, fatigue, suicide)

    ObjectGuid guid = killer->GetGUID();

    for (MobaTowerState const& t : _towers)
        if (t.guid == guid)
            return 1; // tower

    if (_spawnedCreeps.count(guid))
        return 2; // lane creep

    for (MobaCampState const& c : _camps)
        if (std::find(c.memberGuids.begin(), c.memberGuids.end(), guid) != c.memberGuids.end())
            return 3; // neutral camp

    return 0; // unknown creature -> fall back to "the environment"
}

TeamId BattlegroundMOBA::ResolveKillerTeam(Unit* killer) const
{
    if (!killer)
        return TEAM_NEUTRAL;

    // Pets and guardians answer for their owner, matching how both JustDied
    // callers resolve a killing-blow player.
    if (Player* player = killer->GetCharmerOrOwnerPlayerOrPlayerItself())
        return player->GetBgTeamId();

    ObjectGuid guid = killer->GetGUID();

    for (MobaTowerState const& t : _towers)
        if (t.guid == guid)
            return t.team;

    auto itr = _spawnedCreeps.find(guid);
    if (itr != _spawnedCreeps.end())
        return itr->second;

    return TEAM_NEUTRAL;
}

// Transient feed line for a death with no crediting enemy player. Broadcast to all,
// tailored per recipient (POV + team-relative victim colour); the source category is
// resolved once. The addon owns the label text and icon for each category.
void BattlegroundMOBA::BroadcastNonPlayerDeath(Player* victim, Unit* killer)
{
    if (!victim)
        return;

    TeamId victimTeam = victim->GetBgTeamId();
    std::string victimName = victim->GetName();
    uint32 victimClass = victim->getClass(); // uint32: fmt renders uint8 as a character
    uint32 cat = ClassifyKiller(killer);

    for (auto const& itr : GetPlayers())
    {
        Player* recipient = itr.second;
        if (!recipient)
            continue;

        uint32 pov   = (recipient->GetGUID() == victim->GetGUID()) ? 0u : 1u;
        uint32 vSide = (victimTeam == recipient->GetBgTeamId()) ? 0u : 1u;

        SendHudMessage(recipient, Acore::StringFormat("D:{},{},{},{},{}",
            pov, vSide, victimClass, victimName, cat));
    }
}

// Emit a boss event to every player, tailored per recipient. `team` is the side
// the event belongs to, and is TEAM_NEUTRAL for both spawn events -- a boss
// appearing is nobody's news, which is why the side field has a third value that
// K:/O:/X: never needed. `arg` is the lead time in seconds on "spawning soon"
// and 0 everywhere else, the same always-present rule N: uses.
//
// The name comes from creature_template rather than a live creature: the warning
// fires while every member is dead, so there is nothing left to ask. Member 0 is
// the boss by convention -- a tiered camp is a single mob today.
void BattlegroundMOBA::BroadcastBossEvent(MobaCampState const& camp, uint32 event, TeamId team, uint32 arg)
{
    std::string name;
    if (!camp.members.empty())
        if (CreatureTemplate const* tmpl = sObjectMgr->GetCreatureTemplate(camp.members[0].entry))
            name = tmpl->Name;

    for (auto const& itr : GetPlayers())
    {
        Player* recipient = itr.second;
        if (!recipient)
            continue;

        uint32 side = MOBA_BOSS_SIDE_NOBODY;
        if (team == TEAM_ALLIANCE || team == TEAM_HORDE)
            side = (team == recipient->GetBgTeamId()) ? MOBA_BOSS_SIDE_OURS : MOBA_BOSS_SIDE_ENEMY;

        SendHudMessage(recipient, Acore::StringFormat("B:{},{},{},{}", event, side, arg, name));
    }
}

// Emit a structure event to every player, tailored per recipient: which side owns
// the structure, plus enough shape (kind/tier/lane) for the addon to name it. The
// addon owns all wording. `actor` is nullptr when a creep finished the structure
// (npc_moba_tower passes no lastHitter), and the payload's trailing field is then
// EMPTY -- the Lua pattern uses [^,]* for it precisely so that still matches.
//
// This cannot fold into BroadcastKillFeed or BroadcastNonPlayerDeath: both take a
// Player* victim and a structure has none. BroadcastBossEvent exists separately
// for the same reason.
void BattlegroundMOBA::BroadcastStructureEvent(MobaTowerState const& tower, uint32 event, Player* actor)
{
    std::string actorName = actor ? actor->GetName() : "";
    // uint32 (not uint8) on purpose: fmt renders uint8 as a character.
    uint32 kind = tower.kind;
    uint32 tier = tower.tier;
    uint32 lane = tower.lane;

    for (auto const& itr : GetPlayers())
    {
        Player* recipient = itr.second;
        if (!recipient)
            continue;

        // tower.team is the OWNER, never the destroyer: a structure falling is bad
        // news for its own side, whoever landed the blow.
        uint32 ownerSide = (tower.team == recipient->GetBgTeamId()) ? 0u : 1u;

        SendHudMessage(recipient, Acore::StringFormat("O:{},{},{},{},{},{}",
            event, ownerSide, kind, tier, lane, actorName));
    }
}

// Emit a kill-streak line to every player, tailored per recipient. `subject` is
// the player it is about, and is nullptr for an ace -- which belongs to a team,
// not a person. The payload's name field is then EMPTY, so the Lua pattern reads
// it with [^,]* for exactly the reason O:'s trailing actor does. `team` is the
// side the line is GOOD news for, which for an ace is the team left standing.
void BattlegroundMOBA::BroadcastStreak(Player* subject, TeamId team, uint32 type, uint32 count)
{
    std::string name = subject ? subject->GetName() : "";

    for (auto const& itr : GetPlayers())
    {
        Player* recipient = itr.second;
        if (!recipient)
            continue;

        uint32 pov  = (subject && recipient->GetGUID() == subject->GetGUID()) ? 0u : 1u;
        uint32 side = (team == recipient->GetBgTeamId()) ? 0u : 1u;

        SendHudMessage(recipient, Acore::StringFormat("X:{},{},{},{},{}",
            pov, side, name, type, count));
    }
}

// Emit a match-flow notice to every player. The server picks WHICH notice and the
// addon owns every word, as with O: and X:. This is the one feed line that needs
// no per-recipient tailoring -- the minion notices say the same thing to both
// teams -- so it goes out as a single broadcast. `arg` carries any number the
// wording needs; the addon decides whether its line uses one.
void BattlegroundMOBA::BroadcastNotice(uint32 code, uint32 arg)
{
    BroadcastHudMessage(Acore::StringFormat("N:{},{}", code, arg));
}

// The one notice that is NOT the same for everyone: each player is told whether
// THEY won, never which faction did. TEAM_NEUTRAL is a real outcome here --
// Battleground::GetPrematureWinner returns it when neither side still fields
// enough players -- and there is no honest victory or defeat line for it, so it
// takes the one line that is the same for everyone.
//
// That branch is UNREACHABLE wherever battleground_template.MinPlayersPerTeam is
// 1, and EotS's is 1 deliberately -- a MOBA keeps playing 4v5. "Neither team
// meets a min of 1" means zero players total, and Battleground::Update returns on
// an empty BG before the status switch. Kept as a guard, not dead code: a mode
// whose template carries a real minimum reaches it, and without it that match ends
// on a frozen bar that never says why.
void BattlegroundMOBA::BroadcastMatchResult(TeamId winnerTeamId)
{
    if (winnerTeamId != TEAM_ALLIANCE && winnerTeamId != TEAM_HORDE)
    {
        BroadcastNotice(MOBA_NOTICE_DRAW);
        return;
    }

    for (auto const& itr : GetPlayers())
    {
        Player* recipient = itr.second;
        if (!recipient)
            continue;

        uint32 code = (recipient->GetBgTeamId() == winnerTeamId) ? MOBA_NOTICE_VICTORY : MOBA_NOTICE_DEFEAT;
        SendHudMessage(recipient, Acore::StringFormat("N:{},0", code));
    }
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
