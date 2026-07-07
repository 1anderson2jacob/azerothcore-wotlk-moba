#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "GridNotifiers.h"
#include "CellImpl.h"
#include "Player.h"

enum BG_MOBA_TowerSpells
{
    // placeholder ranged attack for testing - simple caster-mob nuke (see RazorfenDowns/razorfen_downs.cpp)
    // tried "Shoot" (6660, weapon dead zone) and "Fiery Boulder" (38512, turned out to be SPELL_EFFECT_SUMMON,
    // not a damage spell); this deals real damage with no dead zone, but the missile doesn't render on the
    // current tower display models (likely no bone attachment point on these prop-style models) - deferred
    SPELL_MOBA_TOWER_SHOOT = 9053
};

struct npc_moba_tower : public ScriptedAI
{
    npc_moba_tower(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        me->SetReactState(REACT_PASSIVE); // tower drives its own targeting, not core aggro/threat
        me->SetCombatMovement(false); // stops core's MoveBackwardsChecks/MoveCircleChecks from repositioning a "stationary" NPC
        scheduler.CancelAll();
        scheduler.Schedule(1500ms, [this](TaskContext context)
        {
            Unit* target = me->GetVictim();
            if (!IsValidTowerTarget(target, 40.0f))
                target = SelectNearestEnemyPlayer(40.0f);

            if (target)
            {
                AttackStartNoMove(target);
                DoCastVictim(SPELL_MOBA_TOWER_SHOOT, true);
            }
            else
                me->AttackStop();

            context.Repeat(1500ms);
        });
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);
    }

    void JustDied(Unit* /*killer*/) override
    {
        // TODO: notify BattlegroundMOBA win condition
    }

private:
    bool IsValidTowerTarget(Unit* target, float range) const
    {
        return target && me->GetDistance(target) <= range && me->IsValidAttackTarget(target);
    }

    Unit* SelectNearestEnemyPlayer(float range) const
    {
        std::list<Player*> players;
        Acore::AnyPlayerInObjectRangeCheck check(me, range);
        Acore::PlayerListSearcher<Acore::AnyPlayerInObjectRangeCheck> searcher(me, players, check);
        Cell::VisitObjects(me, searcher, range);

        Player* nearest = nullptr;
        float nearestDist = range;
        for (Player* player : players)
        {
            if (!me->IsHostileTo(player))
                continue;

            float dist = me->GetDistance(player);
            if (dist <= nearestDist)
            {
                nearest = player;
                nearestDist = dist;
            }
        }

        return nearest;
    }
};

void AddSC_npc_moba_tower()
{
    RegisterCreatureAI(npc_moba_tower);
}
