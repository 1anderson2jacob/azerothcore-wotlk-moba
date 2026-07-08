#include "ScriptMgr.h"
#include "UnitScript.h"
#include "BattlegroundMOBA.h"
#include "MobaTowerData.h"
#include "npc_moba_tower.h"
#include "Player.h"
#include "Creature.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SharedDefines.h"
#include "Map.h"
#include "ObjectAccessor.h"

// Hard-CC mask for the tower aggro-override trigger: the existing
// IMMUNE_TO_MOVEMENT_IMPAIRMENT_AND_LOSS_CONTROL_MASK includes mere slows
// (SNARE, DAZE), which don't count as "hard" CC for this purpose, and excludes
// SILENCE, which we do want to count (loss of ability to act on a
// tower-defended ally is the spirit of the rule). Judgment call -- revisit
// here if the trigger feels too loose/tight in practice.
constexpr uint64 MOBA_HARD_CC_MECHANIC_MASK =
    (IMMUNE_TO_MOVEMENT_IMPAIRMENT_AND_LOSS_CONTROL_MASK
        & ~((1ULL << MECHANIC_SNARE) | (1ULL << MECHANIC_DAZE)))
    | (1ULL << MECHANIC_SILENCE);

namespace
{
    // Shared by OnDamage/OnAuraApply: attacker hurt/CC'd victim (an allied
    // player, from the tower's perspective) -- check whether any of victim's
    // team's towers should switch onto attacker.
    void CheckTowerAggroOverride(Player* attacker, Player* victim)
    {
        if (!attacker->IsHostileTo(victim))
            return;

        if (!victim->GetMap()->IsBattlegroundOrArena())
            return;

        BattlegroundMap* bgMap = victim->GetMap()->ToBattlegroundMap();
        if (!bgMap)
            return;

        auto* moba = dynamic_cast<BattlegroundMOBA*>(bgMap->GetBG());
        if (!moba)
            return;

        for (MobaTowerState& tower : moba->GetTowers())
        {
            if (tower.destroyed || tower.team != victim->GetTeamId())
                continue;

            MobaTowerConfig const* cfg = sMobaTowerDataStore->GetConfig(tower.entry);
            if (!cfg)
                continue;

            Creature* towerCreature = ObjectAccessor::GetCreature(*victim, tower.guid);
            if (!towerCreature || towerCreature->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE))
                continue;

            if (towerCreature->GetDistance(attacker) > cfg->range || towerCreature->GetDistance(victim) > cfg->range)
                continue;

            MobaTowerAggroOverride(towerCreature, attacker);
        }
    }
}

class moba_tower_aggro : public UnitScript
{
public:
    moba_tower_aggro() : UnitScript("moba_tower_aggro", true, std::vector<uint16>{uint16(UNITHOOK_ON_DAMAGE), uint16(UNITHOOK_ON_AURA_APPLY)}) { }

    void OnDamage(Unit* attacker, Unit* victim, uint32& /*damage*/) override
    {
        if (!attacker || !victim || !attacker->IsPlayer() || !victim->IsPlayer())
            return;

        CheckTowerAggroOverride(attacker->ToPlayer(), victim->ToPlayer());
    }

    void OnAuraApply(Unit* unit, Aura* aura) override
    {
        if (!unit || !unit->IsPlayer())
            return;

        Unit* caster = aura->GetCaster();
        if (!caster || !caster->IsPlayer())
            return;

        if (!(aura->GetSpellInfo()->GetAllEffectsMechanicMask() & MOBA_HARD_CC_MECHANIC_MASK))
            return;

        CheckTowerAggroOverride(caster->ToPlayer(), unit->ToPlayer());
    }
};

void AddSC_moba_tower_aggro()
{
    new moba_tower_aggro();
}
