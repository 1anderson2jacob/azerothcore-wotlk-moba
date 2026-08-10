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

// The stock IMMUNE_TO_MOVEMENT_IMPAIRMENT_AND_LOSS_CONTROL_MASK includes mere slows
// (SNARE, DAZE) and excludes SILENCE, so neither end suits this trigger. A judgment
// call -- retune here if it feels loose or tight in play.
constexpr uint64 MOBA_HARD_CC_MECHANIC_MASK =
    (IMMUNE_TO_MOVEMENT_IMPAIRMENT_AND_LOSS_CONTROL_MASK
        & ~((1ULL << MECHANIC_SNARE) | (1ULL << MECHANIC_DAZE)))
    | (1ULL << MECHANIC_SILENCE);

namespace
{
    // Should any of the victim's team's towers switch onto the attacker?
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
            if (tower.destroyed || tower.team != victim->GetBgTeamId())
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
