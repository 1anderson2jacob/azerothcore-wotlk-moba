#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "Battleground.h"
#include "BattlegroundMOBA.h"
#include "Map.h"
#include "MobaNeutralData.h"

// Deliberately NOT npc_moba_creep, which exists to defeat the engine's evade. Neutrals
// want stock evade: pulled too far, a camp runs home and resets to full health. Home
// never drifts here (no waypoint generator runs), so the spawn stays the leash anchor.
struct npc_moba_neutral : public ScriptedAI
{
    npc_moba_neutral(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _cfg = sMobaNeutralDataStore->GetConfig(me->GetEntry());
        if (!_cfg)
        {
            LOG_ERROR("scripts.ai", "npc_moba_neutral: no mod_moba_neutral_data row for entry {}.", me->GetEntry());
            return;
        }

        // 0 = pull-on-hit: passive until damaged. Above 0 rides the engine's proximity
        // aggro (creature_template.detection_range, stamped by the generator).
        me->SetReactState(_cfg->aggroRange > 0.0f ? REACT_AGGRESSIVE : REACT_DEFENSIVE);
    }

    // Hard leash from the camp anchor. Needed because the engine's own leash is SKIPPED
    // while combat stays "fresh" (Creature::CanCreatureAttack, as with lane creeps), so
    // without this cap a player can kite a camp indefinitely. 0 = engine leash only.
    void UpdateAI(uint32 diff) override
    {
        _leashCheckTimer += diff;
        if (_leashCheckTimer >= 1000)
        {
            _leashCheckTimer = 0;
            if (_cfg && _cfg->leashRange > 0.0f && me->IsEngaged() && !me->IsInEvadeMode()
                && me->GetHomePosition().GetExactDist2d(me) > _cfg->leashRange)
            {
                EnterEvadeMode(EVADE_REASON_OTHER);
                return;
            }
        }

        ScriptedAI::UpdateAI(diff);
    }

    // Camps are a player-only objective. Pets, guardians and totems resolve to their
    // owner and stay allowed.
    bool CanAIAttack(Unit const* victim) const override
    {
        return victim->GetCharmerOrOwnerPlayerOrPlayerItself() != nullptr;
    }

    // Camp-link rides damage, not engagement: a one-shot kills the member before
    // EngagementStart fires, so a JustEngagedWith-only link missed the pull.
    void DamageTaken(Unit* attacker, uint32& /*damage*/, DamageEffectType /*damagetype*/, SpellSchoolMask /*damageSchoolMask*/) override
    {
        if (attacker && attacker != me)
            if (BattlegroundMOBA* moba = GetMoba())
                moba->PullCampMates(me, attacker);
    }

    // Covers damage-less pulls, e.g. a mate proximity-aggroing a passer-by.
    void JustEngagedWith(Unit* who) override
    {
        if (BattlegroundMOBA* moba = GetMoba())
            moba->PullCampMates(me, who);
    }

    void JustDied(Unit* killer) override
    {
        BattlegroundMOBA* moba = GetMoba();
        if (!moba)
            return;

        moba->NotifyNeutralDied(me, killer);

        // No team guard, unlike npc_moba_creep: either team can take any camp, so the
        // raw killer is ALWAYS a valid reward source.
        Player* p = killer ? killer->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
        moba->GrantDeathDrops(me, p, killer);
        if (p)
            moba->CreditCreepKill(p);
    }

private:
    BattlegroundMOBA* GetMoba() const
    {
        if (BattlegroundMap* bgMap = me->GetMap()->ToBattlegroundMap())
            return dynamic_cast<BattlegroundMOBA*>(bgMap->GetBG());
        return nullptr;
    }

    MobaNeutralConfig const* _cfg = nullptr;
    uint32 _leashCheckTimer = 0;
};

void AddSC_npc_moba_neutral()
{
    RegisterCreatureAI(npc_moba_neutral);
}
