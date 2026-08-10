#include "BattlegroundMOBA.h"
#include "Player.h"
#include "ScriptMgr.h"

// The countdown starts on release, not on death. BattlegroundMOBA owns the timer and the
// revive; these hooks hand a death over to it. Two entry points because the prep phase
// forbids releasing at all -- see OnPlayerJustDied.
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

        // Prep-phase deaths can never reach the release hook below: SPELL_PREPARATION
        // (44521) carries SPELL_AURA_PREVENT_RESURRECTION, so HandleRepopRequestOpcode
        // drops the release without a word, leaving a player rooted in
        // DeathState::Corpse with nothing to revive them. Instant, because a prep death
        // costs nothing -- but still routed through the timer, since this runs inside
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

        // Retiring the corpse makes the countdown the only way back:
        // HandleReclaimCorpseOpcode bails on a player with no corpse. Must stay paired
        // with StartRespawnTimer -- strip the corpse with no countdown to replace it and
        // the player is a ghost with no way back.
        player->SpawnCorpseBones(false);
    }

private:
    // KillPlayer() and BuildPlayerRepop() send SMSG_CORPSE_RECLAIM_DELAY on the line
    // before firing these hooks, and are the core's only two send sites -- so a 0 here is
    // what the client keeps.
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
