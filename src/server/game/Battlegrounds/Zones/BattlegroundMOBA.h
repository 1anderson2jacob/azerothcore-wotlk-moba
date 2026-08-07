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
#include "MobaCreepData.h"
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
    EVENT_MOBA_WAVE_WARN  = 2,   // one-shot "minions incoming"; no index, so no range
    // Four ranges keyed by a small container index:
    //   EVENT_MOBA_SPAWN_CAMP_FIRST + camp index (into _camps)      -- jungle camp (re)spawn.
    //   EVENT_MOBA_RESPAWN_INHIB_FIRST + tower index (into _towers) -- inhibitor return.
    //   EVENT_MOBA_INHIB_WARN_FIRST + tower index (into _towers)    -- "respawning soon" feed line.
    //   EVENT_MOBA_BOSS_WARN_FIRST + camp index (into _camps)       -- boss "spawning soon" feed line.
    // PostUpdateImpl dispatches by threshold in DESCENDING order, so keep the
    // bases far apart, above any realistic camp/tower count, and test the highest
    // base first -- a new base added below an existing test is swallowed by it.
    // The bare ids above are matched with == ahead of that chain; a new bare id
    // added WITHOUT its own == test falls through every >= branch and vanishes.
    EVENT_MOBA_SPAWN_CAMP_FIRST    = 100,
    EVENT_MOBA_RESPAWN_INHIB_FIRST = 1000,
    EVENT_MOBA_INHIB_WARN_FIRST    = 2000,
    EVENT_MOBA_BOSS_WARN_FIRST     = 3000
};

enum BG_MOBA_Recall
{
    BG_MOBA_RECALL_SPELL        = 8690,  // Hearthstone; redirected to base by moba_recall.cpp
    BG_MOBA_RECALL_ITEM         = 6948,  // Hearthstone item; granted in AddPlayer
    BG_MOBA_RECALL_EMPOWER_AURA = 1243   // PLACEHOLDER empower trigger (Power Word: Fortitude R1); swap for the real mechanic
};

// `event` field of the "O:" HUD payload. The addon owns all wording; this is
// only the shape of what happened.
enum BG_MOBA_StructureEvent
{
    MOBA_STRUCT_EVENT_DESTROYED  = 0,
    MOBA_STRUCT_EVENT_RESPAWNING = 1,
    MOBA_STRUCT_EVENT_RESPAWNED  = 2
};

// `flag` field of the "K:" HUD payload -- what made this kill special. First
// blood and a shutdown are mutually exclusive by construction, so one field
// suffices: nobody can be on a spree before the match's first kill.
enum BG_MOBA_KillFlag
{
    MOBA_KILL_FLAG_NONE        = 0,
    MOBA_KILL_FLAG_FIRST_BLOOD = 1,
    MOBA_KILL_FLAG_SHUTDOWN    = 2
};

// `type` field of the "X:" HUD payload. As with structure events, the addon owns
// every word -- the server sends a shape and a count, never a name, so the
// Double/Triple/Rampage/Legendary ladder lives in exactly one file (Feed.lua).
enum BG_MOBA_StreakType
{
    MOBA_STREAK_MULTI = 0,   // Double/Triple/... -- kills inside the rolling window
    MOBA_STREAK_SPREE = 1,   // consecutive kills without dying
    MOBA_STREAK_ACE   = 2    // a whole team down at once; no subject player
};

// `code` field of the "N:" HUD payload -- a match-flow notice. Same split as O:
// and X:: the server picks WHICH notice, the addon owns every word. Victory and
// defeat are two codes rather than one plus a side flag, because they are the
// only pair here that differs per recipient -- the minion notices say the same
// thing to both teams and go out as one broadcast. Codes start at 1; there has
// never been a 0.
//
// The payload's trailing `arg` is a number the wording needs and the client
// cannot know -- currently only the warning's lead time. Always present, 0 when
// unused, so the Lua pattern stays one anchored match with no optional group.
enum BG_MOBA_Notice
{
    MOBA_NOTICE_MINIONS_SOON    = 1,
    MOBA_NOTICE_MINIONS_SPAWNED = 2,
    MOBA_NOTICE_VICTORY         = 3,
    MOBA_NOTICE_DEFEAT          = 4
};

// `event` field of the "B:" HUD payload -- something happened to a boss (a
// neutral camp with a nonzero tier). Same split as O:/X:/N:: the server sends a
// shape, the addon owns every word.
enum BG_MOBA_BossEvent
{
    MOBA_BOSS_EVENT_SLAIN    = 0,
    MOBA_BOSS_EVENT_SPAWNING = 1,
    MOBA_BOSS_EVENT_SPAWNED  = 2
};

// `side` field of the "B:" payload. K:/O:/X: only ever had to say "yours or
// theirs", because a kill and a structure both belong to someone. A boss
// spawning belongs to nobody, and no two-state field can say that -- which is
// the whole reason boss lines are not O: lines.
enum BG_MOBA_BossSide
{
    MOBA_BOSS_SIDE_OURS   = 0,
    MOBA_BOSS_SIDE_ENEMY  = 1,
    MOBA_BOSS_SIDE_NOBODY = 2
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
    uint8 lane = MOBA_LANE_NONE;
    uint32 guardedByEntry = 0;
    uint8 kind = MOBA_STRUCTURE_TOWER;
    uint32 respawnMs = 0;
    uint32 teamGoldCopper = 0;
    uint32 lastHitGoldCopper = 0;
    bool destroyed = false;
};

// Cached once in SetupBattleground() from mod_moba_creep_data: every entry to
// spawn for each role, per team, so wave-spawn doesn't need to re-query. Wave
// size is data-driven -- one row (one creep_config.yaml creep) is one unit per
// wave, so a role holds as many entries as the config placed. Roles differ only
// in when they spawn: siege every third wave, super while the enemy inhibitor is
// down. See SpawnWave.
struct MobaWaveComposition
{
    std::vector<uint32> byRole[MOBA_CREEP_ROLE_MAX];
};

// Runtime state of one neutral (jungle) camp: the static member list from
// mod_moba_neutral_members plus the live summon GUIDs, rebuilt on every
// (re)spawn. aliveCount hitting 0 schedules the camp's respawn event.
struct MobaCampState
{
    uint32 campId = 0;
    uint8 tier = 0;
    uint32 initialSpawnMs = 0;
    uint32 respawnMs = 0;
    uint32 spawnWarnMs = 0;
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

// Per-player kill streak. Both counters are per-life and die together, which is
// why they share one struct and one erase: `spree` is consecutive kills since
// the last death, `multi` is how many landed inside the rolling multi-kill
// window. `lastKillMs` is what ages that window out.
struct MobaStreakState
{
    uint32 spree = 0;
    uint32 multi = 0;
    uint32 lastKillMs = 0;
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

    // Called only from npc_moba_tower::JustDied, which is the one place that sees a
    // structure's true killing blow -- Unit::Kill overwrites the killer with the
    // loot recipient before HandleKillUnit runs. `lastHitter` is nullptr when a
    // creep finished it: the team payout still lands, the last-hit bonus does not.
    void OnTowerDestroyed(Creature* tower, TeamId winnerTeamId, Player* lastHitter);

    // Credit a lane-creep last-hit (CS) to the killing-blow player. Called from
    // npc_moba_creep::JustDied -- unlike HandleKillUnit, whose killer is the loot
    // recipient (first player to aggro), this is the true last hit.
    void CreditCreepKill(Player* killer);

    // On-death drops for any minion (lane creep or neutral), called from both
    // JustDied choke points. `killer` is the killing-blow player the caller is
    // willing to REWARD (nullptr = none, or refused by the caller's own policy);
    // `killerUnit` is the raw unit that landed the blow, and exists only to name a
    // side for team-wide drops when no player did. The two are separate because
    // npc_moba_creep nulls `killer` on an own-team kill -- policy this function
    // cannot re-derive, since it would resolve that same unit straight back to a
    // team -- so a caller that rewards nothing must pass nullptr for BOTH. Grants
    // buff drops, injects gold into the corpse loot, and re-points native loot
    // rights -- the tapper's group by default -- at the killer's team; with no
    // rewarded killer it strips the corpse instead.
    void GrantDeathDrops(Creature* victim, Player* killer, Unit* killerUnit);

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

    // Remember that the match handed `count` of `item` to the player. Called from
    // npc_moba_store -- for shop purchases, and via the loot hook for creep and
    // neutral item drops. `count` is what we GAVE, which is not item->GetCount()
    // when the grant merged into a stack the player already held.
    void RecordGrantedItem(Player* player, Item* item, uint32 count);

    // How many of an entry the match still owes: what may be sold, and what will
    // be destroyed on exit. 0 = the player brought every copy in themselves.
    uint32 GetGrantedCount(Player* player, uint32 itemEntry) const;

    // Give up the claim on `count` of an item after selling it.
    void ForgetGrantedItem(Player* player, Item* item, uint32 count);

    // The match wallet, in copper. Deliberately NOT Player money: real character
    // wealth is never touched, so no path that skips a cleanup -- crash, hard
    // disconnect, worldserver restart -- can delete or duplicate it. It mirrors
    // copper rather than being a new named resource, so store prices, sell ratios
    // and the client's GetCoinTextureString all keep meaning what they meant.
    // Both mutators push the wallet to the owner; nothing else has to remember to.
    uint32 GetMatchGold(Player* player) const;
    void AddMatchGold(Player* player, uint32 copper);
    bool SpendMatchGold(Player* player, uint32 copper);   // false = short, nothing spent

    // League camp-link: called from npc_moba_neutral::JustEngagedWith so
    // hitting one camp member pulls the rest onto the attacker.
    void PullCampMates(Creature* member, Unit* attacker);

    // Called from npc_moba_neutral::JustDied; starts the camp's respawn timer
    // once its last member is down. `killer` is only read for a boss camp, to
    // name the side that took it.
    void NotifyNeutralDied(Creature* member, Unit* killer);

    // Starts a player's respawn countdown (called from the death and released-ghost
    // hooks). `instant` revives on the next battleground tick instead of waiting out
    // the scaled timer, for deaths that are not meant to cost anything.
    void StartRespawnTimer(Player* player, bool instant = false);

    // Reply to a MobaHUD client "ready" ping with this player's current HUD state.
    void SendHudStateTo(Player* player);

    // MobaShop handshake and panel handoff. The panel is addon-only: a player who
    // never sent HELLO has no shop at all -- there is deliberately no gossip
    // fallback to keep in sync.
    void SetShopAddonReady(Player* player);
    bool HasShopAddon(Player* player) const;
    void SendShopMessage(Player* player, std::string const& body);

    // Whether the player stands inside their own base circle, the only place
    // trading is allowed. Shares the fountain's radius and test: being able to
    // shop and being able to regen are deliberately one place, not two.
    bool IsInShopRange(Player* player) const;

    // Pushes RANGE:0/1 to one player. Silent unless the answer CHANGED, so a
    // player standing still generates no traffic. `force` is for the HELLO
    // handshake: a /reload leaves the cached value matching a client that has
    // just forgotten it, and without it the panel would never be told.
    void SendShopRange(Player* player, bool force = false);

    // Per-map recall cast time (ms) for a player currently in a MOBA BG; 0 = no
    // override (use the spell's default). Read by Spell::prepare to retime Hearthstone.
    static uint32 GetRecallCastTimeMs(Player* player);

private:
    void PostUpdateImpl(uint32 diff) override;
    void SpawnWave(TeamId team, bool includeSiege);
    void SpawnCreep(uint32 entry);
    void RespawnInhibitor(uint32 towerIndex);
    void SpawnCamp(uint32 campIndex);
    void WarnBossRespawn(uint32 campIndex);
    MobaCampState* FindCampOf(ObjectGuid guid);
    void FreezeAllCreeps();
    void UpdateRespawnTimers(uint32 diff);
    void RespawnAtBase(Player* player);
    void UpdateFountainHealing(uint32 diff);
    void UpdatePassiveGold(uint32 diff);
    // Pay every player on `team` who is in the battleground. The delivery mode for
    // OBJECTIVES -- towers, and boss neutrals via MOBA_DROP_TEAM_GOLD -- which have
    // no corpse to walk up to: killing one is a team event, so the payout is one.
    // Flat per player, NOT a split pot: the config number reads as what the
    // objective is worth to you, and team size never dilutes it.
    void AwardTeamGold(TeamId team, uint32 copper);
    // The buff half of the same idea. The IsAlive() gate is the DESIGN rule, not
    // belt-and-braces: Unit::AddAura already drops dead targets, but exempts any
    // spell carrying SPELL_ATTR2_ALLOW_DEAD_TARGET, so without this an innocuous
    // config spell change could silently start buffing corpses.
    void AwardTeamBuff(TeamId team, uint32 spell, uint32 durationMs);
    void UpdateShopRange(uint32 diff);
    Player* ResolveKillCredit(Player* victim, Unit* killer);
    uint32 GetKillCreditWindowMs() const;
    uint32 GetAssistWindowMs() const;
    uint32 GetAssistBuffMaxDurationMs() const;

    // MobaHUD addon feed (client/addons/MobaHUD). `body` is the payload after the
    // "MobaHUD\t" prefix: "T:<sec>" clock start/sync, "E" hide the bar,
    // "S:<ally>,<enemy>,<k>,<d>,<a>,<cs>,<gold>" scoreboard, "R:<sec>" revive-countdown
    // start (0 = hide), "K:<pov>,<killer>,<kClass>,<kSide>,<victim>,<vClass>,<vSide>,<flag>"
    // a player kill line (flag = BG_MOBA_KillFlag), "D:<pov>,<vSide>,<vClass>,<victim>,<cat>"
    // a non-player death line (cat 0=env 1=tower 2=creep 3=neutral),
    // "O:<event>,<ownerSide>,<kind>,<tier>,<lane>,<actor>" a structure event
    // (event 0=destroyed 1=respawning soon 2=respawned; ownerSide 0=recipient's team;
    // kind = MobaStructureKind; lane = MobaLane; actor EMPTY when creeps finished it),
    // "X:<pov>,<side>,<name>,<type>,<count>" a kill-streak line
    // (type = BG_MOBA_StreakType; name EMPTY for an ace, which has no subject player),
    // "B:<event>,<side>,<arg>,<name>" a boss event (event = BG_MOBA_BossEvent;
    // side = BG_MOBA_BossSide, and 2 = nobody is what every spawn carries;
    // arg = lead seconds on "spawning soon", 0 elsewhere; name from creature_template),
    // and "N:<code>,<arg>" a match-flow notice (code = BG_MOBA_Notice; arg is a
    // number the wording needs, 0 when unused).
    // K:/D:/O:/X:/B: are built per recipient; N: only for victory/defeat.
    void SendAddonPacket(Player* player, char const* prefix, std::string const& body);
    void SendHudMessage(Player* player, std::string const& body);
    void BroadcastHudMessage(std::string const& body);
    void BroadcastKillFeed(Player* killer, Player* victim, uint32 flag);
    void BroadcastNonPlayerDeath(Player* victim, Unit* killer);
    void BroadcastStructureEvent(MobaTowerState const& tower, uint32 event, Player* actor);
    void BroadcastStreak(Player* subject, TeamId team, uint32 type, uint32 count);
    void BroadcastNotice(uint32 code, uint32 arg = 0);
    void BroadcastBossEvent(MobaCampState const& camp, uint32 event, TeamId team, uint32 arg = 0);
    void BroadcastMatchResult(TeamId winnerTeamId);
    // Advance the killer's streak counters and announce whatever they crossed.
    void UpdateKillStreak(Player* killer);
    // Announce an ace if `wipedTeam` has nobody left standing.
    void CheckAce(TeamId wipedTeam);
    void WarnInhibitorRespawn(uint32 towerIndex);
    uint32 ClassifyKiller(Unit* killer) const;   // 0 env, 1 tower, 2 lane creep, 3 neutral
    // Which side a killing blow belongs to when no player landed it: towers and
    // lane creeps carry their owner's team. TEAM_NEUTRAL = nobody to pay -- a
    // jungle mob, the environment, or a unit in neither registry.
    TeamId ResolveKillerTeam(Unit* killer) const;
    void SendScoreboard(Player* player);
    void BroadcastScoreboard();
    std::string BuildScoreboardBody(Player* player) const;

    EventMap _bgEvents;
    std::vector<MobaTowerState> _towers;
    MobaWaveComposition _waveComposition[2];
    bool _superMinionsActive[2] = {false, false}; // per beneficiary team: enemy inhibitor down -> super minions in waves
    // Team rides along because a creep that lands a killing blow has to name its
    // side for team-wide drops, which a bare GUID list could not do.
    std::unordered_map<ObjectGuid, TeamId> _spawnedCreeps;
    std::vector<MobaCampState> _camps;
    uint32 _waveCount = 0;
    uint32 _matchElapsedMs = 0;   // time since doors opened (excludes prep phase)
    uint32 _hudResyncMs = 0;      // accumulates toward the next periodic HUD re-broadcast
    uint32 _fountainTickMs = 0;   // accumulates toward the next fountain heal tick
    uint32 _passiveGoldMs = 0;    // accumulates toward the next passive income tick
    uint32 _teamPlayerKills[2] = {0, 0}; // enemy-player kills per team (the "X vs Y" score)
    // victim GUID -> (enemy-attacker GUID -> last damage/debuff time, ms). Per life:
    // cleared on the victim's death and when they leave. Keeps every recent attacker
    // (not just the latest) so assist-split and bounties can read it later.
    std::unordered_map<ObjectGuid, std::unordered_map<ObjectGuid, uint32>> _recentAttackers;

    // What the match handed each player. Two views, because neither alone is
    // right: GUIDs identify WHICH item (gear carries a random suffix, so the
    // entry is ambiguous), and the per-entry ledger says HOW MANY (splitting a
    // stack clones it under a new GUID -- Player::SplitItem -> Item::CloneItem --
    // and looting merges ours into theirs, so a GUID's count is not our count).
    // Gear never stacks and stackables never carry a suffix, so the two failure
    // modes are disjoint and the pair is exact for both.
    std::unordered_map<ObjectGuid, std::vector<ObjectGuid>> _grantedItems;
    std::unordered_map<ObjectGuid, std::unordered_map<uint32, uint32>> _grantedCounts;

    // ally GUID -> (supporter GUID -> last heal/short-buff time, ms). Feeds the LoL
    // assist chain: healing or a short combat buff on a kill participant links the
    // supporter to the kill. Same per-life lifetime as _recentAttackers.
    std::unordered_map<ObjectGuid, std::unordered_map<ObjectGuid, uint32>> _allySupport;

    // player GUID -> match wallet (copper). No exit hook and no cleanup sweep by
    // design: nothing outside this map ever held the number, so there is nothing
    // to reconcile when it dies with the battleground.
    std::unordered_map<ObjectGuid, uint32> _wallets;

    std::unordered_map<ObjectGuid, MobaRespawnState> _respawnTimers;

    // Per-player streaks, and the match's one-shot first-blood latch. Entries are
    // erased on the owner's death (HandlePlayerDeath) and on exit (RemovePlayer);
    // an absent entry IS a zero streak, so nothing has to be pre-seeded.
    std::unordered_map<ObjectGuid, MobaStreakState> _streaks;
    bool _firstBlood = false;

    // Players whose client announced the shop panel (HELLO). Everyone else gets
    // the gossip fallback, which goes away once the panel ships.
    std::unordered_set<ObjectGuid> _shopAddonPlayers;

    // player GUID -> whether they were last known to be inside their own base
    // circle. Only transitions reach the client; this is what makes that test.
    std::unordered_map<ObjectGuid, bool> _shopInRange;
    uint32 _shopRangeMs = 0;      // accumulates toward the next shop range poll

};
#endif
