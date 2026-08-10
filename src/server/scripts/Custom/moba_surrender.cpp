#include "BattlegroundMOBA.h"
#include "Chat.h"
#include "CommandScript.h"
#include "Player.h"
#include "ScriptMgr.h"
#include <algorithm>

using namespace Acore::ChatCommands;

// Every rule, and everything the TEAM is told, lives on HandleSurrenderRequest. This
// file owns parsing yes/no and the refusals personal to whoever typed the command.
class moba_surrender_commandscript : public CommandScript
{
public:
    moba_surrender_commandscript() : CommandScript("moba_surrender_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable commandTable =
        {
            { "surrender", HandleSurrenderCommand, SEC_PLAYER, Console::No },
            { "ff",        HandleSurrenderCommand, SEC_PLAYER, Console::No },
        };

        return commandTable;
    }

    static bool HandleSurrenderCommand(ChatHandler* handler, Optional<std::string> answer)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        // A bare ".surrender" is a yes: starting a vote and agreeing to one are the
        // same intent.
        bool agree = true;
        if (answer)
        {
            std::string word = *answer;
            std::transform(word.begin(), word.end(), word.begin(), ::tolower);

            if (word == "no" || word == "n")
                agree = false;
            else if (word != "yes" && word != "y")
            {
                handler->SendErrorMessage("Syntax: .surrender [yes|no]");
                return false;
            }
        }

        // Doubles as the "is this a MOBA battleground" test, as in GetRecallCastTimeMs.
        BattlegroundMOBA* bg = dynamic_cast<BattlegroundMOBA*>(player->GetBattleground());
        if (!bg)
        {
            handler->SendErrorMessage("You are not in a MOBA match.");
            return false;
        }

        uint32 secondsRemaining = 0;
        switch (bg->HandleSurrenderRequest(player, agree, secondsRemaining))
        {
            case MOBA_SURRENDER_PASSED:
            case MOBA_SURRENDER_VOTE_STARTED:
            case MOBA_SURRENDER_VOTE_COUNTED:
            case MOBA_SURRENDER_VOTE_FAILED:
                // The battleground already told the whole team; repeating it here would
                // double every line for the player who typed the command.
                return true;
            case MOBA_SURRENDER_TOO_EARLY:
                handler->SendErrorMessage("Your team cannot surrender yet -- {}:{:02} remaining.",
                    secondsRemaining / 60, secondsRemaining % 60);
                return false;
            case MOBA_SURRENDER_ON_COOLDOWN:
                handler->SendErrorMessage("A surrender vote failed recently -- {}:{:02} before another may start.",
                    secondsRemaining / 60, secondsRemaining % 60);
                return false;
            case MOBA_SURRENDER_ALREADY_VOTED:
                handler->SendErrorMessage("You have already voted.");
                return false;
            case MOBA_SURRENDER_NO_VOTE:
                handler->SendErrorMessage("There is no surrender vote to refuse.");
                return false;
            case MOBA_SURRENDER_NOT_IN_MATCH:
            default:
                handler->SendErrorMessage("The match is not in progress.");
                return false;
        }
    }
};

void AddSC_moba_surrender()
{
    new moba_surrender_commandscript();
}
