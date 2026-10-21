#include "BattlegroundMOBA.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include <algorithm>

// The global observation points the battleground cannot see on its own; all policy
// lives in BattlegroundMOBA. GetCharmerOrOwnerPlayerOrPlayerItself filters PvE out of
// kill credit for free: a creep's hit resolves to no attacking player.
class moba_kill_credit_unitscript : public UnitScript
{
public:
    moba_kill_credit_unitscript() : UnitScript("moba_kill_credit_unitscript", true,
        { UNITHOOK_ON_DAMAGE, UNITHOOK_ON_HEAL, UNITHOOK_ON_AURA_APPLY, UNITHOOK_ON_UNIT_DEATH }) { }

    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        RecordDamage(attacker, victim);
        RecordQuestDamage(attacker, victim, damage);
    }

    void OnHeal(Unit* healer, Unit* victim, uint32& gain) override
    {
        Player* healerPlr = healer ? healer->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
        if (!healerPlr || !victim)
            return;

        BattlegroundMOBA* moba = dynamic_cast<BattlegroundMOBA*>(healerPlr->GetBattleground());
        if (!moba)
            return;

        if (Player* allyPlr = victim->ToPlayer())
            moba->RecordAllyHeal(allyPlr, healerPlr);

        moba->RecordQuestHeal(healerPlr, victim, gain);
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

    // DealDamage calls the hook before its overkill check (Unit.cpp:1251), so `damage` is
    // the whole hit; a quest counts only the health it removed.
    static void RecordQuestDamage(Unit* attacker, Unit* victim, uint32 damage)
    {
        if (!attacker || !victim || attacker == victim)
            return;

        uint32 const removed = std::min(damage, victim->GetHealth());
        if (!removed)
            return;

        if (Player* attackerPlr = attacker->GetCharmerOrOwnerPlayerOrPlayerItself())
            if (BattlegroundMOBA* moba = dynamic_cast<BattlegroundMOBA*>(attackerPlr->GetBattleground()))
                moba->RecordQuestDamageDealt(attackerPlr, victim, removed);

        if (Player* victimPlr = victim->ToPlayer())
            if (BattlegroundMOBA* moba = dynamic_cast<BattlegroundMOBA*>(victimPlr->GetBattleground()))
                moba->RecordQuestDamageTaken(victimPlr, attacker, removed);
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
