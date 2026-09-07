#ifndef __BATTLEGROUNDMOBA_H
#define __BATTLEGROUNDMOBA_H

#include "Battleground.h"
#include "BattlegroundScore.h"
#include "EventMap.h"
#include "ObjectGuid.h"
#include "MobaNeutralData.h"
#include "MobaCreepData.h"
#include "MobaTowerData.h"
#include "MobaPlayerDropData.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>

// The 3.3.5a client splits CHAT_MSG_ADDON on the first TAB, so the wire format is
// "<prefix>\t<payload>".
constexpr char MOBA_HUD_ADDON_PREFIX[]  = "MobaHUD";
constexpr char MOBA_SHOP_ADDON_PREFIX[] = "MobaShop";

enum BG_MOBA_CreatureTypes
{
    BG_MOBA_CREATURE_FIXED_MAX      = 0 // towers take dynamic slots from 0; see SetupBattleground
};

enum BG_MOBA_ObjectTypes
{
    BG_MOBA_OBJECT_DOOR_A                         = 0,
    BG_MOBA_OBJECT_DOOR_H                         = 1,
    BG_MOBA_OBJECT_MAX                            = 2
};

// PostUpdateImpl dispatches the ranged ids with >= tests in DESCENDING order, so the
// bases must stay far apart and the highest must be tested first -- a base added
// below an existing test is swallowed by it. Bare ids are matched with == ahead of
// that chain; one added without its own == test vanishes.
enum BG_MOBA_Events
{
    EVENT_MOBA_SPAWN_WAVE = 1,
    EVENT_MOBA_WAVE_WARN  = 2,
    EVENT_MOBA_SPAWN_CAMP_FIRST    = 100,   // + camp index into _camps
    EVENT_MOBA_RESPAWN_INHIB_FIRST = 1000,  // + tower index into _towers
    EVENT_MOBA_INHIB_WARN_FIRST    = 2000,  // + tower index into _towers
    EVENT_MOBA_BOSS_WARN_FIRST     = 3000   // + camp index into _camps
};

enum BG_MOBA_Recall
{
    BG_MOBA_RECALL_SPELL        = 8690,  // Hearthstone; redirected to base by moba_recall.cpp
    BG_MOBA_RECALL_ITEM         = 6948,  // Hearthstone item; granted in AddPlayer
    BG_MOBA_RECALL_EMPOWER_AURA = 1243   // PLACEHOLDER empower trigger (PW:F R1); swap for the real mechanic
};

enum BG_MOBA_StructureEvent   // "O:" event field
{
    MOBA_STRUCT_EVENT_DESTROYED  = 0,
    MOBA_STRUCT_EVENT_RESPAWNING = 1,
    MOBA_STRUCT_EVENT_RESPAWNED  = 2
};

enum BG_MOBA_KillFlag         // "K:" flag field
{
    MOBA_KILL_FLAG_NONE        = 0,
    MOBA_KILL_FLAG_FIRST_BLOOD = 1,
    MOBA_KILL_FLAG_SHUTDOWN    = 2
};

enum BG_MOBA_StreakType       // "X:" type field
{
    MOBA_STREAK_MULTI = 0,   // kills inside the rolling window
    MOBA_STREAK_SPREE = 1,   // consecutive kills without dying
    MOBA_STREAK_ACE   = 2    // a whole team down at once; no subject player
};

enum BG_MOBA_Notice           // "N:" code field
{
    MOBA_NOTICE_MINIONS_SOON    = 1,
    MOBA_NOTICE_MINIONS_SPAWNED = 2,
    MOBA_NOTICE_VICTORY         = 3,
    MOBA_NOTICE_DEFEAT          = 4,
    MOBA_NOTICE_DRAW            = 5,
    MOBA_NOTICE_SURRENDER_ENEMY = 6,
    MOBA_NOTICE_SURRENDER_OWN   = 7
};

enum BG_MOBA_SurrenderResult
{
    MOBA_SURRENDER_PASSED        = 0,  // threshold met; the match has ended
    MOBA_SURRENDER_VOTE_STARTED  = 1,
    MOBA_SURRENDER_VOTE_COUNTED  = 2,
    MOBA_SURRENDER_VOTE_FAILED   = 3,  // this ballot put the threshold out of reach
    MOBA_SURRENDER_NOT_IN_MATCH  = 4,
    MOBA_SURRENDER_TOO_EARLY     = 5,  // secondsRemaining set
    MOBA_SURRENDER_ON_COOLDOWN   = 6,  // secondsRemaining set
    MOBA_SURRENDER_ALREADY_VOTED = 7,
    MOBA_SURRENDER_NO_VOTE       = 8   // refused a vote that is not running
};

enum BG_MOBA_BossEvent        // "B:" event field
{
    MOBA_BOSS_EVENT_SLAIN    = 0,
    MOBA_BOSS_EVENT_SPAWNING = 1,
    MOBA_BOSS_EVENT_SPAWNED  = 2
};

enum BG_MOBA_BossSide         // "B:" side field
{
    MOBA_BOSS_SIDE_OURS   = 0,
    MOBA_BOSS_SIDE_ENEMY  = 1,
    MOBA_BOSS_SIDE_NOBODY = 2   // a spawn belongs to no team
};

enum BG_MOBA_GoldSource       // "G:" source field; also gates whether one is sent
{
    MOBA_GOLD_SILENT    = 0,   // wallet moves, nothing floats
    MOBA_GOLD_CORPSE    = 1,   // world float off the corpse
    MOBA_GOLD_KILL      = 2,   // the rest float off the bar's gold column
    MOBA_GOLD_STRUCTURE = 3,
    MOBA_GOLD_OBJECTIVE = 4
};

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

// One config row is one unit per wave, so a role holds as many entries as the
// config placed.
struct MobaWaveComposition
{
    std::vector<uint32> byRole[MOBA_CREEP_ROLE_MAX];
};

struct MobaSurrenderVote
{
    ObjectGuid initiator;
    GuidUnorderedSet yes;
    GuidUnorderedSet no;
    uint32 endsAtMs = 0;        // 0 = no vote running
    uint32 blockedUntilMs = 0;  // outlives the vote that earned it; CloseSurrenderVote clears the rest
};

struct MobaCampState
{
    uint32 campId = 0;
    uint8 tier = 0;             // 0 = ordinary camp; nonzero = boss
    uint32 initialSpawnMs = 0;
    uint32 respawnMs = 0;
    uint32 spawnWarnMs = 0;
    std::vector<MobaNeutralMember> members;
    std::vector<ObjectGuid> memberGuids;   // live summons, rebuilt on every (re)spawn
    uint32 aliveCount = 0;
};

struct MobaRespawnState
{
    uint32 remainingMs = 0;
};

// Both counters are per-life and die together in one erase.
struct MobaStreakState
{
    uint32 spree = 0;        // consecutive kills since the last death
    uint32 multi = 0;        // kills inside the rolling multi-kill window
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
    // Starts a vote, or casts a ballot in the one already running. `secondsRemaining`
    // is written only on MOBA_SURRENDER_TOO_EARLY and MOBA_SURRENDER_ON_COOLDOWN.
    BG_MOBA_SurrenderResult HandleSurrenderRequest(Player* player, bool agree, uint32& secondsRemaining);
    bool UpdatePlayerScore(Player* player, uint32 type, uint32 value, bool doAddHonor = true) override;

    std::vector<MobaTowerState>& GetTowers() { return _towers; }

    // `lastHitter` is nullptr when a creep finished it: the team payout still lands,
    // the last-hit bonus does not.
    void OnTowerDestroyed(Creature* tower, TeamId winnerTeamId, Player* lastHitter);

    // Last-hit credit (CS) for a lane creep or a jungle neutral.
    void CreditCreepKill(Player* killer);

    // On-death drops for any minion. `killer` is the killing-blow player the caller is
    // willing to REWARD; `killerUnit` only names a side for team-wide drops when no
    // player did. A caller that rewards nothing must pass nullptr for BOTH --
    // killerUnit alone would resolve straight back to a team.
    void GrantDeathDrops(Creature* victim, Player* killer, Unit* killerUnit);

    // Straight to the killer -- no corpse, no native loot, unlike GrantDeathDrops.
    void GrantPlayerKillDrops(Player* killer);

    void RecordPlayerDamage(Player* victim, Player* attacker);
    void RecordAllyHeal(Player* ally, Player* healer);   // no duration gate; overheal counts
    void RecordAllyBuff(Player* ally, Player* buffer, int32 buffMaxDurationMs);
    void HandlePlayerDeath(Player* victim, Unit* killer);

    // `count` is what we GAVE, which is not item->GetCount() when the grant merged
    // into a stack the player already held.
    void RecordGrantedItem(Player* player, Item* item, uint32 count);
    uint32 GetGrantedCount(Player* player, uint32 itemEntry) const;
    void ForgetGrantedItem(Player* player, Item* item, uint32 count);

    // Match wallet in copper -- deliberately NOT Player money, so no path that skips
    // a cleanup can touch real character wealth.
    uint32 GetMatchGold(Player* player) const;
    void AddMatchGold(Player* player, uint32 copper, BG_MOBA_GoldSource source,
                      std::string const& name = "");   // `name` reaches the client only for MOBA_GOLD_CORPSE
    bool SpendMatchGold(Player* player, uint32 copper);   // false = short, nothing spent

    void PullCampMates(Creature* member, Unit* attacker);
    void NotifyNeutralDied(Creature* member, Unit* killer);

    // `instant` revives on the next tick instead of waiting out the scaled timer.
    void StartRespawnTimer(Player* player, bool instant = false);

    void SendHudStateTo(Player* player);

    // The shop panel is addon-only: a player who never sent HELLO has no shop, and
    // there is deliberately no gossip fallback.
    void SetShopAddonReady(Player* player);
    bool HasShopAddon(Player* player) const;
    void SendShopMessage(Player* player, std::string const& body);

    // Inside the player's own base circle -- shares the fountain's radius and test.
    bool IsInShopRange(Player* player) const;

    // Silent unless the answer CHANGED. `force` is for the HELLO handshake: a /reload
    // leaves the cached value matching a client that has just forgotten it.
    void SendShopRange(Player* player, bool force = false);

    // 0 = no override; Spell::prepare then keeps the spell's default cast time.
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
    // Flat per player, NOT a split pot: team size never dilutes an objective.
    void AwardTeamGold(TeamId team, uint32 copper, BG_MOBA_GoldSource source);
    // The IsAlive() gate is the DESIGN rule, not belt-and-braces: Unit::AddAura drops
    // dead targets but exempts any spell carrying SPELL_ATTR2_ALLOW_DEAD_TARGET.
    void AwardTeamBuff(TeamId team, uint32 spell, uint32 durationMs);
    void UpdateShopRange(uint32 diff);
    Player* ResolveKillCredit(Player* victim, Unit* killer);
    uint32 GetKillCreditWindowMs() const;
    uint32 GetAssistWindowMs() const;
    uint32 GetAssistBuffMaxDurationMs() const;
    uint32 GetSurrenderMinMs() const;
    uint32 GetSurrenderVoteMs() const;
    uint32 GetSurrenderCooldownMs() const;
    uint32 GetSurrenderVotesNeeded(TeamId team) const;
    uint32 CountSurrenderVotes(TeamId team, bool agree) const;
    BG_MOBA_SurrenderResult ResolveSurrenderVote(TeamId team);
    void CloseSurrenderVote(TeamId team, bool startCooldown);
    void UpdateSurrenderVotes();
    void ExecuteSurrender(TeamId loser);
    void AnnounceToTeam(TeamId team, std::string const& text);
    void AnnounceSurrenderTally(TeamId team);

    // Payload grammar: MobaHUD.lua dispatches on the leading letter, and the anchored
    // patterns in Feed.lua and Bar.lua are what each body must match.
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
    void UpdateKillStreak(Player* killer);
    void CheckAce(TeamId wipedTeam);
    void WarnInhibitorRespawn(uint32 towerIndex);
    uint32 ClassifyKiller(Unit* killer) const;   // 0 env, 1 tower, 2 lane creep, 3 neutral
    TeamId ResolveKillerTeam(Unit* killer) const;   // TEAM_NEUTRAL = nobody to pay
    void SendScoreboard(Player* player);
    void BroadcastScoreboard();
    std::string BuildScoreboardBody(Player* player) const;

    // All three derive from _towers on every read rather than caching. With a core gated
    // on EVERY inhibitor and super minions gated per lane, "unlock what this one guarded"
    // stopped being a local decision: one inhibitor respawning has to re-lock a core that
    // a different inhibitor's death opened.
    bool IsStructureLocked(MobaTowerState const& structure) const;
    void RefreshStructureLocks();
    bool SuperMinionsActive(TeamId team, uint8 lane) const;

    EventMap _bgEvents;
    std::vector<MobaTowerState> _towers;
    MobaWaveComposition _waveComposition[2];
    std::unordered_map<ObjectGuid, TeamId> _spawnedCreeps; // team: a creep's killing blow must name a side
    std::vector<MobaCampState> _camps;
    uint32 _waveCount = 0;
    uint32 _matchElapsedMs = 0;   // time since doors opened (excludes prep phase)
    uint32 _hudResyncMs = 0;
    uint32 _fountainTickMs = 0;
    uint32 _passiveGoldMs = 0;
    uint32 _teamPlayerKills[2] = {0, 0}; // enemy-player kills per team (the "X vs Y" score)
    MobaSurrenderVote _surrenderVote[2];
    // victim -> (enemy attacker -> last damage/debuff ms). Per life; keeps every recent
    // attacker, not just the latest.
    std::unordered_map<ObjectGuid, std::unordered_map<ObjectGuid, uint32>> _recentAttackers;

    // Two views: GUIDs say WHICH item (gear carries a random suffix), the per-entry
    // ledger says HOW MANY (splitting a stack clones it under a new GUID, and looting
    // merges ours into theirs). Gear never stacks and stackables never carry a suffix,
    // so the pair is exact for both.
    std::unordered_map<ObjectGuid, std::vector<ObjectGuid>> _grantedItems;
    std::unordered_map<ObjectGuid, std::unordered_map<uint32, uint32>> _grantedCounts;

    // ally -> (supporter -> last heal/short-buff ms). Same per-life lifetime.
    std::unordered_map<ObjectGuid, std::unordered_map<ObjectGuid, uint32>> _allySupport;

    std::unordered_map<ObjectGuid, uint32> _wallets;   // player -> match wallet (copper)

    std::unordered_map<ObjectGuid, MobaRespawnState> _respawnTimers;

    std::unordered_map<ObjectGuid, MobaStreakState> _streaks;   // absent entry IS a zero streak
    bool _firstBlood = false;

    std::unordered_set<ObjectGuid> _shopAddonPlayers;   // sent HELLO; everyone else has no shop
    std::unordered_map<ObjectGuid, bool> _shopInRange;  // last known; only transitions reach the client
    uint32 _shopRangeMs = 0;
};
#endif
