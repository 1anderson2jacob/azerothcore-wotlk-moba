#include "BattlegroundMOBA.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellScript.h"

// Casting Hearthstone inside a MOBA BG lands the player at their base instead of their
// inn; outside it, an ordinary Hearthstone. Cast time is retimed in Spell::prepare.
// Bound to spell 8690 via spell_script_names (data/sql/custom/mod_moba_recall.sql).

class spell_moba_hearthstone_recall : public SpellScript
{
    PrepareSpellScript(spell_moba_hearthstone_recall);

    bool Load() override
    {
        return GetCaster()->IsPlayer();
    }

    void HandleTeleport(SpellEffIndex effIndex)
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player)
            return;

        // Any status, not just IN_PROGRESS: this also stops a mid-cast Hearthstone from
        // pulling the player out of the battleground.
        BattlegroundMOBA* moba = dynamic_cast<BattlegroundMOBA*>(player->GetBattleground());
        if (!moba)
            return; // ordinary Hearthstone: fall through to the home-bind teleport

        PreventHitDefaultEffect(effIndex);   // cancel the built-in teleport-to-inn

        if (Position const* startPos = moba->GetTeamStartPosition(player->GetBgTeamId()))
            player->TeleportTo(moba->GetMapId(), startPos->GetPositionX(), startPos->GetPositionY(),
                startPos->GetPositionZ(), startPos->GetOrientation());

        // No recall cooldown: clear the one the cast just applied.
        player->RemoveSpellCooldown(BG_MOBA_RECALL_SPELL, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_moba_hearthstone_recall::HandleTeleport, EFFECT_0, SPELL_EFFECT_TELEPORT_UNITS);
    }
};

void AddSC_moba_recall()
{
    RegisterSpellScript(spell_moba_hearthstone_recall);
}
