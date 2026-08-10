#include "BattlegroundMOBA.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuras.h"
#include "SpellInfo.h"

// The global observation points the battleground cannot see on its own; all policy
// lives in BattlegroundMOBA. GetCharmerOrOwnerPlayerOrPlayerItself filters PvE for
// free: a creep's hit resolves to no player and records nothing.
class moba_kill_credit_unitscript : public UnitScript
{
public:
    moba_kill_credit_unitscript() : UnitScript("moba_kill_credit_unitscript", true,
        { UNITHOOK_ON_DAMAGE, UNITHOOK_ON_HEAL, UNITHOOK_ON_AURA_APPLY, UNITHOOK_ON_UNIT_DEATH }) { }

    void OnDamage(Unit* attacker, Unit* victim, uint32& /*damage*/) override
    {
        RecordDamage(attacker, victim);
    }

    void OnHeal(Unit* healer, Unit* victim, uint32& /*gain*/) override
    {
        Player* allyPlr = victim ? victim->ToPlayer() : nullptr;
        Player* healerPlr = healer ? healer->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
        if (!allyPlr || !healerPlr)
            return;
        if (BattlegroundMOBA* moba = dynamic_cast<BattlegroundMOBA*>(allyPlr->GetBattleground()))
            moba->RecordAllyHeal(allyPlr, healerPlr);
    }

    void OnAuraApply(Unit* unit, Aura* aura) override
    {
        Unit* caster = aura ? aura->GetCaster() : nullptr;
        if (!caster)
            return;

        // Negative on an enemy is a debuff (kill credit); positive on an ally is a
        // buff/shield (assist-chain link, if short).
        if (aura->GetSpellInfo()->IsPositive())
            RecordBuff(caster, unit, aura->GetMaxDuration());
        else
            RecordDamage(caster, unit);
    }

    void OnUnitDeath(Unit* victim, Unit* killer) override
    {
        Player* victimPlr = victim ? victim->ToPlayer() : nullptr;
        if (!victimPlr)
            return;
        if (BattlegroundMOBA* moba = dynamic_cast<BattlegroundMOBA*>(victimPlr->GetBattleground()))
            moba->HandlePlayerDeath(victimPlr, killer);
    }

private:
    static void RecordDamage(Unit* attacker, Unit* victim)
    {
        Player* victimPlr = victim ? victim->ToPlayer() : nullptr;
        Player* attackerPlr = attacker ? attacker->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
        if (!victimPlr || !attackerPlr)
            return;
        if (BattlegroundMOBA* moba = dynamic_cast<BattlegroundMOBA*>(victimPlr->GetBattleground()))
            moba->RecordPlayerDamage(victimPlr, attackerPlr);
    }

    // The BG applies the same-team and duration-threshold policy.
    static void RecordBuff(Unit* caster, Unit* target, int32 auraMaxDurationMs)
    {
        Player* targetPlr = target ? target->ToPlayer() : nullptr;
        Player* casterPlr = caster ? caster->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
        if (!targetPlr || !casterPlr)
            return;
        if (BattlegroundMOBA* moba = dynamic_cast<BattlegroundMOBA*>(targetPlr->GetBattleground()))
            moba->RecordAllyBuff(targetPlr, casterPlr, auraMaxDurationMs);
    }
};

void AddSC_moba_kill_credit()
{
    new moba_kill_credit_unitscript();
}
