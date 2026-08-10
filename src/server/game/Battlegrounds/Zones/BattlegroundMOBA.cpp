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
    // Both of these run ahead of the status guard. Buying starting gear during prep
    // is intended, and a prep-phase death is otherwise permanent -- an instanced map
    // has no spirit healer and Player::Update skips its auto-release.
    UpdateShopRange(diff);
    UpdateRespawnTimers(diff);

    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    _matchElapsedMs += diff;

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

            // Not in SpawnWave, which runs once per team and would say it twice.
            if (_waveCount == 1)
                BroadcastNotice(MOBA_NOTICE_MINIONS_SPAWNED);
        }
        else if (eventId == EVENT_MOBA_WAVE_WARN)
            BroadcastNotice(MOBA_NOTICE_MINIONS_SOON, MOBA_WAVE_WARN_MS / 1000);
        // Descending order is mandatory -- see BG_MOBA_Events.
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

    StartTimedAchievement(ACHIEVEMENT_TIMED_TYPE_EVENT, BG_MOBA_EVENT_START_BATTLE);

    _bgEvents.ScheduleEvent(EVENT_MOBA_SPAWN_WAVE, Milliseconds(MOBA_WAVE_INTERVAL_MS));

    // Scheduled once and never re-armed: only the first wave is announced.
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

    BroadcastHudMessage("T:0");
    BroadcastScoreboard();
}

void BattlegroundMOBA::EndBattleground(TeamId winnerTeamId)
{
    // The core's own double-end guard sits below this, in
    // Battleground::EndBattleground(PvPTeamId) -- past the broadcasts.
    if (GetStatus() == STATUS_WAIT_LEAVE)
        return;

    // A separate payload, not a field on "E": the client tests E by exact match.
    BroadcastMatchResult(winnerTeamId);
    BroadcastHudMessage("E");

    Battleground::EndBattleground(winnerTeamId);
}

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

        // The HUD's clock, not Battleground::GetStartTime(), which also counts prep:
        // "available at 1:00" has to mean the 1:00 on the bar.
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

        // Resolved before announcing: a team that meets the threshold on the
        // initiator alone never has a vote worth talking about.
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
// size - 1 a duo surrenders on one player's say-so.
uint32 BattlegroundMOBA::GetSurrenderVotesNeeded(TeamId team) const
{
    uint32 const size = GetPlayersCountByTeam(team);
    return (size <= 2) ? size : size - 1;
}

// The roster a vote is measured against is the one standing now, not the one that
// started it.
uint32 BattlegroundMOBA::CountSurrenderVotes(TeamId team, bool agree) const
{
    GuidUnorderedSet const& ballots = agree ? _surrenderVote[team].yes : _surrenderVote[team].no;

    uint32 count = 0;
    for (ObjectGuid const& guid : ballots)
        if (IsPlayerInBattleground(guid))
            ++count;

    return count;
}

BG_MOBA_SurrenderResult BattlegroundMOBA::ResolveSurrenderVote(TeamId team)
{
    uint32 const needed = GetSurrenderVotesNeeded(team);

    if (CountSurrenderVotes(team, true) >= needed)
    {
        CloseSurrenderVote(team, false);
        ExecuteSurrender(team);
        return MOBA_SURRENDER_PASSED;
    }

    // Out of reach: everyone who has not already refused voting yes still falls
    // short. Refusals only come from this team, so the subtraction cannot underflow.
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
        // A pass ends the match; resolving the other team's vote too would send a
        // second N: pair.
        if (GetStatus() != STATUS_IN_PROGRESS)
            return;

        TeamId const team = TeamId(i);
        if (!_surrenderVote[i].endsAtMs)
            continue;

        // An empty team's threshold is zero, which every vote trivially meets.
        if (!GetPlayersCountByTeam(team))
        {
            CloseSurrenderVote(team, false);
            continue;
        }

        // Every tick, not only when someone votes: a player leaving shrinks the
        // threshold, so a vote can pass with no new ballot cast.
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

    // Ahead of EndBattleground, which sends the VICTORY/DEFEAT pair: this line
    // says WHY, that one says what.
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

// System chat, not the kill feed: "N:<code>,<arg>" carries one number, which cannot
// say "2 of 4" or name the initiator.
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

    if (!player->HasItemCount(BG_MOBA_RECALL_ITEM))
        player->AddItem(BG_MOBA_RECALL_ITEM, 1);

    // Recall must be up the moment they enter; each cast clears it again.
    player->RemoveSpellCooldown(BG_MOBA_RECALL_SPELL, true);

    // SetupBattleground normally loads the stores, but it runs from
    // Battleground::_ProcessJoin -- the first BG tick, AFTER players have ported in.
    // On the first match of a process this hook beats it.
    sMobaBaseDataStore->LoadIfNeeded();

    // AddPlayer runs again on a reconnect and the wallet outlives RemovePlayer, so an
    // absent wallet entry is the "never been paid" test.
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
    // Covers every early exit (Leave, logout, GM removal); EndBattleground covers
    // the normal path.
    SendHudMessage(player, "E");

    if (player)
    {
        _recentAttackers.erase(player->GetGUID());
        _allySupport.erase(player->GetGUID());
        _streaks.erase(player->GetGUID());
        _shopAddonPlayers.erase(player->GetGUID());
        _shopInRange.erase(player->GetGUID());

        // Two passes: the ledger alone would let DestroyItemCount pick the player's
        // own copy of a shared entry while a soulbound one of ours escapes. The GUID
        // pass takes exactly what we handed over; the ledger pass mops up whatever
        // was split off it under a new GUID.
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

                // Never take more of a stack than the match gave: StoreLootItem merges
                // ours into one the player already held.
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
            // This runs in the tick the player is pulled from the world, so the normal
            // flush never reaches the client -- and the 3.3.5 client relocates its own
            // player object on a map change rather than recreating it, so stripped gear
            // stays rendered until relog. Send the changed values synchronously.
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

    // Spawn domes, centered on the same team start position respawn and the fountain
    // read. The zero quaternion is deliberate: SetWorldRotation derives the rotation
    // from the orientation when the quat is zero, which is the Z-axis spin a dome wants.
    Position const* allianceStart = GetTeamStartPosition(TEAM_ALLIANCE);
    Position const* hordeStart    = GetTeamStartPosition(TEAM_HORDE);

    AddObject(BG_MOBA_OBJECT_DOOR_A, baseCfg->domeEntryAlliance,
        allianceStart->GetPositionX(), allianceStart->GetPositionY(), allianceStart->GetPositionZ(),
        allianceStart->GetOrientation(), 0.0f, 0.0f, 0.0f, 0.0f, RESPAWN_IMMEDIATELY);
    AddObject(BG_MOBA_OBJECT_DOOR_H, baseCfg->domeEntryHorde,
        hordeStart->GetPositionX(), hordeStart->GetPositionY(), hordeStart->GetPositionZ(),
        hordeStart->GetOrientation(), 0.0f, 0.0f, 0.0f, 0.0f, RESPAWN_IMMEDIATELY);

    // towers
    _towers.clear();
    _towers.reserve(towerConfigs.size());
    for (size_t i = 0; i < towerConfigs.size(); ++i)
    {
        MobaTowerConfig const& cfg = towerConfigs[i];
        uint32 slot = BG_MOBA_CREATURE_FIXED_MAX + static_cast<uint32>(i);
        // DAY, not the default 0: AddCreature applies a respawn delay only when one is
        // passed, so 0 leaves Creature's own 300s in place and every structure quietly
        // returns ~6 minutes after dying, still flagged destroyed here. Inhibitors are
        // unaffected -- RespawnInhibitor forces a respawn that ignores this.
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

            // Guarded towers stay unattackable until their guard falls (OnTowerDestroyed).
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

    // creep wave composition
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

    // neutral camps -- optional content, so a map with none warns rather than fails
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
    // Intentionally empty. The engine calls this only on a player/pet killing blow, but
    // MOBA deaths are as often finished by a creep, tower, or the environment, so all
    // crediting and death tallying lives in HandlePlayerDeath. Scoring here would
    // double-count the player-blow case.
}

void BattlegroundMOBA::HandleKillUnit(Creature* /*creature*/, Player* /*killer*/)
{
    // Intentionally empty. Unit::Kill reassigns player = creature->GetLootRecipient()
    // before calling this hook, so `killer` here is the first TAPPER, not the killing
    // blow -- wrong for a tower's last-hit bonus and wrong for creep CS. Both are
    // credited from the creature AI's JustDied, which receives the real killer.
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
    // Post-match kills reward nothing, which is what keeps frozen creeps and camps
    // farmproof. Unit::Kill already filled native loot for the first tapper's group, so
    // this must strip the corpse rather than merely skip.
    if (GetStatus() != STATUS_IN_PROGRESS)
    {
        victim->loot.clear();
        victim->RemoveDynamicFlag(UNIT_DYNFLAG_LOOTABLE);
        victim->SetLootRecipient(nullptr);
        return;
    }

    // Team-wide drops follow the killing BLOW's side, which need not be a player's: a
    // creep that finishes a boss still pays its own team. Personal drops stay last-hit.
    TeamId rewardTeam = killer ? killer->GetBgTeamId() : ResolveKillerTeam(killerUnit);

    if (killer)
    {
        // Ours follow the killing blow, not the tapper's group. None of the three is
        // redundant: withGroup=false would zero the recipient group and
        // Player::isAllowedToLoot then rejects every grouped looter (i.e. everyone in a
        // BG); GROUP_LOOT admits anyone when roundRobinPlayer is unset; and SendLoot
        // broadcasts a GroupLoot window for over-threshold items unless loot_type is
        // already stamped. All cosmetic -- moba_loot_rights_globalscript enforces it.
        victim->SetLootRecipient(killer);
        victim->loot.roundRobinPlayer = killer->GetGUID();
        victim->loot.loot_type        = LOOT_CORPSE;
    }
    else
    {
        // No last hit, no CORPSE. The team-wide rows below are NOT forfeited with it --
        // that is why this is a branch and not an early return.
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

    // Gold-only minions have lootid 0, so Unit::Kill saw empty loot and marked the
    // corpse fully-looted; flag it now that gold was injected. Guarded on `killer`:
    // the no-last-hit branch above just stripped the corpse and must not re-flag it.
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

    // A combat cooldown (Power Infusion, PW:S), not a maintenance buff (Fortitude,
    // Blessing of Wisdom). Permanent auras report -1.
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
    // An enemy player who landed the blow (pets/totems credit the owner) wins outright;
    // the window below only matters when no crediting player finished it.
    Player* direct = killer ? killer->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
    if (direct && direct != victim && direct->GetBgTeamId() != victim->GetBgTeamId()
        && IsPlayerInBattleground(direct->GetGUID()))
        return direct;

    auto itr = _recentAttackers.find(victim->GetGUID());
    if (itr == _recentAttackers.end())
        return nullptr;

    // Otherwise: the most recent enemy player to damage or debuff the victim inside the
    // window. getMSTimeDiff is wraparound-safe; "most recent" = smallest elapsed.
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

    // The engine scores deaths only through HandleKillPlayer, which we no-op'd.
    UpdatePlayerScore(victim, SCORE_DEATHS, 1);

    MobaBaseConfig const* cfg = sMobaBaseDataStore->GetConfig(GetMapId());

    // Read BEFORE the erase at the bottom: the victim's streak is the whole definition
    // of a shutdown, and it is gone the moment this death is booked.
    uint32 victimSpree = 0;
    if (auto itr = _streaks.find(victim->GetGUID()); itr != _streaks.end())
        victimSpree = itr->second.spree;

    Player* creditKiller = ResolveKillCredit(victim, killer);
    if (creditKiller && creditKiller != victim)
    {
        UpdatePlayerScore(creditKiller, SCORE_HONORABLE_KILLS, 1);
        UpdatePlayerScore(creditKiller, SCORE_KILLING_BLOWS, 1);

        // Participants on the killer's team: the killer, everyone who damaged or
        // debuffed the victim inside the window, then -- expanded to a fixed point --
        // everyone who healed or short-buffed a participant. Each pass adds only
        // distinct players, so the fixed point cannot exceed team size.
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

        if (auto itr = _recentAttackers.find(victim->GetGUID()); itr != _recentAttackers.end())
            for (auto const& rec : itr->second)
                if (inWindow(rec.second))
                    if (Player* a = teammateInBg(rec.first))
                        participants.insert(a->GetGUID());

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

        // The killer already has both HK and KB above, so their assist column stays 0.
        for (ObjectGuid const& guid : participants)
            if (guid != creditKiller->GetGUID())
                if (Player* p = ObjectAccessor::FindPlayer(guid))
                    UpdatePlayerScore(p, SCORE_HONORABLE_KILLS, 1);

        ++_teamPlayerKills[team];
        GrantPlayerKillDrops(creditKiller);

        // The two flags cannot collide: first blood means no victim can be carrying a
        // spree yet. Line and money are separately gated, so a map that configures no
        // bounty is still told a shutdown happened.
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
        BroadcastNonPlayerDeath(victim, killer);
    }

    // All three are per-life. The streak erase sits OUTSIDE the credited-kill branch:
    // dying to a creep ends a spree exactly as surely as dying to a player does.
    _recentAttackers.erase(victim->GetGUID());
    _allySupport.erase(victim->GetGUID());
    _streaks.erase(victim->GetGUID());

    // Last, so the sweep sees a fully-booked death.
    CheckAce(victim->GetBgTeamId());

    BroadcastScoreboard();
}

void BattlegroundMOBA::UpdateKillStreak(Player* killer)
{
    if (!killer)
        return;

    MobaBaseConfig const* cfg = sMobaBaseDataStore->GetConfig(GetMapId());
    uint32 const now = GameTime::GetGameTimeMS().count();

    MobaStreakState& st = _streaks[killer->GetGUID()];
    ++st.spree;

    // The window runs from the PREVIOUS kill, not the first of the chain, so a steady
    // stream keeps extending one multi-kill. A zero window disables multi-kills: `multi`
    // never leaves 1 and the >= 2 test below never fires.
    uint32 const windowMs = cfg ? cfg->multiKillWindowMs : 0;
    if (st.multi && windowMs && getMSTimeDiff(st.lastKillMs, now) <= windowMs)
        ++st.multi;
    else
        st.multi = 1;
    st.lastKillMs = now;

    TeamId const team = killer->GetBgTeamId();

    // Spree first, so a kill that is both lands the multi-kill on top -- it is the rarer
    // of the two and the one worth reading.
    if (cfg && cfg->spreeMin && st.spree >= cfg->spreeMin)
        BroadcastStreak(killer, team, MOBA_STREAK_SPREE, st.spree);

    if (st.multi >= 2)
        BroadcastStreak(killer, team, MOBA_STREAK_MULTI, st.multi);
}

// Swept after every death rather than kept as a counter: respawns, disconnects and
// mid-match joins all move the number. Called with the team that just LOST someone.
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

        // The player who just died already reads dead: OnUnitDeath is the last statement
        // in Unit::Kill, long after setDeathState.
        if (player->IsAlive())
            return;

        ++total;
    }

    // A solo player wiping is a kill, not an ace. minTeam keeps the line meaningful in
    // a 1v1 test match.
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

    // Ahead of the core branch's early return, or a base kill would announce nothing.
    BroadcastStructureEvent(*itr, MOBA_STRUCT_EVENT_DESTROYED, lastHitter);

    // Behind the `destroyed` guard so nothing can double-pay, and unconditional on team
    // so a creep-finished structure still rewards the push. A re-killed inhibitor pays
    // AGAIN on purpose: taking the same objective twice is worth the same twice.
    AwardTeamGold(winnerTeamId, itr->teamGoldCopper);
    if (lastHitter)
        AddMatchGold(lastHitter, itr->lastHitGoldCopper);

    // Unlock any structures this one was guarding.
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

    if (itr->kind == MOBA_STRUCTURE_CORE)
    {
        FreezeAllCreeps();
        EndBattleground(winnerTeamId);
        return;
    }

    if (itr->kind == MOBA_STRUCTURE_INHIBITOR)
    {
        // The beneficiary is the enemy of the OWNER, never winnerTeamId (the killer's
        // team, per npc_moba_tower::JustDied). Those agree in a real push and diverge on
        // an own-team kill -- and since RespawnInhibitor clears the flag from the owner,
        // any mismatch here leaks super minions for the rest of the match.
        TeamId beneficiary = (itr->team == TEAM_ALLIANCE) ? TEAM_HORDE : TEAM_ALLIANCE;
        _superMinionsActive[beneficiary] = true;

        if (itr->respawnMs)
        {
            uint32 towerIndex = static_cast<uint32>(std::distance(_towers.begin(), itr));
            _bgEvents.ScheduleEvent(EVENT_MOBA_RESPAWN_INHIB_FIRST + towerIndex, Milliseconds(itr->respawnMs));

            // A warning that fires at or after the thing it warns about is worse than none.
            if (itr->respawnMs > MOBA_INHIB_RESPAWN_WARN_MS)
                _bgEvents.ScheduleEvent(EVENT_MOBA_INHIB_WARN_FIRST + towerIndex,
                    Milliseconds(itr->respawnMs - MOBA_INHIB_RESPAWN_WARN_MS));
        }
    }
}

// Silent if the inhibitor is already back: the warn is a separate scheduled event and
// nothing cancels it if the timeline changes under it.
void BattlegroundMOBA::WarnInhibitorRespawn(uint32 towerIndex)
{
    if (GetStatus() != STATUS_IN_PROGRESS || towerIndex >= _towers.size())
        return;

    MobaTowerState const& inhib = _towers[towerIndex];
    if (!inhib.destroyed)
        return;

    BroadcastStructureEvent(inhib, MOBA_STRUCT_EVENT_RESPAWNING, nullptr);
}

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

// Creeps are TempSummons, not Battleground::AddCreature/BgCreatures -- that registry is
// fixed-size and persistent, wrong for repeatedly-spawned ephemerals. Two engine traps:
// Map::SummonCreature takes no TempSummonType (only WorldObject's overload does) and
// TempSummon's constructor defaults to TEMPSUMMON_MANUAL_DESPAWN, so skipping the
// SetTempSummonType below means no creep ever cleans up; and it must be
// TIMED_DESPAWN_OUT_OF_COMBAT, since the CORPSE_ variant's countdown never advances
// while the creep is alive.
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

// CORPSE_TIMED_DESPAWN, unlike SpawnCreep: that type's countdown only runs once the
// creature is dead, which is the point here -- a living camp never despawns, and a
// corpse vanishes long before the respawn event re-summons the whole camp.
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

    // Gated on aliveCount so a camp whose summons all failed announces nothing.
    if (camp.tier && camp.aliveCount)
        BroadcastBossEvent(camp, MOBA_BOSS_EVENT_SPAWNED, TEAM_NEUTRAL);
}

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

// AttackStart works on a REACT_DEFENSIVE mate -- react states gate only self-initiated
// aggro. The status guard matters: post-match camps are frozen passive but DamageTaken
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
            // Same rule GrantDeathDrops pays the team-wide rows on, so the line and the
            // payout can never name different teams.
            BroadcastBossEvent(*camp, MOBA_BOSS_EVENT_SLAIN, ResolveKillerTeam(killer));

            // The generator already rejects a lead >= respawnMs; this guard is what keeps
            // hand-edited SQL from scheduling the warning in the past.
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

        waitMs = std::min<uint32>(capMs,
            baseMs + static_cast<uint32>(static_cast<uint64>(perMinMs) * _matchElapsedMs / 60000));
    }

    MobaRespawnState state;
    state.remainingMs = waitMs;
    _respawnTimers[player->GetGUID()] = state;

    // The addon ticks this down locally, like the T: clock.
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

    // The countdown self-hides at 0, but nail it here in case the local tick has not
    // quite reached 0 at the moment of revive.
    SendHudMessage(player, "R:0");
}

// The radius is the spawn dome's, so the heal zone and the visible dome cannot drift
// apart. The compare is 2D, making the zone a cylinder -- forgiving of terrain slope.
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

// Paid to EVERYONE including the dead: respawning already costs time on the map.
void BattlegroundMOBA::UpdatePassiveGold(uint32 diff)
{
    MobaBaseConfig const* cfg = sMobaBaseDataStore->GetConfig(GetMapId());
    if (!cfg || !cfg->passiveTickMs || !cfg->passiveCopper)
        return;

    _passiveGoldMs += diff;
    if (_passiveGoldMs < cfg->passiveTickMs)
        return;

    // Catch up rather than drop ticks: a world update longer than the cadence would
    // otherwise pay less, making income depend on server load.
    uint32 ticks = _passiveGoldMs / cfg->passiveTickMs;
    _passiveGoldMs %= cfg->passiveTickMs;

    for (auto const& itr : GetPlayers())
        if (Player* player = itr.second)
            AddMatchGold(player, cfg->passiveCopper * ticks);
}

// LANG_ADDON is what marks this as addon traffic client-side; the chat-type byte is
// irrelevant to delivery.
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

// Team kills are team-relative (ally = the recipient's team) so the addon can colour
// segment 1 as "you".
std::string BattlegroundMOBA::BuildScoreboardBody(Player* player) const
{
    TeamId team  = player->GetBgTeamId();
    TeamId other = GetOtherTeamId(team);

    uint32 k = 0, d = 0, a = 0, cs = 0;
    auto itr = PlayerScores.find(player->GetGUID().GetCounter());
    if (itr != PlayerScores.end())
    {
        // Must go through BattlegroundMOBAScore*: GetDeaths / GetHonorableKills are
        // protected on BattlegroundScore, and the protected-member rule requires access
        // via the friended (derived) type.
        BattlegroundMOBAScore* score = static_cast<BattlegroundMOBAScore*>(itr->second);
        k  = score->GetKillingBlows();
        d  = score->GetDeaths();
        uint32 hk = score->GetHonorableKills();
        a  = hk > k ? hk - k : 0;   // assists = credited kills minus own killing blows
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
    // During warmup the clock stays frozen at 0:00 until StartingEventOpenDoors sends T:0.
    if (GetStatus() == STATUS_IN_PROGRESS)
        SendHudMessage(player, Acore::StringFormat("T:{}", _matchElapsedMs / 1000));

    // Persistent per-player state, unlike the transient kill feed, which is never re-sent.
    auto itr = _respawnTimers.find(player->GetGUID());
    if (itr != _respawnTimers.end())
        SendHudMessage(player, Acore::StringFormat("R:{}", (itr->second.remainingMs + 999) / 1000));
}

// Tailored per recipient: a POV flag and team-relative sides, so the addon colours names
// without guessing factions (CFBG-safe).
void BattlegroundMOBA::BroadcastKillFeed(Player* killer, Player* victim, uint32 flag)
{
    if (!killer || !victim)
        return;

    TeamId killerTeam = killer->GetBgTeamId();
    TeamId victimTeam = victim->GetBgTeamId();
    std::string killerName = killer->GetName();
    std::string victimName = victim->GetName();
    // uint32, not uint8: fmt renders uint8 as a character. Same below and in the other
    // Broadcast* builders.
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

    // Pets and guardians answer for their owner, matching how both JustDied callers
    // resolve a killing-blow player.
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

// A death with no crediting enemy player. The addon owns the label and icon per category.
void BattlegroundMOBA::BroadcastNonPlayerDeath(Player* victim, Unit* killer)
{
    if (!victim)
        return;

    TeamId victimTeam = victim->GetBgTeamId();
    std::string victimName = victim->GetName();
    uint32 victimClass = victim->getClass();
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

// `team` is TEAM_NEUTRAL for both spawn events -- a boss appearing is nobody's news. The
// name comes from creature_template, not a live creature: the warning fires while every
// member is dead. Member 0 is the boss by convention.
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

// `actor` is nullptr when a creep finished the structure, and the payload's trailing
// field is then EMPTY -- the Lua pattern uses [^,]* precisely so that still matches.
void BattlegroundMOBA::BroadcastStructureEvent(MobaTowerState const& tower, uint32 event, Player* actor)
{
    std::string actorName = actor ? actor->GetName() : "";
    uint32 kind = tower.kind;
    uint32 tier = tower.tier;
    uint32 lane = tower.lane;

    for (auto const& itr : GetPlayers())
    {
        Player* recipient = itr.second;
        if (!recipient)
            continue;

        // tower.team is the OWNER, never the destroyer: a structure falling is bad news
        // for its own side, whoever landed the blow.
        uint32 ownerSide = (tower.team == recipient->GetBgTeamId()) ? 0u : 1u;

        SendHudMessage(recipient, Acore::StringFormat("O:{},{},{},{},{},{}",
            event, ownerSide, kind, tier, lane, actorName));
    }
}

// `subject` is nullptr for an ace, which belongs to a team, not a person; the name field
// is then EMPTY. `team` is the side the line is GOOD news for.
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

void BattlegroundMOBA::BroadcastNotice(uint32 code, uint32 arg)
{
    BroadcastHudMessage(Acore::StringFormat("N:{},{}", code, arg));
}

// TEAM_NEUTRAL is a real outcome -- Battleground::GetPrematureWinner returns it when
// neither side still fields enough players -- and there is no honest victory or defeat
// line for it. Unreachable while MinPlayersPerTeam is 1 (EotS's is, deliberately), but
// kept: a mode with a real minimum reaches it, and without it that match ends on a
// frozen bar that never says why.
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

    // The dynamic_cast doubles as the "is this a MOBA BG" test.
    BattlegroundMOBA* moba = dynamic_cast<BattlegroundMOBA*>(player->GetBattleground());
    if (!moba)
        return 0;

    MobaBaseConfig const* cfg = sMobaBaseDataStore->GetConfig(moba->GetMapId());
    if (!cfg)
        return 0;

    // PLACEHOLDER trigger: until a real mechanic exists, carrying the aura buys the
    // reduced cast time. Falls back to normal when recallEmpoweredCastMs is 0.
    if (cfg->recallEmpoweredCastMs && player->HasAura(BG_MOBA_RECALL_EMPOWER_AURA))
        return cfg->recallEmpoweredCastMs;

    return cfg->recallCastMs;
}
