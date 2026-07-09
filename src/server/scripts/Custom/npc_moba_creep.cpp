#include "ScriptedCreature.h"
#include "ScriptMgr.h"
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

private:
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
