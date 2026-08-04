#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "Battleground.h"
#include "BattlegroundMOBA.h"
#include "Map.h"
#include "MobaNeutralData.h"

// Jungle-camp AI. Deliberately thin -- and deliberately NOT npc_moba_creep,
// which exists to defeat the engine's evade (lane resume, no run-back).
// Neutrals want stock evade: pull one too far and it runs home and resets to
// full health, which is exactly the League camp reset. Home never drifts here
// (no waypoint generator runs -- see npc_moba_creep's CanAIAttack comment for
// why it drifts on creeps), so the spawn position stays the leash anchor.
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

        // aggroRange 0 = League pull-on-hit: stand passive until damaged.
        // aggroRange > 0 rides the engine's proximity aggro instead
        // (creature_template.detection_range, stamped by the generator).
        me->SetReactState(_cfg->aggroRange > 0.0f ? REACT_AGGRESSIVE : REACT_DEFENSIVE);
    }

    // Hard leash from the camp anchor (home position), per-entry from config.
    // Needed because the engine's own leash (CreatureLeashRadius, 30yd global)
    // is SKIPPED while combat stays "fresh" (Creature::CanCreatureAttack
    // leash-extension window, as with lane creeps) -- without this cap a
    // player can kite a camp indefinitely. leashRange 0 = engine leash only.
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

    // Camp-link rides damage, not engagement: a one-shot kills the member
    // before EngagementStart ever fires, so a JustEngagedWith-only link missed
    // the pull (shipped as a real bug). Mates already fighting are skipped
    // inside PullCampMates, so per-hit calls stay cheap and idempotent.
    void DamageTaken(Unit* attacker, uint32& /*damage*/, DamageEffectType /*damagetype*/, SpellSchoolMask /*damageSchoolMask*/) override
    {
        if (attacker && attacker != me)
            if (BattlegroundMOBA* moba = GetMoba())
                moba->PullCampMates(me, attacker);
    }

    // Still wanted alongside DamageTaken: covers damage-less pulls, e.g. a
    // mate proximity-aggroing a passer-by links the rest of the camp.
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

        moba->NotifyNeutralDied(me);

        // Jungle CS + drops go to the killing-blow player (a pet's blow credits
        // its owner), same rationale as npc_moba_creep::JustDied. No team guard:
        // either team can take any camp, so the raw killer is ALWAYS a valid
        // reward source here -- which is what lets a boss finished by a creep or
        // tower still pay that side's team-wide drops. GrantDeathDrops runs even
        // with no rewarded player -- it must strip the tapper-owned native loot --
        // and is status-guarded inside like CreditCreepKill, so the frozen
        // post-match camps stay farmproof.
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
