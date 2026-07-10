#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "Battleground.h"
#include "Map.h"
#include "MobaCreepData.h"
#include "MotionMaster.h"

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

        // Once the match is over the creep must stay frozen where
        // FreezeAllCreeps() left it. Reset() re-fires on every evade (see
        // below), and the re-arm guard beneath would otherwise restart the
        // lane path -- FreezeAllCreeps() replaced the idle-slot waypoint
        // generator with MoveIdle, so the slot-type check no longer holds
        // after the freeze.
        if (MatchEnded())
            return;

        // Reset() also re-fires on every evade (CreatureAI::EnterEvadeMode calls
        // it immediately, synchronously, right after queuing the home-return
        // move -- while the pre-existing WaypointMovementGenerator survives
        // underneath in MOTION_SLOT_IDLE with its lane progress intact). Only
        // (re)arm the path on first spawn -- otherwise calling MoveWaypoint
        // again here would replace that generator with a fresh one at
        // waypoint 0, rewinding lane progress instead of resuming it.
        if (me->GetMotionMaster()->GetMotionSlotType(MOTION_SLOT_IDLE) != WAYPOINT_MOTION_TYPE)
            me->GetMotionMaster()->MoveWaypoint(_cfg->pathId, true);

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

    void EnterEvadeMode(EvadeReason why) override
    {
        // Post-match evade (e.g. a player poking a frozen creep, or combat
        // unwinding right after EndBattleground): stay put. The default
        // would MoveTargetedHome() to the stale last-reached waypoint node
        // and then re-run Reset() -- both wrong once the match is over.
        if (MatchEnded())
        {
            me->CombatStop(true);
            me->GetMotionMaster()->MoveIdle();
            return;
        }

        ScriptedAI::EnterEvadeMode(why);
    }

private:
    bool MatchEnded() const
    {
        if (BattlegroundMap* bgMap = me->GetMap()->ToBattlegroundMap())
            if (Battleground* bg = bgMap->GetBG())
                return bg->GetStatus() != STATUS_IN_PROGRESS;
        return false;
    }

    void CastAtVictim(TaskContext context)
    {
        if (Unit* victim = me->GetVictim())
            if (me->IsWithinDist(victim, _cfg->range)) // DoCastVictim(..., true) skips range checks itself
                DoCastVictim(_cfg->spellId, false);

        context.Repeat(std::chrono::milliseconds(_cfg->intervalMs));
    }

    MobaCreepConfig const* _cfg = nullptr;
};

void AddSC_npc_moba_creep()
{
    RegisterCreatureAI(npc_moba_creep);
}
