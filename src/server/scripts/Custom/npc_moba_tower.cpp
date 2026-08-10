#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "GridNotifiers.h"
#include "CellImpl.h"
#include "Player.h"
#include "MobaTowerData.h"
#include "npc_moba_tower.h"
#include "ObjectAccessor.h"
#include "UnitAI.h"
#include "MobaCreepData.h"
#include "BattlegroundMOBA.h"
#include "Map.h"

struct npc_moba_tower : public ScriptedAI
{
    npc_moba_tower(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _cfg = sMobaTowerDataStore->GetConfig(me->GetEntry());
        if (!_cfg)
        {
            LOG_ERROR("scripts.ai", "npc_moba_tower: no mod_moba_tower_data row for entry {}, tower will not attack.", me->GetEntry());
            return;
        }

        _lockedTarget.Clear();
        _isAggroLocked = false;

        me->SetReactState(REACT_PASSIVE); // structures drive their own targeting (or none), not core aggro/threat
        me->SetCombatMovement(false); // stops core's MoveBackwardsChecks/MoveCircleChecks from repositioning a "stationary" NPC
        scheduler.CancelAll();

        // Inhibitors and cores gate progression and die, but never shoot.
        if (_cfg->kind != MOBA_STRUCTURE_TOWER)
            return;

        scheduler.Schedule(std::chrono::milliseconds(_cfg->intervalMs), [this](TaskContext context)
        {
            Tick(context);
        });
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);
    }

    // One-shot: fires the creep -> player switch once, and never re-triggers between
    // offenders. Full targeting rule: .github/MOBA_GUIDE.md.
    void TryAggroOverride(Player* offender)
    {
        if (_isAggroLocked || !_cfg)
            return;

        if (!IsValidTowerTarget(offender, _cfg->range))
            return;

        _lockedTarget = offender->GetGUID();
        _isAggroLocked = true;
    }

    // The one place that sees a structure's true killing blow -- see HandleKillUnit for
    // why the engine hook cannot. A creep-finished structure still pays its team; only
    // the last-hit bonus needs a player, and passing nullptr is what skips it.
    void JustDied(Unit* killer) override
    {
        BattlegroundMap* bgMap = me->GetMap()->ToBattlegroundMap();
        if (!bgMap)
            return;

        auto* moba = dynamic_cast<BattlegroundMOBA*>(bgMap->GetBG());
        if (!moba)
            return;

        // Pets and charmed units resolve to their owner.
        if (Player* player = killer ? killer->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr)
        {
            moba->OnTowerDestroyed(me, player->GetBgTeamId(), player);
            return;
        }

        // A lane creep finished it: its config names the team that benefits.
        if (MobaCreepConfig const* creepCfg = killer ? sMobaCreepDataStore->GetConfig(killer->GetEntry()) : nullptr)
            moba->OnTowerDestroyed(me, creepCfg->team, nullptr);
    }

private:
    void Tick(TaskContext context)
    {
        // Guarded towers don't fight back until their guard falls (OnTowerDestroyed).
        if (me->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE))
        {
            context.Repeat(std::chrono::milliseconds(_cfg->intervalMs));
            return;
        }

        Unit* target = _lockedTarget ? ObjectAccessor::GetUnit(*me, _lockedTarget) : nullptr;

        if (!IsValidTowerTarget(target, _cfg->range))
        {
            _lockedTarget.Clear();
            _isAggroLocked = false;

            target = SelectNearestEnemyCreature(_cfg->range);
            if (!target)
                target = SelectNearestEnemyPlayer(_cfg->range);

            if (target)
                _lockedTarget = target->GetGUID();
        }

        if (target)
        {
            AttackStartNoMove(target);
            DoCastVictim(_cfg->spellId, true);
        }
        else
            me->CombatStop();

        context.Repeat(std::chrono::milliseconds(_cfg->intervalMs));
    }

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

    // Any hostile non-player unit in range, excluding other towers.
    Unit* SelectNearestEnemyCreature(float range) const
    {
        std::list<Creature*> creatures;
        Acore::AnyUnitInObjectRangeCheck check(me, range);
        Acore::CreatureListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(me, creatures, check);
        Cell::VisitObjects(me, searcher, range);

        Creature* nearest = nullptr;
        float nearestDist = range;
        for (Creature* creature : creatures)
        {
            if (!me->IsHostileTo(creature))
                continue;

            if (sMobaTowerDataStore->GetConfig(creature->GetEntry()))
                continue; // never target other towers

            float dist = me->GetDistance(creature);
            if (dist <= nearestDist)
            {
                nearest = creature;
                nearestDist = dist;
            }
        }

        return nearest;
    }

    MobaTowerConfig const* _cfg = nullptr;
    ObjectGuid _lockedTarget;
    bool _isAggroLocked = false;
};

void MobaTowerAggroOverride(Creature* tower, Player* offender)
{
    ENSURE_AI(npc_moba_tower, tower->AI())->TryAggroOverride(offender);
}

void AddSC_npc_moba_tower()
{
    RegisterCreatureAI(npc_moba_tower);
}
