USE acore_world;

-- One-shot repair. Hijacking the Eye of the Storm slot did not only add rows, it
-- EDITED three stock ones: battleground_template 7's start locations and player
-- floor, and game_graveyard 1103/1104 -- which are EotS's own Alliance and Horde
-- start graveyards, relocated to map 900.
--
-- Regenerating mod_moba_base.sql without those statements does not undo them. The
-- update system applies files; it never reverts one. Without this file a live
-- acore_world keeps the edit forever, and an Eye of the Storm player who releases
-- lands on Twisted Treeline.
--
-- Values are stock, from data/sql/base/db_world/battleground_template.sql and
-- game_graveyard.sql. Written idempotently on purpose: on a database built fresh
-- from base + custom this writes stock over stock and changes nothing.

UPDATE `battleground_template` SET
    `MinPlayersPerTeam` = 8, `MaxPlayersPerTeam` = 15,
    `MinLvl` = 61, `MaxLvl` = 80,
    `AllianceStartLoc` = 1103, `AllianceStartO` = 3.03123,
    `HordeStartLoc`    = 1104, `HordeStartO`    = 0.055761,
    `StartMaxDist` = 100, `Weight` = 1, `ScriptName` = ''
WHERE `ID` = 7;

UPDATE `game_graveyard` SET `Map` = 566, `x` = 2523.69, `y` = 1596.6,  `z` = 1269.35 WHERE `ID` = 1103;
UPDATE `game_graveyard` SET `Map` = 566, `x` = 1807.74, `y` = 1539.42, `z` = 1267.63 WHERE `ID` = 1104;
