USE acore_world;

-- LoL-style resurrection timers for BattlegroundMOBA (see MobaResurrectionData.{h,cpp}).
-- Map-keyed so each MOBA map can set its own respawn pacing. Timing ONLY: the
-- respawn location reuses the battleground's team start position (game_graveyard
-- 1103/1104 today), so there are no coordinates to duplicate here.
--
-- Respawn wait (ms) = min(CapMs, BaseMs + PerMinMs * match-minutes elapsed).
DROP TABLE IF EXISTS `mod_moba_resurrection`;
CREATE TABLE `mod_moba_resurrection` (
    `Map`      INT UNSIGNED NOT NULL PRIMARY KEY,        -- BG map id (566 = hijacked EotS)
    `BaseMs`   INT UNSIGNED NOT NULL DEFAULT 10000,      -- base respawn wait
    `PerMinMs` INT UNSIGNED NOT NULL DEFAULT 1500,       -- added per elapsed match-minute
    `CapMs`    INT UNSIGNED NOT NULL DEFAULT 60000       -- maximum respawn wait
);

DELETE FROM `mod_moba_resurrection` WHERE `Map` = 566;
INSERT INTO `mod_moba_resurrection` (`Map`, `BaseMs`, `PerMinMs`, `CapMs`)
VALUES
(566, 10000, 1500, 60000);
