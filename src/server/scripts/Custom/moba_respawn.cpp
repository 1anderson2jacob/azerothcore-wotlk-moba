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

// LoL-style respawn: the countdown starts when the player releases their spirit,
// not at the moment of death. BattlegroundMOBA owns the per-player timer and the
// revive; these hooks hand a MOBA death over to it. Two entry points because the
// prep phase forbids releasing at all -- see OnPlayerJustDied.
class moba_respawn_playerscript : public PlayerScript
{
public:
    moba_respawn_playerscript() : PlayerScript("moba_respawn_playerscript",
        { PLAYERHOOK_ON_PLAYER_JUST_DIED, PLAYERHOOK_ON_PLAYER_RELEASED_GHOST }) { }

    void OnPlayerJustDied(Player* player) override
    {
        ClearCorpseReclaimDelay(player);

        if (!player)
            return;

        // Prep-phase deaths only, and they can never reach the release hook below:
        // SPELL_PREPARATION (44521) is cast for exactly this window and carries
        // SPELL_AURA_PREVENT_RESURRECTION, so HandleRepopRequestOpcode drops the
        // release request without a word. The client still auto-accepts its own
        // death popup, leaving a player who looks alive but is rooted in
        // DeathState::Corpse with nothing left to revive them -- an instanced map
        // has no spirit healer and skips Player::Update's auto-release.
        //
        // Instant, because dying before the match starts is meant to cost nothing.
        // Still routed through the timer rather than reviving here: this runs inside
        // Unit::Kill's call stack, which is not done with the victim yet.
        Battleground* bg = player->GetBattleground();
        if (!bg || bg->GetStatus() != STATUS_WAIT_JOIN)
            return;

        if (BattlegroundMOBA* moba = dynamic_cast<BattlegroundMOBA*>(bg))
            moba->StartRespawnTimer(player, true);
    }

    void OnPlayerReleasedGhost(Player* player) override
    {
        ClearCorpseReclaimDelay(player);

        if (!player)
            return;

        Battleground* bg = player->GetBattleground();
        if (!bg || bg->GetStatus() != STATUS_IN_PROGRESS)
            return;

        BattlegroundMOBA* moba = dynamic_cast<BattlegroundMOBA*>(bg);
        if (!moba)
            return;

        moba->StartRespawnTimer(player);

        // Retiring the corpse is what makes the countdown the only way back:
        // HandleReclaimCorpseOpcode bails on a player with no corpse, so running
        // to the body can no longer skip the timer. Nothing here wants a corpse
        // anyway -- kill rewards go straight to the killer (GrantPlayerKillDrops),
        // never through corpse loot. Deliberately paired with StartRespawnTimer:
        // strip the corpse with no countdown to replace it and the player is a
        // ghost with no way back, which is why both sit behind the same guard.
        player->SpawnCorpseBones(false);
    }

private:
    // KillPlayer() and BuildPlayerRepop() each send SMSG_CORPSE_RECLAIM_DELAY
    // (30/60/120s, climbing with recent deaths) on the line before firing these
    // hooks, and those are the core's only two send sites -- so a 0 here is what
    // the client keeps. The number it would otherwise count down governs nothing
    // in a MOBA match: the revive is driven by BattlegroundMOBA's own timer.
    static void ClearCorpseReclaimDelay(Player* player)
    {
        if (!player)
            return;

        if (dynamic_cast<BattlegroundMOBA*>(player->GetBattleground()))
            player->SendCorpseReclaimDelay(0);
    }
};

void AddSC_moba_respawn()
{
    new moba_respawn_playerscript();
}
