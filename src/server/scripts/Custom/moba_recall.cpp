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
#include "SpellScript.h"

// LoL-style recall: the player casts Hearthstone (breaks on movement or damage --
// both free from the spell engine; its cast time is retimed per-map in
// Spell::prepare via BattlegroundMOBA::GetRecallCastTimeMs). While inside the MOBA
// battleground we hijack the teleport: instead of the home bind (their inn), we
// drop them at their team's start position -- the same spot respawn uses -- and
// reset the cooldown so recall is repeatable. Outside the MOBA BG the spell behaves
// exactly like a normal Hearthstone.
//
// Bound to spell 8690 via a spell_script_names row (data/sql/custom/mod_moba_recall.sql).
// Pattern mirrors spell_item_scroll_of_recall in src/server/scripts/Spells/spell_item.cpp.

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

        // Only redirect while in the MOBA BG. Redirecting for any status (not just
        // IN_PROGRESS) also stops a mid-cast Hearthstone from ever pulling the
        // player out of the battleground.
        BattlegroundMOBA* moba = dynamic_cast<BattlegroundMOBA*>(player->GetBattleground());
        if (!moba)
            return; // normal Hearthstone: fall through to the default home-bind teleport

        // Cancel the built-in "teleport to inn" and send the player to their base.
        PreventHitDefaultEffect(effIndex);

        // GetTeamId() to match RespawnAtBase, so recall and respawn land at the same base.
        if (Position const* startPos = moba->GetTeamStartPosition(player->GetTeamId()))
            player->TeleportTo(moba->GetMapId(), startPos->GetPositionX(), startPos->GetPositionY(),
                startPos->GetPositionZ(), startPos->GetOrientation());

        // LoL-style: no recall cooldown -- clear the cooldown the cast just applied.
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
