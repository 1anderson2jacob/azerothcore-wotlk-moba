-- ============================================================
-- GENERATED FILE -- do not hand-edit.
-- Produced by apps/moba/gen_respawn.py from apps/moba/maps/*/respawn_config.json.
-- Timing lives in mod_moba_respawn; spawn LOCATION is written into
-- game_graveyard / battleground_template (read at runtime via
-- GetTeamStartPosition / GetClosestGraveyard).
-- ============================================================

USE acore_world;

DROP TABLE IF EXISTS `mod_moba_respawn`;
CREATE TABLE `mod_moba_respawn` (
    `Map`      INT UNSIGNED NOT NULL PRIMARY KEY,        -- BG map id
    `BaseMs`   INT UNSIGNED NOT NULL DEFAULT 10000,      -- base respawn wait
    `PerMinMs` INT UNSIGNED NOT NULL DEFAULT 1500,       -- added per elapsed match-minute
    `CapMs`    INT UNSIGNED NOT NULL DEFAULT 60000       -- maximum respawn wait
);

INSERT INTO `mod_moba_respawn` (`Map`, `BaseMs`, `PerMinMs`, `CapMs`)
VALUES
(566, 10000, 1500, 60000);

-- Spawn wiring for map 566 (eye_of_the_storm)
UPDATE battleground_template SET AllianceStartLoc = 1103, AllianceStartO = 3.0222116, HordeStartLoc = 1104, HordeStartO = 0.32122585 WHERE ID = 7;
UPDATE game_graveyard SET x = 2387.529, y = 1587.426, z = 1174.763 WHERE ID = 1103;
UPDATE game_graveyard SET x = 1942.9327, y = 1547.6229, z = 1176.458 WHERE ID = 1104;
