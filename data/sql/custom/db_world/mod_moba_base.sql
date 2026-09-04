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
    `SurrenderCooldownMs` INT UNSIGNED NOT NULL DEFAULT 60000  -- after a failed vote, before that team may start another (0 = none)
);

INSERT INTO `mod_moba_base` (`Map`, `RespawnBaseMs`, `RespawnPerMinMs`, `RespawnCapMs`, `RecallCastMs`, `RecallEmpoweredCastMs`, `FountainTickMs`, `FountainHpPct`, `FountainManaPct`, `FountainRadius`, `KillCreditWindowMs`, `AssistWindowMs`, `AssistBuffMaxDurationMs`, `DomeEntryAlliance`, `DomeEntryHorde`, `StartingGold`, `PassiveTickMs`, `PassiveCopper`, `FirstBloodGold`, `ShutdownPerStreak`, `ShutdownCapGold`, `MultiKillWindowMs`, `SpreeMin`, `AceMinTeam`, `SurrenderMinMs`, `SurrenderVoteMs`, `SurrenderCooldownMs`)
VALUES
(900, 10000, 1500, 60000, 9000, 4500, 1000, 10, 10, 20, 15000, 10000, 60000, 900402, 900403, 15000, 5000, 100, 5000, 5000, 40000, 10000, 3, 2, 60000, 15000, 60000);

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

-- Spawn wiring for map 900 (twisted_treeline_v2)
-- StartMaxDist stays 0 ON PURPOSE. It is the core's prep-phase leash
-- (Battleground::_CheckSafePositions), which teleports players back to spawn
-- every 9s -- wrong for a base you are meant to walk around in. The dome holds
-- players in; the radius lives in mod_moba_base.FountainRadius.
-- MinPlayersPerTeam is 1 ON PURPOSE: Battleground::GetPrematureWinner forfeits a
-- team that drops below it, and a MOBA keeps playing 4v5. Stock EotS ships 8.
UPDATE battleground_template SET AllianceStartLoc = 1103, AllianceStartO = 6.2737, HordeStartLoc = 1104, HordeStartO = 3.1396, StartMaxDist = 0, MinPlayersPerTeam = 1 WHERE ID = 7;
-- Map moves with the coordinates. Player::RepopAtGraveyard teleports to
-- ClosestGrave->Map, so a graveyard left on the old map throws a releasing
-- player clean out of the battleground.
UPDATE game_graveyard SET Map = 900, x = -196.80025, y = 0.3477315, z = 3.5 WHERE ID = 1103;
UPDATE game_graveyard SET Map = 900, x = 197.60576, y = -0.386557, z = 3.5 WHERE ID = 1104;
