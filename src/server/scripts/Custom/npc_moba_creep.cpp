#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "Battleground.h"
#include "Map.h"
#include "MobaCreepData.h"
#include "MotionMaster.h"
#include "WaypointMgr.h"

// Max 2D distance a creep may be dragged from its lane (home position always
// sits on a path node) before it force-evades and resumes the lane. The
// engine's own 30yd leash check is deliberately SKIPPED while combat stays
// "fresh" (Creature::CanCreatureAttack: damage, melee proximity, and
// unreachable targets all refresh a ~17s extension window -- authentic WoW
// kiting behavior), so without this hard cap a player moving at run speed
// can drag a wave across the whole map.
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

        // Once the match is over the creep must stay frozen where
        // FreezeAllCreeps() left it. Reset() re-fires on every evade, and the
        // re-arm below would otherwise restart the lane path.
        if (MatchEnded())
            return;

        // Arm the lane path on first spawn only (Reset() also re-fires on
        // evade; the slot-type check keeps us from rewinding to node 1).
        // Non-repeating: a creep that survives to the lane's end stands
        // there and keeps fighting whatever enters aggro range, LoL-style,
        // instead of turning around and walking the lane back.
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
            UpdateVictim(); // keep default threat/target housekeeping; never DoMeleeAttackIfReady
            return;
        }

        ScriptedAI::UpdateAI(diff); // melee & siege: full default engine behavior, no custom code
    }

    void AttackStart(Unit* victim) override
    {
        if (_cfg && _cfg->role == MOBA_CREEP_ROLE_CASTER)
            AttackStartCaster(victim, _cfg->range);
        else
            ScriptedAI::AttackStart(victim);
    }

    // Hard lane-corridor rule, LoL-style, measured against the lane path
    // itself -- NOT home position: the waypoint generator stamps home to
    // the creature's current position every moving tick
    // (WaypointMovementGenerator::DoUpdate), so a home-based corridor
    // followed the creep wherever a player dragged it. Players are the
    // only targets gated: creeps/towers are lane-bound already, and gating
    // them by this rule blocked tower pushes once (tower rejected at 41yd
    // from a mid-drag home anchor).
    bool CanAIAttack(Unit const* victim) const override
    {
        if (!victim->GetCharmerOrOwnerPlayerOrPlayerItself())
            return true;

        return DistanceFromLane2d(victim->GetPositionX(), victim->GetPositionY()) <= MOBA_CREEP_LANE_CORRIDOR;
    }

    // Node Ids are the DB point numbers and are preserved in the truncated
    // resume paths, so this stays comparable across resumes.
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

        if (!_EnterEvadeMode(why))
            return;

        // LoL-style leashing: no run-back. The default evade would
        // MoveTargetedHome() to the last-reached waypoint node and resume
        // the path from there -- a creep that chased 30yd forward runs all
        // 30yd back first. Instead, resume the lane near where combat
        // ended. UNIT_STATE_EVADE is normally cleared by the home-return
        // generator we're skipping, so clear it here (as the engine's
        // pet/MoveFollow evade branch does).
        me->ClearUnitState(UNIT_STATE_EVADE);
        ResumeLaneFromHere();
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

        // Nearest node (2D) to where combat left us...
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

        // ...but never behind our furthest lane progress: a creep kited
        // toward its own base resumes from where it already got to, so a
        // player can't walk a wave backwards (re-dragging it by attacking
        // still works, which matches LoL).
        for (std::size_t i = resumeIdx + 1; i < path->Nodes.size(); ++i)
            if (path->Nodes[i].Id == _highestReachedNodeId)
            {
                resumeIdx = i;
                break;
            }

        WaypointNode const& resumeNode = path->Nodes[resumeIdx];

        // Anchor home to the resume node ON the lane -- never to wherever
        // combat dragged us. Home is what the engine's leash check
        // (CanCreatureAttack, 30yd) measures from; keeping it on the lane
        // bounds every chase to a corridor around the path. Anchoring to
        // the creep's current position instead lets a fleeing player
        // ratchet the leash forward indefinitely (chase 30yd -> evade ->
        // new home right there -> re-aggro -> repeat to the map edge --
        // this shipped as a real bug once).
        me->SetHomePosition(resumeNode.X, resumeNode.Y, resumeNode.Z, me->GetOrientation());

        // The engine can't start a DB waypoint path mid-route (i_currentNode
        // is only seedable from CreatureData, which TempSummons don't have),
        // so hand the generator a truncated copy: the remaining nodes from
        // the resume node onward. Copy the nodes directly -- the
        // WaypointNode convenience constructor would reset MoveType to walk.
        _resumePath.Id = _cfg->pathId;
        _resumePath.Nodes.assign(path->Nodes.begin() + resumeIdx, path->Nodes.end());

        me->GetMotionMaster()->Clear(false); // drop any leftover chase in the active slot
        me->GetMotionMaster()->MoveWaypoint(_resumePath, false);
    }

    void CastAtVictim(TaskContext context)
    {
        if (Unit* victim = me->GetVictim())
            if (me->IsWithinDist(victim, _cfg->range)) // DoCastVictim(..., true) skips range checks itself
                DoCastVictim(_cfg->spellId, false);

        context.Repeat(std::chrono::milliseconds(_cfg->intervalMs));
    }

    MobaCreepConfig const* _cfg = nullptr;
    WaypointPath _resumePath;
    WaypointPath const* _lanePath = nullptr;
    uint32 _highestReachedNodeId = 0;
    uint32 _corridorCheckTimer = 0;
};

void AddSC_npc_moba_creep()
{
    RegisterCreatureAI(npc_moba_creep);
}
