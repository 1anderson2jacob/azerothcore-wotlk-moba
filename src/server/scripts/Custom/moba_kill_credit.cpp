/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "BattlegroundMOBA.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuras.h"
#include "SpellInfo.h"

// Kill-credit tracking. A player who damages or debuffs an enemy and then
// disengages still gets the kill if that enemy dies (to anything) within the
// configured window -- the LoL "you were recently in combat with them" rule.
// All policy lives in BattlegroundMOBA; these are the global observation
// points the BG can't see on its own. GetCharmerOrOwnerPlayerOrPlayerItself
// filters PvE for free: a creep's hit resolves to no player and records nothing.
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
        // Healing an ally is an assist-chain link. Overheal still fires OnHeal, so
        // topping someone off pre-engage counts -- matching LoL.
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

        // Negative aura on an enemy = a debuff (kill credit + direct assist).
        // Positive aura on an ally = a buff/shield (assist-chain link, if short).
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
    // Enemy attacker (or its owner) damaged/debuffed a player victim -> kill-credit
    // + direct-assist tracking. GetCharmerOrOwnerPlayerOrPlayerItself filters PvE.
    static void RecordDamage(Unit* attacker, Unit* victim)
    {
        Player* victimPlr = victim ? victim->ToPlayer() : nullptr;
        Player* attackerPlr = attacker ? attacker->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
        if (!victimPlr || !attackerPlr)
            return;
        if (BattlegroundMOBA* moba = dynamic_cast<BattlegroundMOBA*>(victimPlr->GetBattleground()))
            moba->RecordPlayerDamage(victimPlr, attackerPlr);
    }

    // Teammate buffer applied a positive aura to a player ally -> assist-chain link.
    // The BG applies the same-team + duration-threshold policy.
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
