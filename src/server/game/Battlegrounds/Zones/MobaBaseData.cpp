#include "MobaBaseData.h"
#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "Field.h"
#include "Log.h"

MobaBaseDataStore* MobaBaseDataStore::instance()
{
    static MobaBaseDataStore instance;
    return &instance;
}

void MobaBaseDataStore::LoadIfNeeded()
{
    if (_loaded)
        return;

    _loaded = true;

    QueryResult result = WorldDatabase.Query(
        "SELECT Map, RespawnBaseMs, RespawnPerMinMs, RespawnCapMs, RecallCastMs, RecallEmpoweredCastMs, "
        "FountainTickMs, FountainHpPct, FountainManaPct, FountainRadius, "
        "KillCreditWindowMs, AssistWindowMs, AssistBuffMaxDurationMs, "
        "DomeEntryAlliance, DomeEntryHorde, "
        "StartingGold, PassiveTickMs, PassiveCopper, "
        "FirstBloodGold, ShutdownPerStreak, ShutdownCapGold, "
        "MultiKillWindowMs, SpreeMin, AceMinTeam, SurrenderMinMs, SurrenderVoteMs, SurrenderCooldownMs FROM mod_moba_base");

    if (!result)
    {
        LOG_ERROR("sql.sql", "MobaBaseDataStore: table `mod_moba_base` is empty or missing.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        MobaBaseConfig cfg;
        cfg.map                   = fields[0].Get<uint32>();
        cfg.respawnBaseMs         = fields[1].Get<uint32>();
        cfg.respawnPerMinMs       = fields[2].Get<uint32>();
        cfg.respawnCapMs          = fields[3].Get<uint32>();
        cfg.recallCastMs          = fields[4].Get<uint32>();
        cfg.recallEmpoweredCastMs = fields[5].Get<uint32>();
        cfg.fountainTickMs        = fields[6].Get<uint32>();
        cfg.fountainHpPct         = fields[7].Get<uint32>();
        cfg.fountainManaPct       = fields[8].Get<uint32>();
        cfg.fountainRadius        = fields[9].Get<float>();
        cfg.killCreditWindowMs      = fields[10].Get<uint32>();
        cfg.assistWindowMs          = fields[11].Get<uint32>();
        cfg.assistBuffMaxDurationMs = fields[12].Get<uint32>();
        cfg.domeEntryAlliance       = fields[13].Get<uint32>();
        cfg.domeEntryHorde          = fields[14].Get<uint32>();
        cfg.startingGold            = fields[15].Get<uint32>();
        cfg.passiveTickMs           = fields[16].Get<uint32>();
        cfg.passiveCopper           = fields[17].Get<uint32>();
        cfg.firstBloodGold          = fields[18].Get<uint32>();
        cfg.shutdownPerStreak       = fields[19].Get<uint32>();
        cfg.shutdownCapGold         = fields[20].Get<uint32>();
        cfg.multiKillWindowMs       = fields[21].Get<uint32>();
        cfg.spreeMin                = fields[22].Get<uint32>();
        cfg.aceMinTeam              = fields[23].Get<uint32>();
        cfg.surrenderMinMs          = fields[24].Get<uint32>();
        cfg.surrenderVoteMs         = fields[25].Get<uint32>();
        cfg.surrenderCooldownMs     = fields[26].Get<uint32>();

        _byMap[cfg.map] = cfg;
    } while (result->NextRow());
}

MobaBaseConfig const* MobaBaseDataStore::GetConfig(uint32 mapId) const
{
    auto itr = _byMap.find(mapId);
    return itr != _byMap.end() ? &itr->second : nullptr;
}
