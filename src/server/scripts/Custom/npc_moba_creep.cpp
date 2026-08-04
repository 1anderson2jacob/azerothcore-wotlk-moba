#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "AllSpellScript.h"
#include "Battleground.h"
#include "BattlegroundMOBA.h"
#include "Map.h"
#include "MobaCreepData.h"
#include "MotionMaster.h"
#include "Spell.h"
#include "SpellAuraDefines.h"
#include "SpellInfo.h"
#include "WaypointMgr.h"

// Max 2D distance a creep may be dragged from its lane before it force-evades
// and resumes. The engine's own 30yd leash is deliberately SKIPPED while combat
// stays "fresh" (Creature::CanCreatureAttack: damage, melee proximity, and
// unreachable targets each refresh a ~17s window -- authentic WoW kiting), so
// without this hard cap a player at run speed can drag a wave across the map.
float constexpr MOBA_CREEP_LANE_CORRIDOR = 40.0f;

struct npc_moba_creep : public ScriptedAI
{
    npc_moba_creep(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _cfg = sMobaCreepDataStore->GetConfig(me->GetEntry());
        if (!_cfg)
        {
            LOG_ERROR("scripts.ai", "npc_moba_creep: no mod_moba_creep_data row for entry {}.", me->GetEntry());
            return;
        }

        _lanePath = sWaypointMgr->GetPath(_cfg->pathId);

        // Post-match: stay frozen where FreezeAllCreeps() left us. Reset()
        // re-fires on evade, and the re-arm below would restart the lane.
        if (MatchEnded())
            return;

        // Arm the lane on first spawn only -- Reset() re-fires on evade, and the
        // slot-type check keeps that from rewinding us to node 1. false =
        // non-repeating: a creep that reaches the lane's end holds and fights
        // there, never turns around and walks back.
        if (me->GetMotionMaster()->GetMotionSlotType(MOTION_SLOT_IDLE) != WAYPOINT_MOTION_TYPE)
            me->GetMotionMaster()->MoveWaypoint(_cfg->pathId, false);

        if (_cfg->role == MOBA_CREEP_ROLE_CASTER)
        {
            SetAutoAttackAllowed(false); // casts instead of melee-swinging
            scheduler.CancelAll();
            scheduler.Schedule(std::chrono::milliseconds(_cfg->intervalMs), [this](TaskContext context)
            {
                CastAtVictim(context);
            });
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (MatchEnded())
            return;

        if (_cfg)
        {
            _corridorCheckTimer += diff;
            if (_corridorCheckTimer >= 1000)
            {
                _corridorCheckTimer = 0;
                if (me->IsEngaged() && DistanceFromLane2d(me->GetPositionX(), me->GetPositionY()) > MOBA_CREEP_LANE_CORRIDOR + 15.0f)
                {
                    EnterEvadeMode(EVADE_REASON_OTHER);
                    return;
                }
            }
        }

        if (_cfg && _cfg->role == MOBA_CREEP_ROLE_CASTER)
        {
            scheduler.Update(diff);
            UpdateVictim(); // threat/target housekeeping; casters never DoMeleeAttackIfReady
            return;
        }

        ScriptedAI::UpdateAI(diff); // melee & siege: default engine behavior
    }

    void AttackStart(Unit* victim) override
    {
        if (_cfg && _cfg->role == MOBA_CREEP_ROLE_CASTER)
            AttackStartCaster(victim, _cfg->range);
        else
            ScriptedAI::AttackStart(victim);
    }

    // Lane-corridor rule measured against the lane path, NOT home position: the
    // waypoint generator stamps home to the creature's current position every
    // moving tick (WaypointMovementGenerator::DoUpdate), so a home-based corridor
    // followed the creep wherever a player dragged it. Players are the only
    // targets gated -- creeps/towers are lane-bound already, and gating them by
    // this rule blocked a tower push once (rejected at 41yd from a mid-drag home).
    bool CanAIAttack(Unit const* victim) const override
    {
        if (!victim->GetCharmerOrOwnerPlayerOrPlayerItself())
            return true;

        return DistanceFromLane2d(victim->GetPositionX(), victim->GetPositionY()) <= MOBA_CREEP_LANE_CORRIDOR;
    }

    // Node Ids are the DB point numbers, preserved in the truncated resume
    // paths, so this stays comparable across resumes.
    void WaypointReached(uint32 nodeId, uint32 /*pathId*/) override
    {
        if (nodeId > _highestReachedNodeId)
            _highestReachedNodeId = nodeId;
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        // Post-match: stay put where FreezeAllCreeps() left us.
        if (MatchEnded())
        {
            me->CombatStop(true);
            me->GetMotionMaster()->MoveIdle();
            return;
        }

        if (!_cfg)
        {
            ScriptedAI::EnterEvadeMode(why);
            return;
        }

        // Inlined CreatureAI::_EnterEvadeMode minus its RemoveEvadeAuras() call:
        // creeps evade at the end of EVERY skirmish, and RemoveEvadeAuras strips
        // all player-cast buffs from an unowned creature -- minion buffs should
        // run their full duration instead. Also skipped: the zone-script /
        // formation / summoner evade notifications (creeps have none of the
        // three). Mirror CreatureAI.cpp's _EnterEvadeMode when merging upstream.
        if (me->IsInEvadeMode())
            return;

        if (!me->IsAlive())
        {
            EngagementOver();
            return;
        }

        // Recursion guard, as in _EnterEvadeMode: CombatStop below purges combat
        // refs, which re-enters EnterEvadeMode; IsInEvadeMode() above catches it.
        me->AddUnitState(UNIT_STATE_EVADE);

        me->ClearComboPointHolders();
        me->CombatStop(true);
        me->LoadCreaturesAddon(true);
        me->SetLootRecipient(nullptr);
        me->ResetPlayerDamageReq();
        me->ClearLastLeashExtensionTimePtr();
        me->SetCannotReachTarget();

        // MANDATORY: clears the AI's _isEngaged latch. Without it the creep is
        // permanently "already fighting" after its first evade -- new enemies
        // never trigger EngagementStart (JustStartedThreateningMe gates on it)
        // and on-sight aggro is skipped, so the creep ignores every later wave.
        // Omitting this line shipped as a real bug once.
        EngagementOver();

        // LoL-style leashing: no run-back. The default evade would
        // MoveTargetedHome() to the last-reached node and resume from there --
        // a creep that chased 30yd forward runs all 30yd back first. Instead,
        // resume the lane near where combat ended. UNIT_STATE_EVADE is normally
        // cleared by the home-return generator we're skipping, so clear it here
        // (as the engine's pet/MoveFollow evade branch does).
        me->ClearUnitState(UNIT_STATE_EVADE);
        ResumeLaneFromHere();
    }

    // Last-hit CS: credit goes to whoever landed the killing blow, not the first
    // tapper. HandleKillUnit's killer is the loot recipient (Unit::Kill overwrites
    // it), so it can't award last hits -- but the killer passed here is the true
    // killing-blow attacker; a pet/guardian credits its owner. Enemy players only:
    // friendly-fire on creeps is impossible anyway (team hostility + spell gate),
    // so the team guard is belt-and-suspenders, but it keeps own-team kills off
    // the board if that ever changes. GrantDeathDrops must run even with no
    // rewarded player: it strips the tapper-owned native loot the engine just
    // filled (no last hit, no loot).
    void JustDied(Unit* killer) override
    {
        if (!_cfg)
            return;

        // An own-team kill earns nothing -- not the personal drops, and not the
        // team-wide ones either, so the reward source is dropped alongside the
        // player. GrantDeathDrops cannot infer this: handed the raw unit it would
        // resolve it straight back to a team and pay that team for its own creep.
        Player* p = killer ? killer->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
        Unit* rewardSource = killer;
        if (p && p->GetBgTeamId() == _cfg->team)
        {
            p = nullptr;
            rewardSource = nullptr;
        }

        if (BattlegroundMap* bgMap = me->GetMap()->ToBattlegroundMap())
        {
            if (auto* moba = dynamic_cast<BattlegroundMOBA*>(bgMap->GetBG()))
            {
                moba->GrantDeathDrops(me, p, rewardSource);
                if (p)
                    moba->CreditCreepKill(p);
            }
        }
    }

private:
    bool MatchEnded() const
    {
        if (BattlegroundMap* bgMap = me->GetMap()->ToBattlegroundMap())
            if (Battleground* bg = bgMap->GetBG())
                return bg->GetStatus() != STATUS_IN_PROGRESS;
        return false;
    }

    float DistanceFromLane2d(float x, float y) const
    {
        if (!_lanePath || _lanePath->Nodes.empty())
            return 0.0f;

        float bestSq = std::numeric_limits<float>::max();
        for (WaypointNode const& node : _lanePath->Nodes)
        {
            float dx = x - node.X;
            float dy = y - node.Y;
            bestSq = std::min(bestSq, dx * dx + dy * dy);
        }
        return std::sqrt(bestSq);
    }

    void ResumeLaneFromHere()
    {
        WaypointPath const* path = sWaypointMgr->GetPath(_cfg->pathId);
        if (!path || path->Nodes.empty())
            return;

        // Nearest node to where combat left us...
        std::size_t resumeIdx = 0;
        float bestSq = std::numeric_limits<float>::max();
        for (std::size_t i = 0; i < path->Nodes.size(); ++i)
        {
            float dx = me->GetPositionX() - path->Nodes[i].X;
            float dy = me->GetPositionY() - path->Nodes[i].Y;
            float distSq = dx * dx + dy * dy;
            if (distSq < bestSq)
            {
                bestSq = distSq;
                resumeIdx = i;
            }
        }

        // ...but never behind our furthest progress: a creep kited toward its
        // own base resumes from where it already reached, so a player can't walk
        // a wave backwards (re-dragging by attacking still works, as in LoL).
        for (std::size_t i = resumeIdx + 1; i < path->Nodes.size(); ++i)
            if (path->Nodes[i].Id == _highestReachedNodeId)
            {
                resumeIdx = i;
                break;
            }

        WaypointNode const& resumeNode = path->Nodes[resumeIdx];

        // Anchor home to the resume node ON the lane. Home is what the engine's
        // own leash (CanCreatureAttack, 30yd) measures from, so keeping it on the
        // lane bounds every chase to a corridor around the path; anchoring to the
        // creep's current position instead lets a fleeing player ratchet the leash
        // to the map edge (chase -> evade -> new home here -> re-aggro -> repeat).
        // Shipped as a real bug once. See CanAIAttack for why home drifts.
        me->SetHomePosition(resumeNode.X, resumeNode.Y, resumeNode.Z, me->GetOrientation());

        // The engine can't start a DB waypoint path mid-route (i_currentNode is
        // only seedable from CreatureData, which TempSummons lack), so hand the
        // generator a truncated copy from the resume node on. Copy nodes directly
        // -- the WaypointNode convenience constructor would reset MoveType to walk.
        _resumePath.Id = _cfg->pathId;
        _resumePath.Nodes.assign(path->Nodes.begin() + resumeIdx, path->Nodes.end());

        me->GetMotionMaster()->Clear(false); // drop any leftover chase in the active slot
        me->GetMotionMaster()->MoveWaypoint(_resumePath, false);
    }

    void CastAtVictim(TaskContext context)
    {
        if (Unit* victim = me->GetVictim())
            if (me->IsWithinDist(victim, _cfg->range)) // cheap pre-filter before attempting the cast
                DoCastVictim(_cfg->spellId, false);

        context.Repeat(std::chrono::milliseconds(_cfg->intervalMs));
    }

    MobaCreepConfig const* _cfg = nullptr;
    WaypointPath _resumePath;
    WaypointPath const* _lanePath = nullptr;
    uint32 _highestReachedNodeId = 0;
    uint32 _corridorCheckTimer = 0;
};

// Positive spells a player may cast on a lane creep: heals, HoTs, absorbs, and
// cleanses -- effects that actually do something to a creature. Everything else
// (stat buffs: PW:F, MotW, Kings, ...) is rejected with "Invalid target", because
// Creature::UpdateStats is a no-op -- a stat buff applies its icon but changes
// nothing, which reads in-game as a bug. Rejecting beats silently lying.
// Runs from the top of Spell::CheckCast, before mana/cooldown are consumed.
class moba_creep_spell_gate : public AllSpellScript
{
public:
    moba_creep_spell_gate() : AllSpellScript("moba_creep_spell_gate", std::vector<uint16>{uint16(ALLSPELLHOOK_ON_SPELL_CHECK_CAST)}) { }

    void OnSpellCheckCast(Spell* spell, bool /*strict*/, SpellCastResult& res) override
    {
        if (!spell->GetCaster()->GetCharmerOrOwnerPlayerOrPlayerItself())
            return; // only gate player (and pet) casts; creep/tower/BG internals untouched

        Unit* target = spell->m_targets.GetUnitTarget();
        if (!target || !target->IsCreature() || !sMobaCreepDataStore->GetConfig(target->GetEntry()))
            return;

        SpellInfo const* info = spell->GetSpellInfo();
        if (!info->IsPositive())
            return; // attacking a creep is governed by the normal hostility rules

        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        {
            switch (info->Effects[i].Effect)
            {
                case SPELL_EFFECT_HEAL:
                case SPELL_EFFECT_HEAL_PCT:
                case SPELL_EFFECT_HEAL_MAX_HEALTH:
                case SPELL_EFFECT_DISPEL:
                    return; // allowed
                case SPELL_EFFECT_APPLY_AURA:
                    if (info->Effects[i].ApplyAuraName == SPELL_AURA_PERIODIC_HEAL
                        || info->Effects[i].ApplyAuraName == SPELL_AURA_SCHOOL_ABSORB)
                        return; // allowed
                    break;
                default:
                    break;
            }
        }

        res = SPELL_FAILED_BAD_TARGETS;
    }
};

void AddSC_npc_moba_creep()
{
    RegisterCreatureAI(npc_moba_creep);
    new moba_creep_spell_gate();
}
