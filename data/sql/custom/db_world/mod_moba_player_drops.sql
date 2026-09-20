-- ============================================================
-- GENERATED FILE -- do not hand-edit.
-- Produced by apps/moba/gen_player_drops.py from apps/moba/maps/*/player_config.yaml.
-- Rolled and delivered by BattlegroundMOBA::GrantPlayerKillDrops at the
-- killing blow, straight to the killer -- no corpse, no native loot.
-- Type 0 = buff (aura on the killer; DurationMs 0 = the spell's
-- default), 1 = gold (ModifyMoney), 2 = item (AddItem, Count each).
-- Sell is type 2 only, PER UNIT: the buy-back price npc_moba_store refunds,
-- 0 meaning the item cannot be sold back.
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
    `Sell`       INT UNSIGNED NOT NULL DEFAULT 0,   -- type 2 only, PER UNIT
    `Chance`     FLOAT NOT NULL DEFAULT 100,
    PRIMARY KEY (`Map`, `Idx`)
);

INSERT INTO `mod_moba_player_drops`
(`Map`, `Idx`, `Type`, `Spell`, `DurationMs`, `Copper`, `Item`, `Count`, `Sell`, `Chance`)
VALUES
(900, 0, 1, 0, 0, 10000, 0, 1, 0, 100);

-- ============================================================
-- Custom item copies: entry = source entry + 900000.
-- mod_moba_item_copy is SHARED -- several generators write into
-- 900000-999999, and a source claimed by two of them resolves to ONE row.
-- Hence CREATE ... IF NOT EXISTS and a per-owner DELETE; this table is never
-- dropped. The updater re-applies only files whose hash changed, so a
-- generator that clears another's rows may not see them rebuilt.
-- ============================================================
CREATE TABLE IF NOT EXISTS `mod_moba_item_copy` (
    `Entry` INT UNSIGNED NOT NULL,
    `Owner` VARCHAR(32) NOT NULL,
    PRIMARY KEY (`Entry`, `Owner`)
);

-- Reclaim this generator's previous copies, sparing any a second owner
-- still claims.
DELETE it FROM `item_template` it
    JOIN `mod_moba_item_copy` mine ON mine.`Entry` = it.`entry` AND mine.`Owner` = 'player_drops'
    LEFT JOIN `mod_moba_item_copy` other ON other.`Entry` = it.`entry` AND other.`Owner` <> 'player_drops'
WHERE other.`Entry` IS NULL;

DELETE FROM `mod_moba_item_copy` WHERE `Owner` = 'player_drops';
