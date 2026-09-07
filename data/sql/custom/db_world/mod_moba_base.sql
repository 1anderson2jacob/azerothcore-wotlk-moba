-- ============================================================
-- GENERATED FILE -- do not hand-edit.
-- Produced by apps/moba/gen_base.py from apps/moba/maps/*/base_config.yaml.
-- Tunables live in mod_moba_base; the base LOCATION is written into
-- game_graveyard / battleground_template (read at runtime via
-- GetTeamStartPosition / GetClosestGraveyard).
-- ============================================================

USE acore_world;

DROP TABLE IF EXISTS `mod_moba_base`;
CREATE TABLE `mod_moba_base` (
    `Map`             INT UNSIGNED NOT NULL PRIMARY KEY,      -- BG map id
    `RespawnBaseMs`   INT UNSIGNED NOT NULL DEFAULT 10000,    -- base respawn wait
    `RespawnPerMinMs` INT UNSIGNED NOT NULL DEFAULT 1500,     -- added per elapsed match-minute
    `RespawnCapMs`    INT UNSIGNED NOT NULL DEFAULT 60000,    -- maximum respawn wait
    `RecallCastMs`          INT UNSIGNED NOT NULL DEFAULT 0,  -- recall cast time (ms); 0 = spell default
    `RecallEmpoweredCastMs` INT UNSIGNED NOT NULL DEFAULT 0,  -- empowered recall cast time (ms); 0 = fall back to normal
    `FountainTickMs`  INT UNSIGNED NOT NULL DEFAULT 0,        -- fountain heal cadence (ms); 0 = fountain healing off
    `FountainHpPct`   INT UNSIGNED NOT NULL DEFAULT 0,        -- % of max health restored per tick
    `FountainManaPct` INT UNSIGNED NOT NULL DEFAULT 0,        -- % of max mana restored per tick (mana users only)
    `FountainRadius`  FLOAT NOT NULL DEFAULT 0,               -- spawn-dome radius (yards) = heal zone; 0 = fountain healing off
    `KillCreditWindowMs` INT UNSIGNED NOT NULL DEFAULT 15000,  -- window after enemy-player damage/debuff in which a death still credits that player (0 = off)
    `AssistWindowMs` INT UNSIGNED NOT NULL DEFAULT 10000,      -- window before a death in which damage/debuff/support earns an assist (0 = off)
    `AssistBuffMaxDurationMs` INT UNSIGNED NOT NULL DEFAULT 60000, -- max buff/shield duration (ms) counting as a fight buff for assists; longer = maintenance buff, ignored
    `DomeEntryAlliance` INT UNSIGNED NOT NULL DEFAULT 0,  -- gameobject_template entry of the Alliance spawn dome
    `DomeEntryHorde`    INT UNSIGNED NOT NULL DEFAULT 0,  -- gameobject_template entry of the Horde spawn dome
    `StartingGold`  INT UNSIGNED NOT NULL DEFAULT 0,      -- copper in the match wallet on entry (0 = none)
    `PassiveTickMs` INT UNSIGNED NOT NULL DEFAULT 0,      -- passive income cadence (ms); 0 = passive income off
    `PassiveCopper` INT UNSIGNED NOT NULL DEFAULT 0,      -- copper per tick, per player
    `FirstBloodGold`      INT UNSIGNED NOT NULL DEFAULT 0,     -- bonus copper for the match's first player kill (0 = off)
    `ShutdownPerStreak`   INT UNSIGNED NOT NULL DEFAULT 0,     -- bounty copper per kill on the victim's streak (0 = no bounty)
    `ShutdownCapGold`     INT UNSIGNED NOT NULL DEFAULT 0,     -- ceiling on that bounty (0 = uncapped)
    `MultiKillWindowMs`   INT UNSIGNED NOT NULL DEFAULT 10000, -- a kill this soon after the last extends the multi-kill (0 = multi-kills off)
    `SpreeMin`            INT UNSIGNED NOT NULL DEFAULT 3,     -- consecutive kills that announce a spree AND mark a shutdown target (0 = both off)
    `AceMinTeam`          INT UNSIGNED NOT NULL DEFAULT 2,     -- smallest wiped team that counts as an ace (0 = ace off)
    `SurrenderMinMs`      INT UNSIGNED NOT NULL DEFAULT 0,     -- earliest a surrender vote may start, from doors open (0 = no gate, not 'off')
    `SurrenderVoteMs`     INT UNSIGNED NOT NULL DEFAULT 15000, -- how long a surrender vote stays open before silence fails it
    `SurrenderCooldownMs` INT UNSIGNED NOT NULL DEFAULT 60000, -- after a failed vote, before that team may start another (0 = none)
    `GraveyardAlliance`   INT UNSIGNED NOT NULL DEFAULT 0,     -- game_graveyard id the Alliance releases to
    `GraveyardHorde`      INT UNSIGNED NOT NULL DEFAULT 0      -- game_graveyard id the Horde releases to
);

INSERT INTO `mod_moba_base` (`Map`, `RespawnBaseMs`, `RespawnPerMinMs`, `RespawnCapMs`, `RecallCastMs`, `RecallEmpoweredCastMs`, `FountainTickMs`, `FountainHpPct`, `FountainManaPct`, `FountainRadius`, `KillCreditWindowMs`, `AssistWindowMs`, `AssistBuffMaxDurationMs`, `DomeEntryAlliance`, `DomeEntryHorde`, `StartingGold`, `PassiveTickMs`, `PassiveCopper`, `FirstBloodGold`, `ShutdownPerStreak`, `ShutdownCapGold`, `MultiKillWindowMs`, `SpreeMin`, `AceMinTeam`, `SurrenderMinMs`, `SurrenderVoteMs`, `SurrenderCooldownMs`, `GraveyardAlliance`, `GraveyardHorde`)
VALUES
(900, 10000, 1500, 60000, 9000, 4500, 1000, 10, 10, 20, 15000, 10000, 60000, 900402, 900403, 15000, 5000, 100, 5000, 5000, 40000, 10000, 3, 2, 60000, 15000, 60000, 900500, 900501);

-- Spawn dome gameobjects. The whole block is cleared, not just the entries
-- being inserted, so a dome dropped from a config is dropped from the DB too.
DELETE FROM `gameobject_template` WHERE `entry` BETWEEN 900400 AND 900409;
INSERT INTO `gameobject_template`
(`entry`, `type`, `displayId`, `name`, `IconName`, `castBarCaption`, `unk1`, `size`,
 `Data0`, `Data1`, `Data2`, `Data3`, `Data4`, `Data5`, `Data6`, `Data7`, `Data8`, `Data9`,
 `Data10`, `Data11`, `Data12`, `Data13`, `Data14`, `Data15`, `Data16`, `Data17`, `Data18`,
 `Data19`, `Data20`, `Data21`, `Data22`, `Data23`, `AIName`, `ScriptName`, `VerifiedBuild`)
VALUES
(900402, 0, 7203, 'twisted_treeline_v2 spawn dome (Alliance)', '', '', '', 0.116009, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '', '', 0),
(900403, 0, 7203, 'twisted_treeline_v2 spawn dome (Horde)', '', '', '', 0.116009, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '', '', 0);

-- Dome faction/flags. GameObject reads both ONLY from this table, so a template
-- copy with no row here is selectable and clickable -- and clicking a DOOR opens it.
DELETE FROM `gameobject_template_addon` WHERE `entry` BETWEEN 900400 AND 900409;
INSERT INTO `gameobject_template_addon`
(`entry`, `faction`, `flags`, `mingold`, `maxgold`, `artkit0`, `artkit1`, `artkit2`, `artkit3`)
VALUES
(900402, 1375, 48, 0, 0, 0, 0, 0, 0),
(900403, 1375, 48, 0, 0, 0, 0, 0, 0);

-- Team start graveyards. Player::RepopAtGraveyard teleports to ClosestGrave->Map,
-- so a row left on the wrong map throws a releasing player clean out of the
-- battleground. The whole block is cleared, not just the rows being inserted.
DELETE FROM `game_graveyard` WHERE `ID` BETWEEN 900500 AND 900509;
INSERT INTO `game_graveyard` (`ID`, `Map`, `x`, `y`, `z`, `Comment`) VALUES
(900500, 900, -196.80025, 0.3477315, 3.5, 'twisted_treeline_v2 start (Alliance)'),
(900501, 900, 197.60576, -0.386557, 3.5, 'twisted_treeline_v2 start (Horde)');

-- The queue slot: one row per ACTIVE bundle. Its ID is also the BattlegroundTypeId
-- enumerator and the BattlemasterList.dbc row id -- see mod_moba_bg_map.sql.
-- The block is cleared first, so a bundle that goes inactive leaves no queueable
-- slot behind.
DELETE FROM `battleground_template` WHERE `ID` BETWEEN 12 AND 19;

-- Queue slot for map 900 (twisted_treeline_v2)
-- StartMaxDist stays 0 ON PURPOSE. It is the core's prep-phase leash
-- (Battleground::_CheckSafePositions), which teleports players back to spawn
-- every 9s -- wrong for a base you are meant to walk around in. The dome holds
-- players in; the radius lives in mod_moba_base.FountainRadius.
-- MinPlayersPerTeam is 1 ON PURPOSE: Battleground::GetPrematureWinner forfeits a
-- team that drops below it, and a MOBA keeps playing 2v3. It is NOT what lets a
-- lone tester start a match -- BattlegroundQueue forces 1 while .debug bg is on.
INSERT INTO `battleground_template`
(`ID`, `MinPlayersPerTeam`, `MaxPlayersPerTeam`, `MinLvl`, `MaxLvl`,
 `AllianceStartLoc`, `AllianceStartO`, `HordeStartLoc`, `HordeStartO`,
 `StartMaxDist`, `Weight`, `ScriptName`, `Comment`) VALUES
(12, 1, 3, 61, 80, 900500, 6.2737, 900501, 3.1396, 0, 1, '', 'twisted_treeline_v2 (MOBA)');
