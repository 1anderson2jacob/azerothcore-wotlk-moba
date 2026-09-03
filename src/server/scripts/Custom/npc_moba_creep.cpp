#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "AllSpellScript.h"
#include "Battleground.h"
#include "BattlegroundMOBA.h"
#include "Map.h"
#include "MobaCreepData.h"
#include "MobaNeutralData.h"
#include "MotionMaster.h"
#include "Spell.h"
#include "SpellAuraDefines.h"
#include "SpellInfo.h"
#include "WaypointMgr.h"
#include <unordered_map>

// Max 2D distance a creep may be dragged from its lane before it force-evades. The
// engine's own 30yd leash is SKIPPED while combat stays "fresh" (Creature::CanCreatureAttack
// refreshes a ~17s window on damage, melee proximity, or an unreachable target), so
// without this cap a player at run speed can drag a wave across the map.
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
        BuildLegRatios();
        me->UpdateSpeed(MOVE_RUN, false);

        // Stay frozen where FreezeAllCreeps() left us; the re-arm below would restart
        // the lane.
        if (MatchEnded())
            return;

        // First spawn only: Reset() re-fires on evade, and the slot-type check keeps
        // that from rewinding us to node 1. Non-repeating, so a creep that reaches the
        // lane's end holds and fights there rather than walking back.
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
        // Chasing is not lane movement -- a corner's multiplier would otherwise
        // follow the creep into combat and skew every chase it starts there.
        me->UpdateSpeed(MOVE_RUN, false);

        if (_cfg && _cfg->role == MOBA_CREEP_ROLE_CASTER)
            AttackStartCaster(victim, _cfg->range);
        else
            ScriptedAI::AttackStart(victim);
    }

    // Measured against the lane path, NOT home: the waypoint generator stamps home to
    // the creature's current position every moving tick, so a home-based corridor
    // followed the creep wherever a player dragged it. Players only -- creeps and towers
    // are lane-bound already, and gating them by this rule once blocked a tower push.
    bool CanAIAttack(Unit const* victim) const override
    {
        // Above the non-player early-out below, which would otherwise wave camps through.
        if (victim->IsCreature() && sMobaNeutralDataStore->GetConfig(victim->GetEntry()))
            return false;

        if (!victim->GetCharmerOrOwnerPlayerOrPlayerItself())
            return true;

        return DistanceFromLane2d(victim->GetPositionX(), victim->GetPositionY()) <= MOBA_CREEP_LANE_CORRIDOR;
    }

    // Node Ids are DB point numbers, preserved in the truncated resume paths.
    void WaypointReached(uint32 nodeId, uint32 /*pathId*/) override
    {
        if (nodeId > _highestReachedNodeId)
            _highestReachedNodeId = nodeId;

        // Runs before the generator increments i_currentNode, so the rate is in
        // place for the leg about to launch.
        ApplyLegSpeed(nodeId);
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

        // Inlined CreatureAI::_EnterEvadeMode minus RemoveEvadeAuras: creeps evade at
        // the end of EVERY skirmish, and it would strip player-cast buffs that should
        // run their full duration. Zone-script/formation/summoner notifications are also
        // skipped (creeps have none). Re-mirror CreatureAI.cpp when merging upstream.
        if (me->IsInEvadeMode())
            return;

        if (!me->IsAlive())
        {
            EngagementOver();
            return;
        }

        // Recursion guard: CombatStop below purges combat refs, re-entering
        // EnterEvadeMode; the IsInEvadeMode() check above catches it.
        me->AddUnitState(UNIT_STATE_EVADE);

        me->ClearComboPointHolders();
        me->CombatStop(true);
        me->LoadCreaturesAddon(true);
        me->SetLootRecipient(nullptr);
        me->ResetPlayerDamageReq();
        me->ClearLastLeashExtensionTimePtr();
        me->SetCannotReachTarget();

        // MANDATORY: clears the AI's _isEngaged latch. Without it the creep reads
        // "already fighting" forever after its first evade -- EngagementStart never
        // fires again and it ignores every later wave. Shipped as a real bug once.
        EngagementOver();

        // No run-back: the default evade would MoveTargetedHome() first. UNIT_STATE_EVADE
        // is normally cleared by the home-return generator we are skipping, so clear it
        // here, as the engine's pet/MoveFollow branch does.
        me->ClearUnitState(UNIT_STATE_EVADE);
        ResumeLaneFromHere();
    }

    // The true killing blow, which HandleKillUnit cannot give us -- see there. Enemy
    // players only: friendly fire on creeps is impossible today, so the team guard below
    // is insurance against that changing.
    void JustDied(Unit* killer) override
    {
        if (!_cfg)
            return;

        // An own-team kill earns nothing, so BOTH reward arguments go. GrantDeathDrops
        // cannot infer this: handed the raw unit it would resolve it straight back to a
        // team and pay that team for its own creep.
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

    static float NodeDist(WaypointNode const& a, WaypointNode const& b)
    {
        float dx = a.X - b.X;
        float dy = a.Y - b.Y;
        float dz = a.Z - b.Z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    // Ratio of this slot's leg to the lane centreline's leg at the same index,
    // keyed by the node the leg LEAVES. Run speed times this makes every slot
    // spend equal time on leg i however the lane bends -- that equal timing IS
    // the formation. Requires the two paths to be node-for-node aligned.
    void BuildLegRatios()
    {
        _legRatio.clear();

        WaypointPath const* ref = sWaypointMgr->GetPath(_cfg->refPathId);
        if (!_lanePath || !ref)
            return;

        if (_lanePath->Nodes.size() != ref->Nodes.size())
        {
            LOG_ERROR("scripts.ai", "npc_moba_creep: entry {} path {} has {} nodes but reference path {} has {}; speed compensation disabled.",
                me->GetEntry(), _cfg->pathId, _lanePath->Nodes.size(), _cfg->refPathId, ref->Nodes.size());
            return;
        }

        for (std::size_t i = 0; i + 1 < _lanePath->Nodes.size(); ++i)
        {
            float refLen = NodeDist(ref->Nodes[i], ref->Nodes[i + 1]);
            if (refLen < 0.01f)
                continue;

            _legRatio[_lanePath->Nodes[i].Id] =
                NodeDist(_lanePath->Nodes[i], _lanePath->Nodes[i + 1]) / refLen;
        }
    }

    // Compensation rides on the speed RATE, not the spline velocity. A rate is what
    // Unit::UpdateSpeed rebuilds from the creature's auras, so re-deriving it here
    // each leg makes a slow multiply the corner multiplier instead of replacing it;
    // a velocity written into the path would bypass the aura system entirely.
    // SetSpeedRate deliberately does not notify movement generators -- the value is
    // picked up by the next leg's own spline launch, with no extra relaunch.
    void ApplyLegSpeed(uint32 leavingNodeId)
    {
        me->UpdateSpeed(MOVE_RUN, false);

        auto itr = _legRatio.find(leavingNodeId);
        if (itr == _legRatio.end())
            return;

        me->SetSpeedRate(MOVE_RUN, me->GetSpeedRate(MOVE_RUN) * itr->second);
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

        // ...but never behind our furthest progress, so a player cannot walk a wave
        // backwards. Re-dragging by attacking still works.
        for (std::size_t i = resumeIdx + 1; i < path->Nodes.size(); ++i)
            if (path->Nodes[i].Id == _highestReachedNodeId)
            {
                resumeIdx = i;
                break;
            }

        WaypointNode const& resumeNode = path->Nodes[resumeIdx];

        // Anchor home to the resume node ON the lane: home is what the engine's leash
        // measures from, so anchoring to the creep's current position instead lets a
        // fleeing player ratchet it to the map edge. Shipped as a real bug once.
        me->SetHomePosition(resumeNode.X, resumeNode.Y, resumeNode.Z, me->GetOrientation());

        // The engine cannot start a DB waypoint path mid-route (i_currentNode is only
        // seedable from CreatureData, which TempSummons lack), so hand the generator a
        // truncated copy. Copy nodes directly -- WaypointNode's convenience constructor
        // would reset MoveType to walk.
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
    std::unordered_map<uint32, float> _legRatio;
};

// Heals, HoTs, absorbs and cleanses are allowed; stat buffs are rejected because
// Creature::UpdateStats is a no-op, so they would apply an icon and change nothing.
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
