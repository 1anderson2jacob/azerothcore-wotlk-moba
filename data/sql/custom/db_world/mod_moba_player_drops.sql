-- ============================================================
-- GENERATED FILE -- do not hand-edit.
-- Produced by apps/moba/gen_player_drops.py from apps/moba/maps/*/player_config.yaml.
-- Rolled and delivered by BattlegroundMOBA::GrantPlayerKillDrops at the
-- killing blow, straight to the killer -- no corpse, no native loot.
-- Type 0 = buff (aura on the killer; DurationMs 0 = the spell's
-- default), 1 = gold (ModifyMoney), 2 = item (AddItem, Count each).
-- Chance is a percent (config coefficient x 100).
-- ============================================================

USE acore_world;

DROP TABLE IF EXISTS `mod_moba_player_drops`;
CREATE TABLE `mod_moba_player_drops` (
    `Map`        INT UNSIGNED NOT NULL,
    `Idx`        TINYINT UNSIGNED NOT NULL,
    `Type`       TINYINT UNSIGNED NOT NULL,
    `Spell`      INT UNSIGNED NOT NULL DEFAULT 0,
    `DurationMs` INT UNSIGNED NOT NULL DEFAULT 0,
    `Copper`     INT UNSIGNED NOT NULL DEFAULT 0,
    `Item`       INT UNSIGNED NOT NULL DEFAULT 0,
    `Count`      INT UNSIGNED NOT NULL DEFAULT 1,
    `Chance`     FLOAT NOT NULL DEFAULT 100,
    PRIMARY KEY (`Map`, `Idx`)
);

INSERT INTO `mod_moba_player_drops`
(`Map`, `Idx`, `Type`, `Spell`, `DurationMs`, `Copper`, `Item`, `Count`, `Chance`)
VALUES
(566, 0, 1, 0, 0, 10000, 0, 1, 100),
(900, 0, 1, 0, 0, 10000, 0, 1, 100);
